# 1911-style pistol

Modeled on the classic full-size 1911 Government model with a 5 inch barrel (Miles's queue, 2026-11-19: "wood grip 1911"), without any manufacturer markings. Single action with an exposed hammer, frame-mounted thumb safety, grip safety, slide stop on the left, barrel bushing and recoil spring plug at the muzzle, 7-round single-stack magazine, .45 ACP ammunition. Blued steel with checkered walnut double-diamond grips.

Built to real proportions: about 216 mm long and 140 mm tall, 127 mm barrel, grip raked 18 degrees. Build script: `art\tools\build_1911_v2.py` (after loading `holdover_lib.py`).

- Blender file: `D:\Claude area\Holdover\art\weapons\pistol_1911\pistol_1911.blend`
- Export: `...\pistol_1911\export\` (`SK_Pistol_1911.fbx`, `SM_Pistol_1911_Mag.fbx`, `SM_Round_45ACP.fbx`, `SM_Casing_45ACP.fbx`, `T_Pistol_1911_BaseColor/ORM/Normal.png`, 2K)

## Bones and how each part moves

Millimetres in gun space (X forward, Y left, Z up, bore axis at Z = 0, rear of the slide at X = 0, breech face at X = 79). Starting values; tune in Unreal.

| Bone | Pivot (mm) | Motion |
|---|---|---|
| `Root` | 0, 0, 0 | Frame, grip panels and grip screws. |
| `Slide` | 0, 0, 0 | Slides straight back along -X, about 62 mm. Locks back on the slide stop after the last round. The barrel bushing and recoil spring plug ride on this bone. |
| `Barrel` | 92, 0, -17 (link pin) | Rides back with the slide for about 3 mm, then the rear swings down about 2 mm on the link to unlock. |
| `Hammer` | 5, 0, -21 | Rotates about the left-right axis: down = 0, cocked = about 55 degrees rearward. |
| `Trigger` | 88, 0, -33 | Slides straight back along -X, about 3 mm (a 1911 trigger slides, it does not pivot). |
| `GripSafety` | 2, 0, -18 | Rotates forward about 6 degrees when the hand squeezes the grip. Gun only fires while it is pressed. |
| `ThumbSafety` | 12, 12, -20.3 | Left side lever. Up = safe, rotate about 20 degrees down = fire. Only goes on safe with the hammer cocked. |
| `SlideStop` | 92, 12, -17 | Left side lever. Rotates up about 6 degrees to hold the slide open on an empty magazine; push down to release. |
| `MagRelease` | 66, 12, -31 | Left side button. Pushes in about 2.5 mm (toward -Y) to drop the magazine. |
| `Magazine` | 59, 0, -18 | Slides out along its own axis, raked 18 degrees (down and back). About 120 mm to clear the grip. |
| `MagFollower` | 57, 0, -25 (child of `Magazine`) | Drops about 4.5 mm per round along the magazine axis. |

Socket bones: `Muzzle` (207, 0, 0), `Chamber` (90, 0, 0), `EjectionPort` (98, -12, 5), `SightRear` (18.5, 0, 15.5, bottom of the notch), `SightFront` (187, 0, 15.8, top of the blade), `MagWell` (59, 0, -18), `GripHand` (45, 0, -56). Casings eject to the right (-Y) and up.

## Unreal import

Same steps as every gun (see [README.md](README.md)).
