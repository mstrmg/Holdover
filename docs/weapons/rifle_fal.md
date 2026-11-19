# FAL-style battle rifle (metric pattern)

Modeled on the classic metric-pattern FAL (Miles's queue, 2026-11-19): gray parkerized steel with black synthetic furniture (stock, pistol grip, handguard), which is how I read "lighter gray accents like the classic one". Break-open receiver with a separate upper and lower, left-side folding charging handle, rear aperture sight with protective ears on the top cover, gas block with adjustable gas plug and front sight with ears, long slotted flash hider, 20-round straight steel magazine, 7.62x51 mm ammunition. No manufacturer markings.

Built to real proportions: about 1100 mm long, 533 mm barrel, sight radius about 560 mm, magazine hangs about 155 mm below the receiver. Build scripts: `art\tools\build_fal_A.py` then `build_fal_B.py` (after loading `holdover_lib.py`).

- Blender file: `D:\Claude area\Holdover\art\weapons\rifle_fal\rifle_fal.blend`
- Export: `...\rifle_fal\export\` (`SK_Rifle_FAL.fbx`, `SM_Rifle_FAL_Mag.fbx`, `SM_Round_762x51.fbx`, `SM_Casing_762x51.fbx`, `T_Rifle_FAL_BaseColor/ORM/Normal.png`, 2K)

## Bones and how each part moves

Millimetres in gun space (X forward, Y left, Z up, bore axis at Z = 0, bolt face at X = 0). Starting values; tune in Unreal.

| Bone | Pivot (mm) | Motion |
|---|---|---|
| `Root` | 0, 0, 0 | Upper and lower receiver, barrel, handguard and caps, gas block and front sight, flash hider, stock, butt plate, grip, sling swivel. |
| `TopCover` | -80, 0, 20 | Slides back off the receiver for takedown (not needed for shooting). |
| `BoltCarrier` | 0, 0, 2 | Slides straight back along -X, about 125 mm. Stays back on an empty magazine (bolt hold-open). |
| `Bolt` | -40, 0, 0 (child of `BoltCarrier`) | Tilts: rear drops about 3 mm to lock, lifts to unlock in the first 10 mm of carrier travel. |
| `ChargingHandle` | 11, 20, 2 | Left side, does not move when firing. Pull back along -X up to 125 mm to work the action, then it springs forward. |
| `Trigger` | -121, 0, -44 | Rotates rearward about the left-right axis, about 10 degrees. |
| `Selector` | -170, 16.5, -38 | Left side lever above the grip. Rotates about the left-right axis: safe (pointing back, 0), semi (about 90 degrees down). |
| `BoltCatch` | -35, 16, -32 | Left side lever. Moves up when the gun locks open; push down to release. |
| `MagRelease` | -86, 0, -48 | Paddle behind the magazine. Rotates forward about 12 degrees to release. |
| `Magazine` | -2, 0, -20 (front lug) | Rocks in: front lug hooks in, rear swings up about 15 degrees until it clicks. |
| `MagFollower` | -40, 0, -25 (child of `Magazine`) | Moves down about 6 mm per round. |
| `RearSightAperture` | -180, 0, 28 | Slides along the sight base for range (cosmetic in v1). |
| `GasRegulator` | 398, 0, 20 | Rotates about the bore axis to change gas setting (cosmetic in v1). |

Socket bones: `Muzzle` (590, 0, 0), `Chamber` (25, 0, 0), `EjectionPort` (-58, -16, 4), `SightRear` (-180, 0, 47, centre of the aperture), `SightFront` (379, 0, 47, top of the post), `MagWell` (-40, 0, -22), `ChargingHandle` (14, 30, 2), `GripHand` (-195, 0, -90), `SupportHand` (190, 0, -10), `Shoulder` (-508, 0, -80). Casings eject to the right (-Y) and slightly forward.

## Unreal import

Same steps as every gun (see [README.md](README.md)).
