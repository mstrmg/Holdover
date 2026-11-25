#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HoldoverTypes.h"
#include "HoldoverLoosePiece.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UPrimitiveComponent;
class UPhysicalMaterial;

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
 * Cases and rounds clink when they land and bounce, louder the harder they hit, with the sound of the surface.
 */
UCLASS()
class HOLDOVER_API AHoldoverLoosePiece : public AActor
{
	GENERATED_BODY()

public:
	AHoldoverLoosePiece();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Sets the look of the piece. With no mesh a gray-box shape of FallbackSize (cm) is used:
	 * a cylinder for cases and rounds, a box for magazines.
	 */
	void Setup(EHoldoverPieceKind InKind, UStaticMesh* Mesh, const FVector& FallbackSize, float MassKg);

	/** Moves the piece and throws it. Velocities are in cm/s and deg/s. */
	void Launch(const FTransform& Transform, const FVector& Velocity, const FVector& AngularVelocityDeg);

	/**
	 * Draws the piece with the first-person gun and hands for this long after launch, so a case comes out of the
	 * ejection port you see instead of from behind the gun, then hands it to the normal world view.
	 */
	void DrawWithGunFor(float Seconds);

	UStaticMeshComponent* GetMesh() const { return Mesh; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Piece")
	EHoldoverPieceKind Kind = EHoldoverPieceKind::Casing;

	/** Only meaningful for magazines. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Piece")
	FHoldoverMagazine Magazine;

	/** Gun type this piece belongs to (so a mag only goes back in a matching gun). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Piece")
	FName GunId;

	/** Which bouncing-brass sound set to use ("Pistol" or "Rifle"). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Piece")
	FName SoundSet = TEXT("Pistol");

	/** Slowest impact (change of speed, cm/s) that makes a sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piece")
	float MinSoundImpact = 25.f;

	/** Impact speed (cm/s) for a full-volume clink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piece")
	float FullSoundImpact = 450.f;

private:
	UFUNCTION()
	void OnPieceHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Bouncy, slippery brass (cases and rounds). */
	static UPhysicalMaterial* BrassMaterial();

	UPROPERTY(VisibleAnywhere, Category = "Piece")
	TObjectPtr<UStaticMeshComponent> Mesh;

	float MassKg = 0.004f;
	float LastSoundTime = -1.f;
	float FirstPersonUntil = -1.f;
	int32 LastVariant = -1;
};
