#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HoldoverGameMode.generated.h"

class AHoldoverRangeBuilder;

/**
 * Default game mode. In a level with no range of its own it builds the gray-box range,
 * so pressing Play in an empty level is enough to shoot.
 */
UCLASS(Config = Game)
class HOLDOVER_API AHoldoverGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHoldoverGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void StartPlay() override;

	/** Clears paper, brass and dropped mags. */
	void ResetRange();

	AHoldoverRangeBuilder* GetRange() const { return Range; }

	/** Build the gray-box range when the level has none. */
	UPROPERTY(Config, EditAnywhere, Category = "Range")
	bool bAutoBuildRange = true;

private:
	AHoldoverRangeBuilder* EnsureRange();

	UPROPERTY(Transient)
	TObjectPtr<AHoldoverRangeBuilder> Range;
};
