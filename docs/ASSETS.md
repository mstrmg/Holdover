# Asset sources and licenses

Every asset in the game, where it came from, and its license.

## Original assets (made for Holdover)

| Asset | Made with | Notes |
|---|---|---|
| M9-style pistol, magazine, 9 mm round and casing | Blender (Claude, Python) | Original model, no third-party meshes or textures; textures baked from procedural Blender materials. |
| AK-style rifle, magazine, 7.62x39 round and casing | Blender (Claude, Python) | Original model, no third-party meshes or textures. |
| FAL-style rifle, magazine, 7.62x51 round and casing | Blender (Claude, Python) | Original model, no third-party meshes or textures. |
| 1911-style pistol, magazine, .45 ACP round and casing | Blender (Claude, Python) | Original model, no third-party meshes or textures. |
| AR-15-style carbine, magazine, 5.56x45 round and casing | Blender (Claude, Python) | Original model, no third-party meshes or textures. |
| Paper target on wooden stand, steel plate on post, popper, shooting bench, stool, shooting mat, distance marker signs (5, 10, 25, 50 m) | Blender (Claude, Python) | Original low-poly models in `art/props/` (source `range_props.blend`, FBX per prop). Bullseye paper texture and sign lettering are generated in Blender. Wood and steel use the third-party textures below. |
| Plate rack, no-shoot target on stand, barricade, sandbag rest (lumpy, redone), ammo can | Blender (Claude, Python) | Original low-poly models in `art/props/`, same source file. The no-shoot paper (red border and X) is generated in Blender. Sandbags and the ammo can use plain procedural colours; the rest use the textures below. |
| Target frame, cardboard silhouette on stake, lane divider, spent-brass bin, steel gong on frame, bench seat, "FIRING LINE" and "CEASE FIRE" signs | Blender (Claude, Python) | Original low-poly models in `art/props/`, same source file. Sign lettering uses Blender's built-in font converted to mesh. |
| Forest range ground (`SM_ForestTerrain`): berms, pad, walkway, paths, rolling forest floor | Blender (Claude, Python, `tools/blender/build_forest_terrain.py`) | Original, generated from the layout numbers; textures below. |
| Forest kit: rocks (S, M, L), logs (short, long, thick), stumps (S, L), fallen branches, 4x4 m ground patch | Blender (Claude, Python) | Original models in `art/props/forest/`, same source file. Rocks, logs and stumps include a LOD1 mesh in the same FBX. Shapes are generated from noise in Blender; textures are the ambientCG ones below. |

## Third-party assets

| Asset | Source | License | Used on |
|---|---|---|---|
| Wood049 (1K JPG: color, roughness, normal) | ambientCG, https://ambientcg.com/a/Wood049 | CC0 1.0 (public domain) | Posts, feet, backer board, bench, stool, sign posts, barricade, plate rack, frames, dividers |
| Metal038 (1K JPG: color, roughness, metalness, normal) | ambientCG, https://ambientcg.com/a/Metal038 | CC0 1.0 (public domain) | Steel plates, popper, hinge and clips, plate rack hangers, ammo can latch, gong |
| ScatteredLeaves009 (1K JPG: color, roughness, normal) | ambientCG, https://ambientcg.com/a/ScatteredLeaves009 | CC0 1.0 (public domain) | Forest ground patch, forest range floor |
| Rock063 (1K JPG: color, roughness, normal) | ambientCG, https://ambientcg.com/a/Rock063 | CC0 1.0 (public domain) | Forest rocks |
| Bark014 (1K JPG: color, roughness, normal) | ambientCG, https://ambientcg.com/a/Bark014 | CC0 1.0 (public domain) | Forest logs, stumps and branches |
| Cardboard002 (1K JPG: color, roughness, normal) | ambientCG, https://ambientcg.com/a/Cardboard002 | CC0 1.0 (public domain) | Cardboard silhouette |
| Gravel042 (1K JPG: color, roughness, normal) | ambientCG, https://ambientcg.com/a/Gravel042 | CC0 1.0 (public domain) | Forest range firing pad (until Megascans) |
| Ground037 (1K JPG: color, roughness, normal) | ambientCG, https://ambientcg.com/a/Ground037 | CC0 1.0 (public domain) | Forest range lane and berm grass (until Megascans) |
| Ground068 (1K JPG: color, roughness, normal) | ambientCG, https://ambientcg.com/a/Ground068 | CC0 1.0 (public domain) | Forest range dirt paths (until Megascans) |
| Ground076 (1K JPG: color, roughness, normal) | ambientCG, https://ambientcg.com/a/Ground076 | CC0 1.0 (public domain) | Forest range backstop earth (until Megascans) |
| Megaplants: European Hornbeam, European Beech, English Oak (trees), Elder, European Spindle (young trees and shrubs), Bracken Fern | Fab, Quixel Megaplants, added in the editor 2026-11-20 (in `/Game/Megaplant_Library/`, untracked Content on Miles's PC) | Fab Standard License (free; accepted by Miles in the Fab window) | Forest range trees, understory and ferns |
| Wild Grass (medium quality) | Fab, Quixel Megascans, added 2026-11-20 (in `/Game/Fab/Megascans/`) | Fab Standard License (free) | Forest range tall grass along the lane and on the berms |

Other ambientCG textures were downloaded to compare and are not used in any model (for example Planks040, Metal009, Wood094, Ground037). They sit in `art/textures/` and are also CC0.

Downloaded copies are kept in `art/textures/<AssetId>/` on Miles's PC (binary files, added through Git LFS from his PC).
