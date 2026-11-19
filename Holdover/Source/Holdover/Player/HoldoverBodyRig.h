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

	/** Arms follow the gun (false = arms keep the locomotion pose, e.g. no gun in hand). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	bool bArmsOnGun = true;

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
	FVector FingerCurlAxisLeft = FVector(0.f, 0.f, 1.f);

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
	void CurlFingers(UPoseableMeshComponent* Mesh, const FArmBones& Arm, const TMap<FName, float>& Weights, bool bLeft);
	void ComputeComponentSpace(UPoseableMeshComponent* Mesh, TArray<FTransform>& Out) const;
	void InitDefaultPoses();

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
