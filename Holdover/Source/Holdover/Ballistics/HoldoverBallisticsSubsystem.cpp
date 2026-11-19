#include "Ballistics/HoldoverBallisticsSubsystem.h"

#include "Core/HoldoverBulletReceiver.h"
#include "Core/HoldoverDebug.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Holdover.h"

TStatId UHoldoverBallisticsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHoldoverBallisticsSubsystem, STATGROUP_Tickables);
}

bool UHoldoverBallisticsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

float UHoldoverBallisticsSubsystem::ComputeZeroAngle(const FHoldoverBulletSpec& Spec, float SightHeightCm, float GravityZ)
{
	// With quadratic drag (ignoring gravity's tiny effect on speed): v(x) = v0 * exp(-k x),
	// so the time to reach distance x is t = (exp(k x) - 1) / (k v0).
	const float V0 = Spec.MuzzleVelocity * 100.f;
	const float X = Spec.ZeroDistanceMeters * 100.f;
	const float K = Spec.GetDragFactorPerCm();
	const float T = (K > KINDA_SMALL_NUMBER) ? (FMath::Exp(K * X) - 1.f) / (K * V0) : X / V0;
	const float Drop = 0.5f * FMath::Abs(GravityZ) * T * T;
	return FMath::Atan2(SightHeightCm + Drop, X);
}

void UHoldoverBallisticsSubsystem::FireBullet(const FHoldoverBulletSpec& Spec, const FVector& Origin, const FVector& Velocity,
	AActor* Shooter, const TArray<AActor*>& IgnoreActors)
{
	if (Bullets.Num() >= MaxBullets)
	{
		Bullets.RemoveAt(0);
	}

	FBullet& B = Bullets.AddDefaulted_GetRef();
	B.Position = Origin;
	B.Velocity = Velocity;
	B.DragK = Spec.GetDragFactorPerCm();
	B.MassKg = Spec.GetMassKg();
	B.DiameterCm = Spec.DiameterMm * 0.1f;
	B.MaxAge = Spec.MaxFlightTime;
	B.Shooter = Shooter;
	for (AActor* A : IgnoreActors)
	{
		B.Ignore.Add(A);
	}
}

void UHoldoverBallisticsSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || Bullets.Num() == 0 || DeltaTime <= 0.f)
	{
		return;
	}

	const float GravityZ = World->GetGravityZ();
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime / MaxStep), 1, 64);
	const float Dt = DeltaTime / Steps;

	for (int32 i = Bullets.Num() - 1; i >= 0; --i)
	{
		bool bAlive = true;
		for (int32 s = 0; s < Steps && bAlive; ++s)
		{
			bAlive = StepBullet(Bullets[i], Dt, GravityZ);
		}
		if (!bAlive)
		{
			Bullets.RemoveAtSwap(i);
		}
	}
}

bool UHoldoverBallisticsSubsystem::StepBullet(FBullet& B, float Dt, float GravityZ)
{
	UWorld* World = GetWorld();

	B.Age += Dt;
	if (B.Age > B.MaxAge || B.Velocity.SizeSquared() < 100.f * 100.f)
	{
		return false;
	}

	// Semi-implicit Euler: update velocity from drag and gravity, then position.
	const float Speed = B.Velocity.Size();
	const FVector Accel = FVector(0.f, 0.f, GravityZ) - B.DragK * Speed * B.Velocity;
	B.Velocity += Accel * Dt;

	FVector Start = B.Position;
	const FVector End = B.Position + B.Velocity * Dt;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HoldoverBullet), /*bTraceComplex*/ true);
	Params.bReturnPhysicalMaterial = true;
	for (const TWeakObjectPtr<AActor>& A : B.Ignore)
	{
		if (A.IsValid())
		{
			Params.AddIgnoredActor(A.Get());
		}
	}

	// A bullet can pass through paper and hit something behind it in the same step.
	for (int32 Guard = 0; Guard <= MaxPassThroughs; ++Guard)
	{
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_HoldoverBullet, Params);

		if (HoldoverDebug::Bullets())
		{
			DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End,
				bHit ? FColor::Red : FColor::Orange, false, 3.f, 0, 0.15f);
		}

		if (!bHit)
		{
			B.Position = End;
			return true;
		}

		FHoldoverBulletImpact Impact;
		Impact.Hit = Hit;
		Impact.Velocity = B.Velocity;
		Impact.MassKg = B.MassKg;
		Impact.DiameterCm = B.DiameterCm;
		Impact.Shooter = B.Shooter;

		// Push physics objects (dropped mags, casings, cans).
		if (UPrimitiveComponent* Comp = Hit.GetComponent())
		{
			if (Comp->IsSimulatingPhysics())
			{
				Comp->AddImpulseAtLocation(B.Velocity * B.MassKg, Hit.ImpactPoint, Hit.BoneName);
			}
		}

		bool bPassThrough = false;
		float ExitScale = 1.f;
		if (AActor* HitActor = Hit.GetActor())
		{
			if (IHoldoverBulletReceiver* Receiver = Cast<IHoldoverBulletReceiver>(HitActor))
			{
				bPassThrough = Receiver->ReceiveBulletHit(Impact, ExitScale);
			}
		}

		if (!bPassThrough || B.PassThroughs >= MaxPassThroughs)
		{
			if (HoldoverDebug::Bullets())
			{
				DrawDebugPoint(World, Hit.ImpactPoint, 8.f, FColor::Red, false, 5.f);
			}
			return false;
		}

		// Carry on behind the thin object, a little slower.
		++B.PassThroughs;
		B.Velocity *= FMath::Clamp(ExitScale, 0.f, 1.f);
		const FVector Dir = (End - Start).GetSafeNormal();
		Start = Hit.ImpactPoint + Dir * 0.5f;
		if (AActor* HitActor = Hit.GetActor())
		{
			Params.AddIgnoredActor(HitActor);
			B.Ignore.Add(HitActor);
		}
		if (FVector::DotProduct(End - Start, Dir) <= 0.f)
		{
			B.Position = Start;
			return true;
		}
	}

	B.Position = End;
	return true;
}
