#include "Player/HoldoverCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/HoldoverGunAnimator.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/HoldoverGameMode.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Holdover.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/HoldoverBodyRig.h"
#include "Player/HoldoverInputSet.h"
#include "Weapons/HoldoverGun.h"
#include "Weapons/HoldoverGunData.h"
#include "Weapons/HoldoverLoosePiece.h"

namespace
{
	const FName KeyRack(TEXT("Rack"));
	const FName KeyMagCheck(TEXT("MagCheck"));
	const FName KeyPressCheck(TEXT("PressCheck"));
	constexpr float CapsuleHalf = 92.f;
	constexpr int32 CheckMessageKey = 0x484F4C44; // "HOLD"
}

AHoldoverCharacter::AHoldoverCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	GetCapsuleComponent()->InitCapsuleSize(34.f, CapsuleHalf);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->MaxWalkSpeed = WalkSpeed;
	Move->BrakingDecelerationWalking = 1200.f;
	Move->GroundFriction = 8.f;
	Move->MaxAcceleration = 900.f;
	Move->NavAgentProps.bCanCrouch = true;

	// Eyes.
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Eyes"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(EyeForward, 0.f, EyeHeight - CapsuleHalf));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(FieldOfView);
	Camera->bEnableFirstPersonFieldOfView = true;
	Camera->FirstPersonFieldOfView = FieldOfView; // same as the eyes: no distortion, only depth is compressed
	Camera->bEnableFirstPersonScale = true;
	Camera->FirstPersonScale = FirstPersonScale;

	GunHold = CreateDefaultSubobject<USceneComponent>(TEXT("GunHold"));
	GunHold->SetupAttachment(Camera);

	// The character's own skeletal mesh runs the locomotion animation but is never drawn;
	// the two poseable copies below are what you see and what casts the shadow.
	USkeletalMeshComponent* Driver = GetMesh();
	Driver->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalf));
	Driver->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Driver->SetHiddenInGame(true);
	Driver->SetCastShadow(false);
	Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Driver->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BodyVisible = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("BodyVisible"));
	BodyVisible->SetupAttachment(Driver);
	BodyVisible->SetCastShadow(false);
	BodyVisible->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyVisible->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;

	BodyShadow = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("BodyShadow"));
	BodyShadow->SetupAttachment(Driver);
	BodyShadow->SetCastShadow(true);
	BodyShadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyShadow->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	Animator = CreateDefaultSubobject<UHoldoverGunAnimator>(TEXT("GunAnimator"));
	BodyRig = CreateDefaultSubobject<UHoldoverBodyRig>(TEXT("BodyRig"));
}

void AHoldoverCharacter::BeginPlay()
{
	Super::BeginPlay();

	Camera->SetFieldOfView(FieldOfView);
	Camera->FirstPersonFieldOfView = FieldOfView;
	Camera->FirstPersonScale = FirstPersonScale;
	Camera->SetRelativeLocation(FVector(EyeForward, 0.f, EyeHeight - CapsuleHalf));
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	SetupBody();
	BodyRig->Setup(GetMesh(), BodyVisible, BodyShadow, Animator, Camera);
	BodyRig->AddTickPrerequisiteActor(this);

	Animator->OnEvent.AddUObject(this, &AHoldoverCharacter::OnAnimEvent);

	SpawnGun(0);
}

void AHoldoverCharacter::SetupBody()
{
	USkeletalMesh* Mesh = BodyMesh.IsNull() ? nullptr : BodyMesh.LoadSynchronous();
	if (!Mesh)
	{
		UE_LOG(LogHoldover, Warning, TEXT("No body mesh (%s). Add the Third Person content pack to see your body; the gun works without it."),
			*BodyMesh.ToString());
		BodyRig->SetComponentTickEnabled(false);
		return;
	}

	USkeletalMeshComponent* Driver = GetMesh();
	Driver->SetSkeletalMeshAsset(Mesh);
	if (UClass* AnimClass = BodyAnimClass.IsNull() ? nullptr : BodyAnimClass.LoadSynchronous())
	{
		Driver->SetAnimInstanceClass(AnimClass);
	}
	else
	{
		UE_LOG(LogHoldover, Warning, TEXT("No body animation (%s): the body stands still."), *BodyAnimClass.ToString());
	}

	BodyVisible->SetSkinnedAssetAndUpdate(Mesh, true);
	BodyShadow->SetSkinnedAssetAndUpdate(Mesh, true);
}

// =====================================================================
// Guns
// =====================================================================

UHoldoverGunData* AHoldoverCharacter::LoadGunData(int32 Slot)
{
	if (LoadedGunData.IsValidIndex(Slot) && LoadedGunData[Slot])
	{
		return LoadedGunData[Slot];
	}
	UHoldoverGunData* Data = nullptr;
	if (GunSlots.IsValidIndex(Slot) && !GunSlots[Slot].IsNull())
	{
		Data = GunSlots[Slot].LoadSynchronous();
	}
	if (!Data)
	{
		Data = NewObject<UHoldoverGunData>(this);
		if (Slot == 1)
		{
			UHoldoverGunData::ApplyPresetAK(Data);
		}
	}
	if (LoadedGunData.Num() <= Slot)
	{
		LoadedGunData.SetNum(Slot + 1);
	}
	LoadedGunData[Slot] = Data;
	return Data;
}

void AHoldoverCharacter::SpawnGun(int32 Slot)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Put the current gun away (it keeps its state: mag, chamber, slide).
	Animator->Stop();
	if (Gun)
	{
		Gun->SetTriggerHeld(false);
		Gun->SetActorHiddenInGame(true);
		Gun->SetActorTickEnabled(false);
	}

	if (Guns.Num() <= Slot)
	{
		Guns.SetNum(Slot + 1);
	}

	AHoldoverGun* NewGun = Guns[Slot];
	const bool bFresh = (NewGun == nullptr);
	if (bFresh)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		NewGun = World->SpawnActor<AHoldoverGun>(AHoldoverGun::StaticClass(), GunHold->GetComponentTransform(), Params);
		if (!NewGun)
		{
			return;
		}
		NewGun->AttachToComponent(GunHold, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		NewGun->InitGun(LoadGunData(Slot));
		NewGun->AddTickPrerequisiteActor(this);
		NewGun->OnFired.AddUObject(this, &AHoldoverCharacter::OnGunFired);
		NewGun->OnDryFire.AddUObject(this, &AHoldoverCharacter::OnGunDryFire);
		Guns[Slot] = NewGun;
	}

	Gun = NewGun;
	Gun->SetActorHiddenInGame(false);
	Gun->SetActorTickEnabled(true);
	CurrentSlot = Slot;
	Animator->SetGun(Gun);
	Animator->bHandHasMag = false;
	Recoil = FVector4(0, 0, 0, 0);
	RecoilVel = FVector4(0, 0, 0, 0);
	AimAlpha = 0.f;
	AimAlphaVel = 0.f;

	if (bFresh)
	{
		LoadGun(Gun);
	}
}

void AHoldoverCharacter::LoadGun(AHoldoverGun* InGun)
{
	// Full pouches, a full magazine in the gun and an empty chamber: rack before the first shot.
	const UHoldoverGunData* Data = InGun->GetData();
	TArray<FHoldoverMagazine>& P = Pouches.FindOrAdd(Data->GunId);
	P.Reset();
	for (int32 i = 0; i < Data->StartingMagazines; ++i)
	{
		P.Add(FHoldoverMagazine(Data->MagazineCapacity, Data->MagazineCapacity, NextMagId++));
	}
	const FHoldoverMagazine First(Data->MagazineCapacity, Data->MagazineCapacity, NextMagId++);
	InGun->ResetState(&First);
}

void AHoldoverCharacter::SelectGun(int32 Slot)
{
	if (Slot == CurrentSlot || !CanStartAction())
	{
		return;
	}
	// A mag in the hand goes back in the pouch before switching.
	if (Animator->bHandHasMag)
	{
		Pouch().Add(Animator->HandMag);
		Animator->bHandHasMag = false;
	}
	SpawnGun(Slot);
}

void AHoldoverCharacter::ResetLoadout()
{
	Animator->Stop();
	Animator->bHandHasMag = false;
	LooseRounds.Reset();
	for (AHoldoverGun* G : Guns)
	{
		if (G)
		{
			LoadGun(G);
		}
	}
	if (AHoldoverGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AHoldoverGameMode>() : nullptr)
	{
		GM->ResetRange();
	}
}

TArray<FHoldoverMagazine>& AHoldoverCharacter::Pouch()
{
	const FName Id = (Gun && Gun->GetData()) ? Gun->GetData()->GunId : NAME_None;
	return Pouches.FindOrAdd(Id);
}

bool AHoldoverCharacter::TakeMagFromPouch(FHoldoverMagazine& Out)
{
	TArray<FHoldoverMagazine>& P = Pouch();
	if (P.Num() == 0)
	{
		return false;
	}
	Out = P[0];
	P.RemoveAt(0);
	return true;
}

void AHoldoverCharacter::OnGunFired(AHoldoverGun* InGun)
{
	const UHoldoverGunData* D = InGun->GetData();
	const bool bTimeline = Animator->HasAction(TEXT("Fire"));
	// With a timeline the authored kick carries most of the motion; the spring adds the randomness.
	const float Scale = bTimeline ? 0.5f : 1.f;
	RecoilVel.X += D->RecoilPitchImpulse * FMath::FRandRange(0.85f, 1.15f) * Scale;
	RecoilVel.Y += FMath::FRandRange(-D->RecoilYawImpulse, D->RecoilYawImpulse) * Scale;
	RecoilVel.Z += D->RecoilBackImpulse * Scale;
	RecoilVel.W += D->RecoilRiseImpulse * Scale;
	RecoilDrift += D->RecoilResidual * D->RecoilPitchImpulse / FMath::Sqrt(FMath::Max(D->RecoilStiffness, 1.f));

	if (!Animator->IsBusy())
	{
		Animator->Play(TEXT("Fire"), false);
	}
}

void AHoldoverCharacter::OnGunDryFire(AHoldoverGun* InGun)
{
	if (!Animator->IsBusy())
	{
		Animator->Play(TEXT("DryFire"), false);
	}
}

bool AHoldoverCharacter::CanStartAction() const
{
	return Gun && !Animator->IsBusy() && !Gun->IsRackHeld();
}

bool AHoldoverCharacter::PlayAction(FName Action, bool bHoldable, FName InHoldKey)
{
	if (!Animator->Play(Action, bHoldable))
	{
		return false;
	}
	HoldKey = bHoldable ? InHoldKey : NAME_None;
	return true;
}

void AHoldoverCharacter::ShowCheck(const FString& Text, float Seconds)
{
	// Temporary until the magazine and chamber models show their rounds: what the eyes would see.
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(CheckMessageKey, Seconds, FColor(220, 220, 210), Text);
	}
}

void AHoldoverCharacter::OnAnimEvent(FName EventName)
{
	if (!Gun)
	{
		return;
	}
	const UHoldoverGunData* Data = Gun->GetData();
	const FString Name = EventName.ToString();

	if (Name == TEXT("MagDetach"))
	{
		FHoldoverMagazine M;
		if (Gun->RemoveMagazineToHand(M))
		{
			Animator->HandMag = M;
			Animator->bHandHasMag = true;
		}
	}
	else if (Name == TEXT("MagDrop"))
	{
		if (Animator->bHandHasMag)
		{
			const FTransform At = Gun->GetSocketWorldTransform(Data->MagazineBone);
			const FVector Down = Gun->GetActorTransform().TransformVectorNoScale(Data->MagazineOutDirection.GetSafeNormal());
			Gun->SpawnLooseMagazine(Animator->HandMag, At, GetVelocity() + Down * 150.f);
			Animator->bHandHasMag = false;
		}
	}
	else if (Name == TEXT("MagStow"))
	{
		if (Animator->bHandHasMag)
		{
			Pouch().Add(Animator->HandMag);
			Animator->bHandHasMag = false;
		}
	}
	else if (Name == TEXT("MagFromPouch"))
	{
		FHoldoverMagazine M;
		if (!Animator->bHandHasMag && TakeMagFromPouch(M))
		{
			Animator->HandMag = M;
			Animator->bHandHasMag = true;
		}
	}
	else if (Name == TEXT("MagAttach"))
	{
		if (Animator->bHandHasMag && Gun->InsertMagazine(Animator->HandMag))
		{
			Animator->bHandHasMag = false;
		}
	}
	else if (Name == TEXT("SlideLock") || Name == TEXT("BoltLock"))
	{
		if (Gun->IsRackHeld())
		{
			Gun->PressSlideStop();
		}
	}
	else if (Name == TEXT("SlideRelease") || Name == TEXT("BoltRelease"))
	{
		if (Gun->IsSlideLocked())
		{
			Gun->PressSlideStop();
		}
	}
	else if (Name == TEXT("SafetyOn") || Name == TEXT("SafetyOff"))
	{
		Gun->CycleSelector();
	}
	else if (Name == TEXT("MagCheckView"))
	{
		const int32 Rounds = Animator->bHandHasMag ? Animator->HandMag.Rounds : (Gun->HasMagazine() ? Gun->GetMagazine().Rounds : 0);
		ShowCheck(Rounds <= 0 ? FString(TEXT("Magazine is empty"))
			: Rounds == 1 ? FString(TEXT("1 round in the magazine"))
			: FString::Printf(TEXT("%d rounds in the magazine"), Rounds), 2.f);
	}
	else if (Name == TEXT("ChamberView"))
	{
		const EHoldoverChamber C = Gun->GetChamber();
		ShowCheck(C == EHoldoverChamber::Live || C == EHoldoverChamber::Dud ? TEXT("Brass in the chamber")
			: C == EHoldoverChamber::Spent ? TEXT("Fired case in the chamber")
			: TEXT("Chamber is empty"), 2.f);
	}
}

// =====================================================================
// Frame update
// =====================================================================

void AHoldoverCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Keep the eyes on this frame's look direction before anything is placed relative to them.
	if (Controller)
	{
		Camera->SetWorldRotation(GetControlRotation());
	}

	// Speed: jog, walk, or slower while aiming.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = AimAlpha > 0.5f ? AimWalkSpeed : (bJog ? JogSpeed : WalkSpeed);

	// Slight head motion while walking.
	const float Speed = GetVelocity().Size2D();
	if (Speed > 10.f && Move->IsMovingOnGround())
	{
		WalkPhase += DeltaSeconds * Speed / 70.f * PI; // about one step per 70 cm
	}
	else
	{
		WalkPhase = FMath::FInterpTo(WalkPhase, FMath::RoundToFloat(WalkPhase / PI) * PI, DeltaSeconds, 4.f);
	}
	const float Bob = FMath::Abs(FMath::Sin(WalkPhase)) * WalkBobHeight * FMath::Clamp(Speed / WalkSpeed, 0.f, 1.5f);
	Camera->SetRelativeLocation(FVector(EyeForward, 0.f, EyeHeight - CapsuleHalf - Bob));

	UpdateRecoil(DeltaSeconds);
	UpdateSway(DeltaSeconds);
	UpdateHold(DeltaSeconds);
}

namespace
{
	/** Smoothly limits a value to +-Max: nearly linear when small, never passes Max, no hard stop. */
	float SoftLimit(float V, float Max)
	{
		if (Max <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}
		const float R = V / Max;
		return V / FMath::Sqrt(1.f + R * R);
	}

	FVector SoftLimit(const FVector& V, float Max)
	{
		const float Len = V.Size();
		return Len > KINDA_SMALL_NUMBER ? V * (SoftLimit(Len, Max) / Len) : FVector::ZeroVector;
	}
}

void AHoldoverCharacter::UpdateSway(float DeltaSeconds)
{
	if (!Gun || !Controller || DeltaSeconds <= 0.f)
	{
		return;
	}
	const UHoldoverGunData* D = Gun->GetData();

	const FRotator View = GetControlRotation();
	const FVector ViewVelocity = Camera->GetComponentQuat().UnrotateVector(GetVelocity());
	if (!bHavePrevView)
	{
		PrevViewRotation = View;
		PrevViewVelocity = ViewVelocity;
		bHavePrevView = true;
	}
	const FVector2D ViewDelta(
		FMath::FindDeltaAngleDegrees(PrevViewRotation.Pitch, View.Pitch),
		FMath::FindDeltaAngleDegrees(PrevViewRotation.Yaw, View.Yaw));
	const FVector VelocityChange = ViewVelocity - PrevViewVelocity;
	PrevViewRotation = View;
	PrevViewVelocity = ViewVelocity;

	// A snap (spawn, teleport, reset) is not a turn: don't swing the gun for it.
	if (ViewDelta.GetAbsMax() > 60.f || VelocityChange.Size() > 3000.f)
	{
		SwayAngle = SwayGunVel = FVector2D::ZeroVector;
		MoveSway = MoveSwayVel = FVector::ZeroVector;
		return;
	}

	const float Weight = FMath::Max(D->SwayWeight, 0.2f);
	const float Damping = FMath::Max(SwayDampingRatio, 0.05f);

	// The gun is a weight on a spring that follows the view. The view turns at a steady rate through the frame;
	// the gun keeps its own turn speed, so it trails when you start turning and catches up (a touch past) when you stop.
	const float K = FMath::Max(SwayStiffness, 1.f) / Weight;
	const float C = 2.f * Damping * FMath::Sqrt(K);
	const FVector2D ViewRate = ViewDelta / DeltaSeconds;

	// Body motion: a change of walking speed nudges the hands the other way, then they settle.
	const float Km = FMath::Max(MoveSwayStiffness, 1.f) / Weight;
	const float Cm = 2.f * Damping * FMath::Sqrt(Km);
	MoveSwayVel -= VelocityChange * MoveInertia;

	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / (1.f / 240.f)), 1, 16);
	const float Dt = DeltaSeconds / Steps;
	for (int32 i = 0; i < Steps; ++i)
	{
		const FVector2D Acc = SwayAngle * -K - SwayGunVel * C;
		SwayGunVel += Acc * Dt;
		SwayAngle += (SwayGunVel - ViewRate) * Dt;

		const FVector AccM = MoveSway * -Km - MoveSwayVel * Cm;
		MoveSwayVel += AccM * Dt;
		MoveSway += MoveSwayVel * Dt;
	}

	// Guard against runaway on a very long frame; what you see is soft-limited later.
	const float Guard = FMath::Max(SwayMaxDegrees, 1.f) * 4.f;
	SwayAngle.X = FMath::Clamp(SwayAngle.X, -Guard, Guard);
	SwayAngle.Y = FMath::Clamp(SwayAngle.Y, -Guard, Guard);
	MoveSway = MoveSway.GetClampedToMaxSize(FMath::Max(MoveSwayMaxCm, 0.5f) * 4.f);
}

void AHoldoverCharacter::UpdateAimAlpha(float DeltaSeconds, bool bWantAim, float AimTime)
{
	// Critically damped spring: starts gently, arrives without a bump, and reverses smoothly if you let go half-way.
	// Reaches 95% of the way in AimTime.
	const float W = 4.75f / FMath::Max(AimTime, 0.05f);
	const float Target = bWantAim ? 1.f : 0.f;
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / (1.f / 240.f)), 1, 16);
	const float Dt = DeltaSeconds / Steps;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float Acc = W * W * (Target - AimAlpha) - 2.f * W * AimAlphaVel;
		AimAlphaVel += Acc * Dt;
		AimAlpha += AimAlphaVel * Dt;
	}
	if (AimAlpha < 0.f || AimAlpha > 1.f)
	{
		AimAlpha = FMath::Clamp(AimAlpha, 0.f, 1.f);
		AimAlphaVel = 0.f;
	}
}

void AHoldoverCharacter::UpdateRecoil(float DeltaSeconds)
{
	if (!Gun)
	{
		return;
	}
	const UHoldoverGunData* D = Gun->GetData();
	const float K = D->RecoilStiffness;
	const float C = D->RecoilDamping;
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / (1.f / 240.f)), 1, 16);
	const float Dt = DeltaSeconds / Steps;
	for (int32 s = 0; s < Steps; ++s)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const double Acc = -K * Recoil[i] - C * RecoilVel[i];
			RecoilVel[i] += Acc * Dt;
			Recoil[i] += RecoilVel[i] * Dt;
		}
	}
	RecoilDrift = FMath::FInterpTo(RecoilDrift, 0.f, DeltaSeconds, 1.5f);
}

void AHoldoverCharacter::UpdateHold(float DeltaSeconds)
{
	if (!Gun)
	{
		return;
	}
	const UHoldoverGunData* D = Gun->GetData();

	// Raise or lower the gun.
	const bool bWantAim = bAimHeld && !Animator->IsBusy();
	UpdateAimAlpha(DeltaSeconds, bWantAim, D->AimTime);
	const float A = AimAlpha;
	// The muzzle lines up a little before the gun arrives, as when a real shooter drives the front sight onto the target.
	const float RotA = 1.f - FMath::Pow(1.f - A, FMath::Max(D->AimRotationLead, 1.f));

	// Low ready.
	const FTransform Ready(D->ReadyRotation, D->ReadyOffset);

	// Aimed: rear sight on the eye line at the eye relief, sight line straight ahead.
	FVector Rear, Front;
	Gun->GetSightsLocal(Rear, Front);
	const FVector SightDir = (Front - Rear).GetSafeNormal();
	const FQuat AimRot = SightDir.IsNearlyZero() ? FQuat::Identity : FQuat::FindBetweenNormals(SightDir, FVector::ForwardVector);
	const FTransform Aimed(AimRot, FVector(D->AimEyeRelief, 0.f, 0.f) - AimRot.RotateVector(Rear));

	FTransform Hold;
	// Straight line from ready to the eye, bowed out by the arc (zero at both ends).
	Hold.SetLocation(FMath::Lerp(Ready.GetLocation(), Aimed.GetLocation(), A) + D->AimArc * FMath::Sin(PI * A));
	Hold.SetRotation(FQuat::Slerp(Ready.GetRotation(), Aimed.GetRotation(), RotA).GetNormalized());

	// Recoil: the gun turns about the shooting hand and is pushed back and up.
	const FVector GripLocal = Gun->GetSocketLocal(D->GripHandSocket);
	const FVector Pivot = Hold.TransformPosition(GripLocal);
	const FQuat Kick = FRotator(Recoil.X + RecoilDrift, Recoil.Y, 0.f).Quaternion();
	FTransform Kicked = Hold;
	Kicked.SetRotation(Kick * Hold.GetRotation());
	Kicked.SetLocation(Pivot + Kick.RotateVector(Hold.GetLocation() - Pivot) + FVector(-Recoil.Z, 0.f, Recoil.W));

	// Gun weight: it trails the view and body motion, turning about the wrist and forearm behind the grip.
	const float SwayScale = FMath::Max(SwayStrength, 0.f) * FMath::Lerp(1.f, SwayWhileAiming, A);
	const float LagPitch = SoftLimit(SwayAngle.X * SwayScale, SwayMaxDegrees);
	const float LagYaw = SoftLimit(SwayAngle.Y * SwayScale, SwayMaxDegrees);
	const FQuat SwayRot = FRotator(LagPitch, LagYaw, LagYaw * SwayRollPerYaw).Quaternion();
	const FVector SwayPivot = Kicked.TransformPosition(GripLocal) - FVector(SwayPivotBehindGrip, 0.f, 0.f);
	FTransform Swayed = Kicked;
	Swayed.SetRotation((SwayRot * Kicked.GetRotation()).GetNormalized());
	Swayed.SetLocation(SwayPivot + SwayRot.RotateVector(Kicked.GetLocation() - SwayPivot)
		+ SoftLimit(MoveSway * SwayScale, MoveSwayMaxCm));
	BaseGunInCamera = Swayed;

	FHoldoverAnimFrame Frame;
	Frame.BaseGunInCamera = BaseGunInCamera;
	Frame.CameraWorld = Camera->GetComponentTransform();
	Frame.FeetWorld = FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), GetActorLocation() - FVector(0.f, 0.f, CapsuleHalf));
	Animator->Update(DeltaSeconds, Frame);

	GunHold->SetRelativeTransform(Animator->GetGunInCamera());
}

// =====================================================================
// Input
// =====================================================================

void AHoldoverCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!Inputs)
	{
		Inputs = NewObject<UHoldoverInputSet>(this);
		Inputs->Build();
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogHoldover, Error, TEXT("Enhanced Input is not the default input component; check Config/DefaultInput.ini."));
		return;
	}

	EIC->BindAction(Inputs->Move, ETriggerEvent::Triggered, this, &AHoldoverCharacter::OnMove);
	EIC->BindAction(Inputs->Look, ETriggerEvent::Triggered, this, &AHoldoverCharacter::OnLook);
	EIC->BindAction(Inputs->Jog, ETriggerEvent::Started, this, &AHoldoverCharacter::OnJogStart);
	EIC->BindAction(Inputs->Jog, ETriggerEvent::Completed, this, &AHoldoverCharacter::OnJogEnd);
	EIC->BindAction(Inputs->Trigger, ETriggerEvent::Started, this, &AHoldoverCharacter::OnTriggerPressed);
	EIC->BindAction(Inputs->Trigger, ETriggerEvent::Completed, this, &AHoldoverCharacter::OnTriggerReleased);
	EIC->BindAction(Inputs->Aim, ETriggerEvent::Started, this, &AHoldoverCharacter::OnAimPressed);
	EIC->BindAction(Inputs->Aim, ETriggerEvent::Completed, this, &AHoldoverCharacter::OnAimReleased);
	EIC->BindAction(Inputs->Rack, ETriggerEvent::Started, this, &AHoldoverCharacter::OnRackPressed);
	EIC->BindAction(Inputs->Rack, ETriggerEvent::Completed, this, &AHoldoverCharacter::OnRackReleased);
	EIC->BindAction(Inputs->SlideStop, ETriggerEvent::Started, this, &AHoldoverCharacter::OnSlideStop);
	EIC->BindAction(Inputs->MagDrop, ETriggerEvent::Started, this, &AHoldoverCharacter::OnMagDrop);
	EIC->BindAction(Inputs->MagStow, ETriggerEvent::Started, this, &AHoldoverCharacter::OnMagStow);
	EIC->BindAction(Inputs->MagInsert, ETriggerEvent::Started, this, &AHoldoverCharacter::OnMagInsert);
	EIC->BindAction(Inputs->MagCheck, ETriggerEvent::Started, this, &AHoldoverCharacter::OnMagCheckPressed);
	EIC->BindAction(Inputs->MagCheck, ETriggerEvent::Completed, this, &AHoldoverCharacter::OnMagCheckReleased);
	EIC->BindAction(Inputs->PressCheck, ETriggerEvent::Started, this, &AHoldoverCharacter::OnPressCheckPressed);
	EIC->BindAction(Inputs->PressCheck, ETriggerEvent::Completed, this, &AHoldoverCharacter::OnPressCheckReleased);
	EIC->BindAction(Inputs->Safety, ETriggerEvent::Started, this, &AHoldoverCharacter::OnSafety);
	EIC->BindAction(Inputs->Interact, ETriggerEvent::Started, this, &AHoldoverCharacter::OnInteract);
	EIC->BindAction(Inputs->SelectPistol, ETriggerEvent::Started, this, &AHoldoverCharacter::OnSelectPistol);
	EIC->BindAction(Inputs->SelectRifle, ETriggerEvent::Started, this, &AHoldoverCharacter::OnSelectRifle);
	EIC->BindAction(Inputs->ResetRange, ETriggerEvent::Started, this, &AHoldoverCharacter::OnResetRange);
}

void AHoldoverCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !Inputs)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Sub->ClearAllMappings();
		Sub->AddMappingContext(Inputs->ManualContext, 0);
	}
}

void AHoldoverCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), V.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), V.X);
}

void AHoldoverCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>() * MouseSensitivity;
	AddControllerYawInput(V.X);
	AddControllerPitchInput(V.Y);
}

void AHoldoverCharacter::OnJogStart() { bJog = true; }
void AHoldoverCharacter::OnJogEnd() { bJog = false; }

void AHoldoverCharacter::OnTriggerPressed()
{
	if (Gun)
	{
		Gun->SetTriggerHeld(true);
	}
}

void AHoldoverCharacter::OnTriggerReleased()
{
	if (Gun)
	{
		Gun->SetTriggerHeld(false);
	}
}

void AHoldoverCharacter::OnAimPressed() { bAimHeld = true; }
void AHoldoverCharacter::OnAimReleased() { bAimHeld = false; }

void AHoldoverCharacter::OnRackPressed()
{
	if (!CanStartAction())
	{
		return;
	}
	const bool bPistol = Gun->GetData()->bIsPistol;
	FName Action;
	if (Gun->IsSlideLocked())
	{
		Action = bPistol ? FName(TEXT("SlingshotRelease")) : FName(TEXT("ChargeFromLock"));
	}
	else
	{
		Action = bPistol ? FName(TEXT("RackSlide")) : FName(TEXT("ChargeHandle"));
	}
	if (!Animator->HasAction(Action))
	{
		Action = bPistol ? FName(TEXT("RackSlide")) : FName(TEXT("ChargeHandle"));
	}
	if (!PlayAction(Action, true, KeyRack))
	{
		// No timeline: work the slide directly (hold to keep it back, release to let it go).
		Gun->BeginRack();
		HoldKey = KeyRack;
	}
}

void AHoldoverCharacter::OnRackReleased()
{
	if (HoldKey != KeyRack)
	{
		return;
	}
	if (Animator->IsPlaying())
	{
		Animator->SetHoldInput(false);
	}
	else if (Gun)
	{
		Gun->EndRack();
	}
	HoldKey = NAME_None;
}

void AHoldoverCharacter::OnSlideStop()
{
	if (!Gun)
	{
		return;
	}
	if (Gun->IsRackHeld())
	{
		Gun->PressSlideStop(); // thumb the stop up while the slide is held back: it locks open
		return;
	}
	if (!Gun->IsSlideLocked() || !CanStartAction())
	{
		return;
	}
	const FName Action = Gun->GetData()->bIsPistol ? FName(TEXT("SlideRelease")) : FName(TEXT("BoltRelease"));
	if (!PlayAction(Action, false, NAME_None))
	{
		Gun->PressSlideStop();
	}
}

void AHoldoverCharacter::OnMagDrop()
{
	if (!Gun || !Gun->HasMagazine() || !CanStartAction())
	{
		return;
	}
	if (!PlayAction(TEXT("MagDrop"), false, NAME_None))
	{
		Gun->DropMagazine();
	}
}

void AHoldoverCharacter::OnMagStow()
{
	if (!Gun || !Gun->HasMagazine() || !CanStartAction())
	{
		return;
	}
	if (!PlayAction(TEXT("MagOut"), false, NAME_None))
	{
		FHoldoverMagazine M;
		if (Gun->RemoveMagazineToHand(M))
		{
			Pouch().Add(M);
		}
	}
}

void AHoldoverCharacter::OnMagInsert()
{
	if (!Gun || Gun->HasMagazine() || !CanStartAction() || Pouch().Num() == 0)
	{
		return;
	}
	if (!PlayAction(TEXT("MagIn"), false, NAME_None))
	{
		FHoldoverMagazine M;
		if (TakeMagFromPouch(M))
		{
			Gun->InsertMagazine(M);
		}
	}
}

void AHoldoverCharacter::OnMagCheckPressed()
{
	if (!Gun || !Gun->HasMagazine() || !CanStartAction())
	{
		return;
	}
	if (!PlayAction(TEXT("MagCheck"), true, KeyMagCheck))
	{
		OnAnimEvent(TEXT("MagCheckView"));
	}
}

void AHoldoverCharacter::OnMagCheckReleased()
{
	if (HoldKey == KeyMagCheck)
	{
		Animator->SetHoldInput(false);
		HoldKey = NAME_None;
	}
}

void AHoldoverCharacter::OnPressCheckPressed()
{
	if (!Gun || !CanStartAction())
	{
		return;
	}
	if (!PlayAction(TEXT("PressCheck"), true, KeyPressCheck))
	{
		OnAnimEvent(TEXT("ChamberView"));
	}
}

void AHoldoverCharacter::OnPressCheckReleased()
{
	if (HoldKey == KeyPressCheck)
	{
		Animator->SetHoldInput(false);
		HoldKey = NAME_None;
	}
}

void AHoldoverCharacter::OnSafety()
{
	if (!Gun || !CanStartAction())
	{
		return;
	}
	const UHoldoverGunData* D = Gun->GetData();
	if (!D->bHasSafety && !D->bHasFullAuto)
	{
		return;
	}
	// Which way the lever goes next decides the movement.
	TArray<EHoldoverFireMode> Order;
	if (D->bHasSafety) Order.Add(EHoldoverFireMode::Safe);
	if (D->bHasFullAuto) Order.Add(EHoldoverFireMode::Auto);
	Order.Add(EHoldoverFireMode::Semi);
	const EHoldoverFireMode Next = Order[(Order.IndexOfByKey(Gun->GetFireMode()) + 1) % Order.Num()];

	const FName Action = Next == EHoldoverFireMode::Safe ? FName(TEXT("SafetyOn")) : FName(TEXT("SafetyOff"));
	if (!PlayAction(Action, false, NAME_None))
	{
		Gun->CycleSelector();
	}
}

void AHoldoverCharacter::OnInteract()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * ReachDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HoldoverReach), false, this);
	if (Gun)
	{
		Params.AddIgnoredActor(Gun);
	}
	FHitResult Hit;
	if (!World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(8.f), Params))
	{
		return;
	}
	AHoldoverLoosePiece* Piece = Cast<AHoldoverLoosePiece>(Hit.GetActor());
	if (!Piece)
	{
		return;
	}
	switch (Piece->Kind)
	{
	case EHoldoverPieceKind::Magazine:
		Pouches.FindOrAdd(Piece->GunId).Add(Piece->Magazine);
		Piece->Destroy();
		break;
	case EHoldoverPieceKind::LiveRound:
		LooseRounds.FindOrAdd(Gun ? Gun->GetData()->GunId : NAME_None) += 1;
		Piece->Destroy();
		break;
	default:
		break; // fired cases stay where they fell
	}
}

void AHoldoverCharacter::OnSelectPistol() { SelectGun(0); }
void AHoldoverCharacter::OnSelectRifle() { SelectGun(1); }
void AHoldoverCharacter::OnResetRange() { ResetLoadout(); }
