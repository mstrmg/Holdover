#include "Player/HoldoverCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/HoldoverGunAnimator.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Core/HoldoverDebug.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/HoldoverGameMode.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Holdover.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
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
	// The eyes follow the look direction every frame in Tick (plus the recoil's share), so the camera must not snap
	// back to the plain control rotation on its own.
	Camera->bUsePawnControlRotation = false;
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

	// First-person arms ("viewmodel"): a hidden copy plays the idle hold animation, the poseable copy is drawn
	// and moved with the gun by the body rig.
	ArmsDriver = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmsDriver"));
	ArmsDriver->SetupAttachment(Camera);
	ArmsDriver->SetHiddenInGame(true);
	ArmsDriver->SetCastShadow(false);
	ArmsDriver->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArmsDriver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	ArmsDriver->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	ArmsMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("ArmsMesh"));
	ArmsMesh->SetupAttachment(Camera);
	ArmsMesh->SetCastShadow(false);
	ArmsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArmsMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	ArmsMesh->bUseAttachParentBound = false;

	PistolArmsIdle = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS")));
	RifleArmsIdle = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS")));

	Animator = CreateDefaultSubobject<UHoldoverGunAnimator>(TEXT("GunAnimator"));
	BodyRig = CreateDefaultSubobject<UHoldoverBodyRig>(TEXT("BodyRig"));
}

void AHoldoverCharacter::UpdateArmsAnimation()
{
	if (!ArmsDriver->GetSkeletalMeshAsset() || !Gun || !Gun->GetData())
	{
		return;
	}
	const TSoftObjectPtr<UAnimSequence>& Wanted = Gun->GetData()->bIsPistol ? PistolArmsIdle : RifleArmsIdle;
	UAnimSequence* Seq = Wanted.IsNull() ? nullptr : Wanted.LoadSynchronous();
	if (!Seq)
	{
		UE_LOG(LogHoldover, Warning, TEXT("No first-person arms animation (%s): the arms show their reference pose."), *Wanted.ToString());
		return;
	}
	ArmsDriver->PlayAnimation(Seq, true);
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
	USkeletalMesh* LoadedBody = BodyMesh.IsNull() ? nullptr : BodyMesh.LoadSynchronous();
	if (!LoadedBody)
	{
		UE_LOG(LogHoldover, Warning, TEXT("No body mesh (%s). Add the Third Person content pack to see your body; the gun works without it."),
			*BodyMesh.ToString());
		BodyRig->SetComponentTickEnabled(false);
		return;
	}

	USkeletalMeshComponent* Driver = GetMesh();
	Driver->SetSkeletalMeshAsset(LoadedBody);
	if (UClass* AnimClass = BodyAnimClass.IsNull() ? nullptr : BodyAnimClass.LoadSynchronous())
	{
		Driver->SetAnimInstanceClass(AnimClass);
	}
	else
	{
		UE_LOG(LogHoldover, Warning, TEXT("No body animation (%s): the body stands still."), *BodyAnimClass.ToString());
	}

	BodyVisible->SetSkinnedAssetAndUpdate(LoadedBody, true);
	BodyShadow->SetSkinnedAssetAndUpdate(LoadedBody, true);

	ArmsDriver->SetSkeletalMeshAsset(LoadedBody);
	ArmsDriver->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	ArmsMesh->SetSkinnedAssetAndUpdate(LoadedBody, true);
	BodyRig->SetupArms(ArmsDriver, ArmsMesh);
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
		UHoldoverGunData::ApplyPresetForSlot(Data, Slot);
	}
	UHoldoverGunData::LoadImportedArt(Data);
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
	ResetRecoil();
	AimAlpha = 0.f;
	AimAlphaVel = 0.f;
	UpdateArmsAnimation();

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

	// Roughly bell-shaped randomness: most shots near the average, the odd one well off it.
	auto Bell = [](float Spread) { return (FMath::FRand() + FMath::FRand() + FMath::FRand() - 1.5f) * Spread * 1.15f; };

	// The kick: never the same twice, and never straight up.
	KickVel[0] += D->RecoilPitchImpulse * FMath::Max(0.3f, 1.f + Bell(D->RecoilPitchVariance));
	KickVel[1] += D->RecoilYawBias + Bell(D->RecoilYawImpulse);
	KickVel[2] += D->RecoilRollBias + Bell(D->RecoilRollImpulse);
	KickVel[3] += D->RecoilBackImpulse * FMath::Max(0.3f, 1.f + Bell(0.15f));
	KickVel[4] += D->RecoilRiseImpulse * FMath::Max(0.f, 1.f + Bell(0.3f));
	KickVel[5] += Bell(D->RecoilSideImpulse);

	// The climb: the whole aim walks up and wanders sideways; quick strings stack up before the shooter catches it.
	ClimbVel[0] += D->RecoilClimbImpulse * FMath::Max(0.2f, 1.f + Bell(0.35f));
	ClimbVel[1] += D->RecoilWanderBias + Bell(D->RecoilWanderImpulse);

	// The blast's short tremor.
	ShakeAmp = FMath::Max(ShakeAmp * FMath::Exp(-ShakeTime / 0.045f), D->RecoilShake * FMath::FRandRange(0.8f, 1.2f));
	ShakeTime = 0.f;
	ShakePhase = FVector(FMath::FRandRange(0.f, 2.f * PI), FMath::FRandRange(0.f, 2.f * PI), FMath::FRandRange(0.f, 2.f * PI));

	// The trigger finger's press and release. The shot has already broken, so start the timeline at its break.
	if (!Animator->IsBusy())
	{
		const float Break = Animator->GetEventTime(TEXT("Fire"), TEXT("ShotBreak"));
		Animator->Play(TEXT("Fire"), false, FMath::Max(Break, 0.f));
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

	// Test command: holdover.Gun <slot>.
	const int32 WantGun = HoldoverDebug::SelectGun();
	if (WantGun >= 0)
	{
		HoldoverDebug::CVarSelectGun->Set(-1, ECVF_SetByConsole);
		if (WantGun < UHoldoverGunData::NumPresetSlots)
		{
			SelectGun(WantGun);
		}
	}

	UpdateRecoil(DeltaSeconds);
	ApplyHeadKick();
	UpdateSway(DeltaSeconds);
	UpdateHold(DeltaSeconds);
	UpdateDebugView();
	UpdateTestShots(DeltaSeconds);
}

// =====================================================================
// Test tools
// =====================================================================

void AHoldoverCharacter::UpdateDebugView()
{
	APlayerController* PC = Cast<APlayerController>(Controller);
	const int32 View = HoldoverDebug::DebugView();
	if (!PC)
	{
		return;
	}
	// Close-ups (4, 5) show only the first-person arms and the gun.
	const bool bArmsOnly = View >= 4;
	BodyVisible->SetVisibility(!bArmsOnly);
	BodyShadow->SetVisibility(!bArmsOnly);
	// 7 = the normal eye view with the full body hidden (to tell which mesh is which); 8 = eyes with the arms hidden.
	ArmsMesh->SetVisibility(View != 8);
	if (View == 8)
	{
		BodyVisible->SetVisibility(true);
		BodyShadow->SetVisibility(true);
	}
	if (View <= 0 || View >= 7)
	{
		if (AppliedDebugView != 0)
		{
			PC->SetViewTarget(this);
			AppliedDebugView = 0;
		}
		return;
	}
	if (!DebugCam)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		DebugCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		if (!DebugCam)
		{
			return;
		}
		DebugCam->GetCameraComponent()->SetFieldOfView(60.f);
		DebugCam->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	// Placed relative to the eyes, looking at the hands: 1 = from the right side, 2 = from the front, 3 = from behind and above.
	const FTransform Eyes = Camera->GetComponentTransform();
	FVector Look = Eyes.TransformPosition(FVector(30.f, 0.f, -18.f));
	float CloseDist = 45.f;
	if (bArmsOnly && Gun && Gun->GetData())
	{
		// Between the two hands, far enough back to see both.
		const FVector Grip = Gun->GetSocketWorldTransform(Gun->GetData()->GripHandSocket).GetLocation();
		const FVector Support = Gun->GetSocketWorldTransform(Gun->GetData()->SupportHandSocket).GetLocation();
		Look = 0.5f * (Grip + Support);
		CloseDist += FVector::Dist(Grip, Support);
	}
	FVector From;
	switch (View)
	{
	case 4: From = Look + Eyes.GetRotation().RotateVector(FVector(10.f, CloseDist, 10.f)); break;  // close, right side
	case 5: From = Look + Eyes.GetRotation().RotateVector(FVector(10.f, -CloseDist, 10.f)); break; // close, left side
	case 6: From = Look + Eyes.GetRotation().RotateVector(FVector(15.f, -10.f, -CloseDist)); break; // close, from below left
	case 2: From = Eyes.TransformPosition(FVector(170.f, 25.f, 0.f)); break;
	case 3: From = Eyes.TransformPosition(FVector(-70.f, 45.f, 35.f)); break;
	default: From = Eyes.TransformPosition(FVector(35.f, 150.f, 5.f)); break;
	}
	DebugCam->SetActorLocationAndRotation(From, (Look - From).Rotation());
	if (AppliedDebugView != View || PC->GetViewTarget() != DebugCam)
	{
		PC->SetViewTarget(DebugCam);
		AppliedDebugView = View;
	}
}

void AHoldoverCharacter::LogArmDiagnostics(const FString& Label) const
{
	const FTransform Eyes = Camera->GetComponentTransform();
	// The arms you see are the first-person arms when they are set up, else the full body.
	UPoseableMeshComponent* Src = (ArmsMesh && ArmsMesh->GetSkinnedAsset()) ? ArmsMesh.Get() : BodyVisible.Get();
	auto Bone = [Src, &Eyes](const TCHAR* Name)
	{
		if (!Src || !Src->GetSkinnedAsset() || Src->GetBoneIndex(Name) == INDEX_NONE)
		{
			return FString(TEXT("n/a"));
		}
		const FVector W = Src->GetBoneLocationByName(Name, EBoneSpaces::WorldSpace);
		return Eyes.InverseTransformPosition(W).ToCompactString();
	};
	if (Gun && Gun->GetData() && Src && Src->GetBoneIndex(TEXT("hand_l")) != INDEX_NONE)
	{
		// Support hand: where the gun timelines want the palm, and where the left wrist actually ended up (gun space, cm).
		const FTransform GunTM = Gun->GetActorTransform();
		const FTransform Target = Animator->GetHandInCamera(true) * Eyes;
		const FVector WristWant = Target.GetLocation() - Target.GetRotation().GetAxisX() * BodyRig->PalmToWrist - Target.GetRotation().GetAxisZ() * BodyRig->PalmDepth;
		const FVector WristGot = Src->GetBoneLocationByName(TEXT("hand_l"), EBoneSpaces::WorldSpace);
		UE_LOG(LogHoldover, Log, TEXT("[Arms %s] left palm target %s fingers %s palm %s | wrist want %s got %s miss %.1f cm | grip %s support %s"), *Label,
			*GunTM.InverseTransformPosition(Target.GetLocation()).ToCompactString(),
			*GunTM.InverseTransformVector(Target.GetRotation().GetAxisX()).ToCompactString(),
			*GunTM.InverseTransformVector(Target.GetRotation().GetAxisZ()).ToCompactString(),
			*GunTM.InverseTransformPosition(WristWant).ToCompactString(), *GunTM.InverseTransformPosition(WristGot).ToCompactString(),
			FVector::Dist(WristWant, WristGot),
			*Gun->GetSocketLocal(Gun->GetData()->GripHandSocket).ToCompactString(), *Gun->GetSocketLocal(Gun->GetData()->SupportHandSocket).ToCompactString());
		// Which way the solved left hand really faces: fingers = wrist to middle knuckle, palm from the knuckle line,
		// thumb = wrist to thumb tip joint. Compare with the target above.
		auto W = [Src](const TCHAR* N) { return Src->GetBoneLocationByName(N, EBoneSpaces::WorldSpace); };
		const FVector Fingers = (W(TEXT("middle_01_l")) - WristGot).GetSafeNormal();
		const FVector Across = (W(TEXT("index_01_l")) - W(TEXT("pinky_01_l"))).GetSafeNormal();
		const FVector PalmA = FVector::CrossProduct(Fingers, Across).GetSafeNormal();
		const FVector Thumb = (W(TEXT("thumb_02_l")) - WristGot).GetSafeNormal();
		UE_LOG(LogHoldover, Log, TEXT("[Arms %s] left hand actual fingers %s fingersxacross %s thumb %s | middle_03 %s"), *Label,
			*GunTM.InverseTransformVector(Fingers).ToCompactString(), *GunTM.InverseTransformVector(PalmA).ToCompactString(),
			*GunTM.InverseTransformVector(Thumb).ToCompactString(),
			*GunTM.InverseTransformPosition(W(TEXT("middle_03_l"))).ToCompactString());
	}
	if (Gun && Gun->GetData())
	{
		UE_LOG(LogHoldover, Log, TEXT("[Arms %s] weapon_r %s gun GripHand %s SupportHand %s"), *Label, *Bone(TEXT("weapon_r")),
			*Eyes.InverseTransformPosition(Gun->GetSocketWorldTransform(Gun->GetData()->GripHandSocket).GetLocation()).ToCompactString(),
			*Eyes.InverseTransformPosition(Gun->GetSocketWorldTransform(Gun->GetData()->SupportHandSocket).GetLocation()).ToCompactString());
	}
	UE_LOG(LogHoldover, Log, TEXT("[Arms %s] eye-space cm (X fwd, Y right, Z up): head %s neck %s clav_r %s upperarm_r %s lowerarm_r %s hand_r %s | upperarm_l %s lowerarm_l %s hand_l %s"),
		*Label, *Bone(TEXT("head")), *Bone(TEXT("neck_01")), *Bone(TEXT("clavicle_r")), *Bone(TEXT("upperarm_r")), *Bone(TEXT("lowerarm_r")),
		*Bone(TEXT("hand_r")), *Bone(TEXT("upperarm_l")), *Bone(TEXT("lowerarm_l")), *Bone(TEXT("hand_l")));
	UE_LOG(LogHoldover, Log, TEXT("[Arms %s] drawn wrist off its place on the gun, worst since last shot: %.2f cm"), *Label, BodyRig->MaxHandMiss);
	BodyRig->MaxHandMiss = 0.f;
	UE_LOG(LogHoldover, Log, TEXT("[Arms %s] targets: handR %s handL %s gun %s aim %.2f"), *Label,
		*Animator->GetHandInCamera(false).GetLocation().ToCompactString(), *Animator->GetHandInCamera(true).GetLocation().ToCompactString(),
		*GunHold->GetRelativeLocation().ToCompactString(), AimAlpha);
}

void AHoldoverCharacter::UpdateTestShots(float DeltaSeconds)
{
	const int32 Mode = HoldoverDebug::TestShots();
	if (Mode <= 0)
	{
		return;
	}
	TestTime += DeltaSeconds;
	if (Mode == 2 || Mode == 3)
	{
		UpdateFireTest();
		return;
	}
	if (Mode == 4)
	{
		UpdateActionTest();
		return;
	}

	// Each step happens at its time; a screenshot is taken one step after the view is set so the frame shows it.
	struct FStep { float T; int32 Slot; int32 Aim; int32 View; const TCHAR* Shot; int32 Do = 0; }; // Do: 1/2 = press/release rack, 3/4/5 = look down 45/70/level, 8 = look down 30 (as when working the gun), 6/7 = press/release trigger
	static const FStep Steps[] = {
		{ 4.0f, 0, 0, 0, nullptr },
		{ 4.5f, 0, 0, 0, TEXT("pistol_ready_eyes") },
		{ 5.0f, 0, 0, 1, nullptr },
		{ 5.5f, 0, 0, 1, TEXT("pistol_ready_side") },
		{ 6.0f, 0, 0, 3, nullptr },
		{ 6.5f, 0, 0, 3, TEXT("pistol_ready_behind") },
		{ 6.6f, 0, 0, 4, nullptr },
		{ 6.75f, 0, 0, 4, TEXT("pistol_ready_closeR") },
		{ 6.85f, 0, 0, 5, nullptr },
		{ 6.95f, 0, 0, 5, TEXT("pistol_ready_closeL") },
		{ 7.0f, 0, 1, 0, nullptr },
		{ 9.0f, 0, 1, 0, TEXT("pistol_aim_eyes") },
		{ 9.5f, 0, 1, 1, nullptr },
		{ 10.0f, 0, 1, 1, TEXT("pistol_aim_side") },
		{ 10.2f, 0, 1, 4, nullptr },
		{ 10.4f, 0, 1, 4, TEXT("pistol_aim_closeR") },
		{ 10.6f, 0, 1, 5, nullptr },
		{ 10.8f, 0, 1, 5, TEXT("pistol_aim_closeL") },
		{ 10.9f, 0, 1, 6, nullptr },
		{ 10.98f, 0, 1, 6, TEXT("pistol_aim_below") },
		{ 11.0f, 0, 0, 0, nullptr, 1 },
		{ 11.3f, 0, 0, 0, TEXT("pistol_rack_eyes") },
		{ 11.35f, 0, 0, 5, nullptr },
		{ 11.45f, 0, 0, 5, TEXT("pistol_rack_closeL"), 2 },
		{ 12.0f, 0, 1, 0, nullptr },
		{ 12.15f, 0, 1, 0, TEXT("pistol_aimin_eyes") },
		{ 12.8f, 0, 1, 0, nullptr, 6 },
		{ 12.86f, 0, 1, 0, TEXT("pistol_recoil_eyes"), 7 },
		{ 13.3f, 0, 0, 0, nullptr, 3 },
		{ 13.8f, 0, 0, 0, TEXT("pistol_down45_eyes") },
		{ 13.9f, 0, 0, 0, nullptr, 4 },
		{ 14.4f, 0, 0, 0, TEXT("pistol_down70_eyes") },
		{ 14.6f, 0, 0, 7, nullptr },
		{ 14.9f, 0, 0, 7, TEXT("pistol_down70_nobody") },
		{ 15.2f, 0, 0, 8, nullptr },
		{ 15.5f, 0, 0, 8, TEXT("pistol_down70_noarms") },
		{ 15.8f, 0, 0, 0, nullptr, 5 },
		{ 15.0f, 1, 0, 0, nullptr },
		{ 17.0f, 1, 0, 0, TEXT("rifle_ready_eyes") },
		{ 17.5f, 1, 0, 1, nullptr },
		{ 18.0f, 1, 0, 1, TEXT("rifle_ready_side") },
		{ 18.5f, 1, 1, 0, nullptr },
		{ 18.7f, 1, 1, 0, TEXT("rifle_aimin_eyes") },
		{ 20.5f, 1, 1, 0, TEXT("rifle_aim_eyes") },
		{ 21.0f, 1, 1, 1, nullptr },
		{ 21.5f, 1, 1, 1, TEXT("rifle_aim_side") },
		{ 21.7f, 1, 1, 4, nullptr },
		{ 21.9f, 1, 1, 4, TEXT("rifle_aim_closeR") },
		{ 22.1f, 1, 1, 5, nullptr },
		{ 22.3f, 1, 1, 5, TEXT("rifle_aim_closeL") },
		{ 22.4f, 1, 1, 6, nullptr },
		{ 22.48f, 1, 1, 6, TEXT("rifle_aim_below") },
		{ 22.6f, 1, 0, 0, nullptr, 3 },
		{ 23.1f, 1, 0, 0, TEXT("rifle_down45_eyes") },
		{ 23.2f, 1, 0, 0, nullptr, 4 },
		{ 23.7f, 1, 0, 0, TEXT("rifle_down70_eyes") },
		{ 23.8f, 1, 0, 0, nullptr, 5 },
		// Rifle charging handle: the support hand leaving the handguard, on the handle, and held at the back.
		{ 24.2f, 1, 0, 0, nullptr, 8 },
		{ 24.3f, 1, 0, 0, nullptr, 1 },
		{ 24.42f, 1, 0, 0, TEXT("rifle_rack_reach_eyes") },
		{ 24.62f, 1, 0, 0, TEXT("rifle_rack_grab_eyes") },
		{ 25.0f, 1, 0, 0, TEXT("rifle_rack_pulled_eyes") },
		{ 25.1f, 1, 0, 1, nullptr },
		{ 25.3f, 1, 0, 1, TEXT("rifle_rack_pulled_side") },
		{ 25.4f, 1, 0, 4, nullptr },
		{ 25.6f, 1, 0, 4, TEXT("rifle_rack_pulled_closeR") },
		{ 25.7f, 1, 0, 5, nullptr },
		{ 25.9f, 1, 0, 5, TEXT("rifle_rack_pulled_closeL") },
		{ 26.0f, 1, 0, 0, nullptr, 2 },
		{ 26.15f, 1, 0, 0, TEXT("rifle_rack_return_eyes") },
		{ 27.0f, 1, 0, 0, TEXT("rifle_rack_done_eyes") },
		{ 27.1f, 1, 0, 0, nullptr, 5 },
		// Rifle shot (chambered by the rack above): the gun kicks fast, so a hand trailing the gun shows here and
		// in the log's "drawn wrist off its place" line.
		{ 27.2f, 1, 1, 0, nullptr },
		{ 28.6f, 1, 1, 0, nullptr, 6 },
		{ 28.65f, 1, 1, 0, TEXT("rifle_recoil_eyes"), 7 },
		{ 29.0f, 1, 0, 0, TEXT("rifle_after_shot_eyes") },
	};
	constexpr int32 NumSteps = UE_ARRAY_COUNT(Steps);
	while (TestStep < NumSteps && TestTime >= Steps[TestStep].T)
	{
		const FStep& S = Steps[TestStep++];
		HoldoverDebug::CVarForceAim->Set(S.Aim, ECVF_SetByConsole);
		HoldoverDebug::CVarDebugView->Set(S.View, ECVF_SetByConsole);
		if (S.Slot != CurrentSlot)
		{
			SelectGun(S.Slot);
		}
		if (S.Do == 1) { OnRackPressed(); }
		if (S.Do == 6) { OnTriggerPressed(); }
		if ((S.Do >= 3 && S.Do <= 5) || S.Do == 8)
		{
			if (Controller)
			{
				FRotator R = Controller->GetControlRotation();
				R.Pitch = S.Do == 3 ? -45.f : (S.Do == 4 ? -70.f : (S.Do == 8 ? -30.f : 0.f));
				Controller->SetControlRotation(R);
			}
		}
		if (S.Shot)
		{
			const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots/test") / (FString(S.Shot) + TEXT(".png"));
			FScreenshotRequest::RequestScreenshot(File, false, false);
			LogArmDiagnostics(S.Shot);
		}
		if (S.Do == 2) { OnRackReleased(); }
		if (S.Do == 7) { OnTriggerReleased(); }
	}
	if (TestStep >= NumSteps && Mode == 1)
	{
		HoldoverDebug::CVarTestShots->Set(0, ECVF_SetByConsole);
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	}
}

void AHoldoverCharacter::UpdateActionTest()
{
	// holdover.TestShots 4: racking, mag check and a reload on the M9 and the AK (to check the slide and magazine
	// moves), then a full-auto burst on the AK. Pictures: Saved/Screenshots/test/act_<gun>_*.png.
	static const TCHAR* Names[] = { TEXT("m9"), TEXT("ak") };
	enum EDo { Select, Ready, Aim, RackP, RackR, Close, Eyes, CheckP, CheckR, Drop, Insert, Auto, Trig, Release, Yaw, Shot };
	struct FStep { float T; EDo Do; const TCHAR* Shot; };
	static const FStep Seq[] = {
		{ 0.0f, Select, nullptr }, { 0.1f, Ready, nullptr },
		{ 1.0f, RackP, nullptr }, { 1.45f, Shot, TEXT("rack_held") }, { 1.5f, Close, nullptr }, { 1.75f, Shot, TEXT("rack_held_close") },
		{ 1.9f, RackR, nullptr }, { 2.05f, Shot, TEXT("rack_released_close") }, { 2.3f, Eyes, nullptr },
		{ 3.0f, CheckP, nullptr }, { 3.7f, Shot, TEXT("magcheck") }, { 3.9f, CheckR, nullptr },
		{ 5.0f, Drop, nullptr }, { 5.25f, Shot, TEXT("magdrop") }, { 6.2f, Insert, nullptr }, { 6.6f, Shot, TEXT("maginsert") },
		{ 7.6f, Shot, TEXT("after_reload") },
		{ 8.5f, Aim, nullptr }, { 9.3f, Shot, TEXT("burst_before") }, { 9.3f, Yaw, nullptr }, { 9.4f, Auto, nullptr }, { 9.5f, Yaw, nullptr }, { 9.5f, Trig, nullptr },
		{ 9.8f, Shot, TEXT("burst_1") }, { 9.8f, Yaw, nullptr }, { 10.1f, Shot, TEXT("burst_2") }, { 10.5f, Shot, TEXT("burst_3") },
		{ 10.7f, Release, nullptr }, { 10.7f, Yaw, nullptr }, { 11.4f, Shot, TEXT("burst_after") }, { 11.4f, Yaw, nullptr },
	};
	constexpr int32 NumSeq = UE_ARRAY_COUNT(Seq);
	constexpr float PerGun = 12.f;
	constexpr int32 NumGuns = 2;
	const float Start = 3.f;
	while (TestStep < NumSeq * NumGuns)
	{
		const int32 GunIndex = TestStep / NumSeq;
		const FStep& S = Seq[TestStep % NumSeq];
		if (TestTime < Start + GunIndex * PerGun + S.T)
		{
			break;
		}
		++TestStep;
		const bool bAuto = (GunIndex == 1);
		switch (S.Do)
		{
		case Select:
			SelectGun(GunIndex);
			if (Gun)
			{
				for (int32 i = 0; i < 3 && Gun->GetFireMode() != EHoldoverFireMode::Semi; ++i)
				{
					Gun->CycleSelector();
				}
			}
			break;
		case Ready: HoldoverDebug::CVarForceAim->Set(0, ECVF_SetByConsole); HoldoverDebug::CVarDebugView->Set(0, ECVF_SetByConsole); break;
		case Aim: HoldoverDebug::CVarForceAim->Set(1, ECVF_SetByConsole); break;
		case RackP: OnRackPressed(); break;
		case RackR: OnRackReleased(); break;
		case Close: HoldoverDebug::CVarDebugView->Set(4, ECVF_SetByConsole); break;
		case Eyes: HoldoverDebug::CVarDebugView->Set(0, ECVF_SetByConsole); break;
		case CheckP: OnMagCheckPressed(); break;
		case CheckR: OnMagCheckReleased(); break;
		case Drop: OnMagDrop(); break;
		case Insert: OnMagInsert(); break;
		case Auto:
			if (bAuto && Gun)
			{
				for (int32 i = 0; i < 3 && Gun->GetFireMode() != EHoldoverFireMode::Auto; ++i)
				{
					Gun->CycleSelector();
				}
			}
			break;
		case Trig: if (bAuto) { OnTriggerPressed(); } break;
		case Release: if (bAuto) { OnTriggerReleased(); } break;
		case Yaw:
			if (bAuto && Gun)
			{
				UE_LOG(LogHoldover, Log, TEXT("[ActionTest] gun in eyes %s kick %.1f %.1f %.1f back %.1f rise %.1f control %s camera %s climb %.2f/%.2f mag %d"), *GunHold->GetRelativeLocation().ToCompactString(), Kick[0], Kick[1], Kick[2], Kick[3], Kick[4], *GetControlRotation().ToCompactString(),
					*Camera->GetComponentRotation().ToCompactString(), Climb[0], Climb[1], Gun->HasMagazine() ? Gun->GetMagazine().Rounds : -1);
			}
			break;
		case Shot:
			if (!bAuto && FString(S.Shot).StartsWith(TEXT("burst")))
			{
				break;
			}
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/test") / FString::Printf(TEXT("act_%s_%s.png"), Names[GunIndex], S.Shot), false, false);
			break;
		default: break;
		}
	}
	if (TestStep >= NumSeq * NumGuns && TestTime > Start + NumGuns * PerGun)
	{
		HoldoverDebug::CVarTestShots->Set(0, ECVF_SetByConsole);
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	}
}

void AHoldoverCharacter::UpdateFireTest()
{
	// holdover.TestShots 2: for each gun, chamber a round, aim, fire the three flash kinds with the flash held still
	// for the picture, a side view, then one shot in slow motion photographed as the case leaves, and the smoke after.
	// Pictures go to Saved/Screenshots/test/fire_<gun>_*.png.
	static const TCHAR* Names[] = { TEXT("m9"), TEXT("ak"), TEXT("1911"), TEXT("fal"), TEXT("ar") };
	enum EDo { None, Select, Rack, Unrack, Fire, Release, Freeze, Unfreeze, Slow, Normal, Side, Close, Eyes, Shot };
	struct FStep { float T; EDo Do; int32 Arg; const TCHAR* Shot; };
	static const FStep Seq[] = {
		{ 0.0f, Select, 0, nullptr },
		{ 1.0f, Rack, 0, nullptr },
		{ 1.7f, Unrack, 0, nullptr },
		{ 3.2f, Fire, 0, nullptr }, { 3.201f, Shot, 0, TEXT("flash_faint") }, { 3.25f, Release, 0, nullptr }, { 3.5f, Unfreeze, 0, nullptr },
		{ 4.2f, Fire, 1, nullptr }, { 4.201f, Shot, 0, TEXT("flash_normal") }, { 4.25f, Release, 0, nullptr }, { 4.5f, Unfreeze, 0, nullptr },
		{ 5.2f, Fire, 2, nullptr }, { 5.201f, Shot, 0, TEXT("flash_fireball") }, { 5.25f, Release, 0, nullptr }, { 5.5f, Unfreeze, 0, nullptr },
		{ 5.6f, Side, 0, nullptr },
		{ 6.2f, Fire, 1, nullptr }, { 6.201f, Shot, 0, TEXT("flash_side") }, { 6.25f, Release, 0, nullptr }, { 6.5f, Unfreeze, 0, nullptr },
		{ 6.6f, Eyes, 0, nullptr },
		{ 7.5f, Slow, 0, nullptr },
		{ 7.6f, Fire, -1, nullptr }, { 7.6f, Unfreeze, 0, nullptr },
		{ 7.616f, Shot, 0, TEXT("eject_1") }, { 7.632f, Shot, 0, TEXT("eject_2") }, { 7.65f, Shot, 0, TEXT("eject_3") },
		{ 7.675f, Shot, 0, TEXT("eject_4") }, { 7.71f, Shot, 0, TEXT("eject_5") }, { 7.78f, Shot, 0, TEXT("eject_6") },
		{ 7.9f, Shot, 0, TEXT("eject_7") }, { 7.95f, Release, 0, nullptr },
		{ 8.0f, Normal, 0, nullptr },
		{ 8.6f, Shot, 0, TEXT("smoke") },
		// The same in slow motion from close beside the gun, to see the slide and the case.
		{ 9.0f, Close, 0, nullptr }, { 9.3f, Slow, 0, nullptr },
		{ 9.4f, Fire, -1, nullptr }, { 9.4f, Unfreeze, 0, nullptr },
		{ 9.41f, Shot, 0, TEXT("close_1") }, { 9.425f, Shot, 0, TEXT("close_2") }, { 9.44f, Shot, 0, TEXT("close_3") },
		{ 9.46f, Shot, 0, TEXT("close_4") }, { 9.49f, Shot, 0, TEXT("close_5") }, { 9.54f, Shot, 0, TEXT("close_6") },
		{ 9.6f, Release, 0, nullptr }, { 9.7f, Normal, 0, nullptr }, { 9.8f, Eyes, 0, nullptr },
	};
	constexpr int32 NumSeq = UE_ARRAY_COUNT(Seq);
	constexpr float PerGun = 11.f;
	const int32 NumGuns = HoldoverDebug::TestShots() == 3 ? 2 : UE_ARRAY_COUNT(Names); // 3 = only the M9 and the AK (quicker)
	const float Start = 3.f;

	HoldoverDebug::CVarForceAim->Set(1, ECVF_SetByConsole);
	while (TestStep < NumSeq * NumGuns)
	{
		const int32 GunIndex = TestStep / NumSeq;
		const FStep& S = Seq[TestStep % NumSeq];
		if (TestTime < Start + GunIndex * PerGun + S.T)
		{
			break;
		}
		++TestStep;
		switch (S.Do)
		{
		case Select:
			SelectGun(GunIndex);
			HoldoverDebug::CVarDebugView->Set(0, ECVF_SetByConsole);
			if (Gun)
			{
				// Off safe (the AK starts on safe, its selector goes safe, auto, semi).
				for (int32 i = 0; i < 3 && Gun->GetFireMode() != EHoldoverFireMode::Semi; ++i)
				{
					Gun->CycleSelector();
				}
			}
			break;
		case Rack: OnRackPressed(); break;
		case Unrack: OnRackReleased(); break;
		case Fire:
			if (Controller)
			{
				// Same aim for every picture, whatever the mouse did.
				Controller->SetControlRotation(FRotator::ZeroRotator); // the test range runs down +X
			}
			HoldoverDebug::CVarFlashVariant->Set(S.Arg, ECVF_SetByConsole);
			HoldoverDebug::CVarFlashFreeze->Set(S.Arg >= 0 ? 1 : 0, ECVF_SetByConsole);
			OnTriggerPressed();
			break;
		case Release: OnTriggerReleased(); break;
		case Unfreeze:
			HoldoverDebug::CVarFlashFreeze->Set(0, ECVF_SetByConsole);
			HoldoverDebug::CVarFlashVariant->Set(-1, ECVF_SetByConsole);
			break;
		case Slow: UGameplayStatics::SetGlobalTimeDilation(this, 0.04f); break;
		case Normal: UGameplayStatics::SetGlobalTimeDilation(this, 1.f); break;
		case Side: HoldoverDebug::CVarDebugView->Set(1, ECVF_SetByConsole); break;
		case Close: HoldoverDebug::CVarDebugView->Set(4, ECVF_SetByConsole); break;
		case Eyes: HoldoverDebug::CVarDebugView->Set(0, ECVF_SetByConsole); break;
		case Shot:
		{
			const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots/test") / FString::Printf(TEXT("fire_%s_%s.png"), Names[GunIndex], S.Shot);
			FScreenshotRequest::RequestScreenshot(File, false, false);
			break;
		}
		default: break;
		}
	}
	if (TestStep >= NumSeq * NumGuns && TestTime > Start + NumGuns * PerGun)
	{
		HoldoverDebug::CVarTestShots->Set(0, ECVF_SetByConsole);
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	}
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
	const float K = FMath::Max(D->RecoilStiffness, 1.f);
	const float C = FMath::Max(D->RecoilDamping, 0.f);
	const float K2 = FMath::Max(D->RecoilRecoveryStiffness, 0.5f);
	const float C2 = FMath::Max(D->RecoilRecoveryDamping, 0.f);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / (1.f / 480.f)), 1, 32);
	const float Dt = DeltaSeconds / Steps;
	for (int32 s = 0; s < Steps; ++s)
	{
		for (int32 i = 0; i < NumKick; ++i)
		{
			KickVel[i] += (-K * Kick[i] - C * KickVel[i]) * Dt;
			Kick[i] += KickVel[i] * Dt;
		}
		for (int32 i = 0; i < 2; ++i)
		{
			ClimbVel[i] += (-K2 * Climb[i] - C2 * ClimbVel[i]) * Dt;
			Climb[i] += ClimbVel[i] * Dt;
		}
	}
	ShakeTime += DeltaSeconds;
}

void AHoldoverCharacter::ResetRecoil()
{
	for (int32 i = 0; i < NumKick; ++i)
	{
		Kick[i] = KickVel[i] = 0.f;
	}
	Climb[0] = Climb[1] = ClimbVel[0] = ClimbVel[1] = 0.f;
	ShakeAmp = 0.f;
	ShakeTime = 10.f;
	HeadKick = FRotator::ZeroRotator;
	HeadPush = 0.f;
}

void AHoldoverCharacter::ApplyHeadKick()
{
	HeadKick = FRotator::ZeroRotator;
	HeadPush = 0.f;
	if (Gun && Gun->GetData())
	{
		const UHoldoverGunData* D = Gun->GetData();
		// The eyes share part of the gun's turn (a rifle's stock pushes the cheek and shoulder; a pistol barely moves the head).
		const float F = D->RecoilHeadFollow;
		HeadKick = FRotator((Kick[0] + Climb[0]) * F, (Kick[1] + Climb[1]) * F, Kick[2] * F * 0.5f);
		HeadPush = FMath::Max(Kick[3], 0.f) * D->RecoilHeadPush;

		// Blast tremor: a fast, quickly dying wobble, different every shot.
		if (ShakeTime < 0.25f && ShakeAmp > 0.f)
		{
			const float Env = ShakeAmp * FMath::Exp(-ShakeTime / 0.045f);
			const float W = 2.f * PI * 24.f;
			HeadKick.Pitch += Env * FMath::Sin(W * ShakeTime + ShakePhase.X);
			HeadKick.Yaw += Env * 0.7f * FMath::Sin(W * 1.13f * ShakeTime + ShakePhase.Y);
			HeadKick.Roll += Env * 0.5f * FMath::Sin(W * 0.87f * ShakeTime + ShakePhase.Z);
		}
	}
	if (Controller)
	{
		const FQuat View = GetControlRotation().Quaternion();
		Camera->SetWorldRotation(View * HeadKick.Quaternion());
	}
	if (HeadPush > 0.f)
	{
		const FVector BackWorld = -Camera->GetForwardVector() * HeadPush;
		Camera->SetRelativeLocation(Camera->GetRelativeLocation() + GetActorTransform().InverseTransformVectorNoScale(BackWorld));
	}
}

void AHoldoverCharacter::UpdateHold(float DeltaSeconds)
{
	if (!Gun)
	{
		return;
	}
	const UHoldoverGunData* D = Gun->GetData();

	// Raise or lower the gun.
	const bool bWantAim = (bAimHeld || HoldoverDebug::ForceAim()) && !Animator->IsBusy();
	UpdateAimAlpha(DeltaSeconds, bWantAim, D->AimTime);
	const float A = AimAlpha;
	// The muzzle lines up a little before the gun arrives, as when a real shooter drives the front sight onto the target.
	const float RotA = 1.f - FMath::Pow(1.f - A, FMath::Max(D->AimRotationLead, 1.f));

	// Low ready.
	FTransform Ready(D->ReadyRotation, D->ReadyOffset);

	// Aimed: rear sight on the eye line at the eye relief, sight line straight ahead.
	FVector Rear, Front;
	Gun->GetSightsLocal(Rear, Front);
	const FVector SightDir = (Front - Rear).GetSafeNormal();
	const FQuat AimRot = SightDir.IsNearlyZero() ? FQuat::Identity : FQuat::FindBetweenNormals(SightDir, FVector::ForwardVector);
	float EyeRelief = D->AimEyeRelief;

	if (bHoldAtArmsDistance && ArmsMesh && ArmsMesh->GetSkinnedAsset())
	{
		// Hold the gun where the first-person arms' animation holds it, so the arms keep their natural bent
		// shape instead of stretching out to a gun held far away.
		const FVector GripLocal0 = Gun->GetSocketLocal(D->GripHandSocket);
		const FVector ReadyGrip = D->bIsPistol ? PistolReadyGrip : RifleReadyGrip;
		Ready.SetLocation(ReadyGrip - Ready.GetRotation().RotateVector(GripLocal0));
		const float AimGripX = D->bIsPistol ? PistolAimGripForward : RifleAimGripForward;
		EyeRelief = AimGripX - AimRot.RotateVector(GripLocal0 - Rear).X;
	}
	// The hand-made hold pose was posed with the gun at this distance from the eye: aim the gun there.
	const float HoldRelief = (BodyRig && Gun->UsesSkeletalMesh()) ? BodyRig->GetHoldEyeRelief(D->bIsPistol) : 0.f;
	if (HoldRelief > 0.f)
	{
		EyeRelief = HoldRelief;
	}
	const FTransform Aimed(AimRot, FVector(EyeRelief, 0.f, 0.f) - AimRot.RotateVector(Rear));

	FTransform Hold;
	// Straight line from ready to the eye, bowed out by the arc (zero at both ends).
	Hold.SetLocation(FMath::Lerp(Ready.GetLocation(), Aimed.GetLocation(), A) + D->AimArc * FMath::Sin(PI * A));
	Hold.SetRotation(FQuat::Slerp(Ready.GetRotation(), Aimed.GetRotation(), RotA).GetNormalized());

	// Recoil: the gun flips about the wrist (pistols) or the shoulder (rifles), rolls a little, slams back and
	// lifts. The share the head already took (ApplyHeadKick) is left out here, since the gun hangs from the eyes.
	const FVector GripLocal = Gun->GetSocketLocal(D->GripHandSocket);
	FVector Pivot = Hold.TransformPosition(GripLocal) - FVector(D->RecoilPivotBehindGrip, 0.f, 0.f);
	if (!D->bIsPistol && Gun->HasSocket(TEXT("Shoulder")))
	{
		Pivot = Hold.TransformPosition(Gun->GetSocketLocal(TEXT("Shoulder")));
	}
	const float GunShare = 1.f - FMath::Clamp(D->RecoilHeadFollow, 0.f, 1.f);
	const FQuat KickRot = FRotator((Kick[0] + Climb[0]) * GunShare, (Kick[1] + Climb[1]) * GunShare, Kick[2] * (1.f - 0.5f * D->RecoilHeadFollow)).Quaternion();
	const float BackInHands = FMath::Max(Kick[3], 0.f) * (1.f - D->RecoilHeadPush) + FMath::Min(Kick[3], 0.f);
	FTransform Kicked = Hold;
	Kicked.SetRotation(KickRot * Hold.GetRotation());
	Kicked.SetLocation(Pivot + KickRot.RotateVector(Hold.GetLocation() - Pivot) + FVector(-BackInHands, Kick[5], Kick[4]));

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
	EIC->BindAction(Inputs->Select1911, ETriggerEvent::Started, this, &AHoldoverCharacter::OnSelect1911);
	EIC->BindAction(Inputs->SelectFAL, ETriggerEvent::Started, this, &AHoldoverCharacter::OnSelectFAL);
	EIC->BindAction(Inputs->SelectAR, ETriggerEvent::Started, this, &AHoldoverCharacter::OnSelectAR);
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
void AHoldoverCharacter::OnSelect1911() { SelectGun(2); }
void AHoldoverCharacter::OnSelectFAL() { SelectGun(3); }
void AHoldoverCharacter::OnSelectAR() { SelectGun(4); }
void AHoldoverCharacter::OnResetRange() { ResetLoadout(); }
