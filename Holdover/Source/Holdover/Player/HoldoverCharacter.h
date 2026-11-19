#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/HoldoverTypes.h"
#include "HoldoverCharacter.generated.h"

class UCameraComponent;
class UPoseableMeshComponent;
class USceneComponent;
class UInputMappingContext;
class UHoldoverInputSet;
class UHoldoverGunAnimator;
class UHoldoverBodyRig;
class UHoldoverGunData;
class AHoldoverGun;
class UAnimInstance;
class USkeletalMesh;
struct FInputActionValue;

/**
 * The player: seen through their own eyes, full body, holding one gun with both hands.
 * Every gun operation is its own input (full manual controls). See docs/CONTROLS.md.
 */
UCLASS(Config = Game)
class HOLDOVER_API AHoldoverCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHoldoverCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	AHoldoverGun* GetGun() const { return Gun; }

	/** Switches to the gun in the given slot (0 = pistol, 1 = rifle). */
	void SelectGun(int32 Slot);

	/** Refills the pouches, puts a full mag in the gun with an empty chamber, clears the range. */
	void ResetLoadout();

	// ---------- Eye view ----------

	/** Horizontal field of view, degrees. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Eye View", meta = (ClampMin = "60", ClampMax = "120"))
	float FieldOfView = 90.f;

	/** Eye height above the floor, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye View")
	float EyeHeight = 165.f;

	/** Eyes sit this far in front of the body's centre line, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye View")
	float EyeForward = 12.f;

	/** First Person rendering scale for the hands and gun (smaller keeps them out of walls; 1 = off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye View", meta = (ClampMin = "0.1", ClampMax = "1"))
	float FirstPersonScale = 0.7f;

	/** Up-down head motion while walking, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye View")
	float WalkBobHeight = 0.35f;

	/** Mouse sensitivity multiplier. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Eye View", meta = (ClampMin = "0.05", ClampMax = "5"))
	float MouseSensitivity = 0.5f;

	// ---------- Gun weight: the gun trails a little behind where you look and move ----------

	/** Overall strength of the gun lag (0 = gun locked to the view, 1 = default, 2 = heavy). Meant for the settings menu. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Gun Sway", meta = (ClampMin = "0", ClampMax = "2"))
	float SwayStrength = 1.f;

	/** How hard the arms pull the gun back onto the look direction. Lower = more lag. Divided by the gun's Sway Weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun Sway", meta = (ClampMin = "50", ClampMax = "5000"))
	float SwayStiffness = 600.f;

	/** 1 = the gun settles without overshooting; lower = it swings past and back a little. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun Sway", meta = (ClampMin = "0.2", ClampMax = "2"))
	float SwayDampingRatio = 0.75f;

	/** Largest angle the gun can trail behind the view, degrees (reached gently, never a hard stop). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun Sway", meta = (ClampMin = "0", ClampMax = "20"))
	float SwayMaxDegrees = 6.f;

	/** The gun cants this much (degrees of roll per degree of sideways lag) when you turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun Sway")
	float SwayRollPerYaw = 0.35f;

	/** The gun turns about a point this far behind the shooting hand (the wrist and forearm), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun Sway")
	float SwayPivotBehindGrip = 12.f;

	/** Lag while aiming, as a fraction of the lag at ready (arms locked out, gun braced). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun Sway", meta = (ClampMin = "0", ClampMax = "1"))
	float SwayWhileAiming = 0.6f;

	/** Starting, stopping and strafing shift the gun: cm/s of hand speed per cm/s of body speed change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun Sway")
	float MoveInertia = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun Sway", meta = (ClampMin = "10", ClampMax = "2000"))
	float MoveSwayStiffness = 140.f;

	/** Largest shift from moving, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun Sway", meta = (ClampMin = "0", ClampMax = "10"))
	float MoveSwayMaxCm = 2.5f;

	// ---------- Movement ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float JogSpeed = 380.f;

	/** Moving while aiming is slower. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float AimWalkSpeed = 90.f;

	// ---------- Body (optional: the Manny mannequin from the Third Person content pack) ----------

	UPROPERTY(Config, EditAnywhere, Category = "Body")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Body")
	TSoftClassPtr<UAnimInstance> BodyAnimClass;

	// ---------- Guns ----------

	/** Gun data per slot. An empty slot uses the built-in preset (slot 0 M9-style pistol, slot 1 AK-style rifle). */
	UPROPERTY(Config, EditAnywhere, Category = "Guns")
	TArray<TSoftObjectPtr<UHoldoverGunData>> GunSlots;

	/** Pick-up reach, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guns")
	float ReachDistance = 220.f;

protected:
	virtual void BeginPlay() override;

private:
	// Input handlers
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJogStart();
	void OnJogEnd();
	void OnTriggerPressed();
	void OnTriggerReleased();
	void OnAimPressed();
	void OnAimReleased();
	void OnRackPressed();
	void OnRackReleased();
	void OnSlideStop();
	void OnMagDrop();
	void OnMagStow();
	void OnMagInsert();
	void OnMagCheckPressed();
	void OnMagCheckReleased();
	void OnPressCheckPressed();
	void OnPressCheckReleased();
	void OnSafety();
	void OnInteract();
	void OnSelectPistol();
	void OnSelectRifle();
	void OnResetRange();

	// Gun and animation glue
	void SpawnGun(int32 Slot);
	void LoadGun(AHoldoverGun* InGun);
	UHoldoverGunData* LoadGunData(int32 Slot);
	void OnGunFired(AHoldoverGun* InGun);
	void OnGunDryFire(AHoldoverGun* InGun);
	void OnAnimEvent(FName EventName);
	bool PlayAction(FName Action, bool bHoldable, FName HoldKey);
	bool CanStartAction() const;
	TArray<FHoldoverMagazine>& Pouch();
	bool TakeMagFromPouch(FHoldoverMagazine& Out);
	void ShowCheck(const FString& Text, float Seconds);

	// Per-frame
	void UpdateHold(float DeltaSeconds);
	void UpdateRecoil(float DeltaSeconds);
	void UpdateSway(float DeltaSeconds);
	void UpdateAimAlpha(float DeltaSeconds, bool bWantAim, float AimTime);
	void SetupBody();

	// Test tools (holdover.DebugView, holdover.TestShots)
	void UpdateDebugView();
	void UpdateTestShots(float DeltaSeconds);
	void LogArmDiagnostics(const FString& Label) const;

	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> DebugCam;
	int32 AppliedDebugView = 0;
	float TestTime = 0.f;
	int32 TestStep = 0;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	/** The gun hangs here; moved every frame relative to the eyes. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> GunHold;

	/** The body the player sees (head hidden), posed by the body rig. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPoseableMeshComponent> BodyVisible;

	/** Full body (with head) that only casts the shadow. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPoseableMeshComponent> BodyShadow;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHoldoverGunAnimator> Animator;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHoldoverBodyRig> BodyRig;

	UPROPERTY(Transient)
	TObjectPtr<UHoldoverInputSet> Inputs;

	UPROPERTY(Transient)
	TObjectPtr<AHoldoverGun> Gun;

	/** One gun per slot; guns not in the hands keep their state. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AHoldoverGun>> Guns;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHoldoverGunData>> LoadedGunData;

	int32 CurrentSlot = 0;
	int32 NextMagId = 1;

	/** Magazines in the belt pouches, per gun. */
	TMap<FName, TArray<FHoldoverMagazine>> Pouches;

	/** Loose live rounds picked up, per gun. */
	TMap<FName, int32> LooseRounds;

	// Hold state
	bool bAimHeld = false;
	bool bJog = false;
	float AimAlpha = 0.f;
	float AimAlphaVel = 0.f;
	FName HoldKey;

	// Recoil spring state: pitch (deg), yaw (deg), back (cm), rise (cm), and their speeds.
	FVector4 Recoil = FVector4(0, 0, 0, 0);
	FVector4 RecoilVel = FVector4(0, 0, 0, 0);
	float RecoilDrift = 0.f;
	float WalkPhase = 0.f;

	// Gun lag state. SwayAngle is the gun's angle minus the view's (pitch, yaw, degrees); SwayGunVel is the gun's
	// own turn speed (deg/s). MoveSway is the shift from body motion (cm, view space).
	FVector2D SwayAngle = FVector2D::ZeroVector;
	FVector2D SwayGunVel = FVector2D::ZeroVector;
	FRotator PrevViewRotation = FRotator::ZeroRotator;
	bool bHavePrevView = false;
	FVector MoveSway = FVector::ZeroVector;
	FVector MoveSwayVel = FVector::ZeroVector;
	FVector PrevViewVelocity = FVector::ZeroVector;

	FTransform BaseGunInCamera = FTransform::Identity;
};
