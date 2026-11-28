#include "Range/HoldoverWoodsRange.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/HoldoverDebug.h"
#include "Core/HoldoverShapes.h"
#include "Core/HoldoverTypes.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ProceduralMeshComponent.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace Woods
{
	/** Smooth wave in [-1, 1] from three sines. */
	float Wobble(float T, float Phase)
	{
		return 0.55f * FMath::Sin(T * 0.083f + Phase) + 0.30f * FMath::Sin(T * 0.197f + Phase * 2.3f)
			+ 0.15f * FMath::Sin(T * 0.471f + Phase * 3.7f);
	}

	float Perlin(float X, float Y)
	{
		return FMath::PerlinNoise2D(FVector2D(X, Y));
	}

	float SegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const float Len2 = FMath::Max(static_cast<float>(AB.SizeSquared()), 1e-4f);
		const float T = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(P - A, AB)) / Len2, 0.f, 1.f);
		return static_cast<float>(FVector2D::Distance(P, A + AB * T));
	}

	// Footpath from the car park (off the map, behind-left) to the firing line, metres.
	const FVector2D AccessPath[] = {
		FVector2D(-7.f, -7.5f), FVector2D(-11.f, -9.f), FVector2D(-18.f, -10.5f), FVector2D(-27.f, -14.f),
		FVector2D(-38.f, -18.5f), FVector2D(-52.f, -19.f), FVector2D(-66.f, -16.f), FVector2D(-82.f, -21.f), FVector2D(-100.f, -26.f) };

	// The shooter's eye on the firing line, where lines of fire start (metres).
	constexpr float EyeX = -1.5f;

	// Spots for holdover.RangeShots: metres, eye height above ground, pitch, yaw. 07 is framed like Miles's photo.
	struct FShot { const TCHAR* Name; float X, Y, Up, Pitch, Yaw; bool bWalkway; };
	const FShot Shots[] = {
		{ TEXT("01_firing_line"), -1.5f, 0.f, 1.65f, -1.f, 0.f, false },
		{ TEXT("02_left"), -1.5f, 0.f, 1.65f, -3.f, -42.f, false },
		{ TEXT("03_right"), -1.5f, 0.f, 1.65f, -2.f, 48.f, false },
		{ TEXT("04_bench"), -3.2f, -1.6f, 1.65f, -16.f, 28.f, false },
		{ TEXT("05_walkway_back"), 95.f, 0.f, 1.65f, -2.f, 180.f, true },
		{ TEXT("06_backstop"), 172.f, -2.f, 1.65f, 3.f, 4.f, false },
		{ TEXT("07_reference"), 42.f, -26.f, 1.65f, 6.f, -95.f, false },
		{ TEXT("08_forest_right"), 85.f, 27.f, 1.65f, 4.f, 70.f, false },
		{ TEXT("09_path_arrival"), -22.f, -13.f, 1.65f, -2.f, 20.f, false },
		{ TEXT("10_overview"), -10.f, 0.f, 18.f, -20.f, 0.f, false },
		{ TEXT("11_lane_edge"), 32.f, -6.f, 1.65f, -6.f, -65.f, false },
		{ TEXT("12_canopy_up"), 60.f, -40.f, 1.65f, 58.f, 20.f, false },
	};

	const int32 SignDistances[] = { 5, 10, 25, 50, 75, 100, 150 };
}

using namespace Woods;

AHoldoverWoodsRange::AHoldoverWoodsRange()
{
	Root->SetMobility(EComponentMobility::Static);
	TargetDistances = { 5.f, 7.f, 10.f, 15.f, 25.f, 50.f, 75.f, 100.f, 150.f };
	PrimaryActorTick.bCanEverTick = true; // only for holdover.RangeShots
	PrimaryActorTick.TickInterval = 0.1f;
}

void AHoldoverWoodsRange::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateRangeShots(DeltaSeconds);
}

// ===========================================================================
// Layout

float AHoldoverWoodsRange::Noise(float X, float Y, float Scale, float Off) const
{
	const float S = static_cast<float>(Seed % 997) * 0.37f;
	return FMath::Clamp(0.5f + 0.5f * Perlin(X / Scale + Off + S, Y / Scale - Off * 1.7f - S), 0.f, 1.f);
}

float AHoldoverWoodsRange::LaneEdge(float X, float Y) const
{
	const float Ph = Y >= 0.f ? 1.3f : 4.7f;
	float E = LaneHalfWidth + LaneWander * Wobble(X * 1.7f, Ph) + 1.1f * Wobble(X * 4.3f, Ph + 2.f);
	E = FMath::Max(E, 7.4f);
	// Near the firing line the clearing is as wide as the pad and its shelter.
	return FMath::Lerp(9.f + 0.6f * Wobble(X * 5.f, Ph), E, FMath::SmoothStep(2.f, 10.f, X));
}

float AHoldoverWoodsRange::Backstop(float Y) const
{
	return BackstopToe + 1.6f * Wobble(Y * 2.f, 6.1f);
}

float AHoldoverWoodsRange::TargetY(int32 Index) const
{
	return (Index - (TargetDistances.Num() - 1) * 0.5f) * TargetSpacing / 100.f;
}

float AHoldoverWoodsRange::WalkwayY(float X) const
{
	return -8.f + 1.3f * Wobble(X * 2.6f, 0.8f) + 0.8f * FMath::SmoothStep(120.f, 185.f, X);
}

bool AHoldoverWoodsRange::OnWalkway(float X, float Y, float Extra) const
{
	if (X < 1.f || X > Backstop(Y) - 1.f)
	{
		return false;
	}
	return FMath::Abs(Y - WalkwayY(X)) < 0.55f + Extra;
}

float AHoldoverWoodsRange::PathDistance(float X, float Y) const
{
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 0; i + 1 < UE_ARRAY_COUNT(AccessPath); ++i)
	{
		Best = FMath::Min(Best, SegmentDistance(FVector2D(X, Y), AccessPath[i], AccessPath[i + 1]));
	}
	return Best;
}

float AHoldoverWoodsRange::FireDistance(float X, float Y) const
{
	float Best = BIG_NUMBER;
	for (int32 i = 0; i < TargetDistances.Num(); ++i)
	{
		const float D = TargetDistances[i];
		const float End = D + 3.f;
		const float XC = FMath::Min(X, End);
		const float Side = FMath::Abs(Y - TargetY(i) * (XC - EyeX) / (D - EyeX));
		Best = FMath::Min(Best, FMath::Sqrt(Side * Side + FMath::Square(FMath::Max(X - End, 0.f))));
	}
	return Best;
}

float AHoldoverWoodsRange::SignY(float D) const
{
	// By the left edge, but never where a nearer target hides it from the bench.
	float SY = -LaneEdge(D, -1.f) + 1.2f;
	for (int32 i = 0; i < TargetDistances.Num(); ++i)
	{
		const float TD = TargetDistances[i];
		if (TD >= D - 1.f)
		{
			continue;
		}
		const float K = (TD - EyeX) / (D - EyeX);
		if (FMath::Abs(SY * K - TargetY(i)) < 1.2f)
		{
			SY = FMath::Min(SY, (TargetY(i) - 1.2f) / K);
		}
	}
	return SY;
}

float AHoldoverWoodsRange::LaneOpen(float X, float Y) const
{
	const float AY = FMath::Abs(Y);
	// A ragged edge: the boundary itself is pushed in and out by noise, then softened over a few metres.
	const float Rag = (Noise(X, Y, 3.5f, 2.2f) - 0.5f) * 3.f + (Noise(X, Y, 1.2f, 7.f) - 0.5f) * 1.f;
	float Open = 1.f - FMath::SmoothStep(-1.5f, 2.f, AY - LaneEdge(X, Y) + Rag);
	// Lengthwise: the clearing behind the pad (benches, path arrival) to the backstop.
	const float Behind = FMath::SmoothStep(-16.f + 2.f * Wobble(Y, 3.f), -12.f, X);
	Open *= Behind;
	return Open;
}

bool AHoldoverWoodsRange::InClearing(float X, float Y, float Margin) const
{
	if (X > -14.f - Margin && X < Backstop(Y) + 4.f + Margin && FMath::Abs(Y) < LaneEdge(X, Y) + Margin)
	{
		return true;
	}
	return PathDistance(X, Y) < 1.6f + Margin;
}

bool AHoldoverWoodsRange::KeepClear(float X, float Y, float Margin) const
{
	const float AY = FMath::Abs(Y);
	if (X > -9.f - Margin && X < 1.5f + Margin && AY < 9.5f + Margin)
	{
		return true; // the pad and its shelter
	}
	if (PathDistance(X, Y) < 1.f + Margin * 0.5f)
	{
		return true;
	}
	if (OnWalkway(X, Y, FMath::Min(Margin, 0.8f)))
	{
		return true;
	}
	if (X > Backstop(Y) - 3.f - Margin * 0.5f && X < Backstop(Y) + 6.5f && AY < LaneEdge(X, Y) + 2.f)
	{
		return true; // the backstop, where the bullets land
	}
	// The backstop stays in view from the bench behind the far targets.
	if (X > 55.f && AY < 1.4f + 0.05f * (X - EyeX) + Margin * 0.3f && X < Backstop(Y))
	{
		return true;
	}
	for (const int32 SD : SignDistances)
	{
		const float SX = static_cast<float>(SD);
		const float SY = SignY(SX);
		if (FVector2D::Distance(FVector2D(X, Y), FVector2D(SX, SY)) < 1.f + Margin * 0.5f)
		{
			return true;
		}
		if (X > 1.f && X < SX && FMath::Abs(Y - SY * (X - EyeX) / (SX - EyeX)) < 0.35f + Margin * 0.4f)
		{
			return true;
		}
	}
	return X < 160.f && FireDistance(X, Y) < Margin + 0.7f;
}

bool AHoldoverWoodsRange::NearShotSpot(float X, float Y, float Radius, float Cone) const
{
	for (const FShot& S : Shots)
	{
		const FVector2D Spot(S.X, S.bWalkway ? WalkwayY(S.X) : S.Y);
		const FVector2D To = FVector2D(X, Y) - Spot;
		const float D = static_cast<float>(To.Size());
		if (D < Radius || (S.Up > 3.f && D < 7.f))
		{
			return true;
		}
		if (D < Cone && S.Pitch < 20.f && S.Up < 3.f)
		{
			const float Rad = FMath::DegreesToRadians(S.Yaw);
			if (FVector2D::DotProduct(To, FVector2D(FMath::Cos(Rad), FMath::Sin(Rad))) > D * 0.9f)
			{
				return true;
			}
		}
	}
	return false;
}

float AHoldoverWoodsRange::Valley(float X, float Y) const
{
	const float T = FMath::Clamp((X - 5.f) / (BackstopToe - 5.f), 0.f, 1.f);
	const float Far = FarRise * FMath::Pow(T, 1.6f) + FarRise * 0.15f * FMath::Max(X - BackstopToe, 0.f) / 20.f * (X < BackstopToe + 20.f ? 1.f : 0.f);
	const float Beyond = BeyondRise * FMath::SmoothStep(BackstopToe + 6.f, BackstopToe + 80.f, X) + (X > BackstopToe + 20.f ? FarRise * 0.15f : 0.f);
	const float Behind = 3.f * FMath::SmoothStep(15.f, 85.f, -X);
	const float Out = FMath::Max(FMath::Abs(Y) - LaneEdge(X, Y) - 1.f, 0.f);
	const float Side = SideRise * (1.f + 0.35f * Wobble(X * 0.9f, Y >= 0.f ? 1.4f : 3.6f)) * FMath::SmoothStep(0.f, SideRiseRun, Out);
	return Far + Beyond + Behind + Side;
}

float AHoldoverWoodsRange::BackstopHeightAt(float X, float Y) const
{
	const float T = Backstop(Y);
	const float D = X - T;
	if (D <= 0.f || D > 16.f)
	{
		return 0.f;
	}
	const float Face = FMath::SmoothStep(0.f, 5.5f, D);            // about 45 degrees
	const float Back = 1.f - FMath::SmoothStep(8.f, 16.f, D);       // long back slope into the trees
	float H = BackstopHeight * FMath::Min(Face, Back);
	// Rounded, eroded: it fades into the hillside at the sides.
	H *= 1.f - FMath::SmoothStep(LaneEdge(X, Y) + 3.f, LaneEdge(X, Y) + 13.f, FMath::Abs(Y));
	H += 0.35f * (Noise(X, Y, 2.5f, 9.f) - 0.5f) * FMath::Min(Face, Back);
	return FMath::Max(H, 0.f);
}

float AHoldoverWoodsRange::GroundHeightM(float X, float Y) const
{
	const float Open = LaneOpen(X, Y);
	const float Forest = 1.f - Open;
	const float NearPath = FMath::SmoothStep(1.f, 5.f, PathDistance(X, Y));

	float H = Valley(X, Y);

	// Hills and hollows in the forest, and a rough, root-bumped floor.
	H += HillHeight * (0.65f * Perlin(X / 52.f + 3.1f, Y / 52.f - 0.7f) + 0.35f * Perlin(X / 23.f - 5.3f, Y / 23.f + 1.7f))
		* FMath::SmoothStep(0.f, 1.f, Forest * 1.4f) * FMath::Lerp(0.5f, 1.f, NearPath);
	H += (0.40f * Perlin(X / 8.f + 1.9f, Y / 8.f - 3.3f) + 0.13f * Perlin(X / 2.9f - 7.7f, Y / 2.9f + 2.2f)
		+ 0.05f * Perlin(X / 1.1f + 4.4f, Y / 1.1f - 6.1f)) * Forest * FMath::Lerp(0.3f, 1.f, NearPath);

	// The lane: gentle swells everywhere, low mounds away from the lines of fire and the walkway.
	if (X > 2.f)
	{
		const float Mound = FMath::SmoothStep(4.f, 10.f, X) * FMath::SmoothStep(1.6f, 5.f, FireDistance(X, Y))
			* (1.f - FMath::SmoothStep(Backstop(Y) - 10.f, Backstop(Y) - 4.f, X))
			* FMath::SmoothStep(0.8f, 3.5f, FMath::Abs(Y - WalkwayY(X)));
		const float M = 0.6f * Perlin(X / 9.f - 1.3f, Y / 9.f + 4.9f) + 0.4f * Perlin(X / 16.f + 8.2f, Y / 16.f - 2.6f);
		H += Open * (LaneMound * 1.3f * (M > 0.f ? M : 0.3f * M) * Mound + 0.18f * Perlin(X / 21.f - 6.6f, Y / 21.f + 3.3f) * FMath::SmoothStep(3.f, 12.f, X)
			+ 0.03f * Perlin(X / 1.5f, Y / 1.5f));
	}

	H += BackstopHeightAt(X, Y);

	// The firing pad and the ground under its shelter are levelled.
	const float PadFlat = (1.f - FMath::SmoothStep(1.5f, 4.f, X)) * FMath::SmoothStep(-12.f, -9.f, X)
		* (1.f - FMath::SmoothStep(9.f, 11.5f, FMath::Abs(Y)));
	H = FMath::Lerp(H, 0.f, PadFlat);
	return H;
}

FLinearColor AHoldoverWoodsRange::SurfaceWeights(float X, float Y) const
{
	const float AY = FMath::Abs(Y);
	const float Open = LaneOpen(X, Y);
	float WGrass = 0.f, WSoil = 0.f, WGravel = 0.f, WMoss = 0.f;

	// WGravel pad under the shelter, with a ragged edge where the gravel thins into the ground.
	const float PadFront = 1.3f + 0.35f * Wobble(Y * 3.f, 1.9f);
	const float PadBack = -6.8f + 0.5f * Wobble(Y * 2.f, 3.3f);
	const float PadSide = 8.f + 0.5f * Wobble(X * 4.f, 0.4f);
	const float Ragged = (Noise(X, Y, 0.9f, 5.5f) - 0.5f) * 0.9f;
	const float InPad = FMath::Min3(PadFront - X, X - PadBack, PadSide - AY) + Ragged;
	WGravel = FMath::SmoothStep(-0.2f, 0.5f, InPad);

	// WGrass in the lane and the clearing behind the pad; leaf litter drifts in from the forest in patches.
	if (Open > 0.f)
	{
		const float Patches = FMath::SmoothStep(0.62f, 0.78f, Noise(X, Y, 5.5f, 1.3f)) * FMath::SmoothStep(4.f, 10.f, X);
		WGrass = Open * (0.92f - 0.65f * Patches) * (0.85f + 0.15f * Noise(X, Y, 1.7f, 8.f));
	}

	// Worn soil: the walkway, a trodden trail out to each target, the access path, around the pad, and the
	// backstop face where bullets churn the earth.
	if (X > 0.f && X < Backstop(Y))
	{
		const float Worn = 0.55f + 0.45f * Wobble(X * 3.4f, 2.f);
		WSoil = FMath::Max(WSoil, Worn * (1.f - FMath::SmoothStep(0.25f, 0.85f, FMath::Abs(Y - WalkwayY(X)))));
		for (int32 i = 0; i < TargetDistances.Num(); ++i)
		{
			const float D = TargetDistances[i];
			const FVector2D From(D - 1.5f, WalkwayY(D - 1.5f));
			const float Dist = SegmentDistance(FVector2D(X, Y), From, FVector2D(D - 0.4f, TargetY(i)));
			WSoil = FMath::Max(WSoil, 0.5f * (1.f - FMath::SmoothStep(0.15f, 0.55f, Dist + (Noise(X, Y, 0.7f, 3.f) - 0.5f) * 0.3f)));
		}
	}
	WSoil = FMath::Max(WSoil, 0.9f * (1.f - FMath::SmoothStep(0.5f, 1.4f, PathDistance(X, Y) + (Noise(X, Y, 1.f, 4.f) - 0.5f) * 0.6f)));
	WSoil = FMath::Max(WSoil, 0.55f * (1.f - FMath::SmoothStep(-0.5f, 1.5f, -InPad)) * (1.f - WGravel));
	const float Berm = BackstopHeightAt(X, Y);
	if (Berm > 0.05f && X < Backstop(Y) + 7.5f)
	{
		WSoil = FMath::Max(WSoil, FMath::SmoothStep(0.05f, 0.8f, Berm) * (0.8f + 0.2f * Noise(X, Y, 1.5f, 6.f)));
		WGrass *= 0.3f;
	}

	// WMoss in the damp forest: patches, more on the low ground.
	const float Forest = 1.f - Open;
	WMoss = Forest * FMath::SmoothStep(0.58f, 0.74f, Noise(X, Y, 6.f, 4.4f)) * 0.85f;

	WGrass = FMath::Clamp(WGrass * (1.f - WSoil) * (1.f - WGravel), 0.f, 1.f);
	return FLinearColor(WGrass, FMath::Clamp(WSoil * (1.f - WGravel), 0.f, 1.f), WGravel, WMoss * (1.f - WGravel) * (1.f - WSoil));
}

bool AHoldoverWoodsRange::IsPad(float X, float Y) const
{
	return SurfaceWeights(X, Y).B > 0.5f;
}

FVector AHoldoverWoodsRange::At(float X, float Y, float Sink) const
{
	return FVector(X * 100.f, Y * 100.f, GroundHeight(X * 100.f, Y * 100.f) - Sink);
}

FVector AHoldoverWoodsRange::AtLow(float X, float Y, float R, float Sink) const
{
	float Low = GroundHeightM(X, Y);
	for (int32 k = 0; k < 6; ++k)
	{
		const float A = k * (UE_TWO_PI / 6.f);
		Low = FMath::Min(Low, GroundHeightM(X + R * FMath::Cos(A), Y + R * FMath::Sin(A)));
	}
	return FVector(X * 100.f, Y * 100.f, Low * 100.f - Sink);
}

// ===========================================================================
// Build

void AHoldoverWoodsRange::BuildGeometry()
{
	const double Start = FPlatformTime::Seconds();

	DirtPhys = NewObject<UPhysicalMaterial>(this);
	DirtPhys->SurfaceType = SurfaceType4; // "Dirt"
	GravelPhys = NewObject<UPhysicalMaterial>(this);
	GravelPhys->SurfaceType = SurfaceType5; // "Gravel"

	FindAssets();
	BuildGround();
	PlaceRangeProps();

	// Each layer has its own stream, so changing one density doesn't reshuffle the others.
	FRandomStream R1(Seed), R2(Seed + 11), R3(Seed + 22), R4(Seed + 33), R5(Seed + 44), R6(Seed + 55), R7(Seed + 66), R8(Seed + 77);
	ScatterRocks(R7);
	ScatterDeadWood(R8);
	FRandomStream R9(Seed + 88);
	ScatterTwigs(R9);
	FRandomStream R10(Seed + 99);
	DressLaneEdges(R10);
	GrowCanopy(R1);
	GrowYoungTrees(R2);
	GrowSaplings(R3);
	GrowFerns(R4);
	GrowGroundPlants(R5);
	GrowGrass(R6);
	Flush();

	int32 Instances = 0;
	for (const auto& Pair : Batches)
	{
		Instances += Pair.Value ? Pair.Value->GetInstanceCount() : 0;
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods range built in %.2f s: %d static pieces in %d batches, %d Megaplants in %d components."),
		FPlatformTime::Seconds() - Start, Instances, Batches.Num(), SkinnedCount, SkinnedComponents.Num());
}

// ---------------------------------------------------------------------------
// Ground

void AHoldoverWoodsRange::BuildGround()
{
	TArray<UStaticMesh*> Tiles;
	bool bAll = true;
	for (int32 I = 0; I < TilesX && bAll; ++I)
	{
		for (int32 J = 0; J < TilesY; ++J)
		{
			UStaticMesh* M = LoadMesh(GroundFolder() / GroundTileName(I, J));
			if (!M)
			{
				bAll = false;
				break;
			}
			Tiles.Add(M);
		}
	}
	UStaticMesh* Pad = LoadMesh(GroundFolder() / GroundPadName());
	if (!bAll || !Pad)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Holdover] Woods ground tiles v%d not baked (run tools/unreal/build_woods_ground.py); using a generated ground."), GroundVersion);
		BuildGeneratedGround();
	}
	else
	{
		Tiles.Add(Pad);
		for (UStaticMesh* M : Tiles)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetStaticMesh(M);
			C->SetMobility(EComponentMobility::Static);
			C->SetupAttachment(Root);
			C->SetCollisionProfileName(TEXT("BlockAll"));
			C->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
			C->SetPhysMaterialOverride(M == Pad ? GravelPhys : DirtPhys);
			C->RegisterComponent();
			AddInstanceComponent(C);
			GroundParts.Add(C);
		}
	}

	// A floor far below the edges, so the world never ends in a void.
	if (UStaticMeshComponent* Skirt = HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cube(),
		FVector(9600.f, 0.f, -300.f), FRotator::ZeroRotator, FVector(200000.f, 200000.f, 100.f), FLinearColor(0.03f, 0.03f, 0.02f), true))
	{
		Skirt->SetCastShadow(false);
	}
}

void AHoldoverWoodsRange::BuildGeneratedGround()
{
	UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, TEXT("WoodsGround"));
	PM->SetMobility(EComponentMobility::Static);
	PM->SetupAttachment(Root);
	PM->bUseComplexAsSimpleCollision = true;
	PM->bUseAsyncCooking = false;
	PM->SetCollisionProfileName(TEXT("BlockAll"));
	PM->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
	PM->SetPhysMaterialOverride(DirtPhys);
	PM->RegisterComponent();
	AddInstanceComponent(PM);
	GroundParts.Add(PM);

	constexpr float Cell = 1.f;
	const float X0 = GroundX0, X1 = GroundX0 + TilesX * TileSize, Y0 = GroundY0, Y1 = GroundY0 + TilesY * TileSize;
	const int32 NX = FMath::RoundToInt((X1 - X0) / Cell), NY = FMath::RoundToInt((Y1 - Y0) / Cell);
	UMaterialInterface* Mat = LoadMaterial(GroundMaterialPath());
	for (int32 Strip = 0; Strip < NX; Strip += 64)
	{
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> Col;
		const int32 SX = FMath::Min(64, NX - Strip);
		for (int32 i = 0; i <= SX; ++i)
		{
			for (int32 j = 0; j <= NY; ++j)
			{
				const float X = X0 + (Strip + i) * Cell, Y = Y0 + j * Cell;
				const float Hx = GroundHeightM(X + 0.25f, Y) - GroundHeightM(X - 0.25f, Y);
				const float Hy = GroundHeightM(X, Y + 0.25f) - GroundHeightM(X, Y - 0.25f);
				V.Add(FVector(X * 100.f, Y * 100.f, GroundHeightM(X, Y) * 100.f));
				N.Add(FVector(-Hx, -Hy, 0.5f).GetSafeNormal());
				UV.Add(FVector2D(X, Y));
				Col.Add(SurfaceWeights(X, Y));
			}
		}
		for (int32 i = 0; i < SX; ++i)
		{
			for (int32 j = 0; j < NY; ++j)
			{
				const int32 A = i * (NY + 1) + j, B = (i + 1) * (NY + 1) + j;
				T.Append({ A, B, B + 1, A, B + 1, A + 1 });
			}
		}
		const int32 Section = Strip / 64;
		PM->CreateMeshSection_LinearColor(Section, V, T, N, UV, Col, TArray<FProcMeshTangent>(), true);
		if (Mat)
		{
			PM->SetMaterial(Section, Mat);
		}
	}
}

// ---------------------------------------------------------------------------
// Assets

UStaticMesh* AHoldoverWoodsRange::LoadMesh(const FString& Path) const
{
	FString P = Path;
	if (!P.Contains(TEXT(".")))
	{
		P += TEXT(".") + FPaths::GetBaseFilename(Path);
	}
	return Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet));
}

USkeletalMesh* AHoldoverWoodsRange::LoadSkinned(const FString& Path) const
{
	const FString P = Path + TEXT(".") + FPaths::GetBaseFilename(Path);
	return Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet));
}

UMaterialInterface* AHoldoverWoodsRange::LoadMaterial(const FString& Path) const
{
	return Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
}

TArray<UStaticMesh*> AHoldoverWoodsRange::MeshesUnder(const FString& Folder) const
{
	TArray<UStaticMesh*> Out;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ Folder }, false);
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*Folder));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	TArray<FAssetData> Found;
	Registry.GetAssets(Filter, Found);
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
	for (const FAssetData& A : Found)
	{
		if (UStaticMesh* M = Cast<UStaticMesh>(A.GetAsset()))
		{
			Out.Add(M);
		}
	}
	return Out;
}

void AHoldoverWoodsRange::FindAssets()
{
	const FString MP = TEXT("/Game/Megaplant_Library");
	const FString PH = TEXT("/Game/Range/Woods/PolyHaven");
	auto Skinned = [&](FMeshList& L, const FString& Path, int32 Weight = 1)
	{
		if (USkeletalMesh* M = LoadSkinned(MP / Path))
		{
			for (int32 w = 0; w < Weight; ++w) { L.Skinned.Add(M); }
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Holdover] Woods: missing %s"), *Path);
		}
	};
	auto Static = [&](FMeshList& L, const FString& Path, int32 Weight = 1)
	{
		if (UStaticMesh* M = LoadMesh(Path))
		{
			for (int32 w = 0; w < Weight; ++w) { L.Static.Add(M); }
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Holdover] Woods: missing %s"), *Path);
		}
	};
	auto Folder = [&](FMeshList& L, const FString& Path, const TCHAR* Skip = nullptr)
	{
		for (UStaticMesh* M : MeshesUnder(Path))
		{
			if (!Skip || !M->GetName().Contains(Skip))
			{
				L.Static.Add(M);
			}
		}
	};

	// Tall forest trees: forest-grown oaks and beeches with long bare trunks, the narrower hornbeams.
	// Weighted toward the broad-crowned beeches, so the canopy closes overhead as in the reference photo.
	Skinned(Canopy, TEXT("Tree_English_Oak/Tree_English_Oak_Forest_01/Tree_English_Oak_Forest_01_A"), 2);
	Skinned(Canopy, TEXT("Tree_English_Oak/Tree_English_Oak_Forest_01/Tree_English_Oak_Forest_01_B"), 2);
	Skinned(Canopy, TEXT("Tree_English_Oak/Tree_English_Oak_Forest_01/Tree_English_Oak_Forest_01_C"), 1);
	Skinned(Canopy, TEXT("Tree_European_Beech/Tree_European_Beech_01/Tree_European_Beech_01_A"), 4);
	Skinned(Canopy, TEXT("Tree_European_Beech/Tree_European_Beech_01/Tree_European_Beech_01_B"), 3);
	Skinned(Canopy, TEXT("Tree_Hornbeam/Tree_Hornbeam_01/Tree_Hornbeam_01_A"), 1);
	Skinned(Canopy, TEXT("Tree_Hornbeam/Tree_Hornbeam_01/Tree_Hornbeam_01_C"), 1);
	Skinned(Canopy, TEXT("Tree_Hornbeam/Tree_Hornbeam_01/Tree_Hornbeam_01_D"), 1);
	// Wide-crowned trees that only grow where they get light: at the edge of the clearing.
	Skinned(EdgeTrees, TEXT("Tree_Hornbeam/Tree_Hornbeam_01/Tree_Hornbeam_01_A"));
	Skinned(EdgeTrees, TEXT("Tree_Hornbeam/Tree_Hornbeam_01/Tree_Hornbeam_01_B"));
	Skinned(EdgeTrees, TEXT("Tree_European_Beech/Tree_European_Beech_01/Tree_European_Beech_01_B"));
	// Young trees of the same kinds under the big ones.
	Skinned(YoungTrees, TEXT("Tree_European_Beech/Tree_European_Beech_01/Tree_European_Beech_01_C"), 3);
	Skinned(YoungTrees, TEXT("Tree_European_Beech/Tree_European_Beech_01/Tree_European_Beech_01_D"), 3);
	Skinned(YoungTrees, TEXT("Tree_English_Oak/Tree_English_Oak_Forest_01/Tree_English_Oak_Forest_01_D"), 2);
	Skinned(YoungTrees, TEXT("Tree_Hornbeam/Tree_Hornbeam_01/Tree_Hornbeam_01_C"), 2);
	Skinned(YoungTrees, TEXT("Tree_Hornbeam/Tree_Hornbeam_01/Tree_Hornbeam_01_D"), 2);
	for (const TCHAR* E : { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D") })
	{
		Skinned(Elders, FString(TEXT("Tree_Elder/Tree_Elder_01/Tree_Elder_01_")) + E);
		Skinned(FernPlants, FString(TEXT("Plant_Bracken_Fern/Plant_Bracken_Fern_01/Plant_Bracken_Fern_01_")) + E);
	}
	// Saplings: real leafy branches stood upright, 0.6 to 3 m.
	const FString HB = MP / TEXT("Tree_Hornbeam/Instances/");
	for (const TCHAR* N : { TEXT("Top_Branch_Hornbeam_01"), TEXT("Top_Branch_Hornbeam_02"), TEXT("Branch_Hornbeam_Large_01"), TEXT("Branch_Hornbeam_Large_02"),
		TEXT("Branch_Hornbeam_Large_03"), TEXT("Branch_Hornbeam_Large_04"), TEXT("Branch_Hornbeam_Medium_01"), TEXT("Branch_Hornbeam_Medium_02"), TEXT("Branch_Hornbeam_Medium_03") })
	{
		Static(Saplings, HB + N);
	}
	for (const TCHAR* N : { TEXT("Branch_Hornbeam_Small_01"), TEXT("Branch_Hornbeam_Small_02"), TEXT("Branch_Hornbeam_Small_03") })
	{
		Static(SmallSaplings, HB + N);
	}
	for (const TCHAR* N : { TEXT("Branch_European_Beech_01"), TEXT("Branch_European_Beech_02"), TEXT("Branch_European_Beech_03") })
	{
		Static(SmallSaplings, MP / TEXT("Tree_European_Beech/Instances/") + N);
	}
	for (const TCHAR* N : { TEXT("Branch_English_Oak_01"), TEXT("Branch_English_Oak_02"), TEXT("Branch_English_Oak_03"), TEXT("Branch_English_Oak_04"), TEXT("Branch_English_Oak_05") })
	{
		Static(SmallSaplings, MP / TEXT("Tree_English_Oak/Instances/") + N);
	}
	for (const TCHAR* N : { TEXT("Branch_European_Spindle_NoFlowers_01"), TEXT("Branch_European_Spindle_NoFlowers_03") })
	{
		Static(SmallSaplings, MP / TEXT("Shrub_European_Spindle/Instances/") + N);
	}
	// Seedlings on the forest floor: twigs of beech and oak standing up.
	for (const TCHAR* N : { TEXT("Twig_European_Beech_01"), TEXT("Twig_European_Beech_02"), TEXT("Twig_European_Beech_04") })
	{
		Static(Seedlings, MP / TEXT("Tree_European_Beech/Instances/") + N);
	}
	for (const TCHAR* N : { TEXT("Twig_English_Oak_01"), TEXT("Twig_English_Oak_03"), TEXT("Twig_English_Oak_05"), TEXT("Twig_English_Oak_07"), TEXT("Twig_English_Oak_08") })
	{
		Static(Seedlings, MP / TEXT("Tree_English_Oak/Instances/") + N);
	}
	for (int32 i = 1; i <= 6; ++i)
	{
		Static(Seedlings, MP / FString::Printf(TEXT("Tree_Elder/Instances/Twig_Elder_0%d"), i));
	}
	// Single bracken fronds, fanned out in clumps.
	for (int32 i = 3; i <= 18; ++i)
	{
		Static(FernFronds, MP / FString::Printf(TEXT("Plant_Bracken_Fern/Instances/Branch_Bracken_Fern_%02d"), i));
	}
	Folder(FernFronds, PH / TEXT("fern_02"));
	// Low plants.
	Folder(GroundPlants, PH / TEXT("shrub_03"));
	Folder(GroundPlants, PH / TEXT("shrub_04"));
	Folder(GroundPlants, PH / TEXT("nettle_plant"), TEXT("small"));
	Folder(GroundPlants, PH / TEXT("celandine_01"));
	Folder(GroundPlants, PH / TEXT("shrub_02"));
	Folder(Moss, PH / TEXT("moss_01"));
	// Grass.
	Folder(Grass, TEXT("/Game/Fab/Megascans/Plants"));
	Folder(Grass, PH / TEXT("grass_medium_01"), TEXT("tiny"));
	Folder(Grass, PH / TEXT("grass_medium_02"));
	// Dead wood and rocks.
	Folder(Logs, PH / TEXT("dead_tree_trunk"));
	Folder(Stumps, PH / TEXT("tree_stump_01"));
	Folder(Stumps, PH / TEXT("tree_stump_02"));
	Folder(Roots, PH / TEXT("root_cluster_01"));
	Folder(Roots, PH / TEXT("single_root"));
	Folder(Branches, PH / TEXT("dry_branches_medium_01"));
	Folder(Branches, PH / TEXT("root_cluster_02"));
	for (const TCHAR* N : { TEXT("Tree_Hornbeam/Instances/Branch_Hornbeam_Dead_01"), TEXT("Tree_Hornbeam/Instances/Branch_Hornbeam_Dead_02"),
		TEXT("Tree_Hornbeam/Instances/Branch_Hornbeam_Dead_03"), TEXT("Tree_European_Beech/Instances/Branch_European_Beech_Dead_01"),
		TEXT("Tree_European_Beech/Instances/Branch_European_Beech_Dead_02"), TEXT("Tree_European_Beech/Instances/Branch_European_Beech_Dead_03"),
		TEXT("Shrub_European_Spindle/Instances/Branch_European_Spindle_Dead_01"), TEXT("Shrub_European_Spindle/Instances/Branch_European_Spindle_Dead_03") })
	{
		Static(Branches, MP / N);
	}
	for (const TCHAR* N : { TEXT("Tree_English_Oak/Instances/Twig_English_Oak_Dead_01"), TEXT("Tree_English_Oak/Instances/Twig_English_Oak_Dead_02"),
		TEXT("Tree_European_Beech/Instances/Branch_European_Beech_Dead_03"), TEXT("Tree_European_Beech/Instances/Branch_European_Beech_Dead_04"),
		TEXT("Shrub_European_Spindle/Instances/Branch_European_Spindle_Dead_02"), TEXT("Tree_Hornbeam/Instances/Branch_Hornbeam_Dead_03") })
	{
		Static(Twigs, MP / N);
	}
	Folder(Twigs, PH / TEXT("dry_branches_medium_01"));
	Folder(Boulders, PH / TEXT("rock_moss_set_01"));
	Folder(Boulders, PH / TEXT("boulder_01"));
	Folder(Rocks, PH / TEXT("rock_moss_set_02"));
	Folder(Rocks, PH / TEXT("rock_07"));
	Folder(Rocks, PH / TEXT("rock_09"));
	Folder(Rocks, PH / TEXT("stone_01"));

	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods assets: %d canopy, %d edge, %d young, %d elder, %d saplings, %d small saplings, %d seedlings, %d fern plants, %d fronds, %d low plants, %d moss, %d grass, %d logs, %d stumps, %d roots, %d branches, %d boulders, %d rocks"),
		Canopy.Num(), EdgeTrees.Num(), YoungTrees.Num(), Elders.Num(), Saplings.Num(), SmallSaplings.Num(), Seedlings.Num(), FernPlants.Num(), FernFronds.Num(),
		GroundPlants.Num(), Moss.Num(), Grass.Num(), Logs.Num(), Stumps.Num(), Roots.Num(), Branches.Num(), Boulders.Num(), Rocks.Num());

	// Log the tree materials' parameters once (used to tune the bark tint).
	if (Canopy.Skinned.Num() > 0 && Canopy.Skinned[0])
	{
		for (const FSkeletalMaterial& SM : Canopy.Skinned[0]->GetMaterials())
		{
			if (SM.MaterialInterface)
			{
				TArray<FMaterialParameterInfo> Infos;
				TArray<FGuid> Ids;
				SM.MaterialInterface->GetAllVectorParameterInfo(Infos, Ids);
				FString Names;
				for (const FMaterialParameterInfo& I : Infos) { Names += I.Name.ToString() + TEXT(" "); }
				TArray<FMaterialParameterInfo> SInfos;
				SM.MaterialInterface->GetAllScalarParameterInfo(SInfos, Ids);
				FString SNames;
				for (const FMaterialParameterInfo& I : SInfos) { SNames += I.Name.ToString() + TEXT(" "); }
				UE_LOG(LogTemp, Log, TEXT("[Holdover] Tree material %s: vectors [%s] scalars [%s]"), *SM.MaterialInterface->GetPathName(), *Names, *SNames);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Instancing

UHierarchicalInstancedStaticMeshComponent* AHoldoverWoodsRange::Batch(UStaticMesh* Mesh, bool bCollide, float CullM, bool bShadow)
{
	if (!Mesh)
	{
		return nullptr;
	}
	const FName Key(*FString::Printf(TEXT("%s_%d_%d_%d"), *Mesh->GetPathName(), bCollide ? 1 : 0, FMath::RoundToInt(CullM), bShadow ? 1 : 0));
	if (TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Found = Batches.Find(Key))
	{
		return *Found;
	}
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	H->SetStaticMesh(Mesh);
	H->SetMobility(EComponentMobility::Static);
	H->SetupAttachment(Root);
	if (bCollide)
	{
		H->SetCollisionProfileName(TEXT("BlockAll"));
		H->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
		H->SetPhysMaterialOverride(DirtPhys);
	}
	else
	{
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetGenerateOverlapEvents(false);
	}
	if (CullM > 0.f)
	{
		H->SetCullDistances(FMath::RoundToInt(CullM * 80.f), FMath::RoundToInt(CullM * 100.f));
	}
	H->SetCastShadow(bShadow);
	H->RegisterComponent();
	AddInstanceComponent(H);
	Batches.Add(Key, H);
	return H;
}

UHierarchicalInstancedStaticMeshComponent* AHoldoverWoodsRange::HiddenBatch(const TCHAR* Name, const FLinearColor& Color, bool bCollide)
{
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
	H->SetStaticMesh(bCollide ? HoldoverShapes::Cylinder() : HoldoverShapes::Sphere());
	H->SetMobility(EComponentMobility::Static);
	H->SetupAttachment(Root);
	H->SetVisibility(false);
	H->SetHiddenInGame(true);
	H->SetCastShadow(false);
	H->bAffectIndirectLightingWhileHidden = bCanopyShade;
	if (bCollide)
	{
		H->SetCollisionProfileName(TEXT("BlockAll"));
		H->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
		if (!PhysWood)
		{
			PhysWood = NewObject<UPhysicalMaterial>(this);
			PhysWood->SurfaceType = SurfaceType3; // "Wood"
		}
		H->SetPhysMaterialOverride(PhysWood);
	}
	else
	{
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetGenerateOverlapEvents(false);
	}
	if (UMaterialInterface* BaseMat = HoldoverShapes::BaseMaterial())
	{
		if (UMaterialInstanceDynamic* MID = H->CreateDynamicMaterialInstance(0, BaseMat))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
	H->RegisterComponent();
	AddInstanceComponent(H);
	return H;
}

void AHoldoverWoodsRange::AddStatic(UStaticMesh* Mesh, const FTransform& T, bool bCollide, float CullM, bool bShadow)
{
	if (UHierarchicalInstancedStaticMeshComponent* H = Batch(Mesh, bCollide, CullM, bShadow))
	{
		PendingStatic.FindOrAdd(H).Add(T);
	}
}

void AHoldoverWoodsRange::AddSkinned(USkeletalMesh* Mesh, const FTransform& T, bool bTrunk)
{
	if (!Mesh)
	{
		return;
	}
	PendingSkinned.FindOrAdd(Mesh).Add(T);
	++SkinnedCount;
	const float H = static_cast<float>(Mesh->GetBounds().BoxExtent.Z * 2.0 * T.GetScale3D().Z);
	if (bCanopyShade && H > 300.f)
	{
		AddCanopyShade(Mesh, T);
	}
	if (bTrunk)
	{
		AddTrunkCollider(T.GetLocation(), FMath::Clamp(H * 0.011f, 6.f, 45.f), FMath::Min(H * 0.45f, 700.f));
	}
}

bool AHoldoverWoodsRange::PlaceFrom(const FMeshList& L, FRandomStream& R, const FTransform& T, bool bCollide, float CullM, bool bTrunk, bool bShadow)
{
	const int32 N = L.Num();
	if (N == 0)
	{
		return false;
	}
	const int32 Pick = R.RandRange(0, N - 1);
	if (Pick < L.Static.Num())
	{
		AddStatic(L.Static[Pick], T, bCollide, CullM, bShadow);
	}
	else
	{
		AddSkinned(L.Skinned[Pick - L.Static.Num()], T, bTrunk);
	}
	return true;
}

void AHoldoverWoodsRange::AddTrunkCollider(const FVector& Base, float RadiusCm, float HeightCm)
{
	if (!TrunkColliders)
	{
		TrunkColliders = HiddenBatch(TEXT("TrunkColliders"), FLinearColor(0.05f, 0.045f, 0.035f), true);
	}
	PendingStatic.FindOrAdd(TrunkColliders).Add(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, HeightCm * 0.5f - 20.f),
		FVector(RadiusCm / 50.f, RadiusCm / 50.f, HeightCm / 100.f)));
}

void AHoldoverWoodsRange::AddCanopyShade(USkeletalMesh* Mesh, const FTransform& T)
{
	// Lumen's software tracing doesn't see the skinned Megaplants, so their crowns would let all the sky light
	// through. A hidden dark ellipsoid inside each crown takes their place in the bounce light only.
	if (!CanopyShade)
	{
		CanopyShade = HiddenBatch(TEXT("CanopyShade"), FLinearColor(0.025f, 0.05f, 0.015f), false);
	}
	const FBoxSphereBounds B = Mesh->GetBounds();
	const float H = static_cast<float>(B.BoxExtent.Z * 2.0);
	const float Bottom = static_cast<float>(B.Origin.Z - B.BoxExtent.Z);
	const FVector Centre(B.Origin.X, B.Origin.Y, Bottom + H * 0.68f);
	const float Rad = static_cast<float>(FMath::Min(B.BoxExtent.X, B.BoxExtent.Y)) * CanopyShadeSize;
	const FVector S = T.GetScale3D();
	PendingStatic.FindOrAdd(CanopyShade).Add(FTransform(T.GetRotation(), T.TransformPosition(Centre),
		FVector(Rad * S.X / 50.f, Rad * S.Y / 50.f, H * 0.24f * S.Z / 50.f)));
}

void AHoldoverWoodsRange::Flush()
{
	for (auto& Pair : PendingSkinned)
	{
		USkeletalMesh* Mesh = Pair.Key;
		if (!Mesh || Pair.Value.Num() == 0)
		{
			continue;
		}
		UInstancedSkinnedMeshComponent* C = NewObject<UInstancedSkinnedMeshComponent>(this);
		C->SetSkinnedAssetAndUpdate(Mesh);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		// Darker trunks, like the reference photo: tint every bark material of the big trees.
		if (BarkDarken < 0.99f)
		{
			const TArray<FSkeletalMaterial>& Mats = Mesh->GetMaterials();
			for (int32 i = 0; i < Mats.Num(); ++i)
			{
				UMaterialInterface* Base = Mats[i].MaterialInterface;
				if (Base && Base->GetName().Contains(TEXT("Bark")))
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
					MID->SetVectorParameterValue(TEXT("BaseColor Tint"), FLinearColor(BarkDarken, BarkDarken * 0.97f, BarkDarken * 0.92f, 1.f));
					C->SetMaterial(i, MID);
					KeepAlive.Add(MID);
				}
			}
		}
		C->RegisterComponent();
		AddInstanceComponent(C);
		for (const FTransform& T : Pair.Value)
		{
			C->AddInstance(T, 0);
		}
		SkinnedComponents.Add(C);
	}
	PendingSkinned.Reset();

	for (auto& Pair : PendingStatic)
	{
		if (Pair.Key && Pair.Value.Num() > 0)
		{
			Pair.Key->AddInstances(Pair.Value, false);
		}
	}
	PendingStatic.Reset();
}

// ---------------------------------------------------------------------------
// Plants

namespace Woods
{
	/** A transform that stands a mesh at P, turned by Yaw, leaning LeanDeg toward LeanYaw. */
	FTransform Stand(const FVector& P, float Yaw, float LeanDeg, float LeanYaw, const FVector& Scale)
	{
		const FQuat Turn(FRotator(0.f, Yaw, 0.f));
		// Lean: tilt +Z toward the LeanYaw direction.
		const FQuat Lean = FQuat(FRotator(0.f, LeanYaw, 0.f)) * FQuat(FRotator(-LeanDeg, 0.f, 0.f)) * FQuat(FRotator(0.f, -LeanYaw, 0.f));
		return FTransform(Lean * Turn, P, Scale);
	}

	float MeshHeightM(const UObject* M)
	{
		if (const UStaticMesh* S = Cast<UStaticMesh>(M))
		{
			return static_cast<float>(S->GetBounds().BoxExtent.Z * 0.02);
		}
		if (const USkeletalMesh* K = Cast<USkeletalMesh>(M))
		{
			return static_cast<float>(K->GetBounds().BoxExtent.Z * 0.02);
		}
		return 1.f;
	}
}

void AHoldoverWoodsRange::GrowCanopy(FRandomStream& R)
{
	if (Canopy.Num() == 0)
	{
		return;
	}
	constexpr float Cell = 6.1f;
	const float X1 = GroundX0 + TilesX * TileSize, Y1 = GroundY0 + TilesY * TileSize;
	int32 Placed = 0;
	for (float X = GroundX0 + 2.f; X < X1 - 2.f; X += Cell)
	{
		for (float Y = GroundY0 + 2.f; Y < Y1 - 2.f; Y += Cell)
		{
			const float PX = X + R.FRand() * Cell, PY = Y + R.FRand() * Cell;
			const float Gap = Noise(PX, PY, 28.f, 3.3f);
			float Keep = 0.95f * FMath::Lerp(0.6f, 1.12f, Gap) * TreeDensity * ForestDensity;
			const float Roll = R.FRand();
			const float De = FMath::Abs(PY) - LaneEdge(PX, PY); // metres outside the lane edge
			bool bEdge = false;
			if (InClearing(PX, PY, 0.f))
			{
				// Trees stand right at the ragged edge and, here and there, well into the lane, but never in a line
				// of fire, on the walkway, the pad or the backstop.
				if (KeepClear(PX, PY, 3.5f) || PX < 3.f || PX > Backstop(PY) - 3.f || PathDistance(PX, PY) < 3.f)
				{
					continue;
				}
				const float Grove = Noise(PX, PY, 17.f, 7.7f);
				Keep = De > -3.f ? 0.75f : 0.05f + 0.35f * Grove * Grove;
				bEdge = true;
			}
			else if (De < 6.f && PX > -14.f && PX < Backstop(PY) + 4.f)
			{
				bEdge = true;
			}
			if (Roll > Keep || NearShotSpot(PX, PY, 2.5f, 8.f))
			{
				continue;
			}
			FRandomStream Local(R.RandHelper(1 << 30));
			const bool bWide = bEdge && EdgeTrees.Num() > 0 && Local.FRand() < 0.3f;
			const FMeshList& L = bWide ? EdgeTrees : Canopy;
			const int32 Pick = Local.RandRange(0, L.Num() - 1);
			USkeletalMesh* Mesh = L.Skinned.IsValidIndex(Pick) ? L.Skinned[Pick] : nullptr;
			if (!Mesh)
			{
				continue;
			}
			// Height 16 to 27 m; each tree its own size, turn and lean.
			const float Want = Local.FRandRange(19.f, 31.f) * (bWide ? 0.75f : 1.f);
			const float S = FMath::Clamp(Want / MeshHeightM(Mesh), 0.8f, 1.65f);
			const FVector Scale(S * Local.FRandRange(0.94f, 1.06f), S * Local.FRandRange(0.94f, 1.06f), S);
			// Edge trees lean out over the light of the lane.
			const float LeanYaw = bEdge ? (PY > 0.f ? -90.f : 90.f) + Local.FRandRange(-30.f, 30.f) : Local.FRandRange(0.f, 360.f);
			const float Lean = bEdge ? Local.FRandRange(2.f, 7.f) : Local.FRandRange(0.f, 2.5f);
			AddSkinned(Mesh, Stand(AtLow(PX, PY, 0.7f, 20.f), Local.FRandRange(0.f, 360.f), Lean, LeanYaw, Scale), true);
			++Placed;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods: %d tall trees"), Placed);
}

void AHoldoverWoodsRange::GrowYoungTrees(FRandomStream& R)
{
	constexpr float Cell = 5.5f;
	const float X1 = GroundX0 + TilesX * TileSize, Y1 = GroundY0 + TilesY * TileSize;
	int32 Placed = 0;
	for (float X = GroundX0 + 1.f; X < X1 - 1.f; X += Cell)
	{
		for (float Y = GroundY0 + 1.f; Y < Y1 - 1.f; Y += Cell)
		{
			const float PX = X + R.FRand() * Cell, PY = Y + R.FRand() * Cell;
			const float Gap = Noise(PX, PY, 28.f, 3.3f);
			const float De = FMath::Abs(PY) - LaneEdge(PX, PY);
			// Young trees fill the gaps and crowd the sunny edge of the clearing.
			float Chance = 0.35f + 0.45f * (1.f - Gap) + ((De > -3.f && De < 9.f && PX > -14.f) ? 0.6f : 0.f);
			if (InClearing(PX, PY, 0.f))
			{
				if (KeepClear(PX, PY, 2.8f) || PX < 2.5f || PathDistance(PX, PY) < 2.5f)
				{
					continue;
				}
				const float Thicket = Noise(PX, PY, 11.f, 4.4f);
				Chance = De > -4.f ? 0.8f : 0.05f + 0.6f * Thicket * Thicket;
			}
			if (R.FRand() > Chance * 1.1f * YoungTreeDensity * ForestDensity || NearShotSpot(PX, PY, 2.f, 6.f))
			{
				continue;
			}
			FRandomStream Local(R.RandHelper(1 << 30));
			const bool bElder = Elders.Num() > 0 && Local.FRand() < 0.08f;
			const FMeshList& L = bElder ? Elders : YoungTrees;
			const int32 Pick = Local.RandRange(0, L.Num() - 1);
			USkeletalMesh* Mesh = L.Skinned.IsValidIndex(Pick) ? L.Skinned[Pick] : nullptr;
			if (!Mesh)
			{
				continue;
			}
			const float Want = bElder ? Local.FRandRange(2.5f, 4.f) : Local.FRandRange(4.f, 13.f);
			const float S = FMath::Clamp(Want / MeshHeightM(Mesh), 0.3f, 1.3f);
			const float LeanYaw = (De > -4.f && De < 10.f) ? (PY > 0.f ? -90.f : 90.f) + Local.FRandRange(-40.f, 40.f) : Local.FRandRange(0.f, 360.f);
			AddSkinned(Mesh, Stand(AtLow(PX, PY, 0.3f, 8.f), Local.FRandRange(0.f, 360.f), Local.FRandRange(1.f, 8.f), LeanYaw,
				FVector(S * Local.FRandRange(0.9f, 1.1f), S * Local.FRandRange(0.9f, 1.1f), S)), S > 0.6f);
			++Placed;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods: %d young trees"), Placed);
}

void AHoldoverWoodsRange::GrowSaplings(FRandomStream& R)
{
	if (Saplings.Num() + SmallSaplings.Num() == 0)
	{
		return;
	}
	constexpr float Cell = 2.1f;
	const float X1 = GroundX0 + TilesX * TileSize, Y1 = GroundY0 + TilesY * TileSize;
	const float Cull = SmallCull * 1.6f;
	int32 Placed = 0;
	for (float X = GroundX0 + 0.5f; X < X1 - 0.5f; X += Cell)
	{
		for (float Y = GroundY0 + 0.5f; Y < Y1 - 0.5f; Y += Cell)
		{
			const float PX = X + R.FRand() * Cell, PY = Y + R.FRand() * Cell;
			const bool bNear = FMath::Abs(PY) < DetailReach || (PX < -10.f && FVector2D(PX, PY).Size() < DetailReach);
			const float Gap = Noise(PX, PY, 28.f, 3.3f);
			const float Patch = Noise(PX, PY, 7.f, 2.9f);
			const float De = FMath::Abs(PY) - LaneEdge(PX, PY);
			float Chance = (0.32f + 0.6f * (1.f - Gap) * Patch + ((De > -2.f && De < 7.f && PX > -14.f) ? 0.75f : 0.f)) * (bNear ? 1.f : 0.3f);
			if (InClearing(PX, PY, 0.f))
			{
				if (KeepClear(PX, PY, 1.6f) || PX < 1.5f || PathDistance(PX, PY) < 1.6f)
				{
					continue;
				}
				const float Thicket = Noise(PX, PY, 9.f, 6.6f);
				Chance = De > -3.5f ? 0.9f : 0.02f + 0.75f * FMath::Pow(Thicket, 3.f);
			}
			Chance *= SaplingDensity * ForestDensity;
			if (R.FRand() > FMath::Min(Chance, 1.f) || NearShotSpot(PX, PY, 2.5f, 6.f))
			{
				continue;
			}
			// Saplings grow in small groups.
			FRandomStream Local(R.RandHelper(1 << 30));
			const int32 Group = Local.RandRange(1, 4);
			for (int32 k = 0; k < Group; ++k)
			{
				const float QX = PX + (k ? Local.FRandRange(-0.9f, 0.9f) : 0.f), QY = PY + (k ? Local.FRandRange(-0.9f, 0.9f) : 0.f);
				if (k && InClearing(QX, QY, 0.f) && KeepClear(QX, QY, 1.4f))
				{
					continue;
				}
				const bool bBig = Saplings.Num() > 0 && Local.FRand() < 0.6f;
				const FMeshList& L = bBig ? Saplings : SmallSaplings;
				const float S = bBig ? Local.FRandRange(0.9f, 1.9f) : Local.FRandRange(0.8f, 1.4f);
				PlaceFrom(L, Local, Stand(At(QX, QY, 6.f), Local.FRandRange(0.f, 360.f), Local.FRandRange(0.f, 14.f), Local.FRandRange(0.f, 360.f), FVector(S)), false, Cull);
				++Placed;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods: %d saplings"), Placed);
}

void AHoldoverWoodsRange::GrowFerns(FRandomStream& R)
{
	constexpr float Cell = 1.3f;
	const float X1 = GroundX0 + TilesX * TileSize, Y1 = GroundY0 + TilesY * TileSize;
	int32 Placed = 0;
	for (float X = GroundX0 + 0.5f; X < X1 - 0.5f; X += Cell)
	{
		for (float Y = GroundY0 + 0.5f; Y < Y1 - 0.5f; Y += Cell)
		{
			const float PX = X + R.FRand() * Cell, PY = Y + R.FRand() * Cell;
			if (FMath::Abs(PY) > DetailReach + 15.f && !(PX < -10.f && FVector2D(PX, PY).Size() < DetailReach + 15.f))
			{
				R.FRand(); R.FRand();
				continue;
			}
			// Ferns grow in drifts: thick in some places, absent in others.
			const float Drift = Noise(PX, PY, 9.f, 5.1f) * 0.7f + Noise(PX, PY, 3.f, 1.9f) * 0.3f;
			const float De = FMath::Abs(PY) - LaneEdge(PX, PY);
			float Chance = 0.2f + FMath::SmoothStep(0.28f, 0.58f, Drift) * 0.9f + ((De > -1.5f && De < 5.f && PX > -14.f) ? 0.35f : 0.f);
			if (InClearing(PX, PY, 0.f))
			{
				// Into the lane edge, thinning out; low enough in the lines of fire never.
				if (KeepClear(PX, PY, 0.9f) || PathDistance(PX, PY) < 1.2f || PX < 1.5f)
				{
					continue;
				}
				Chance = De > -2.5f ? 0.7f * FMath::SmoothStep(0.3f, 0.6f, Drift) + 0.2f : 0.25f * FMath::SmoothStep(0.55f, 0.75f, Drift);
			}
			if (R.FRand() > Chance * FernDensity * ForestDensity || NearShotSpot(PX, PY, 1.f, 2.5f))
			{
				continue;
			}
			FRandomStream Local(R.RandHelper(1 << 30));
			const float Kind = Local.FRand();
			if (Kind < 0.6f && FernPlants.Num() > 0)
			{
				const float S = Local.FRandRange(0.8f, 1.45f);
				PlaceFrom(FernPlants, Local, Stand(At(PX, PY, 4.f), Local.FRandRange(0.f, 360.f), Local.FRandRange(0.f, 6.f), Local.FRandRange(0.f, 360.f), FVector(S)), false, SmallCull);
			}
			else if (FernFronds.Num() > 0)
			{
				// A clump: fronds rising from one crown and arching outward, each its own length and angle.
				const int32 Fronds = Local.RandRange(5, 10);
				const float Yaw0 = Local.FRandRange(0.f, 360.f);
				const FVector C = At(PX, PY, 3.f);
				for (int32 k = 0; k < Fronds; ++k)
				{
					const float Yaw = Yaw0 + k * 360.f / Fronds + Local.FRandRange(-20.f, 20.f);
					const float S = Local.FRandRange(0.85f, 1.5f);
					AddStatic(FernFronds.Static[Local.RandRange(0, FernFronds.Static.Num() - 1)],
						Stand(C, Local.FRandRange(0.f, 360.f), Local.FRandRange(10.f, 42.f), Yaw, FVector(S)), false, SmallCull);
				}
			}
			++Placed;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods: %d fern clumps"), Placed);
}

void AHoldoverWoodsRange::GrowGroundPlants(FRandomStream& R)
{
	constexpr float Cell = 1.f;
	const float X1 = GroundX0 + TilesX * TileSize, Y1 = GroundY0 + TilesY * TileSize;
	int32 Placed = 0;
	for (float X = GroundX0 + 0.5f; X < X1 - 0.5f; X += Cell)
	{
		for (float Y = GroundY0 + 0.5f; Y < Y1 - 0.5f; Y += Cell)
		{
			const float PX = X + R.FRand() * Cell, PY = Y + R.FRand() * Cell;
			const float Pick = R.FRand();
			if (FMath::Abs(PY) > DetailReach && !(PX < -10.f && FVector2D(PX, PY).Size() < DetailReach))
			{
				continue;
			}
			const float Open = LaneOpen(PX, PY);
			const float Patch = Noise(PX, PY, 4.f, 8.8f);
			float Chance = (1.f - Open) * (0.35f + 0.6f * Patch);
			if (Open > 0.3f)
			{
				// Low weeds in the lane only where the mower missed, never right in front of a target.
				if (KeepClear(PX, PY, 0.2f) && FireDistance(PX, PY) < 0.9f)
				{
					continue;
				}
				Chance = 0.06f + 0.3f * Patch * Patch;
			}
			if (IsPad(PX, PY) || PathDistance(PX, PY) < 0.8f || OnWalkway(PX, PY) || R.FRand() > Chance * GroundPlantDensity * ForestDensity)
			{
				continue;
			}
			FRandomStream Local(R.RandHelper(1 << 30));
			const FVector P = At(PX, PY, 2.f);
			const float Yaw = Local.FRandRange(0.f, 360.f);
			if (Pick < 0.4f && Seedlings.Num() > 0)
			{
				// Tree seedlings: a twig of beech, oak or elder standing up.
				const float S = Local.FRandRange(0.9f, 2.f);
				PlaceFrom(Seedlings, Local, Stand(P, Yaw, Local.FRandRange(0.f, 20.f), Local.FRandRange(0.f, 360.f), FVector(S)), false, SmallCull * 0.6f, false, false);
			}
			else if (Pick < 0.85f && GroundPlants.Num() > 0)
			{
				const float S = Local.FRandRange(1.f, 1.9f);
				PlaceFrom(GroundPlants, Local, Stand(P, Yaw, Local.FRandRange(0.f, 8.f), Local.FRandRange(0.f, 360.f), FVector(S)), false, SmallCull * 0.7f);
			}
			else if (Moss.Num() > 0 && Open < 0.3f)
			{
				const float S = Local.FRandRange(0.8f, 1.6f);
				PlaceFrom(Moss, Local, FTransform(FRotator(0.f, Yaw, 0.f), At(PX, PY, 1.f), FVector(S)), false, SmallCull * 0.5f, false, false);
			}
			++Placed;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods: %d low plants"), Placed);
}

void AHoldoverWoodsRange::GrowGrass(FRandomStream& R)
{
	if (Grass.Num() == 0)
	{
		return;
	}
	constexpr float Cell = 0.75f;
	int32 Placed = 0;
	for (float X = -16.f; X < BackstopToe + 8.f; X += Cell)
	{
		for (float Y = -26.f; Y < 26.f; Y += Cell)
		{
			const float PX = X + R.FRand() * Cell, PY = Y + R.FRand() * Cell;
			const float Roll = R.FRand();
			const FLinearColor W = SurfaceWeights(PX, PY);
			const float De = FMath::Abs(PY) - LaneEdge(PX, PY);
			const float Drift = Noise(PX, PY, 6.f, 2.4f);
			// Uncut long grass in a ragged band where the lane meets the trees, and in drifts between the lines of fire.
			const bool bBand = De > -3.5f + 2.f * Drift && De < 1.5f;
			const bool bFire = PX > 1.f && KeepClear(PX, PY, 0.6f);
			float Chance = W.R * (0.18f + 0.35f * Drift);
			if (bBand && !bFire)
			{
				Chance = 0.85f;
			}
			else if (!bFire && PX > 3.f && Drift > 0.5f)
			{
				Chance = FMath::Max(Chance, 0.6f * W.R);
			}
			if (W.B > 0.4f || W.G > 0.55f || Roll > Chance * GrassDensity)
			{
				continue;
			}
			float NearTarget = BIG_NUMBER;
			for (const float D : TargetDistances)
			{
				NearTarget = FMath::Min(NearTarget, FMath::Abs(PX - D));
			}
			if (bFire && NearTarget < 2.f)
			{
				continue;
			}
			FRandomStream Local(R.RandHelper(1 << 30));
			const int32 Clumps = (bBand || Drift > 0.5f) && !bFire ? Local.RandRange(2, 5) : 1;
			for (int32 k = 0; k < Clumps; ++k)
			{
				const float QX = PX + (k ? Local.FRandRange(-0.4f, 0.4f) : 0.f), QY = PY + (k ? Local.FRandRange(-0.4f, 0.4f) : 0.f);
				const float S = bFire ? Local.FRandRange(0.5f, 0.85f) : Local.FRandRange(0.9f, 1.8f);
				PlaceFrom(Grass, Local, FTransform(FRotator(Local.FRandRange(-4.f, 4.f), Local.FRandRange(0.f, 360.f), 0.f), At(QX, QY, 2.f), FVector(S)), false, SmallCull, false, false);
				++Placed;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods: %d grass clumps"), Placed);
}

void AHoldoverWoodsRange::ScatterDeadWood(FRandomStream& R)
{
	constexpr float Cell = 5.f;
	const float X1 = GroundX0 + TilesX * TileSize, Y1 = GroundY0 + TilesY * TileSize;
	auto Pick = [](const FMeshList& L, FRandomStream& S) -> UStaticMesh* { return L.Static.Num() ? L.Static[S.RandRange(0, L.Static.Num() - 1)] : nullptr; };
	int32 Placed = 0;
	for (float X = GroundX0 + 1.f; X < X1 - 1.f; X += Cell)
	{
		for (float Y = GroundY0 + 1.f; Y < Y1 - 1.f; Y += Cell)
		{
			const float PX = X + R.FRand() * Cell, PY = Y + R.FRand() * Cell;
			const float Kind = R.FRand();
			const float Roll = R.FRand();
			const bool bNear = FMath::Abs(PY) < DetailReach || (PX < -10.f && FVector2D(PX, PY).Size() < DetailReach);
			if (InClearing(PX, PY, 1.f) || Roll > (bNear ? 0.75f : 0.25f) * DebrisDensity || NearShotSpot(PX, PY, 2.f, 5.f))
			{
				// Fallen branches also lie in the lane edge (never in a line of fire).
				if (!(Kind > 0.55f && InClearing(PX, PY, 0.f) && !KeepClear(PX, PY, 1.f) && FMath::Abs(PY) > LaneEdge(PX, PY) - 4.f && Roll < 0.4f))
				{
					continue;
				}
			}
			FRandomStream Local(R.RandHelper(1 << 30));
			const float Yaw = Local.FRandRange(0.f, 360.f);
			if (Kind < 0.13f)
			{
				// A fallen trunk, lying along the slope.
				if (UStaticMesh* M = Pick(Logs, Local))
				{
					const float S = Local.FRandRange(0.9f, 1.6f);
					const float Len = static_cast<float>(M->GetBounds().BoxExtent.X * 0.02 * S);
					const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
					const float H0 = GroundHeightM(PX - Dir.X * Len * 0.45f, PY - Dir.Y * Len * 0.45f);
					const float H1 = GroundHeightM(PX + Dir.X * Len * 0.45f, PY + Dir.Y * Len * 0.45f);
					const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(H1 - H0, Len * 0.9f));
					const float Dia = static_cast<float>(M->GetBounds().BoxExtent.Z * 2.0 * S);
					const FVector P(PX * 100.f, PY * 100.f, (H0 + H1) * 50.f - Dia * 0.25f);
					AddStatic(M, FTransform(FRotator(Pitch, Yaw, Local.FRandRange(0.f, 360.f)), P, FVector(S)), true, 0.f);
					++Placed;
				}
			}
			else if (Kind < 0.21f)
			{
				if (UStaticMesh* M = Pick(Stumps, Local))
				{
					const float S = Local.FRandRange(0.7f, 1.2f);
					AddStatic(M, FTransform(FRotator(0.f, Yaw, 0.f), AtLow(PX, PY, 0.6f * S, 8.f), FVector(S)), true, 0.f);
					++Placed;
				}
			}
			else if (Kind < 0.27f)
			{
				if (UStaticMesh* M = Pick(Roots, Local))
				{
					const float S = Local.FRandRange(0.8f, 1.3f);
					AddStatic(M, FTransform(FRotator(0.f, Yaw, 0.f), AtLow(PX, PY, 1.f * S, 15.f), FVector(S)), true, SmallCull * 1.5f);
					++Placed;
				}
			}
			else
			{
				// Fallen branches: lying on the ground, the tip resting a little up.
				const int32 N = Local.RandRange(1, 3);
				for (int32 k = 0; k < N; ++k)
				{
					if (UStaticMesh* M = Pick(Branches, Local))
					{
						const float QX = PX + Local.FRandRange(-1.5f, 1.5f), QY = PY + Local.FRandRange(-1.5f, 1.5f);
						const bool bUpright = M->GetBounds().BoxExtent.Z > M->GetBounds().BoxExtent.X && M->GetBounds().BoxExtent.Z > M->GetBounds().BoxExtent.Y;
						const FRotator Rot = bUpright ? FRotator(-88.f + Local.FRandRange(0.f, 8.f), Yaw + Local.FRandRange(-40.f, 40.f), Local.FRandRange(0.f, 360.f))
							: FRotator(Local.FRandRange(-3.f, 3.f), Yaw + Local.FRandRange(-40.f, 40.f), 0.f);
						AddStatic(M, FTransform(Rot, At(QX, QY, 2.f), FVector(Local.FRandRange(0.8f, 1.3f))), false, SmallCull);
						++Placed;
					}
				}
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods: %d pieces of dead wood"), Placed);
}

void AHoldoverWoodsRange::DressLaneEdges(FRandomStream& R)
{
	// Things the range builders left at the edge of the lane: a mossy boulder rolled aside, a cut log, an old stump,
	// a heap of brush. Never in a line of fire, on the walkway or in front of a sign.
	int32 Placed = 0;
	for (float X = 6.f; X < BackstopToe - 8.f; X += 9.f)
	{
		for (const float Side : { -1.f, 1.f })
		{
			const float PX = X + R.FRandRange(0.f, 7.f);
			const float In = R.FRandRange(0.3f, 3.2f);
			const float PY = Side * (LaneEdge(PX, Side) - In);
			const float Kind = R.FRand();
			const float Roll = R.FRand();
			if (Roll > 0.55f || KeepClear(PX, PY, 2.2f) || OnWalkway(PX, PY, 1.5f))
			{
				continue;
			}
			FRandomStream Local(R.RandHelper(1 << 30));
			const float Yaw = Local.FRandRange(0.f, 360.f);
			if (Kind < 0.4f && Rocks.Static.Num() + Boulders.Static.Num() > 0)
			{
				const bool bBig = Boulders.Static.Num() > 0 && Local.FRand() < 0.4f;
				const TArray<UStaticMesh*>& L = bBig ? Boulders.Static : Rocks.Static;
				UStaticMesh* M = L[Local.RandRange(0, L.Num() - 1)];
				const float S = bBig ? Local.FRandRange(0.35f, 0.6f) : Local.FRandRange(0.5f, 1.f);
				const float H = static_cast<float>(M->GetBounds().BoxExtent.Z * 2.0 * S);
				AddStatic(M, FTransform(FRotator(Local.FRandRange(-15.f, 15.f), Yaw, Local.FRandRange(-15.f, 15.f)), AtLow(PX, PY, 0.5f, H * 0.35f), FVector(S)), true, 0.f);
			}
			else if (Kind < 0.65f && Logs.Static.Num() > 0)
			{
				// Cut logs lie along the edge, roughly parallel to it.
				UStaticMesh* M = Logs.Static[Local.RandRange(0, Logs.Static.Num() - 1)];
				const float S = Local.FRandRange(0.8f, 1.2f);
				const float Dia = static_cast<float>(M->GetBounds().BoxExtent.Z * 2.0 * S);
				AddStatic(M, FTransform(FRotator(0.f, Local.FRandRange(-15.f, 15.f), Local.FRandRange(0.f, 360.f)), AtLow(PX, PY, 1.2f, Dia * 0.2f), FVector(S)), true, 0.f);
			}
			else if (Kind < 0.8f && Stumps.Static.Num() > 0)
			{
				UStaticMesh* M = Stumps.Static[Local.RandRange(0, Stumps.Static.Num() - 1)];
				AddStatic(M, FTransform(FRotator(0.f, Yaw, 0.f), AtLow(PX, PY, 0.5f, 8.f), FVector(Local.FRandRange(0.6f, 0.9f))), true, 0.f);
			}
			else if (Branches.Static.Num() > 0)
			{
				// A brush heap: several dead branches piled together.
				for (int32 k = 0; k < 6; ++k)
				{
					UStaticMesh* M = Branches.Static[Local.RandRange(0, Branches.Static.Num() - 1)];
					const bool bUpright = M->GetBounds().BoxExtent.Z > FMath::Max(M->GetBounds().BoxExtent.X, M->GetBounds().BoxExtent.Y);
					const FRotator Rot = bUpright ? FRotator(-70.f + Local.FRandRange(0.f, 25.f), Yaw + Local.FRandRange(-60.f, 60.f), Local.FRandRange(0.f, 360.f))
						: FRotator(Local.FRandRange(-15.f, 15.f), Yaw + Local.FRandRange(-60.f, 60.f), Local.FRandRange(-20.f, 20.f));
					AddStatic(M, FTransform(Rot, At(PX + Local.FRandRange(-0.8f, 0.8f), PY + Local.FRandRange(-0.6f, 0.6f), -10.f * k), FVector(Local.FRandRange(0.9f, 1.4f))), false, SmallCull * 1.5f);
				}
			}
			++Placed;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods: %d things at the lane edges"), Placed);
}

void AHoldoverWoodsRange::ScatterTwigs(FRandomStream& R)
{
	if (Twigs.Num() == 0)
	{
		return;
	}
	constexpr float Cell = 1.4f;
	int32 Placed = 0;
	for (float X = -60.f; X < BackstopToe + 40.f; X += Cell)
	{
		for (float Y = -DetailReach; Y < DetailReach; Y += Cell)
		{
			const float PX = X + R.FRand() * Cell, PY = Y + R.FRand() * Cell;
			const float Roll = R.FRand();
			const float Open = LaneOpen(PX, PY);
			if (Roll > 0.45f * (1.f - Open) * DebrisDensity || PathDistance(PX, PY) < 1.f || IsPad(PX, PY))
			{
				continue;
			}
			FRandomStream Local(R.RandHelper(1 << 30));
			UStaticMesh* M = Twigs.Static[Local.RandRange(0, Twigs.Static.Num() - 1)];
			const bool bUpright = M->GetBounds().BoxExtent.Z > FMath::Max(M->GetBounds().BoxExtent.X, M->GetBounds().BoxExtent.Y);
			const FRotator Rot = bUpright ? FRotator(-86.f + Local.FRandRange(0.f, 6.f), Local.FRandRange(0.f, 360.f), Local.FRandRange(0.f, 360.f))
				: FRotator(Local.FRandRange(-3.f, 3.f), Local.FRandRange(0.f, 360.f), 0.f);
			AddStatic(M, FTransform(Rot, At(PX, PY, 1.f), FVector(Local.FRandRange(0.7f, 1.4f))), false, SmallCull * 0.45f, false);
			++Placed;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods: %d twigs"), Placed);
}

void AHoldoverWoodsRange::ScatterRocks(FRandomStream& R)
{
	constexpr float Cell = 9.f;
	const float X1 = GroundX0 + TilesX * TileSize, Y1 = GroundY0 + TilesY * TileSize;
	int32 Placed = 0;
	for (float X = GroundX0 + 2.f; X < X1 - 2.f; X += Cell)
	{
		for (float Y = GroundY0 + 2.f; Y < Y1 - 2.f; Y += Cell)
		{
			const float PX = X + R.FRand() * Cell, PY = Y + R.FRand() * Cell;
			const float Roll = R.FRand();
			// Rocks come up through the soil in some parts of the forest (ridges), not evenly.
			const float Rocky = Noise(PX, PY, 40.f, 6.2f);
			if (Roll > 0.45f * FMath::SmoothStep(0.35f, 0.75f, Rocky) * DebrisDensity || InClearing(PX, PY, 2.f) || NearShotSpot(PX, PY, 3.f, 8.f))
			{
				continue;
			}
			FRandomStream Local(R.RandHelper(1 << 30));
			// One big rock, half buried, with smaller ones around it.
			if (Boulders.Static.Num() > 0)
			{
				UStaticMesh* M = Boulders.Static[Local.RandRange(0, Boulders.Static.Num() - 1)];
				const float S = Local.FRandRange(0.5f, 1.f);
				const float H = static_cast<float>(M->GetBounds().BoxExtent.Z * 2.0 * S);
				const float Reach = static_cast<float>(FMath::Max(M->GetBounds().BoxExtent.X, M->GetBounds().BoxExtent.Y) * S * 0.007);
				AddStatic(M, FTransform(FRotator(Local.FRandRange(-12.f, 12.f), Local.FRandRange(0.f, 360.f), Local.FRandRange(-12.f, 12.f)),
					AtLow(PX, PY, Reach, H * Local.FRandRange(0.25f, 0.45f)), FVector(S)), true, 0.f);
				++Placed;
			}
			const int32 Small = Local.RandRange(1, 5);
			for (int32 k = 0; k < Small && Rocks.Static.Num() > 0; ++k)
			{
				UStaticMesh* M = Rocks.Static[Local.RandRange(0, Rocks.Static.Num() - 1)];
				const float A = Local.FRandRange(0.f, UE_TWO_PI), D = Local.FRandRange(1.f, 3.5f);
				const float QX = PX + D * FMath::Cos(A), QY = PY + D * FMath::Sin(A);
				if (InClearing(QX, QY, 0.5f))
				{
					continue;
				}
				const float S = Local.FRandRange(0.35f, 1.f);
				const float H = static_cast<float>(M->GetBounds().BoxExtent.Z * 2.0 * S);
				AddStatic(M, FTransform(FRotator(Local.FRandRange(-20.f, 20.f), Local.FRandRange(0.f, 360.f), Local.FRandRange(-20.f, 20.f)),
					AtLow(QX, QY, 0.3f, H * 0.3f), FVector(S)), true, SmallCull * 1.5f);
				++Placed;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Woods: %d rocks"), Placed);
}

// ===========================================================================
// Range furniture

bool AHoldoverWoodsRange::PlaceProp(const TArray<FString>& Names, const FVector& LocM, float YawDeg, bool bCollide)
{
	UStaticMesh* Mesh = nullptr;
	for (const FString& N : Names)
	{
		Mesh = N.StartsWith(TEXT("/")) ? LoadMesh(N) : LoadMesh(TEXT("/Game/Range/Props/") + N);
		if (Mesh)
		{
			break;
		}
	}
	if (!Mesh)
	{
		return false;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Static);
	C->SetupAttachment(Root);
	const FVector G = At(LocM.X, LocM.Y);
	C->SetRelativeLocation(FVector(G.X, G.Y, G.Z + LocM.Z * 100.f));
	C->SetRelativeRotation(FRotator(0.f, YawDeg, 0.f));
	if (bCollide)
	{
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	C->RegisterComponent();
	AddInstanceComponent(C);
	return true;
}

UStaticMeshComponent* AHoldoverWoodsRange::Beam(const FVector& A, const FVector& B, float W, float H, UMaterialInterface* Mat, float Roll)
{
	// A timber from A to B (cm), W wide and H deep, its length along local X.
	const FVector D = B - A;
	const FRotator Rot = FRotationMatrix::MakeFromX(D.GetSafeNormal()).Rotator() + FRotator(0.f, 0.f, Roll);
	UStaticMeshComponent* C = HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cube(), (A + B) * 0.5f, Rot,
		FVector(D.Size(), W, H), FLinearColor(0.2f, 0.14f, 0.09f), true);
	if (C)
	{
		C->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
		if (Mat)
		{
			C->SetMaterial(0, Mat);
		}
		if (!PhysWood)
		{
			PhysWood = NewObject<UPhysicalMaterial>(this);
			PhysWood->SurfaceType = SurfaceType3;
		}
		C->SetPhysMaterialOverride(PhysWood);
	}
	return C;
}

void AHoldoverWoodsRange::BuildShelter()
{
	// A rough timber shelter over the firing line: posts, beams, rafters and a board roof gone green with moss.
	UMaterialInterface* Timber = LoadMaterial(TEXT("/Game/Range/Woods/Materials/MI_WoodsTimber.MI_WoodsTimber"));
	UMaterialInterface* Roof = LoadMaterial(TEXT("/Game/Range/Woods/Materials/MI_WoodsRoof.MI_WoodsRoof"));
	const float Front = 60.f, Back = -470.f, Half = 760.f;
	const float FrontH = 275.f, BackH = 235.f;
	const float Posts[] = { -Half, -Half * 0.33f, Half * 0.33f, Half };
	for (const float Y : Posts)
	{
		for (const float X : { Front, Back })
		{
			const float Top = X == Front ? FrontH : BackH;
			const float G = GroundHeight(X, Y);
			Beam(FVector(X, Y, G - 40.f), FVector(X, Y, G + Top), 15.f, 15.f, Timber);
		}
	}
	// Beams along the front and back, rafters across, then roof boards.
	Beam(FVector(Front, -Half - 40.f, FrontH - 8.f), FVector(Front, Half + 40.f, FrontH - 8.f), 18.f, 22.f, Timber);
	Beam(FVector(Back, -Half - 40.f, BackH - 8.f), FVector(Back, Half + 40.f, BackH - 8.f), 18.f, 22.f, Timber);
	const float Slope = (FrontH - BackH) / (Front - Back);
	for (float Y = -Half - 30.f; Y <= Half + 31.f; Y += 95.f)
	{
		Beam(FVector(Back - 60.f, Y, BackH + 10.f - 60.f * Slope), FVector(Front + 70.f, Y, FrontH + 10.f + 70.f * Slope), 7.f, 16.f, Timber);
	}
	for (float X = Back - 55.f; X < Front + 70.f; X += 24.f)
	{
		const float Z = BackH + 21.f + (X - Back) * Slope;
		Beam(FVector(X, -Half - 60.f, Z), FVector(X, Half + 60.f, Z), 27.f, 3.f, Roof ? Roof : Timber, FMath::RadiansToDegrees(FMath::Atan(Slope)));
	}
	// Diagonal braces at the front corners.
	for (const float Y : { -Half, Half })
	{
		const float S = Y > 0.f ? -1.f : 1.f;
		Beam(FVector(Front, Y, FrontH - 80.f), FVector(Front, Y + S * 70.f, FrontH - 10.f), 10.f, 10.f, Timber);
	}
}

void AHoldoverWoodsRange::PlaceRangeProps()
{
	constexpr float FaceShooter = 90.f; // the Blender props face Blender -Y (Unreal +Y)
	auto Text = [this](const FString& Label, const FVector& Where, float YawDeg, float Size, const FColor& Color)
	{
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
		T->SetupAttachment(Root);
		T->SetRelativeLocation(Where);
		T->SetRelativeRotation(FRotator(0.f, YawDeg, 0.f));
		T->SetText(FText::FromString(Label));
		T->SetTextRenderColor(Color);
		T->SetWorldSize(Size);
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		T->RegisterComponent();
		AddInstanceComponent(T);
	};

	BuildShelter();

	// Benches on the firing line: yours just to your right, two more bays.
	for (const FVector& B : { FVector(-0.3f, 1.1f, 0.f), FVector(-0.3f, -4.2f, 0.f), FVector(-0.3f, 6.4f, 0.f) })
	{
		if (!PlaceProp({ TEXT("SM_ShootingBench"), TEXT("SM_Bench") }, B, FaceShooter))
		{
			HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cube(), At(B.X, B.Y) + FVector(0, 0, 88.f), FRotator::ZeroRotator, FVector(75.f, 140.f, 5.f), HoldoverShapes::Wood, true);
		}
	}
	for (const FVector& S : { FVector(-1.1f, -4.2f, 0.f), FVector(-1.1f, 6.4f, 0.f) })
	{
		PlaceProp({ TEXT("SM_Stool") }, S, FaceShooter);
	}
	PlaceProp({ TEXT("SM_AmmoCan") }, FVector(-0.45f, 1.55f, 0.905f), 90.f);
	PlaceProp({ TEXT("SM_SandbagRest"), TEXT("SM_Sandbags") }, FVector(-0.25f, -4.2f, 0.905f), FaceShooter);
	PlaceProp({ TEXT("SM_BrassBin"), TEXT("SM_SpentBrassBin") }, FVector(-2.6f, 3.4f, 0.f), FaceShooter);
	PlaceProp({ TEXT("SM_BenchSeat"), TEXT("SM_Seat") }, FVector(-11.f, 4.f, 0.f), FaceShooter);

	// Extras: an old ammo crate by the left bay, a plastic barrel for rubbish behind the line, a hatchet left in a
	// stump in the clearing.
	const FString PH = TEXT("/Game/Range/Woods/PolyHaven/");
	PlaceProp({ PH + TEXT("old_military_crate/old_military_crate_2k/StaticMeshes/old_military_crate_a") }, FVector(-1.7f, -6.1f, 0.f), 8.f);
	PlaceProp({ PH + TEXT("old_military_crate/old_military_crate_2k/StaticMeshes/old_military_crate_lid_a") }, FVector(-1.7f, -6.1f, 0.52f), 8.f);
	PlaceProp({ PH + TEXT("old_military_crate/old_military_crate_2k/StaticMeshes/old_military_crate_b") }, FVector(-5.2f, 7.2f, 0.f), 81.f);
	PlaceProp({ PH + TEXT("Barrel_02/Barrel_02_2k/StaticMeshes/Barrel_02_2k") }, FVector(-6.2f, -2.4f, 0.f), 30.f);
	if (Stumps.Static.Num() > 0)
	{
		UStaticMesh* Stump = Stumps.Static[0];
		AddStatic(Stump, FTransform(FRotator(0.f, 40.f, 0.f), At(-13.5f, -3.5f, 6.f), FVector(0.8f)), true, 0.f);
		PlaceProp({ PH + TEXT("hatchet/hatchet_2k/StaticMeshes/hatchet_2k") }, FVector(-13.5f, -3.5f, 0.42f), 110.f, false);
	}

	// Firing line: a timber lying across the front of the pad.
	Beam(FVector(130.f, -880.f, GroundHeight(130.f, 0.f) + 6.f), FVector(130.f, 880.f, GroundHeight(130.f, 0.f) + 6.f), 18.f, 14.f,
		LoadMaterial(TEXT("/Game/Range/Woods/Materials/MI_WoodsTimber.MI_WoodsTimber")));

	// Distance signs along the left edge, facing you.
	for (const int32 D : SignDistances)
	{
		const FVector P(static_cast<float>(D), SignY(static_cast<float>(D)), 0.f);
		const FString Id = FString::FromInt(D);
		PlaceProp({ TEXT("SM_DistanceMarker_") + Id + TEXT("m"), TEXT("SM_Sign_") + Id + TEXT("m") }, P, FaceShooter);
	}
	if (!PlaceProp({ TEXT("SM_Sign_FiringLine"), TEXT("SM_FiringLineSign") }, FVector(0.4f, -8.4f, 0.f), FaceShooter))
	{
		Text(TEXT("FIRING LINE"), At(0.4f, -8.4f) + FVector(0, 0, 160.f), 180.f, 16.f, FColor(200, 160, 20));
	}
	PlaceProp({ TEXT("SM_Sign_CeaseFire"), TEXT("SM_CeaseFireSign") }, FVector(-8.5f, -9.6f, 0.f), FaceShooter + 25.f);

	// A range rules board on the shelter's back left post.
	const FVector Board = At(-4.6f, -7.3f) + FVector(14.f, 0.f, 150.f);
	if (UStaticMeshComponent* B = HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cube(), Board, FRotator::ZeroRotator, FVector(2.f, 70.f, 52.f), FLinearColor(0.55f, 0.52f, 0.45f), false))
	{
		B->SetCastShadow(true);
	}
	Text(TEXT("RANGE RULES"), Board + FVector(1.6f, 0.f, 18.f), 0.f, 6.f, FColor(20, 20, 20));
	Text(TEXT("Treat every gun as loaded"), Board + FVector(1.6f, 0.f, 8.f), 0.f, 3.2f, FColor(30, 30, 30));
	Text(TEXT("Muzzle downrange"), Board + FVector(1.6f, 0.f, 2.f), 0.f, 3.2f, FColor(30, 30, 30));
	Text(TEXT("Finger off the trigger"), Board + FVector(1.6f, 0.f, -4.f), 0.f, 3.2f, FColor(30, 30, 30));
	Text(TEXT("Eyes and ears on"), Board + FVector(1.6f, 0.f, -10.f), 0.f, 3.2f, FColor(30, 30, 30));
	Text(TEXT("CEASE FIRE means stop"), Board + FVector(1.6f, 0.f, -18.f), 0.f, 3.4f, FColor(120, 10, 10));
}

// ===========================================================================
// Light

void AHoldoverWoodsRange::SpawnLightingIfMissing()
{
	UWorld* World = GetWorld();
	if (!World || LevelHasSun())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// The sun stands off to one side, so leaves between you and it glow and the trunks read dark, as in the photo.
	const FRotator SunRot(-SunPitch, SunBearing + 180.f, 0.f); // the light travels away from where the sun stands
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 1000.f), GetActorRotation() + SunRot, Params))
	{
		Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			L->SetIntensity(SunLux);
			L->bUseTemperature = true;
			L->Temperature = SunKelvin;
			L->SetAtmosphereSunLight(true);
			L->VolumetricScatteringIntensity = SunShafts;
			L->LightSourceAngle = 0.53f;
			L->MarkRenderStateDirty();
		}
		SpawnedHelpers.Add(Sun);
	}

	if (ASkyAtmosphere* Sky = World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		if (USkyAtmosphereComponent* C = Sky->GetComponent())
		{
			// Humid summer air: more haze, a paler, whiter sky.
			C->SetMieScatteringScale(C->MieScatteringScale * SkyHaze);
			C->SetMieAnisotropy(0.85f);
			C->SetRayleighScatteringScale(C->RayleighScatteringScale * 0.75f);
		}
		SpawnedHelpers.Add(Sky);
	}

	if (ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator, Params))
	{
		SkyLight->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		if (USkyLightComponent* C = SkyLight->GetLightComponent())
		{
			C->bRealTimeCapture = true;
			C->SetIntensity(SkyLightIntensity);
			C->MarkRenderStateDirty();
			C->RecaptureSky();
		}
		SpawnedHelpers.Add(SkyLight);
	}

	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		if (UExponentialHeightFogComponent* F = Fog->GetComponent())
		{
			F->SetFogDensity(FogDensity);
			F->SetFogHeightFalloff(FogFalloff);
			F->SetFogInscatteringColor(FogColor);
			F->SetVolumetricFog(true);
			F->SetVolumetricFogScatteringDistribution(0.7f);
			F->SetVolumetricFogExtinctionScale(VolumetricFogScatter);
			F->SetVolumetricFogDistance(9000.f);
			F->SetDirectionalInscatteringExponent(6.f);
			F->SetDirectionalInscatteringColor(FLinearColor(0.35f, 0.33f, 0.26f));
		}
		SpawnedHelpers.Add(Fog);
	}

	if (APostProcessVolume* PP = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		PP->bUnbound = true;
		FPostProcessSettings& S = PP->Settings;
		S.bOverride_AutoExposureMethod = true;
		S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		S.bOverride_AutoExposureMinBrightness = true;
		S.AutoExposureMinBrightness = EyeMinEV;
		S.bOverride_AutoExposureMaxBrightness = true;
		S.AutoExposureMaxBrightness = EyeMaxEV;
		S.bOverride_AutoExposureSpeedUp = true;
		S.AutoExposureSpeedUp = 2.f;
		S.bOverride_AutoExposureSpeedDown = true;
		S.AutoExposureSpeedDown = 0.8f;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = ExposureBias;
		S.bOverride_LocalExposureHighlightContrastScale = true;
		S.LocalExposureHighlightContrastScale = HighlightContrast;
		S.bOverride_LocalExposureShadowContrastScale = true;
		S.LocalExposureShadowContrastScale = ShadowContrast;
		S.bOverride_WhiteTemp = true;
		S.WhiteTemp = WhiteBalance;
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(1.f, 1.f, 1.f, Saturation);
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(1.f, 1.f, 1.f, Contrast);
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = 0.f;
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = Bloom;
		SpawnedHelpers.Add(PP);
	}
}

// ===========================================================================
// Check pictures

void AHoldoverWoodsRange::UpdateRangeShots(float DeltaSeconds)
{
	const int32 Mode = HoldoverDebug::RangeShots();
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (Mode <= 0 || !PC)
	{
		return;
	}
#if WITH_EDITOR
	if (GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0)
	{
		return;
	}
#endif
	ShotClock += DeltaSeconds;
	constexpr float Settle = 7.f;
	if (ShotStep == 0)
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			TArray<AActor*> Attached;
			Pawn->GetAttachedActors(Attached, true, true);
			Pawn->SetActorHiddenInGame(true);
			for (AActor* A : Attached)
			{
				A->SetActorHiddenInGame(true);
			}
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ACameraActor* Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params))
		{
			Cam->GetCameraComponent()->SetFieldOfView(90.f);
			Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
			PC->SetViewTarget(Cam);
			ShotCamera = Cam;
		}
		ShotStep = 1;
		ShotClock = 0.f;
	}
	const int32 Spot = (ShotStep - 1) / 2;
	if (Spot >= UE_ARRAY_COUNT(Shots))
	{
		if (Mode == 1)
		{
			HoldoverDebug::CVarRangeShots->Set(0, ECVF_SetByConsole);
			PC->ConsoleCommand(TEXT("quit"));
		}
		return;
	}
	const FShot& S = Shots[Spot];
	if ((ShotStep % 2) == 1)
	{
		if (ShotCamera)
		{
			const float SY = S.bWalkway ? WalkwayY(S.X) : S.Y;
			const FVector Local(S.X * 100.f, SY * 100.f, GroundHeightM(S.X, SY) * 100.f + S.Up * 100.f);
			ShotCamera->SetActorLocationAndRotation(GetActorTransform().TransformPosition(Local),
				(GetActorRotation().Quaternion() * FRotator(S.Pitch, S.Yaw, 0.f).Quaternion()).Rotator());
		}
		ShotClock = 0.f;
		ShotFrameStart = GFrameCounter;
		++ShotStep;
	}
	else if (ShotClock >= Settle)
	{
		const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots/range") / (FString(S.Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		const double Fps = static_cast<double>(GFrameCounter - ShotFrameStart) / FMath::Max(ShotClock, 0.1f);
		UE_LOG(LogTemp, Log, TEXT("[Holdover] Range shot %s, %.0f fps while settling"), *File, Fps);
		ShotClock = 0.f;
		++ShotStep;
	}
}
