#include "Weapons/HoldoverDebrisSubsystem.h"

#include "Engine/World.h"

bool UHoldoverDebrisSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

AHoldoverLoosePiece* UHoldoverDebrisSubsystem::Eject(EHoldoverPieceKind Kind, UStaticMesh* Mesh, const FVector& FallbackSize,
	float MassKg, const FTransform& Transform, const FVector& Velocity, const FVector& AngularVelocityDeg)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	Pieces.RemoveAll([](const TObjectPtr<AHoldoverLoosePiece>& P) { return !IsValid(P); });

	AHoldoverLoosePiece* Piece = nullptr;
	if (Pieces.Num() < MaxPieces)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Piece = World->SpawnActor<AHoldoverLoosePiece>(AHoldoverLoosePiece::StaticClass(), Transform, Params);
		if (Piece)
		{
			Pieces.Add(Piece);
		}
	}
	else
	{
		NextRecycle = NextRecycle % Pieces.Num();
		Piece = Pieces[NextRecycle];
		++NextRecycle;
	}

	if (Piece)
	{
		Piece->Setup(Kind, Mesh, FallbackSize, MassKg);
		Piece->Launch(Transform, Velocity, AngularVelocityDeg);
	}
	return Piece;
}

void UHoldoverDebrisSubsystem::ClearAll()
{
	for (AHoldoverLoosePiece* P : Pieces)
	{
		if (IsValid(P))
		{
			P->Destroy();
		}
	}
	Pieces.Reset();
	NextRecycle = 0;
}
