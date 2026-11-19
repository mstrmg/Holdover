# AR-15-style carbine

Modeled on a classic AR-15-style carbine (Miles's queue, 2026-11-19: "ar 15 style carbine"), without any manufacturer markings: flat-top upper with a top rail and a flip-up rear aperture sight, A2-style front sight base, round ribbed carbine handguards, 16 inch barrel with an A2-style flash hider, M4-style collapsible stock on a buffer tube, A2-style pistol grip, 30-round curved aluminum magazine, 5.56x45 mm ammunition. Black anodized aluminum, dark phosphate steel parts, black polymer furniture.

Built to real proportions: about 840 mm long with the stock collapsed, 406 mm barrel, sight height about 66 mm above the bore. Build scripts: `art\tools\build_ar_A.py` then `build_ar_B.py` (after loading `holdover_lib.py`).

- Blender file: `D:\Claude area\Holdover\art\weapons\carbine_ar\carbine_ar.blend`
- Export: `...\carbine_ar\export\` (`SK_Carbine_AR.fbx`, `SM_Carbine_AR_Mag.fbx`, `SM_Round_556.fbx`, `SM_Casing_556.fbx`, `T_Carbine_AR_BaseColor/ORM/Normal.png`, 2K)

## Bones and how each part moves

Millimetres in gun space (X forward, Y left, Z up, bore axis at Z = 0, bolt face at X = 0). Starting values; tune in Unreal.

| Bone | Pivot (mm) | Motion |
|---|---|---|
| `Root` | 0, 0, 0 | Upper and lower receiver, buffer tube, castle nut, barrel and barrel nut, handguards and cap, front sight base, flash hider, pistol grip. |
| `BoltCarrier` | -80, 0, 2 | Slides straight back along -X, about 95 mm. Note: locked back, the top of its rear end shows above the buffer tube (the carrier is a little taller than the tube); a model fix for later. Held back by the bolt catch on an empty magazine. |
| `Bolt` | -6, 0, 0 (child of `BoltCarrier`) | Rotates about 22 degrees around the bore axis to unlock in the first 8 mm of carrier travel, and slides back with the carrier. |
| `ChargingHandle` | -165, 0, 13 | Pull straight back along -X up to about 95 mm (it drags the bolt carrier), then push it forward again; it does not move when firing. |
| `DustCover` | -62, -16.6, -10 (hinge) | Right side. Springs open (rotates about 100 degrees down around the X axis) the first time the bolt moves. |
| `Trigger` | -90, 0, -31 | Rotates rearward about the left-right axis, about 8 degrees. |
| `Selector` | -128, 16, -26 | Left side lever. Rotates about the left-right axis: safe = 0, semi = 90 degrees. |
| `BoltCatch` | -68, 16, -22 | Left side paddle. Rotates up about 8 degrees to hold the bolt open; press the top to release. |
| `MagRelease` | -73, -16, -36 | Right side button. Pushes in about 3 mm (toward +Y) to drop the magazine. |
| `Magazine` | -29, 0, -11 | Drops straight down along -Z, about 170 mm to clear the magazine well. |
| `MagFollower` | -29, 0, -21.7 (child of `Magazine`) | Drops about 4.5 mm per round (follows the magazine's curve in the game). |
| `RearSight` | -140, 0, 31 | Flip-up aperture: rotates about 90 degrees forward to fold flat (cosmetic in v1). |
| `Stock` | -300, 0, 0 | Slides along the buffer tube along X, 4 positions about 33 mm apart (cosmetic in v1). |

Socket bones: `Muzzle` (447, 0, 0), `Chamber` (25, 0, 0), `EjectionPort` (-60, -16, 2), `SightRear` (-140, 0, 66, centre of the aperture), `SightFront` (236, 0, 66, top of the post), `MagWell` (-29, 0, -11), `ChargingHandleKnob` (-168, 20, 13, the latch the hand pulls), `GripHand` (-171, 0, -73), `SupportHand` (150, 0, -10), `Shoulder` (-395, 0, -40). Casings eject to the right (-Y) and slightly rearward.

## Unreal import

Same steps as every gun (see [README.md](README.md)).
