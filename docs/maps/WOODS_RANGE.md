# Woods range (the forest map, rebuilt 2026-11-27)

Replaces the first forest range, which Miles said looked bad ("an open park"). The target is his reference photo
(project file `reference/forest-vibe.png`): a dim, cool eastern hardwood forest ("New Jersey, not Alabama") with tall
dark trunks, high crowns that close overhead, a thick layer of saplings and young trees whose leaves glow where the sun
comes through, ferns and low plants over the floor, and damp haze in the depth.

## Layout (shooter at the origin facing downrange)

| Part | Where and how |
|---|---|
| Firing line | A timber shelter with a mossy board roof over the gravel pad, posts at the ends, three benches (yours to your right), stools, sandbag rest, ammo can, brass bin, two old ammo crates, a rubbish barrel, a range-rules board on the back-left post |
| Lane | About 17 m wide, ragged edges that swing in and out by several metres on both sides; mown grass with drifts of uncut grass, weeds and leaf litter; trees, saplings and ferns grow into it anywhere except a line of fire |
| Targets | 5, 7, 10, 15, 25, 50, 75, 100 and 150 m, 1.6 m apart sideways, each with a trodden trail from the walkway |
| Walkway | A worn path down the left side to the targets |
| Landform | The range lies in a valley: the ground climbs 8 m to the backstop and a wooded hill rises 9 m behind it; the forest floor climbs away on both sides with hills and hollows; low mounds in the lane stay off the lines of fire |
| Backstop | 5 m earth bank at about 190 m, eroded, fading into the hillside, plants on its back |
| Behind the pad | Small clearing with a rest bench, a hatchet left in a stump, the CEASE FIRE sign where the footpath arrives |
| Forest | About 380 m by 290 m: ~2,300 tall trees (19-31 m, oaks, beeches, hornbeams), ~2,300 young trees, ~20,000 leafy saplings, ~31,000 fern clumps, ~34,000 low plants and seedlings, logs, stumps, root plates, fallen branches, twigs and mossy rock outcrops. Every plant has its own size, turn and lean; trees at the edge lean out over the light of the lane |

## How it is built

`Range/HoldoverWoodsRange` builds everything when you press Play in the empty map (the same seed grows the same forest).

- **Ground**: 48 Nanite tiles plus the gravel pad (`/Game/Range/Woods/Ground`), baked in the editor from the class's own
  height and surface functions by `tools/unreal/build_woods_ground.py` (C++: `Range/HoldoverWoodsGround`). Their vertex
  colours paint the surfaces for `MI_WoodsGround` (leaf litter base; grass, worn soil, gravel, moss on top, blended by
  each texture's height map). Bump `AHoldoverWoodsRange::GroundVersion` and re-bake after changing the ground shape.
  If the tiles are missing, a generated ground stands in. Ground sounds as dirt, the pad as gravel.
- **Plants**: Fab Megaplants (oak, beech, hornbeam, elder, bracken) for trees, young trees and ferns; their loose leafy
  branches stood upright as saplings and twigs as seedlings; Poly Haven ferns, shrubs, nettles, celandine, grass and
  moss; Megascans wild grass.
- **Dead wood and rocks**: Poly Haven scanned logs, stumps, roots, branches and mossy rocks, with real collision.
- **Light**: sun to the left of the lane (leaves between you and it glow, trunks read dark), a hazy pale sky, ground fog
  with light shafts, eye adaptation, darker and cooler grading.
- **Lumen**: the skinned Megaplants are invisible to Lumen's software tracing, so hidden stand-in crowns and trunks
  shade the forest floor (`bCanopyShade`).

## Tuning without a rebuild

Every density and light value is a config setting in `Config/DefaultGame.ini`, section
`[/Script/Holdover.HoldoverWoodsRange]`: `ForestDensity`, `TreeDensity`, `YoungTreeDensity`, `SaplingDensity`,
`FernDensity`, `GroundPlantDensity`, `GrassDensity`, `DebrisDensity`, `SunPitch`, `SunBearing`, `SunLux`, `SunKelvin`,
`SkyHaze`, `FogDensity`, `ExposureBias`, `EyeMinEV`, `EyeMaxEV`, `WhiteBalance`, `Saturation`, `Contrast`, `BarkDarken`
and more (see the header). Change and relaunch.

## Pipeline (editor closed, headless)

1. `tools/range/fetch_polyhaven.py` (Blender's Python): downloads the CC0 models and textures into `art/polyhaven`.
2. `tools/unreal/import_woods_assets.py`: imports them (Nanite, collision for solid pieces).
3. `tools/unreal/make_woods_materials.py`: ground, timber, foliage and surface materials; usage flags.
4. `tools/unreal/build_woods_ground.py`: bakes the ground tiles.
5. `tools/range/woods_round.ps1 -Name rN [-Build] [-Materials] [-Bake]`: builds, runs `holdover.RangeShots 1` (12 eye-level
   pictures at 1920x1080, `07_reference` framed like Miles's photo) and makes a contact sheet in `art/range/woods/rN`.

## Performance

60 fps at 1080p on the RX 9070 XT when nothing else runs. The smallest plants (grass, seedlings, moss, twigs) cast no
shadows; that alone took the frame rate from 46 to 60. Small things vanish beyond about 90 m; trees never do.

## The old forest range

Retired 2026-11-27 at Miles's request: its code (`HoldoverForestRange`), layout/terrain scripts, home-made kit and
Blender terrain are gone; the unused assets were moved to `art/old_forest_backup/` on Miles's PC.
