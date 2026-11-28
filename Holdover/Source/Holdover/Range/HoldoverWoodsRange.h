#pragma once

#include "CoreMinimal.h"
#include "Range/HoldoverRangeBuilder.h"
#include "HoldoverWoodsRange.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UPhysicalMaterial;
class UProceduralMeshComponent;
class USceneComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The woods range (2026-11-27, replaces the first forest range): a long shooting lane cut through a dim,
 * layered eastern hardwood forest, matched to Miles's reference photo (project file reference/forest-vibe.png):
 * tall dark trunks with high crowns, a thick layer of saplings and young trees whose leaves glow where the sun
 * comes through, ferns and low plants covering the floor, fallen wood and mossy rocks, damp haze in the depth.
 * Layout and art: docs/maps/WOODS_RANGE.md.
 *
 * Built at runtime in the empty map, like the gray range. The ground is a set of Nanite mesh tiles that the
 * editor bakes from this class's own height and surface functions (tools/unreal/build_woods_ground.py), so the
 * plants placed here always stand exactly on it; when the tiles are missing or out of date, a generated ground
 * takes their place. Every look and density value is a config setting ([/Script/Holdover.HoldoverWoodsRange]
 * in Config/DefaultGame.ini), so it can be tuned with a relaunch, no rebuild.
 *
 * The shooter stands at the origin facing +X. The same seed always grows the same forest.
 */
UCLASS(Config = Game)
class HOLDOVER_API AHoldoverWoodsRange : public AHoldoverRangeBuilder
{
	GENERATED_BODY()

public:
	AHoldoverWoodsRange();

	/** Bump when the ground shape or surface paint changes: the baked tiles carry it in their names. */
	static constexpr int32 GroundVersion = 1;

	/** Baked ground tiles: TilesX x TilesY squares of TileSize metres, starting at (GroundX0, GroundY0). */
	static constexpr float GroundX0 = -96.f;
	static constexpr float GroundY0 = -144.f;
	static constexpr float TileSize = 48.f;
	static constexpr int32 TilesX = 8;
	static constexpr int32 TilesY = 6;
	static constexpr float GroundCell = 0.5f;
	static FString GroundFolder() { return TEXT("/Game/Range/Woods/Ground"); }
	static FString GroundTileName(int32 I, int32 J) { return FString::Printf(TEXT("SM_WoodsGround_v%d_%d_%d"), GroundVersion, I, J); }
	static FString GroundPadName() { return FString::Printf(TEXT("SM_WoodsGround_v%d_Pad"), GroundVersion); }
	static FString GroundMaterialPath() { return TEXT("/Game/Range/Woods/Materials/MI_WoodsGround.MI_WoodsGround"); }
	static FString PadMaterialPath() { return TEXT("/Game/Range/Woods/Materials/MI_WoodsPad.MI_WoodsPad"); }

	/** Ground height in metres at a point in metres (actor space). */
	float GroundHeightM(float X, float Y) const;

	/** Ground height in cm at a point in cm (actor space). */
	float GroundHeight(float X, float Y) const { return GroundHeightM(X / 100.f, Y / 100.f) * 100.f; }

	/** Ground paint at a point (metres): R grass, G bare soil (paths, backstop), B gravel, A moss; the rest is leaf litter. */
	FLinearColor SurfaceWeights(float X, float Y) const;

	/** True where the ground belongs to the gravel firing pad (its own mesh, with gravel's impact sounds). */
	bool IsPad(float X, float Y) const;

	// ---------- Layout (metres) ----------

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	int32 Seed = 2711;

	/** Half width of the mown lane, before it wanders. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	float LaneHalfWidth = 8.5f;

	/** How far each lane edge drifts in and out along the range, so no stretch of it runs straight. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	float LaneWander = 3.5f;

	/** Where the backstop rises (190 m: the range is 3-4x the first design, Miles 2026-11-20). */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	float BackstopToe = 190.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	float BackstopHeight = 5.f;

	/** Metres the ground climbs from the firing line to the backstop (Miles: the far end higher than the player). */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	float FarRise = 8.f;

	/** Metres a wooded hill climbs behind the backstop. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	float BeyondRise = 9.f;

	/** Metres the forest floor climbs away from the lane on both sides, over SideRiseRun metres. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	float SideRise = 7.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	float SideRiseRun = 55.f;

	/** Hills and hollows in the forest (m up and down). */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	float HillHeight = 3.f;

	/** Low mounds in the lane, kept off the lines of fire (m). */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Layout")
	float LaneMound = 0.9f;

	// ---------- Plants (multipliers, 1 = the designed look) ----------

	/** Thins or thickens everything that grows. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Plants")
	float ForestDensity = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Plants")
	float TreeDensity = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Plants")
	float YoungTreeDensity = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Plants")
	float SaplingDensity = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Plants")
	float FernDensity = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Plants")
	float GroundPlantDensity = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Plants")
	float GrassDensity = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Plants")
	float DebrisDensity = 1.f;

	/** Small plants and debris are only grown this far from the lane's centre line (m); beyond, only trees and young trees. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Plants")
	float DetailReach = 75.f;

	/** Small plants disappear beyond this distance from the eye (m). */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Plants")
	float SmallCull = 90.f;

	/** Hidden stand-ins for the Megaplant crowns and trunks in the bounce light (Lumen's software tracing can't see
	 *  skinned meshes): without them the forest floor washes out bright. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	bool bCanopyShade = true;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float CanopyShadeSize = 0.6f;

	// ---------- Light ----------

	/** Sun height, degrees. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float SunPitch = 38.f;

	/** Where the sun stands, degrees around the shooter: 0 = straight downrange, 90 = to the right, -90 = left. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float SunBearing = -75.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float SunLux = 60000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float SunKelvin = 5600.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float SkyLightIntensity = 1.f;

	/** Aerosol haze in the sky (multiplies the engine's default): higher = paler, whiter sky. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float SkyHaze = 6.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float FogDensity = 0.03f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float FogFalloff = 0.12f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	FLinearColor FogColor = FLinearColor(0.30f, 0.36f, 0.36f);

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float VolumetricFogScatter = 1.f;

	/** Light shafts: how strongly the sun lights the haze. */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float SunShafts = 1.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float ExposureBias = -0.3f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float EyeMinEV = 8.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float EyeMaxEV = 14.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float WhiteBalance = 6100.f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float Saturation = 0.92f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float Contrast = 1.08f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float HighlightContrast = 0.6f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float ShadowContrast = 0.95f;

	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float Bloom = 0.35f;

	/** Bark tint multiplier for the big trees (the reference's trunks are dark). */
	UPROPERTY(Config, EditAnywhere, Category = "Woods|Light")
	float BarkDarken = 0.55f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BuildGeometry() override;
	virtual void SpawnLightingIfMissing() override;
	virtual float TargetGroundZ(float X, float Y) const override { return GroundHeight(X, Y); }

private:
	// ---- Layout (metres)
	float LaneEdge(float X, float Y) const;       // distance from centre to the lane edge on Y's side
	float Backstop(float Y) const;                 // X of the backstop toe
	float BackstopHeightAt(float X, float Y) const;
	float Valley(float X, float Y) const;
	float FireDistance(float X, float Y) const;    // sideways metres to the nearest line of fire
	float WalkwayY(float X) const;
	bool OnWalkway(float X, float Y, float Extra = 0.f) const;
	float PathDistance(float X, float Y) const;    // to the access path behind the range
	float TargetY(int32 Index) const;
	float SignY(float D) const;
	float LaneOpen(float X, float Y) const;        // 1 inside the open lane, fading to 0 into the forest
	bool InClearing(float X, float Y, float Margin) const; // lane, pad area, backstop face, paths
	bool KeepClear(float X, float Y, float Margin) const;   // nothing taller than low plants: lines of fire, signs, walkway
	bool NearShotSpot(float X, float Y, float Radius, float Cone) const;
	FVector At(float X, float Y, float Sink = 0.f) const; // cm, on the ground
	FVector AtLow(float X, float Y, float R, float Sink = 0.f) const;
	float Noise(float X, float Y, float Scale, float Off) const; // 0..1

	// ---- Ground
	void BuildGround();
	void BuildGeneratedGround();

	// ---- Plants and debris
	void FindAssets();
	void GrowCanopy(FRandomStream& R);
	void GrowYoungTrees(FRandomStream& R);
	void GrowSaplings(FRandomStream& R);
	void GrowFerns(FRandomStream& R);
	void GrowGroundPlants(FRandomStream& R);
	void GrowGrass(FRandomStream& R);
	void ScatterDeadWood(FRandomStream& R);
	void ScatterRocks(FRandomStream& R);
	void ScatterTwigs(FRandomStream& R);
	void DressLaneEdges(FRandomStream& R);

	// ---- Range furniture
	void PlaceRangeProps();
	void BuildShelter();
	bool PlaceProp(const TArray<FString>& Names, const FVector& LocM, float YawDeg, bool bCollide = true);
	UStaticMeshComponent* Beam(const FVector& A, const FVector& B, float W, float H, UMaterialInterface* Mat, float Roll = 0.f);

	// ---- Instancing
	struct FMeshList
	{
		TArray<UStaticMesh*> Static;
		TArray<USkeletalMesh*> Skinned;
		int32 Num() const { return Static.Num() + Skinned.Num(); }
	};
	void AddStatic(UStaticMesh* Mesh, const FTransform& T, bool bCollide, float CullM, bool bShadow = true);
	void AddSkinned(USkeletalMesh* Mesh, const FTransform& T, bool bTrunk);
	bool PlaceFrom(const FMeshList& L, FRandomStream& R, const FTransform& T, bool bCollide, float CullM, bool bTrunk = false, bool bShadow = true);
	void AddTrunkCollider(const FVector& Base, float RadiusCm, float HeightCm);
	void AddCanopyShade(USkeletalMesh* Mesh, const FTransform& T);
	void Flush();
	UHierarchicalInstancedStaticMeshComponent* Batch(UStaticMesh* Mesh, bool bCollide, float CullM, bool bShadow);
	UHierarchicalInstancedStaticMeshComponent* HiddenBatch(const TCHAR* Name, const FLinearColor& Color, bool bCollide);

	TMap<FName, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Batches;
	TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<FTransform>> PendingStatic;
	TMap<USkeletalMesh*, TArray<FTransform>> PendingSkinned;
	int32 SkinnedCount = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> SkinnedComponents;

	UPROPERTY(Transient)
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TrunkColliders;

	UPROPERTY(Transient)
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CanopyShade;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> GroundParts;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> DirtPhys;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> GravelPhys;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> PhysWood;

	// Asset lists
	FMeshList Canopy, EdgeTrees, YoungTrees, Elders, Saplings, SmallSaplings, Seedlings, FernPlants, FernFronds,
		GroundPlants, Moss, Grass, Logs, Stumps, Roots, Branches, Twigs, Rocks, Boulders;

	// Helpers
	UStaticMesh* LoadMesh(const FString& Path) const;
	USkeletalMesh* LoadSkinned(const FString& Path) const;
	TArray<UStaticMesh*> MeshesUnder(const FString& Folder) const;
	UMaterialInterface* LoadMaterial(const FString& Path) const;

	// Bark material overrides (darker trunks), made at runtime per tree mesh.
	TMap<USkeletalMesh*, TArray<TObjectPtr<UMaterialInterface>>> BarkOverrides;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;

	// Check pictures (holdover.RangeShots)
	void UpdateRangeShots(float DeltaSeconds);
	float ShotClock = 0.f;
	uint64 ShotFrameStart = 0;
	int32 ShotStep = 0;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ShotCamera;
};
