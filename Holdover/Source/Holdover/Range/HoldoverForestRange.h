#pragma once

#include "CoreMinimal.h"
#include "Range/HoldoverRangeBuilder.h"
#include "HoldoverForestRange.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The first real range: a lane cut through a dense summer hardwood forest.
 * Layout and numbers: docs/maps/FOREST_RANGE.md (keep in sync with tools/range/forest_layout.py).
 *
 * Built at runtime, like the gray range, so it works in an empty map:
 * - Ground: the Blender terrain mesh (/Game/Range/Forest/Ground/SM_ForestTerrain) if imported,
 *   otherwise a shaped ground mesh made here (berms, banks, paths, gently rolling forest floor).
 * - Trees, saplings, ferns and bushes: every static mesh found in the Trees, Saplings and Plants
 *   folders (Megaplants from Fab), each one placed with its own size, turn and lean. With no
 *   meshes there, varied stand-in trees are built from engine shapes, no two alike.
 * - Rocks, logs, stumps and branches: the Blender forest kit, or stand-ins.
 * - Range furniture: the Blender range props (benches, stools, signs...), or stand-ins.
 * - Light: summer afternoon sun through the canopy, sky, forest haze with light shafts, eye adaptation.
 *
 * The same seed always grows the same forest. The shooter stands at the origin facing +X.
 */
UCLASS()
class HOLDOVER_API AHoldoverForestRange : public AHoldoverRangeBuilder
{
	GENERATED_BODY()

public:
	AHoldoverForestRange();

	/** Ground height (cm, actor space) at a point (cm, actor space). */
	float GroundHeight(float X, float Y) const;

	// ---------- Layout (metres) ----------

	/** Same seed, same forest. */
	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	int32 Seed = 1907;

	/** Ground built around the range: X from -GroundBehind to +GroundAhead, Y +-GroundHalfWidth. */
	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float GroundBehind = 90.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float GroundAhead = 260.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float GroundHalfWidth = 90.f;

	/** Half width of the mown lane floor between the side berms. */
	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float LaneHalfWidth = 13.f;

	/** Where the backstop berm starts to rise (its toe). 190 m: the range is about 3.5x the first design. */
	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float BackBermToe = 190.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float BackBermHeight = 5.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float SideBermHeight = 1.8f;

	/** Gravel firing pad, from this far behind the firing line to 1 m in front of it. */
	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float PadDepth = 7.f;

	/** How far the lane edge drifts in and out along the range (m), so it looks cut by hand, not with a ruler. */
	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float LaneEdgeWander = 2.8f;

	/** Share the side berm height and width drift along the range. */
	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float BermWander = 0.45f;

	/** How far the tree line drifts in and out (m). */
	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float TreeLineWander = 6.f;

	/** How far the backstop toe drifts across the lane (m). */
	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float BackToeWander = 1.5f;

	/** Ground roughness of the forest floor, metres up and down. */
	UPROPERTY(EditAnywhere, Category = "Forest|Layout")
	float ForestFloorRoll = 0.7f;

	// ---------- Forest (densities are multipliers, 1 = the designed look) ----------

	UPROPERTY(EditAnywhere, Category = "Forest|Plants", meta = (ClampMin = "0", ClampMax = "3"))
	float TreeDensity = 1.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Plants", meta = (ClampMin = "0", ClampMax = "3"))
	float SaplingDensity = 1.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Plants", meta = (ClampMin = "0", ClampMax = "3"))
	float UndergrowthDensity = 1.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Plants", meta = (ClampMin = "0", ClampMax = "3"))
	float DebrisDensity = 1.f;

	/** Small plants, rocks and branches disappear beyond this distance (m). Trees never do. */
	UPROPERTY(EditAnywhere, Category = "Forest|Plants")
	float SmallThingsDrawDistance = 110.f;

	/** Content folders searched for meshes (put Megaplants and Megascans here). */
	UPROPERTY(EditAnywhere, Category = "Forest|Assets")
	FString TreeFolder = TEXT("/Game/Range/Forest/Trees");

	UPROPERTY(EditAnywhere, Category = "Forest|Assets")
	FString SaplingFolder = TEXT("/Game/Range/Forest/Saplings");

	UPROPERTY(EditAnywhere, Category = "Forest|Assets")
	FString PlantFolder = TEXT("/Game/Range/Forest/Plants");

	/** Grass clumps for the lane edges and berms (optional). */
	UPROPERTY(EditAnywhere, Category = "Forest|Assets")
	FString GrassFolder = TEXT("/Game/Range/Forest/Grass");

	UPROPERTY(EditAnywhere, Category = "Forest|Assets")
	FString KitFolder = TEXT("/Game/Range/Forest/Kit");

	UPROPERTY(EditAnywhere, Category = "Forest|Assets")
	FString PropFolder = TEXT("/Game/Range/Props");

	UPROPERTY(EditAnywhere, Category = "Forest|Assets")
	FString GroundFolder = TEXT("/Game/Range/Forest/Ground");

	/**
	 * Where Fab puts Megaplants and Megascans plants. Every plant found under them (except the loose branch
	 * parts in Instances folders) is sorted by its height: over 10 m a tree, 2.5 to 10 m a young tree or
	 * shrub, lower a ground plant; anything with grass or reed in its path is grass. They take over from
	 * the stand-ins.
	 */
	UPROPERTY(EditAnywhere, Category = "Forest|Assets")
	TArray<FString> MegaplantFolders = { TEXT("/Game/Megaplant_Library"), TEXT("/Game/Fab/Megascans") };

	/** Draw Megaplants as instanced skinned meshes (fast). Off: one skeletal mesh component per plant (slow, for testing). */
	UPROPERTY(EditAnywhere, Category = "Forest|Assets")
	bool bInstancedMegaplants = true;

	/** Extra turn applied to every imported range prop, degrees, in case the Blender export faces another way. */
	UPROPERTY(EditAnywhere, Category = "Forest|Assets")
	float PropYawOffset = 0.f;

	// ---------- Light ----------

	/** Sun height above the horizon, degrees (summer mid-afternoon). */
	UPROPERTY(EditAnywhere, Category = "Forest|Light")
	float ForestSunElevation = 52.f;

	/** Sun direction, degrees (0 = from behind the shooter). Slightly from behind-left so targets are lit. */
	UPROPERTY(EditAnywhere, Category = "Forest|Light")
	float ForestSunAzimuth = 30.f;

	/** Sunlight, lux (clear summer afternoon, a little hazy). */
	UPROPERTY(EditAnywhere, Category = "Forest|Light")
	float SunLux = 75000.f;

	/** Sun colour temperature, kelvin. */
	UPROPERTY(EditAnywhere, Category = "Forest|Light")
	float SunTemperature = 5500.f;

	/** Ground haze. Higher = thicker air and stronger light shafts. */
	UPROPERTY(EditAnywhere, Category = "Forest|Light")
	float HazeDensity = 0.008f;

	/** Eye adaptation range (EV100). Your eyes adjust between the sunny lane and the shade inside this range. */
	UPROPERTY(EditAnywhere, Category = "Forest|Light")
	float EyeAdaptMinEV = 9.5f;

	UPROPERTY(EditAnywhere, Category = "Forest|Light")
	float EyeAdaptMaxEV = 15.f;

	/** How fast eyes adjust to brighter light (stops per second) and to darker light (slower, like real eyes). */
	UPROPERTY(EditAnywhere, Category = "Forest|Light")
	float EyeAdaptSpeedUp = 2.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Light")
	float EyeAdaptSpeedDown = 0.8f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BuildGeometry() override;
	virtual void SpawnLightingIfMissing() override;

private:
	// Ground
	void BuildGround();
	void BuildProceduralGround();
	float GroundHeightMetres(float Xm, float Ym) const;
	float BermHeightMetres(float Xm, float Ym) const;
	float LaneEdge(float Xm, float Ym) const;
	float BackToe(float Ym) const;
	float TreeLine(float Xm, float Ym) const;
	bool IsBareGround(float Xm, float Ym) const;
	int32 SurfaceAt(float Xm, float Ym) const;
	float DistanceToAccessPath(float Xm, float Ym) const;
	bool IsClearedForTrees(float Xm, float Ym, float Margin) const;
	FVector OnGround(float Xm, float Ym, float Sink = 0.f) const;

	// Plants
	void GrowTrees(FRandomStream& Rng);
	void GrowSaplings(FRandomStream& Rng);
	void GrowUndergrowth(FRandomStream& Rng);
	void ScatterDebris(FRandomStream& Rng);
	void StandInTree(FRandomStream& Rng, const FVector& Base, float HeightCm, float TrunkRadiusCm, bool bSapling);
	void AddLimb(const FVector& A, const FVector& B, float Radius, int32 BarkTone, bool bCollide);

	// Range furniture
	void PlaceRangeProps();
	bool PlaceProp(const TArray<FString>& Names, const FVector& LocM, float YawDeg, bool bCollide = true);
	void StandInBench(const FVector& LocM, float YawDeg);
	void StandInSign(const FVector& LocM, float YawDeg, float BoardW, float BoardH, const FLinearColor& Board);

	// Instancing
	UHierarchicalInstancedStaticMeshComponent* Batch(UStaticMesh* Mesh, int32 Tone, const FLinearColor* Tint, bool bCollide, float CullMetres);
	void AddInstance(UStaticMesh* Mesh, const FTransform& T, int32 Tone = -1, const FLinearColor* Tint = nullptr, bool bCollide = false, float CullMetres = 0.f);
	void FlushInstances();

	// Plants that may be static meshes (our own folders) or skeletal meshes (Megaplants)
	struct FPlantSet
	{
		TArray<UStaticMesh*> Static;
		TArray<USkeletalMesh*> Skinned;
		int32 Num() const { return Static.Num() + Skinned.Num(); }
	};
	void FindMegaplants();
	/** Places a random member of Set at T. Returns false when the set is empty. */
	bool PlacePlant(const FPlantSet& Set, FRandomStream& Rng, const FTransform& T, bool bCollide, float CullMetres);
	void AddSkinned(USkeletalMesh* Mesh, const FTransform& T);
	void AddTrunkCollider(const FVector& Base, float RadiusCm, float HeightCm);
	void FlushSkinned();
	FPlantSet TreeSet, YoungSet, PlantSet, GrassSet;
	TMap<USkeletalMesh*, TArray<FTransform>> PendingSkinned;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> SkinnedComponents;

	UPROPERTY(Transient)
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TrunkColliders;

	// Assets
	TArray<UStaticMesh*> MeshesIn(const FString& Folder) const;
	UStaticMesh* FindMesh(const FString& Folder, const TArray<FString>& Names) const;
	UMaterialInterface* FindMaterial(const FString& Folder, const TArray<FString>& Names) const;

	// Check pictures (holdover.RangeShots)
	void UpdateRangeShots(float DeltaSeconds);
	float ShotClock = 0.f;
	uint64 ShotFrameStart = 0;
	int32 ShotStep = 0;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ShotCamera;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> GroundMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ImportedGround;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Batches;

	TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<FTransform>> PendingInstances;

	TArray<UStaticMesh*> KitRocks;
	TArray<UStaticMesh*> KitLogs;
	TArray<UStaticMesh*> KitStumps;
	TArray<UStaticMesh*> KitBranches;
};
