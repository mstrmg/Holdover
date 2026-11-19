#pragma once

#include "CoreMinimal.h"

/**
 * Gun animation timelines loaded from Holdover/Data/GunAnims/<GunId>.json
 * (format: docs/animation/gun_anim_format.md).
 *
 * On load every value is converted to Unreal conventions: millimetres to centimetres,
 * Blender's Y-left to Unreal's Y-right (positions and directions mirror Y; rotation axes
 * become (-x, y, -z) with the same angle, which is the exact mirror of a rotation).
 */
namespace HoldoverAnim
{
	enum class EEase : uint8 { Linear, Smooth, Step };
	enum class ESpace : uint8 { Gun, Camera, Body };

	struct FVecKey
	{
		float T = 0.f;
		FVector V = FVector::ZeroVector;
		EEase Ease = EEase::Linear;
		bool bFromStart = false; // "v": null on the first key
	};

	struct FRotKey
	{
		float T = 0.f;
		FQuat Q = FQuat::Identity;
		EEase Ease = EEase::Linear;
		bool bFromStart = false;
	};

	struct FBoneTrack
	{
		TArray<FVecKey> Loc;
		TArray<FRotKey> Rot;
		bool IsEmpty() const { return Loc.Num() == 0 && Rot.Num() == 0; }
		float FirstTime() const;
	};

	/** A hand target: palm centre plus finger direction and palm normal (out of the palm). */
	struct FHandTarget
	{
		ESpace Space = ESpace::Gun;
		FVector Pos = FVector::ZeroVector;
		FVector FingersDir = FVector::ForwardVector;
		FVector PalmNormal = FVector::UpVector;
	};

	struct FHandKey
	{
		float T = 0.f;
		EEase Ease = EEase::Smooth;
		bool bHold = false;
		FHandTarget Target;
	};

	struct FFingerKey
	{
		float T = 0.f;
		FName Pose;
		float Weight = 0.f;
		EEase Ease = EEase::Smooth;
	};

	struct FAttachKey
	{
		float T = 0.f;
		bool bLeftHand = true;
		FName Bone; // None = let go
		/** Where this hand holds the part when the part sits at its rest pose (gun space). Fixes the carry offset. */
		bool bHasGrip = false;
		FHandTarget Grip;
	};

	struct FEventKey
	{
		float T = 0.f;
		FName Name;
	};

	struct FAction
	{
		FName Name;
		float Duration = 0.f;
		float HoldAt = -1.f;
		TMap<FName, FBoneTrack> Bones;
		FName GunPivot;
		TArray<FVecKey> GunLoc;   // camera space, cm
		TArray<FRotKey> GunRot;   // camera space
		TArray<FHandKey> HandL;
		TArray<FHandKey> HandR;
		TArray<FFingerKey> FingersL;
		TArray<FFingerKey> FingersR;
		TArray<FAttachKey> Attach;
		TArray<FEventKey> Events;

		bool HasEvent(FName EventName, float NearTime = -1.f, float Tolerance = 0.03f) const;
	};

	struct FHold
	{
		bool bValid = false;
		FHandTarget HandL;
		FHandTarget HandR;
		TMap<FName, float> FingersL;
		TMap<FName, float> FingersR;
	};

	/** All timelines for one gun. */
	class HOLDOVER_API FGunAnimSet
	{
	public:
		FName GunId;
		FHold Hold;
		TMap<FName, FAction> Actions;
		/** State poses ("SlideLocked", "HammerCocked"...), bone -> (loc, rot), already converted. */
		TMap<FName, TMap<FName, TPair<FVector, FQuat>>> States;

		const FAction* Find(FName Action) const { return Actions.Find(Action); }
		bool Has(FName Action) const { return Actions.Contains(Action); }

		/** Loads Holdover/Data/GunAnims/<GunId>.json. Returns null (and logs why) if missing or broken. */
		static TSharedPtr<FGunAnimSet> Load(FName GunId);
	};

	// ---------- Sampling helpers ----------

	float ApplyEase(float Alpha, EEase Ease);

	/** Samples a vector track. StartValue replaces a "from start" first key. */
	FVector SampleVec(const TArray<FVecKey>& Keys, float T, const FVector& StartValue);

	/** Samples a rotation track. StartValue replaces a "from start" first key. */
	FQuat SampleRot(const TArray<FRotKey>& Keys, float T, const FQuat& StartValue);

	/**
	 * Samples one finger pose's weight. Returns false if the pose has no keys (keep the hold weight).
	 */
	bool SampleFinger(const TArray<FFingerKey>& Keys, FName Pose, float T, float& OutWeight);

	/** Orientation whose X is the finger direction and Z the palm normal. */
	FQuat HandBasis(const FVector& FingersDir, const FVector& PalmNormal);

	// Conversions from the file's Blender-style space.
	inline FVector ConvPos(double X, double Y, double Z) { return FVector(X, -Y, Z) * 0.1; }
	inline FVector ConvDir(double X, double Y, double Z) { return FVector(X, -Y, Z).GetSafeNormal(); }
	inline FQuat ConvRot(double AX, double AY, double AZ, double Deg)
	{
		const FVector Axis = FVector(-AX, AY, -AZ).GetSafeNormal();
		if (Axis.IsNearlyZero() || FMath::IsNearlyZero(Deg))
		{
			return FQuat::Identity;
		}
		return FQuat(Axis, FMath::DegreesToRadians(Deg));
	}
}
