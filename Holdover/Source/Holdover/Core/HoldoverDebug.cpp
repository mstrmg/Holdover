#include "Core/HoldoverDebug.h"

namespace HoldoverDebug
{
	TAutoConsoleVariable<int32> CVarDebugGun(
		TEXT("holdover.DebugGun"), 0,
		TEXT("1 = show the gun's chamber, magazine and slide state on screen."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarDebugBullets(
		TEXT("holdover.DebugBullets"), 0,
		TEXT("1 = draw every bullet's flight path for a few seconds."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarDebugShots(
		TEXT("holdover.DebugShots"), 0,
		TEXT("1 = print the ring and distance from centre of each paper hit."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarDebugHands(
		TEXT("holdover.DebugHands"), 0,
		TEXT("1 = draw the hand IK targets and arm chains."),
		ECVF_Default);
}
