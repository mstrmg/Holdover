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

/** Test helpers for checking the hands and the eye view without a mouse (also handy in automated runs). */
namespace HoldoverDebug
{
	extern TAutoConsoleVariable<int32> CVarForceAim;  // holdover.ForceAim 1: aim as if the right mouse button were held
	extern TAutoConsoleVariable<int32> CVarDebugView; // holdover.DebugView 1-3: look at yourself from the side, front or behind
	extern TAutoConsoleVariable<int32> CVarTestShots; // holdover.TestShots 1: take a set of screenshots (pistol and rifle, ready and aimed), then quit

	inline bool ForceAim() { return CVarForceAim.GetValueOnGameThread() != 0; }
	inline int32 DebugView() { return CVarDebugView.GetValueOnGameThread(); }
	inline int32 TestShots() { return CVarTestShots.GetValueOnGameThread(); }
}
