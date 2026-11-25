#include "Weapons/HoldoverGunData.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Holdover.h"
#include "Sound/SoundBase.h"

UHoldoverGunData::UHoldoverGunData()
{
	ApplyPresetM9(this);
}

static FHoldoverBoneMotion MakeMove(FName Bone, const FVector& Translation)
{
	FHoldoverBoneMotion M;
	M.Bone = Bone;
	M.Translation = Translation;
	M.RotationDegrees = 0.f;
	return M;
}

static FHoldoverBoneMotion MakeTurn(FName Bone, const FVector& Axis, float Degrees, const FVector& Translation = FVector::ZeroVector)
{
	FHoldoverBoneMotion M;
	M.Bone = Bone;
	M.Translation = Translation;
	M.RotationAxis = Axis;
	M.RotationDegrees = Degrees;
	return M;
}

static void SetRecoil(UHoldoverGunData* D, float Pitch, float PitchVar, float Yaw, float YawBias, float Roll, float RollBias,
	float Back, float Rise, float Side, float Stiffness, float Damping, float Pivot, float Climb, float Wander, float WanderBias,
	float RecoveryK, float RecoveryC, float HeadFollow, float HeadPush, float Shake)
{
	D->RecoilPitchImpulse = Pitch;
	D->RecoilPitchVariance = PitchVar;
	D->RecoilYawImpulse = Yaw;
	D->RecoilYawBias = YawBias;
	D->RecoilRollImpulse = Roll;
	D->RecoilRollBias = RollBias;
	D->RecoilBackImpulse = Back;
	D->RecoilRiseImpulse = Rise;
	D->RecoilSideImpulse = Side;
	D->RecoilStiffness = Stiffness;
	D->RecoilDamping = Damping;
	D->RecoilPivotBehindGrip = Pivot;
	D->RecoilClimbImpulse = Climb;
	D->RecoilWanderImpulse = Wander;
	D->RecoilWanderBias = WanderBias;
	D->RecoilRecoveryStiffness = RecoveryK;
	D->RecoilRecoveryDamping = RecoveryC;
	D->RecoilHeadFollow = HeadFollow;
	D->RecoilHeadPush = HeadPush;
	D->RecoilShake = Shake;
}

static void SetFlash(UHoldoverGunData* D, EHoldoverMuzzleDevice Device, float Size, float Length, float Brightness,
	float FireballChance, float FaintChance, float Light, float Smoke)
{
	D->MuzzleDevice = Device;
	D->FlashSize = Size;
	D->FlashLength = Length;
	D->FlashBrightness = Brightness;
	D->FlashFireballChance = FireballChance;
	D->FlashFaintChance = FaintChance;
	D->FlashLightIntensity = Light;
	D->SmokeAmount = Smoke;
}

// Bone numbers come from docs/weapons/pistol_m9.md, converted to Unreal gun space
// (centimetres, X forward, Y right, Z up). A positive turn about +Y swings a part's
// top forward, so "rearward" turns of hammer tops are negative.
void UHoldoverGunData::ApplyPresetM9(UHoldoverGunData* D)
{
	D->GunId = TEXT("Pistol_M9");
	D->DisplayName = NSLOCTEXT("Holdover", "GunM9", "9 mm service pistol");
	D->bIsPistol = true;

	D->FallbackCasingSize = FVector(0.99f, 0.99f, 1.92f);
	D->FallbackRoundLength = 2.97f;
	D->FallbackMagazineSize = FVector(2.8f, 2.2f, 12.5f);

	D->ActionType = EHoldoverActionType::PistolSlide;
	D->SlideMotion = MakeMove(TEXT("Slide"), FVector(-6.0f, 0.f, 0.f));
	D->LockedBackFraction = 0.96f;
	D->EjectFraction = 0.85f;
	D->bLastRoundHoldOpen = true;
	D->bHasSlideStop = true;
	D->CycleTime = 0.07f;
	D->HandPullTime = 0.18f;
	D->SpringReturnTime = 0.05f;
	D->LockingMotion = MakeMove(TEXT("Barrel"), FVector(-0.9f, 0.f, -0.15f));
	D->LockingTravelFraction = 0.15f;
	D->HammerCockMotion = MakeTurn(TEXT("Hammer"), FVector(0.f, 1.f, 0.f), -60.f);
	D->TriggerPullMotion = MakeTurn(TEXT("Trigger"), FVector(0.f, 1.f, 0.f), 15.f);
	D->SlideStopUpMotion = MakeTurn(TEXT("SlideStop"), FVector(0.f, 1.f, 0.f), -8.f);
	D->MagReleaseMotion = MakeMove(TEXT("MagRelease"), FVector(0.f, 0.25f, 0.f));
	D->SafetyMotion = MakeTurn(TEXT("Safety"), FVector(0.f, 1.f, 0.f), -60.f);
	D->SelectorSafeFraction = 1.f;
	D->SelectorSemiFraction = 0.f;
	D->SelectorAutoFraction = 0.f;
	D->bHasSafety = true;
	D->bSafetyDecocks = true;
	D->bSafetyBlocksAction = false;
	D->bDoubleAction = true;
	D->bHasFullAuto = false;
	D->CyclicRateRPM = 600.f;

	D->MagazineCapacity = 15;
	D->StartingMagazines = 4;
	D->MagazineBone = TEXT("Magazine");
	D->MagazineOutDirection = FVector(-0.259f, 0.f, -0.966f);
	D->MagazineOutTravel = 12.5f;
	D->MagazineRockMotion = FHoldoverBoneMotion();
	D->MagazineSeatTime = 0.12f;
	D->MagFollowerBone = TEXT("MagFollower");
	D->FollowerOffsetPerRound = FVector(-0.119f, 0.f, -0.444f);

	D->Bullet.MuzzleVelocity = 360.f;
	D->Bullet.MassGrams = 8.0f;
	D->Bullet.DiameterMm = 9.01f;
	D->Bullet.DragCoefficient = 0.30f;
	D->Bullet.AccuracyMOA = 3.0f;
	D->Bullet.ZeroDistanceMeters = 25.f;
	D->Bullet.MaxFlightTime = 4.f;

	D->CaseEjectVelocity = FVector(-50.f, 380.f, 300.f); // right, up and a little back: lands 1 to 2 m away
	D->CaseEjectRandom = 60.f;
	D->CaseSpinDegPerSec = 3200.f;
	D->LiveRoundEjectScale = 0.35f;
	D->CaseMassGrams = 4.0f;
	D->RoundMassGrams = 12.0f;

	D->ReadyOffset = FVector(40.f, 7.f, -12.f);
	D->ReadyRotation = FRotator(-5.f, -2.f, 0.f);
	D->AimEyeRelief = 58.f;
	D->InspectOffset = FVector(32.f, 4.f, -14.f);
	D->InspectRotation = FRotator(5.f, -10.f, -35.f);
	D->AimTime = 0.4f;
	D->AimArc = FVector(3.f, 0.f, -1.f);
	D->SwayWeight = 1.f;

	// Snappy 9 mm: a quick flip that the hands bring straight back.
	SetRecoil(D, /*Pitch*/ 300.f, 0.18f, /*Yaw*/ 40.f, 5.f, /*Roll*/ 25.f, 6.f, /*Back, rise, side*/ 90.f, 25.f, 6.f,
		/*Stiffness, damping*/ 300.f, 26.f, /*Pivot*/ 6.f, /*Climb*/ 14.f, /*Wander*/ 5.f, 1.f, /*Recovery*/ 40.f, 12.6f,
		/*Head follow, push, shake*/ 0.04f, 0.04f, 0.10f);

	SetFlash(D, EHoldoverMuzzleDevice::Bare, /*Size*/ 8.f, /*Length*/ 12.f, /*Brightness*/ 1.f, /*Fireball*/ 0.3f, /*Faint*/ 0.25f,
		/*Light*/ 0.45f, /*Smoke*/ 1.f);
	D->BrassSoundSet = TEXT("Pistol");
	D->ShotSounds.Reset();
	D->TailSounds.Reset();
	D->MechSounds.Reset();

	D->StovepipeChance = 0.f;
	D->DoubleFeedChance = 0.f;
	D->DudChance = 0.f;
}

// Numbers from docs/weapons/rifle_ak.md.
void UHoldoverGunData::ApplyPresetAK(UHoldoverGunData* D)
{
	ApplyPresetM9(D); // start from complete defaults, then override

	D->GunId = TEXT("Rifle_AK");
	D->DisplayName = NSLOCTEXT("Holdover", "GunAK", "7.62x39 rifle");
	D->bIsPistol = false;

	D->FallbackCasingSize = FVector(1.13f, 1.13f, 3.86f);
	D->FallbackRoundLength = 5.6f;
	D->FallbackMagazineSize = FVector(6.5f, 2.6f, 19.5f);

	D->ActionType = EHoldoverActionType::RotatingBolt;
	D->SlideMotion = MakeMove(TEXT("BoltCarrier"), FVector(-7.5f, 0.f, 0.f)); // 75 mm: matches the model and the timelines
	D->LockedBackFraction = 1.f;
	D->EjectFraction = 0.8f;
	D->bLastRoundHoldOpen = false;
	D->bHasSlideStop = false;
	D->CycleTime = 0.1f;
	D->HandPullTime = 0.24f;
	D->SpringReturnTime = 0.06f;
	D->LockingMotion = MakeTurn(TEXT("Bolt"), FVector(1.f, 0.f, 0.f), 35.f);
	D->LockingTravelFraction = 0.107f; // bolt unlocks in the first 8 mm of 75
	D->HammerCockMotion = FHoldoverBoneMotion();
	D->TriggerPullMotion = MakeTurn(TEXT("Trigger"), FVector(0.f, 1.f, 0.f), 12.f);
	D->SlideStopUpMotion = FHoldoverBoneMotion();
	D->MagReleaseMotion = MakeTurn(TEXT("MagRelease"), FVector(0.f, 1.f, 0.f), 12.f);
	// Selector: SAFE is the top position; AUTO is the middle, SEMI the bottom.
	D->SafetyMotion = MakeTurn(TEXT("Selector"), FVector(0.f, 1.f, 0.f), 24.f);
	D->SelectorSafeFraction = 0.f;
	D->SelectorAutoFraction = 0.5f;
	D->SelectorSemiFraction = 1.f;
	D->bHasSafety = true;
	D->bSafetyDecocks = false;
	D->bSafetyBlocksAction = true;
	D->bDoubleAction = false;
	D->bHasFullAuto = true;
	D->CyclicRateRPM = 600.f;

	D->MagazineCapacity = 30;
	D->StartingMagazines = 4;
	D->MagazineBone = TEXT("Magazine");
	D->MagazineOutDirection = FVector(0.f, 0.f, -1.f);
	D->MagazineOutTravel = 20.f;
	// Rock out: the mag's bottom swings forward about the front lug before it drops.
	D->MagazineRockMotion = MakeTurn(TEXT("Magazine"), FVector(0.f, 1.f, 0.f), -20.f);
	D->MagazineSeatTime = 0.18f;
	D->MagFollowerBone = TEXT("MagFollower");
	D->FollowerOffsetPerRound = FVector(0.15f, 0.f, -0.58f);

	D->Bullet.MuzzleVelocity = 715.f;
	D->Bullet.MassGrams = 7.97f;
	D->Bullet.DiameterMm = 7.92f;
	D->Bullet.DragCoefficient = 0.29f;
	D->Bullet.AccuracyMOA = 4.0f;
	D->Bullet.ZeroDistanceMeters = 50.f;
	D->Bullet.MaxFlightTime = 4.f;

	D->LiveRoundEjectScale = 0.3f;
	D->CaseMassGrams = 6.6f;
	D->RoundMassGrams = 16.3f;

	D->ReadyOffset = FVector(32.f, 10.f, -11.f);
	D->ReadyRotation = FRotator(-5.f, -3.f, 0.f);
	D->AimEyeRelief = 52.f;
	D->InspectOffset = FVector(30.f, 6.f, -16.f);
	D->InspectRotation = FRotator(5.f, -15.f, -40.f);
	D->AimTime = 0.5f;
	D->AimArc = FVector(5.f, 0.f, -2.f);
	D->SwayWeight = 1.6f;

	// Heavy rolling push that wanders: loose damping and a lot of sideways climb.
	SetRecoil(D, /*Pitch*/ 150.f, 0.3f, /*Yaw*/ 70.f, 12.f, /*Roll*/ 40.f, 15.f, /*Back, rise, side*/ 75.f, 14.f, 10.f,
		/*Stiffness, damping*/ 210.f, 20.f, /*Pivot*/ 6.f, /*Climb*/ 26.f, /*Wander*/ 16.f, 4.f, /*Recovery*/ 18.f, 7.5f,
		/*Head follow, push, shake*/ 0.85f, 0.85f, 0.18f);

	SetFlash(D, EHoldoverMuzzleDevice::SlantBrake, /*Size*/ 13.f, /*Length*/ 24.f, /*Brightness*/ 1.1f, /*Fireball*/ 0.35f, /*Faint*/ 0.1f,
		/*Light*/ 0.7f, /*Smoke*/ 1.4f);
	D->BrassSoundSet = TEXT("Rifle");

	// Throws its brass hard, forward and to the right (lands 3 to 5 m away).
	D->CaseEjectVelocity = FVector(170.f, 620.f, 130.f);
	D->CaseEjectRandom = 90.f;
	D->CaseSpinDegPerSec = 2600.f;
}

// Numbers from docs/weapons/pistol_1911.md and tools/anim/gen_gun_anims.py (mm there, cm here; Y mirrored).
void UHoldoverGunData::ApplyPreset1911(UHoldoverGunData* D)
{
	ApplyPresetM9(D);

	D->GunId = TEXT("Pistol_1911");
	D->DisplayName = NSLOCTEXT("Holdover", "Gun1911", ".45 single-action pistol");
	D->bIsPistol = true;

	D->FallbackCasingSize = FVector(1.21f, 1.21f, 2.28f);
	D->FallbackRoundLength = 3.24f;
	D->FallbackMagazineSize = FVector(3.4f, 1.5f, 11.8f);

	D->ActionType = EHoldoverActionType::PistolSlide;
	D->SlideMotion = MakeMove(TEXT("Slide"), FVector(-6.2f, 0.f, 0.f));
	D->LockedBackFraction = 0.968f; // 60 of 62 mm
	D->EjectFraction = 0.85f;
	D->CycleTime = 0.078f;          // heavier slide, softer spring than the 9 mm
	D->HandPullTime = 0.18f;
	D->SpringReturnTime = 0.052f;
	// Barrel rides back 3 mm with the slide, then its rear swings down on the link.
	D->LockingMotion = MakeTurn(TEXT("Barrel"), FVector(0.f, 1.f, 0.f), -1.5f, FVector(-0.3f, 0.f, -0.2f));
	D->LockingTravelFraction = 0.1f;
	D->HammerCockMotion = MakeTurn(TEXT("Hammer"), FVector(0.f, 1.f, 0.f), -55.f);
	D->TriggerPullMotion = MakeMove(TEXT("Trigger"), FVector(-0.3f, 0.f, 0.f)); // a 1911 trigger slides straight back
	D->SlideStopUpMotion = MakeTurn(TEXT("SlideStop"), FVector(0.f, 1.f, 0.f), -6.f);
	D->MagReleaseMotion = MakeMove(TEXT("MagRelease"), FVector(0.f, 0.25f, 0.f));
	// Frame thumb safety: up = safe, 20 degrees down = fire.
	D->SafetyMotion = MakeTurn(TEXT("ThumbSafety"), FVector(0.f, 1.f, 0.f), 20.f);
	D->SelectorSafeFraction = 0.f;
	D->SelectorSemiFraction = 1.f;
	D->SelectorAutoFraction = 1.f;
	D->bHasSafety = true;
	D->bSafetyDecocks = false;
	D->bDoubleAction = false;   // single action: the hammer must be cocked (rack or thumb)

	D->MagazineCapacity = 7;
	D->MagazineOutDirection = FVector(-0.309f, 0.f, -0.951f); // grip raked 18 degrees
	D->MagazineOutTravel = 12.f;
	D->FollowerOffsetPerRound = FVector(-0.139f, 0.f, -0.428f);

	D->Bullet.MuzzleVelocity = 255.f;  // 230 grain ball
	D->Bullet.MassGrams = 14.9f;
	D->Bullet.DiameterMm = 11.48f;
	D->Bullet.DragCoefficient = 0.29f;
	D->Bullet.AccuracyMOA = 3.5f;
	D->Bullet.ZeroDistanceMeters = 25.f;

	D->CaseEjectVelocity = FVector(-60.f, 420.f, 300.f); // right and up, a little back
	D->CaseEjectRandom = 70.f;
	D->CaseSpinDegPerSec = 2800.f;
	D->CaseMassGrams = 6.2f;
	D->RoundMassGrams = 21.f;

	// A slower, heavier shove than the 9 mm that lifts the muzzle further.
	SetRecoil(D, /*Pitch*/ 340.f, 0.15f, /*Yaw*/ 45.f, 6.f, /*Roll*/ 30.f, 8.f, /*Back, rise, side*/ 105.f, 30.f, 7.f,
		/*Stiffness, damping*/ 260.f, 25.f, /*Pivot*/ 6.f, /*Climb*/ 18.f, /*Wander*/ 6.f, 1.5f, /*Recovery*/ 35.f, 11.8f,
		/*Head follow, push, shake*/ 0.04f, 0.05f, 0.12f);

	// Low-pressure round: an orange puff, often small, and plenty of smoke.
	SetFlash(D, EHoldoverMuzzleDevice::Bare, /*Size*/ 9.f, /*Length*/ 10.f, /*Brightness*/ 0.9f, /*Fireball*/ 0.2f, /*Faint*/ 0.35f,
		/*Light*/ 0.4f, /*Smoke*/ 1.3f);
	D->BrassSoundSet = TEXT("Pistol");
}

// Numbers from docs/weapons/rifle_fal.md.
void UHoldoverGunData::ApplyPresetFAL(UHoldoverGunData* D)
{
	ApplyPresetAK(D);

	D->GunId = TEXT("Rifle_FAL");
	D->DisplayName = NSLOCTEXT("Holdover", "GunFAL", "7.62x51 battle rifle");
	D->bIsPistol = false;

	D->FallbackCasingSize = FVector(1.20f, 1.20f, 5.12f);
	D->FallbackRoundLength = 7.1f;
	D->FallbackMagazineSize = FVector(7.0f, 2.6f, 15.5f);

	D->ActionType = EHoldoverActionType::BoltCarrier;
	D->SlideMotion = MakeMove(TEXT("BoltCarrier"), FVector(-12.5f, 0.f, 0.f));
	D->LockedBackFraction = 0.97f;
	D->EjectFraction = 0.75f;
	D->bLastRoundHoldOpen = true;
	D->bHasSlideStop = true;
	D->CycleTime = 0.09f;
	D->HandPullTime = 0.28f;
	D->SpringReturnTime = 0.06f;
	D->LockingMotion = MakeTurn(TEXT("Bolt"), FVector(0.f, 1.f, 0.f), 4.f); // tilting bolt
	D->LockingTravelFraction = 0.08f;
	D->HammerCockMotion = FHoldoverBoneMotion();
	D->TriggerPullMotion = MakeTurn(TEXT("Trigger"), FVector(0.f, 1.f, 0.f), 10.f);
	D->SlideStopUpMotion = MakeTurn(TEXT("BoltCatch"), FVector(0.f, 1.f, 0.f), -6.f);
	D->MagReleaseMotion = MakeTurn(TEXT("MagRelease"), FVector(0.f, 1.f, 0.f), 12.f);
	D->SafetyMotion = MakeTurn(TEXT("Selector"), FVector(0.f, 1.f, 0.f), -90.f);
	D->SelectorSafeFraction = 0.f;
	D->SelectorSemiFraction = 1.f;
	D->SelectorAutoFraction = 1.f;
	D->bHasSafety = true;
	D->bSafetyDecocks = false;
	D->bSafetyBlocksAction = false;
	D->bDoubleAction = false;
	D->bHasFullAuto = false;

	D->MagazineCapacity = 20;
	D->MagazineOutDirection = FVector(0.f, 0.f, -1.f);
	D->MagazineOutTravel = 18.f;
	D->MagazineRockMotion = MakeTurn(TEXT("Magazine"), FVector(0.f, 1.f, 0.f), -15.f);
	D->MagazineSeatTime = 0.18f;
	D->FollowerOffsetPerRound = FVector(0.f, 0.f, -0.6f);

	D->Bullet.MuzzleVelocity = 840.f;
	D->Bullet.MassGrams = 9.5f;
	D->Bullet.DiameterMm = 7.82f;
	D->Bullet.DragCoefficient = 0.27f;
	D->Bullet.AccuracyMOA = 3.f;
	D->Bullet.ZeroDistanceMeters = 100.f;

	// Battle-rifle brass leaves violently, right and a little forward (5 m or more).
	D->CaseEjectVelocity = FVector(120.f, 720.f, 160.f);
	D->CaseEjectRandom = 110.f;
	D->CaseSpinDegPerSec = 2200.f;
	D->CaseMassGrams = 12.f;
	D->RoundMassGrams = 25.f;

	D->SwayWeight = 2.f;
	D->AimTime = 0.55f;

	// The hardest push: a big shove into the shoulder that moves the head and lifts the muzzle a long way.
	SetRecoil(D, /*Pitch*/ 200.f, 0.25f, /*Yaw*/ 60.f, 8.f, /*Roll*/ 35.f, 10.f, /*Back, rise, side*/ 105.f, 18.f, 9.f,
		/*Stiffness, damping*/ 190.f, 21.f, /*Pivot*/ 6.f, /*Climb*/ 34.f, /*Wander*/ 12.f, 3.f, /*Recovery*/ 16.f, 8.f,
		/*Head follow, push, shake*/ 0.85f, 0.85f, 0.28f);

	SetFlash(D, EHoldoverMuzzleDevice::SlottedHider, /*Size*/ 11.f, /*Length*/ 20.f, /*Brightness*/ 1.f, /*Fireball*/ 0.25f, /*Faint*/ 0.25f,
		/*Light*/ 0.7f, /*Smoke*/ 1.3f);
	D->BrassSoundSet = TEXT("Rifle");
}

// Numbers from docs/weapons/carbine_ar.md.
void UHoldoverGunData::ApplyPresetAR(UHoldoverGunData* D)
{
	ApplyPresetAK(D);

	D->GunId = TEXT("Carbine_AR");
	D->DisplayName = NSLOCTEXT("Holdover", "GunAR", "5.56 carbine");
	D->bIsPistol = false;

	D->FallbackCasingSize = FVector(0.96f, 0.96f, 4.48f);
	D->FallbackRoundLength = 5.7f;
	D->FallbackMagazineSize = FVector(6.3f, 2.3f, 18.f);

	D->ActionType = EHoldoverActionType::BoltCarrier;
	D->SlideMotion = MakeMove(TEXT("BoltCarrier"), FVector(-9.5f, 0.f, 0.f));
	D->LockedBackFraction = 0.97f;
	D->EjectFraction = 0.7f;
	D->bLastRoundHoldOpen = true;
	D->bHasSlideStop = true;
	D->CycleTime = 0.08f;
	D->HandPullTime = 0.24f;
	D->SpringReturnTime = 0.055f;
	D->LockingMotion = MakeTurn(TEXT("Bolt"), FVector(1.f, 0.f, 0.f), 22.f);
	D->LockingTravelFraction = 0.084f;
	D->HammerCockMotion = FHoldoverBoneMotion();
	D->TriggerPullMotion = MakeTurn(TEXT("Trigger"), FVector(0.f, 1.f, 0.f), 8.f);
	D->SlideStopUpMotion = MakeTurn(TEXT("BoltCatch"), FVector(0.f, 1.f, 0.f), 8.f);
	D->MagReleaseMotion = MakeMove(TEXT("MagRelease"), FVector(0.f, -0.3f, 0.f));
	D->SafetyMotion = MakeTurn(TEXT("Selector"), FVector(0.f, 1.f, 0.f), -90.f);
	D->SelectorSafeFraction = 0.f;
	D->SelectorSemiFraction = 1.f;
	D->SelectorAutoFraction = 1.f;
	D->bHasSafety = true;
	D->bSafetyDecocks = false;
	D->bSafetyBlocksAction = false;
	D->bDoubleAction = false;
	D->bHasFullAuto = false;

	D->MagazineCapacity = 30;
	D->MagazineOutDirection = FVector(0.f, 0.f, -1.f);
	D->MagazineOutTravel = 17.f;
	D->MagazineRockMotion = FHoldoverBoneMotion();
	D->MagazineSeatTime = 0.12f;
	D->FollowerOffsetPerRound = FVector(0.f, 0.f, -0.45f);

	D->Bullet.MuzzleVelocity = 910.f; // 55 grain from a 16 inch barrel
	D->Bullet.MassGrams = 3.56f;
	D->Bullet.DiameterMm = 5.7f;
	D->Bullet.DragCoefficient = 0.25f;
	D->Bullet.AccuracyMOA = 2.5f;
	D->Bullet.ZeroDistanceMeters = 50.f;

	// Off the brass deflector: right and a little back, about the 4 o'clock direction.
	D->CaseEjectVelocity = FVector(-110.f, 470.f, 110.f);
	D->CaseEjectRandom = 80.f;
	D->CaseSpinDegPerSec = 2800.f;
	D->CaseMassGrams = 6.f;
	D->RoundMassGrams = 12.f;

	D->SwayWeight = 1.4f;

	// Light and quick: a straight, short push that settles fast.
	SetRecoil(D, /*Pitch*/ 110.f, 0.2f, /*Yaw*/ 35.f, 4.f, /*Roll*/ 18.f, 5.f, /*Back, rise, side*/ 55.f, 10.f, 4.f,
		/*Stiffness, damping*/ 300.f, 30.f, /*Pivot*/ 6.f, /*Climb*/ 14.f, /*Wander*/ 6.f, 1.5f, /*Recovery*/ 30.f, 11.f,
		/*Head follow, push, shake*/ 0.8f, 0.85f, 0.1f);

	SetFlash(D, EHoldoverMuzzleDevice::Birdcage, /*Size*/ 7.5f, /*Length*/ 11.f, /*Brightness*/ 0.9f, /*Fireball*/ 0.12f, /*Faint*/ 0.35f,
		/*Light*/ 0.5f, /*Smoke*/ 0.8f);
	D->BrassSoundSet = TEXT("Rifle");
}

bool UHoldoverGunData::ApplyPresetForSlot(UHoldoverGunData* D, int32 Slot)
{
	switch (Slot)
	{
	case 0: ApplyPresetM9(D); return true;
	case 1: ApplyPresetAK(D); return true;
	case 2: ApplyPreset1911(D); return true;
	case 3: ApplyPresetFAL(D); return true;
	case 4: ApplyPresetAR(D); return true;
	default: return false;
	}
}

void UHoldoverGunData::LoadImportedArt(UHoldoverGunData* D)
{
	if (!D || D->GunId.IsNone())
	{
		return;
	}
	// The Blender exports are imported to /Game/Guns/<GunId>/ with their file names (tools/unreal/import_guns.py).
	static const TMap<FName, FString> Calibers = {
		{TEXT("Pistol_M9"), TEXT("9mm")},
		{TEXT("Pistol_1911"), TEXT("45ACP")},
		{TEXT("Rifle_AK"), TEXT("762x39")},
		{TEXT("Rifle_FAL"), TEXT("762x51")},
		{TEXT("Carbine_AR"), TEXT("556")},
	};
	const FString Id = D->GunId.ToString();
	const FString Folder = FString::Printf(TEXT("/Game/Guns/%s/"), *Id);
	auto Load = [&Folder](const FString& Name) -> UObject*
	{
		const FString Path = Folder + Name + TEXT(".") + Name;
		return StaticLoadObject(UObject::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	};
	if (!D->GunMesh)
	{
		D->GunMesh = Cast<USkeletalMesh>(Load(TEXT("SK_") + Id));
	}
	if (!D->MagazineMesh)
	{
		D->MagazineMesh = Cast<UStaticMesh>(Load(TEXT("SM_") + Id + TEXT("_Mag")));
	}
	if (const FString* Cal = Calibers.Find(D->GunId))
	{
		if (!D->RoundMesh)
		{
			D->RoundMesh = Cast<UStaticMesh>(Load(TEXT("SM_Round_") + *Cal));
		}
		if (!D->CasingMesh)
		{
			D->CasingMesh = Cast<UStaticMesh>(Load(TEXT("SM_Casing_") + *Cal));
		}
	}
	// Sounds made by tools/audio/gen_gun_audio.py and imported by tools/unreal/import_fx.py:
	// /Game/Audio/Guns/<GunId>/SW_<GunId>_<Layer>_01, _02, ...
	const FString SoundFolder = FString::Printf(TEXT("/Game/Audio/Guns/%s/"), *Id);
	auto LoadSet = [&SoundFolder, &Id](TArray<TObjectPtr<USoundBase>>& Out, const TCHAR* Layer)
	{
		if (Out.Num() > 0)
		{
			return;
		}
		for (int32 i = 1; i <= 16; ++i)
		{
			const FString Name = FString::Printf(TEXT("SW_%s_%s_%02d"), *Id, Layer, i);
			const FString Path = SoundFolder + Name + TEXT(".") + Name;
			USoundBase* S = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
			if (!S)
			{
				break;
			}
			Out.Add(S);
		}
	};
	LoadSet(D->ShotSounds, TEXT("Shot"));
	LoadSet(D->TailSounds, TEXT("Tail"));
	LoadSet(D->MechSounds, TEXT("Mech"));

	UE_LOG(LogHoldover, Log, TEXT("Gun art for %s: model %s, magazine %s, sounds %d shot / %d tail / %d action"), *Id,
		D->GunMesh ? TEXT("imported") : TEXT("missing (gray stand-in)"),
		D->MagazineMesh ? TEXT("imported") : TEXT("missing"),
		D->ShotSounds.Num(), D->TailSounds.Num(), D->MechSounds.Num());
}
