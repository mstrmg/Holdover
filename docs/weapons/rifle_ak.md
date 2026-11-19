# AK-style rifle (classic wood)

Modeled closely on the AKM (the common AK-47 pattern), Miles's choice, 2026-11-19, with classic reddish-brown varnished wood: buttstock, pistol grip, upper and lower handguards. Dark blued steel with light edge wear. Stamped receiver with ribbed dust cover, rivets and magazine dimples, slant muzzle brake, 30-round curved steel magazine, 7.62x39 mm ammunition. No manufacturer markings.

Rebuilt the same day to real AKM proportions after Miles's review (the first version had an oversized magazine and parts poking through each other). Build scripts: `art\tools\build_ak_A.py`, `build_ak_B.py`, `build_ak_C.py` (run in order, after loading `holdover_lib.py`).

- Blender file: `D:\Claude area\Holdover\art\weapons\rifle_ak\rifle_ak.blend` (first version kept as `rifle_ak_v1_backup.blend`)
- Export: `...\rifle_ak\export\` (`SK_Rifle_AK.fbx`, `SM_Rifle_AK_Mag.fbx`, `SM_Round_762x39.fbx`, `SM_Casing_762x39.fbx`, `T_Rifle_AK_BaseColor/ORM/Normal.png`)
- Size: about 893 mm long including the brake, 415 mm barrel, magazine hangs about 195 mm below the receiver.

## Bones and how each part moves

Millimetres in gun space (X forward, Y left, Z up, bore axis at Z = 0, bolt face at X = 0). Starting values; tune in Unreal.

| Bone | Pivot (mm) | Motion |
|---|---|---|
| `Root` | 0, 0, 0 | Receiver, barrel, gas block, gas tube, sight bases, handguard retainer, cleaning rod, all wood, butt plate, trigger guard. |
| `DustCover` | -90, 0, 16 | Lifts off for takedown (rear end up first). Not needed for normal shooting. |
| `BoltCarrier` | 0, 0, 10 | Slides straight back along -X, about 110 mm full travel. Includes the gas piston and the charging handle (right side). The AK has no bolt hold-open: after the last round it closes on an empty chamber. |
| `Bolt` | -50, 0, 0 (child of `BoltCarrier`) | Rotates about the bore axis about 35 degrees to unlock in the first 8 mm of carrier travel, then rides back with the carrier. Visible through the ejection port. |
| `Trigger` | -118, 0, -30 | Rotates rearward about the left-right axis, about 12 degrees. |
| `Selector` | -178, -16.6, -4 | Long lever on the right side. Rotates about the left-right axis, front end down: safe (up, covers the charging handle slot), full auto (about 12 degrees), semi (about 24 degrees). In safe it blocks the charging handle's travel. |
| `MagRelease` | -84, 0, -40 | Paddle just behind the magazine. Rotates forward about 12 degrees to release the mag. |
| `Magazine` | -12, 0, -20 (front lug) | AK "rock in": the front lug hooks into the receiver at this pivot and the mag rotates back about 20 degrees until the rear lug clicks into the catch. Removal is the reverse, then it drops. |
| `MagFollower` | -45, 0, -23.5 (child of `Magazine`) | Top position when empty; moves down the curved mag about 6 mm per round. |
| `RearSightLeaf` | 88, 0, 31 | Hinged at its front end; tilts up a few degrees for range settings (cosmetic in v1). |
| `RearSightSlider` | 65, 0, 31.7 | Slides along the leaf (cosmetic in v1). |

Socket bones: `Muzzle` (447, 0, 0), `Chamber` (20, 0, 0), `EjectionPort` (-58, -16, -2), `SightRear` (29.7, 0, 37, bottom of the rear notch), `SightFront` (404, 0, 37, top of the front post), `MagWell` (-45, 0, -38), `ChargingHandle` (12, -30, 5), `GripHand` (-188, 0, -84), `SupportHand` (180, 0, -12, lower handguard), `Shoulder` (-444, 0, -112, butt plate). Sight line is 37 mm above the bore; sight radius 374 mm. Casings eject to the right (-Y) and slightly forward.

## Unreal import

Same steps as every gun (see [README.md](README.md)): import `SK_Rifle_AK.fbx` as a skeletal mesh (no animations), the mag, round and casing as static meshes, set the normal map to flip green, and the ORM texture to linear (not sRGB).
