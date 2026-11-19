#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/HoldoverTypes.h"
#include "HoldoverGunData.generated.h"

class USkeletalMesh;
class UStaticMesh;
class USoundBase;

/** How the slide or bolt is operated, which decides the hold-open and charging behaviour. */
UENUM(BlueprintType)
enum class EHoldoverActionType : uint8
{
	PistolSlide,   // M9, 1911: slide with slide stop, locks open on empty mag
	RotatingBolt,  // AK: no hold-open, charging handle on the carrier
	BoltCarrier    // AR, FAL: bolt catch locks open on empty mag
};

/** A bone motion in gun component space: move along an axis and/or turn about an axis through the bone's own pivot. */
USTRUCT(BlueprintType)
struct FHoldoverBoneMotion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone")
	FName Bone = NAME_None;

	/** Full movement in centimetres (gun space: X forward, Y right, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone")
	FVector Translation = FVector::ZeroVector;

	/** Axis to rotate about, in gun space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone")
	FVector RotationAxis = FVector(0.f, 1.f, 0.f);

	/** Full rotation in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone")
	float RotationDegrees = 0.f;
};

/**
 * Everything that makes one gun that gun. One data asset per gun (Content/Weapons/<Gun>/DA_<Gun>).
 * Defaults are an M9-style 9 mm pistol, so a gun with no asset still works.
 * Distances are in centimetres unless the name says otherwise.
 */
UCLASS(BlueprintType)
class HOLDOVER_API UHoldoverGunData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UHoldoverGunData();

	// ---------- Identity ----------

	/** Matches the animation timeline file Holdover/Data/GunAnims/<GunId>.json. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName GunId = TEXT("Pistol_M9");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	bool bIsPistol = true;

	// ---------- Art (all optional: missing art falls back to gray-box shapes) ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	TObjectPtr<USkeletalMesh> GunMesh;

	/** Fix-up if the imported mesh does not point its muzzle along +X. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	FRotator MeshImportRotation = FRotator::ZeroRotator;

	/** Loose magazine (dropped or in the hand). Origin at the top centre, mag axis along local Z. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	TObjectPtr<UStaticMesh> MagazineMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	TObjectPtr<UStaticMesh> CasingMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	TObjectPtr<UStaticMesh> RoundMesh;

	/** Size of the gray-box casing in cm (diameter, diameter, length) when there is no casing mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	FVector FallbackCasingSize = FVector(0.99f, 0.99f, 1.92f);

	/** Overall length of a loaded round in cm, for the gray-box live round. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	float FallbackRoundLength = 2.97f;

	/** Gray-box magazine size in cm (X, Y, Z) when there is no magazine mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	FVector FallbackMagazineSize = FVector(2.8f, 2.2f, 12.5f);

	// ---------- Sockets (bone names in the gun skeleton) ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	FName MuzzleSocket = TEXT("Muzzle");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	FName EjectionPortSocket = TEXT("EjectionPort");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	FName SightRearSocket = TEXT("SightRear");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	FName SightFrontSocket = TEXT("SightFront");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	FName MagWellSocket = TEXT("MagWell");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	FName GripHandSocket = TEXT("GripHand");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	FName SupportHandSocket = TEXT("SupportHand");

	// ---------- Action ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	EHoldoverActionType ActionType = EHoldoverActionType::PistolSlide;

	/** Slide or bolt carrier bone and its full rearward travel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FHoldoverBoneMotion SlideMotion;

	/** Where the slide stops when held open, as a fraction of full travel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.5", ClampMax = "1"))
	float LockedBackFraction = 0.95f;

	/** Fraction of travel at which the case is pulled clear and ejected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.3", ClampMax = "1"))
	float EjectFraction = 0.85f;

	/** Locks open after the last round is fired (or racked on an empty mag). False for the AK. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bLastRoundHoldOpen = true;

	/** Has a slide stop or bolt catch the player can use to lock open or release. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bHasSlideStop = true;

	/** Seconds for a recoil-driven cycle, rear and back (real guns: 0.04 to 0.1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.01"))
	float CycleTime = 0.07f;

	/** Seconds for the hand to pull the slide fully back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.03"))
	float HandPullTime = 0.18f;

	/** Seconds for the spring to slam the slide home after release. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.01"))
	float SpringReturnTime = 0.05f;

	/** Unlocking during the first part of slide travel: the pistol barrel rides back and drops, the AK bolt turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FHoldoverBoneMotion LockingMotion;

	/** Fraction of slide travel over which the locking motion happens. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.01", ClampMax = "1"))
	float LockingTravelFraction = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FHoldoverBoneMotion HammerCockMotion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FHoldoverBoneMotion TriggerPullMotion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FHoldoverBoneMotion SlideStopUpMotion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FHoldoverBoneMotion MagReleaseMotion;

	/** Safety, decocker or selector lever. Each fire mode below is a fraction of this full motion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FHoldoverBoneMotion SafetyMotion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	float SelectorSafeFraction = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	float SelectorSemiFraction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	float SelectorAutoFraction = 0.f;

	/** Has a manual safety. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bHasSafety = true;

	/** Putting the safety on also drops the hammer (M9 decocker). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bSafetyDecocks = true;

	/** Safety blocks the slide or charging handle from moving (AK selector). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bSafetyBlocksAction = false;

	/** Double action: a trigger pull also cocks the hammer, so a dropped hammer can still fire. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bDoubleAction = true;

	/** Can fire full auto (selector has an AUTO position). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bHasFullAuto = false;

	/** Cyclic rate in full auto, rounds per minute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "60"))
	float CyclicRateRPM = 600.f;

	// ---------- Magazine ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "1"))
	int32 MagazineCapacity = 15;

	/** Full magazines in the belt pouches at the start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0"))
	int32 StartingMagazines = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine")
	FName MagazineBone = TEXT("Magazine");

	/** Direction the magazine slides out, in gun space (M9 grip is raked 15 degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine")
	FVector MagazineOutDirection = FVector(-0.259f, 0.f, -0.966f);

	/** Distance the magazine travels to clear the gun. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine")
	float MagazineOutTravel = 12.5f;

	/** AK-style rock-in: rotation from seated to unhooked, about the front lug (0 for straight insertion). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine")
	FHoldoverBoneMotion MagazineRockMotion;

	/** Seconds from the mag touching the well to seated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0.05"))
	float MagazineSeatTime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine")
	FName MagFollowerBone = TEXT("MagFollower");

	/** Follower drop per round, along the magazine (negative = down into the mag). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine")
	FVector FollowerOffsetPerRound = FVector(-0.119f, 0.f, -0.444f);

	// ---------- Ballistics ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics")
	FHoldoverBulletSpec Bullet;

	// ---------- Ejection ----------

	/** Ejection speed of the case, cm/s, in gun space (Y = right). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ejection")
	FVector CaseEjectVelocity = FVector(-60.f, 350.f, 250.f);

	/** Random spread added to each component of the ejection speed, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ejection")
	float CaseEjectRandom = 60.f;

	/** Tumble of the case, degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ejection")
	float CaseSpinDegPerSec = 1800.f;

	/** A live round pulled out by hand leaves much slower. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ejection", meta = (ClampMin = "0", ClampMax = "1"))
	float LiveRoundEjectScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ejection")
	float CaseMassGrams = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ejection")
	float RoundMassGrams = 12.0f;

	// ---------- Handling ----------

	/** Gun position relative to the eye at low ready (cm, X forward, Y right, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling")
	FVector ReadyOffset = FVector(38.f, 9.f, -24.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling")
	FRotator ReadyRotation = FRotator(-12.f, -4.f, 0.f);

	/** Distance from the eye to the rear sight when aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = "5"))
	float AimEyeRelief = 58.f;

	/** Gun position for inspection (mag check, press check, malfunction clearing): close and tilted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling")
	FVector InspectOffset = FVector(32.f, 4.f, -14.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling")
	FRotator InspectRotation = FRotator(5.f, -10.f, -35.f);

	/** Seconds to raise the gun from ready to the eye. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = "0.05"))
	float AimTime = 0.25f;

	/** While coming up to the eye the gun also travels this far out of the straight line, at mid-way (cm, X forward, Z up). Gives the push-out arc of a real presentation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling")
	FVector AimArc = FVector(3.f, 0.f, -1.f);

	/** How far ahead the gun's angle is of its position while aiming in (1 = together, higher = the muzzle lines up first). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = "1", ClampMax = "3"))
	float AimRotationLead = 1.5f;

	/** How heavy the gun feels when you turn or move: 1 = pistol, higher lags more and settles slower. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling", meta = (ClampMin = "0.2", ClampMax = "4"))
	float SwayWeight = 1.f;

	// ---------- Recoil (procedural; M2 adds per-gun patterns) ----------

	/** Upward muzzle flip, degrees per second added at the shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilPitchImpulse = 260.f;

	/** Random left-right, degrees per second (plus or minus). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilYawImpulse = 45.f;

	/** Rearward push, cm per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilBackImpulse = 90.f;

	/** Upward push of the hands, cm per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilRiseImpulse = 25.f;

	/** How hard the hands pull the gun back on target (spring stiffness). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilStiffness = 260.f;

	/** How quickly the movement dies out (damping). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	float RecoilDamping = 24.f;

	/** Fraction of the muzzle flip the shooter does not fully recover (drifts the aim a little). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0", ClampMax = "0.5"))
	float RecoilResidual = 0.04f;

	// ---------- Reliability (chances per shot, 0 to 1) ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reliability", meta = (ClampMin = "0", ClampMax = "1"))
	float StovepipeChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reliability", meta = (ClampMin = "0", ClampMax = "1"))
	float DoubleFeedChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reliability", meta = (ClampMin = "0", ClampMax = "1"))
	float DudChance = 0.0f;

	// ---------- Sounds (optional) ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> DryFireSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> SlideBackSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> SlideForwardSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> SlideLockSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> MagOutSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> MagInSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> SafetySound;

	float GetSelectorFraction(EHoldoverFireMode Mode) const
	{
		switch (Mode)
		{
		case EHoldoverFireMode::Safe: return SelectorSafeFraction;
		case EHoldoverFireMode::Auto: return SelectorAutoFraction;
		default: return SelectorSemiFraction;
		}
	}

	/** Time between full-auto shots in seconds. */
	float GetAutoInterval() const { return 60.f / FMath::Max(CyclicRateRPM, 60.f); }

	/** Built-in presets, used when no data asset is assigned. */
	static void ApplyPresetM9(UHoldoverGunData* Data);
	static void ApplyPresetAK(UHoldoverGunData* Data);

	/** Fills empty model slots from the imported Blender exports in /Game/Guns/<GunId>/, if they are there. */
	static void LoadImportedArt(UHoldoverGunData* Data);
};
