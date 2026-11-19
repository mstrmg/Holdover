# Forest range (the first real map)

A shooting lane cut through a dense summer hardwood forest, like `docs/reference/forest-vibe.png`: tall dark trunks, layers of young trees and ferns, sun breaking through the leaves in patches, a little haze in the air. Miles's bar (2026-11-19): "quite realistic", and it has to look good and detailed the first time he sees it, with no two trees alike.

Plan picture (top view, 1920x1080): `maps/forest_range_plan.png` in the project files. Redraw it any time with `python tools/range/plan_forest_range.py out.png`.

## Layout

The shooter stands at the firing line facing downrange. All numbers live in `tools/range/forest_layout.py` and in `Holdover/Source/Holdover/Range/HoldoverForestRange.cpp`; keep the two in sync.

| Part | Where and how big |
|---|---|
| Gravel firing pad | 8 m deep (7 m behind the firing line to 1 m in front), 26 m wide |
| Firing line | A timber beam lying across the front of the pad |
| Shooting benches | Three: yours just to your right (ammo can on it), two more at the empty bays (stools, a sandbag rest on the left one) |
| Spent-brass bin | Behind your bench |
| Lane | Mown grass, 26 m wide, flat, from the pad to the backstop at 56 m |
| Paper targets | 5, 7, 10, 15, 25 and 50 m, spread 1.6 m apart sideways (same as the gray range) |
| Distance signs | 5, 10, 25 and 50 m, on posts along the left edge, facing you |
| Walkway | Trodden dirt strip down the left side of the lane, to walk out and look at your holes |
| Side berms | Earth banks 2.5 m high along both sides, grassed, with a 1 m flat top; the forest grows on their back slopes |
| Backstop berm | 5 m high, about 45 degree face of bare earth starting at 56 m, flat top, back slope into the trees |
| Behind the pad | A small clearing with a rest bench; the CEASE FIRE sign stands where the path arrives |
| Access path | A dirt path winding off into the woods behind-left, toward an imagined parking area |
| Forest | Everywhere else, about 210 m by 180 m of it, so the trees go on as far as you can see |
| Signs | FIRING LINE at the left end of the pad, CEASE FIRE at the path |

Steel plates, the plate rack, poppers and the barricade are not placed yet: they would look shootable but not react. They arrive with the targets milestone (M4), at 10 to 25 m.

## The forest

- Big trees about 6 m apart (roughly 300 a hectare, a mature hardwood stand), 15 to 24 m tall, a few old giants taller and thicker. Bare trunks low down, leafy crowns above, so you look through a hall of trunks as in the reference.
- Every tree is different: its own height, thickness, lean (up to 4.5 degrees), turn and size, picked from several tree models.
- Young trees under the canopy, and twice as many along the sunny edge of the clearing.
- Ferns and bushes in drifts, thick along the edge of the clearing, thinner deep in the forest.
- Taller grass at the foot of the berms and the edges of the pad, where a mower wouldn't reach.
- Mossy rocks, fallen logs, stumps and fallen branches scattered on the forest floor.
- The forest floor rolls gently (up to about 0.7 m); the range itself is level.
- The same forest grows every time (fixed seed), so it never changes between sessions.

## Light and air

- Summer mid-afternoon sun, 52 degrees high, from behind and to the left, so the targets are lit and you never aim into the sun.
- Real sunlight strength (75,000 lux, slightly warm white), real sky, sky light.
- Haze that thickens near the ground and shows rays where the sun breaks through the leaves.
- Your eyes adjust between the sunny lane and the shade under the trees: quicker to bright light than to darkness, like real eyes.
- No vignette or camera effects, only a soft bloom on bright light.

## How it is built

There is no level file to open. When you press Play in the empty map, the game builds the forest range, the same way it built the gray range. The range picks up real art automatically when it is in the project, and builds a stand-in from simple shapes when it is not:

| Piece | Uses, when present | Otherwise |
|---|---|---|
| Ground | `/Game/Range/Forest/Ground/SM_ForestTerrain` (Blender terrain) | A ground mesh shaped in code (berms, paths, rolling floor) |
| Ground look | Materials `MI_ForestFloor`, `MI_LaneGrass`, `MI_Gravel`, `MI_DirtPath`, `MI_BermEarth` in `/Game/Range/Forest/Ground/` (made from Megascans surfaces) | Plain colours |
| Big trees | Every mesh in `/Game/Range/Forest/Trees/` (Megaplants) | Trees built from shapes, all different, but clearly blocky |
| Young trees | Every mesh in `/Game/Range/Forest/Saplings/` | Small shape trees |
| Ferns, bushes | Every mesh in `/Game/Range/Forest/Plants/` | Clusters of green shapes |
| Grass clumps | Every mesh in `/Game/Range/Forest/Grass/` | Small green shapes |
| Rocks, logs, stumps, branches | The Blender forest kit in `/Game/Range/Forest/Kit/` | Shapes |
| Benches, stools, signs, ammo can, brass bin | The Blender range props in `/Game/Range/Props/` | Shapes, with real lettering on the signs |

The stand-ins only keep the game playable. The look Miles asked for needs the real plants, so the first time Miles judges the forest should be after steps 2 to 4 below are done.

To get the plain gray range back for one session, type in the console: `open /Engine/Maps/Entry?Range=Gray` (or set `bForestRange=False` in `Config/DefaultGame.ini`).

## Steps on Miles's PC

1. **Build and look once.** Pull the branch, build, press Play. You stand on the gravel pad. This checks the code, not the look.
2. **Get the plants from Fab (Miles, about 10 minutes, Claude can guide or do the clicking).** In Unreal: Window, then Fab. Add to the project, all free:
   - Megaplants: two or three summer deciduous trees (beech, maple, oak, birch), one or two small or young trees, two or three ferns, two or three bushes or undergrowth plants, and a grass clump. Pick the green summer versions.
   - Megascans surfaces: a forest floor with leaves and moss, a short grass, a gravel, a dirt path, a bare earth.
   Tell Claude when they are in. Claude then moves each one into the right folder (`Trees`, `Saplings`, `Plants`, `Grass`, `Ground`) and makes the five ground materials, and logs them in `docs/ASSETS.md`.
3. **Import the Blender props and forest kit (Claude, through Remote Control).** Claude runs `tools/unreal/import_range.py`, which imports `art/props/` and `art/props/forest/`, makes the plant folders, and lists the names. Claude matches any names the game looks for.
4. **Blender pass (Claude, using Blender on the PC).** Claude sculpts the ground in Blender from the same layout numbers (`tools/range/forest_layout.py`): softer, more natural berms with erosion and footprints in the walkway, roots and dips on the forest floor, then exports `art/range/forest/SM_ForestTerrain.fbx` and imports it. A real mesh also gives Lumen better light under the trees than the code-shaped ground. Claude then renders a few 1920x1080 checks from your eye height (standing at the bench, at 25 m, at the berm) and fixes what looks off before Miles plays.
5. **Play and say what to change.**

Why trees come from Fab rather than Blender: Megaplants are free, scanned from real plants, move in the wind, and are built for Unreal's Nanite, so they are both better looking and lighter than trees Claude could model in Blender. Blender is used where it is strongest here: shaping the ground and checking the layout. If Megaplants ever don't work, Blender's free tree generator is the backup.

## Tuning (no code needed)

Named settings on the forest range; ask Claude to change any of them: `TreeDensity`, `SaplingDensity`, `UndergrowthDensity`, `DebrisDensity` (1 = designed look), `SmallThingsDrawDistance`, `Seed` (a different forest), `SunLux`, `ForestSunElevation`, `ForestSunAzimuth`, `SunTemperature`, `HazeDensity`, `EyeAdaptMinEV`, `EyeAdaptMaxEV`, `EyeAdaptSpeedUp`, `EyeAdaptSpeedDown`, and the layout numbers (`LaneHalfWidth`, `BackBermToe`, berm heights...). If imported props face the wrong way, `PropYawOffset` turns them all.

## Performance (RX 9070 XT, Ryzen 5 3600, 16 GB)

- Everything is instanced and static, which keeps the CPU cost low and lets shadows be cached.
- Small plants, rocks and branches disappear beyond 70 m; trees never pop.
- With Megaplants, start with `TreeDensity` 1 and drop it to 0.8 if the frame rate is under 60 at 1080p. The densities are multipliers, so the layout stays the same.
- The CLAUDE.md note stands: 32 GB of RAM is recommended before the big forest work, mainly for the editor while importing Megaplants.

## Not yet

- Wind in the trees (comes with Megaplants' own wind once they are in).
- Forest sounds and reverb (M5 audio).
- Steel, poppers, barricade and drills (M4).
- Bird and insect life, falling leaves (later polish, if wanted).
