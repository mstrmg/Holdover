#include "Weapons/HoldoverMuzzleFX.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HoldoverDebug.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Holdover.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Weapons/HoldoverGunData.h"

namespace
{
	constexpr int32 NumFlashCards = 32;
	constexpr int32 NumSmokeCards = 28;

	// Texture atlas rows (4 x 4 frames, made by tools/fx/gen_fx_textures.py).
	constexpr int32 RowCore = 0;      // hot round core
	constexpr int32 RowPlume = 1;     // flame tongue, muzzle at the left edge
	constexpr int32 RowStar = 2;      // burst seen end-on, ragged petals
	constexpr int32 RowFireball = 3;  // billowing ball of burning gas

	float R(float A, float B) { return FMath::FRandRange(A, B); }
	int32 Frame(int32 Row) { return Row * 4 + FMath::RandRange(0, 3); }

	/** Roughly bell-shaped random number with the given spread (sum of three uniforms). */
	float Bell(float Spread)
	{
		return (FMath::FRand() + FMath::FRand() + FMath::FRand() - 1.5f) * Spread * 1.15f;
	}
}

UHoldoverMuzzleFX::UHoldoverMuzzleFX()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // after the camera has moved this frame
	bAutoActivate = true;
}

void UHoldoverMuzzleFX::BeginPlay()
{
	Super::BeginPlay();
	EnsurePool();
}

void UHoldoverMuzzleFX::EnsurePool()
{
	if (bPoolReady)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		return;
	}
	bPoolReady = true;

	FlashMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
		TEXT("/Game/FX/M_MuzzleFlash.M_MuzzleFlash"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	SmokeMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
		TEXT("/Game/FX/M_MuzzleSmoke.M_MuzzleSmoke"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	UStaticMesh* Plane = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr,
		TEXT("/Engine/BasicShapes/Plane.Plane"), nullptr, LOAD_NoWarn | LOAD_Quiet));

	if ((!FlashMaterial || !SmokeMaterial) && !bWarnedMissing)
	{
		bWarnedMissing = true;
		UE_LOG(LogHoldover, Warning, TEXT("Muzzle flash materials not found in /Game/FX (run tools/unreal/import_fx.py). Only the flash light will show."));
	}

	USceneComponent* Root = Owner->GetRootComponent();
	auto MakeCards = [&](TArray<FCard>& Out, int32 Count, UMaterialInterface* Mat, bool bSmoke)
	{
		if (!Mat || !Plane)
		{
			return;
		}
		for (int32 i = 0; i < Count; ++i)
		{
			UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
			Comp->SetStaticMesh(Plane);
			Comp->SetupAttachment(Root);
			Comp->SetUsingAbsoluteLocation(true);
			Comp->SetUsingAbsoluteRotation(true);
			Comp->SetUsingAbsoluteScale(true);
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Comp->SetCastShadow(false);
			Comp->bReceivesDecals = false;
			Comp->SetCanEverAffectNavigation(false);
			Comp->SetTranslucentSortPriority(bSmoke ? 0 : 2);
			Comp->SetVisibility(false);
			Comp->RegisterComponent();
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, Owner);
			Comp->SetMaterial(0, MID);
			PoolComponents.Add(Comp);
			PoolMaterials.Add(MID);
			FCard C;
			C.Comp = Comp;
			C.MID = MID;
			C.bSmoke = bSmoke;
			Out.Add(C);
		}
	};
	MakeCards(FlashCards, NumFlashCards, FlashMaterial, false);
	MakeCards(SmokeCards, NumSmokeCards, SmokeMaterial, true);

	Light = NewObject<UPointLightComponent>(Owner);
	Light->SetupAttachment(Root);
	Light->SetUsingAbsoluteLocation(true);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.f);
	Light->SetLightColor(LightColor);
	Light->SetAttenuationRadius(LightRadius);
	Light->SetCastShadows(false);
	Light->SetSourceRadius(4.f);
	Light->SetVisibility(false);
	Light->bAffectTranslucentLighting = false;
	Light->RegisterComponent();
}

UHoldoverMuzzleFX::FCard* UHoldoverMuzzleFX::Grab(bool bSmoke)
{
	TArray<FCard>& Pool = bSmoke ? SmokeCards : FlashCards;
	if (Pool.Num() == 0)
	{
		return nullptr;
	}
	for (FCard& C : Pool)
	{
		if (!C.bActive)
		{
			return &C;
		}
	}
	// All busy: smoke reuses its oldest puff; a flash card is never stolen mid-flash.
	if (bSmoke)
	{
		NextSmoke = (NextSmoke + 1) % Pool.Num();
		return &Pool[NextSmoke];
	}
	return nullptr;
}

void UHoldoverMuzzleFX::Spawn(const FCardSpec& S, bool bSmoke)
{
	FCard* C = Grab(bSmoke);
	if (!C)
	{
		return;
	}
	C->bActive = true;
	C->Mode = S.Mode;
	C->Age = 0.f;
	C->Life = FMath::Max(S.Life, 0.001f);
	C->Tau = FMath::Max(S.Tau, 0.001f);
	C->Delay = S.Delay;
	C->Start = S.Start;
	C->Velocity = S.Velocity;
	C->Drag = S.Drag;
	C->Rise = S.Rise;
	C->Axis = S.Axis.GetSafeNormal();
	C->Size0 = S.Size0;
	C->Size1 = S.Size1;
	C->Spin = S.Spin;
	C->SpinRate = S.SpinRate;
	C->Peak = S.Peak;
	C->Tint = S.Tint;
	C->Frame = static_cast<float>(S.Frame);
	C->bGunLocal = S.bGunLocal && GetOwner();
	if (C->bGunLocal)
	{
		const FTransform Gun = GetOwner()->GetActorTransform();
		C->Start = Gun.InverseTransformPosition(S.Start);
		C->Axis = Gun.InverseTransformVectorNoScale(S.Axis).GetSafeNormal();
		C->Velocity = Gun.InverseTransformVectorNoScale(S.Velocity);
	}
	if (C->MID)
	{
		C->MID->SetScalarParameterValue(TEXT("Frame"), C->Frame);
		C->MID->SetVectorParameterValue(bSmoke ? TEXT("Color") : TEXT("Tint"), C->Tint);
	}
}

void UHoldoverMuzzleFX::HideCard(FCard& C)
{
	C.bActive = false;
	if (C.Comp)
	{
		C.Comp->SetVisibility(false);
	}
}

float UHoldoverMuzzleFX::SunLux()
{
	if (CachedSunLux <= 0.f)
	{
		CachedSunLux = 10.f;
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			if (const ULightComponent* L = It->GetLightComponent())
			{
				CachedSunLux = FMath::Max(L->Intensity, 0.1f);
				break;
			}
		}
	}
	return CachedSunLux;
}

// =====================================================================
// One shot
// =====================================================================

void UHoldoverMuzzleFX::Fire(const UHoldoverGunData* Data, const FTransform& MuzzleWorld, const FVector& PortWorld)
{
	if (!Data)
	{
		return;
	}
	EnsurePool();

	// Every shot is different: a faint one that hardly leaves the barrel, a normal one, or one where the gas
	// mixes with the air out in front and lights up as a second, bigger fireball.
	EHoldoverFlashVariant Variant = EHoldoverFlashVariant::Normal;
	const int32 Forced = HoldoverDebug::FlashVariant();
	if (Forced >= 0 && Forced <= 2)
	{
		Variant = static_cast<EHoldoverFlashVariant>(Forced);
	}
	else if (FMath::FRand() < Data->FlashFaintChance)
	{
		Variant = EHoldoverFlashVariant::Faint;
	}
	else if (FMath::FRand() < Data->FlashFireballChance)
	{
		Variant = EHoldoverFlashVariant::Fireball;
	}
	LastVariant = Variant;

	FTransform Muzzle = MuzzleWorld;
	Muzzle.SetScale3D(FVector::OneVector);
	BuildFlash(Data, Muzzle, Variant);
	BuildSmoke(Data, Muzzle, PortWorld, Variant);
}

void UHoldoverMuzzleFX::BuildFlash(const UHoldoverGunData* Data, const FTransform& M, EHoldoverFlashVariant Variant)
{
	const FVector O = M.GetLocation();
	const FVector Fwd = M.GetUnitAxis(EAxis::X);
	const FVector Right = M.GetUnitAxis(EAxis::Y);
	const FVector Up = M.GetUnitAxis(EAxis::Z);
	auto Radial = [&](float AngleFromUpDeg) // round the bore: 0 = up, 90 = right
	{
		const float A = FMath::DegreesToRadians(AngleFromUpDeg);
		return Up * FMath::Cos(A) + Right * FMath::Sin(A);
	};

	const bool bFaint = (Variant == EHoldoverFlashVariant::Faint);
	float S = Data->FlashSize * R(0.75f, 1.3f);
	float L = Data->FlashLength * R(0.6f, 1.4f);
	float B = Data->FlashBrightness * R(0.7f, 1.3f) * CardBrightness;
	// Most of a flash is over in a millisecond; what the eye keeps is one frame, sometimes a dimmer second or third.
	const float Tau = R(0.005f, 0.016f);
	float Life = R(0.016f, 0.05f);
	if (bFaint)
	{
		S *= 0.45f;
		L *= 0.35f;
		B *= 0.35f;
		Life = FMath::Min(Life, 0.02f);
	}
	// Colour from yellow-white (hot) to deep orange (cooler, more unburnt powder).
	const float Warm = FMath::FRand();
	const FLinearColor Tint = FMath::Lerp(FLinearColor(1.f, 0.86f, 0.62f), FLinearColor(1.f, 0.56f, 0.28f), Warm);
	const FLinearColor Hot = FMath::Lerp(FLinearColor(1.f, 0.95f, 0.85f), Tint, 0.4f);

	auto Card = [&](ECardMode Mode, const FVector& Start, const FVector& Axis, const FVector2D& Size, float Grow,
		float Peak, int32 Row, const FLinearColor& Color, float LifeScale = 1.f, float TauScale = 1.f, float Delay = 0.f)
	{
		FCardSpec Spec;
		Spec.Mode = Mode;
		Spec.Start = Start;
		Spec.Axis = Axis;
		Spec.Size0 = Size;
		Spec.Size1 = Size * Grow;
		Spec.Life = Life * LifeScale;
		Spec.Tau = Tau * TauScale;
		Spec.Delay = Delay;
		Spec.Peak = Peak;
		Spec.Frame = Frame(Row);
		Spec.Spin = R(0.f, 360.f);
		Spec.Tint = Color;
		// Over in a few milliseconds, so it stays on the muzzle while the gun kicks; a fireball out in the air stays put.
		Spec.bGunLocal = (Row != RowFireball);
		Spawn(Spec, false);
	};
	auto Jet = [&](const FVector& Dir, float Length, float Width, float Peak)
	{
		// A tongue of flame that starts at the muzzle and points along Dir.
		Card(ECardMode::Axial, O + Fwd * (S * 0.1f), Dir, FVector2D(Length, Width), 1.2f, Peak, RowPlume, Tint);
	};

	// Hot core right at the crown, seen from any side.
	const float CoreScale = (Data->MuzzleDevice == EHoldoverMuzzleDevice::Birdcage) ? 0.8f : 1.f;
	Card(ECardMode::Facing, O + Fwd * (S * 0.2f), Fwd, FVector2D(S, S) * R(0.9f, 1.2f) * CoreScale, 1.15f, 10.f * B, RowCore, Hot);

	if (bFaint)
	{
		// Just a small hot glow and a short lick of flame.
		Jet((Fwd + FMath::VRand() * 0.08f).GetSafeNormal(), FMath::Max(L, S * 0.8f), S * 0.6f, 4.f * B);
	}
	else
	{
		// The burst seen end-on (what the shooter mostly sees looking down the barrel).
		Card(ECardMode::Facing, O + Fwd * (S * 0.45f), Fwd, FVector2D(S, S) * R(1.3f, 1.8f) * CoreScale, 1.2f, 5.f * B, RowStar, Tint);

		// The main plume along the bore.
		const float PlumePeak = (Data->MuzzleDevice == EHoldoverMuzzleDevice::Birdcage) ? 3.5f * B : 6.f * B;
		const FVector PlumeDir = (Fwd + FMath::VRand() * 0.05f).GetSafeNormal();
		Jet(PlumeDir, L, S * R(0.6f, 0.85f), PlumePeak);

		switch (Data->MuzzleDevice)
		{
		case EHoldoverMuzzleDevice::SlantBrake:
		{
			// The slant cut is longer on the lower left, so the gas spills up and to the right.
			const int32 N = FMath::RandRange(1, 3);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector Dir = (Fwd * R(0.35f, 0.7f) + Radial(R(25.f, 70.f)) * R(0.7f, 1.f)).GetSafeNormal();
				Jet(Dir, L * R(0.35f, 0.65f), S * R(0.4f, 0.6f), 5.5f * B);
			}
			// The gas it throws out the top-right side billows into a lopsided cloud of flame.
			Card(ECardMode::Facing, O + Fwd * (S * 0.5f) + Radial(R(30.f, 60.f)) * (S * R(0.35f, 0.6f)), Fwd,
				FVector2D(S, S) * R(0.9f, 1.4f), 1.25f, 4.f * B, RowStar, Tint);
			break;
		}
		case EHoldoverMuzzleDevice::Birdcage:
		{
			// Slots on the top and sides, closed underneath: a small star of short jets in the upper half.
			static const float Slots[] = { 0.f, 60.f, -60.f, 115.f, -115.f };
			for (float A : Slots)
			{
				if (FMath::FRand() < 0.7f)
				{
					const FVector Dir = (Fwd * 0.9f + Radial(A + R(-8.f, 8.f)) * 0.5f).GetSafeNormal();
					Jet(Dir, L * R(0.25f, 0.45f), S * R(0.18f, 0.28f), 4.5f * B);
				}
			}
			break;
		}
		case EHoldoverMuzzleDevice::SlottedHider:
		{
			// Thin jets between the long prongs, all the way round, opening like a cone.
			const int32 N = FMath::RandRange(4, 6);
			const float Phase = R(0.f, 360.f);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector Dir = (Fwd + Radial(Phase + 360.f * i / N) * R(0.25f, 0.45f)).GetSafeNormal();
				Jet(Dir, L * R(0.5f, 0.85f), S * R(0.16f, 0.24f), 4.5f * B);
			}
			break;
		}
		default:
		{
			// Bare muzzle: the gas balloons out evenly, with an odd uneven lick to one side.
			const int32 N = FMath::RandRange(0, 3);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector Dir = (Fwd * 0.8f + Radial(R(0.f, 360.f)) * R(0.4f, 0.7f)).GetSafeNormal();
				Jet(Dir, L * R(0.25f, 0.5f), S * R(0.3f, 0.45f), 4.5f * B);
			}
			break;
		}
		}

		if (Variant == EHoldoverFlashVariant::Fireball)
		{
			// Unburnt gas meets the air out in front and flares: bigger, redder, a moment later and a moment longer.
			const float D = L * R(0.6f, 1.6f) + S * 0.8f;
			const FLinearColor Fire = FMath::Lerp(Tint, FLinearColor(1.f, 0.42f, 0.16f), 0.6f);
			Card(ECardMode::Facing, O + Fwd * D + FMath::VRand() * S * 0.2f, Fwd, FVector2D(S, S) * R(1.5f, 2.6f), 1.3f,
				4.f * B, RowFireball, Fire, R(1.6f, 2.4f), R(1.8f, 2.8f), R(0.f, 0.008f));
			if (FMath::FRand() < 0.5f)
			{
				// Sometimes it tears into two lobes.
				Card(ECardMode::Facing, O + Fwd * (D * R(0.6f, 0.85f)) + FMath::VRand() * S * 0.5f, Fwd, FVector2D(S, S) * R(1.1f, 1.8f), 1.4f,
					3.f * B, RowFireball, Fire, R(1.3f, 2.f), R(1.5f, 2.4f), R(0.f, 0.006f));
			}
		}

		// Burning powder grains: a few short bright streaks flying out (more from rifles).
		const float SparkChance = Data->bIsPistol ? 0.3f : 0.55f;
		if (FMath::FRand() < SparkChance)
		{
			const int32 N = FMath::RandRange(2, Data->bIsPistol ? 4 : 7);
			for (int32 i = 0; i < N; ++i)
			{
				FCardSpec Spark;
				Spark.Mode = ECardMode::Axial;
				const FVector Dir = FMath::VRandCone(Fwd, FMath::DegreesToRadians(14.f));
				Spark.Axis = Dir;
				Spark.Start = O + Dir * R(1.f, 4.f);
				Spark.Velocity = Dir * R(1500.f, 4500.f);
				Spark.Size0 = FVector2D(R(2.5f, 7.f), R(0.2f, 0.35f));
				Spark.Size1 = Spark.Size0;
				Spark.Life = R(0.03f, 0.09f);
				Spark.Tau = R(0.03f, 0.06f);
				Spark.Peak = 6.f * B;
				Spark.Frame = Frame(RowCore);
				Spark.Tint = FLinearColor(1.f, 0.6f, 0.25f);
				Spawn(Spark, false);
			}
		}
	}

	// The flash lights up the hands, the gun and the ground for a frame.
	const float LightScale = bFaint ? 0.3f : (Variant == EHoldoverFlashVariant::Fireball ? 1.4f : 1.f);
	// FlashLightIntensity is how bright the flash makes the hands (half a metre away) compared with the sun, so the light
	// works the same on the gray test range and on a physically lit map: candela = fraction x sun lux x (0.5 m)^2.
	LightPeak = Data->FlashLightIntensity * SunLux() * 0.25f * Data->FlashBrightness * R(0.7f, 1.3f) * LightScale;
	LightTau = Tau * 1.3f;
	LightLife = Life + 0.01f;
	LightAge = 0.f;
	LightPos = GetOwner() ? GetOwner()->GetActorTransform().InverseTransformPosition(O + Fwd * (S * 0.6f)) : O;
}

void UHoldoverMuzzleFX::BuildSmoke(const UHoldoverGunData* Data, const FTransform& M, const FVector& Port, EHoldoverFlashVariant Variant)
{
	const float Amount = Data->SmokeAmount * (Variant == EHoldoverFlashVariant::Faint ? 0.7f : 1.f);
	if (Amount <= 0.f)
	{
		return;
	}
	const FVector O = M.GetLocation();
	const FVector Fwd = M.GetUnitAxis(EAxis::X);
	const FVector Right = M.GetUnitAxis(EAxis::Y);
	const float S = Data->FlashSize;
	const float L = Data->FlashLength;

	auto Puff = [&](const FVector& Start, const FVector& Vel, float Size0, float Size1, float Life, float Opacity, float Delay, float Drag)
	{
		FCardSpec Spec;
		Spec.Mode = ECardMode::Facing;
		Spec.Start = Start;
		Spec.Velocity = Vel;
		Spec.Drag = Drag;
		Spec.Rise = R(4.f, 10.f);
		Spec.Size0 = FVector2D(Size0, Size0);
		Spec.Size1 = FVector2D(Size1, Size1);
		Spec.Life = Life;
		Spec.Peak = Opacity;
		Spec.Delay = Delay;
		Spec.Frame = FMath::RandRange(0, 15);
		Spec.Spin = R(0.f, 360.f);
		Spec.SpinRate = R(-25.f, 25.f);
		const float Grey = R(0.5f, 0.72f);
		Spec.Tint = FLinearColor(Grey, Grey, Grey * 0.98f);
		Spawn(Spec, true);
	};

	// Smoke pushed out of the muzzle: it shoots forward, stalls in the air within a fraction of a second,
	// then swells, rises and drifts away on the breeze.
	const int32 N = FMath::Clamp(FMath::RoundToInt(R(2.f, 4.f) * Amount), 1, 8);
	for (int32 i = 0; i < N; ++i)
	{
		const FVector Start = O + Fwd * (R(0.2f, 0.9f) * L);
		const FVector Vel = Fwd * R(60.f, 260.f) + FMath::VRand() * R(10.f, 40.f) + FVector(0.f, 0.f, R(5.f, 20.f));
		Puff(Start, Vel, S * R(0.6f, 1.f), S * R(4.f, 8.f), R(1.2f, 2.6f), R(0.12f, 0.26f) * FMath::Min(Amount, 1.5f), R(0.f, 0.02f), R(2.5f, 4.5f));
	}

	// A thin wisp leaking out of the ejection port as the action opens.
	if (FMath::FRand() < 0.8f)
	{
		const FVector Vel = FVector(0.f, 0.f, R(10.f, 30.f)) + Right * R(5.f, 20.f);
		Puff(Port, Vel, 2.f, S * R(2.f, 3.5f), R(1.f, 2.f), 0.1f * FMath::Min(Amount, 1.5f), R(0.02f, 0.05f), 1.5f);
	}
}

// =====================================================================
// Every frame
// =====================================================================

void UHoldoverMuzzleFX::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FVector CameraLoc = GetOwner() ? GetOwner()->GetActorLocation() - GetOwner()->GetActorForwardVector() * 50.f : FVector::ZeroVector;
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		CameraLoc = Cam->GetCameraLocation();
	}
	const bool bFreeze = HoldoverDebug::FlashFreeze();

	for (FCard& C : FlashCards)
	{
		if (C.bActive)
		{
			UpdateCard(C, CameraLoc, DeltaTime, bFreeze);
		}
	}
	for (FCard& C : SmokeCards)
	{
		if (C.bActive)
		{
			UpdateCard(C, CameraLoc, DeltaTime, false);
		}
	}

	if (Light)
	{
		if (LightAge < LightLife && LightPeak > 0.f)
		{
			const float I = LightPeak * FMath::Exp(-LightAge / FMath::Max(LightTau, 0.001f));
			Light->SetWorldLocation(GetOwner() ? GetOwner()->GetActorTransform().TransformPosition(LightPos) : LightPos);
			Light->SetIntensity(I);
			Light->SetVisibility(true);
			if (!bFreeze)
			{
				LightAge += DeltaTime;
			}
		}
		else if (Light->IsVisible())
		{
			Light->SetVisibility(false);
			Light->SetIntensity(0.f);
		}
	}
}

void UHoldoverMuzzleFX::UpdateCard(FCard& C, const FVector& CameraLoc, float Dt, bool bFreeze)
{
	if (!C.Comp)
	{
		C.bActive = false;
		return;
	}
	if (bFreeze)
	{
		C.Delay = 0.f; // held for a picture: show the delayed parts (fireball) straight away
	}
	if (C.Delay > 0.f)
	{
		C.Comp->SetVisibility(false);
		C.Delay -= Dt;
		return;
	}
	const float T = C.Age;
	if (T >= C.Life)
	{
		HideCard(C);
		return;
	}
	const float F = FMath::Clamp(T / C.Life, 0.f, 1.f);

	FVector Pos;
	FVector2D Size;
	float Value;
	if (C.bSmoke)
	{
		// Pushed out, slowed by the air, then lifted and carried by the breeze.
		const float Travel = C.Drag > 0.f ? (1.f - FMath::Exp(-C.Drag * T)) / C.Drag : T;
		Pos = C.Start + C.Velocity * Travel + FVector(0.f, 0.f, 0.5f * C.Rise * T * T) + Wind * T;
		const float Grow = 1.f - FMath::Pow(1.f - F, 2.5f);
		Size = FMath::Lerp(C.Size0, C.Size1, Grow);
		const float FadeIn = FMath::Clamp(T / 0.06f, 0.f, 1.f);
		Value = C.Peak * FadeIn * FMath::Pow(1.f - F, 1.6f);
	}
	else
	{
		Pos = C.Start + C.Velocity * T;
		Size = FMath::Lerp(C.Size0, C.Size1, F);
		if (C.bGunLocal && GetOwner())
		{
			Pos = GetOwner()->GetActorTransform().TransformPosition(Pos);
		}
		Value = C.Peak * FMath::Exp(-T / C.Tau);
	}
	C.Spin += C.SpinRate * (bFreeze ? 0.f : Dt);

	FRotator Rot;
	FVector Center = Pos;
	if (C.Mode == ECardMode::Axial)
	{
		// Lies along its axis and turns about it to face the eye as much as it can (seen end-on it all but vanishes,
		// which is right: looking down the bore you see the burst, not the tongue).
		const FVector Axis = (C.bGunLocal && GetOwner()) ? GetOwner()->GetActorTransform().TransformVectorNoScale(C.Axis) : C.Axis;
		FVector ToCam = CameraLoc - Pos;
		FVector Normal = ToCam - Axis * FVector::DotProduct(ToCam, Axis);
		if (!Normal.Normalize())
		{
			Normal = FVector::UpVector;
		}
		Center = Pos + Axis * (Size.X * 0.5f);
		Rot = FRotationMatrix::MakeFromXZ(Axis, Normal).Rotator();
	}
	else
	{
		FVector ToCam = (CameraLoc - Pos).GetSafeNormal();
		if (ToCam.IsNearlyZero())
		{
			ToCam = FVector::UpVector;
		}
		const FVector Ref = FMath::Abs(ToCam.Z) > 0.95f ? FVector::ForwardVector : FVector::UpVector;
		const FVector Side = FVector::CrossProduct(Ref, ToCam).GetSafeNormal();
		const FVector X = Side.RotateAngleAxis(C.Spin, ToCam);
		Rot = FRotationMatrix::MakeFromZX(ToCam, X).Rotator();
	}

	C.Comp->SetWorldLocationAndRotation(Center, Rot);
	C.Comp->SetWorldScale3D(FVector(FMath::Max(Size.X, 0.05f) / 100.f, FMath::Max(Size.Y, 0.05f) / 100.f, 1.f));
	if (C.MID)
	{
		C.MID->SetScalarParameterValue(C.bSmoke ? TEXT("Opacity") : TEXT("Intensity"), Value);
	}
	C.Comp->SetVisibility(true);

	if (!bFreeze)
	{
		C.Age += Dt;
	}
}
