#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/HoldoverTypes.h"
#include "HoldoverBallisticsSubsystem.generated.h"

/**
 * Flies every bullet in the world as a point mass with gravity and air drag.
 * No hitscan: each frame is split into short steps and each step is traced
 * against the "Bullet" channel, so travel time and drop are real.
 */
UCLASS()
class HOLDOVER_API UHoldoverBallisticsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Launch a bullet. Velocity in cm/s. Actors in IgnoreActors are never hit. */
	void FireBullet(const FHoldoverBulletSpec& Spec, const FVector& Origin, const FVector& Velocity,
		AActor* Shooter, const TArray<AActor*>& IgnoreActors);

	/**
	 * Angle in radians the bore must point above the sight line so the bullet
	 * crosses the sight line again at the zero distance.
	 * @param SightHeightCm  Height of the sight line above the bore.
	 */
	static float ComputeZeroAngle(const FHoldoverBulletSpec& Spec, float SightHeightCm, float GravityZ);

	int32 GetBulletsInFlight() const { return Bullets.Num(); }

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FBullet
	{
		FVector Position;
		FVector Velocity;
		float DragK = 0.f;
		float MassKg = 0.f;
		float DiameterCm = 0.f;
		float Age = 0.f;
		float MaxAge = 4.f;
		int32 PassThroughs = 0;
		TWeakObjectPtr<AActor> Shooter;
		TArray<TWeakObjectPtr<AActor>> Ignore;
	};

	/** Moves one bullet through one step. Returns false when the bullet is finished. */
	bool StepBullet(FBullet& B, float Dt, float GravityZ);

	TArray<FBullet> Bullets;

	static constexpr int32 MaxBullets = 512;
	static constexpr float MaxStep = 1.f / 240.f;
	static constexpr int32 MaxPassThroughs = 4;
};
