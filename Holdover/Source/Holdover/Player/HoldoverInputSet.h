#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HoldoverInputSet.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * All input actions and the key layout, built in code so there are no input assets to make.
 * Two mapping contexts, as planned: full manual (built now) and assisted (shares most keys,
 * adds a one-key reload later).
 */
UCLASS()
class HOLDOVER_API UHoldoverInputSet : public UObject
{
	GENERATED_BODY()

public:
	/** Creates every action and fills the mapping contexts with the default keys. */
	void Build();

	UPROPERTY(Transient) TObjectPtr<UInputAction> Move;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Look;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Jog;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Trigger;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Aim;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Rack;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SlideStop;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MagDrop;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MagStow;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MagInsert;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MagCheck;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PressCheck;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Safety;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Interact;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SelectPistol;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SelectRifle;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Select1911;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SelectFAL;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SelectAR;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ResetRange;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Reload; // assisted mode only

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> ManualContext;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> AssistedContext;

private:
	UInputAction* MakeAction(const TCHAR* Name, bool bAxis2D);
	void MapCommon(UInputMappingContext* Context);
};
