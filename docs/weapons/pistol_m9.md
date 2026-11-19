# M9-style pistol

Modeled on the Beretta M9 (Miles's choice, 2026-11-19), without any Beretta markings. Double/single action with an exposed hammer, slide-mounted safety/decocker, open-top slide, 15-round double-stack magazine, 9x19 mm ammunition. Walnut wood grips (Miles's request).

- Blender file: `D:\Claude area\Holdover\art\weapons\pistol_m9\pistol_m9.blend`
- Export: `...\pistol_m9\export\` (`SK_Pistol_M9.fbx`, `SM_Pistol_M9_Mag.fbx`, `SM_Round_9mm.fbx`, `SM_Casing_9mm.fbx`, `T_Pistol_M9_BaseColor/ORM/Normal.png`)
- Size: 217 mm long, about 140 mm tall. About 11,700 triangles. One 4K texture set.

## Bones and how each part moves

Positions in millimetres in gun space (X forward, Y left, Z up, bore axis at Z = 0, rear of slide at X = 0). Numbers are starting points for the gun data asset; tune them in Unreal.

| Bone | Pivot (mm) | Motion |
|---|---|---|
| `Root` | 0, 0, 0 | Frame, grip panels, recoil guide rod. Everything else hangs off it. |
| `Slide` | 0, 0, 0 | Slides straight back along -X. Full travel about 60 mm; held open by the slide stop at about 58 mm. |
| `Barrel` | 3, 0, 0 | Rides back with the slide for the first 9 mm, then drops about 1.5 mm (falling locking block) and stops. |
| `Hammer` | 2, 0, -22 | Rotates about the left-right axis. Down = 0, cocked = about 60 degrees rearward. |
| `Trigger` | 75, 0, -25 | Rotates rearward about the left-right axis: about 15 degrees for a double-action pull, about 5 degrees single action. |
| `Safety` | 18, 0, 5 | Child of `Slide`. Lever rotates about 60 degrees down for safe/decock, back up for fire. |
| `SlideStop` | 50, 13.5, -19 | Left side. Rotates up about 8 degrees to catch the slide on an empty mag. |
| `Takedown` | 102, 13, -22 | Left side. Rotates 90 degrees for field stripping (not used in v1). |
| `MagRelease` | 54, 14, -49 | Left side. Pushes in 2.5 mm (toward -Y) when pressed. |
| `Magazine` | 42, 0, -18 | Slides out along its own axis, which is raked 15 degrees (down and back). About 125 mm to clear the grip. |
| `MagFollower` | child of `Magazine` | Sits at the top when the mag is empty; drops about 4.6 mm per round along the mag axis (about 69 mm when full). |

Socket bones: `Muzzle` (209, 0, 0) and `Chamber` (8, 0, 0) on the barrel; `EjectionPort` (75, -13, 5), `SightRear` (9, 0, 15.5) and `SightFront` (201, 0, 15.5) on the slide; `MagWell` (42, 0, -18) and `GripHand` (18, 0, -80) on the root. Rounds eject to the right (-Y).
