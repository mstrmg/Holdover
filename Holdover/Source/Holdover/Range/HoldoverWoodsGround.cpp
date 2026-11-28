#include "Range/HoldoverWoodsGround.h"

#include "Range/HoldoverWoodsRange.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace WoodsGround
{
	/** Builds one mesh from the grid cells for which Keep(cell centre) is true, and saves it. */
	bool BakeMesh(const AHoldoverWoodsRange& Range, const FString& Name, float X0, float Y0, int32 NX, int32 NY,
		TFunctionRef<bool(float, float)> Keep, UMaterialInterface* Mat)
	{
		const float C = AHoldoverWoodsRange::GroundCell;
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> Positions = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attr.GetVertexInstanceColors();
		TPolygonGroupAttributesRef<FName> SlotNames = Attr.GetPolygonGroupMaterialSlotNames();
		UVs.SetNumChannels(1);
		const FPolygonGroupID Group = MD.CreatePolygonGroup();
		SlotNames[Group] = TEXT("Ground");

		TArray<FVertexInstanceID> Grid;
		Grid.Init(FVertexInstanceID(INDEX_NONE), (NX + 1) * (NY + 1));
		auto Corner = [&](int32 i, int32 j) -> FVertexInstanceID
		{
			FVertexInstanceID& Slot = Grid[i * (NY + 1) + j];
			if (Slot != INDEX_NONE)
			{
				return Slot;
			}
			const float X = X0 + i * C, Y = Y0 + j * C;
			const float H = Range.GroundHeightM(X, Y);
			const float E = 0.25f;
			const float Hx = Range.GroundHeightM(X + E, Y) - Range.GroundHeightM(X - E, Y);
			const float Hy = Range.GroundHeightM(X, Y + E) - Range.GroundHeightM(X, Y - E);
			const FVertexID V = MD.CreateVertex();
			Positions[V] = FVector3f(X * 100.f, Y * 100.f, H * 100.f);
			Slot = MD.CreateVertexInstance(V);
			Normals[Slot] = FVector3f(-Hx, -Hy, 2.f * E).GetSafeNormal();
			UVs.Set(Slot, 0, FVector2f(X / 2.f, Y / 2.f)); // one UV unit = 2 m; the material scales per layer
			const FLinearColor W = Range.SurfaceWeights(X, Y);
			Colors[Slot] = FVector4f(W.R, W.G, W.B, W.A);
			return Slot;
		};
		int32 Tris = 0;
		for (int32 i = 0; i < NX; ++i)
		{
			for (int32 j = 0; j < NY; ++j)
			{
				if (!Keep(X0 + (i + 0.5f) * C, Y0 + (j + 0.5f) * C))
				{
					continue;
				}
				const FVertexInstanceID A = Corner(i, j), B = Corner(i + 1, j), Cc = Corner(i + 1, j + 1), D = Corner(i, j + 1);
				MD.CreateTriangle(Group, { A, Cc, B }); // clockwise seen from above = facing up
				MD.CreateTriangle(Group, { A, D, Cc });
				Tris += 2;
			}
		}
		if (Tris == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[woods_ground] %s has no cells"), *Name);
			return false;
		}

		const FString PackageName = AHoldoverWoodsRange::GroundFolder() / Name;
		UPackage* Pkg = CreatePackage(*PackageName);
		Pkg->FullyLoad();
		UStaticMesh* SM = FindObject<UStaticMesh>(Pkg, *Name);
		if (!SM)
		{
			SM = NewObject<UStaticMesh>(Pkg, FName(*Name), RF_Public | RF_Standalone);
		}
		SM->PreEditChange(nullptr);
		SM->SetNumSourceModels(1);
		FStaticMeshSourceModel& Src = SM->GetSourceModel(0);
		Src.BuildSettings.bRecomputeNormals = false;
		Src.BuildSettings.bRecomputeTangents = true;
		Src.BuildSettings.bGenerateLightmapUVs = false;
		Src.BuildSettings.bUseMikkTSpace = true;
		Src.BuildSettings.DistanceFieldResolutionScale = 2.f;
		SM->GetStaticMaterials().Reset();
		SM->GetStaticMaterials().Add(FStaticMaterial(Mat, TEXT("Ground"), TEXT("Ground")));
		SM->CreateMeshDescription(0, MoveTemp(MD));
		SM->CommitMeshDescription(0);
		FMeshNaniteSettings Nanite = SM->GetNaniteSettings();
		Nanite.bEnabled = true;
		SM->SetNaniteSettings(Nanite);
		SM->CreateBodySetup();
		SM->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
		SM->Build(false);
		SM->PostEditChange();
		SM->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(SM);

		const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const bool bOk = UPackage::SavePackage(Pkg, SM, *File, Args);
		UE_LOG(LogTemp, Log, TEXT("[woods_ground] %s: %d triangles, saved %s"), *Name, Tris, bOk ? TEXT("ok") : TEXT("FAILED"));
		return bOk;
	}
}
#endif

int32 UHoldoverWoodsGround::BakeWoodsGround()
{
	int32 Saved = 0;
#if WITH_EDITOR
	const AHoldoverWoodsRange* Range = GetDefault<AHoldoverWoodsRange>();
	UMaterialInterface* Mat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *AHoldoverWoodsRange::GroundMaterialPath()));
	const int32 PerTile = FMath::RoundToInt(AHoldoverWoodsRange::TileSize / AHoldoverWoodsRange::GroundCell);
	for (int32 I = 0; I < AHoldoverWoodsRange::TilesX; ++I)
	{
		for (int32 J = 0; J < AHoldoverWoodsRange::TilesY; ++J)
		{
			const float X0 = AHoldoverWoodsRange::GroundX0 + I * AHoldoverWoodsRange::TileSize;
			const float Y0 = AHoldoverWoodsRange::GroundY0 + J * AHoldoverWoodsRange::TileSize;
			Saved += WoodsGround::BakeMesh(*Range, AHoldoverWoodsRange::GroundTileName(I, J), X0, Y0, PerTile, PerTile,
				[Range](float X, float Y) { return !Range->IsPad(X, Y); }, Mat) ? 1 : 0;
		}
	}
	// The gravel pad: its own mesh so it can sound like gravel when brass lands on it.
	const float C = AHoldoverWoodsRange::GroundCell;
	Saved += WoodsGround::BakeMesh(*Range, AHoldoverWoodsRange::GroundPadName(), -12.f, -12.f,
		FMath::RoundToInt(16.f / C), FMath::RoundToInt(24.f / C), [Range](float X, float Y) { return Range->IsPad(X, Y); }, Mat) ? 1 : 0;
	UE_LOG(LogTemp, Log, TEXT("[woods_ground] done: %d meshes saved (ground v%d)"), Saved, AHoldoverWoodsRange::GroundVersion);
#endif
	return Saved;
}
