#include "Animation/HoldoverGunAnimator.h"

#include "Weapons/HoldoverGun.h"
#include "Weapons/HoldoverGunData.h"

using namespace HoldoverAnim;

TMap<FName, TSharedPtr<FGunAnimSet>> UHoldoverGunAnimator::Cache;

static const FName AnimFire(TEXT("Fire"));
static const FName AnimFireDA(TEXT("FireDA"));
static const FName AnimDryFire(TEXT("DryFire"));
static const FName AnimMagFromPouch(TEXT("MagFromPouch"));

UHoldoverGunAnimator::UHoldoverGunAnimator()
{
	// Updated explicitly by the character so the order within a frame is fixed.
	PrimaryComponentTick.bCanEverTick = false;
}

void UHoldoverGunAnimator::SetGun(AHoldoverGun* InGun)
{
	Stop();
	Gun = InGun;
	Set.Reset();
	if (!InGun || !InGun->GetData())
	{
		return;
	}

	const FName Id = InGun->GetData()->GunId;
	if (TSharedPtr<FGunAnimSet>* Cached = Cache.Find(Id))
	{
		Set = *Cached;
	}
	else
	{
		Set = FGunAnimSet::Load(Id);
		Cache.Add(Id, Set); // cache misses too, so a missing file is only reported once
	}
	BuildDefaultHold();
}

void UHoldoverGunAnimator::BuildDefaultHold()
{
	// Used when the gun has no timeline file: a plain two-handed grip.
	DefaultHold = FHold();
	DefaultHold.bValid = true;
	const bool bPistol = Gun.IsValid() && Gun->GetData() && Gun->GetData()->bIsPistol;
	if (bPistol)
	{
		DefaultHold.HandR.Pos = FVector(1.8f, 1.7f, -8.0f);
		DefaultHold.HandR.FingersDir = FVector(0.71f, 0.f, -0.71f).GetSafeNormal();
		DefaultHold.HandR.PalmNormal = FVector(0.f, -1.f, 0.f);
		DefaultHold.HandL.Pos = FVector(2.6f, -2.4f, -9.2f);
		DefaultHold.HandL.FingersDir = FVector(0.55f, 0.3f, -0.78f).GetSafeNormal();
		DefaultHold.HandL.PalmNormal = FVector(-0.17f, 0.95f, 0.25f).GetSafeNormal();
	}
	else
	{
		const FVector Grip = Gun.IsValid() ? Gun->GetSocketLocal(TEXT("GripHand")) : FVector(-18.8f, 0.f, -8.4f);
		const FVector Support = Gun.IsValid() ? Gun->GetSocketLocal(TEXT("SupportHand")) : FVector(18.f, 0.f, -1.2f);
		DefaultHold.HandR.Pos = Grip + FVector(0.f, 1.6f, 0.f);
		DefaultHold.HandR.FingersDir = FVector(0.5f, 0.f, -0.87f);
		DefaultHold.HandR.PalmNormal = FVector(0.f, -1.f, 0.f);
		DefaultHold.HandL.Pos = Support + FVector(0.f, 0.f, -2.5f);
		DefaultHold.HandL.FingersDir = FVector(0.3f, 0.95f, 0.f).GetSafeNormal();
		DefaultHold.HandL.PalmNormal = FVector(0.f, 0.f, 1.f);
	}
	DefaultHold.FingersR.Add(TEXT("Grip"), 1.f);
	DefaultHold.FingersL.Add(TEXT("SupportWrap"), 1.f);
}

bool UHoldoverGunAnimator::HasAction(FName Action) const
{
	return Set.IsValid() && Set->Has(Action);
}

bool UHoldoverGunAnimator::IsBusy() const
{
	if (!Current)
	{
		return false;
	}
	return Current->Name != AnimFire && Current->Name != AnimFireDA && Current->Name != AnimDryFire;
}

bool UHoldoverGunAnimator::Play(FName Action, bool bInHoldable)
{
	if (!Set.IsValid())
	{
		return false;
	}
	const FAction* A = Set->Find(Action);
	if (!A)
	{
		return false;
	}
	if (Current && Current != A)
	{
		FinishAction();
	}

	Current = A;
	Time = 0.f;
	bHoldable = bInHoldable;
	bHoldInput = bInHoldable;
	bPausedAtHold = false;
	bRackEngaged = false;
	bSlideReleased = false;
	LastSlideFraction = Gun.IsValid() ? Gun->GetSlidePosition() : 0.f;
	AttachedBone = NAME_None;
	bAttachOffsetValid = false;

	// Events at time zero fire straight away.
	FireEvents(-1.f, 0.f);
	return true;
}

void UHoldoverGunAnimator::Stop()
{
	if (Current)
	{
		FinishAction();
	}
}

void UHoldoverGunAnimator::FinishAction()
{
	if (bRackEngaged && Gun.IsValid())
	{
		Gun->EndRack();
	}
	bRackEngaged = false;
	if (Gun.IsValid())
	{
		Gun->SetHandSlideTarget(-1.f);
	}
	AttachedBone = NAME_None;
	bAttachOffsetValid = false;
	Current = nullptr;
	Time = 0.f;
	bPausedAtHold = false;
}

// ---------------------------------------------------------------------
// Frame update
// ---------------------------------------------------------------------

void UHoldoverGunAnimator::Update(float DeltaSeconds, const FHoldoverAnimFrame& Frame)
{
	LastFrame = Frame;
	GunInCamera = Frame.BaseGunInCamera;

	// 1. Advance time (pausing at holdAt while the key is held).
	float Prev = Time;
	if (Current)
	{
		float Next = Time + DeltaSeconds * SpeedScale;
		if (bHoldable && bHoldInput && Current->HoldAt >= 0.f && Time <= Current->HoldAt && Next >= Current->HoldAt)
		{
			Next = Current->HoldAt;
			bPausedAtHold = true;
		}
		else
		{
			bPausedAtHold = false;
		}
		Time = Next;
	}

	// 2. Whole-gun offset, in camera space, turning about the pivot socket.
	if (Current && (Current->GunLoc.Num() > 0 || Current->GunRot.Num() > 0))
	{
		const FVector Loc = SampleVec(Current->GunLoc, Time, FVector::ZeroVector);
		const FQuat Rot = SampleRot(Current->GunRot, Time, FQuat::Identity);
		FVector PivotLocal = FVector::ZeroVector;
		if (Gun.IsValid() && !Current->GunPivot.IsNone())
		{
			PivotLocal = Gun->GetSocketLocal(Current->GunPivot);
		}
		const FVector Pivot = Frame.BaseGunInCamera.TransformPosition(PivotLocal);
		FTransform Out = Frame.BaseGunInCamera;
		Out.SetRotation(Rot * Frame.BaseGunInCamera.GetRotation());
		Out.SetLocation(Pivot + Rot.RotateVector(Frame.BaseGunInCamera.GetLocation() - Pivot) + Loc);
		GunInCamera = Out;
	}

	// 3. Hands and fingers.
	FHoldoverAnimFrame Final = Frame;
	Final.BaseGunInCamera = GunInCamera;
	if (Current)
	{
		HandLInCamera = Current->HandL.Num() > 0 ? SampleHand(Current->HandL, true, Time, Final) : HoldTarget(true, Final);
		HandRInCamera = Current->HandR.Num() > 0 ? SampleHand(Current->HandR, false, Time, Final) : HoldTarget(false, Final);
	}
	else
	{
		HandLInCamera = HoldTarget(true, Final);
		HandRInCamera = HoldTarget(false, Final);
	}
	SampleFingers(Time);

	// 4. Gun bones, the slide under the hand, and anything carried.
	if (Current && Gun.IsValid())
	{
		ApplyBoneTracks(Time);
		ApplySlideHand(Time);
		ApplyAttachments(Time, Final);
	}
	if (Gun.IsValid())
	{
		Gun->SetHandMagazine(bHandHasMag, HandMag.Rounds);
	}

	// 5. Events, then finish.
	if (Current)
	{
		FireEvents(Prev, Time);
		if (Current && Time >= Current->Duration && !bPausedAtHold)
		{
			FinishAction();
		}
	}
}

FTransform UHoldoverGunAnimator::ResolveTarget(const FHandTarget& Target, const FHoldoverAnimFrame& Frame) const
{
	const FQuat Basis = HandBasis(Target.FingersDir, Target.PalmNormal);
	const FTransform Local(Basis, Target.Pos);
	switch (Target.Space)
	{
	case ESpace::Camera:
		return Local;
	case ESpace::Body:
		return Local * Frame.FeetWorld.GetRelativeTransform(Frame.CameraWorld);
	default:
		return Local * Frame.BaseGunInCamera;
	}
}

FTransform UHoldoverGunAnimator::HoldTarget(bool bLeft, const FHoldoverAnimFrame& Frame) const
{
	const FHold& H = (Set.IsValid() && Set->Hold.bValid) ? Set->Hold : DefaultHold;
	return ResolveTarget(bLeft ? H.HandL : H.HandR, Frame);
}

FTransform UHoldoverGunAnimator::SampleHand(const TArray<FHandKey>& Keys, bool bLeft, float T, const FHoldoverAnimFrame& Frame) const
{
	auto KeyTM = [&](const FHandKey& K)
	{
		return K.bHold ? HoldTarget(bLeft, Frame) : ResolveTarget(K.Target, Frame);
	};

	if (Keys.Num() == 0)
	{
		return HoldTarget(bLeft, Frame);
	}
	if (T <= Keys[0].T)
	{
		// Before the first key the hand is still on its grip, moving toward the first key.
		if (Keys[0].T <= KINDA_SMALL_NUMBER)
		{
			return KeyTM(Keys[0]);
		}
		const float A = ApplyEase(T / Keys[0].T, EEase::Smooth);
		const FTransform From = HoldTarget(bLeft, Frame);
		const FTransform To = KeyTM(Keys[0]);
		FTransform Out;
		Out.SetLocation(FMath::Lerp(From.GetLocation(), To.GetLocation(), A));
		Out.SetRotation(FQuat::Slerp(From.GetRotation(), To.GetRotation(), A).GetNormalized());
		return Out;
	}
	for (int32 i = 0; i < Keys.Num() - 1; ++i)
	{
		if (T < Keys[i + 1].T)
		{
			const float Span = FMath::Max(Keys[i + 1].T - Keys[i].T, KINDA_SMALL_NUMBER);
			const float A = ApplyEase((T - Keys[i].T) / Span, Keys[i].Ease);
			const FTransform From = KeyTM(Keys[i]);
			const FTransform To = KeyTM(Keys[i + 1]);
			FTransform Out;
			Out.SetLocation(FMath::Lerp(From.GetLocation(), To.GetLocation(), A));
			Out.SetRotation(FQuat::Slerp(From.GetRotation(), To.GetRotation(), A).GetNormalized());
			return Out;
		}
	}
	return KeyTM(Keys.Last());
}

void UHoldoverGunAnimator::SampleFingers(float T)
{
	const FHold& H = (Set.IsValid() && Set->Hold.bValid) ? Set->Hold : DefaultHold;
	FingersL = H.FingersL;
	FingersR = H.FingersR;
	if (!Current)
	{
		return;
	}

	static const FName Poses[] = {
		TEXT("Grip"), TEXT("IndexTrigger"), TEXT("SupportWrap"), TEXT("PinchRack"),
		TEXT("OverhandRack"), TEXT("PalmMag"), TEXT("ThumbPress"), TEXT("Relaxed") };

	for (const FName& Pose : Poses)
	{
		float W;
		if (SampleFinger(Current->FingersL, Pose, T, W))
		{
			FingersL.Add(Pose, W);
		}
		if (SampleFinger(Current->FingersR, Pose, T, W))
		{
			FingersR.Add(Pose, W);
		}
	}
}

void UHoldoverGunAnimator::ApplyBoneTracks(float T)
{
	AHoldoverGun* G = Gun.Get();
	for (const auto& Pair : Current->Bones)
	{
		const FName Bone = Pair.Key;
		// The state machine owns the slide group; the carried magazine follows the hand instead.
		if (G->IsMechanicalBone(Bone) || Bone == AttachedBone)
		{
			continue;
		}
		const FBoneTrack& Track = Pair.Value;
		if (T < Track.FirstTime())
		{
			continue; // not this bone's turn yet: the gun's own pose stays
		}
		const FVector Loc = SampleVec(Track.Loc, T, FVector::ZeroVector);
		const FQuat Rot = SampleRot(Track.Rot, T, FQuat::Identity);
		G->AddBoneOffset(Bone, Loc, Rot, /*bReplace*/ true);
	}
}

void UHoldoverGunAnimator::ApplySlideHand(float T)
{
	AHoldoverGun* G = Gun.Get();
	const UHoldoverGunData* Data = G->GetData();
	const FBoneTrack* Track = Data ? Current->Bones.Find(Data->SlideMotion.Bone) : nullptr;
	if (!Track || Track->Loc.Num() == 0 || T < Track->FirstTime() || bSlideReleased)
	{
		return;
	}

	// Slide offset along its travel, as a fraction (0 forward, 1 fully back).
	const FVector Travel = Data->SlideMotion.Translation;
	const float Len2 = Travel.SizeSquared();
	if (Len2 < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Loc = SampleVec(Track->Loc, T, FVector::ZeroVector);
	const float Fraction = FMath::Clamp(FVector::DotProduct(Loc, Travel) / Len2, 0.f, 1.f);

	if (!bRackEngaged)
	{
		// The hand takes the slide when it starts moving back past where it already is.
		if (Fraction > LastSlideFraction + 0.005f || (Fraction > 0.01f && Fraction >= G->GetSlidePosition()))
		{
			G->BeginRack();
			bRackEngaged = G->IsRackHeld();
		}
	}
	if (bRackEngaged)
	{
		if (Fraction < LastSlideFraction - 0.002f)
		{
			// The hand lets go (or rides it forward): the spring takes over and the gun decides
			// whether it feeds, locks open or stays jammed.
			G->EndRack();
			bRackEngaged = false;
			bSlideReleased = true;
		}
		else
		{
			G->SetHandSlideTarget(Fraction);
		}
	}
	LastSlideFraction = Fraction;
}

void UHoldoverGunAnimator::ApplyAttachments(float T, const FHoldoverAnimFrame& Frame)
{
	AHoldoverGun* G = Gun.Get();

	// Which bone is in which hand right now, and when it was picked up and let go.
	FName Bone = NAME_None;
	bool bLeft = true;
	float PickTime = 0.f;
	float LetGoTime = -1.f;
	const FAttachKey* PickKey = nullptr;
	for (int32 i = 0; i < Current->Attach.Num(); ++i)
	{
		const FAttachKey& K = Current->Attach[i];
		if (K.T > T)
		{
			break;
		}
		Bone = K.Bone;
		bLeft = K.bLeftHand;
		PickTime = K.T;
		PickKey = &K;
		LetGoTime = -1.f;
		for (int32 j = i + 1; j < Current->Attach.Num(); ++j)
		{
			if (Current->Attach[j].Bone.IsNone() || Current->Attach[j].Bone != K.Bone)
			{
				LetGoTime = Current->Attach[j].T;
				break;
			}
		}
	}

	if (Bone != AttachedBone || bLeft != bAttachedLeft)
	{
		AttachedBone = Bone;
		bAttachedLeft = bLeft;
		bAttachOffsetValid = false;
	}
	if (AttachedBone.IsNone())
	{
		return;
	}

	FTransform Rest;
	if (!G->GetBoneRest(AttachedBone, Rest))
	{
		return;
	}

	if (!bAttachOffsetValid && PickKey && PickKey->bHasGrip)
	{
		// The timeline says how the hand holds the part when it sits at rest: keep that grip while carrying it.
		// offset = inverse(grip hand) x bone rest, both in gun space.
		const FTransform GripGun(HandBasis(PickKey->Grip.FingersDir, PickKey->Grip.PalmNormal), PickKey->Grip.Pos);
		AttachOffset = Rest.GetRelativeTransform(GripGun);
		bAttachOffsetValid = true;
	}
	if (!bAttachOffsetValid)
	{
		// Older timelines without a grip: a fresh mag from the pouch is held so that it lands exactly seated when it is let go;
		// a mag taken out of the gun keeps the grip it had when the hand took it.
		const bool bFromPouch = Current->HasEvent(AnimMagFromPouch, PickTime, 0.05f);
		const float RefTime = (bFromPouch && LetGoTime >= 0.f) ? LetGoTime : PickTime;

		FVector TrackLoc = FVector::ZeroVector;
		FQuat TrackRot = FQuat::Identity;
		if (const FBoneTrack* Track = Current->Bones.Find(AttachedBone))
		{
			TrackLoc = SampleVec(Track->Loc, RefTime, FVector::ZeroVector);
			TrackRot = SampleRot(Track->Rot, RefTime, FQuat::Identity);
		}
		const FTransform BoneGun(TrackRot * Rest.GetRotation(), Rest.GetLocation() + TrackLoc);
		const FTransform BoneCam = BoneGun * Frame.BaseGunInCamera;

		const TArray<FHandKey>& Keys = bAttachedLeft ? Current->HandL : Current->HandR;
		const FTransform HandRef = SampleHand(Keys, bAttachedLeft, RefTime, Frame);
		AttachOffset = BoneCam.GetRelativeTransform(HandRef);
		bAttachOffsetValid = true;
	}

	const FTransform& HandNow = bAttachedLeft ? HandLInCamera : HandRInCamera;
	const FTransform BoneCam = AttachOffset * HandNow;
	const FTransform BoneGun = BoneCam.GetRelativeTransform(Frame.BaseGunInCamera);
	G->AddBoneOffset(AttachedBone,
		BoneGun.GetLocation() - Rest.GetLocation(),
		BoneGun.GetRotation() * Rest.GetRotation().Inverse(),
		/*bReplace*/ true);
}

void UHoldoverGunAnimator::FireEvents(float From, float To)
{
	if (!Current)
	{
		return;
	}
	const FAction* Playing = Current;
	for (const FEventKey& E : Playing->Events)
	{
		if (E.T > From && E.T <= To)
		{
			OnEvent.Broadcast(E.Name);
			if (Current != Playing)
			{
				return; // a handler started another action
			}
		}
	}
}
