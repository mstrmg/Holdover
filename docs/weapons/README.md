# Weapon models

All gun models are original Blender builds made by Claude with Python scripts, using real guns only as a guide for size, proportions and how the parts work. No manufacturer logos or markings.

## Where the files are (Miles's PC)

Each gun has its own folder under `D:\Claude area\Holdover\art\weapons\<gun>\`:

| Folder | What's in it |
|---|---|
| `<gun>.blend` | Working Blender file. Collection `<Gun>` holds the parts, `EXPORT_<Gun>` holds the game-ready copies. |
| `textures\` | Baked 4K textures (plus the separate AO, roughness and metal maps they were packed from). |
| `export\` | Everything Unreal needs: `SK_<Gun>.fbx`, `SM_<Gun>_Mag.fbx`, ammo meshes, and the three textures. |
| `renders\` | Preview pictures. |

## Conventions

- Built in millimetres, gun pointing along Blender +X, up is +Z, bore axis at Z = 0.
- Exported with forward X, up Z and "FBX scale all", so 1 Blender metre = 100 Unreal units. The muzzle should point along Unreal +X; if it imports turned, set Import Rotation on the FBX import dialog.
- One skeletal mesh per gun. Every moving part is rigidly skinned (100%) to its own bone, so code can move parts by moving bones (Control Rig or AnimInstance, in component space).
- Extra bones with no geometry act as sockets: `Muzzle`, `Chamber`, `EjectionPort`, `SightRear`, `SightFront`, `MagWell`, `GripHand`. Use them as socket names in code.
- The magazine is in the skeletal mesh (bone `Magazine`) and is also exported on its own as `SM_<Gun>_Mag` for dropped or held mags. Its origin is the top centre of the mag, with the mag axis along its local Z.
- One material per gun with three textures:
  - `T_<Gun>_BaseColor` (sRGB)
  - `T_<Gun>_ORM` (not sRGB): R = ambient occlusion, G = roughness, B = metallic
  - `T_<Gun>_Normal` (normal map, OpenGL / Blender style: flip green in Unreal)
- Ammo meshes (`SM_Round_*`, `SM_Casing_*`) use plain brass and copper materials, no textures. Origin at the case head, pointing +X.

## Importing a gun into Unreal (Miles)

1. In the Content Browser, make a folder `Content/Weapons/<Gun>`.
2. Drag in everything from the gun's `export\` folder.
3. For `SK_<Gun>.fbx`: choose Skeletal Mesh, turn on "Import Normals and Tangents" (Normals: Import Normals and Tangents), leave "Create Physics Asset" off, click Import.
4. For the `SM_*.fbx` files: Static Mesh, same normals setting, click Import All.
5. Open `T_<Gun>_ORM` and untick sRGB. Open `T_<Gun>_Normal`, set Compression to Normalmap and tick Flip Green Channel.
6. Open the material Unreal made for the gun and wire: BaseColor texture to Base Color; ORM red to Ambient Occlusion, green to Roughness, blue to Metallic; Normal texture to Normal. (Claude will replace this with a shared C++-friendly master material later.)

## Guns

| Gun | Doc | Status |
|---|---|---|
| M9-style pistol | [pistol_m9.md](pistol_m9.md) | Done: modeled, textured, exported |
