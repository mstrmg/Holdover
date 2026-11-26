#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
class UObject;

/**
 * Small helpers for the gun sounds. Every sound is an imported wave in /Game/Audio (made by
 * tools/audio/gen_gun_audio.py, imported by tools/unreal/import_fx.py); a missing file is simply silent.
 */
namespace HoldoverAudio
{
	/** Picks one sound from a list at random, never the same one twice in a row when there is a choice. */
	HOLDOVER_API USoundBase* PickVariant(const TArray<TObjectPtr<USoundBase>>& Sounds, int32& InOutLast);

	/** Loads /Game/Audio/<Folder>/SW_<Prefix>_01, _02... once and returns them (empty if not imported). */
	HOLDOVER_API const TArray<TObjectPtr<USoundBase>>& LoadSet(const FString& Folder, const FString& Prefix);

	/** Plays a sound for the shooter's own ears (not placed in the world), with a random pitch spread. */
	HOLDOVER_API void PlayAtEars(const UObject* WorldContext, USoundBase* Sound, float Volume, float PitchSpread, USoundConcurrency* Concurrency = nullptr);

	/** Keeps long echo tails from piling up in full auto (the oldest fade out first). */
	HOLDOVER_API USoundConcurrency* TailConcurrency();

	/** Plays a sound at a place in the world, heard falling off with distance (small objects: casings, mags). */
	HOLDOVER_API void PlayAt(const UObject* WorldContext, USoundBase* Sound, const FVector& Location, float Volume, float Pitch);

	/** Surface name used in sound file names for a physical surface ("Hard", "Soft", "Wood", "Metal"). */
	HOLDOVER_API const TCHAR* SurfaceSoundName(EPhysicalSurface Surface);

	/** Distance falloff for small objects (audible to about 15 m). */
	HOLDOVER_API USoundAttenuation* SmallObjectAttenuation();

	/** Limits how many casing sounds play at once (full auto on concrete). */
	HOLDOVER_API USoundConcurrency* CasingConcurrency();
}
