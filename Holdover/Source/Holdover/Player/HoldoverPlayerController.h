#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HoldoverPlayerController.generated.h"

UCLASS()
class HOLDOVER_API AHoldoverPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
