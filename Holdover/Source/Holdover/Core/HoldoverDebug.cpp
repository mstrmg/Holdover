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

namespace HoldoverDebug
{
	TAutoConsoleVariable<int32> CVarForceAim(
		TEXT("holdover.ForceAim"), 0,
		TEXT("1 = aim as if the aim button were held (for testing)."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarDebugView(
		TEXT("holdover.DebugView"), 0,
		TEXT("0 = normal eyes. 1 = side view, 2 = front view, 3 = from behind: see where the arms and gun really are."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarTestShots(
		TEXT("holdover.TestShots"), 0,
		TEXT("1 = take screenshots of the pistol and rifle at ready and aimed (eyes and side views) into Saved/Screenshots/test, then quit."),
		ECVF_Default);
}
