#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HoldoverTypes.h"
#include "HoldoverGun.generated.h"

class UHoldoverGunData;
class UPoseableMeshComponent;
class USceneComponent;
class USoundBase;
class AHoldoverGun;
struct FHoldoverBoneMotion;

DECLARE_MULTICAST_DELEGATE_OneParam(FHoldoverGunSignal, AHoldoverGun*);

/**
 * One gun, run as a mechanical state machine. Nothing here is faked with counters:
 * the chamber, the magazine and the slide (or bolt) are tracked separately, and every
 * round and case moves through the gun the way it would in a real one.
 *
 * Driven by the character through the "input" functions below; the animation player
 * can also move the slide by hand (SetHandSlideTarget) and add bone offsets.
 */
UCLASS()
class HOLDOVER_API AHoldoverGun : public AActor
{
	GENERATED_BODY()

public:
	AHoldoverGun();

	/** Sets up the gun from its data asset: skeletal mesh if it has one, gray-box shapes otherwise. */
	void InitGun(UHoldoverGunData* InData);

	UHoldoverGunData* GetData() const { return Data; }

	virtual void Tick(float DeltaSeconds) override;

	// ---------- Inputs ----------

	void SetTriggerHeld(bool bHeld);

	/** Starts pulling the slide or charging handle back by hand. */
	void BeginRack();

	/** Lets go of the slide: it flies forward (or locks open if the slide stop is up). */
	void EndRack();

	/** Slide stop / bolt catch: releases a locked-open slide, or locks it if the slide is held back. */
	void PressSlideStop();

	/** Presses the magazine release: the magazine drops out and falls to the ground. */
	void DropMagazine();

	/** Pulls the magazine out into the hand without dropping it (for reloads with retention and mag checks). */
	bool RemoveMagazineToHand(FHoldoverMagazine& OutMag);

	/** Pushes a magazine into the well. Fails if one is already in. */
	bool InsertMagazine(const FHoldoverMagazine& Mag);

	/** Moves the safety or selector to its next position. */
	void CycleSelector();

	/** Returns the gun to a known state: given magazine inserted, chamber empty, slide forward. */
	void ResetState(const FHoldoverMagazine* Mag);

	// ---------- Animation hooks ----------

	/**
	 * While the hand is on the slide, the animation can drive the slide position directly
	 * (0 = forward, 1 = fully back). Pass a negative value to hand control back to BeginRack's ramp.
	 */
	void SetHandSlideTarget(float Target) { HandSlideTarget = Target; }

	/**
	 * Extra bone offset for this frame, in gun space (cm, rotation about the bone's pivot).
	 * With bReplace the gun's own motion for that bone is ignored this frame. Cleared after each tick.
	 */
	void AddBoneOffset(FName Bone, const FVector& Translation, const FQuat& Rotation, bool bReplace = false);

	/** Shows the magazine bone while a mag is in the hand (out of the gun), with its round count for the follower. */
	void SetHandMagazine(bool bShow, int32 Rounds) { bMagInHand = bShow; HandMagRounds = Rounds; }

	/** Rest transform of a bone or gray-box part in gun space. */
	bool GetBoneRest(FName Bone, FTransform& OutGunSpace) const;

	/** Puts a loose magazine into the world (dropped or let go). TopWorld is the top-centre of the mag. */
	void SpawnLooseMagazine(const FHoldoverMagazine& Mag, const FTransform& TopWorld, const FVector& Velocity);

	/** Bones whose motion the state machine owns (slide, barrel or bolt, hammer, slide stop). */
	bool IsMechanicalBone(FName Bone) const;

	// ---------- State ----------

	EHoldoverChamber GetChamber() const { return Chamber; }
	bool HasMagazine() const { return bHasMagazine; }
	const FHoldoverMagazine& GetMagazine() const { return Magazine; }
	bool IsSlideLocked() const { return bSlideLocked; }
	float GetSlidePosition() const { return SlidePos; }
	EHoldoverSlideMotion GetSlideMotion() const { return Motion; }
	EHoldoverFireMode GetFireMode() const { return FireMode; }
	EHoldoverMalfunction GetMalfunction() const { return Malfunction; }
	bool IsHammerCocked() const { return bHammerCocked; }
	bool IsInBattery() const { return Motion == EHoldoverSlideMotion::Idle && SlidePos <= KINDA_SMALL_NUMBER; }
	bool IsTriggerHeld() const { return bTriggerHeld; }
	bool IsRackHeld() const { return bRackHeld; }

	/** Plain-language summary for the debug overlay. */
	FString DescribeState() const;

	// ---------- Geometry ----------

	/** World transform of a socket or bone; falls back to gray-box marker points. */
	FTransform GetSocketWorldTransform(FName Socket) const;

	/** True when the gun shows its imported skeletal model (false for the gray-box stand-in). */
	bool UsesSkeletalMesh() const { return bUseSkeletal; }

	/** World transform of the gun model (the imported mesh's own frame), or the actor's for the gray box. */
	FTransform GetMeshWorldTransform() const;

	/** Socket position in the gun's own space (cm, X forward). */
	FVector GetSocketLocal(FName Socket) const;

	/** Sight positions in gun space (rest pose, slide forward). */
	void GetSightsLocal(FVector& OutRear, FVector& OutFront) const;

	// ---------- Events ----------

	FHoldoverGunSignal OnFired;
	FHoldoverGunSignal OnDryFire;
	FHoldoverGunSignal OnSlideLocked;
	FHoldoverGunSignal OnMagazineOut;
	FHoldoverGunSignal OnMagazineSeated;
	FHoldoverGunSignal OnMalfunction;
	FHoldoverGunSignal OnSelectorChanged;

protected:
	virtual void BeginPlay() override;

private:
	// State machine steps
	void TryPullTrigger();
	void Fire();
	void UpdateSlide(float Dt);
	void OnExtractionPoint(bool bByRecoil);
	void OnReachedRear(bool bByRecoil);
	void ReleaseFromRear();
	void StartReturn(float Target);
	void StartReturnToBattery();
	void OnSlideArrived();
	void LaunchBullet();
	void EjectPiece(bool bLiveRound, float SpeedScale);
	void PlayGunSound(USoundBase* Sound) const;
	bool ShouldLockOpen() const;

	// Visuals
	void BuildSkeletalVisual();
	void BuildFallbackVisual();
	void UpdateVisuals(float Dt);
	void AddMotion(const FHoldoverBoneMotion& M, float Alpha);
	void AddDelta(FName Bone, const FVector& T, const FQuat& R, float Scale = 1.f);
	void ApplyBoneDeltas();
	USceneComponent* AddFallbackPart(FName Bone, USceneComponent* Parent, const FVector& Pivot);

	UPROPERTY(VisibleAnywhere, Category = "Gun")
	TObjectPtr<USceneComponent> Root;

	/** The gun the player sees (First Person rendering). */
	UPROPERTY(VisibleAnywhere, Category = "Gun")
	TObjectPtr<UPoseableMeshComponent> Mesh;

	/** Invisible copy that casts the gun's shadow into the world. */
	UPROPERTY(VisibleAnywhere, Category = "Gun")
	TObjectPtr<UPoseableMeshComponent> ShadowMesh;

	UPROPERTY(Transient)
	TObjectPtr<UHoldoverGunData> Data;

	/** Gray-box parts by bone name (pivot scene components). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USceneComponent>> FallbackBones;

	/** Gray-box socket markers by socket name. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USceneComponent>> FallbackSockets;

	TMap<FName, FTransform> FallbackRest;

	struct FBoneDelta
	{
		FVector T = FVector::ZeroVector;
		FQuat R = FQuat::Identity;
		float Scale = 1.f;
	};
	TMap<FName, FBoneDelta> FrameDeltas;
	TMap<FName, FBoneDelta> ExternalDeltas;
	TSet<FName> ExternalReplace;

	/** Rest pose of the skeletal mesh: local and component-space, by bone index. */
	TArray<FTransform> RefLocal;
	TArray<FTransform> RefComponent;

	bool bUseSkeletal = false;

	// ---------- Mechanical state ----------

	EHoldoverChamber Chamber = EHoldoverChamber::Empty;
	bool bHasMagazine = false;
	FHoldoverMagazine Magazine;
	bool bMagSeating = false;
	float MagSeatTimer = 0.f;
	bool bMagInHand = false;
	int32 HandMagRounds = 0;

	float SlidePos = 0.f;
	EHoldoverSlideMotion Motion = EHoldoverSlideMotion::Idle;
	float ReturnTarget = 0.f;
	bool bSlideLocked = false;
	bool bLockRequested = false;
	bool bExtractedThisStroke = false;
	bool bFeeding = false;
	bool bExtractionFailed = false;
	bool bJammedRound = false;
	float HandSlideTarget = -1.f;

	bool bHammerCocked = false;
	bool bTriggerHeld = false;
	bool bTriggerReset = true;
	bool bRackHeld = false;
	float LastShotTime = -100.f;

	EHoldoverFireMode FireMode = EHoldoverFireMode::Semi;
	EHoldoverMalfunction Malfunction = EHoldoverMalfunction::None;

	// Smoothed visual values
	float TriggerVisual = 0.f;
	float HammerVisual = 0.f;
	float SelectorVisual = 0.f;
	float MagReleaseVisual = 0.f;

	/** Slide position where a stovepipe or double feed stops it. */
	static constexpr float JamFraction = 0.28f;
};
