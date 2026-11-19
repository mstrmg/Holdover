#include "Animation/HoldoverGunAnimSet.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Holdover.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace HoldoverAnim
{
	// ------------------------------------------------------------------
	// Small JSON helpers (tolerant: missing fields fall back to defaults)
	// ------------------------------------------------------------------

	using FJsonObj = TSharedPtr<FJsonObject>;
	using FJsonArr = TArray<TSharedPtr<FJsonValue>>;

	static double Num(const FJsonObj& O, const TCHAR* Field, double Default = 0.0)
	{
		double V = Default;
		if (O.IsValid())
		{
			O->TryGetNumberField(Field, V);
		}
		return V;
	}

	static FString Str(const FJsonObj& O, const TCHAR* Field)
	{
		FString S;
		if (O.IsValid())
		{
			O->TryGetStringField(Field, S);
		}
		return S;
	}

	static const FJsonArr* Arr(const FJsonObj& O, const TCHAR* Field)
	{
		const FJsonArr* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A))
		{
			return A;
		}
		return nullptr;
	}

	static FJsonObj Obj(const FJsonObj& O, const TCHAR* Field)
	{
		if (O.IsValid() && O->HasTypedField<EJson::Object>(Field))
		{
			return O->GetObjectField(Field);
		}
		return nullptr;
	}

	static bool ReadTriple(const TSharedPtr<FJsonValue>& V, double& X, double& Y, double& Z)
	{
		if (!V.IsValid() || V->Type != EJson::Array)
		{
			return false;
		}
		const FJsonArr& A = V->AsArray();
		if (A.Num() < 3)
		{
			return false;
		}
		X = A[0]->AsNumber();
		Y = A[1]->AsNumber();
		Z = A[2]->AsNumber();
		return true;
	}

	static EEase ReadEase(const FJsonObj& O)
	{
		const FString E = Str(O, TEXT("ease"));
		if (E == TEXT("smooth")) return EEase::Smooth;
		if (E == TEXT("step")) return EEase::Step;
		return EEase::Linear;
	}

	static ESpace ReadSpace(const FJsonObj& O)
	{
		const FString S = Str(O, TEXT("space"));
		if (S == TEXT("camera")) return ESpace::Camera;
		if (S == TEXT("body")) return ESpace::Body;
		return ESpace::Gun;
	}

	static FQuat ReadRotValue(const TSharedPtr<FJsonValue>& V)
	{
		if (!V.IsValid() || V->Type != EJson::Object)
		{
			return FQuat::Identity;
		}
		const FJsonObj R = V->AsObject();
		double X = 0, Y = 1, Z = 0;
		const TSharedPtr<FJsonValue> AxisV = R->TryGetField(TEXT("axis"));
		ReadTriple(AxisV, X, Y, Z);
		return ConvRot(X, Y, Z, Num(R, TEXT("deg")));
	}

	static void ReadVecKeys(const FJsonArr* Keys, TArray<FVecKey>& Out)
	{
		if (!Keys)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& KV : *Keys)
		{
			const FJsonObj K = KV.IsValid() ? KV->AsObject() : nullptr;
			if (!K.IsValid())
			{
				continue;
			}
			FVecKey Key;
			Key.T = static_cast<float>(Num(K, TEXT("t")));
			Key.Ease = ReadEase(K);
			const TSharedPtr<FJsonValue> V = K->TryGetField(TEXT("v"));
			double X, Y, Z;
			if (!V.IsValid() || V->Type == EJson::Null)
			{
				Key.bFromStart = true;
			}
			else if (ReadTriple(V, X, Y, Z))
			{
				Key.V = ConvPos(X, Y, Z);
			}
			Out.Add(Key);
		}
	}

	static void ReadRotKeys(const FJsonArr* Keys, TArray<FRotKey>& Out)
	{
		if (!Keys)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& KV : *Keys)
		{
			const FJsonObj K = KV.IsValid() ? KV->AsObject() : nullptr;
			if (!K.IsValid())
			{
				continue;
			}
			FRotKey Key;
			Key.T = static_cast<float>(Num(K, TEXT("t")));
			Key.Ease = ReadEase(K);
			const TSharedPtr<FJsonValue> V = K->TryGetField(TEXT("v"));
			if (!V.IsValid() || V->Type == EJson::Null)
			{
				Key.bFromStart = true;
			}
			else
			{
				Key.Q = ReadRotValue(V);
			}
			Out.Add(Key);
		}
	}

	static bool ReadHandTarget(const FJsonObj& K, FHandTarget& Out)
	{
		double X, Y, Z;
		if (!ReadTriple(K->TryGetField(TEXT("pos")), X, Y, Z))
		{
			return false;
		}
		Out.Space = ReadSpace(K);
		Out.Pos = ConvPos(X, Y, Z);
		if (ReadTriple(K->TryGetField(TEXT("fingersDir")), X, Y, Z))
		{
			Out.FingersDir = ConvDir(X, Y, Z);
		}
		if (ReadTriple(K->TryGetField(TEXT("palmNormal")), X, Y, Z))
		{
			Out.PalmNormal = ConvDir(X, Y, Z);
		}
		return true;
	}

	static void ReadHandKeys(const FJsonArr* Keys, TArray<FHandKey>& Out)
	{
		if (!Keys)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& KV : *Keys)
		{
			const FJsonObj K = KV.IsValid() ? KV->AsObject() : nullptr;
			if (!K.IsValid())
			{
				continue;
			}
			FHandKey Key;
			Key.T = static_cast<float>(Num(K, TEXT("t")));
			Key.Ease = ReadEase(K);
			bool bHold = false;
			K->TryGetBoolField(TEXT("hold"), bHold);
			Key.bHold = bHold;
			if (!bHold && !ReadHandTarget(K, Key.Target))
			{
				Key.bHold = true; // malformed target: fall back to the grip
			}
			Out.Add(Key);
		}
	}

	static void ReadFingerKeys(const FJsonArr* Keys, TArray<FFingerKey>& Out)
	{
		if (!Keys)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& KV : *Keys)
		{
			const FJsonObj K = KV.IsValid() ? KV->AsObject() : nullptr;
			if (!K.IsValid())
			{
				continue;
			}
			FFingerKey Key;
			Key.T = static_cast<float>(Num(K, TEXT("t")));
			Key.Pose = FName(*Str(K, TEXT("pose")));
			Key.Weight = static_cast<float>(Num(K, TEXT("weight")));
			Key.Ease = ReadEase(K);
			Out.Add(Key);
		}
	}

	static void ReadFingerHold(const FJsonArr* List, TMap<FName, float>& Out)
	{
		if (!List)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const FJsonObj O = V.IsValid() ? V->AsObject() : nullptr;
			if (O.IsValid())
			{
				Out.Add(FName(*Str(O, TEXT("pose"))), static_cast<float>(Num(O, TEXT("weight"))));
			}
		}
	}

	// ------------------------------------------------------------------
	// Loading
	// ------------------------------------------------------------------

	TSharedPtr<FGunAnimSet> FGunAnimSet::Load(FName GunId)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("GunAnims"), GunId.ToString() + TEXT(".json"));
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogHoldover, Warning, TEXT("No animation timelines for %s (%s); using simple procedural motion."), *GunId.ToString(), *Path);
			return nullptr;
		}

		FJsonObj Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogHoldover, Error, TEXT("Could not read %s: %s"), *Path, *Reader->GetErrorMessage());
			return nullptr;
		}

		TSharedPtr<FGunAnimSet> Set = MakeShared<FGunAnimSet>();
		Set->GunId = GunId;

		// Resting grip.
		if (const FJsonObj Hold = Obj(Root, TEXT("hold")))
		{
			const FJsonObj L = Obj(Hold, TEXT("handL"));
			const FJsonObj R = Obj(Hold, TEXT("handR"));
			Set->Hold.bValid = L.IsValid() && R.IsValid()
				&& ReadHandTarget(L, Set->Hold.HandL) && ReadHandTarget(R, Set->Hold.HandR);
			ReadFingerHold(Arr(Hold, TEXT("fingersL")), Set->Hold.FingersL);
			ReadFingerHold(Arr(Hold, TEXT("fingersR")), Set->Hold.FingersR);
		}

		// Mechanical state poses.
		if (const FJsonObj States = Obj(Root, TEXT("states")))
		{
			for (const auto& StatePair : States->Values)
			{
				const FJsonObj StateObj = StatePair.Value.IsValid() ? StatePair.Value->AsObject() : nullptr;
				if (!StateObj.IsValid())
				{
					continue;
				}
				TMap<FName, TPair<FVector, FQuat>>& Bones = Set->States.Add(FName(*StatePair.Key));
				for (const auto& BonePair : StateObj->Values)
				{
					const FJsonObj B = BonePair.Value.IsValid() ? BonePair.Value->AsObject() : nullptr;
					if (!B.IsValid())
					{
						continue;
					}
					FVector Loc = FVector::ZeroVector;
					double X, Y, Z;
					if (ReadTriple(B->TryGetField(TEXT("loc")), X, Y, Z))
					{
						Loc = ConvPos(X, Y, Z);
					}
					const FQuat Rot = ReadRotValue(B->TryGetField(TEXT("rot")));
					Bones.Add(FName(*BonePair.Key), TPair<FVector, FQuat>(Loc, Rot));
				}
			}
		}

		// Actions.
		if (const FJsonObj Actions = Obj(Root, TEXT("actions")))
		{
			for (const auto& ActionPair : Actions->Values)
			{
				const FJsonObj A = ActionPair.Value.IsValid() ? ActionPair.Value->AsObject() : nullptr;
				if (!A.IsValid())
				{
					continue;
				}
				FAction& Act = Set->Actions.Add(FName(*ActionPair.Key));
				Act.Name = FName(*ActionPair.Key);
				Act.Duration = static_cast<float>(Num(A, TEXT("duration")));
				Act.HoldAt = static_cast<float>(Num(A, TEXT("holdAt"), -1.0));

				if (const FJsonObj Bones = Obj(A, TEXT("bones")))
				{
					for (const auto& BonePair : Bones->Values)
					{
						const FJsonObj B = BonePair.Value.IsValid() ? BonePair.Value->AsObject() : nullptr;
						if (!B.IsValid())
						{
							continue;
						}
						FBoneTrack& Track = Act.Bones.Add(FName(*BonePair.Key));
						ReadVecKeys(Arr(B, TEXT("loc")), Track.Loc);
						ReadRotKeys(Arr(B, TEXT("rot")), Track.Rot);
					}
				}

				if (const FJsonObj Gun = Obj(A, TEXT("gun")))
				{
					Act.GunPivot = FName(*Str(Gun, TEXT("pivot")));
					ReadVecKeys(Arr(Gun, TEXT("loc")), Act.GunLoc);
					ReadRotKeys(Arr(Gun, TEXT("rot")), Act.GunRot);
				}

				ReadHandKeys(Arr(A, TEXT("handL")), Act.HandL);
				ReadHandKeys(Arr(A, TEXT("handR")), Act.HandR);
				ReadFingerKeys(Arr(A, TEXT("fingersL")), Act.FingersL);
				ReadFingerKeys(Arr(A, TEXT("fingersR")), Act.FingersR);

				if (const FJsonArr* Attach = Arr(A, TEXT("attach")))
				{
					for (const TSharedPtr<FJsonValue>& V : *Attach)
					{
						const FJsonObj K = V.IsValid() ? V->AsObject() : nullptr;
						if (!K.IsValid())
						{
							continue;
						}
						FAttachKey Key;
						Key.T = static_cast<float>(Num(K, TEXT("t")));
						Key.bLeftHand = Str(K, TEXT("hand")) != TEXT("handR");
						FString Bone;
						if (K->TryGetStringField(TEXT("bone"), Bone))
						{
							Key.Bone = FName(*Bone);
						}
						Act.Attach.Add(Key);
					}
				}

				if (const FJsonArr* Events = Arr(A, TEXT("events")))
				{
					for (const TSharedPtr<FJsonValue>& V : *Events)
					{
						const FJsonObj K = V.IsValid() ? V->AsObject() : nullptr;
						if (K.IsValid())
						{
							FEventKey Key;
							Key.T = static_cast<float>(Num(K, TEXT("t")));
							Key.Name = FName(*Str(K, TEXT("name")));
							Act.Events.Add(Key);
						}
					}
					Act.Events.StableSort([](const FEventKey& X, const FEventKey& Y) { return X.T < Y.T; });
				}
			}
		}

		UE_LOG(LogHoldover, Log, TEXT("Loaded %d gun actions for %s."), Set->Actions.Num(), *GunId.ToString());
		return Set;
	}

	// ------------------------------------------------------------------
	// Sampling
	// ------------------------------------------------------------------

	float FBoneTrack::FirstTime() const
	{
		float T = TNumericLimits<float>::Max();
		if (Loc.Num() > 0) T = FMath::Min(T, Loc[0].T);
		if (Rot.Num() > 0) T = FMath::Min(T, Rot[0].T);
		return T;
	}

	bool FAction::HasEvent(FName EventName, float NearTime, float Tolerance) const
	{
		for (const FEventKey& E : Events)
		{
			if (E.Name == EventName && (NearTime < 0.f || FMath::Abs(E.T - NearTime) <= Tolerance))
			{
				return true;
			}
		}
		return false;
	}

	float ApplyEase(float Alpha, EEase Ease)
	{
		Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
		switch (Ease)
		{
		case EEase::Smooth: return Alpha * Alpha * (3.f - 2.f * Alpha);
		case EEase::Step: return 0.f;
		default: return Alpha;
		}
	}

	template <typename KeyType>
	static bool FindSegment(const TArray<KeyType>& Keys, float T, int32& OutIndex, float& OutAlpha)
	{
		if (Keys.Num() == 0)
		{
			return false;
		}
		if (T <= Keys[0].T)
		{
			OutIndex = 0;
			OutAlpha = 0.f;
			return true;
		}
		for (int32 i = 0; i < Keys.Num() - 1; ++i)
		{
			if (T < Keys[i + 1].T)
			{
				const float Span = FMath::Max(Keys[i + 1].T - Keys[i].T, KINDA_SMALL_NUMBER);
				OutIndex = i;
				OutAlpha = ApplyEase((T - Keys[i].T) / Span, Keys[i].Ease);
				return true;
			}
		}
		OutIndex = Keys.Num() - 1;
		OutAlpha = 0.f;
		return true;
	}

	FVector SampleVec(const TArray<FVecKey>& Keys, float T, const FVector& StartValue)
	{
		int32 i;
		float A;
		if (!FindSegment(Keys, T, i, A))
		{
			return StartValue;
		}
		const FVector V0 = Keys[i].bFromStart ? StartValue : Keys[i].V;
		if (i + 1 >= Keys.Num() || A <= 0.f)
		{
			return V0;
		}
		const FVector V1 = Keys[i + 1].bFromStart ? StartValue : Keys[i + 1].V;
		return FMath::Lerp(V0, V1, A);
	}

	FQuat SampleRot(const TArray<FRotKey>& Keys, float T, const FQuat& StartValue)
	{
		int32 i;
		float A;
		if (!FindSegment(Keys, T, i, A))
		{
			return StartValue;
		}
		const FQuat Q0 = Keys[i].bFromStart ? StartValue : Keys[i].Q;
		if (i + 1 >= Keys.Num() || A <= 0.f)
		{
			return Q0;
		}
		const FQuat Q1 = Keys[i + 1].bFromStart ? StartValue : Keys[i + 1].Q;
		return FQuat::Slerp(Q0, Q1, A).GetNormalized();
	}

	bool SampleFinger(const TArray<FFingerKey>& Keys, FName Pose, float T, float& OutWeight)
	{
		const FFingerKey* Prev = nullptr;
		const FFingerKey* Next = nullptr;
		for (const FFingerKey& K : Keys)
		{
			if (K.Pose != Pose)
			{
				continue;
			}
			if (K.T <= T)
			{
				Prev = &K;
			}
			else
			{
				Next = &K;
				break;
			}
		}
		if (!Prev && !Next)
		{
			return false;
		}
		if (!Prev)
		{
			OutWeight = Next->Weight;
			return true;
		}
		if (!Next)
		{
			OutWeight = Prev->Weight;
			return true;
		}
		const float Span = FMath::Max(Next->T - Prev->T, KINDA_SMALL_NUMBER);
		OutWeight = FMath::Lerp(Prev->Weight, Next->Weight, ApplyEase((T - Prev->T) / Span, Prev->Ease));
		return true;
	}

	FQuat HandBasis(const FVector& FingersDir, const FVector& PalmNormal)
	{
		const FVector X = FingersDir.GetSafeNormal();
		FVector Z = PalmNormal - X * FVector::DotProduct(PalmNormal, X);
		if (!Z.Normalize())
		{
			Z = FVector::UpVector;
		}
		return FRotationMatrix::MakeFromXZ(X, Z).ToQuat();
	}
}
