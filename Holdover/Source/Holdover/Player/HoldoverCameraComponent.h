#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "HoldoverCameraComponent.generated.h"

/**
 * The player's eyes. Same as the engine camera, with its own near clipping distance: the engine default (set in
 * the project config) cut open the arms and gun whenever they came close to the eyes, showing their insides.
 */
UCLASS(ClassGroup = Camera, meta = (BlueprintSpawnableComponent))
class HOLDOVER_API UHoldoverCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	/** Nothing nearer the eye than this (cm) is drawn. Small, so hands and gun right in front of the face stay whole. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSettings", meta = (ClampMin = "0.1", ClampMax = "10"))
	float EyeNearClip = 1.f;

	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;
};
