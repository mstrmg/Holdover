#include "Weapons/HoldoverGun.h"

#include "Audio/HoldoverAudio.h"
#include "Ballistics/HoldoverBallisticsSubsystem.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HoldoverDebug.h"
#include "Core/HoldoverShapes.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Holdover.h"
#include "Kismet/GameplayStatics.h"
#include "ReferenceSkeleton.h"
#include "Sound/SoundBase.h"
#include "Weapons/HoldoverDebrisSubsystem.h"
#include "Weapons/HoldoverGunData.h"
#include "Weapons/HoldoverLoosePiece.h"
#include "Weapons/HoldoverMuzzleFX.h"

AHoldoverGun::AHoldoverGun()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;

	ShadowMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("ShadowMesh"));
	ShadowMesh->SetupAttachment(Root);
	ShadowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShadowMesh->SetCastShadow(true);
	ShadowMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	MuzzleFX = CreateDefaultSubobject<UHoldoverMuzzleFX>(TEXT("MuzzleFX"));
}

void AHoldoverGun::BeginPlay()
{
	Super::BeginPlay();
	if (!Data)
	{
		InitGun(nullptr);
	}
}

// =====================================================================
// Setup
// =====================================================================

void AHoldoverGun::InitGun(UHoldoverGunData* InData)
{
	Data = InData ? InData : NewObject<UHoldoverGunData>(this);

	// Remove any gray-box parts from an earlier setup.
	TArray<UActorComponent*> OldParts = GetInstanceComponents();
	for (UActorComponent* Part : OldParts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	FallbackBones.Reset();
	FallbackSockets.Reset();
	FallbackRest.Reset();
	RefLocal.Reset();
	RefComponent.Reset();

	bUseSkeletal = (Data->GunMesh != nullptr);
	if (bUseSkeletal)
	{
		BuildSkeletalVisual();
	}
	else
	{
		Mesh->SetVisibility(false);
		ShadowMesh->SetVisibility(false);
		BuildFallbackVisual();
	}

	ResetState(nullptr);
}

void AHoldoverGun::BuildSkeletalVisual()
{
	USkeletalMesh* SkMesh = Data->GunMesh;
	for (UPoseableMeshComponent* Comp : { Mesh.Get(), ShadowMesh.Get() })
	{
		Comp->SetSkinnedAssetAndUpdate(SkMesh, true);
		Comp->SetRelativeRotation(Data->MeshImportRotation);
		Comp->SetVisibility(true);
	}

	// Cache the rest pose so bone motions can be expressed in gun space.
	const FReferenceSkeleton& RefSkel = SkMesh->GetRefSkeleton();
	const TArray<FTransform>& Pose = RefSkel.GetRefBonePose();
	const int32 Num = Pose.Num();
	RefLocal = Pose;
	RefComponent.SetNum(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		const int32 Parent = RefSkel.GetParentIndex(i);
		RefComponent[i] = (Parent == INDEX_NONE) ? Pose[i] : Pose[i] * RefComponent[Parent];
	}
}

USceneComponent* AHoldoverGun::AddFallbackPart(FName Bone, USceneComponent* Parent, const FVector& Pivot)
{
	USceneComponent* Part = NewObject<USceneComponent>(this);
	Part->SetupAttachment(Parent ? Parent : Root.Get());
	Part->SetRelativeLocation(Pivot);
	Part->RegisterComponent();
	AddInstanceComponent(Part);
	if (!Bone.IsNone())
	{
		FallbackBones.Add(Bone, Part);
		FallbackRest.Add(Bone, Part->GetRelativeTransform());
	}
	return Part;
}

void AHoldoverGun::BuildFallbackVisual()
{
	UStaticMesh* Cube = HoldoverShapes::Cube();
	UStaticMesh* Cyl = HoldoverShapes::Cylinder();
	const FLinearColor Steel = HoldoverShapes::GunSteel;
	const FLinearColor Dark(0.02f, 0.02f, 0.02f);
	const FLinearColor Wood = HoldoverShapes::Wood;

	auto Shape = [this](USceneComponent* Parent, UStaticMesh* M, const FVector& Loc, const FRotator& Rot, const FVector& Size, const FLinearColor& C)
	{
		UStaticMeshComponent* S = HoldoverShapes::AddShape(this, Parent, M, Loc, Rot, Size, C, false);
		if (S)
		{
			S->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
			S->SetCastShadow(false);
		}
		return S;
	};
	auto Socket = [this](FName Name, USceneComponent* Parent, const FVector& Loc)
	{
		USceneComponent* S = AddFallbackPart(NAME_None, Parent, Loc);
		FallbackSockets.Add(Name, S);
	};

	if (Data->bIsPistol)
	{
		// --- Gray-box pistol, about M9 size. Rear of slide at X = 0, bore at Z = 0. ---
		USceneComponent* Frame = AddFallbackPart(NAME_None, Root, FVector::ZeroVector);
		Shape(Frame, Cube, FVector(12.f, 0.f, -1.6f), FRotator::ZeroRotator, FVector(12.f, 2.6f, 1.2f), Steel);      // dust cover
		Shape(Frame, Cube, FVector(3.5f, 0.f, -7.15f), FRotator(-15.f, 0.f, 0.f), FVector(3.2f, 3.0f, 11.5f), Wood);   // grip
		Shape(Frame, Cube, FVector(8.6f, 0.f, -4.1f), FRotator::ZeroRotator, FVector(5.0f, 0.6f, 0.4f), Steel);       // trigger guard bottom
		Shape(Frame, Cube, FVector(11.0f, 0.f, -2.9f), FRotator::ZeroRotator, FVector(0.4f, 0.6f, 2.4f), Steel);      // trigger guard front
		Shape(Frame, Cyl, FVector(20.2f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), FVector(1.1f, 1.1f, 1.2f), Dark);      // barrel crown

		USceneComponent* Slide = AddFallbackPart(TEXT("Slide"), Root, FVector::ZeroVector);
		Shape(Slide, Cube, FVector(9.85f, 0.f, 0.3f), FRotator::ZeroRotator, FVector(19.7f, 2.9f, 2.6f), Steel);
		Shape(Slide, Cube, FVector(19.2f, 0.f, 1.8f), FRotator::ZeroRotator, FVector(0.4f, 0.3f, 0.4f), Dark);       // front sight
		Shape(Slide, Cube, FVector(1.0f, -0.55f, 1.8f), FRotator::ZeroRotator, FVector(0.5f, 0.7f, 0.4f), Dark);     // rear sight, left
		Shape(Slide, Cube, FVector(1.0f, 0.55f, 1.8f), FRotator::ZeroRotator, FVector(0.5f, 0.7f, 0.4f), Dark);      // rear sight, right
		Shape(Slide, Cube, FVector(7.5f, 1.46f, 0.7f), FRotator::ZeroRotator, FVector(5.5f, 0.05f, 1.2f), Dark);     // ejection port
		Socket(TEXT("SightRear"), Slide, FVector(1.0f, 0.f, 2.0f));
		Socket(TEXT("SightFront"), Slide, FVector(19.2f, 0.f, 2.0f));
		Socket(TEXT("EjectionPort"), Slide, FVector(7.5f, 1.5f, 0.7f));

		USceneComponent* Safety = AddFallbackPart(TEXT("Safety"), Slide, FVector(1.8f, -1.6f, 0.8f));
		Shape(Safety, Cube, FVector(-0.4f, -0.1f, 0.f), FRotator::ZeroRotator, FVector(1.0f, 0.4f, 0.5f), Dark);

		USceneComponent* Hammer = AddFallbackPart(TEXT("Hammer"), Root, FVector(0.2f, 0.f, -2.2f));
		Shape(Hammer, Cube, FVector(0.f, 0.f, 1.2f), FRotator::ZeroRotator, FVector(0.6f, 0.8f, 1.6f), Dark);

		USceneComponent* Trigger = AddFallbackPart(TEXT("Trigger"), Root, FVector(7.5f, 0.f, -2.5f));
		Shape(Trigger, Cube, FVector(0.f, 0.f, -0.8f), FRotator::ZeroRotator, FVector(0.5f, 0.5f, 1.6f), Dark);

		USceneComponent* SlideStop = AddFallbackPart(TEXT("SlideStop"), Root, FVector(5.0f, -1.45f, -1.9f));
		Shape(SlideStop, Cube, FVector(1.2f, -0.1f, 0.3f), FRotator::ZeroRotator, FVector(2.6f, 0.3f, 0.4f), Dark);

		USceneComponent* MagRelease = AddFallbackPart(TEXT("MagRelease"), Root, FVector(5.4f, -1.5f, -4.9f));
		Shape(MagRelease, Cube, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.8f, 0.3f, 0.8f), Dark);

		// Magazine pivot at the top centre of the mag in the well.
		USceneComponent* Mag = AddFallbackPart(TEXT("Magazine"), Root, FVector(4.2f, 0.f, -1.8f));
		const FVector MagSize = Data->FallbackMagazineSize;
		const FVector MagDown = Data->MagazineOutDirection.GetSafeNormal();
		Shape(Mag, Cube, MagDown * (MagSize.Z * 0.5f), FRotator(-15.f, 0.f, 0.f), MagSize, Steel);
		Shape(Mag, Cube, MagDown * (MagSize.Z + 0.3f), FRotator(-15.f, 0.f, 0.f), FVector(MagSize.X + 0.6f, MagSize.Y + 0.4f, 0.6f), Dark); // base plate
		USceneComponent* Follower = AddFallbackPart(TEXT("MagFollower"), Mag, FVector::ZeroVector);
		Shape(Follower, Cube, MagDown * 0.3f, FRotator(-15.f, 0.f, 0.f), FVector(MagSize.X - 0.4f, MagSize.Y - 0.4f, 0.3f), Dark);

		Socket(TEXT("Muzzle"), Root, FVector(20.8f, 0.f, 0.f));
		Socket(TEXT("MagWell"), Root, FVector(4.2f, 0.f, -1.8f));
		Socket(TEXT("GripHand"), Root, FVector(1.8f, 0.f, -8.0f));
		Socket(TEXT("SupportHand"), Root, FVector(2.5f, 0.f, -8.0f));
	}
	else
	{
		// --- Gray-box AK-style rifle. Bolt face at X = 0, bore at Z = 0. ---
		USceneComponent* Body = AddFallbackPart(NAME_None, Root, FVector::ZeroVector);
		Shape(Body, Cube, FVector(-8.f, 0.f, -1.0f), FRotator::ZeroRotator, FVector(24.f, 3.2f, 6.f), Steel);           // receiver
		Shape(Body, Cyl, FVector(24.35f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), FVector(2.0f, 2.0f, 40.7f), Dark);         // barrel
		Shape(Body, Cube, FVector(16.f, 0.f, -1.5f), FRotator::ZeroRotator, FVector(24.f, 4.2f, 4.f), Wood);             // lower handguard
		Shape(Body, Cube, FVector(16.f, 0.f, 2.2f), FRotator::ZeroRotator, FVector(16.f, 3.4f, 2.2f), Wood);             // upper handguard
		Shape(Body, Cube, FVector(-42.f, 0.f, -6.f), FRotator(8.f, 0.f, 0.f), FVector(44.f, 3.6f, 7.f), Wood);           // stock
		Shape(Body, Cube, FVector(-19.f, 0.f, -8.5f), FRotator(-20.f, 0.f, 0.f), FVector(3.f, 2.8f, 11.f), Wood);        // pistol grip
		Shape(Body, Cube, FVector(-10.f, 0.f, -5.2f), FRotator::ZeroRotator, FVector(6.f, 0.6f, 0.4f), Steel);           // trigger guard
		Shape(Body, Cube, FVector(40.4f, 0.f, 2.4f), FRotator::ZeroRotator, FVector(1.2f, 1.6f, 1.8f), Steel);          // front sight block
		Shape(Body, Cube, FVector(40.4f, 0.f, 3.4f), FRotator::ZeroRotator, FVector(0.3f, 0.2f, 0.6f), Dark);           // front sight post
		Shape(Body, Cube, FVector(2.97f, -0.4f, 2.8f), FRotator::ZeroRotator, FVector(0.6f, 0.5f, 1.8f), Dark);         // rear sight, left
		Shape(Body, Cube, FVector(2.97f, 0.4f, 2.8f), FRotator::ZeroRotator, FVector(0.6f, 0.5f, 1.8f), Dark);          // rear sight, right
		Shape(Body, Cube, FVector(44.7f, 0.f, 0.f), FRotator::ZeroRotator, FVector(2.4f, 2.2f, 2.2f), Steel);           // muzzle brake
		Socket(TEXT("SightRear"), Body, FVector(2.97f, 0.f, 3.7f));
		Socket(TEXT("SightFront"), Body, FVector(40.4f, 0.f, 3.7f));
		Socket(TEXT("EjectionPort"), Body, FVector(-5.8f, 1.7f, -0.2f));

		USceneComponent* Carrier = AddFallbackPart(TEXT("BoltCarrier"), Root, FVector(0.f, 0.f, 1.0f));
		Shape(Carrier, Cube, FVector(-4.f, 1.0f, 0.2f), FRotator::ZeroRotator, FVector(12.f, 1.4f, 1.6f), Steel);
		Shape(Carrier, Cube, FVector(1.2f, 2.6f, -0.5f), FRotator::ZeroRotator, FVector(1.0f, 2.2f, 0.8f), Steel);      // charging handle

		USceneComponent* Selector = AddFallbackPart(TEXT("Selector"), Root, FVector(-17.8f, 1.7f, -0.4f));
		Shape(Selector, Cube, FVector(5.f, 0.15f, 0.f), FRotator::ZeroRotator, FVector(10.f, 0.3f, 1.2f), Dark);

		USceneComponent* Trigger = AddFallbackPart(TEXT("Trigger"), Root, FVector(-11.8f, 0.f, -3.0f));
		Shape(Trigger, Cube, FVector(0.f, 0.f, -0.9f), FRotator::ZeroRotator, FVector(0.5f, 0.5f, 1.8f), Dark);

		// Magazine pivot at the front lug: the mag rocks about it.
		USceneComponent* Mag = AddFallbackPart(TEXT("Magazine"), Root, FVector(-1.2f, 0.f, -2.0f));
		const FVector MagSize = Data->FallbackMagazineSize;
		Shape(Mag, Cube, FVector(-0.78f, 0.f, -11.2f), FRotator(15.f, 0.f, 0.f), MagSize, Steel);
		USceneComponent* Follower = AddFallbackPart(TEXT("MagFollower"), Mag, FVector(-3.3f, 0.f, -1.8f));
		Shape(Follower, Cube, FVector(0.f, 0.f, -0.3f), FRotator::ZeroRotator, FVector(MagSize.X - 1.f, MagSize.Y - 0.4f, 0.3f), Dark);

		Socket(TEXT("Muzzle"), Root, FVector(46.f, 0.f, 0.f));
		Socket(TEXT("MagWell"), Root, FVector(-4.5f, 0.f, -3.8f));
		Socket(TEXT("GripHand"), Root, FVector(-18.8f, 0.f, -8.4f));
		Socket(TEXT("SupportHand"), Root, FVector(18.f, 0.f, -1.2f));
		Socket(TEXT("ChargingHandle"), Carrier, FVector(1.2f, 3.0f, -0.5f));
		Socket(TEXT("Shoulder"), Root, FVector(-64.f, 0.f, -10.f));
	}
}

// =====================================================================
// Inputs
// =====================================================================

void AHoldoverGun::SetTriggerHeld(bool bHeld)
{
	if (bHeld == bTriggerHeld)
	{
		return;
	}
	bTriggerHeld = bHeld;
	if (bHeld)
	{
		TryPullTrigger();
	}
	else
	{
		// Trigger resets as it moves forward.
		bTriggerReset = true;
	}
}

void AHoldoverGun::TryPullTrigger()
{
	if (!bTriggerReset)
	{
		return;
	}
	bTriggerReset = false;

	if (FireMode == EHoldoverFireMode::Safe)
	{
		return; // trigger blocked (or disconnected on a decocker)
	}
	if (!IsInBattery())
	{
		return; // disconnector: nothing happens out of battery
	}

	if (!bHammerCocked)
	{
		if (!Data->bDoubleAction)
		{
			return; // single action with the hammer down: dead trigger
		}
		bHammerCocked = true; // double-action pull cocks it on the way
	}

	// Hammer falls.
	bHammerCocked = false;

	if (Chamber == EHoldoverChamber::Live)
	{
		if (FMath::FRand() < Data->DudChance)
		{
			Chamber = EHoldoverChamber::Dud;
			Malfunction = EHoldoverMalfunction::Dud;
			PlayGunSound(Data->DryFireSound);
			OnDryFire.Broadcast(this);
			OnMalfunction.Broadcast(this);
			return;
		}
		Fire();
	}
	else
	{
		PlayGunSound(Data->DryFireSound);
		OnDryFire.Broadcast(this);
	}
}

void AHoldoverGun::BeginRack()
{
	if (bRackHeld)
	{
		return;
	}
	if (Data->bSafetyBlocksAction && FireMode == EHoldoverFireMode::Safe)
	{
		return; // AK: the selector covers the charging handle slot
	}
	if (Motion == EHoldoverSlideMotion::Cycling)
	{
		return;
	}

	bRackHeld = true;
	bLockRequested = false;
	bSlideLocked = false;
	bExtractedThisStroke = false;
	Motion = EHoldoverSlideMotion::HandPulling;
	PlayGunSound(Data->SlideBackSound);
}

void AHoldoverGun::EndRack()
{
	if (!bRackHeld)
	{
		return;
	}
	bRackHeld = false;
	HandSlideTarget = -1.f;

	if (Motion == EHoldoverSlideMotion::HandHeld)
	{
		ReleaseFromRear();
	}
	else if (Motion == EHoldoverSlideMotion::HandPulling)
	{
		if (bExtractedThisStroke)
		{
			// Pulled past the ejector: counts as a full rack.
			OnReachedRear(false);
			ReleaseFromRear();
		}
		else
		{
			// Short stroke: the slide just goes back where it was, nothing feeds.
			const bool bJammed = (Malfunction == EHoldoverMalfunction::Stovepipe || Malfunction == EHoldoverMalfunction::DoubleFeed);
			bFeeding = false;
			StartReturn(bJammed ? JamFraction : 0.f);
		}
	}
}

void AHoldoverGun::PressSlideStop()
{
	if (!Data->bHasSlideStop)
	{
		return;
	}
	if (Motion == EHoldoverSlideMotion::HandHeld || Motion == EHoldoverSlideMotion::HandPulling)
	{
		bLockRequested = true; // thumb holds the stop up; slide locks when released
		return;
	}
	if (bSlideLocked && Motion == EHoldoverSlideMotion::Idle)
	{
		bSlideLocked = false;
		StartReturnToBattery();
	}
}

void AHoldoverGun::DropMagazine()
{
	if (!bHasMagazine)
	{
		return;
	}
	MagReleaseVisual = 1.f;

	// The loose magazine starts exactly where the one in the gun is.
	const FVector OutDirWorld = GetActorTransform().TransformVectorNoScale(Data->MagazineOutDirection.GetSafeNormal());
	const FVector Inherit = GetOwner() ? GetOwner()->GetVelocity() : FVector::ZeroVector;
	SpawnLooseMagazine(Magazine, GetSocketWorldTransform(Data->MagazineBone), Inherit + OutDirWorld * 150.f);

	bHasMagazine = false;
	bMagSeating = false;
	Magazine = FHoldoverMagazine();
	PlayGunSound(Data->MagOutSound);

	// Stripping the mag frees the round jammed against the case in a double feed.
	if (bJammedRound)
	{
		bJammedRound = false;
		EjectPiece(true, 0.1f);
	}

	OnMagazineOut.Broadcast(this);
}

void AHoldoverGun::SpawnLooseMagazine(const FHoldoverMagazine& Mag, const FTransform& TopWorld, const FVector& Velocity)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHoldoverLoosePiece* Piece = World->SpawnActor<AHoldoverLoosePiece>(AHoldoverLoosePiece::StaticClass(), TopWorld, Params);
	if (!Piece)
	{
		return;
	}
	Piece->Setup(EHoldoverPieceKind::Magazine, Data->MagazineMesh, Data->FallbackMagazineSize, 0.25f);
	Piece->Magazine = Mag;
	Piece->GunId = Data->GunId;

	// The mesh origin is the top of the mag along its axis. The gray-box cube is centred,
	// so shift it down half its length along the mag's out direction.
	const FVector OutDirWorld = GetActorTransform().TransformVectorNoScale(Data->MagazineOutDirection.GetSafeNormal());
	FQuat Rot = TopWorld.GetRotation();
	FVector Start = TopWorld.GetLocation();
	if (!Data->MagazineMesh)
	{
		Rot = GetActorQuat() * FQuat::FindBetweenNormals(FVector(0.f, 0.f, -1.f), Data->MagazineOutDirection.GetSafeNormal());
		Start += OutDirWorld * (Data->FallbackMagazineSize.Z * 0.5f);
	}
	Piece->Launch(FTransform(Rot, Start), Velocity, FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 0.f));
}

bool AHoldoverGun::RemoveMagazineToHand(FHoldoverMagazine& OutMag)
{
	if (!bHasMagazine)
	{
		return false;
	}
	MagReleaseVisual = 1.f;
	OutMag = Magazine;
	bHasMagazine = false;
	bMagSeating = false;
	Magazine = FHoldoverMagazine();
	PlayGunSound(Data->MagOutSound);
	if (bJammedRound)
	{
		bJammedRound = false;
		EjectPiece(true, 0.1f);
	}
	OnMagazineOut.Broadcast(this);
	return true;
}

bool AHoldoverGun::InsertMagazine(const FHoldoverMagazine& Mag)
{
	if (bHasMagazine)
	{
		return false;
	}
	bHasMagazine = true;
	Magazine = Mag;
	Magazine.Capacity = FMath::Max(Magazine.Capacity, 1);
	bMagSeating = true;
	MagSeatTimer = 0.f;
	return true;
}

void AHoldoverGun::CycleSelector()
{
	if (!Data->bHasSafety && !Data->bHasFullAuto)
	{
		return;
	}

	// Physical order of the lever positions.
	TArray<EHoldoverFireMode> Order;
	if (Data->bHasSafety)
	{
		Order.Add(EHoldoverFireMode::Safe);
	}
	if (Data->bHasFullAuto)
	{
		Order.Add(EHoldoverFireMode::Auto);
	}
	Order.Add(EHoldoverFireMode::Semi);

	const int32 Index = Order.IndexOfByKey(FireMode);
	FireMode = Order[(Index + 1) % Order.Num()];

	if (FireMode == EHoldoverFireMode::Safe && Data->bSafetyDecocks && bHammerCocked)
	{
		bHammerCocked = false; // decocker drops the hammer safely
	}

	PlayGunSound(Data->SafetySound);
	OnSelectorChanged.Broadcast(this);
}

void AHoldoverGun::ResetState(const FHoldoverMagazine* Mag)
{
	Chamber = EHoldoverChamber::Empty;
	bHasMagazine = false;
	Magazine = FHoldoverMagazine();
	if (Mag)
	{
		bHasMagazine = true;
		Magazine = *Mag;
	}
	bMagSeating = false;
	bMagInHand = false;
	HandMagRounds = 0;
	SlidePos = 0.f;
	Motion = EHoldoverSlideMotion::Idle;
	bSlideLocked = false;
	bLockRequested = false;
	bExtractedThisStroke = false;
	bFeeding = false;
	bExtractionFailed = false;
	bJammedRound = false;
	HandSlideTarget = -1.f;
	bHammerCocked = false;
	bTriggerHeld = false;
	bTriggerReset = true;
	bRackHeld = false;
	Malfunction = EHoldoverMalfunction::None;
	FireMode = (Data && Data->bHasSafety && Data->bSafetyBlocksAction) ? EHoldoverFireMode::Safe : EHoldoverFireMode::Semi;
}

// =====================================================================
// Firing
// =====================================================================

void AHoldoverGun::Fire()
{
	Chamber = EHoldoverChamber::Spent;
	LastShotTime = GetWorld()->GetTimeSeconds();

	LaunchBullet();
	PlayShotSounds();
	if (MuzzleFX)
	{
		// Socket bones may be turned any way in the model; the bore is always the gun's own X axis.
		const FTransform MuzzleTM(GetActorQuat(), GetSocketWorldTransform(Data->MuzzleSocket).GetLocation());
		MuzzleFX->Fire(Data, MuzzleTM, GetSocketWorldTransform(Data->EjectionPortSocket).GetLocation());
	}

	// Recoil drives the slide or bolt back.
	Motion = EHoldoverSlideMotion::Cycling;
	bExtractedThisStroke = false;
	bExtractionFailed = false;

	OnFired.Broadcast(this);
}

void AHoldoverGun::LaunchBullet()
{
	UWorld* World = GetWorld();
	UHoldoverBallisticsSubsystem* Ballistics = World ? World->GetSubsystem<UHoldoverBallisticsSubsystem>() : nullptr;
	if (!Ballistics)
	{
		return;
	}

	const FTransform GunTM = GetActorTransform();
	const FVector Muzzle = GetSocketWorldTransform(Data->MuzzleSocket).GetLocation();
	const FVector Fwd = GunTM.GetUnitAxis(EAxis::X);
	const FVector Up = GunTM.GetUnitAxis(EAxis::Z);

	// Sights are regulated: the bore points slightly up so the bullet crosses the sight line at the zero distance.
	FVector RearLocal, FrontLocal;
	GetSightsLocal(RearLocal, FrontLocal);
	const FVector MuzzleLocal = GetSocketLocal(Data->MuzzleSocket);
	const float SightHeight = FMath::Max(0.f, 0.5f * (RearLocal.Z + FrontLocal.Z) - MuzzleLocal.Z);
	const float ZeroAngle = UHoldoverBallisticsSubsystem::ComputeZeroAngle(Data->Bullet, SightHeight, World->GetGravityZ());
	FVector Dir = (Fwd * FMath::Cos(ZeroAngle) + Up * FMath::Sin(ZeroAngle)).GetSafeNormal();

	// Mechanical dispersion: MOA is the full cone diameter, 1 MOA = 1/60 degree.
	const float HalfAngleRad = FMath::DegreesToRadians(Data->Bullet.AccuracyMOA / 60.f) * 0.5f;
	if (HalfAngleRad > 0.f)
	{
		Dir = FMath::VRandCone(Dir, HalfAngleRad);
	}

	const FVector Inherit = GetOwner() ? GetOwner()->GetVelocity() : FVector::ZeroVector;
	TArray<AActor*> Ignore;
	Ignore.Add(this);
	if (GetOwner())
	{
		Ignore.Add(GetOwner());
	}
	Ballistics->FireBullet(Data->Bullet, Muzzle, Dir * Data->Bullet.MuzzleVelocity * 100.f + Inherit, GetOwner(), Ignore);
}

// =====================================================================
// Slide / bolt cycle
// =====================================================================

bool AHoldoverGun::ShouldLockOpen() const
{
	if (!Data->bHasSlideStop)
	{
		return false;
	}
	if (bLockRequested)
	{
		return true;
	}
	return Data->bLastRoundHoldOpen && bHasMagazine && !bMagSeating && Magazine.Rounds <= 0;
}

void AHoldoverGun::UpdateSlide(float Dt)
{
	switch (Motion)
	{
	case EHoldoverSlideMotion::Cycling:
	{
		SlidePos += Dt / FMath::Max(Data->CycleTime * 0.5f, 0.005f);
		if (!bExtractedThisStroke && SlidePos >= Data->EjectFraction)
		{
			OnExtractionPoint(true);
		}
		if (SlidePos >= 1.f)
		{
			SlidePos = 1.f;
			OnReachedRear(true);
		}
		break;
	}
	case EHoldoverSlideMotion::HandPulling:
	{
		if (HandSlideTarget >= 0.f)
		{
			SlidePos = FMath::FInterpConstantTo(SlidePos, HandSlideTarget, Dt, 1.f / FMath::Max(Data->HandPullTime * 0.5f, 0.01f));
		}
		else
		{
			SlidePos += Dt / FMath::Max(Data->HandPullTime, 0.01f);
		}
		if (!bExtractedThisStroke && SlidePos >= Data->EjectFraction)
		{
			OnExtractionPoint(false);
		}
		if (SlidePos >= 1.f)
		{
			SlidePos = 1.f;
			OnReachedRear(false);
			Motion = EHoldoverSlideMotion::HandHeld;
		}
		break;
	}
	case EHoldoverSlideMotion::HandHeld:
	{
		if (HandSlideTarget >= 0.f && HandSlideTarget < 1.f)
		{
			// The animation is easing the slide forward under hand control (a "ride").
			SlidePos = FMath::FInterpConstantTo(SlidePos, HandSlideTarget, Dt, 1.f / FMath::Max(Data->HandPullTime * 0.5f, 0.01f));
		}
		break;
	}
	case EHoldoverSlideMotion::Returning:
	{
		SlidePos -= Dt / FMath::Max(Data->SpringReturnTime, 0.005f);
		if (SlidePos <= ReturnTarget)
		{
			SlidePos = ReturnTarget;
			OnSlideArrived();
		}
		break;
	}
	default:
		break;
	}
}

void AHoldoverGun::OnExtractionPoint(bool bByRecoil)
{
	bExtractedThisStroke = true;

	switch (Malfunction)
	{
	case EHoldoverMalfunction::Stovepipe:
		// The case caught in the port falls free when the slide is pulled back.
		EjectPiece(false, 0.25f);
		Malfunction = EHoldoverMalfunction::None;
		return;
	case EHoldoverMalfunction::DoubleFeed:
		if (bHasMagazine)
		{
			return; // still jammed: the mag keeps pushing a round against the case
		}
		EjectPiece(false, 0.3f);
		Chamber = EHoldoverChamber::Empty;
		Malfunction = EHoldoverMalfunction::None;
		return;
	case EHoldoverMalfunction::Dud:
		Malfunction = EHoldoverMalfunction::None;
		break;
	default:
		break;
	}

	switch (Chamber)
	{
	case EHoldoverChamber::Spent:
		if (bByRecoil && FMath::FRand() < Data->DoubleFeedChance)
		{
			bExtractionFailed = true; // extractor slipped: case stays in the chamber
			return;
		}
		if (bByRecoil && FMath::FRand() < Data->StovepipeChance)
		{
			Chamber = EHoldoverChamber::Empty;
			Malfunction = EHoldoverMalfunction::Stovepipe; // case caught upright in the port
			OnMalfunction.Broadcast(this);
			return;
		}
		EjectPiece(false, 1.f);
		Chamber = EHoldoverChamber::Empty;
		break;
	case EHoldoverChamber::Live:
	case EHoldoverChamber::Dud:
		// A round pulled out by hand just drops out of the port.
		EjectPiece(true, bByRecoil ? 1.f : Data->LiveRoundEjectScale);
		Chamber = EHoldoverChamber::Empty;
		break;
	default:
		break;
	}
}

void AHoldoverGun::OnReachedRear(bool bByRecoil)
{
	// The slide (or bolt carrier) rides over the hammer and cocks it.
	bHammerCocked = true;

	if (bByRecoil)
	{
		ReleaseFromRear();
	}
}

void AHoldoverGun::ReleaseFromRear()
{
	if (Malfunction == EHoldoverMalfunction::Stovepipe)
	{
		bFeeding = false;
		StartReturn(JamFraction);
		return;
	}
	if (ShouldLockOpen())
	{
		bFeeding = false;
		StartReturn(Data->LockedBackFraction);
		return;
	}
	StartReturnToBattery();
}

void AHoldoverGun::StartReturn(float Target)
{
	ReturnTarget = FMath::Clamp(Target, 0.f, 1.f);
	if (SlidePos <= ReturnTarget)
	{
		SlidePos = ReturnTarget;
		OnSlideArrived();
		return;
	}
	Motion = EHoldoverSlideMotion::Returning;
	PlayGunSound(Data->SlideForwardSound);
}

void AHoldoverGun::StartReturnToBattery()
{
	bFeeding = false;

	if (Malfunction == EHoldoverMalfunction::DoubleFeed && bHasMagazine)
	{
		StartReturn(JamFraction);
		return;
	}

	const bool bCanFeed = bHasMagazine && !bMagSeating && Magazine.Rounds > 0;
	if (bCanFeed)
	{
		if (Chamber == EHoldoverChamber::Empty)
		{
			--Magazine.Rounds;
			bFeeding = true;
		}
		else if (Chamber == EHoldoverChamber::Spent && bExtractionFailed)
		{
			// The next round slams into the back of the stuck case.
			--Magazine.Rounds;
			bJammedRound = true;
			bExtractionFailed = false;
			Malfunction = EHoldoverMalfunction::DoubleFeed;
			OnMalfunction.Broadcast(this);
			StartReturn(JamFraction);
			return;
		}
	}
	bExtractionFailed = false;
	StartReturn(0.f);
}

void AHoldoverGun::OnSlideArrived()
{
	Motion = EHoldoverSlideMotion::Idle;

	if (ReturnTarget <= KINDA_SMALL_NUMBER)
	{
		if (bFeeding)
		{
			Chamber = EHoldoverChamber::Live;
			bFeeding = false;
		}
		return;
	}

	if (Malfunction == EHoldoverMalfunction::None && ShouldLockOpen())
	{
		bSlideLocked = true;
		bLockRequested = false;
		PlayGunSound(Data->SlideLockSound);
		OnSlideLocked.Broadcast(this);
	}
}

void AHoldoverGun::EjectPiece(bool bLiveRound, float SpeedScale)
{
	UWorld* World = GetWorld();
	UHoldoverDebrisSubsystem* Debris = World ? World->GetSubsystem<UHoldoverDebrisSubsystem>() : nullptr;
	if (!Debris)
	{
		return;
	}

	const FTransform GunTM = GetActorTransform();
	const FVector Port = GetSocketWorldTransform(Data->EjectionPortSocket).GetLocation();

	const float R = Data->CaseEjectRandom;
	const FVector LocalVel = Data->CaseEjectVelocity * SpeedScale
		+ FVector(FMath::FRandRange(-R, R), FMath::FRandRange(-R, R), FMath::FRandRange(-R, R)) * SpeedScale;
	const FVector Inherit = GetOwner() ? GetOwner()->GetVelocity() : FVector::ZeroVector;
	const FVector Vel = GunTM.TransformVectorNoScale(LocalVel) + Inherit;

	// The ejector kicks the case head out sideways while the extractor still holds the rim, so the case cartwheels
	// mostly about the gun's up axis (head swinging out first), with a random wobble that makes it tumble.
	const float Spin = Data->CaseSpinDegPerSec * SpeedScale * FMath::FRandRange(0.75f, 1.25f);
	const FVector AngVel = GunTM.TransformVectorNoScale(
		FVector(FMath::FRandRange(-0.35f, 0.35f), FMath::FRandRange(-0.4f, 0.4f), -1.f) * Spin);

	// Cases leave lying along the bore; the gray-box cylinder's axis is Z, so turn it to lie along X.
	const bool bHasMesh = bLiveRound ? (Data->RoundMesh != nullptr) : (Data->CasingMesh != nullptr);
	const FQuat Lie = bHasMesh ? FQuat::Identity : FRotator(-90.f, 0.f, 0.f).Quaternion();
	const FTransform Start(GunTM.GetRotation() * Lie, Port);

	FVector Size = Data->FallbackCasingSize;
	if (bLiveRound)
	{
		Size.Z = Data->FallbackRoundLength;
	}

	AHoldoverLoosePiece* Piece = Debris->Eject(bLiveRound ? EHoldoverPieceKind::LiveRound : EHoldoverPieceKind::Casing,
		bLiveRound ? Data->RoundMesh : Data->CasingMesh,
		Size,
		(bLiveRound ? Data->RoundMassGrams : Data->CaseMassGrams) * 0.001f,
		Start, Vel, AngVel);
	if (Piece)
	{
		Piece->SoundSet = Data->BrassSoundSet;
		Piece->GunId = Data->GunId;
		// Drawn with the first-person gun until it is clear of it, so it comes out of the port you see.
		Piece->DrawWithGunFor(HoldoverDebug::CVarCaseWithGun.GetValueOnGameThread());
	}
	UE_LOG(LogHoldover, Verbose, TEXT("Eject %s at %s vel %s piece %s mesh %s radius %.2f visible %d hidden %d"), bLiveRound ? TEXT("round") : TEXT("case"), *Port.ToCompactString(), *Vel.ToCompactString(),
		Piece ? *Piece->GetName() : TEXT("none"), (Piece && Piece->GetMesh()->GetStaticMesh()) ? *Piece->GetMesh()->GetStaticMesh()->GetName() : TEXT("-"),
		Piece ? Piece->GetMesh()->Bounds.SphereRadius : -1.f, Piece ? (int32)Piece->GetMesh()->IsVisible() : -1, Piece ? (int32)Piece->IsHidden() : -1);

}

void AHoldoverGun::PlayShotSounds()
{
	// Three layers: the blast as the shooter hears it, the action cycling, and the echo rolling back off the
	// trees and the berm. Each is one of several recordings, with a little pitch and volume change every shot.
	if (Data->ShotSounds.Num() == 0 && Data->FireSound)
	{
		PlayGunSound(Data->FireSound);
	}
	HoldoverAudio::PlayAtEars(this, HoldoverAudio::PickVariant(Data->ShotSounds, LastShotVariant), Data->ShotVolume * FMath::FRandRange(0.92f, 1.f), 0.03f);
	HoldoverAudio::PlayAtEars(this, HoldoverAudio::PickVariant(Data->MechSounds, LastMechVariant), Data->MechVolume * FMath::FRandRange(0.85f, 1.f), 0.04f);
	HoldoverAudio::PlayAtEars(this, HoldoverAudio::PickVariant(Data->TailSounds, LastTailVariant), Data->TailVolume * FMath::FRandRange(0.85f, 1.f), 0.02f, HoldoverAudio::TailConcurrency());
}

void AHoldoverGun::PlayGunSound(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

// =====================================================================
// Tick and visuals
// =====================================================================

void AHoldoverGun::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Data)
	{
		return;
	}

	if (bMagSeating)
	{
		MagSeatTimer += DeltaSeconds;
		if (MagSeatTimer >= Data->MagazineSeatTime)
		{
			bMagSeating = false;
			PlayGunSound(Data->MagInSound);
			OnMagazineSeated.Broadcast(this);
		}
	}

	UpdateSlide(DeltaSeconds);

	// Full auto: the auto sear trips the hammer as soon as the bolt is home.
	if (FireMode == EHoldoverFireMode::Auto && bTriggerHeld && IsInBattery() && Chamber == EHoldoverChamber::Live && bHammerCocked)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastShotTime >= Data->GetAutoInterval())
		{
			bHammerCocked = false;
			if (FMath::FRand() < Data->DudChance)
			{
				Chamber = EHoldoverChamber::Dud;
				Malfunction = EHoldoverMalfunction::Dud;
				OnMalfunction.Broadcast(this);
			}
			else
			{
				Fire();
			}
		}
	}

	UpdateVisuals(DeltaSeconds);

	if (HoldoverDebug::Gun() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::White, DescribeState());
	}
}

void AHoldoverGun::AddBoneOffset(FName Bone, const FVector& Translation, const FQuat& Rotation, bool bReplace)
{
	if (Bone.IsNone())
	{
		return;
	}
	FBoneDelta& D = ExternalDeltas.FindOrAdd(Bone);
	D.T += Translation;
	D.R = Rotation * D.R;
	if (bReplace)
	{
		ExternalReplace.Add(Bone);
	}
}

bool AHoldoverGun::IsMechanicalBone(FName Bone) const
{
	if (!Data || Bone.IsNone())
	{
		return false;
	}
	return Bone == Data->SlideMotion.Bone
		|| Bone == Data->LockingMotion.Bone
		|| Bone == Data->HammerCockMotion.Bone
		|| Bone == Data->SlideStopUpMotion.Bone;
}

bool AHoldoverGun::GetBoneRest(FName Bone, FTransform& OutGunSpace) const
{
	if (bUseSkeletal && Data && Data->GunMesh)
	{
		const int32 Index = Data->GunMesh->GetRefSkeleton().FindBoneIndex(Bone);
		if (Index != INDEX_NONE && RefComponent.IsValidIndex(Index))
		{
			OutGunSpace = RefComponent[Index] * Mesh->GetRelativeTransform();
			return true;
		}
		return false;
	}
	const TObjectPtr<USceneComponent>* Part = FallbackBones.Find(Bone);
	const FTransform* Rest = FallbackRest.Find(Bone);
	if (Part && *Part && Rest)
	{
		// Rest is relative to the part's parent; parents of moving parts are the root or the slide.
		USceneComponent* Parent = (*Part)->GetAttachParent();
		FTransform ParentGun = FTransform::Identity;
		if (Parent && Parent != Root)
		{
			ParentGun = Parent->GetComponentTransform().GetRelativeTransform(GetActorTransform());
		}
		OutGunSpace = (*Rest) * ParentGun;
		return true;
	}
	return false;
}

void AHoldoverGun::AddDelta(FName Bone, const FVector& T, const FQuat& R, float Scale)
{
	if (Bone.IsNone())
	{
		return;
	}
	FBoneDelta& D = FrameDeltas.FindOrAdd(Bone);
	D.T += T;
	D.R = R * D.R;
	D.Scale *= Scale;
}

void AHoldoverGun::AddMotion(const FHoldoverBoneMotion& M, float Alpha)
{
	if (M.Bone.IsNone() || FMath::IsNearlyZero(Alpha))
	{
		return;
	}
	const FQuat R = FMath::IsNearlyZero(M.RotationDegrees)
		? FQuat::Identity
		: FQuat(M.RotationAxis.GetSafeNormal(), FMath::DegreesToRadians(M.RotationDegrees * Alpha));
	AddDelta(M.Bone, M.Translation * Alpha, R);
}

void AHoldoverGun::UpdateVisuals(float Dt)
{
	FrameDeltas.Reset();

	// Slide or bolt carrier.
	AddMotion(Data->SlideMotion, SlidePos);

	// Barrel unlock (pistol) or bolt rotation (rifle) during the first part of the travel.
	const float LockAlpha = FMath::Clamp(SlidePos / FMath::Max(Data->LockingTravelFraction, 0.01f), 0.f, 1.f);
	AddMotion(Data->LockingMotion, LockAlpha);

	// Hammer: cocked, or pushed back by the passing slide.
	const float HammerTarget = FMath::Max(bHammerCocked ? 1.f : 0.f, FMath::Clamp(SlidePos * 1.6f, 0.f, 1.f));
	HammerVisual = (HammerTarget > HammerVisual) ? HammerTarget : FMath::FInterpTo(HammerVisual, HammerTarget, Dt, 60.f);
	AddMotion(Data->HammerCockMotion, HammerVisual);

	TriggerVisual = FMath::FInterpTo(TriggerVisual, bTriggerHeld ? 1.f : 0.f, Dt, 30.f);
	AddMotion(Data->TriggerPullMotion, TriggerVisual);

	const bool bStopUp = bSlideLocked || (bLockRequested && bRackHeld);
	AddMotion(Data->SlideStopUpMotion, bStopUp ? 1.f : 0.f);

	MagReleaseVisual = FMath::FInterpTo(MagReleaseVisual, 0.f, Dt, 8.f);
	AddMotion(Data->MagReleaseMotion, MagReleaseVisual);

	SelectorVisual = FMath::FInterpTo(SelectorVisual, Data->GetSelectorFraction(FireMode), Dt, 25.f);
	AddMotion(Data->SafetyMotion, SelectorVisual);

	// Magazine: hidden when out (unless a hand holds it), sliding home while seating.
	if (bMagInHand)
	{
		AddDelta(Data->MagFollowerBone, Data->FollowerOffsetPerRound * static_cast<float>(HandMagRounds), FQuat::Identity);
	}
	else if (!bHasMagazine)
	{
		AddDelta(Data->MagazineBone, FVector::ZeroVector, FQuat::Identity, 0.f);
	}
	else
	{
		const float Out = bMagSeating ? 1.f - FMath::Clamp(MagSeatTimer / FMath::Max(Data->MagazineSeatTime, 0.01f), 0.f, 1.f) : 0.f;
		if (Out > 0.f)
		{
			AddMotion(Data->MagazineRockMotion, Out);
			AddDelta(Data->MagazineBone, Data->MagazineOutDirection.GetSafeNormal() * (Data->MagazineOutTravel * 0.3f * Out), FQuat::Identity);
		}
		AddDelta(Data->MagFollowerBone, Data->FollowerOffsetPerRound * static_cast<float>(Magazine.Rounds), FQuat::Identity);
	}

	for (const auto& Pair : ExternalDeltas)
	{
		if (ExternalReplace.Contains(Pair.Key))
		{
			// Keep a hidden scale (magazine out of the gun) but replace the motion.
			FBoneDelta Replaced = Pair.Value;
			if (const FBoneDelta* Own = FrameDeltas.Find(Pair.Key))
			{
				Replaced.Scale = Own->Scale;
			}
			FrameDeltas.Add(Pair.Key, Replaced);
		}
		else
		{
			AddDelta(Pair.Key, Pair.Value.T, Pair.Value.R, Pair.Value.Scale);
		}
	}
	ExternalDeltas.Reset();
	ExternalReplace.Reset();

	ApplyBoneDeltas();
}

void AHoldoverGun::ApplyBoneDeltas()
{
	if (bUseSkeletal)
	{
		// Write local (parent-relative) transforms directly: rest pose plus the gun-space delta,
		// converted through the parent's rest orientation. Children follow their parents.
		const FReferenceSkeleton& RefSkel = Data->GunMesh->GetRefSkeleton();
		for (UPoseableMeshComponent* Comp : { Mesh.Get(), ShadowMesh.Get() })
		{
			if (Comp->BoneSpaceTransforms.Num() != RefLocal.Num())
			{
				continue;
			}
			for (int32 i = 0; i < RefLocal.Num(); ++i)
			{
				Comp->BoneSpaceTransforms[i] = RefLocal[i];
			}
			for (const auto& Pair : FrameDeltas)
			{
				const int32 Index = RefSkel.FindBoneIndex(Pair.Key);
				if (Index == INDEX_NONE)
				{
					continue;
				}
				const int32 Parent = RefSkel.GetParentIndex(Index);
				const FQuat ParentRot = (Parent == INDEX_NONE) ? FQuat::Identity : RefComponent[Parent].GetRotation();
				const FQuat InvParent = ParentRot.Inverse();

				FTransform Local = RefLocal[Index];
				// Into the parent's space with its scale too: the imported skeletons carry a scale on the root,
				// and without dividing it out a 6 cm slide stroke sent the slide metres away (it vanished mid-cycle).
				const FVector LocalT = (Parent == INDEX_NONE) ? Pair.Value.T : RefComponent[Parent].InverseTransformVector(Pair.Value.T);
				Local.SetTranslation(Local.GetTranslation() + LocalT);
				Local.SetRotation((InvParent * Pair.Value.R * ParentRot) * Local.GetRotation());
				Local.SetScale3D(Local.GetScale3D() * FMath::Max(Pair.Value.Scale, 0.001f));
				Comp->BoneSpaceTransforms[Index] = Local;
			}
			// Apply now: the mesh's own tick already ran this frame, so a dirty mark would show the moving parts
			// (slide, carrier, charging handle) a frame behind the hand working them.
			Comp->RefreshBoneTransforms();
		}
	}
	else
	{
		for (const auto& Pair : FallbackBones)
		{
			USceneComponent* Part = Pair.Value;
			const FTransform* Rest = FallbackRest.Find(Pair.Key);
			if (!Part || !Rest)
			{
				continue;
			}
			const FBoneDelta* D = FrameDeltas.Find(Pair.Key);
			if (!D)
			{
				Part->SetRelativeTransform(*Rest);
				continue;
			}
			// Gray-box parts hang directly off the gun root (or the slide, which only translates),
			// so gun-space deltas apply directly.
			FTransform T = *Rest;
			T.SetTranslation(Rest->GetTranslation() + D->T);
			T.SetRotation(D->R * Rest->GetRotation());
			T.SetScale3D(FVector(FMath::Max(D->Scale, 0.001f)));
			Part->SetRelativeTransform(T);
		}
	}
}

// =====================================================================
// Geometry
// =====================================================================

FTransform AHoldoverGun::GetSocketWorldTransform(FName Socket) const
{
	if (bUseSkeletal && Mesh && Mesh->DoesSocketExist(Socket))
	{
		return Mesh->GetSocketTransform(Socket, RTS_World);
	}
	if (const TObjectPtr<USceneComponent>* Marker = FallbackSockets.Find(Socket))
	{
		if (*Marker)
		{
			return (*Marker)->GetComponentTransform();
		}
	}
	if (const TObjectPtr<USceneComponent>* Part = FallbackBones.Find(Socket))
	{
		if (*Part)
		{
			return (*Part)->GetComponentTransform();
		}
	}
	return GetActorTransform();
}

bool AHoldoverGun::HasSocket(FName Socket) const
{
	if (bUseSkeletal && Mesh && Mesh->DoesSocketExist(Socket))
	{
		return true;
	}
	return FallbackSockets.Contains(Socket) || FallbackBones.Contains(Socket);
}

FTransform AHoldoverGun::GetMeshWorldTransform() const
{
	return (bUseSkeletal && Mesh) ? Mesh->GetComponentTransform() : GetActorTransform();
}

FVector AHoldoverGun::GetSocketLocal(FName Socket) const
{
	return GetActorTransform().InverseTransformPosition(GetSocketWorldTransform(Socket).GetLocation());
}

void AHoldoverGun::GetSightsLocal(FVector& OutRear, FVector& OutFront) const
{
	if (bUseSkeletal && Data && Data->GunMesh)
	{
		// Use the rest pose so a locked-back slide does not shift the aim point.
		const FReferenceSkeleton& RefSkel = Data->GunMesh->GetRefSkeleton();
		const int32 Rear = RefSkel.FindBoneIndex(Data->SightRearSocket);
		const int32 Front = RefSkel.FindBoneIndex(Data->SightFrontSocket);
		if (Rear != INDEX_NONE && Front != INDEX_NONE && RefComponent.IsValidIndex(Rear) && RefComponent.IsValidIndex(Front))
		{
			const FTransform MeshRel = Mesh->GetRelativeTransform();
			OutRear = MeshRel.TransformPosition(RefComponent[Rear].GetLocation());
			OutFront = MeshRel.TransformPosition(RefComponent[Front].GetLocation());
			return;
		}
	}
	OutRear = GetSocketLocal(Data ? Data->SightRearSocket : FName(TEXT("SightRear")));
	OutFront = GetSocketLocal(Data ? Data->SightFrontSocket : FName(TEXT("SightFront")));
	if (bSlideLocked || SlidePos > 0.f)
	{
		// Gray-box sights ride on the slide; undo the slide offset for a steady aim point.
		if (Data && Data->bIsPistol)
		{
			const FVector Back = Data->SlideMotion.Translation * SlidePos;
			OutRear -= Back;
			OutFront -= Back;
		}
	}
}

FString AHoldoverGun::DescribeState() const
{
	auto ChamberText = [this]()
	{
		switch (Chamber)
		{
		case EHoldoverChamber::Live: return TEXT("round");
		case EHoldoverChamber::Spent: return TEXT("fired case");
		case EHoldoverChamber::Dud: return TEXT("dud round");
		default: return TEXT("empty");
		}
	};
	auto MotionText = [this]()
	{
		if (bSlideLocked) return TEXT("locked open");
		switch (Motion)
		{
		case EHoldoverSlideMotion::Cycling: return TEXT("cycling");
		case EHoldoverSlideMotion::HandPulling: return TEXT("being pulled");
		case EHoldoverSlideMotion::HandHeld: return TEXT("held back");
		case EHoldoverSlideMotion::Returning: return TEXT("going forward");
		default: return SlidePos > 0.01f ? TEXT("stuck open") : TEXT("closed");
		}
	};
	auto ModeText = [this]()
	{
		switch (FireMode)
		{
		case EHoldoverFireMode::Safe: return TEXT("SAFE");
		case EHoldoverFireMode::Auto: return TEXT("AUTO");
		default: return TEXT("SEMI");
		}
	};
	auto MalfText = [this]()
	{
		switch (Malfunction)
		{
		case EHoldoverMalfunction::Stovepipe: return TEXT("  STOVEPIPE");
		case EHoldoverMalfunction::DoubleFeed: return TEXT("  DOUBLE FEED");
		case EHoldoverMalfunction::Dud: return TEXT("  DUD");
		default: return TEXT("");
		}
	};

	const FString MagText = bHasMagazine
		? FString::Printf(TEXT("%d/%d%s"), Magazine.Rounds, Magazine.Capacity, bMagSeating ? TEXT(" (seating)") : TEXT(""))
		: FString(TEXT("none"));

	return FString::Printf(TEXT("%s | chamber: %s | mag: %s | slide: %s | hammer: %s | %s%s"),
		Data ? *Data->GunId.ToString() : TEXT("?"),
		ChamberText(), *MagText, MotionText(),
		bHammerCocked ? TEXT("cocked") : TEXT("down"),
		ModeText(), MalfText());
}
