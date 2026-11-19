#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Engine/HitResult.h"
#include "HoldoverBulletReceiver.generated.h"

/** Result of a bullet striking something that wants to know about it. */
struct FHoldoverBulletImpact
{
	FHitResult Hit;
	FVector Velocity = FVector::ZeroVector; // cm/s at impact
	float MassKg = 0.f;
	float DiameterCm = 0.f;
	TWeakObjectPtr<AActor> Shooter;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHoldoverBulletReceiver : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by actors that react to bullets (paper targets, steel, poppers). */
class HOLDOVER_API IHoldoverBulletReceiver
{
	GENERATED_BODY()

public:
	/**
	 * Called when a simulated bullet hits this actor.
	 * @param OutExitVelocityScale  Set to the fraction of speed the bullet keeps if it passes through (paper about 0.98).
	 * @return true if the bullet passes through and keeps flying, false if it stops here.
	 */
	virtual bool ReceiveBulletHit(const FHoldoverBulletImpact& Impact, float& OutExitVelocityScale) = 0;
};
