#include "Range/HoldoverRangeBuilder.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HoldoverShapes.h"
#include "Core/HoldoverTypes.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Range/HoldoverPaperTarget.h"

AHoldoverRangeBuilder::AHoldoverRangeBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

FTransform AHoldoverRangeBuilder::GetFiringPosition() const
{
	// Capsule centre about 1 m up; the character drops onto the ground.
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(FVector(-150.f, 0.f, 100.f)));
}

void AHoldoverRangeBuilder::BeginPlay()
{
	Super::BeginPlay();
	if (bSpawnLightingIfMissing)
	{
		SpawnLightingIfMissing();
	}
	BuildGeometry();
	SpawnTargets();
}

void AHoldoverRangeBuilder::EndPlay(const EEndPlayReason::Type Reason)
{
	for (AActor* A : SpawnedHelpers)
	{
		if (IsValid(A))
		{
			A->Destroy();
		}
	}
	for (AHoldoverPaperTarget* T : Targets)
	{
		if (IsValid(T))
		{
			T->Destroy();
		}
	}
	Super::EndPlay(Reason);
}

void AHoldoverRangeBuilder::BuildGeometry()
{
	UStaticMesh* Cube = HoldoverShapes::Cube();
	const float Len = RangeLength * 100.f;
	const float Wid = RangeWidth * 100.f;
	const float Behind = 1500.f; // ground behind the firing line

	auto Block = [&](const FVector& Loc, const FVector& Size, const FLinearColor& Color, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* C = HoldoverShapes::AddShape(this, Root, Cube, Loc, Rot, Size, Color, true);
		if (C)
		{
			C->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
		}
		return C;
	};

	// Ground slab, top at Z = 0.
	Block(FVector((Len - Behind) * 0.5f, 0.f, -25.f), FVector(Len + Behind + 1000.f, Wid + 2000.f, 50.f), HoldoverShapes::Ground);

	// Side walls (3 m high) along the lane.
	for (float Side : { -1.f, 1.f })
	{
		Block(FVector(Len * 0.5f, Side * (Wid * 0.5f + 50.f), 150.f), FVector(Len + 400.f, 100.f, 300.f), HoldoverShapes::Wall);
	}

	// Backstop berm: a tall earth wall with a sloped face.
	Block(FVector(Len + 400.f, 0.f, 400.f), FVector(800.f, Wid + 200.f, 800.f), HoldoverShapes::Berm);
	Block(FVector(Len - 150.f, 0.f, 120.f), FVector(500.f, Wid + 200.f, 400.f), HoldoverShapes::Berm, FRotator(-35.f, 0.f, 0.f));

	// Firing line: a white stripe across the lane at X = 0.
	HoldoverShapes::AddShape(this, Root, Cube, FVector(0.f, 0.f, 0.3f), FRotator::ZeroRotator, FVector(10.f, Wid, 0.6f), HoldoverShapes::White, false);

	// Shooting bench at the firing line, to the right of the shooter.
	Block(FVector(-30.f, 110.f, 88.f), FVector(70.f, 140.f, 5.f), HoldoverShapes::Wood);
	for (float Dx : { -55.f, -5.f })
	{
		for (float Dy : { 50.f, 170.f })
		{
			Block(FVector(Dx, Dy, 43.f), FVector(6.f, 6.f, 86.f), HoldoverShapes::Wood);
		}
	}

	// Distance posts on the left side of the lane, with a white band per 5 m.
	for (float D : TargetDistances)
	{
		const float X = D * 100.f;
		Block(FVector(X, -Wid * 0.5f + 60.f, 75.f), FVector(10.f, 10.f, 150.f), HoldoverShapes::Wood);
		const int32 Bands = FMath::Max(1, FMath::RoundToInt(D / 5.f));
		for (int32 b = 0; b < FMath::Min(Bands, 10); ++b)
		{
			HoldoverShapes::AddShape(this, Root, Cube, FVector(X - 5.5f, -Wid * 0.5f + 60.f, 140.f - b * 9.f),
				FRotator::ZeroRotator, FVector(1.f, 10.5f, 4.f), HoldoverShapes::White, false);
		}
	}
}

void AHoldoverRangeBuilder::SpawnTargets()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 Count = TargetDistances.Num();
	for (int32 i = 0; i < Count; ++i)
	{
		const float X = TargetDistances[i] * 100.f;
		// Spread targets across the lane so near ones do not hide far ones.
		const float Y = (i - (Count - 1) * 0.5f) * TargetSpacing;
		const FVector Loc = GetActorTransform().TransformPosition(FVector(X, Y, 0.f));
		// Target's +X faces the shooter.
		const FRotator Rot = (GetActorRotation().Quaternion() * FRotator(0.f, 180.f, 0.f).Quaternion()).Rotator();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AHoldoverPaperTarget* T = World->SpawnActor<AHoldoverPaperTarget>(AHoldoverPaperTarget::StaticClass(), Loc, Rot, Params))
		{
			Targets.Add(T);
		}
	}
}

void AHoldoverRangeBuilder::ResetTargets()
{
	for (AHoldoverPaperTarget* T : Targets)
	{
		if (IsValid(T))
		{
			T->ResetPaper();
		}
	}
}

void AHoldoverRangeBuilder::SpawnLightingIfMissing()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		return; // the level already has its own lighting
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Sun. The light shines along its +X, so pitch it down by the elevation.
	const FRotator SunRot(-SunElevation, SunAzimuth, 0.f);
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 1000.f), SunRot, Params))
	{
		Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			L->SetIntensity(10.f); // lux; auto exposure adapts
			L->SetAtmosphereSunLight(true);
			L->SetDynamicShadowCascades(4);
		}
		SpawnedHelpers.Add(Sun);
	}

	if (ASkyAtmosphere* Sky = World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		SpawnedHelpers.Add(Sky);
	}

	if (ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator, Params))
	{
		SkyLight->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		if (USkyLightComponent* C = SkyLight->GetLightComponent())
		{
			C->bRealTimeCapture = true;
			C->SetIntensity(1.f);
			C->MarkRenderStateDirty();
			C->RecaptureSky();
		}
		SpawnedHelpers.Add(SkyLight);
	}

	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		if (UExponentialHeightFogComponent* F = Fog->GetComponent())
		{
			F->SetFogDensity(0.01f);
			F->SetVolumetricFog(true);
		}
		SpawnedHelpers.Add(Fog);
	}
}
