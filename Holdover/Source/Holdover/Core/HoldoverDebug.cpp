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
		TEXT("1 = take screenshots of the pistol and rifle at ready and aimed (eyes and side views) into Saved/Screenshots/test, then quit. 2 = fire every gun and photograph the muzzle flash kinds, the case leaving the port in slow motion and the smoke (fire_*.png), then quit. 3 = the same for the M9 and AK only. 4 = action test. 5 = sweep every action for arms near the eyes (Saved/Screenshots/sweep). 6 = film strips of the pistol mag and inspect actions (Saved/Screenshots/film)."),
		ECVF_Default);
}

namespace HoldoverDebug
{
	TAutoConsoleVariable<int32> CVarFlashVariant(
		TEXT("holdover.FlashVariant"), -1,
		TEXT("Muzzle flash of the next shots: -1 = random (normal play), 0 = faint, 1 = normal, 2 = with a fireball."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarFlashFreeze(
		TEXT("holdover.FlashFreeze"), 0,
		TEXT("1 = the muzzle flash stays lit as it looks in its first frame, so it can be looked at or photographed."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarSelectGun(
		TEXT("holdover.Gun"), -1,
		TEXT("Switch guns: 0 = M9, 1 = AK, 2 = 1911, 3 = FAL, 4 = AR (test command until the number keys are decided)."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCaseWithGun(
		TEXT("holdover.CaseWithGun"), 0.15f,
		TEXT("Seconds a fresh case is drawn together with the first-person gun and hands before it becomes a normal object (0 = never)."),
		ECVF_Default);
}
