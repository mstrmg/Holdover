# Forest kit for the first range

The first range is a dense summer hardwood forest (reference: `docs/reference/forest-vibe.png`). This page says what is already built and what Miles adds from Fab inside Unreal, because the cloud session cannot download Fab content.

## Already built in Blender (originals, CC0 textures)

All files are under `art/props/forest/` on Miles's PC, one FBX each. Rocks, logs and stumps include a simpler LOD1 mesh in the same file, so Unreal picks it up automatically at distance.

| File | What it is | Faces (LOD0) |
|---|---|---|
| `SM_Rock_S.fbx`, `SM_Rock_M.fbx`, `SM_Rock_L.fbx` | Mossy rocks, about 0.3 m, 0.7 m and 1.4 m | 320 each |
| `SM_Log_Short.fbx`, `SM_Log_Long.fbx`, `SM_Log_Thick.fbx` | Fallen logs, 1.6 m, 3.2 m, 2.4 m long | about 130 each |
| `SM_Stump_S.fbx`, `SM_Stump_L.fbx` | Stumps with flared roots | about 100 each |
| `SM_Branches.fbx` | Bundle of four fallen sticks | 32 |
| `SM_GroundPatch_4x4.fbx` | 4 m by 4 m bumpy leaf-litter ground tile | 576 |

Known rough spots: log and stump cut ends use the same bark texture as the sides, and the leaf texture is brown (autumn litter). `Ground037` (green moss) is already downloaded if a greener floor is wanted.

## To add from Fab (Miles, inside Unreal)

1. Open Unreal, then Window, then Fab (or the Fab website in a browser), and sign in with the Epic account.
2. Add to the project, one search at a time (all are free for personal use; check the license shown on each page and tell Claude if any is not free):
   - **Megaplants** (Epic, free): pick two or three deciduous or hardwood trees such as beech, maple or oak, plus a fern and a few low undergrowth plants.
   - **Quixel Megascans** (free): a mossy forest-floor surface, a bark surface, and a few scanned rocks and logs for close-up variety.
3. After each add, tell Claude the exact asset names so they are logged in `docs/ASSETS.md`.

## Performance notes (Ryzen 5 3600, 16 GB RAM)

- Keep foliage density modest and use the LOD meshes; CPU and RAM are the bottlenecks.
- Do not add Megaplants at maximum quality everywhere. Use the high-quality ones within about 15 m of the lane and cheaper variants beyond.
