#include "Player/HoldoverPlayerController.h"

void AHoldoverPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}
