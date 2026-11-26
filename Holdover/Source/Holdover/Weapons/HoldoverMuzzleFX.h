#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HoldoverMuzzleFX.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UHoldoverGunData;

/** Which kind of flash a shot makes (picked at random per shot, or forced with holdover.FlashVariant). */
UENUM(BlueprintType)
enum class EHoldoverFlashVariant : uint8
{
	Faint,     // barely leaves the barrel
	Normal,    // a flash shaped by the muzzle device
	Fireball   // a normal flash plus a big ball of gas igniting further out
};

/**
 * Muzzle flash, smoke and the flash's light, all driven from code (no particle assets, so every number can be
 * read and tuned here). Each shot builds a fresh flash from a few textured cards: a hot core, a flame plume along the
 * bore, jets shaped by the muzzle device, sometimes a fireball further out and a few burning powder sparks, then
 * smoke that drifts and spreads. Size, shape, brightness and duration change every shot.
 *
 * Textures and materials are made by tools/fx/gen_fx_textures.py and tools/unreal/import_fx.py
 * (/Game/FX/M_MuzzleFlash, /Game/FX/M_MuzzleSmoke). Without them only the light flashes.
 */
UCLASS(ClassGroup = (Holdover), meta = (BlueprintSpawnableComponent))
class HOLDOVER_API UHoldoverMuzzleFX : public UActorComponent
{
	GENERATED_BODY()

public:
	UHoldoverMuzzleFX();

	/**
	 * Fires the flash. MuzzleWorld: the muzzle socket (X along the bore). PortWorld: the ejection port, for the
	 * smoke wisp that leaks out of the action.
	 */
	void Fire(const UHoldoverGunData* Data, const FTransform& MuzzleWorld, const FVector& PortWorld);

	/** The variant of the last shot (for tests). */
	EHoldoverFlashVariant GetLastVariant() const { return LastVariant; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Light colour of the flash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Muzzle Flash")
	FLinearColor LightColor = FLinearColor(1.f, 0.62f, 0.28f);

	/** How far the flash's light reaches, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Muzzle Flash")
	float LightRadius = 900.f;

	/** Overall brightness of every flash card (1 = as tuned). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Muzzle Flash")
	float CardBrightness = 1.f;

	/** Gentle breeze that carries the smoke, cm/s in world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Muzzle Flash")
	FVector Wind = FVector(4.f, 12.f, 0.f);

protected:
	virtual void BeginPlay() override;

private:
	enum class ECardMode : uint8 { Facing, Axial };

	struct FCard
	{
		UStaticMeshComponent* Comp = nullptr;          // kept alive by PoolComponents
		UMaterialInstanceDynamic* MID = nullptr;       // kept alive by PoolMaterials
		bool bActive = false;
		bool bSmoke = false;
		bool bGunLocal = false;    // Start and Axis are in the gun's own space: the flash rides with the muzzle
		ECardMode Mode = ECardMode::Facing;
		float Age = 0.f;
		float Life = 0.05f;
		float Tau = 0.01f;         // flash: brightness falls by e every Tau seconds
		float Delay = 0.f;         // appears this long after the shot
		FVector Start = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Drag = 0.f;          // 1/s
		float Rise = 0.f;          // cm/s^2 upward (smoke buoyancy)
		FVector Axis = FVector::ForwardVector;
		FVector2D Size0 = FVector2D(5.f, 5.f);
		FVector2D Size1 = FVector2D(5.f, 5.f);
		float Spin = 0.f;          // in-plane rotation, degrees
		float SpinRate = 0.f;
		float Peak = 1.f;          // flash intensity or smoke opacity
		FLinearColor Tint = FLinearColor::White;
		float Frame = 0.f;
	};

	struct FCardSpec
	{
		ECardMode Mode = ECardMode::Facing;
		FVector Start = FVector::ZeroVector;
		FVector Axis = FVector::ForwardVector;
		FVector Velocity = FVector::ZeroVector;
		FVector2D Size0 = FVector2D(5.f, 5.f);
		FVector2D Size1 = FVector2D(5.f, 5.f);
		float Life = 0.05f;
		float Tau = 0.01f;
		float Delay = 0.f;
		float Peak = 1.f;
		int32 Frame = 0;
		float Spin = 0.f;
		float SpinRate = 0.f;
		float Drag = 0.f;
		float Rise = 0.f;
		FLinearColor Tint = FLinearColor::White;
		bool bGunLocal = false;
	};

	void EnsurePool();
	float SunLux();
	float CachedSunLux = -1.f;
	FCard* Grab(bool bSmoke);
	void Spawn(const FCardSpec& Spec, bool bSmoke);
	void UpdateCard(FCard& C, const FVector& CameraLoc, float Dt, bool bFreeze);
	void HideCard(FCard& C);

	void BuildFlash(const UHoldoverGunData* Data, const FTransform& Muzzle, EHoldoverFlashVariant Variant);
	void BuildSmoke(const UHoldoverGunData* Data, const FTransform& Muzzle, const FVector& Port, EHoldoverFlashVariant Variant);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FlashMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SmokeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Light;

	/** Keeps the pooled card components alive. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PoolComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PoolMaterials;

	TArray<FCard> FlashCards;
	TArray<FCard> SmokeCards;
	int32 NextSmoke = 0;

	bool bPoolReady = false;
	bool bWarnedMissing = false;

	// Light pulse of the last shot
	float LightAge = 100.f;
	float LightPeak = 0.f;
	float LightTau = 0.01f;
	float LightLife = 0.f;
	FVector LightPos = FVector::ZeroVector; // gun space

	EHoldoverFlashVariant LastVariant = EHoldoverFlashVariant::Normal;
};
