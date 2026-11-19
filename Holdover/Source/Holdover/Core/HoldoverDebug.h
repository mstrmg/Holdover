#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

/** Debug switches, all off by default. Type them in the game console (` key). */
namespace HoldoverDebug
{
	extern TAutoConsoleVariable<int32> CVarDebugGun;     // holdover.DebugGun 1: gun state text on screen
	extern TAutoConsoleVariable<int32> CVarDebugBullets; // holdover.DebugBullets 1: draw bullet paths
	extern TAutoConsoleVariable<int32> CVarDebugShots;   // holdover.DebugShots 1: print the ring of each hit
	extern TAutoConsoleVariable<int32> CVarDebugHands;   // holdover.DebugHands 1: draw hand targets

	inline bool Gun() { return CVarDebugGun.GetValueOnGameThread() != 0; }
	inline bool Bullets() { return CVarDebugBullets.GetValueOnGameThread() != 0; }
	inline bool Shots() { return CVarDebugShots.GetValueOnGameThread() != 0; }
	inline bool Hands() { return CVarDebugHands.GetValueOnGameThread() != 0; }
}
