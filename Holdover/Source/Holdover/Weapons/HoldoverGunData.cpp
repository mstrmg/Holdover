#include "Weapons/HoldoverGunData.h"

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

	D->CaseEjectVelocity = FVector(-60.f, 350.f, 250.f);
	D->CaseEjectRandom = 60.f;
	D->CaseSpinDegPerSec = 1800.f;
	D->LiveRoundEjectScale = 0.35f;
	D->CaseMassGrams = 4.0f;
	D->RoundMassGrams = 12.0f;

	D->ReadyOffset = FVector(38.f, 9.f, -24.f);
	D->ReadyRotation = FRotator(-12.f, -4.f, 0.f);
	D->AimEyeRelief = 58.f;
	D->InspectOffset = FVector(32.f, 4.f, -14.f);
	D->InspectRotation = FRotator(5.f, -10.f, -35.f);
	D->AimTime = 0.25f;
	D->AimArc = FVector(3.f, 0.f, -1.f);
	D->SwayWeight = 1.f;

	D->RecoilPitchImpulse = 260.f;
	D->RecoilYawImpulse = 45.f;
	D->RecoilBackImpulse = 90.f;
	D->RecoilRiseImpulse = 25.f;
	D->RecoilStiffness = 260.f;
	D->RecoilDamping = 24.f;
	D->RecoilResidual = 0.04f;

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

	D->CaseEjectVelocity = FVector(60.f, 600.f, 150.f);
	D->CaseEjectRandom = 90.f;
	D->CaseSpinDegPerSec = 1400.f;
	D->LiveRoundEjectScale = 0.3f;
	D->CaseMassGrams = 6.6f;
	D->RoundMassGrams = 16.3f;

	D->ReadyOffset = FVector(30.f, 12.f, -22.f);
	D->ReadyRotation = FRotator(-15.f, -6.f, 0.f);
	D->AimEyeRelief = 52.f;
	D->InspectOffset = FVector(30.f, 6.f, -16.f);
	D->InspectRotation = FRotator(5.f, -15.f, -40.f);
	D->AimTime = 0.32f;
	D->AimArc = FVector(5.f, 0.f, -2.f);
	D->SwayWeight = 1.6f;

	D->RecoilPitchImpulse = 170.f;
	D->RecoilYawImpulse = 60.f;
	D->RecoilBackImpulse = 140.f;
	D->RecoilRiseImpulse = 18.f;
	D->RecoilStiffness = 200.f;
	D->RecoilDamping = 22.f;
	D->RecoilResidual = 0.06f;
}
