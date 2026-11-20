#include "Core/HoldoverGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Player/HoldoverCharacter.h"
#include "Player/HoldoverPlayerController.h"
#include "Range/HoldoverForestRange.h"
#include "Range/HoldoverRangeBuilder.h"
#include "Weapons/HoldoverDebrisSubsystem.h"
#include "Weapons/HoldoverLoosePiece.h"

AHoldoverGameMode::AHoldoverGameMode()
{
	DefaultPawnClass = AHoldoverCharacter::StaticClass();
	PlayerControllerClass = AHoldoverPlayerController::StaticClass();
	HUDClass = nullptr;
}

void AHoldoverGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	const FString RangeOption = UGameplayStatics::ParseOption(Options, TEXT("Range"));
	if (RangeOption.Equals(TEXT("Gray"), ESearchCase::IgnoreCase) || RangeOption.Equals(TEXT("Grey"), ESearchCase::IgnoreCase))
	{
		bForestRange = false;
	}
	else if (RangeOption.Equals(TEXT("Forest"), ESearchCase::IgnoreCase))
	{
		bForestRange = true;
	}
}

AHoldoverRangeBuilder* AHoldoverGameMode::EnsureRange()
{
	if (Range)
	{
		return Range;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AHoldoverRangeBuilder> It(World); It; ++It)
	{
		Range = *It;
		return Range;
	}
	if (!bAutoBuildRange)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* RangeClass = bForestRange ? AHoldoverForestRange::StaticClass() : AHoldoverRangeBuilder::StaticClass();
	Range = World->SpawnActor<AHoldoverRangeBuilder>(RangeClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
	return Range;
}

AActor* AHoldoverGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// A level with its own Player Start uses it.
	UWorld* World = GetWorld();
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		return *It;
	}

	// Otherwise stand at the built range's firing line.
	if (AHoldoverRangeBuilder* R = EnsureRange())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FTransform At = R->GetFiringPosition();
		return World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), At.GetLocation(), At.Rotator(), Params);
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AHoldoverGameMode::StartPlay()
{
	EnsureRange();
	Super::StartPlay();
}

void AHoldoverGameMode::ResetRange()
{
	if (Range)
	{
		Range->ResetTargets();
	}
	if (UWorld* World = GetWorld())
	{
		if (UHoldoverDebrisSubsystem* Debris = World->GetSubsystem<UHoldoverDebrisSubsystem>())
		{
			Debris->ClearAll();
		}
		for (TActorIterator<AHoldoverLoosePiece> It(World); It; ++It)
		{
			It->Destroy();
		}
	}
}
