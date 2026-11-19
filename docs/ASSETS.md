# Asset sources and licenses

Every asset in the game, where it came from, and its license.

## Original assets (made for Holdover)

| Asset | Made with | Notes |
|---|---|---|
| M9-style pistol, magazine, 9 mm round and casing | Blender (Claude, Python) | Original model, no third-party meshes or textures; textures baked from procedural Blender materials. |
| AK-style rifle, magazine, 7.62x39 round and casing | Blender (Claude, Python) | Original model, no third-party meshes or textures. |
| FAL-style rifle, magazine, 7.62x51 round and casing | Blender (Claude, Python) | Original model, no third-party meshes or textures. |
| 1911-style pistol, magazine, .45 ACP round and casing | Blender (Claude, Python) | Original model, no third-party meshes or textures. |
| Paper target on wooden stand, steel plate on post, popper, shooting bench, stool, shooting mat, distance marker signs (5, 10, 25, 50 m) | Blender (Claude, Python) | Original low-poly models in `art/props/` (source `range_props.blend`, FBX per prop). Bullseye paper texture and sign lettering are generated in Blender. Wood and steel use the third-party textures below. |

## Third-party assets

| Asset | Source | License | Used on |
|---|---|---|---|
| Wood049 (1K JPG: color, roughness, normal) | ambientCG, https://ambientcg.com/a/Wood049 | CC0 1.0 (public domain) | Posts, feet, backer board, bench, stool, sign posts |
| Metal038 (1K JPG: color, roughness, metalness, normal) | ambientCG, https://ambientcg.com/a/Metal038 | CC0 1.0 (public domain) | Steel plate, popper, hinge and clips |

Downloaded copies are kept in `art/textures/<AssetId>/` on Miles's PC (binary files, added through Git LFS from his PC).
