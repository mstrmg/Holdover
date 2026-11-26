#include "Player/HoldoverCameraComponent.h"

void UHoldoverCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	Super::GetCameraView(DeltaTime, DesiredView);
	DesiredView.PerspectiveNearClipPlane = FMath::Max(EyeNearClip, 0.1f);
}
