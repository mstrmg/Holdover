#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HoldoverRangeBuilder.generated.h"

class AHoldoverPaperTarget;
class USceneComponent;

/**
 * Builds the gray-box test range at runtime from engine shapes: ground, lane, side walls,
 * backstop berm, firing line, bench, distance posts and paper targets. Adds a sun, sky,
 * sky light and haze if the level has none. The shooter stands at the origin facing +X.
 */
UCLASS()
class HOLDOVER_API AHoldoverRangeBuilder : public AActor
{
	GENERATED_BODY()

public:
	AHoldoverRangeBuilder();

	/** Where the player starts: behind the firing line, facing downrange. */
	FTransform GetFiringPosition() const;

	/** Fresh paper on every target. */
	void ResetTargets();

	/** Target distances in metres. */
	UPROPERTY(EditAnywhere, Category = "Range")
	TArray<float> TargetDistances = { 5.f, 7.f, 10.f, 15.f, 25.f, 50.f };

	/** Sideways spacing between targets, cm. */
	UPROPERTY(EditAnywhere, Category = "Range")
	float TargetSpacing = 160.f;

	/** Range length to the berm, metres. */
	UPROPERTY(EditAnywhere, Category = "Range")
	float RangeLength = 60.f;

	/** Range width, metres. */
	UPROPERTY(EditAnywhere, Category = "Range")
	float RangeWidth = 24.f;

	/** Spawn sun, sky and fog when the level has no directional light. */
	UPROPERTY(EditAnywhere, Category = "Range")
	bool bSpawnLightingIfMissing = true;

	/** Sun angle above the horizon, degrees. */
	UPROPERTY(EditAnywhere, Category = "Range")
	float SunElevation = 50.f;

	/** Sun direction around the range, degrees (0 = shining downrange from behind the shooter). */
	UPROPERTY(EditAnywhere, Category = "Range")
	float SunAzimuth = 35.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Builds the ground, berm and range furniture. Subclasses build other ranges. */
	virtual void BuildGeometry();

	/** Adds sun, sky and fog unless the level already has a sun. */
	virtual void SpawnLightingIfMissing();

	/** True when the level already has its own directional light. */
	bool LevelHasSun() const;

	UPROPERTY(VisibleAnywhere, Category = "Range")
	TObjectPtr<USceneComponent> Root;

	/** Actors spawned for this range (lights, fog...), destroyed with it. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedHelpers;

private:
	void SpawnTargets();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AHoldoverPaperTarget>> Targets;
};
