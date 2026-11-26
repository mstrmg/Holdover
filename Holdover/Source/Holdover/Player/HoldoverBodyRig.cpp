#include "Player/HoldoverBodyRig.h"

#include "Animation/HoldoverGunAnimator.h"
#include "Animation/HoldoverGunAnimSet.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/HoldoverDebug.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkinnedAsset.h"
#include "GameFramework/Actor.h"
#include "ReferenceSkeleton.h"
#include "Player/HoldoverCharacter.h"
#include "Weapons/HoldoverGun.h"
#include "Weapons/HoldoverGunData.h"
#include "Holdover.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static const TCHAR* GFingerNames[5] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };

UHoldoverBodyRig::UHoldoverBodyRig()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the animated body, the character and the gun have all moved this frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	InitDefaultPoses();
}

void UHoldoverBodyRig::InitDefaultPoses()
{
	auto Pose = [](std::initializer_list<FHoldoverFingerCurl> Curls, int32 Mask)
	{
		FHoldoverHandPose P;
		P.Fingers = Curls;
		P.FingerMask = Mask;
		return P;
	};
	using C = FHoldoverFingerCurl;
	// Order: thumb, index, middle, ring, pinky. Degrees for the three joints of each finger.
	HandPoses.Add(TEXT("Relaxed"), Pose({ C(10, 10, 5), C(15, 15, 10), C(18, 18, 10), C(20, 20, 12), C(22, 22, 12) }, 0x1F));
	HandPoses.Add(TEXT("Grip"), Pose({ C(20, 30, 20), C(10, 5, 5), C(75, 85, 50), C(78, 85, 50), C(80, 85, 50) }, 0x1F));
	HandPoses.Add(TEXT("IndexTrigger"), Pose({ C(0, 0, 0), C(35, 50, 25), C(0, 0, 0), C(0, 0, 0), C(0, 0, 0) }, 0x02));
	HandPoses.Add(TEXT("SupportWrap"), Pose({ C(15, 20, 10), C(60, 70, 40), C(65, 75, 45), C(68, 75, 45), C(70, 75, 45) }, 0x1F));
	HandPoses.Add(TEXT("PinchRack"), Pose({ C(10, 25, 20), C(45, 60, 40), C(70, 80, 50), C(72, 80, 50), C(75, 80, 50) }, 0x1F));
	HandPoses.Add(TEXT("OverhandRack"), Pose({ C(25, 30, 20), C(50, 55, 30), C(55, 60, 35), C(58, 60, 35), C(60, 60, 35) }, 0x1F));
	// Overhand rake (AK charging handle): fingers nearly straight, lying flat down the right side of the receiver.
	HandPoses.Add(TEXT("OverhandRake"), Pose({ C(-55, 0, 0), C(10, 0, 5), C(10, 0, 5), C(10, 0, 5), C(10, 0, 5) }, 0x1F));
	HandPoses.Add(TEXT("PalmMag"), Pose({ C(30, 30, 15), C(20, 25, 15), C(55, 60, 35), C(58, 60, 35), C(60, 60, 35) }, 0x1F));
	HandPoses.Add(TEXT("ThumbPress"), Pose({ C(0, 0, 0), C(0, 0, 0), C(0, 0, 0), C(0, 0, 0), C(0, 0, 0) }, 0x01));
}

void UHoldoverBodyRig::Setup(USkeletalMeshComponent* InDriver, UPoseableMeshComponent* InVisible, UPoseableMeshComponent* InShadow,
	UHoldoverGunAnimator* InAnimator, UCameraComponent* InCamera)
{
	Driver = InDriver;
	Visible = InVisible;
	Shadow = InShadow;
	Animator = InAnimator;
	Camera = InCamera;
	CachedFor = nullptr;
}

void UHoldoverBodyRig::SetupArms(USkeletalMeshComponent* InArmsDriver, UPoseableMeshComponent* InArms)
{
	ArmsDriver = InArmsDriver;
	Arms = InArms;
}

void UHoldoverBodyRig::ScaleBonesToNothing(UPoseableMeshComponent* Mesh, const TArray<FName>& Bones) const
{
	const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
	if (!Asset)
	{
		return;
	}
	for (const FName& Name : Bones)
	{
		const int32 Index = Asset->GetRefSkeleton().FindBoneIndex(Name);
		if (Mesh->BoneSpaceTransforms.IsValidIndex(Index))
		{
			Mesh->BoneSpaceTransforms[Index].SetScale3D(FVector(0.001f));
		}
	}
}

void UHoldoverBodyRig::UpdateArms(const FTransform& CamTM)
{
	UPoseableMeshComponent* ArmsMesh = Arms.Get();
	USkeletalMeshComponent* Src = ArmsDriver.Get();
	const AHoldoverCharacter* Character = Cast<AHoldoverCharacter>(GetOwner());
	const AHoldoverGun* Gun = Character ? Character->GetGun() : nullptr;
	const UHoldoverGunData* Data = Gun ? Gun->GetData() : nullptr;
	if (!ArmsMesh || !Src || !Data || !ArmsMesh->GetSkinnedAsset() || !Src->GetSkeletalMeshAsset()
		|| ArmsMesh->BoneSpaceTransforms.Num() == 0 || ArmsMesh->GetSkinnedAsset() != CachedFor)
	{
		return;
	}

	LoadHoldPoses();
	if (UpdateArmsFromHold(ArmsMesh, Gun, Data, CamTM))
	{
		return;
	}

	ArmsMesh->CopyPoseFromSkeletalComponent(Src);
	ScaleBonesToNothing(ArmsMesh, ArmsHiddenBones);

	const FReferenceSkeleton& Ref = ArmsMesh->GetSkinnedAsset()->GetRefSkeleton();
	const int32 GripIndex = Ref.FindBoneIndex(ArmsGripBone);
	const int32 HeadIndex = Ref.FindBoneIndex(TEXT("head"));
	TArray<FTransform> CS;
	ComputeComponentSpace(ArmsMesh, CS);
	if (!CS.IsValidIndex(GripIndex) || !RightArm.bValid)
	{
		return;
	}
	// The Manny mesh has no weapon_r bone; the gun frame is the animation's weapon_r measured in hand_r.
	const FTransform& WeaponInHand = Data->bIsPistol ? PistolWeaponInHand : RifleWeaponInHand;
	const FTransform WeaponInArms = WeaponInHand * CS[GripIndex];
	const FTransform LeftInWeapon = LeftArm.bValid ? CS[LeftArm.Hand].GetRelativeTransform(WeaponInArms) : FTransform::Identity;

	// Where the shooting hand must be: the animation's gun frame on the gun's grip frame
	// (at the GripHand socket, turned like the gun: X along the barrel, Z up).
	const FTransform GunTM = Gun->GetActorTransform();
	const FTransform GunGrip(GunTM.GetRotation(), Gun->GetSocketWorldTransform(Data->GripHandSocket).GetLocation());
	const FTransform& Offset = Data->bIsPistol ? PistolArmsGripOffset : RifleArmsGripOffset;
	const FTransform WeaponWorld = Offset * GunGrip;
	FTransform HandRWorld = WeaponInHand.Inverse() * WeaponWorld;
	HandRWorld.SetScale3D(FVector::OneVector);

	// Body of the arms: facing where the eyes look (mesh forward is +Y), head bone just behind the eyes.
	// The body stays upright (turns with the view left and right only), like a real torso; tilting it with the
	// view would swing the shoulders and back into sight when looking down. The arms reach up or down with IK.
	const FRotator ViewRot = CamTM.Rotator();
	const FQuat UprightView = FRotator(ViewRot.Pitch * ArmsBodyPitchFollow, ViewRot.Yaw, 0.f).Quaternion();
	const FQuat ArmsRot = UprightView * FRotator(0.f, -90.f, 0.f).Quaternion();
	const FVector HeadLocal = CS.IsValidIndex(HeadIndex) ? CS[HeadIndex].GetLocation() : FVector(0.f, 0.f, 160.f);
	FVector ArmsLoc = CamTM.GetLocation() + UprightView.RotateVector(Data->bIsPistol ? PistolArmsHeadInCamera : RifleArmsHeadInCamera)
		- ArmsRot.RotateVector(HeadLocal);
	if (Data->bIsPistol ? bPistolArmsFollowGrip : bRifleArmsFollowGrip)
	{
		// Move the arms as one piece so the animation's hand lands on the grip: the arms keep the animation's
		// exact shape (no stretched forearms); the IK below then only fixes small turns.
		ArmsLoc = WeaponWorld.GetLocation() - ArmsRot.RotateVector(WeaponInArms.GetLocation());
	}

	// If the grip is out of reach, slide the arms toward it just enough.
	const float Upper = FVector::Dist(CS[RightArm.Upper].GetLocation(), CS[RightArm.Lower].GetLocation());
	const float Lower = FVector::Dist(CS[RightArm.Lower].GetLocation(), CS[RightArm.Hand].GetLocation());
	const float Reach = (Upper + Lower) * ArmsMaxReach;
	auto SlideToReach = [&](int32 UpperBone, const FVector& Target)
	{
		const FVector Shoulder = ArmsLoc + ArmsRot.RotateVector(CS[UpperBone].GetLocation());
		const FVector ToHand = Target - Shoulder;
		if (ToHand.Size() > Reach)
		{
			ArmsLoc += ToHand.GetSafeNormal() * (ToHand.Size() - Reach);
		}
	};
	SlideToReach(RightArm.Upper, HandRWorld.GetLocation());
	if (LeftArm.bValid && Animator.IsValid() && CS.IsValidIndex(LeftArm.Upper))
	{
		// The support hand must reach the handguard too (wrist a little behind the palm target).
		const FTransform LeftTarget = Animator->GetHandInCamera(true) * CamTM;
		SlideToReach(LeftArm.Upper, LeftTarget.GetLocation() - LeftTarget.GetRotation().GetAxisX() * PalmToWrist);
		SlideToReach(RightArm.Upper, HandRWorld.GetLocation());
	}
	ArmsMesh->SetWorldTransform(FTransform(ArmsRot, ArmsLoc));

	bElbowFromHand = true;
	SolveArmTo(ArmsMesh, RightArm, HandRWorld.GetLocation(), HandRWorld.GetRotation(), false, WeaponWorld);
	bElbowFromHand = false;

	// Support hand: onto the gun's support-hand target (and wherever the gun timelines take it), or where the
	// idle animation holds it relative to the gun.
	UHoldoverGunAnimator* Anim = Animator.Get();
	const bool bLeftIK = Anim && LeftArm.bValid
		&& ((Data->bIsPistol ? bPistolLeftHandIK : bRifleLeftHandIK) || Anim->IsBusy());
	if (!bLeftIK && LeftArm.bValid)
	{
		FTransform LeftOnGun = LeftInWeapon;
		if (bLeftHandSlideToSupport && !Data->bIsPistol)
		{
			// Same hand shape as the animation, slid along the barrel to this gun's handguard.
			const FVector GripLocal = Gun->GetSocketLocal(Data->GripHandSocket);
			const FVector SupportLocal = Gun->GetSocketLocal(Data->SupportHandSocket);
			FVector L = LeftOnGun.GetLocation();
			L.X = SupportLocal.X - GripLocal.X;
			LeftOnGun.SetLocation(L);
		}
		const FTransform HandLWorld = LeftOnGun * WeaponWorld;
		bElbowFromHand = true;
		SolveArmTo(ArmsMesh, LeftArm, HandLWorld.GetLocation(), HandLWorld.GetRotation(), true, HandLWorld);
		bElbowFromHand = false;
	}
	if (bLeftIK)
	{
		// The support elbow hangs down under the gun rather than out to the side, so the forearm stays out of view.
		const float SavedOutward = ElbowOutward;
		ElbowOutward = ArmsLeftElbowOutward;
		FTransform LeftTarget = Anim->GetHandInCamera(true) * CamTM;
		LeftTarget.AddToTranslation(GunTM.GetRotation().RotateVector(Data->bIsPistol ? PistolLeftHandOffset : RifleLeftHandOffset));
		SolveArm(ArmsMesh, LeftArm, LeftTarget, true);
		ElbowOutward = SavedOutward;
		if (bLeftHandCurlFromTimeline)
		{
			CurlFingers(ArmsMesh, LeftArm, Anim->GetFingerWeights(true), true);
		}
	}
	// Apply the new pose now. Only marking it dirty would leave it for the mesh's own tick, which ran earlier this
	// frame, so the arms would show last frame's pose while the gun shows this frame's: the hands trail the gun.
	ArmsMesh->RefreshBoneTransforms();
}

void UHoldoverBodyRig::CacheBones()
{
	UPoseableMeshComponent* Mesh = Visible.Get();
	const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
	if (!Asset || CachedFor == Asset)
	{
		return;
	}
	CachedFor = Asset;

	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	Parents.SetNum(Ref.GetNum());
	for (int32 i = 0; i < Ref.GetNum(); ++i)
	{
		Parents[i] = Ref.GetParentIndex(i);
	}
	HiddenIndex = Ref.FindBoneIndex(HiddenBone);

	// Reference pose in component space, to learn which way each hand's fingers and palm face.
	const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
	TArray<FTransform> RefCS;
	RefCS.SetNum(RefPose.Num());
	for (int32 i = 0; i < RefPose.Num(); ++i)
	{
		RefCS[i] = Parents[i] == INDEX_NONE ? RefPose[i] : RefPose[i] * RefCS[Parents[i]];
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const TCHAR* S = Side == 0 ? TEXT("_l") : TEXT("_r");
		FArmBones& Arm = Side == 0 ? LeftArm : RightArm;
		Arm = FArmBones();
		Arm.Upper = Ref.FindBoneIndex(FName(*FString::Printf(TEXT("upperarm%s"), S)));
		Arm.Lower = Ref.FindBoneIndex(FName(*FString::Printf(TEXT("lowerarm%s"), S)));
		Arm.Hand = Ref.FindBoneIndex(FName(*FString::Printf(TEXT("hand%s"), S)));
		for (int32 f = 0; f < 5; ++f)
		{
			for (int32 j = 1; j <= 3; ++j)
			{
				Arm.Fingers.Add(Ref.FindBoneIndex(FName(*FString::Printf(TEXT("%s_0%d%s"), GFingerNames[f], j, S))));
			}
		}
		Arm.bValid = Arm.Upper != INDEX_NONE && Arm.Lower != INDEX_NONE && Arm.Hand != INDEX_NONE;
		if (!Arm.bValid)
		{
			continue;
		}

		const FTransform& HandCS = RefCS[Arm.Hand];
		const int32 Middle = Arm.Fingers[2 * 3];
		const int32 Index = Arm.Fingers[1 * 3];
		const int32 Pinky = Arm.Fingers[4 * 3];
		FVector FingersCS = (Middle != INDEX_NONE) ? (RefCS[Middle].GetLocation() - HandCS.GetLocation()) : HandCS.GetRotation().GetAxisX();
		FingersCS.Normalize();
		FVector Across = (Index != INDEX_NONE && Pinky != INDEX_NONE)
			? (RefCS[Index].GetLocation() - RefCS[Pinky].GetLocation()) : HandCS.GetRotation().GetAxisY();
		FVector PalmCS = FVector::CrossProduct(FingersCS, Across).GetSafeNormal();
		// The mannequin's reference pose has the palms facing down.
		if (PalmCS.Z > 0.f)
		{
			PalmCS = -PalmCS;
		}
		Arm.LocalFingers = HandCS.GetRotation().UnrotateVector(FingersCS);
		Arm.LocalPalm = HandCS.GetRotation().UnrotateVector(PalmCS);
		if (Side == 0 && bFlipLeftPalm)
		{
			Arm.LocalPalm = -Arm.LocalPalm;
		}
	}
}

void UHoldoverBodyRig::ComputeComponentSpace(UPoseableMeshComponent* Mesh, TArray<FTransform>& Out) const
{
	const TArray<FTransform>& Local = Mesh->BoneSpaceTransforms;
	Out.SetNum(Local.Num());
	for (int32 i = 0; i < Local.Num(); ++i)
	{
		const int32 P = Parents.IsValidIndex(i) ? Parents[i] : INDEX_NONE;
		Out[i] = P == INDEX_NONE ? Local[i] : Local[i] * Out[P];
	}
}

void UHoldoverBodyRig::PoseCopy(UPoseableMeshComponent* Mesh, bool bHideHead)
{
	Mesh->CopyPoseFromSkeletalComponent(Driver.Get());
	if (bHideHead && Mesh->BoneSpaceTransforms.IsValidIndex(HiddenIndex))
	{
		Mesh->BoneSpaceTransforms[HiddenIndex].SetScale3D(FVector(0.001f));
	}
}

void UHoldoverBodyRig::SolveArm(UPoseableMeshComponent* Mesh, const FArmBones& Arm, const FTransform& TargetWorld, bool bLeft)
{
	TArray<FTransform> CS;
	ComputeComponentSpace(Mesh, CS);
	if (!CS.IsValidIndex(Arm.Upper) || !CS.IsValidIndex(Arm.Lower) || !CS.IsValidIndex(Arm.Hand))
	{
		return;
	}
	const FTransform CompTM = Mesh->GetComponentTransform();
	const FTransform UpperW = CS[Arm.Upper] * CompTM;
	const FTransform LowerW = CS[Arm.Lower] * CompTM;
	const FTransform HandW = CS[Arm.Hand] * CompTM;

	const FVector A = UpperW.GetLocation();
	const FVector B = LowerW.GetLocation();
	const FVector C = HandW.GetLocation();
	const float L1 = FVector::Dist(A, B);
	const float L2 = FVector::Dist(B, C);
	if (L1 < KINDA_SMALL_NUMBER || L2 < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Hand orientation and wrist position from the palm target.
	const FQuat TargetBasis = TargetWorld.GetRotation();
	const FQuat LocalBasis = HoldoverAnim::HandBasis(Arm.LocalFingers, Arm.LocalPalm);
	const FQuat HandRot = TargetBasis * LocalBasis.Inverse();
	const FVector FingersW = TargetBasis.GetAxisX();
	const FVector PalmW = TargetBasis.GetAxisZ();
	const FVector Wrist = TargetWorld.GetLocation() - FingersW * PalmToWrist - PalmW * PalmDepth;
	SolveArmTo(Mesh, Arm, Wrist, HandRot, bLeft, TargetWorld);
}

void UHoldoverBodyRig::SolveArmTo(UPoseableMeshComponent* Mesh, const FArmBones& Arm, const FVector& Wrist, const FQuat& HandRot, bool bLeft,
	const FTransform& DebugTarget)
{
	TArray<FTransform> CS;
	ComputeComponentSpace(Mesh, CS);
	if (!CS.IsValidIndex(Arm.Upper) || !CS.IsValidIndex(Arm.Lower) || !CS.IsValidIndex(Arm.Hand))
	{
		return;
	}
	const FTransform CompTM = Mesh->GetComponentTransform();
	const FTransform UpperW = CS[Arm.Upper] * CompTM;
	const FTransform LowerW = CS[Arm.Lower] * CompTM;
	const FTransform HandW = CS[Arm.Hand] * CompTM;
	const FVector A = UpperW.GetLocation();
	const FVector B = LowerW.GetLocation();
	const FVector C = HandW.GetLocation();
	const float L1 = FVector::Dist(A, B);
	const float L2 = FVector::Dist(B, C);
	if (L1 < KINDA_SMALL_NUMBER || L2 < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FTransform TargetWorld = DebugTarget;

	// Two-bone IK by the law of cosines.
	FVector ToWrist = Wrist - A;
	const float Reach = ToWrist.Size();
	const FVector U = Reach > KINDA_SMALL_NUMBER ? ToWrist / Reach : (C - A).GetSafeNormal();
	const float D = FMath::Clamp(Reach, FMath::Abs(L1 - L2) + 0.1f, L1 + L2 - 0.01f);

	const AActor* Owner = GetOwner();
	const FVector Right = Owner ? Owner->GetActorRightVector() : FVector::RightVector;
	const FVector Fwd = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
	FVector Hint = FVector(0.f, 0.f, -1.f) + Right * (bLeft ? -ElbowOutward : ElbowOutward) - Fwd * 0.2f;
	if (bElbowFromHand)
	{
		// Put the elbow where the source pose's forearm points from this hand, so the wrist keeps its natural bend
		// (a sharply bent wrist tears the skin around it).
		const FVector ForearmInHand = HandW.GetRotation().UnrotateVector(B - C).GetSafeNormal();
		const FVector WantedElbow = Wrist + HandRot.RotateVector(ForearmInHand) * L2;
		const FVector ToElbow = WantedElbow - A;
		if (!ToElbow.IsNearlyZero())
		{
			const float W = FMath::Clamp(ElbowFromHandWeight, 0.f, 1.f);
			Hint = W >= 1.f ? ToElbow : FMath::Lerp(Hint.GetSafeNormal(), ToElbow.GetSafeNormal(), W);
		}
	}
	FVector N = Hint - U * FVector::DotProduct(Hint, U);
	if (!N.Normalize())
	{
		N = FVector(0.f, 0.f, -1.f);
	}

	const float CosA = FMath::Clamp((L1 * L1 + D * D - L2 * L2) / (2.f * L1 * D), -1.f, 1.f);
	const float SinA = FMath::Sqrt(FMath::Max(0.f, 1.f - CosA * CosA));
	const FVector Elbow = A + U * (L1 * CosA) + N * (L1 * SinA);
	const FVector HandPos = A + U * D;

	const FQuat D1 = FQuat::FindBetweenNormals((B - A).GetSafeNormal(), (Elbow - A).GetSafeNormal());
	const FQuat D2 = FQuat::FindBetweenNormals(D1.RotateVector(C - B).GetSafeNormal(), (HandPos - Elbow).GetSafeNormal());

	FTransform NewUpper = UpperW;
	NewUpper.SetRotation(D1 * UpperW.GetRotation());
	FTransform NewLower = LowerW;
	NewLower.SetRotation(D2 * D1 * LowerW.GetRotation());
	NewLower.SetLocation(Elbow);
	FTransform NewHand = HandW;
	NewHand.SetRotation(HandRot);
	NewHand.SetLocation(HandPos);

	// Roll the forearm about its own length so the wrist keeps the twist it had in the source pose; otherwise
	// the forearm skin wrings like a candy wrapper when the hand is turned a long way.
	{
		const FQuat SourceRel = LowerW.GetRotation().Inverse() * HandW.GetRotation();
		const FQuat NewRel = NewLower.GetRotation().Inverse() * HandRot;
		const FQuat Delta = (NewRel * SourceRel.Inverse()).GetNormalized();
		const FVector Axis = NewLower.GetRotation().UnrotateVector(HandPos - Elbow).GetSafeNormal();
		if (!Axis.IsNearlyZero())
		{
			const FVector V(Delta.X, Delta.Y, Delta.Z);
			const FVector P = Axis * FVector::DotProduct(V, Axis);
			FQuat Twist(P.X, P.Y, P.Z, Delta.W);
			if (Twist.SizeSquared() > KINDA_SMALL_NUMBER)
			{
				Twist.Normalize();
				NewLower.SetRotation((NewLower.GetRotation() * Twist).GetNormalized());
			}
		}
	}

	// Back to parent-relative transforms (the clavicle above the upper arm is unchanged).
	const FTransform InvComp = CompTM.Inverse();
	const FTransform UpperCS = NewUpper * InvComp;
	const FTransform LowerCS = NewLower * InvComp;
	const FTransform HandCS = NewHand * InvComp;
	const int32 UpperParent = Parents[Arm.Upper];
	const FTransform UpperParentCS = UpperParent == INDEX_NONE ? FTransform::Identity : CS[UpperParent];

	auto SetLocal = [Mesh](int32 Index, const FTransform& ChildCS, const FTransform& ParentCS)
	{
		FTransform L = ChildCS.GetRelativeTransform(ParentCS);
		L.SetScale3D(Mesh->BoneSpaceTransforms[Index].GetScale3D());
		Mesh->BoneSpaceTransforms[Index] = L;
	};
	SetLocal(Arm.Upper, UpperCS, UpperParentCS);
	// Lower arm and hand are expected to be direct children (true for the UE5 mannequin);
	// twist bones in between keep their copied pose.
	SetLocal(Arm.Lower, LowerCS, Parents[Arm.Lower] == Arm.Upper ? UpperCS : CS[Parents[Arm.Lower]]);
	SetLocal(Arm.Hand, HandCS, Parents[Arm.Hand] == Arm.Lower ? LowerCS : CS[Parents[Arm.Hand]]);

	if (HoldoverDebug::Hands())
	{
		UWorld* World = GetWorld();
		DrawDebugLine(World, A, Elbow, FColor::Cyan, false, 0.f, 0, 0.3f);
		DrawDebugLine(World, Elbow, HandPos, FColor::Cyan, false, 0.f, 0, 0.3f);
		DrawDebugCoordinateSystem(World, TargetWorld.GetLocation(), TargetWorld.Rotator(), 4.f, false, 0.f, 0, 0.2f);
	}
}

void UHoldoverBodyRig::CurlFingers(UPoseableMeshComponent* Mesh, const FArmBones& Arm, const TMap<FName, float>& Weights, bool bLeft)
{
	if (Arm.Fingers.Num() != 15)
	{
		return;
	}

	// Full-hand poses blend by weight (topped up with Relaxed); partial poses (trigger finger,
	// thumb press) then override their own fingers by their weight.
	float Curl[15];
	float Total = 0.f;
	for (int32 i = 0; i < 15; ++i)
	{
		Curl[i] = 0.f;
	}
	for (const auto& W : Weights)
	{
		const FHoldoverHandPose* P = HandPoses.Find(W.Key);
		if (!P || P->FingerMask != 0x1F || W.Value <= 0.f || P->Fingers.Num() < 5)
		{
			continue;
		}
		Total += W.Value;
		for (int32 f = 0; f < 5; ++f)
		{
			Curl[f * 3 + 0] += P->Fingers[f].J1 * W.Value;
			Curl[f * 3 + 1] += P->Fingers[f].J2 * W.Value;
			Curl[f * 3 + 2] += P->Fingers[f].J3 * W.Value;
		}
	}
	const FHoldoverHandPose* Relaxed = HandPoses.Find(TEXT("Relaxed"));
	if (Total < 1.f && Relaxed && Relaxed->Fingers.Num() >= 5)
	{
		const float Fill = 1.f - Total;
		for (int32 f = 0; f < 5; ++f)
		{
			Curl[f * 3 + 0] += Relaxed->Fingers[f].J1 * Fill;
			Curl[f * 3 + 1] += Relaxed->Fingers[f].J2 * Fill;
			Curl[f * 3 + 2] += Relaxed->Fingers[f].J3 * Fill;
		}
		Total = 1.f;
	}
	for (int32 i = 0; i < 15; ++i)
	{
		Curl[i] /= FMath::Max(Total, KINDA_SMALL_NUMBER);
	}
	for (const auto& W : Weights)
	{
		const FHoldoverHandPose* P = HandPoses.Find(W.Key);
		if (!P || P->FingerMask == 0x1F || W.Value <= 0.f || P->Fingers.Num() < 5)
		{
			continue;
		}
		const float A = FMath::Clamp(W.Value, 0.f, 1.f);
		for (int32 f = 0; f < 5; ++f)
		{
			if (P->FingerMask & (1 << f))
			{
				Curl[f * 3 + 0] = FMath::Lerp(Curl[f * 3 + 0], P->Fingers[f].J1, A);
				Curl[f * 3 + 1] = FMath::Lerp(Curl[f * 3 + 1], P->Fingers[f].J2, A);
				Curl[f * 3 + 2] = FMath::Lerp(Curl[f * 3 + 2], P->Fingers[f].J3, A);
			}
		}
	}

	const FVector Axis = (bLeft ? FingerCurlAxisLeft : FingerCurlAxisRight).GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		return;
	}
	const TArray<FTransform>& RefPose = Mesh->GetSkinnedAsset()->GetRefSkeleton().GetRefBonePose();
	for (int32 i = 0; i < 15; ++i)
	{
		const int32 Bone = Arm.Fingers[i];
		if (Bone == INDEX_NONE || !Mesh->BoneSpaceTransforms.IsValidIndex(Bone) || !RefPose.IsValidIndex(Bone))
		{
			continue;
		}
		// Start from the reference finger pose so the curls are absolute, not added to the idle animation.
		FTransform& L = Mesh->BoneSpaceTransforms[Bone];
		L.SetRotation(RefPose[Bone].GetRotation() * FQuat(Axis, FMath::DegreesToRadians(Curl[i])));
	}
}

void UHoldoverBodyRig::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USkeletalMeshComponent* Src = Driver.Get();
	UPoseableMeshComponent* Vis = Visible.Get();
	if (!Src || !Vis || !Src->GetSkeletalMeshAsset() || !Vis->GetSkinnedAsset())
	{
		return;
	}
	CacheBones();

	UHoldoverGunAnimator* Anim = Animator.Get();
	UCameraComponent* Cam = Camera.Get();
	const bool bIK = bArmsOnGun && Anim && Cam;
	const FTransform CamTM = Cam ? Cam->GetComponentTransform() : FTransform::Identity;

	for (UPoseableMeshComponent* Mesh : { Vis, Shadow.Get() })
	{
		if (!Mesh || !Mesh->GetSkinnedAsset() || Mesh->BoneSpaceTransforms.Num() == 0)
		{
			continue;
		}
		PoseCopy(Mesh, Mesh == Vis);
		const bool bHiddenArms = (Mesh == Vis) && bHideBodyArms && Arms.IsValid();
		if (bHiddenArms)
		{
			// The first-person arms replace these; the arm IK code stays for the shadow body.
			ScaleBonesToNothing(Mesh, BodyArmBones);
		}
		if (bIK && !bHiddenArms)
		{
			if (LeftArm.bValid)
			{
				SolveArm(Mesh, LeftArm, Anim->GetHandInCamera(true) * CamTM, true);
				CurlFingers(Mesh, LeftArm, Anim->GetFingerWeights(true), true);
			}
			if (RightArm.bValid)
			{
				SolveArm(Mesh, RightArm, Anim->GetHandInCamera(false) * CamTM, false);
				CurlFingers(Mesh, RightArm, Anim->GetFingerWeights(false), false);
			}
		}
		Mesh->RefreshBoneTransforms(); // now, not at the mesh's next tick (one frame late)
	}

	UpdateArms(CamTM);
}

// =====================================================================
// Hold poses (Data/HoldPoses/<Pistol|Rifle>.json, posed in Blender around each gun model)
// =====================================================================

namespace
{
	bool ReadVec(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(Field, Arr) || !Arr || Arr->Num() != 3)
		{
			return false;
		}
		Out = FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
		return true;
	}

	/** Rotation from its three turned axes (where X, Y and Z end up). */
	FQuat QuatFromAxes(const FVector& X, const FVector& Y, const FVector& Z)
	{
		return FQuat(FMatrix(X, Y, Z, FVector::ZeroVector)).GetNormalized();
	}
}

float UHoldoverBodyRig::GetHoldEyeRelief(bool bPistol) const
{
	const FHoldPose& Hold = bPistol ? PistolHold : RifleHold;
	return (bUseHoldPoses && Hold.bValid) ? Hold.EyeRelief : 0.f;
}

void UHoldoverBodyRig::LoadHoldPoses()
{
	const UPoseableMeshComponent* ArmsMesh = Arms.Get();
	const USkinnedAsset* Asset = ArmsMesh ? ArmsMesh->GetSkinnedAsset() : nullptr;
	if (!Asset || HoldCachedFor == Asset)
	{
		return;
	}
	HoldCachedFor = Asset;
	PistolHold = FHoldPose();
	RifleHold = FHoldPose();
	LoadHoldPose(TEXT("Pistol"), PistolHold);
	LoadHoldPose(TEXT("Rifle"), RifleHold);
}

bool UHoldoverBodyRig::LoadHoldPose(const FString& Name, FHoldPose& Out) const
{
	const UPoseableMeshComponent* ArmsMesh = Arms.Get();
	const USkinnedAsset* Asset = ArmsMesh ? ArmsMesh->GetSkinnedAsset() : nullptr;
	if (!Asset)
	{
		return false;
	}
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("HoldPoses"), Name + TEXT(".json"));
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogHoldover, Warning, TEXT("Hold pose %s not found or unreadable; the arms use the idle animation."), *Path);
		return false;
	}
	const TSharedPtr<FJsonObject>* Bones = nullptr;
	const TSharedPtr<FJsonObject>* GunObj = nullptr;
	FVector Eye;
	if (!Root->TryGetObjectField(TEXT("bones"), Bones) || !Root->TryGetObjectField(TEXT("gun"), GunObj) || !ReadVec(Root, TEXT("eye"), Eye))
	{
		UE_LOG(LogHoldover, Warning, TEXT("Hold pose %s is missing bones, gun or eye."), *Path);
		return false;
	}

	// Component space: each bone turned from its reference pose by the stored rotation, at the stored place.
	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
	const int32 Num = Ref.GetNum();
	TArray<FTransform> RefCS, CS;
	RefCS.SetNum(Num);
	CS.SetNum(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		const int32 P = Ref.GetParentIndex(i);
		RefCS[i] = P == INDEX_NONE ? RefPose[i] : RefPose[i] * RefCS[P];
		const TSharedPtr<FJsonObject>* B = nullptr;
		FVector Loc, DX, DY, DZ;
		if ((*Bones)->TryGetObjectField(Ref.GetBoneName(i).ToString(), B) && ReadVec(*B, TEXT("loc"), Loc)
			&& ReadVec(*B, TEXT("dx"), DX) && ReadVec(*B, TEXT("dy"), DY) && ReadVec(*B, TEXT("dz"), DZ))
		{
			CS[i] = FTransform((QuatFromAxes(DX, DY, DZ) * RefCS[i].GetRotation()).GetNormalized(), Loc);
		}
		else
		{
			CS[i] = P == INDEX_NONE ? RefPose[i] : RefPose[i] * CS[P];
			CS[i].SetScale3D(FVector::OneVector);
		}
	}
	Out.Local.SetNum(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		const int32 P = Ref.GetParentIndex(i);
		Out.Local[i] = P == INDEX_NONE ? CS[i] : CS[i].GetRelativeTransform(CS[P]);
		Out.Local[i].SetScale3D(FVector::OneVector);
	}

	FVector GLoc, GX, GY, GZ;
	if (!ReadVec(*GunObj, TEXT("loc"), GLoc) || !ReadVec(*GunObj, TEXT("x"), GX) || !ReadVec(*GunObj, TEXT("y"), GY) || !ReadVec(*GunObj, TEXT("z"), GZ))
	{
		return false;
	}
	const FTransform GunCS(QuatFromAxes(GX, GY, GZ), GLoc);
	const int32 HandR = Ref.FindBoneIndex(TEXT("hand_r"));
	const int32 HandL = Ref.FindBoneIndex(TEXT("hand_l"));
	if (HandR == INDEX_NONE || HandL == INDEX_NONE)
	{
		return false;
	}
	Out.HandRInGun = CS[HandR].GetRelativeTransform(GunCS);
	Out.HandLInGun = CS[HandL].GetRelativeTransform(GunCS);
	Out.Eye = Eye;
	Out.EyeRelief = static_cast<float>(Root->GetNumberField(TEXT("eye_relief_cm")));
	Out.bValid = true;
	UE_LOG(LogHoldover, Log, TEXT("Loaded hold pose %s (eye relief %.1f cm)."), *Path, Out.EyeRelief);
	return true;
}

bool UHoldoverBodyRig::UpdateArmsFromHold(UPoseableMeshComponent* ArmsMesh, const AHoldoverGun* Gun, const UHoldoverGunData* Data,
	const FTransform& CamTM)
{
	const FHoldPose& Hold = Data->bIsPistol ? PistolHold : RifleHold;
	if (!bUseHoldPoses || !Hold.bValid || !Gun->UsesSkeletalMesh() || Hold.Local.Num() != ArmsMesh->BoneSpaceTransforms.Num()
		|| !RightArm.bValid || !LeftArm.bValid)
	{
		return false;
	}

	// The whole hand-made pose: body, arms, hands and fingers.
	ArmsMesh->BoneSpaceTransforms = Hold.Local;
	ScaleBonesToNothing(ArmsMesh, ArmsHiddenBones);
	TArray<FTransform> CS;
	ComputeComponentSpace(ArmsMesh, CS);

	// Body upright and facing where the eyes look (mesh forward is +Y), with the pose's eye at the camera.
	const FRotator ViewRot = CamTM.Rotator();
	const FQuat UprightView = FRotator(ViewRot.Pitch * ArmsBodyPitchFollow, ViewRot.Yaw, 0.f).Quaternion();
	const FQuat ArmsRot = UprightView * FRotator(0.f, -90.f, 0.f).Quaternion();
	FVector ArmsLoc = CamTM.GetLocation() - ArmsRot.RotateVector(Hold.Eye);

	// Both hands exactly where the pose has them on the gun.
	const FTransform GunTM = Gun->GetMeshWorldTransform();
	FTransform HandRWorld = Hold.HandRInGun * GunTM;
	HandRWorld.SetScale3D(FVector::OneVector);
	FTransform HandLWorld = Hold.HandLInGun * GunTM;
	HandLWorld.SetScale3D(FVector::OneVector);

	// If a hand is out of reach (gun pushed out, recoil), slide the body toward it just enough. An arm the pose
	// holds nearly straight (the pistol's isosceles) may stay as straight as posed, so its elbow and wrist keep
	// the posed angles.
	const float Upper = FVector::Dist(CS[RightArm.Upper].GetLocation(), CS[RightArm.Lower].GetLocation());
	const float Lower = FVector::Dist(CS[RightArm.Lower].GetLocation(), CS[RightArm.Hand].GetLocation());
	const float Reach = (Upper + Lower) * ArmsMaxReach;
	auto SlideToReach = [&](const FArmBones& Arm, const FVector& Target)
	{
		const float PosedReach = FVector::Dist(CS[Arm.Upper].GetLocation(), CS[Arm.Hand].GetLocation()) + 0.5f;
		const float Limit = FMath::Min(FMath::Max(Reach, PosedReach), Upper + Lower - 0.05f);
		const FVector Shoulder = ArmsLoc + ArmsRot.RotateVector(CS[Arm.Upper].GetLocation());
		const FVector ToHand = Target - Shoulder;
		if (ToHand.Size() > Limit)
		{
			ArmsLoc += ToHand.GetSafeNormal() * (ToHand.Size() - Limit);
		}
	};
	// The support hand keeps its hand-made place on the gun until a gun action (racking, reloads, checks) moves it;
	// then it blends to the timeline's target by how far the timeline has taken it away, so it never pops.
	UHoldoverGunAnimator* Anim = Animator.Get();
	const float Away = (Anim && Anim->IsBusy()) ? FMath::Clamp(Anim->GetHandAway(true), 0.f, 1.f) : 0.f;
	FVector LeftWrist = HandLWorld.GetLocation();
	FQuat LeftRot = HandLWorld.GetRotation();
	FTransform LeftDebug = HandLWorld;
	if (Away > 0.f)
	{
		FTransform LeftTarget = Anim->GetHandInCamera(true) * CamTM;
		LeftTarget.AddToTranslation(GunTM.GetRotation().RotateVector(Data->bIsPistol ? PistolLeftHandOffset : RifleLeftHandOffset));
		const FQuat TargetBasis = LeftTarget.GetRotation();
		const FQuat TimelineRot = TargetBasis * HoldoverAnim::HandBasis(LeftArm.LocalFingers, LeftArm.LocalPalm).Inverse();
		const FVector TimelineWrist = LeftTarget.GetLocation() - TargetBasis.GetAxisX() * PalmToWrist - TargetBasis.GetAxisZ() * PalmDepth;
		LeftWrist = FMath::Lerp(LeftWrist, TimelineWrist, Away);
		LeftRot = FQuat::Slerp(LeftRot, TimelineRot, Away).GetNormalized();
		LeftDebug = LeftTarget;
	}

	SlideToReach(RightArm, HandRWorld.GetLocation());
	SlideToReach(LeftArm, LeftWrist);
	SlideToReach(RightArm, HandRWorld.GetLocation());
	ArmsMesh->SetWorldTransform(FTransform(ArmsRot, ArmsLoc));

	bElbowFromHand = true;
	SolveArmTo(ArmsMesh, RightArm, HandRWorld.GetLocation(), HandRWorld.GetRotation(), false, HandRWorld);
	const float SavedOutward = ElbowOutward;
	ElbowOutward = ArmsLeftElbowOutward;
	ElbowFromHandWeight = FMath::Lerp(1.f, TimelineElbowFromHand, Away);
	SolveArmTo(ArmsMesh, LeftArm, LeftWrist, LeftRot, true, LeftDebug);
	ElbowFromHandWeight = 1.f;
	ElbowOutward = SavedOutward;
	bElbowFromHand = false;

	// Fingers: the hand-made grip, blended toward the timeline's finger pose as the hand leaves.
	if (Away > 0.f && bLeftHandCurlFromTimeline && LeftArm.Fingers.Num() == 15)
	{
		FQuat Held[15];
		for (int32 i = 0; i < 15; ++i)
		{
			const int32 Bone = LeftArm.Fingers[i];
			Held[i] = ArmsMesh->BoneSpaceTransforms.IsValidIndex(Bone) ? ArmsMesh->BoneSpaceTransforms[Bone].GetRotation() : FQuat::Identity;
		}
		CurlFingers(ArmsMesh, LeftArm, Anim->GetFingerWeights(true), true);
		for (int32 i = 0; i < 15; ++i)
		{
			const int32 Bone = LeftArm.Fingers[i];
			if (ArmsMesh->BoneSpaceTransforms.IsValidIndex(Bone))
			{
				FTransform& L = ArmsMesh->BoneSpaceTransforms[Bone];
				L.SetRotation(FQuat::Slerp(Held[i], L.GetRotation(), Away).GetNormalized());
			}
		}
	}
	// Apply the new pose now. Only marking it dirty would leave it for the mesh's own tick, which ran earlier this
	// frame, so the arms would show last frame's pose while the gun shows this frame's: the hands trail the gun.
	ArmsMesh->RefreshBoneTransforms();
	if (HoldoverDebug::TestShots() > 0)
	{
		// What the renderer gets this frame (the refreshed bones) against where the wrists belong now.
		const float MissR = FVector::Dist(ArmsMesh->GetBoneLocation(ArmsMesh->GetBoneName(RightArm.Hand)), HandRWorld.GetLocation());
		const float MissL = FVector::Dist(ArmsMesh->GetBoneLocation(ArmsMesh->GetBoneName(LeftArm.Hand)), LeftWrist);
		MaxHandMiss = FMath::Max3(MaxHandMiss, MissR, MissL);
	}
	return true;
}
