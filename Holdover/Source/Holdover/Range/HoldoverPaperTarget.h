#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HoldoverBulletReceiver.h"
#include "HoldoverPaperTarget.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class AHoldoverPaperTarget;

/** One hit on a paper target. */
USTRUCT(BlueprintType)
struct FHoldoverPaperHit
{
	GENERATED_BODY()

	/** Ring scored, 0 for a miss on the paper outside the rings. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target")
	int32 Ring = 0;

	/** Hit position on the face in cm from the centre (X right, Y up as seen by the shooter). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target")
	FVector2D FacePosition = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target")
	float Time = 0.f;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHoldoverPaperHitSignal, AHoldoverPaperTarget*, const FHoldoverPaperHit&);

/**
 * A paper bullseye on a wooden stand. Bullets punch real holes where they actually hit
 * (front and back), so groups can be walked up to and inspected. The actor's +X faces the shooter.
 */
UCLASS()
class HOLDOVER_API AHoldoverPaperTarget : public AActor, public IHoldoverBulletReceiver
{
	GENERATED_BODY()

public:
	AHoldoverPaperTarget();

	// IHoldoverBulletReceiver
	virtual bool ReceiveBulletHit(const FHoldoverBulletImpact& Impact, float& OutExitVelocityScale) override;

	/** Removes all holes and clears the hit list (fresh paper). */
	void ResetPaper();

	const TArray<FHoldoverPaperHit>& GetHits() const { return Hits; }

	/** Ring radii in cm, from the 10 ring outwards. Line-breakers score the higher ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	TArray<float> RingRadii = { 2.5f, 5.f, 7.5f, 10.f, 12.5f, 15.f, 17.5f, 20.f };

	/** Rings inside this radius are printed on black. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	float BlackRadius = 10.f;

	/** Width and height of the paper in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	FVector2D PaperSize = FVector2D(45.f, 60.f);

	/** Height of the bullseye centre above the ground, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	float CentreHeight = 140.f;

	/** Most holes kept per target before the oldest are reused. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	int32 MaxHoles = 200;

	FHoldoverPaperHitSignal OnHit;

protected:
	virtual void BeginPlay() override;

private:
	void BuildTarget();
	int32 ScoreRadius(float RadiusCm, float BulletRadiusCm) const;
	void AddHole(const FVector& LocalOnFace, float DiameterCm);

	UPROPERTY(VisibleAnywhere, Category = "Target")
	TObjectPtr<USceneComponent> Root;

	/** Backer board: the only part bullets collide with. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Board;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Holes;

	TArray<FHoldoverPaperHit> Hits;
	int32 NextHole = 0;
	float FaceFrontX = 0.f;
	float BoardThickness = 1.2f;
	bool bBuilt = false;
};
