#include "Audio/HoldoverAudio.h"

#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "UObject/UObjectGlobals.h"

namespace HoldoverAudio
{
	USoundBase* PickVariant(const TArray<TObjectPtr<USoundBase>>& Sounds, int32& InOutLast)
	{
		const int32 Num = Sounds.Num();
		if (Num == 0)
		{
			return nullptr;
		}
		int32 Index = FMath::RandRange(0, Num - 1);
		if (Num > 1 && Index == InOutLast)
		{
			Index = (Index + 1 + FMath::RandRange(0, Num - 2)) % Num;
		}
		InOutLast = Index;
		return Sounds[Index];
	}

	const TArray<TObjectPtr<USoundBase>>& LoadSet(const FString& Folder, const FString& Prefix)
	{
		// Loaded once for the whole game; the sounds are kept alive by the root set.
		static TMap<FString, TArray<TObjectPtr<USoundBase>>> Cache;
		const FString Key = Folder + TEXT("|") + Prefix;
		if (const TArray<TObjectPtr<USoundBase>>* Found = Cache.Find(Key))
		{
			return *Found;
		}
		TArray<TObjectPtr<USoundBase>>& Out = Cache.Add(Key);
		for (int32 i = 1; i <= 16; ++i)
		{
			const FString Name = FString::Printf(TEXT("SW_%s_%02d"), *Prefix, i);
			const FString Path = FString::Printf(TEXT("/Game/Audio/%s/%s.%s"), *Folder, *Name, *Name);
			USoundBase* S = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
			if (!S)
			{
				break;
			}
			S->AddToRoot();
			Out.Add(S);
		}
		return Out;
	}

	void PlayAtEars(const UObject* WorldContext, USoundBase* Sound, float Volume, float PitchSpread, USoundConcurrency* Concurrency)
	{
		if (!Sound || !WorldContext || Volume <= 0.f)
		{
			return;
		}
		const float Pitch = FMath::FRandRange(1.f - PitchSpread, 1.f + PitchSpread);
		UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume, Pitch, 0.f, Concurrency);
	}

	void PlayAt(const UObject* WorldContext, USoundBase* Sound, const FVector& Location, float Volume, float Pitch)
	{
		if (!Sound || !WorldContext || Volume <= 0.f)
		{
			return;
		}
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f,
			SmallObjectAttenuation(), CasingConcurrency());
	}

	const TCHAR* SurfaceSoundName(EPhysicalSurface Surface)
	{
		// Names match the surface list in DefaultEngine.ini (SurfaceType1 = Paper ... SurfaceType8 = Brass).
		switch (Surface)
		{
		case SurfaceType1: return TEXT("Soft");   // paper
		case SurfaceType2: return TEXT("Metal");  // steel
		case SurfaceType3: return TEXT("Wood");
		case SurfaceType4: return TEXT("Soft");   // dirt
		case SurfaceType5: return TEXT("Hard");   // gravel
		case SurfaceType6: return TEXT("Soft");   // grass
		case SurfaceType7: return TEXT("Hard");   // concrete
		case SurfaceType8: return TEXT("Metal");  // brass on brass
		default: return TEXT("Hard");             // the gray test range has no surface set
		}
	}

	USoundAttenuation* SmallObjectAttenuation()
	{
		static TObjectPtr<USoundAttenuation> Att;
		if (!Att)
		{
			Att = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("HoldoverSmallObjectAttenuation"));
			Att->AddToRoot();
			FSoundAttenuationSettings& S = Att->Attenuation;
			S.bAttenuate = true;
			S.bSpatialize = true;
			S.AttenuationShape = EAttenuationShape::Sphere;
			S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
			S.AttenuationShapeExtents = FVector(60.f, 0.f, 0.f);
			S.FalloffDistance = 1500.f;
			S.dBAttenuationAtMax = -60.f;
			S.bAttenuateWithLPF = true;
			S.LPFRadiusMin = 300.f;
			S.LPFRadiusMax = 1500.f;
			S.LPFFrequencyAtMin = 20000.f;
			S.LPFFrequencyAtMax = 6000.f;
		}
		return Att;
	}

	USoundConcurrency* TailConcurrency()
	{
		static TObjectPtr<USoundConcurrency> Conc;
		if (!Conc)
		{
			Conc = NewObject<USoundConcurrency>(GetTransientPackage(), TEXT("HoldoverTailConcurrency"));
			Conc->AddToRoot();
			Conc->Concurrency.MaxCount = 6;
			Conc->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
		}
		return Conc;
	}

	USoundConcurrency* CasingConcurrency()
	{
		static TObjectPtr<USoundConcurrency> Conc;
		if (!Conc)
		{
			Conc = NewObject<USoundConcurrency>(GetTransientPackage(), TEXT("HoldoverCasingConcurrency"));
			Conc->AddToRoot();
			Conc->Concurrency.MaxCount = 12;
			Conc->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopQuietest;
		}
		return Conc;
	}
}
