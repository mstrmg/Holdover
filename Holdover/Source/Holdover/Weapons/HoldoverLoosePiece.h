#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HoldoverTypes.h"
#include "HoldoverLoosePiece.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

UENUM(BlueprintType)
enum class EHoldoverPieceKind : uint8
{
	Casing,
	LiveRound,
	Magazine
};

/**
 * A loose physical object that came out of a gun: a fired case, a live round, or a magazine.
 * Magazines carry their own round count and can be picked up again.
 */
UCLASS()
class HOLDOVER_API AHoldoverLoosePiece : public AActor
{
	GENERATED_BODY()

public:
	AHoldoverLoosePiece();

	/**
	 * Sets the look of the piece. With no mesh a gray-box shape of FallbackSize (cm) is used:
	 * a cylinder for cases and rounds, a box for magazines.
	 */
	void Setup(EHoldoverPieceKind InKind, UStaticMesh* Mesh, const FVector& FallbackSize, float MassKg);

	/** Moves the piece and throws it. Velocities are in cm/s and deg/s. */
	void Launch(const FTransform& Transform, const FVector& Velocity, const FVector& AngularVelocityDeg);

	UStaticMeshComponent* GetMesh() const { return Mesh; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Piece")
	EHoldoverPieceKind Kind = EHoldoverPieceKind::Casing;

	/** Only meaningful for magazines. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Piece")
	FHoldoverMagazine Magazine;

	/** Gun type this piece belongs to (so a mag only goes back in a matching gun). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Piece")
	FName GunId;

private:
	UPROPERTY(VisibleAnywhere, Category = "Piece")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
