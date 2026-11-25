#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/HoldoverGunAnimSet.h"
#include "Core/HoldoverTypes.h"
#include "HoldoverGunAnimator.generated.h"

class AHoldoverGun;

DECLARE_MULTICAST_DELEGATE_OneParam(FHoldoverAnimEventSignal, FName /*EventName*/);

/** Where the player's head and feet are this frame, for converting hand targets. */
struct FHoldoverAnimFrame
{
	/** Gun transform relative to the camera before any action offset (ready, aim and recoil). */
	FTransform BaseGunInCamera = FTransform::Identity;
	FTransform CameraWorld = FTransform::Identity;
	/** Pawn root between the feet, facing the body's forward. */
	FTransform FeetWorld = FTransform::Identity;
};

/**
 * Plays the gun timelines (docs/animation/gun_anim_format.md): moves the whole gun, the
 * cosmetic gun bones and carried magazines, works the slide by hand through the gun's
 * state machine, produces hand targets and finger weights for the arms, and fires the
 * timeline's events for the character to act on.
 */
UCLASS(ClassGroup = (Holdover), meta = (BlueprintSpawnableComponent))
class HOLDOVER_API UHoldoverGunAnimator : public UActorComponent
{
	GENERATED_BODY()

public:
	UHoldoverGunAnimator();

	/** Switches to a gun and loads its timelines (cached per gun). */
	void SetGun(AHoldoverGun* InGun);

	bool HasAction(FName Action) const;

	/**
	 * Starts an action. If bHoldable, the action pauses at its holdAt time while SetHoldInput(true).
	 * Returns false if the gun has no such timeline.
	 */
	bool Play(FName Action, bool bHoldable, float StartTime = 0.f);

	/** Time of the first event with this name in an action, or -1 if it has none. */
	float GetEventTime(FName Action, FName EventName) const;

	/** Stops the current action straight away (hands return to the grip). */
	void Stop();

	/** The key that started the current action is held (true) or released (false). */
	void SetHoldInput(bool bHeld) { bHoldInput = bHeld; }

	bool IsPlaying() const { return Current != nullptr; }
	FName GetCurrentAction() const { return Current ? Current->Name : NAME_None; }

	/** True while an action other than a cosmetic one (fire, dry fire) is playing. */
	bool IsBusy() const;

	/** True while paused at holdAt. */
	bool IsHolding() const { return bPausedAtHold; }

	/** Advances the action and works out this frame's gun pose, hand targets and finger weights. */
	void Update(float DeltaSeconds, const FHoldoverAnimFrame& Frame);

	/** Final gun transform relative to the camera, including the action's gun offset. */
	const FTransform& GetGunInCamera() const { return GunInCamera; }

	/** Hand target (palm centre; X = fingers, Z = out of the palm) relative to the camera. */
	const FTransform& GetHandInCamera(bool bLeft) const { return bLeft ? HandLInCamera : HandRInCamera; }

	/** Current finger pose weights for one hand. */
	const TMap<FName, float>& GetFingerWeights(bool bLeft) const { return bLeft ? FingersL : FingersR; }

	/** Fired for every timeline event as playback passes it. */
	FHoldoverAnimEventSignal OnEvent;

	// ---------- Magazine carried in a hand ----------

	bool bHandHasMag = false;
	FHoldoverMagazine HandMag;

	/** Playback speed (1 = as authored). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.25", ClampMax = "3"))
	float SpeedScale = 1.f;

private:
	FTransform ResolveTarget(const HoldoverAnim::FHandTarget& Target, const FHoldoverAnimFrame& Frame) const;
	FTransform HoldTarget(bool bLeft, const FHoldoverAnimFrame& Frame) const;
	FTransform SampleHand(const TArray<HoldoverAnim::FHandKey>& Keys, bool bLeft, float T, const FHoldoverAnimFrame& Frame) const;
	void SampleFingers(float T);
	void ApplyBoneTracks(float T);
	void ApplySlideHand(float T);
	void ApplyAttachments(float T, const FHoldoverAnimFrame& Frame);
	void FireEvents(float From, float To);
	void FinishAction();
	void BuildDefaultHold();

	TWeakObjectPtr<AHoldoverGun> Gun;
	TSharedPtr<HoldoverAnim::FGunAnimSet> Set;
	HoldoverAnim::FHold DefaultHold;

	const HoldoverAnim::FAction* Current = nullptr;
	float Time = 0.f;
	bool bHoldable = false;
	bool bHoldInput = false;
	bool bPausedAtHold = false;

	// Slide worked by hand
	bool bRackEngaged = false;
	float LastSlideFraction = 0.f;
	bool bSlideReleased = false;

	// Carried bone (magazine)
	FName AttachedBone;
	bool bAttachedLeft = true;
	bool bAttachOffsetValid = false;
	FTransform AttachOffset = FTransform::Identity;

	FTransform GunInCamera = FTransform::Identity;
	FTransform HandLInCamera = FTransform::Identity;
	FTransform HandRInCamera = FTransform::Identity;
	TMap<FName, float> FingersL;
	TMap<FName, float> FingersR;

	FHoldoverAnimFrame LastFrame;

	/** Loaded timelines shared by every character. */
	static TMap<FName, TSharedPtr<HoldoverAnim::FGunAnimSet>> Cache;
};
