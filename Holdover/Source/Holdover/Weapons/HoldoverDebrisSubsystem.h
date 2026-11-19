#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Weapons/HoldoverLoosePiece.h"
#include "HoldoverDebrisSubsystem.generated.h"

/**
 * Keeps ejected cases and rounds in the world up to a cap, reusing the oldest
 * when the cap is reached (CPU-friendly for the Ryzen 5 3600).
 */
UCLASS()
class HOLDOVER_API UHoldoverDebrisSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Spawns (or recycles) a case or live round and throws it. */
	AHoldoverLoosePiece* Eject(EHoldoverPieceKind Kind, UStaticMesh* Mesh, const FVector& FallbackSize, float MassKg,
		const FTransform& Transform, const FVector& Velocity, const FVector& AngularVelocityDeg);

	/** Removes every case and round. */
	void ClearAll();

	/** Most cases and rounds kept on the ground at once. */
	int32 MaxPieces = 150;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY()
	TArray<TObjectPtr<AHoldoverLoosePiece>> Pieces;

	int32 NextRecycle = 0;
};
