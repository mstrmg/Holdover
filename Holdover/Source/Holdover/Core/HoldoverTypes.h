#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "HoldoverTypes.generated.h"

/** Trace channel every simulated bullet uses ("Bullet" in DefaultEngine.ini). */
#define ECC_HoldoverBullet ECC_GameTraceChannel1

/** What sits in the chamber. */
UENUM(BlueprintType)
enum class EHoldoverChamber : uint8
{
	Empty,
	Live,   // unfired round
	Spent,  // fired case still in the chamber (only until the slide or bolt extracts it)
	Dud     // round that failed to fire: still a full round, but it will not fire again
};

/** How the slide or bolt is currently moving. */
UENUM(BlueprintType)
enum class EHoldoverSlideMotion : uint8
{
	Idle,         // at rest: in battery, or locked back on the slide stop
	Cycling,      // driven by the shot, travelling to the rear
	HandPulling,  // the player is pulling it back
	HandHeld,     // held at the rear by the player's hand
	Returning     // spring is driving it forward
};

UENUM(BlueprintType)
enum class EHoldoverFireMode : uint8
{
	Safe,
	Semi,
	Auto
};

UENUM(BlueprintType)
enum class EHoldoverMalfunction : uint8
{
	None,
	Stovepipe,   // spent case caught upright in the ejection port; slide slightly open
	DoubleFeed,  // fresh round pushed into the back of a case still in the chamber
	Dud          // the round in the chamber did not go off (fixed by tap-rack)
};

/** One physical magazine. Each keeps its own count, wherever it is. */
USTRUCT(BlueprintType)
struct FHoldoverMagazine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine")
	int32 Rounds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine")
	int32 Capacity = 15;

	/** Stable id so a specific mag can be followed (pouch, gun, ground). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine")
	int32 Id = 0;

	FHoldoverMagazine() = default;
	FHoldoverMagazine(int32 InRounds, int32 InCapacity, int32 InId)
		: Rounds(InRounds), Capacity(InCapacity), Id(InId) {}

	bool IsEmpty() const { return Rounds <= 0; }
	bool IsFull() const { return Rounds >= Capacity; }
};

/** Everything a bullet needs to fly. Units: centimetres, seconds, kilograms. */
USTRUCT(BlueprintType)
struct FHoldoverBulletSpec
{
	GENERATED_BODY()

	/** Muzzle velocity in metres per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics", meta = (ClampMin = "50"))
	float MuzzleVelocity = 360.f;

	/** Bullet mass in grams (9 mm 124 grain is 8.0 g). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics", meta = (ClampMin = "0.5"))
	float MassGrams = 8.0f;

	/** Bullet diameter in millimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics", meta = (ClampMin = "1"))
	float DiameterMm = 9.01f;

	/** Drag coefficient of the bullet shape (round nose about 0.3, spitzer rifle about 0.25-0.3 at supersonic speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics", meta = (ClampMin = "0.05", ClampMax = "1.5"))
	float DragCoefficient = 0.30f;

	/** Mechanical accuracy of gun and ammo, in minutes of angle (cone diameter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics", meta = (ClampMin = "0"))
	float AccuracyMOA = 3.0f;

	/** Distance in metres at which the sights are zeroed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics", meta = (ClampMin = "5"))
	float ZeroDistanceMeters = 25.f;

	/** Seconds before an in-flight bullet is dropped from the simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ballistics", meta = (ClampMin = "0.5"))
	float MaxFlightTime = 4.f;

	float GetMassKg() const { return MassGrams * 0.001f; }

	/** Quadratic drag factor k in 1/cm, so that drag deceleration = k * |v| * v (v in cm/s). */
	float GetDragFactorPerCm() const
	{
		const float AirDensity = 1.225f; // kg/m^3 at sea level
		const float Radius = DiameterMm * 0.0005f; // m
		const float Area = PI * Radius * Radius; // m^2
		const float KPerMeter = 0.5f * AirDensity * DragCoefficient * Area / FMath::Max(GetMassKg(), 1e-4f);
		return KPerMeter * 0.01f;
	}
};
