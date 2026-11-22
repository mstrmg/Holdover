#include "Range/HoldoverForestRange.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "CoreGlobals.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Modules/ModuleManager.h"
#include "ProceduralMeshComponent.h"

namespace ForestRange
{
	// Ground surfaces, one mesh section each.
	enum ESurface : int32 { Forest = 0, Grass = 1, Gravel = 2, Dirt = 3, Earth = 4, NumSurfaces = 5 };

	const TCHAR* SurfaceMaterialNames[NumSurfaces] = {
		TEXT("MI_ForestFloor"), TEXT("MI_LaneGrass"), TEXT("MI_Gravel"), TEXT("MI_DirtPath"), TEXT("MI_BermEarth") };

	// Stand-in colours (linear), chosen from summer forest photos.
	const FLinearColor SurfaceColors[NumSurfaces] = {
		FLinearColor(0.060f, 0.048f, 0.030f), // leaf litter and soil
		FLinearColor(0.075f, 0.110f, 0.035f), // mown grass
		FLinearColor(0.210f, 0.200f, 0.180f), // gravel
		FLinearColor(0.120f, 0.085f, 0.050f), // trodden dirt
		FLinearColor(0.130f, 0.090f, 0.055f), // bare berm earth
	};

	const FLinearColor Bark[] = {
		FLinearColor(0.055f, 0.047f, 0.040f), // dark oak
		FLinearColor(0.085f, 0.075f, 0.062f), // maple
		FLinearColor(0.150f, 0.145f, 0.130f), // beech grey
		FLinearColor(0.070f, 0.060f, 0.045f),
	};
	const FLinearColor Leaves[] = {
		FLinearColor(0.035f, 0.075f, 0.015f), // deep shade green
		FLinearColor(0.055f, 0.115f, 0.020f),
		FLinearColor(0.080f, 0.150f, 0.030f),
		FLinearColor(0.110f, 0.190f, 0.035f), // sunlit young leaves
		FLinearColor(0.060f, 0.100f, 0.030f), // olive
	};
	const FLinearColor Ferns[] = {
		FLinearColor(0.050f, 0.120f, 0.025f),
		FLinearColor(0.080f, 0.160f, 0.035f),
		FLinearColor(0.070f, 0.110f, 0.040f),
	};
	const FLinearColor Rocks[] = {
		FLinearColor(0.160f, 0.160f, 0.150f),
		FLinearColor(0.090f, 0.110f, 0.055f), // moss
		FLinearColor(0.120f, 0.115f, 0.100f),
	};
	const FLinearColor DeadWood[] = {
		FLinearColor(0.090f, 0.070f, 0.050f),
		FLinearColor(0.120f, 0.100f, 0.075f),
	};

	constexpr int32 NumBark = UE_ARRAY_COUNT(Bark);
	constexpr int32 NumLeaves = UE_ARRAY_COUNT(Leaves);
	constexpr int32 NumFerns = UE_ARRAY_COUNT(Ferns);
	constexpr int32 NumRocks = UE_ARRAY_COUNT(Rocks);
	constexpr int32 NumDeadWood = UE_ARRAY_COUNT(DeadWood);

	// Tone ids keep batches of different colours apart.
	constexpr int32 ToneBark = 0, ToneLeaves = 10, ToneFern = 20, ToneRock = 30, ToneDeadWood = 40, ToneGrass = 50;

	// Path from the parking area behind the range to the firing pad (metres, actor space).
	const FVector2D AccessPath[] = {
		FVector2D(-7.f, -7.f), FVector2D(-12.f, -9.5f), FVector2D(-20.f, -11.f), FVector2D(-30.f, -15.f),
		FVector2D(-42.f, -19.f), FVector2D(-56.f, -20.f), FVector2D(-70.f, -18.f), FVector2D(-95.f, -23.f) };
	constexpr int32 NumPathPoints = UE_ARRAY_COUNT(AccessPath);

	// Cleared range area (no trees): behind the pad to past the back berm, out to the side berm crests.
	constexpr float ClearBehind = -12.f;
	constexpr float ClearSideExtra = 5.f;   // beyond the lane half width
	constexpr float ClearPastBermToe = 10.f;

	// Inspection walkway down the left side of the lane.
	constexpr float WalkwayY = -8.5f;
	constexpr float WalkwayHalf = 0.7f;

	float BermProfile(float D, float Toe, float CrestStart, float CrestEnd, float BackToe, float Height)
	{
		if (D <= Toe || D >= BackToe)
		{
			return 0.f;
		}
		if (D < CrestStart)
		{
			return Height * FMath::SmoothStep(Toe, CrestStart, D);
		}
		if (D <= CrestEnd)
		{
			return Height;
		}
		return Height * (1.f - FMath::SmoothStep(CrestEnd, BackToe, D));
	}

	/** Smooth wave in [-1, 1] from three sines; the same formula as wobble() in tools/range/forest_layout.py. */
	float Wobble(float T, float Phase)
	{
		return 0.55f * FMath::Sin(T * 0.083f + Phase) + 0.30f * FMath::Sin(T * 0.197f + Phase * 2.3f)
			+ 0.15f * FMath::Sin(T * 0.471f + Phase * 3.7f);
	}

	float SegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const float Len2 = FMath::Max(static_cast<float>(AB.SizeSquared()), 1e-4f);
		const float T = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(P - A, AB)) / Len2, 0.f, 1.f);
		return static_cast<float>(FVector2D::Distance(P, A + AB * T));
	}
}

using namespace ForestRange;

AHoldoverForestRange::AHoldoverForestRange()
{
	// Everything here stays put: static lets Lumen and the shadow cache treat it cheaply.
	Root->SetMobility(EComponentMobility::Static);

	// Targets out to 150 m now the lane is about 190 m long (the gray range keeps 5 to 50 m).
	TargetDistances = { 5.f, 7.f, 10.f, 15.f, 25.f, 50.f, 75.f, 100.f, 150.f };

	// Ticks only to serve holdover.RangeShots; otherwise it returns at once.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
}

void AHoldoverForestRange::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateRangeShots(DeltaSeconds);
}

void AHoldoverForestRange::UpdateRangeShots(float DeltaSeconds)
{
	const int32 Mode = HoldoverDebug::RangeShots();
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (Mode <= 0 || !PC)
	{
		return;
	}
#if WITH_EDITOR
	// Wait until materials have finished compiling, or the pictures show grey stand-in materials.
	if (GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0)
	{
		return;
	}
#endif
	ShotClock += DeltaSeconds;

	// Spots in metres (actor space) with eye height above the ground, and where to look.
	struct FSpot { const TCHAR* Name; float X, Y, Up, Pitch, Yaw; };
	static const FSpot Spots[] = {
		{ TEXT("01_firing_line"), -1.5f, 0.f, 1.65f, -2.f, 0.f },
		{ TEXT("02_left_signs"), -1.5f, 0.f, 1.65f, -4.f, -38.f },
		{ TEXT("03_right_edge"), -1.5f, 0.f, 1.65f, 0.f, 55.f },
		{ TEXT("04_benches"), -4.f, -2.f, 1.65f, -18.f, 30.f },
		{ TEXT("05_walkway_back"), 95.f, -8.5f, 1.65f, -2.f, 180.f },
		{ TEXT("06_backstop"), 178.f, -3.f, 1.65f, 2.f, 8.f },
		{ TEXT("07_in_the_forest"), -25.f, 30.f, 1.65f, 4.f, 35.f },
		{ TEXT("08_path_arrival"), -20.f, -12.f, 1.65f, -2.f, 15.f },
		{ TEXT("09_canopy_up"), -30.f, -40.f, 1.65f, 55.f, 60.f },
		{ TEXT("10_overview"), -11.f, 0.f, 20.f, -22.f, 0.f }, // above the cleared lane, clear of tree crowns
		{ TEXT("11_berm_close"), 20.f, -6.f, 1.65f, -12.f, -60.f }, // close look at the left berm and its grass
	};
	constexpr int32 NumSpots = UE_ARRAY_COUNT(Spots);
	constexpr float Settle = 6.f; // seconds for light and eye adaptation to settle after each cut

	if (ShotStep == 0)
	{
		// Hide yourself (body, gun) so the pictures show only the range.
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
		ACameraActor* Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		if (Cam)
		{
			Cam->GetCameraComponent()->SetFieldOfView(90.f);
			Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
			PC->SetViewTarget(Cam);
			ShotCamera = Cam;
		}
		ShotStep = 1;
		ShotClock = 0.f;
	}

	// Step 2k-1: move the camera to spot k. Step 2k: after the settle time, take its picture.
	const int32 Spot = (ShotStep - 1) / 2;
	if (Spot >= NumSpots)
	{
		if (Mode == 1)
		{
			HoldoverDebug::CVarRangeShots->Set(0, ECVF_SetByConsole);
			PC->ConsoleCommand(TEXT("quit"));
		}
		return;
	}
	const FSpot& S = Spots[Spot];
	if ((ShotStep % 2) == 1)
	{
		if (ShotCamera)
		{
			const FVector Local(S.X * 100.f, S.Y * 100.f, GroundHeight(S.X * 100.f, S.Y * 100.f) + S.Up * 100.f);
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

// ---------------------------------------------------------------------------
// Ground shape

float AHoldoverForestRange::LaneEdge(float X, float Y) const
{
	return LaneHalfWidth + LaneEdgeWander * Wobble(X, Y >= 0.f ? 1.1f : 4.2f);
}

float AHoldoverForestRange::BackToe(float Y) const
{
	return BackBermToe + BackToeWander * Wobble(Y, 6.1f);
}

float AHoldoverForestRange::TreeLine(float X, float Y) const
{
	return LaneEdge(X, Y) + ClearSideExtra + TreeLineWander * Wobble(X, Y >= 0.f ? 7.3f : 8.9f);
}

float AHoldoverForestRange::BermHeightMetres(float X, float Y) const
{
	const float AY = FMath::Abs(Y);
	const float L = LaneEdge(X, Y);
	const float Ph = Y >= 0.f ? 2.9f : 5.3f;
	const float H = SideBermHeight * (1.f + BermWander * Wobble(X, Ph));
	const float W = 1.f + BermWander * Wobble(X, Ph + 1.7f);

	// Side berms: rise over about 3.5 m, a short crest, long back slope into the forest; all of it wanders.
	float Side = BermProfile(AY, L, L + 3.5f * W, L + 4.5f * W, L + 8.f * W, H);
	Side *= FMath::SmoothStep(-6.f, -3.f, X);
	Side *= 1.f - FMath::SmoothStep(BackBermToe + 10.f, BackBermToe + 14.f, X);

	// Backstop berm: about 45 degrees on the face, 1.5 m crest, back slope.
	const float T = BackToe(Y);
	float Back = BermProfile(X, T, T + 5.f, T + 6.5f, T + 13.f, BackBermHeight);
	Back *= 1.f - FMath::SmoothStep(L + 6.f, L + 12.f, AY);

	return FMath::Max(Side, Back);
}

float AHoldoverForestRange::DistanceToAccessPath(float X, float Y) const
{
	const FVector2D P(X, Y);
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 0; i + 1 < NumPathPoints; ++i)
	{
		Best = FMath::Min(Best, SegmentDistance(P, AccessPath[i], AccessPath[i + 1]));
	}
	return Best;
}

float AHoldoverForestRange::GroundHeightMetres(float X, float Y) const
{
	const float Berm = BermHeightMetres(X, Y);

	// Distance outside the cleared range area: the forest floor rolls gently beyond it.
	const float ClearAhead = BackBermToe + ClearPastBermToe + TreeLineWander * Wobble(Y, 9.4f);
	const float ClearSide = TreeLine(X, Y);
	const float Dx = FMath::Max3(ClearBehind - X, 0.f, X - ClearAhead);
	const float Dy = FMath::Max(FMath::Abs(Y) - ClearSide, 0.f);
	float Mask = FMath::SmoothStep(0.f, 8.f, FMath::Sqrt(Dx * Dx + Dy * Dy));
	// Paths are worn flatter.
	Mask *= FMath::Lerp(0.35f, 1.f, FMath::SmoothStep(1.5f, 5.f, DistanceToAccessPath(X, Y)));

	const float Seedf = static_cast<float>(Seed % 1000);
	const float Roll =
		FMath::PerlinNoise2D(FVector2D(X / 30.f + Seedf, Y / 30.f - Seedf)) * 0.80f +
		FMath::PerlinNoise2D(FVector2D(X / 9.f + 17.3f, Y / 9.f - 4.1f)) * 0.25f +
		FMath::PerlinNoise2D(FVector2D(X / 2.5f - 7.7f, Y / 2.5f + 2.2f)) * 0.05f;

	return Berm + Roll * ForestFloorRoll * Mask;
}

float AHoldoverForestRange::GroundHeight(float X, float Y) const
{
	if (ImportedGround)
	{
		FHitResult Hit;
		const FVector WStart = GetActorTransform().TransformPosition(FVector(X, Y, 5000.f));
		const FVector WEnd = GetActorTransform().TransformPosition(FVector(X, Y, -5000.f));
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestGround), true);
		if (ImportedGround->LineTraceComponent(Hit, WStart, WEnd, Params))
		{
			return static_cast<float>(GetActorTransform().InverseTransformPosition(Hit.ImpactPoint).Z);
		}
		return 0.f;
	}
	return GroundHeightMetres(X / 100.f, Y / 100.f) * 100.f;
}

FVector AHoldoverForestRange::OnGround(float Xm, float Ym, float Sink) const
{
	return FVector(Xm * 100.f, Ym * 100.f, GroundHeight(Xm * 100.f, Ym * 100.f) - Sink);
}

int32 AHoldoverForestRange::SurfaceAt(float X, float Y) const
{
	const float AY = FMath::Abs(Y);
	if (X < -PadDepth && DistanceToAccessPath(X, Y) < 1.1f)
	{
		return Dirt;
	}
	const float L = LaneEdge(X, Y);
	const float PadEdge = LaneHalfWidth - 1.f + 0.6f * Wobble(Y * 3.f, 0.4f);
	if (X >= -PadDepth + 0.5f * Wobble(Y * 2.f, 3.3f) && X <= 1.f + 0.4f * Wobble(Y * 2.f, 1.9f) && AY <= PadEdge)
	{
		return Gravel;
	}
	const float T = BackToe(Y);
	const float Berm = BermHeightMetres(X, Y);
	if (Berm > 0.25f)
	{
		// The backstop face is bare earth where bullets land; the side berms are grassed, then the forest takes over.
		if (X > T - 1.f && AY < L + 4.f)
		{
			return Earth;
		}
		if (AY < L + 4.5f + 2.f * Wobble(X, Y >= 0.f ? 3.9f : 6.6f))
		{
			return Grass;
		}
		return Forest;
	}
	if (X > 1.f && X < T + 0.5f && AY <= L + 0.5f)
	{
		if (FMath::Abs(Y - WalkwayY) < WalkwayHalf && X < T - 2.f)
		{
			return Dirt;
		}
		// Tongues of leaf litter reaching a few metres into the lane from the forest side.
		const float Reach = 1.f + 2.5f * (0.5f + 0.5f * Wobble(X * 2.3f, Y >= 0.f ? 2.2f : 5.1f));
		if (L - AY < Reach * (0.5f + 0.5f * Wobble(X * 5.1f, 0.9f)))
		{
			return Forest;
		}
		return Grass;
	}
	return Forest;
}

bool AHoldoverForestRange::IsClearedForTrees(float X, float Y, float Margin) const
{
	const float Ahead = BackBermToe + ClearPastBermToe + TreeLineWander * Wobble(Y, 9.4f);
	if (X > ClearBehind - Margin && X < Ahead + Margin && FMath::Abs(Y) < TreeLine(X, Y) + Margin)
	{
		return true;
	}
	if (DistanceToAccessPath(X, Y) < 2.f + Margin)
	{
		return true;
	}
	return false;
}

bool AHoldoverForestRange::IsBareGround(float X, float Y) const
{
	// Where nothing taller than grass may grow: the lane floor, the pad, the backstop face, the walkway and paths.
	const int32 S = SurfaceAt(X, Y);
	if (S == Gravel || S == Earth || S == Dirt)
	{
		return true;
	}
	return X > ClearBehind && X < BackToe(Y) + 1.f && FMath::Abs(Y) < LaneEdge(X, Y) + 1.f;
}

// ---------------------------------------------------------------------------
// Build

void AHoldoverForestRange::BuildGeometry()
{
	const double Start = FPlatformTime::Seconds();

	KitRocks.Reset();
	KitLogs.Reset();
	KitStumps.Reset();
	KitBranches.Reset();
	for (const TCHAR* N : { TEXT("SM_Rock_S"), TEXT("SM_Rock_M"), TEXT("SM_Rock_L") })
	{
		if (UStaticMesh* M = FindMesh(KitFolder, { N })) { KitRocks.Add(M); }
	}
	for (const TCHAR* N : { TEXT("SM_Log_Short"), TEXT("SM_Log_Long"), TEXT("SM_Log_Thick") })
	{
		if (UStaticMesh* M = FindMesh(KitFolder, { N })) { KitLogs.Add(M); }
	}
	for (const TCHAR* N : { TEXT("SM_Stump_S"), TEXT("SM_Stump_L") })
	{
		if (UStaticMesh* M = FindMesh(KitFolder, { N })) { KitStumps.Add(M); }
	}
	if (UStaticMesh* M = FindMesh(KitFolder, { TEXT("SM_Branches") })) { KitBranches.Add(M); }

	FindMegaplants();
	BuildGround();
	PlaceRangeProps();

	// Each layer gets its own stream, so changing one density doesn't reshuffle the others.
	FRandomStream TreeRng(Seed);
	FRandomStream SaplingRng(Seed + 101);
	FRandomStream UnderRng(Seed + 202);
	FRandomStream DebrisRng(Seed + 303);
	GrowTrees(TreeRng);
	GrowSaplings(SaplingRng);
	GrowUndergrowth(UnderRng);
	ScatterDebris(DebrisRng);
	FlushInstances();
	FlushSkinned();

	int32 Instances = 0;
	for (const auto& Pair : Batches)
	{
		if (Pair.Value)
		{
			Instances += Pair.Value->GetInstanceCount();
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Forest range built in %.2f s: %d instanced pieces in %d batches, %d Megaplant components, ground %s."),
		FPlatformTime::Seconds() - Start, Instances, Batches.Num(), SkinnedComponents.Num(), ImportedGround ? TEXT("imported") : TEXT("generated"));
}

void AHoldoverForestRange::BuildGround()
{
	if (UStaticMesh* Terrain = FindMesh(GroundFolder, { TEXT("SM_ForestTerrain") }))
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, TEXT("ForestTerrain"));
		C->SetStaticMesh(Terrain);
		C->SetMobility(EComponentMobility::Static);
		C->SetupAttachment(Root);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
		C->RegisterComponent();
		AddInstanceComponent(C);
		ImportedGround = C;

		// The Blender terrain's material slots are named after the surfaces (ForestFloor, LaneGrass...).
		// Swap in the Megascans-based MI_<slot> materials when they exist.
		const TArray<FStaticMaterial>& Slots = Terrain->GetStaticMaterials();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (UMaterialInterface* Mat = FindMaterial(GroundFolder, { TEXT("MI_") + Slots[i].MaterialSlotName.ToString() }))
			{
				C->SetMaterial(i, Mat);
			}
		}
	}
	else
	{
		BuildProceduralGround();
	}

	// A wide floor far below the edges, so the world never ends in a void.
	UStaticMeshComponent* Skirt = HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cube(),
		FVector(1500.f, 0.f, -200.f), FRotator::ZeroRotator, FVector(200000.f, 200000.f, 100.f), SurfaceColors[Forest], true);
	if (Skirt)
	{
		Skirt->SetCastShadow(false);
	}
}

void AHoldoverForestRange::BuildProceduralGround()
{
	GroundMesh = NewObject<UProceduralMeshComponent>(this, TEXT("ForestGround"));
	GroundMesh->SetMobility(EComponentMobility::Static);
	GroundMesh->SetupAttachment(Root);
	GroundMesh->bUseComplexAsSimpleCollision = true;
	GroundMesh->bUseAsyncCooking = false; // collision must exist before the player lands
	GroundMesh->SetCollisionProfileName(TEXT("BlockAll"));
	GroundMesh->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
	GroundMesh->RegisterComponent();
	AddInstanceComponent(GroundMesh);

	struct FSection
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
	};
	FSection Sections[NumSurfaces];

	constexpr float Cell = 1.f; // metres
	const float X0 = -GroundBehind, X1 = GroundAhead;
	const float Y0 = -GroundHalfWidth, Y1 = GroundHalfWidth;

	auto Normal = [this](float X, float Y)
	{
		constexpr float E = 0.5f;
		const float Dx = GroundHeightMetres(X + E, Y) - GroundHeightMetres(X - E, Y);
		const float Dy = GroundHeightMetres(X, Y + E) - GroundHeightMetres(X, Y - E);
		return FVector(-Dx, -Dy, 2.f * E).GetSafeNormal();
	};

	for (float X = X0; X < X1; X += Cell)
	{
		for (float Y = Y0; Y < Y1; Y += Cell)
		{
			FSection& S = Sections[SurfaceAt(X + Cell * 0.5f, Y + Cell * 0.5f)];
			const int32 Base = S.Verts.Num();
			const float Xs[4] = { X, X + Cell, X + Cell, X };
			const float Ys[4] = { Y, Y, Y + Cell, Y + Cell };
			for (int32 k = 0; k < 4; ++k)
			{
				S.Verts.Add(FVector(Xs[k] * 100.f, Ys[k] * 100.f, GroundHeightMetres(Xs[k], Ys[k]) * 100.f));
				S.Normals.Add(Normal(Xs[k], Ys[k]));
				S.UVs.Add(FVector2D(Xs[k] / 4.f, Ys[k] / 4.f)); // textures repeat every 4 m
			}
			// Front faces point along (B-A)x(C-A): up.
			S.Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		}
	}

	UMaterialInterface* BaseMat = HoldoverShapes::BaseMaterial();
	for (int32 i = 0; i < NumSurfaces; ++i)
	{
		FSection& S = Sections[i];
		if (S.Verts.Num() == 0)
		{
			continue;
		}
		GroundMesh->CreateMeshSection_LinearColor(i, S.Verts, S.Tris, S.Normals, S.UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
		if (UMaterialInterface* Mat = FindMaterial(GroundFolder, { SurfaceMaterialNames[i] }))
		{
			GroundMesh->SetMaterial(i, Mat);
		}
		else if (BaseMat)
		{
			if (UMaterialInstanceDynamic* MID = GroundMesh->CreateDynamicMaterialInstance(i, BaseMat))
			{
				MID->SetVectorParameterValue(TEXT("Color"), SurfaceColors[i]);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Instancing

UHierarchicalInstancedStaticMeshComponent* AHoldoverForestRange::Batch(UStaticMesh* Mesh, int32 Tone, const FLinearColor* Tint, bool bCollide, float CullMetres)
{
	if (!Mesh)
	{
		return nullptr;
	}
	const FName Key(*FString::Printf(TEXT("%s_%d_%d_%d"), *Mesh->GetPathName(), Tone, bCollide ? 1 : 0, FMath::RoundToInt(CullMetres)));
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
	}
	else
	{
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetGenerateOverlapEvents(false);
	}
	if (CullMetres > 0.f)
	{
		H->SetCullDistances(FMath::RoundToInt(CullMetres * 85.f), FMath::RoundToInt(CullMetres * 100.f));
	}
	H->RegisterComponent();
	AddInstanceComponent(H);

	if (Tint)
	{
		if (UMaterialInterface* BaseMat = HoldoverShapes::BaseMaterial())
		{
			if (UMaterialInstanceDynamic* MID = H->CreateDynamicMaterialInstance(0, BaseMat))
			{
				MID->SetVectorParameterValue(TEXT("Color"), *Tint);
			}
		}
	}

	Batches.Add(Key, H);
	return H;
}

void AHoldoverForestRange::AddInstance(UStaticMesh* Mesh, const FTransform& T, int32 Tone, const FLinearColor* Tint, bool bCollide, float CullMetres)
{
	if (UHierarchicalInstancedStaticMeshComponent* H = Batch(Mesh, Tone, Tint, bCollide, CullMetres))
	{
		PendingInstances.FindOrAdd(H).Add(T);
	}
}

void AHoldoverForestRange::FlushInstances()
{
	for (auto& Pair : PendingInstances)
	{
		if (Pair.Key && Pair.Value.Num() > 0)
		{
			Pair.Key->AddInstances(Pair.Value, false);
		}
	}
	PendingInstances.Reset();
}

void AHoldoverForestRange::AddLimb(const FVector& A, const FVector& B, float Radius, int32 BarkTone, bool bCollide)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < 1.f)
	{
		return;
	}
	const int32 Idx = FMath::Abs(BarkTone) % NumBark;
	const FTransform T(FRotationMatrix::MakeFromZ(D / Len).Rotator(), (A + B) * 0.5f, FVector(Radius / 50.f, Radius / 50.f, Len / 100.f));
	AddInstance(HoldoverShapes::Cylinder(), T, ToneBark + Idx, &Bark[Idx], bCollide);
}

// ---------------------------------------------------------------------------
// Trees and plants

void AHoldoverForestRange::StandInTree(FRandomStream& Rng, const FVector& Base, float HeightCm, float RadiusCm, bool bSapling)
{
	UStaticMesh* Sphere = HoldoverShapes::Sphere();
	const int32 BarkTone = Rng.RandRange(0, NumBark - 1);
	const int32 LeafTone = bSapling ? Rng.RandRange(2, 3) : Rng.RandRange(0, NumLeaves - 1);

	// Lean: every tree grows a little off vertical, towards its own light.
	const float LeanDeg = Rng.FRandRange(0.f, bSapling ? 9.f : 4.5f);
	const float LeanYaw = Rng.FRandRange(0.f, 360.f);
	FVector Dir = FRotator(0.f, LeanYaw, 0.f).RotateVector(FVector(FMath::Sin(FMath::DegreesToRadians(LeanDeg)), 0.f, FMath::Cos(FMath::DegreesToRadians(LeanDeg))));

	// Root flare on big trees.
	if (!bSapling)
	{
		AddLimb(Base - FVector(0, 0, 40.f), Base + FVector(0, 0, 45.f), RadiusCm * 1.3f, BarkTone, true);
	}

	// Trunk: tapering segments with a slight wander, up to about 85% of the height.
	const int32 Segments = bSapling ? 2 : 4;
	const float TrunkLen = HeightCm * 0.85f;
	FVector P = Base - FVector(0, 0, 30.f);
	TArray<FVector> Spine;
	Spine.Add(P);
	for (int32 s = 0; s < Segments; ++s)
	{
		const float R = RadiusCm * FMath::Lerp(1.f, 0.4f, static_cast<float>(s) / Segments);
		const FVector Next = P + Dir * (TrunkLen / Segments);
		AddLimb(P, Next, R, BarkTone, s < 2);
		P = Next;
		Spine.Add(P);
		Dir = (Dir + Rng.GetUnitVector() * 0.05f).GetSafeNormal();
	}
	auto AlongTrunk = [&Spine](float T)
	{
		const float F = FMath::Clamp(T, 0.f, 0.999f) * (Spine.Num() - 1);
		const int32 I = FMath::FloorToInt(F);
		return FMath::Lerp(Spine[I], Spine[I + 1], F - I);
	};

	// Branches with leaf clusters. Forest trees are bare low down and leafy up top.
	const float CrownStart = bSapling ? Rng.FRandRange(0.3f, 0.5f) : Rng.FRandRange(0.45f, 0.6f);
	const int32 NumBranches = bSapling ? Rng.RandRange(2, 3) : Rng.RandRange(4, 7);
	for (int32 b = 0; b < NumBranches; ++b)
	{
		const float T = FMath::Lerp(CrownStart, 0.92f, (b + Rng.FRand()) / NumBranches);
		const FVector From = AlongTrunk(T);
		const float Yaw = Rng.FRandRange(0.f, 360.f);
		const float Up = Rng.FRandRange(25.f, 60.f);
		const float Len = HeightCm * Rng.FRandRange(0.10f, 0.22f) * (1.15f - T * 0.4f);
		const FVector BranchDir = FRotator(Up, Yaw, 0.f).Vector();
		const FVector To = From + BranchDir * Len;
		AddLimb(From, To, FMath::Max(RadiusCm * Rng.FRandRange(0.15f, 0.28f), 1.5f), BarkTone, false);

		// One or two leaf clusters at the end of the branch (few, big clusters keep the triangle count sane).
		const int32 Blobs = bSapling ? 1 : Rng.RandRange(2, 3);
		for (int32 k = 0; k < Blobs; ++k)
		{
			const float Size = Len * Rng.FRandRange(1.0f, 1.5f) * (bSapling ? 1.2f : 1.f);
			const FVector C = FMath::Lerp(From, To, Rng.FRandRange(0.6f, 1.1f)) + Rng.GetUnitVector() * Size * 0.2f;
			const int32 Tone = (Rng.FRand() < 0.25f) ? Rng.RandRange(0, NumLeaves - 1) : LeafTone;
			const FTransform BT(FRotator(Rng.FRandRange(-15.f, 15.f), Rng.FRandRange(0.f, 360.f), 0.f), C,
				FVector(Size, Size * Rng.FRandRange(0.75f, 1.f), Size * Rng.FRandRange(0.55f, 0.8f)) / 100.f);
			AddInstance(Sphere, BT, ToneLeaves + Tone, &Leaves[Tone], false);
		}
	}

	// Crown top.
	const int32 TopBlobs = bSapling ? 1 : Rng.RandRange(3, 4);
	const FVector Top = Spine.Last();
	for (int32 k = 0; k < TopBlobs; ++k)
	{
		const float Size = HeightCm * Rng.FRandRange(0.18f, 0.28f);
		const FVector C = Top + FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-0.6f, 0.2f)) * HeightCm * 0.08f;
		const int32 Tone = (k == 0) ? LeafTone : Rng.RandRange(0, NumLeaves - 1);
		const FTransform BT(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), C, FVector(Size, Size * Rng.FRandRange(0.8f, 1.f), Size * Rng.FRandRange(0.6f, 0.85f)) / 100.f);
		AddInstance(Sphere, BT, ToneLeaves + Tone, &Leaves[Tone], false);
	}
}

void AHoldoverForestRange::GrowTrees(FRandomStream& Rng)
{
	const TArray<UStaticMesh*> Meshes = MeshesIn(TreeFolder);
	constexpr float Cell = 5.5f; // about 330 trees a hectare, a mature hardwood stand
	const float Keep = FMath::Clamp(0.9f * TreeDensity, 0.f, 1.f);

	for (float X = -GroundBehind; X < GroundAhead; X += Cell)
	{
		for (float Y = -GroundHalfWidth; Y < GroundHalfWidth; Y += Cell)
		{
			const float PX = X + Rng.FRand() * Cell;
			const float PY = Y + Rng.FRand() * Cell;
			const float Roll = Rng.FRand();
			if (Roll > Keep)
			{
				continue;
			}
			if (IsClearedForTrees(PX, PY, 0.f))
			{
				// A few lone trees stand inside the clearing too, on the berm backs, well off the lane.
				const float OffLane = FMath::Abs(PY) - LaneEdge(PX, PY);
				if (OffLane < 4.f || IsBareGround(PX, PY) || DistanceToAccessPath(PX, PY) < 3.f || PX < 2.f || PX > BackToe(PY) - 2.f
					|| Rng.FRand() > 0.18f)
				{
					continue;
				}
			}
			if (TreeSet.Num() > 0)
			{
				// Real trees (Megaplants): each its own size and turn, standing nearly straight.
				const float S = Rng.FRandRange(0.8f, 1.2f);
				const FTransform T(FRotator(Rng.FRandRange(-2.f, 2.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-2.f, 2.f)),
					OnGround(PX, PY, 15.f), FVector(S * Rng.FRandRange(0.95f, 1.05f), S * Rng.FRandRange(0.95f, 1.05f), S));
				PlacePlant(TreeSet, Rng, T, true, 0.f);
			}
			else if (Meshes.Num() > 0)
			{
				UStaticMesh* M = Meshes[Rng.RandRange(0, Meshes.Num() - 1)];
				const float S = Rng.FRandRange(0.8f, 1.25f);
				const FTransform T(FRotator(Rng.FRandRange(-2.5f, 2.5f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-2.5f, 2.5f)),
					OnGround(PX, PY, 15.f), FVector(S * Rng.FRandRange(0.95f, 1.05f), S * Rng.FRandRange(0.95f, 1.05f), S));
				AddInstance(M, T, -1, nullptr, true);
			}
			else
			{
				// Taller, slimmer trees deep in the stand; a few big old ones.
				const bool bVeteran = Rng.FRand() < 0.08f;
				const float H = Rng.FRandRange(1500.f, 2400.f) * (bVeteran ? 1.15f : 1.f);
				const float R = Rng.FRandRange(16.f, 30.f) * (bVeteran ? 1.6f : 1.f);
				StandInTree(Rng, OnGround(PX, PY), H, R, false);
			}
		}
	}
}

void AHoldoverForestRange::GrowSaplings(FRandomStream& Rng)
{
	const TArray<UStaticMesh*> Meshes = MeshesIn(SaplingFolder);
	constexpr float Cell = 4.5f;
	for (float X = -GroundBehind; X < GroundAhead; X += Cell)
	{
		for (float Y = -GroundHalfWidth; Y < GroundHalfWidth; Y += Cell)
		{
			const float PX = X + Rng.FRand() * Cell;
			const float PY = Y + Rng.FRand() * Cell;
			// Young trees crowd the sunny edge of the clearing.
			const bool bEdge = !IsClearedForTrees(PX, PY, 0.f) && IsClearedForTrees(PX, PY, 7.f);
			float Chance = (bEdge ? 0.8f : 0.4f) * SaplingDensity;
			if (IsClearedForTrees(PX, PY, -0.5f))
			{
				// Young trees and shrubs creep in from the edge, never onto the lane itself.
				const float OffLane = FMath::Abs(PY) - LaneEdge(PX, PY);
				Chance = (OffLane < 1.5f || IsBareGround(PX, PY) || DistanceToAccessPath(PX, PY) < 2.f) ? 0.f : 0.3f * SaplingDensity;
			}
			if (Rng.FRand() > Chance)
			{
				continue;
			}
			if (YoungSet.Num() > 0)
			{
				const float S = Rng.FRandRange(0.7f, 1.25f);
				const FTransform T(FRotator(Rng.FRandRange(-4.f, 4.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-4.f, 4.f)), OnGround(PX, PY, 5.f), FVector(S));
				PlacePlant(YoungSet, Rng, T, false, SmallThingsDrawDistance * 1.5f);
			}
			else if (Meshes.Num() > 0)
			{
				UStaticMesh* M = Meshes[Rng.RandRange(0, Meshes.Num() - 1)];
				const float S = Rng.FRandRange(0.7f, 1.3f);
				const FTransform T(FRotator(Rng.FRandRange(-5.f, 5.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-5.f, 5.f)), OnGround(PX, PY, 5.f), FVector(S));
				AddInstance(M, T, -1, nullptr, false, SmallThingsDrawDistance * 1.5f);
			}
			else
			{
				StandInTree(Rng, OnGround(PX, PY), Rng.FRandRange(250.f, 800.f), Rng.FRandRange(2.5f, 6.f), true);
			}
		}
	}
}

void AHoldoverForestRange::GrowUndergrowth(FRandomStream& Rng)
{
	const TArray<UStaticMesh*> Plants = MeshesIn(PlantFolder);
	const TArray<UStaticMesh*> GrassMeshes = MeshesIn(GrassFolder);
	UStaticMesh* Sphere = HoldoverShapes::Sphere();
	const float Cull = SmallThingsDrawDistance;

	// Ferns and bushes on the forest floor, thick along the clearing edge.
	constexpr float Cell = 2.5f;
	for (float X = -GroundBehind; X < GroundAhead; X += Cell)
	{
		for (float Y = -GroundHalfWidth; Y < GroundHalfWidth; Y += Cell)
		{
			const float PX = X + Rng.FRand() * Cell;
			const float PY = Y + Rng.FRand() * Cell;
			const bool bEdge = !IsClearedForTrees(PX, PY, 0.f) && IsClearedForTrees(PX, PY, 6.f);
			// Patchy: ferns grow in drifts, not evenly.
			const float Patch = 0.5f + 0.5f * FMath::PerlinNoise2D(FVector2D(PX / 12.f + 3.3f, PY / 12.f - 8.1f));
			// Inside the clearing too (berm backs, the strip before the tree line), just thinner: the range
			// blends into the forest instead of stopping at a line.
			const bool bInClearing = IsClearedForTrees(PX, PY, -0.3f);
			float Chance = (bEdge ? 0.9f : 0.25f + 0.5f * Patch) * UndergrowthDensity;
			if (bInClearing)
			{
				Chance = IsBareGround(PX, PY) || DistanceToAccessPath(PX, PY) < 1.5f ? 0.f : (0.15f + 0.35f * Patch) * UndergrowthDensity;
			}
			if (Rng.FRand() > Chance)
			{
				continue;
			}
			if (PlantSet.Num() > 0)
			{
				const float S = Rng.FRandRange(0.7f, 1.3f);
				const FTransform T(FRotator(Rng.FRandRange(-5.f, 5.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-5.f, 5.f)), OnGround(PX, PY, 3.f), FVector(S));
				PlacePlant(PlantSet, Rng, T, false, Cull);
			}
			else if (Plants.Num() > 0)
			{
				UStaticMesh* M = Plants[Rng.RandRange(0, Plants.Num() - 1)];
				const float S = Rng.FRandRange(0.7f, 1.35f);
				const FTransform T(FRotator(Rng.FRandRange(-6.f, 6.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-6.f, 6.f)), OnGround(PX, PY, 3.f), FVector(S));
				AddInstance(M, T, -1, nullptr, false, Cull);
			}
			else
			{
				// A fern: fronds rising from one crown and arching outwards, each a long thin leaf shape.
				const FVector C = OnGround(PX, PY);
				const int32 Fronds = Rng.RandRange(5, 8);
				const float Size = Rng.FRandRange(0.6f, 1.25f);
				const int32 Tone = Rng.RandRange(0, NumFerns - 1);
				const float Yaw0 = Rng.FRandRange(0.f, 360.f);
				for (int32 k = 0; k < Fronds; ++k)
				{
					const float Len = Rng.FRandRange(55.f, 95.f) * Size;
					const float Yaw = Yaw0 + k * (360.f / Fronds) + Rng.FRandRange(-15.f, 15.f);
					const float Rise = Rng.FRandRange(20.f, 40.f); // frond angle above the ground
					const FVector Dir = FRotator(Rise, Yaw, 0.f).Vector();
					const FTransform T(FRotator(Rise, Yaw, Rng.FRandRange(-10.f, 10.f)), C + Dir * Len * 0.5f,
						FVector(Len, Len * Rng.FRandRange(0.22f, 0.32f), 4.f) / 100.f);
					AddInstance(Sphere, T, ToneFern + Tone, &Ferns[Tone], false, Cull * 0.7f);
				}
			}
		}
	}

	// Taller grass where the mower doesn't reach: the foot of the side berms, around the pad, and on the berms.
	constexpr float GrassCell = 1.2f;
	for (float X = -PadDepth - 2.f; X < BackBermToe + 4.f; X += GrassCell)
	{
		for (float Y = -LaneHalfWidth - 6.f; Y < LaneHalfWidth + 6.f; Y += GrassCell)
		{
			const float PX = X + Rng.FRand() * GrassCell;
			const float PY = Y + Rng.FRand() * GrassCell;
			const float AY = FMath::Abs(PY);
			// Thick at the foot of the berms, thinning out over a few metres into the lane, in uneven drifts.
			const float FromEdge = LaneEdge(PX, PY) - AY;
			const float Drift = 0.5f + 0.5f * FMath::PerlinNoise2D(FVector2D(PX / 7.f - 2.4f, PY / 7.f + 5.5f));
			const float Foot = (1.f - FMath::SmoothStep(-0.5f, 4.f + 2.f * Drift, FromEdge)) * (FromEdge > -1.5f ? 1.f : 0.f);
			const bool bOnBerm = BermHeightMetres(PX, PY) > 0.3f;
			const bool bPadEdge = PX < -PadDepth + 0.8f; // behind the pad only; the view downrange stays clear
			float Chance = FMath::Max3(0.7f * Foot * (0.6f + 0.4f * Drift), bOnBerm ? 0.35f + 0.25f * Drift : 0.f, bPadEdge ? 0.4f : 0.f);
			bool bLaneTuft = false;
			if (AY < 8.f)
			{
				// The shooting lanes stay low: only sparse, short tufts the mower missed, so the lane reads as
				// a worn meadow rather than a painted strip. None on the pad or right around a target.
				float NearTarget = BIG_NUMBER;
				for (const float D : TargetDistances)
				{
					NearTarget = FMath::Min(NearTarget, FMath::Abs(PX - D));
				}
				Chance = (PX > 1.f && NearTarget > 2.f) ? 0.12f * Drift : 0.f;
				bLaneTuft = true;
			}
			if (SurfaceAt(PX, PY) == Earth || SurfaceAt(PX, PY) == Dirt || FMath::Abs(PY - WalkwayY) < WalkwayHalf + 0.3f)
			{
				Chance *= 0.15f;
			}
			if (Rng.FRand() > Chance * UndergrowthDensity)
			{
				continue;
			}
			if (GrassSet.Num() > 0)
			{
				// Berms and the foot of the berms get a small cluster of clumps, so they read as grass, not paint.
				// v7 still read as a mown slope with 2-4 clumps, so the uncut berms get a thick, taller patch.
				const bool bUncut = !bLaneTuft && (bOnBerm || Foot > 0.3f);
				const int32 Clumps = bLaneTuft ? 1 : (bUncut ? Rng.RandRange(6, 10) : 1);
				for (int32 k = 0; k < Clumps; ++k)
				{
					const float CX = PX + (k ? Rng.FRandRange(-0.5f, 0.5f) * GrassCell : 0.f);
					const float CY = PY + (k ? Rng.FRandRange(-0.5f, 0.5f) * GrassCell : 0.f);
					const float S = bLaneTuft ? Rng.FRandRange(0.45f, 0.75f) : (bUncut ? Rng.FRandRange(0.9f, 1.7f) : Rng.FRandRange(0.7f, 1.3f));
					PlacePlant(GrassSet, Rng, FTransform(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), OnGround(CX, CY, 2.f), FVector(S)), false, Cull * 1.2f);
				}
			}
			else if (GrassMeshes.Num() > 0)
			{
				UStaticMesh* M = GrassMeshes[Rng.RandRange(0, GrassMeshes.Num() - 1)];
				const float S = Rng.FRandRange(0.7f, 1.3f);
				AddInstance(M, FTransform(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), OnGround(PX, PY, 2.f), FVector(S)), -1, nullptr, false, Cull * 0.6f);
			}
			else
			{
				const float W = Rng.FRandRange(25.f, 55.f);
				const int32 Tone = Rng.RandRange(0, NumFerns - 1);
				const FTransform T(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), OnGround(PX, PY, -W * 0.1f),
					FVector(W, W * Rng.FRandRange(0.6f, 1.f), W * Rng.FRandRange(0.4f, 0.7f)) / 100.f);
				AddInstance(Sphere, T, ToneGrass + Tone, &Ferns[Tone], false, Cull * 0.6f);
			}
		}
	}
}

void AHoldoverForestRange::ScatterDebris(FRandomStream& Rng)
{
	UStaticMesh* Sphere = HoldoverShapes::Sphere();
	UStaticMesh* Cyl = HoldoverShapes::Cylinder();
	const float Cull = SmallThingsDrawDistance;

	auto Pick = [&Rng](const TArray<UStaticMesh*>& From) -> UStaticMesh*
	{
		return From.Num() > 0 ? From[Rng.RandRange(0, From.Num() - 1)] : nullptr;
	};

	constexpr float Cell = 5.f;
	for (float X = -GroundBehind; X < GroundAhead; X += Cell)
	{
		for (float Y = -GroundHalfWidth; Y < GroundHalfWidth; Y += Cell)
		{
			const float PX = X + Rng.FRand() * Cell;
			const float PY = Y + Rng.FRand() * Cell;
			const float Kind = Rng.FRand();
			if (Rng.FRand() > 0.55f * DebrisDensity || IsClearedForTrees(PX, PY, 0.5f))
			{
				continue;
			}
			const float Yaw = Rng.FRandRange(0.f, 360.f);

			if (Kind < 0.30f) // rock
			{
				if (UStaticMesh* M = Pick(KitRocks))
				{
					const float S = Rng.FRandRange(0.7f, 1.4f);
					AddInstance(M, FTransform(FRotator(Rng.FRandRange(-10.f, 10.f), Yaw, Rng.FRandRange(-10.f, 10.f)), OnGround(PX, PY, 8.f * S), FVector(S)), -1, nullptr, true, Cull * 1.3f);
				}
				else
				{
					const float W = Rng.FRandRange(30.f, 140.f);
					const int32 Tone = Rng.RandRange(0, NumRocks - 1);
					const FVector Size(W, W * Rng.FRandRange(0.6f, 0.95f), W * Rng.FRandRange(0.45f, 0.7f));
					AddInstance(Sphere, FTransform(FRotator(Rng.FRandRange(-12.f, 12.f), Yaw, 0.f), OnGround(PX, PY, Size.Z * 0.25f), Size / 100.f),
						ToneRock + Tone, &Rocks[Tone], true, Cull * 1.3f);
				}
			}
			else if (Kind < 0.50f) // fallen log
			{
				if (UStaticMesh* M = Pick(KitLogs))
				{
					const float S = Rng.FRandRange(0.8f, 1.3f);
					AddInstance(M, FTransform(FRotator(Rng.FRandRange(-4.f, 4.f), Yaw, 0.f), OnGround(PX, PY, 5.f), FVector(S)), -1, nullptr, true, Cull * 1.3f);
				}
				else
				{
					const float Len = Rng.FRandRange(160.f, 450.f);
					const float Dia = Rng.FRandRange(22.f, 45.f);
					const int32 Tone = Rng.RandRange(0, NumDeadWood - 1);
					// Cylinder axis is Z; pitch 90 lays it down along the yaw.
					AddInstance(Cyl, FTransform(FRotator(90.f + Rng.FRandRange(-4.f, 4.f), Yaw, 0.f), OnGround(PX, PY, -Dia * 0.3f), FVector(Dia, Dia, Len) / 100.f),
						ToneDeadWood + Tone, &DeadWood[Tone], true, Cull * 1.3f);
				}
			}
			else if (Kind < 0.62f) // stump
			{
				if (UStaticMesh* M = Pick(KitStumps))
				{
					const float S = Rng.FRandRange(0.8f, 1.3f);
					AddInstance(M, FTransform(FRotator(0.f, Yaw, 0.f), OnGround(PX, PY, 4.f), FVector(S)), -1, nullptr, true, Cull * 1.3f);
				}
				else
				{
					const float Dia = Rng.FRandRange(35.f, 75.f);
					const float H = Rng.FRandRange(25.f, 70.f);
					const int32 Tone = Rng.RandRange(0, NumDeadWood - 1);
					AddInstance(Cyl, FTransform(FRotator(Rng.FRandRange(-4.f, 4.f), Yaw, 0.f), OnGround(PX, PY, -H * 0.4f), FVector(Dia, Dia, H) / 100.f),
						ToneDeadWood + Tone, &DeadWood[Tone], true, Cull * 1.3f);
				}
			}
			else // fallen branches
			{
				if (UStaticMesh* M = Pick(KitBranches))
				{
					AddInstance(M, FTransform(FRotator(0.f, Yaw, 0.f), OnGround(PX, PY, 1.f), FVector(Rng.FRandRange(0.8f, 1.3f))), -1, nullptr, false, Cull);
				}
				else
				{
					const int32 Sticks = Rng.RandRange(2, 4);
					for (int32 k = 0; k < Sticks; ++k)
					{
						const float Len = Rng.FRandRange(60.f, 180.f);
						const float Dia = Rng.FRandRange(3.f, 7.f);
						const FVector Off(Rng.FRandRange(-40.f, 40.f), Rng.FRandRange(-40.f, 40.f), Dia * 0.4f);
						AddInstance(Cyl, FTransform(FRotator(90.f + Rng.FRandRange(-6.f, 6.f), Yaw + Rng.FRandRange(-35.f, 35.f), 0.f), OnGround(PX, PY) + Off, FVector(Dia, Dia, Len) / 100.f),
							ToneDeadWood, &DeadWood[0], false, Cull);
					}
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Range furniture

bool AHoldoverForestRange::PlaceProp(const TArray<FString>& Names, const FVector& LocM, float YawDeg, bool bCollide)
{
	UStaticMesh* Mesh = FindMesh(PropFolder, Names);
	if (!Mesh)
	{
		return false;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Static);
	C->SetupAttachment(Root);
	const FVector Ground = OnGround(LocM.X, LocM.Y);
	C->SetRelativeLocation(FVector(Ground.X, Ground.Y, Ground.Z + LocM.Z * 100.f));
	C->SetRelativeRotation(FRotator(0.f, YawDeg + PropYawOffset, 0.f));
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

void AHoldoverForestRange::StandInBench(const FVector& LocM, float YawDeg)
{
	UStaticMesh* Cube = HoldoverShapes::Cube();
	const FVector G = OnGround(LocM.X, LocM.Y);
	const FTransform At(FRotator(0.f, YawDeg, 0.f), G);
	auto Part = [&](const FVector& Local, const FVector& Size, const FLinearColor& Color)
	{
		if (UStaticMeshComponent* C = HoldoverShapes::AddShape(this, Root, Cube, At.TransformPosition(Local), FRotator(0.f, YawDeg, 0.f), Size, Color, true))
		{
			C->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
		}
	};
	Part(FVector(0.f, 0.f, 88.f), FVector(75.f, 140.f, 5.f), HoldoverShapes::Wood);
	Part(FVector(0.f, 0.f, 30.f), FVector(60.f, 120.f, 3.f), HoldoverShapes::Wood * 0.8f); // lower shelf
	for (float Dx : { -30.f, 30.f })
	{
		for (float Dy : { -62.f, 62.f })
		{
			Part(FVector(Dx, Dy, 43.f), FVector(8.f, 8.f, 86.f), HoldoverShapes::Wood * 0.9f);
		}
	}
}

void AHoldoverForestRange::StandInSign(const FVector& LocM, float YawDeg, float BoardW, float BoardH, const FLinearColor& Board)
{
	// Board on two posts; the face points along the yaw.
	UStaticMesh* Cube = HoldoverShapes::Cube();
	const FVector G = OnGround(LocM.X, LocM.Y);
	const FTransform At(FRotator(0.f, YawDeg, 0.f), G);
	const float Top = LocM.Z * 100.f;
	for (float Side : { -1.f, 1.f })
	{
		HoldoverShapes::AddShape(this, Root, Cube, At.TransformPosition(FVector(-3.f, Side * (BoardW * 0.5f - 6.f), Top * 0.5f)),
			FRotator(0.f, YawDeg, 0.f), FVector(8.f, 8.f, Top + 20.f), HoldoverShapes::Wood, true);
	}
	HoldoverShapes::AddShape(this, Root, Cube, At.TransformPosition(FVector(2.f, 0.f, Top - BoardH * 0.5f)),
		FRotator(0.f, YawDeg, 0.f), FVector(2.5f, BoardW, BoardH), Board, true);
}

void AHoldoverForestRange::PlaceRangeProps()
{
	// The yaw that makes an imported prop face the shooter. The Blender props are built long along X with
	// their front (sign faces, lettering) toward Blender -Y, which arrives in Unreal as +Y; 90 turns that to -X.
	constexpr float FaceShooter = 90.f;
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
	// Writes the label on a stand-in board, on both faces (the one facing away is hidden by the board).
	auto Lettered = [&](const FVector& LocM, float YawDeg, float BoardW, float BoardH, const FLinearColor& Board, const FString& Label, float Size, const FColor& Ink)
	{
		StandInSign(LocM, YawDeg, BoardW, BoardH, Board);
		const FTransform At(FRotator(0.f, YawDeg, 0.f), OnGround(LocM.X, LocM.Y));
		const float Mid = LocM.Z * 100.f - BoardH * 0.5f;
		Text(Label, At.TransformPosition(FVector(3.6f, 0.f, Mid)), YawDeg, Size, Ink);
		Text(Label, At.TransformPosition(FVector(0.4f, 0.f, Mid)), YawDeg + 180.f, Size, Ink);
	};

	// Shooting benches on the firing line: yours just to your right, two more for the empty bays.
	const FVector Benches[] = { FVector(-0.3f, 1.1f, 0.f), FVector(-0.3f, -4.2f, 0.f), FVector(-0.3f, 6.4f, 0.f) };
	for (const FVector& B : Benches)
	{
		if (!PlaceProp({ TEXT("SM_ShootingBench"), TEXT("SM_Bench"), TEXT("SM_Range_Bench") }, B, FaceShooter))
		{
			StandInBench(B, 0.f);
		}
	}

	// Stools at the empty bays (yours is left out so it isn't in your way).
	for (const FVector& S : { FVector(-1.1f, -4.2f, 0.f), FVector(-1.1f, 6.4f, 0.f) })
	{
		if (!PlaceProp({ TEXT("SM_Stool"), TEXT("SM_Range_Stool") }, S, FaceShooter))
		{
			const FVector G = OnGround(S.X, S.Y);
			HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cylinder(), G + FVector(0, 0, 60.f), FRotator::ZeroRotator, FVector(36.f, 36.f, 4.f), HoldoverShapes::Wood, true);
			for (int32 k = 0; k < 3; ++k)
			{
				const FVector Off = FRotator(0.f, k * 120.f, 0.f).RotateVector(FVector(13.f, 0.f, 0.f));
				HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cylinder(), G + Off + FVector(0, 0, 29.f), FRotator::ZeroRotator, FVector(4.f, 4.f, 58.f), HoldoverShapes::Wood * 0.8f, true);
			}
		}
	}

	// On the benches: an ammo can on yours, a sandbag rest on the left bay.
	if (!PlaceProp({ TEXT("SM_AmmoCan") }, FVector(-0.45f, 1.55f, 0.905f), 90.f))
	{
		const FVector G = OnGround(-0.45f, 1.55f) + FVector(0, 0, 90.5f);
		HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cube(), G + FVector(0, 0, 9.f), FRotator(0.f, 90.f, 0.f), FVector(28.f, 15.f, 18.f), FLinearColor(0.05f, 0.07f, 0.03f), true);
	}
	if (!PlaceProp({ TEXT("SM_SandbagRest"), TEXT("SM_Sandbags") }, FVector(-0.25f, -4.2f, 0.905f), FaceShooter))
	{
		const FVector G = OnGround(-0.25f, -4.2f) + FVector(0, 0, 90.5f);
		HoldoverShapes::AddShape(this, Root, HoldoverShapes::Sphere(), G + FVector(0, -12.f, 5.f), FRotator::ZeroRotator, FVector(30.f, 22.f, 12.f), FLinearColor(0.35f, 0.3f, 0.2f), true);
		HoldoverShapes::AddShape(this, Root, HoldoverShapes::Sphere(), G + FVector(0, 12.f, 5.f), FRotator::ZeroRotator, FVector(30.f, 22.f, 12.f), FLinearColor(0.33f, 0.29f, 0.19f), true);
	}

	// Spent-brass bin behind your bench.
	if (!PlaceProp({ TEXT("SM_BrassBin"), TEXT("SM_SpentBrassBin") }, FVector(-2.4f, 3.4f, 0.f), FaceShooter))
	{
		HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cylinder(), OnGround(-2.4f, 3.4f) + FVector(0, 0, 35.f), FRotator::ZeroRotator, FVector(45.f, 45.f, 70.f), FLinearColor(0.04f, 0.06f, 0.035f), true);
	}

	// Resting bench in the little clearing behind the pad.
	if (!PlaceProp({ TEXT("SM_BenchSeat"), TEXT("SM_Seat") }, FVector(-10.f, 4.f, 0.f), FaceShooter))
	{
		const FVector G = OnGround(-10.f, 4.f);
		HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cube(), G + FVector(0, 0, 45.f), FRotator::ZeroRotator, FVector(35.f, 180.f, 5.f), HoldoverShapes::Wood, true);
		for (float Dy : { -75.f, 75.f })
		{
			HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cube(), G + FVector(0, Dy, 22.f), FRotator::ZeroRotator, FVector(30.f, 6.f, 44.f), HoldoverShapes::Wood * 0.85f, true);
		}
	}

	// Firing line: a timber beam lying across the front of the gravel pad.
	HoldoverShapes::AddShape(this, Root, HoldoverShapes::Cube(), FVector(25.f, 0.f, 4.f), FRotator::ZeroRotator,
		FVector(15.f, LaneHalfWidth * 200.f, 12.f), FLinearColor(0.11f, 0.09f, 0.065f), true);

	// Distance signs along the left edge, facing you.
	for (const int32 D : { 5, 10, 25, 50, 75, 100, 150 })
	{
		const FVector At(static_cast<float>(D), -LaneEdge(static_cast<float>(D), -1.f) + 1.f, 1.1f);
		const FString Id = FString::FromInt(D);
		if (!PlaceProp({ TEXT("SM_DistanceMarker_") + Id + TEXT("m"), TEXT("SM_Sign_") + Id + TEXT("m") }, FVector(At.X, At.Y, 0.f), FaceShooter))
		{
			Lettered(At, 180.f, 50.f, 35.f, HoldoverShapes::White, Id + TEXT(" m"), 22.f, FColor(10, 10, 10));
		}
	}

	// Range signs: FIRING LINE at the side of the pad, CEASE FIRE where the path arrives.
	if (!PlaceProp({ TEXT("SM_Sign_FiringLine"), TEXT("SM_FiringLineSign") }, FVector(0.f, -LaneHalfWidth - 0.6f, 0.f), FaceShooter))
	{
		Lettered(FVector(0.f, -LaneHalfWidth - 0.6f, 1.6f), 180.f, 110.f, 30.f, FLinearColor(0.75f, 0.62f, 0.05f), TEXT("FIRING LINE"), 16.f, FColor(10, 10, 10));
	}
	if (!PlaceProp({ TEXT("SM_Sign_CeaseFire"), TEXT("SM_CeaseFireSign") }, FVector(-8.5f, -9.f, 0.f), FaceShooter + 25.f))
	{
		Lettered(FVector(-8.5f, -9.f, 1.6f), 205.f, 100.f, 30.f, FLinearColor(0.6f, 0.04f, 0.03f), TEXT("CEASE FIRE"), 16.f, FColor(235, 235, 235));
	}
}

// ---------------------------------------------------------------------------
// Light

void AHoldoverForestRange::SpawnLightingIfMissing()
{
	UWorld* World = GetWorld();
	if (!World || LevelHasSun())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Summer afternoon sun from behind-left, so the targets are lit and you never aim into it.
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 1000.f), FRotator(-ForestSunElevation, ForestSunAzimuth, 0.f), Params))
	{
		Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			L->SetIntensity(SunLux);
			L->bUseTemperature = true;
			L->Temperature = SunTemperature;
			L->SetAtmosphereSunLight(true);
			L->VolumetricScatteringIntensity = 1.0f; // light shafts through the canopy
			L->MarkRenderStateDirty();
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

	// Forest air: a light haze that thickens near the ground and shows the sun's rays.
	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		if (UExponentialHeightFogComponent* F = Fog->GetComponent())
		{
			F->SetFogDensity(HazeDensity);
			F->SetFogHeightFalloff(0.35f);
			F->SetVolumetricFog(true);
			F->SetVolumetricFogScatteringDistribution(0.75f); // strongly forward: rays show when looking toward the sun
			F->SetVolumetricFogDistance(8000.f);
		}
		SpawnedHelpers.Add(Fog);
	}

	// Eyes, not a camera: adaptation between sun and shade, no vignette, light bloom only.
	if (APostProcessVolume* PP = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		PP->bUnbound = true;
		FPostProcessSettings& S = PP->Settings;
		S.bOverride_AutoExposureMethod = true;
		S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		S.bOverride_AutoExposureMinBrightness = true;
		S.AutoExposureMinBrightness = EyeAdaptMinEV;
		S.bOverride_AutoExposureMaxBrightness = true;
		S.AutoExposureMaxBrightness = EyeAdaptMaxEV;
		S.bOverride_AutoExposureSpeedUp = true;
		S.AutoExposureSpeedUp = EyeAdaptSpeedUp;
		S.bOverride_AutoExposureSpeedDown = true;
		S.AutoExposureSpeedDown = EyeAdaptSpeedDown;
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = 0.f;
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = 0.3f;
		SpawnedHelpers.Add(PP);
	}
}

// ---------------------------------------------------------------------------
// Assets

// ---------------------------------------------------------------------------
// Megaplants (Fab): skeletal meshes, sorted by size

void AHoldoverForestRange::FindMegaplants()
{
	TreeSet = FPlantSet();
	YoungSet = FPlantSet();
	PlantSet = FPlantSet();
	GrassSet = FPlantSet();
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	FARFilter Filter;
	for (const FString& Folder : MegaplantFolders)
	{
		if (!Folder.IsEmpty())
		{
			Filter.PackagePaths.Add(FName(*Folder));
		}
	}
	if (Filter.PackagePaths.Num() == 0)
	{
		return;
	}
	// In an uncooked game run the registry is still scanning when the range is built; scan these folders now.
	TArray<FString> ScanPaths;
	for (const FName& P : Filter.PackagePaths)
	{
		ScanPaths.Add(P.ToString());
	}
	Registry.ScanPathsSynchronous(ScanPaths, false);
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	TArray<FAssetData> Found;
	Registry.GetAssets(Filter, Found);
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

	for (const FAssetData& Asset : Found)
	{
		const FString Path = Asset.PackageName.ToString();
		// Skip the loose branches and leaf clusters the trees are built from, and flat far-away billboards.
		if (Path.Contains(TEXT("/Instances/")) || Path.Contains(TEXT("Billboard")))
		{
			continue;
		}
		UObject* Obj = Asset.GetAsset();
		UStaticMesh* SM = Cast<UStaticMesh>(Obj);
		USkeletalMesh* SK = Cast<USkeletalMesh>(Obj);
		if (!SM && !SK)
		{
			continue;
		}
		const FBoxSphereBounds B = SM ? SM->GetBounds() : SK->GetBounds();
		const float HeightCm = static_cast<float>(B.BoxExtent.Z * 2.0);
		const FString Name = Path.ToLower();
		FPlantSet* Set = nullptr;
		const TCHAR* Kind = TEXT("");
		if (Name.Contains(TEXT("grass")) || Name.Contains(TEXT("reed")))
		{
			Set = &GrassSet; Kind = TEXT("grass");
		}
		else if (HeightCm > 1000.f)
		{
			Set = &TreeSet; Kind = TEXT("tree");
		}
		else if (HeightCm > 250.f)
		{
			Set = &YoungSet; Kind = TEXT("young tree or shrub");
		}
		else
		{
			Set = &PlantSet; Kind = TEXT("ground plant");
		}
		if (SM)
		{
			Set->Static.Add(SM);
		}
		else
		{
			Set->Skinned.Add(SK);
		}
		UE_LOG(LogTemp, Log, TEXT("[Holdover] Megaplant %s: %.1f m tall, %s mesh -> %s"), *Path, HeightCm / 100.f, SM ? TEXT("static") : TEXT("skeletal"), Kind);
	}
	UE_LOG(LogTemp, Log, TEXT("[Holdover] Megaplants: %d trees, %d young trees/shrubs, %d ground plants, %d grasses"),
		TreeSet.Num(), YoungSet.Num(), PlantSet.Num(), GrassSet.Num());
}

bool AHoldoverForestRange::PlacePlant(const FPlantSet& Set, FRandomStream& Rng, const FTransform& T, bool bCollide, float CullMetres)
{
	const int32 N = Set.Num();
	if (N == 0)
	{
		return false;
	}
	const int32 Pick = Rng.RandRange(0, N - 1);
	if (Pick < Set.Static.Num())
	{
		AddInstance(Set.Static[Pick], T, -1, nullptr, bCollide, CullMetres);
		return true;
	}
	USkeletalMesh* Mesh = Set.Skinned[Pick - Set.Static.Num()];
	AddSkinned(Mesh, T);
	if (bCanopyShade && Mesh && Mesh->GetBounds().BoxExtent.Z * 2.0 > 250.0)
	{
		AddCanopyShade(Mesh, T);
	}
	if (bCollide)
	{
		// Skinned plants carry no collision: a hidden trunk stops bullets and the player.
		const float H = static_cast<float>(Mesh->GetBounds().BoxExtent.Z * 2.0 * T.GetScale3D().Z);
		AddTrunkCollider(T.GetLocation(), FMath::Clamp(H * 0.011f, 10.f, 45.f), FMath::Min(H * 0.4f, 600.f));
	}
	return true;
}

void AHoldoverForestRange::AddSkinned(USkeletalMesh* Mesh, const FTransform& T)
{
	if (Mesh)
	{
		PendingSkinned.FindOrAdd(Mesh).Add(T);
	}
}

void AHoldoverForestRange::AddTrunkCollider(const FVector& Base, float RadiusCm, float HeightCm)
{
	if (!TrunkColliders)
	{
		TrunkColliders = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, TEXT("TrunkColliders"));
		TrunkColliders->SetStaticMesh(HoldoverShapes::Cylinder());
		TrunkColliders->SetMobility(EComponentMobility::Static);
		TrunkColliders->SetupAttachment(Root);
		TrunkColliders->SetVisibility(false);
		TrunkColliders->SetHiddenInGame(true);
		TrunkColliders->SetCastShadow(false);
		TrunkColliders->SetCollisionProfileName(TEXT("BlockAll"));
		TrunkColliders->SetCollisionResponseToChannel(ECC_HoldoverBullet, ECR_Block);
		// Lumen's software tracing can't see the skinned Megaplant trunks; these hidden ones stand in for them.
		TrunkColliders->bAffectIndirectLightingWhileHidden = bCanopyShade;
		if (UMaterialInterface* BaseMat = HoldoverShapes::BaseMaterial())
		{
			if (UMaterialInstanceDynamic* MID = TrunkColliders->CreateDynamicMaterialInstance(0, BaseMat))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.10f, 0.09f, 0.07f));
			}
		}
		TrunkColliders->RegisterComponent();
		AddInstanceComponent(TrunkColliders);
	}
	// The engine cylinder is 100 cm across and 100 cm tall, centred on its middle.
	PendingInstances.FindOrAdd(TrunkColliders).Add(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, HeightCm * 0.5f),
		FVector(RadiusCm / 50.f, RadiusCm / 50.f, HeightCm / 100.f)));
}

void AHoldoverForestRange::AddCanopyShade(USkeletalMesh* Mesh, const FTransform& T)
{
	// Lumen (software tracing) doesn't see skinned meshes, so the Megaplant crowns let all the sky light
	// through: the forest floor and trunks came out washed-out white (v5, v6). A hidden, dark-green ellipsoid
	// a bit smaller than each crown takes their place in the bounce light only; you never see it, and the real
	// leaves still cast the sun's shadows.
	if (!CanopyShade)
	{
		CanopyShade = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, TEXT("CanopyShade"));
		CanopyShade->SetStaticMesh(HoldoverShapes::Sphere());
		CanopyShade->SetMobility(EComponentMobility::Static);
		CanopyShade->SetupAttachment(Root);
		CanopyShade->SetVisibility(false);
		CanopyShade->SetHiddenInGame(true);
		CanopyShade->SetCastShadow(false);
		CanopyShade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CanopyShade->SetGenerateOverlapEvents(false);
		CanopyShade->bAffectIndirectLightingWhileHidden = true;
		if (UMaterialInterface* BaseMat = HoldoverShapes::BaseMaterial())
		{
			if (UMaterialInstanceDynamic* MID = CanopyShade->CreateDynamicMaterialInstance(0, BaseMat))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.04f, 0.07f, 0.02f));
			}
		}
		CanopyShade->RegisterComponent();
		AddInstanceComponent(CanopyShade);
	}
	const FBoxSphereBounds B = Mesh->GetBounds();
	const float H = static_cast<float>(B.BoxExtent.Z * 2.0);
	const float Bottom = static_cast<float>(B.Origin.Z - B.BoxExtent.Z);
	const FVector LocalCentre(B.Origin.X, B.Origin.Y, Bottom + H * 0.64f);
	const float R = static_cast<float>(FMath::Min(B.BoxExtent.X, B.BoxExtent.Y)) * CanopyShadeSize;
	const FVector S = T.GetScale3D();
	// The engine sphere is 100 cm across.
	const FVector Scale(R * S.X / 50.f, R * S.Y / 50.f, H * 0.26f * S.Z / 50.f);
	PendingInstances.FindOrAdd(CanopyShade).Add(FTransform(T.GetRotation(), T.TransformPosition(LocalCentre), Scale));
}

void AHoldoverForestRange::FlushSkinned()
{
	for (auto& Pair : PendingSkinned)
	{
		USkeletalMesh* Mesh = Pair.Key;
		if (!Mesh || Pair.Value.Num() == 0)
		{
			continue;
		}
		if (bInstancedMegaplants)
		{
			UInstancedSkinnedMeshComponent* C = NewObject<UInstancedSkinnedMeshComponent>(this);
			C->SetSkinnedAssetAndUpdate(Mesh);
			C->SetupAttachment(Root);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetGenerateOverlapEvents(false);
			C->RegisterComponent();
			AddInstanceComponent(C);
			for (const FTransform& T : Pair.Value)
			{
				C->AddInstance(T, 0);
			}
			SkinnedComponents.Add(C);
		}
		else
		{
			for (const FTransform& T : Pair.Value)
			{
				USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(this);
				C->SetSkeletalMesh(Mesh);
				C->SetupAttachment(Root);
				C->SetRelativeTransform(T);
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->SetGenerateOverlapEvents(false);
				C->SetComponentTickEnabled(false); // no animation yet: hold the rest pose
				C->RegisterComponent();
				AddInstanceComponent(C);
				SkinnedComponents.Add(C);
			}
		}
	}
	PendingSkinned.Reset();
	FlushInstances(); // trunk colliders
}

TArray<UStaticMesh*> AHoldoverForestRange::MeshesIn(const FString& Folder) const
{
	TArray<UStaticMesh*> Out;
	if (Folder.IsEmpty())
	{
		return Out;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ Folder }, false); // the registry may still be scanning in an uncooked game run
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*Folder));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	TArray<FAssetData> Found;
	Registry.GetAssets(Filter, Found);
	// Same order every time, so the same seed grows the same forest.
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
	for (const FAssetData& Asset : Found)
	{
		if (UStaticMesh* M = Cast<UStaticMesh>(Asset.GetAsset()))
		{
			Out.Add(M);
		}
	}
	if (Out.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Holdover] Forest: %d meshes in %s"), Out.Num(), *Folder);
	}
	return Out;
}

UStaticMesh* AHoldoverForestRange::FindMesh(const FString& Folder, const TArray<FString>& Names) const
{
	for (const FString& Name : Names)
	{
		const FString Path = FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name);
		if (UStaticMesh* M = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)))
		{
			return M;
		}
	}
	return nullptr;
}

UMaterialInterface* AHoldoverForestRange::FindMaterial(const FString& Folder, const TArray<FString>& Names) const
{
	for (const FString& Name : Names)
	{
		const FString Path = FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name);
		if (UMaterialInterface* M = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)))
		{
			return M;
		}
	}
	return nullptr;
}
