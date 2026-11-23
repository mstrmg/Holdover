#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HoldoverBodyRig.generated.h"

class UPoseableMeshComponent;
class USkeletalMeshComponent;
class UHoldoverGunAnimator;
class UCameraComponent;

/** Curl angles (degrees) for one finger's three joints. */
USTRUCT(BlueprintType)
struct FHoldoverFingerCurl
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers") float J1 = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers") float J2 = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers") float J3 = 10.f;

	FHoldoverFingerCurl() = default;
	FHoldoverFingerCurl(float A, float B, float C) : J1(A), J2(B), J3(C) {}
};

/** A whole-hand finger pose: thumb, index, middle, ring, pinky. */
USTRUCT(BlueprintType)
struct FHoldoverHandPose
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers")
	TArray<FHoldoverFingerCurl> Fingers;

	/** Which fingers this pose sets (others are left to the other poses). Bit 0 = thumb ... bit 4 = pinky. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fingers")
	int32 FingerMask = 0x1F;
};

/**
 * Poses the player's full body every frame: copies the locomotion pose from the (hidden)
 * animated body, then bends both arms with two-bone IK so the hands land on the gun's
 * hand targets, curls the fingers, and hides the head from the player's own eyes.
 * Works on two copies: the one you see (First Person rendering) and the one that casts
 * the shadow (with a head).
 */
UCLASS(ClassGroup = (Holdover), meta = (BlueprintSpawnableComponent))
class HOLDOVER_API UHoldoverBodyRig : public UActorComponent
{
	GENERATED_BODY()

public:
	UHoldoverBodyRig();

	void Setup(USkeletalMeshComponent* InDriver, UPoseableMeshComponent* InVisible, UPoseableMeshComponent* InShadow,
		UHoldoverGunAnimator* InAnimator, UCameraComponent* InCamera);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * First-person arms ("viewmodel"): InArmsDriver plays a ready-made idle hold animation (hidden),
	 * InArms is the copy you see. Each frame the arms are moved as one piece so their grip bone sits on
	 * the gun's GripHand socket; the left hand can be bent onto the gun with IK.
	 */
	void SetupArms(USkeletalMeshComponent* InArmsDriver, UPoseableMeshComponent* InArms);

	/** Arms follow the gun (false = arms keep the locomotion pose, e.g. no gun in hand). Used on the shadow body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	bool bArmsOnGun = true;

	/** Hide the arms of the full body you see (the first-person arms replace them). The shadow body keeps its arms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	bool bHideBodyArms = true;

	/** Bones scaled to nothing on the full body you see when Hide Body Arms is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	TArray<FName> BodyArmBones = { TEXT("spine_01") };

	/** Bones scaled to nothing on the first-person arms (head, legs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	TArray<FName> ArmsHiddenBones = { TEXT("head"), TEXT("thigh_l"), TEXT("thigh_r") };

	/** Bone (or socket) on the arms that is placed on the gun's grip. weapon_r is where the idle animations hold the gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FName ArmsGripBone = TEXT("hand_r");

	/** Where the idle animation holds the gun (its weapon_r bone) relative to the grip bone, pistol. Measured from MF_Pistol_Idle_ADS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FTransform PistolWeaponInHand = FTransform(FRotator(-5.781f, 176.373f, 3.239f), FVector(-7.312f, 3.645f, -1.429f));

	/** Same for the rifle, measured from MF_Rifle_Idle_ADS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FTransform RifleWeaponInHand = FTransform(FRotator(-8.935f, 171.404f, 14.858f), FVector(-7.218f, 3.418f, -0.947f));

	/** Fine adjustment of the grip, in the gun's frame (X along the barrel, Z up), pistol. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FTransform PistolArmsGripOffset = FTransform(FQuat::Identity, FVector(-1.f, 0.f, -1.f));

	/** Fine adjustment of the grip, in the gun's frame, rifle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FTransform RifleArmsGripOffset = FTransform::Identity;

	/**
	 * Where the arms' head bone sits relative to the eyes (cm, X forward, Y right, Z up). The arms' shoulders hang
	 * from here and both hands are bent onto the gun; if the gun is out of reach the arms slide forward just enough.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FVector PistolArmsHeadInCamera = FVector(-14.f, 0.f, -18.f);

	/** Same for the rifle (lower: the cheek rests on the stock, the shoulders sit well below the eye). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FVector RifleArmsHeadInCamera = FVector(-16.f, 4.f, -22.f);

	/** Place the arms so the animation's hand sits exactly on the grip (keeps the animation's arm shape) instead of hanging them from the anchor above. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	bool bPistolArmsFollowGrip = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	bool bRifleArmsFollowGrip = false;

	/** Sideways pull of the support elbow on the first-person arms (0 = straight down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	float ArmsLeftElbowOutward = 0.1f;

	/** How much the first-person arms' body tilts with the view up and down (0 = stays upright like a real torso, 1 = tilts fully). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms", meta = (ClampMin = "0", ClampMax = "1"))
	float ArmsBodyPitchFollow = 0.f;

	/** Arms reach at most this fraction of their full length (1 = arm locked straight). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms", meta = (ClampMin = "0.8", ClampMax = "1"))
	float ArmsMaxReach = 0.97f;

	/** Bend the left arm onto the gun's support-hand target with IK (pistol / rifle). Always on during gun actions.
	 *  When off, the left hand keeps the place on the gun the idle animation gives it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	bool bPistolLeftHandIK = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	bool bRifleLeftHandIK = true;

	/** Rifle without left-hand IK: slide the animation's support hand along the barrel to this gun's SupportHand socket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	bool bLeftHandSlideToSupport = true;

	/** Also curl the left fingers from the gun timelines when the left hand IK is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	bool bLeftHandCurlFromTimeline = true;

	/** Turn the left hand's palm the other way when bending it onto a hand target (fixes a palm facing away from the gun). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	bool bFlipLeftPalm = false;

	/** Nudge of the support hand from the gun's hand target, in the gun's frame (X along the barrel, Z up), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FVector PistolLeftHandOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arms")
	FVector RifleLeftHandOffset = FVector::ZeroVector;

	/** Bone hidden on the body you see (hides the head and everything above it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FName HiddenBone = TEXT("neck_01");

	/** Wrist sits this far behind the palm centre along the fingers (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float PalmToWrist = 4.5f;

	/** ...and this far behind the palm surface (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float PalmDepth = 1.5f;

	/** Elbows point down and slightly outward: sideways weight of the elbow hint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float ElbowOutward = 0.45f;

	/** Axis in each finger bone's own space that curls the finger (flip the sign if fingers bend backward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FVector FingerCurlAxisRight = FVector(0.f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FVector FingerCurlAxisLeft = FVector(0.f, 0.f, -1.f);

	/** Named finger poses used by the gun timelines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	TMap<FName, FHoldoverHandPose> HandPoses;

private:
	struct FArmBones
	{
		int32 Upper = INDEX_NONE;
		int32 Lower = INDEX_NONE;
		int32 Hand = INDEX_NONE;
		TArray<int32> Fingers; // 15 entries: thumb, index, middle, ring, pinky x 3 joints
		FVector LocalFingers = FVector::ForwardVector; // hand bone space
		FVector LocalPalm = FVector::UpVector;          // hand bone space, out of the palm
		bool bValid = false;
	};

	void CacheBones();
	void PoseCopy(UPoseableMeshComponent* Mesh, bool bHideHead);
	void SolveArm(UPoseableMeshComponent* Mesh, const FArmBones& Arm, const FTransform& TargetWorld, bool bLeft);
	/** Two-bone IK putting the hand bone at Wrist with rotation HandRot (world). */
	void SolveArmTo(UPoseableMeshComponent* Mesh, const FArmBones& Arm, const FVector& Wrist, const FQuat& HandRot, bool bLeft,
		const FTransform& DebugTarget);
	void CurlFingers(UPoseableMeshComponent* Mesh, const FArmBones& Arm, const TMap<FName, float>& Weights, bool bLeft);
	void ComputeComponentSpace(UPoseableMeshComponent* Mesh, TArray<FTransform>& Out) const;
	void InitDefaultPoses();
	void UpdateArms(const FTransform& CamTM);
	void ScaleBonesToNothing(UPoseableMeshComponent* Mesh, const TArray<FName>& Bones) const;

	bool bElbowFromHand = false; // set while solving the first-person arms
	TWeakObjectPtr<USkeletalMeshComponent> ArmsDriver;
	TWeakObjectPtr<UPoseableMeshComponent> Arms;
	TWeakObjectPtr<USkeletalMeshComponent> Driver;
	TWeakObjectPtr<UPoseableMeshComponent> Visible;
	TWeakObjectPtr<UPoseableMeshComponent> Shadow;
	TWeakObjectPtr<UHoldoverGunAnimator> Animator;
	TWeakObjectPtr<UCameraComponent> Camera;

	FArmBones LeftArm;
	FArmBones RightArm;
	int32 HiddenIndex = INDEX_NONE;
	const void* CachedFor = nullptr;
	TArray<int32> Parents;
};
