#include "Player/HoldoverInputSet.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

UInputAction* UHoldoverInputSet::MakeAction(const TCHAR* Name, bool bAxis2D)
{
	UInputAction* A = NewObject<UInputAction>(this, FName(Name));
	A->ValueType = bAxis2D ? EInputActionValueType::Axis2D : EInputActionValueType::Boolean;
	return A;
}

void UHoldoverInputSet::Build()
{
	Move = MakeAction(TEXT("IA_Move"), true);
	Look = MakeAction(TEXT("IA_Look"), true);
	Jog = MakeAction(TEXT("IA_Jog"), false);
	Trigger = MakeAction(TEXT("IA_Trigger"), false);
	Aim = MakeAction(TEXT("IA_Aim"), false);
	Rack = MakeAction(TEXT("IA_Rack"), false);
	SlideStop = MakeAction(TEXT("IA_SlideStop"), false);
	MagDrop = MakeAction(TEXT("IA_MagDrop"), false);
	MagStow = MakeAction(TEXT("IA_MagStow"), false);
	MagInsert = MakeAction(TEXT("IA_MagInsert"), false);
	MagCheck = MakeAction(TEXT("IA_MagCheck"), false);
	PressCheck = MakeAction(TEXT("IA_PressCheck"), false);
	Safety = MakeAction(TEXT("IA_Safety"), false);
	Interact = MakeAction(TEXT("IA_Interact"), false);
	SelectPistol = MakeAction(TEXT("IA_SelectPistol"), false);
	SelectRifle = MakeAction(TEXT("IA_SelectRifle"), false);
	ResetRange = MakeAction(TEXT("IA_ResetRange"), false);
	Reload = MakeAction(TEXT("IA_Reload"), false);

	ManualContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Manual"));
	AssistedContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Assisted"));

	MapCommon(ManualContext);
	MapCommon(AssistedContext);
}

void UHoldoverInputSet::MapCommon(UInputMappingContext* C)
{
	// WASD: the move action's X is right, Y is forward.
	{
		FEnhancedActionKeyMapping& W = C->MapKey(Move, EKeys::W);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		W.Modifiers.Add(Swizzle);
	}
	{
		FEnhancedActionKeyMapping& S = C->MapKey(Move, EKeys::S);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		S.Modifiers.Add(Swizzle);
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	C->MapKey(Move, EKeys::D);
	{
		FEnhancedActionKeyMapping& A = C->MapKey(Move, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}

	// Mouse: X turns, Y looks up and down (mouse forward = look up).
	{
		FEnhancedActionKeyMapping& M = C->MapKey(Look, EKeys::Mouse2D);
		UInputModifierNegate* NegY = NewObject<UInputModifierNegate>(this);
		NegY->bX = false;
		NegY->bY = true;
		NegY->bZ = false;
		M.Modifiers.Add(NegY);
	}

	C->MapKey(Jog, EKeys::LeftShift);
	C->MapKey(Trigger, EKeys::LeftMouseButton);
	C->MapKey(Aim, EKeys::RightMouseButton);
	C->MapKey(Rack, EKeys::R);
	C->MapKey(SlideStop, EKeys::T);
	C->MapKey(MagDrop, EKeys::Q);
	C->MapKey(MagStow, EKeys::G);
	C->MapKey(MagInsert, EKeys::E);
	C->MapKey(MagCheck, EKeys::C);
	C->MapKey(PressCheck, EKeys::X);
	C->MapKey(Safety, EKeys::V);
	C->MapKey(Interact, EKeys::F);
	C->MapKey(SelectPistol, EKeys::One);
	C->MapKey(SelectRifle, EKeys::Two);
	C->MapKey(ResetRange, EKeys::BackSpace);
}
