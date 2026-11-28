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
| Woods range ground (`SM_WoodsGround_*`): valley, lane, pad, walkway, paths, forest floor | Unreal (Claude, C++ `Range/HoldoverWoodsGround`) | Original, baked from the range's own layout functions; textures below. |
| Firing-line shelter, rules board | Unreal (Claude, C++, built from engine cubes) | Original; world-projected Wood049 and Ground068 textures. |

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

## Generated effects and sounds (made for Holdover, no third-party material)

| Asset | Made with | Notes |
|---|---|---|
| Muzzle flash atlas (`T_MuzzleFlash`: cores, plumes, end-on bursts, fireballs) and smoke atlas (`T_MuzzleSmoke`) | `tools/fx/gen_fx_textures.py` (Python + numpy, procedural noise) | Written to `art/fx/textures/` on Miles's PC, imported to `/Game/FX` by `tools/unreal/import_fx.py`. 2K. |
| Every gun sound: shots, echo tails, action cycling, brass / live round / magazine landing per surface | `tools/audio/gen_gun_audio.py` (Python + numpy synthesis) | Written to `art/audio/` on Miles's PC, imported to `/Game/Audio` by `tools/unreal/import_fx.py`. |
| `M_MuzzleFlash`, `M_MuzzleSmoke`, `M_Brass` materials | `tools/unreal/import_fx.py` | Built by script; `M_Brass` is assigned to the `SM_Casing_*` meshes. |

## Poly Haven (woods range, downloaded 2026-11-27 with Miles's OK)

All from https://polyhaven.com, license CC0 1.0 (public domain), 2K glTF/JPG, fetched by `tools/range/fetch_polyhaven.py` into `art/polyhaven/`, imported to `/Game/Range/Woods/PolyHaven` and `/Game/Range/Woods/Textures`.

| Assets | Used for |
|---|---|
| Models: boulder_01, rock_07, rock_09, rock_moss_set_01, rock_moss_set_02, stone_01 | Rock outcrops, stones, boulders at the lane edge |
| Models: dead_tree_trunk, dead_tree_trunk_02, tree_stump_01, tree_stump_02, root_cluster_01, root_cluster_02, single_root, dry_branches_medium_01 | Fallen trunks, stumps, root plates, fallen branches and twigs, brush heaps |
| Models: fern_02, moss_01, shrub_01 to shrub_04, nettle_plant, celandine_01, grass_medium_01, grass_medium_02 (with their alpha masks) | Ferns, low plants, moss and grass |
| Models: old_military_crate, Barrel_02, hatchet | Ammo crates at the firing line, rubbish barrel, hatchet in a stump |
| Textures: forest_leaves_02, forest_leaves_04, leaves_forest_ground, forest_ground_05, dirt_floor, leafy_grass, mossy_rock, gravel, moss_wood, bark_brown_02 | Ground layers (leaf litter, worn soil, lane grass, gravel) |

