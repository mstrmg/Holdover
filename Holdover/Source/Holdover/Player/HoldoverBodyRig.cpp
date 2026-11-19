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

	// Two-bone IK by the law of cosines.
	FVector ToWrist = Wrist - A;
	const float Reach = ToWrist.Size();
	const FVector U = Reach > KINDA_SMALL_NUMBER ? ToWrist / Reach : (C - A).GetSafeNormal();
	const float D = FMath::Clamp(Reach, FMath::Abs(L1 - L2) + 0.1f, L1 + L2 - 0.01f);

	const AActor* Owner = GetOwner();
	const FVector Right = Owner ? Owner->GetActorRightVector() : FVector::RightVector;
	const FVector Fwd = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
	const FVector Hint = FVector(0.f, 0.f, -1.f) + Right * (bLeft ? -ElbowOutward : ElbowOutward) - Fwd * 0.2f;
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
		if (bIK)
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
		Mesh->MarkRefreshTransformDirty();
	}
}
