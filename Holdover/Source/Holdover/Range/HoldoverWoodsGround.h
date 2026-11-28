#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HoldoverWoodsGround.generated.h"

/**
 * Editor tool: bakes the woods range's ground into Nanite static mesh tiles (/Game/Range/Woods/Ground), straight from
 * AHoldoverWoodsRange's own height and surface functions, so the plants the game places always stand on it.
 * Each tile carries the surface paint in its vertex colours (R grass, G soil, B gravel, A moss) for MI_WoodsGround.
 * Run headless: tools/unreal/build_woods_ground.py. Re-run whenever AHoldoverWoodsRange::GroundVersion changes.
 */
UCLASS()
class HOLDOVER_API UHoldoverWoodsGround : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Writes every ground tile and the gravel pad. Returns how many meshes were saved (0 outside the editor). */
	UFUNCTION(BlueprintCallable, Category = "Holdover|Woods")
	static int32 BakeWoodsGround();
};
