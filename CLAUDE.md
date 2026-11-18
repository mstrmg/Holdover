# Holdover

A single-player, photorealistic shooting range game in Unreal Engine 5. No enemies or bots, only targets, seen through a body-worn camera. Personal project, not for sale.

## Working rules (from Miles)

Miles does not code. Claude writes all code and scripts; Miles builds, plays and gives feedback on his PC.

**Maintenance: just do it, never ask first.** Commit, push and merge without waiting for approval, then tell Miles what changed in one line. Maintenance includes:
- Repo setup, `.gitignore`, `.gitattributes`, Git LFS rules, folder structure
- Docs, README, this file, the plan
- Fixing build or compile errors, warnings, crashes
- Refactors and cleanup that don't change how the game plays or looks
- Updating engine or plugin versions, project settings unrelated to gameplay
- Performance fixes that keep the same look and feel

**Gameplay: always ask Miles before doing it.** Propose the change, explain it in plain language (no code jargon), and wait for his yes. Gameplay includes:
- Weapon behavior: recoil, ballistics, handling, reloads, malfunctions, sounds
- Controls and input
- Targets, drills, scoring, rules
- Level layout, environment look, lighting, art direction
- Camera and the bodycam effect
- UI, menus and anything the player sees or feels
- Adding or removing any feature

When unsure which one a change is, treat it as gameplay and ask.

Explain everything to Miles in plain language. When he must do something on his PC, give numbered steps.

## Technical conventions

- Engine: Unreal Engine 5, C++ First Person template, project name `Holdover`.
- Core logic lives in C++ (`Source/Holdover/`), with Blueprints only as thin wrappers, because Claude cannot read or edit binary Blueprint assets.
- Expose tuning values (recoil, spread, casing ejection speed, etc.) as `UPROPERTY(EditAnywhere)` so they can be tweaked without code changes.
- Large binary files go through Git LFS (see `.gitattributes`).
- Free assets only: Poly Haven, ambientCG, Fab free listings, Sketchfab (check license), Freesound, Sonniss GDC bundles. Record each asset's source and license in `docs/ASSETS.md`.
- Target hardware (Miles's PC): Radeon RX 9070 XT, Ryzen 5 3600, 16 GB DDR4 (32 GB recommended before the photoreal pass), NVMe SSD. The CPU and RAM are the bottlenecks: watch foliage density, shader count and editor memory.

## Plan

The living plan doc: https://claude.ai/artifact/WnGBCzQrguHhMrC7VW5WbT

### Vision
First-person target practice that looks and feels like real body-worn camera footage. Three pillars: a photoreal environment, a camera that moves like it is strapped to a body, and guns that behave like real ones.

### Gun mechanics (core identity)
- Chamber state tracked separately from the magazine; a full mag with an empty chamber doesn't fire until racked.
- Racking the slide, charging handle or bolt is its own input; racking a loaded chamber ejects a live round.
- Bolt-action cycling in four steps (lift, back, forward, down).
- Physical shell ejection: per-weapon direction and speed, bounce sounds by surface (dirt, gravel, wood), casings stay on the ground up to a limit.
- Slide or bolt locks back on the last round.
- Each magazine keeps its own round count; dropped mags stay on the ground; mags are loaded round by round.
- Malfunctions: stovepipe, double feed, failure to fire, each cleared with the correct drill.
- Safety and fire selector; press check and mag check instead of an ammo counter.
- Muzzle flash, lingering smoke, heat shimmer; mechanical sounds for every action.
- Controls: **full manual** (each step its own key) is built first; an **assisted** mode (one reload key, manual racking and malfunction clearing) is added later as a setting.
- Recoil: per-weapon pattern plus randomness, camera kick and settle; no crosshair by default.
- Ballistics: travel time and drop for rifles; hitscan is fine for pistols up to 25 m.

### Ranges and targets
- First range: a lane cut through a dense summer hardwood forest (eastern US style): tall dark trunks, saplings, ferns, sunlight breaking through, light haze. 5 to 50 m. A reference image Miles shared is in the project files (forest-vibe.png); add it under `docs/reference/` from his PC.
- Later: more forest ranges, a long range field or power line cut, a shoot house.
- Targets: paper (real holes, inspectable), steel plates and poppers (ring and swing or fall), moving and turning targets, no-shoot targets.

### Scoring
Drills scored on time plus accuracy with hit zones; misses and no-shoots cost time. Shot timer with start beep and split times. Personal bests saved locally.

### Bodycam look
Wide lens with barrel distortion, vignette, chromatic aberration, motion blur, compression artifacts, timestamp overlay, head-mounted sway driven by movement.

### Version 1 scope
In: the forest range, pistol and carbine (original, unbranded), paper and steel targets, 6 to 8 drills, the bodycam camera, local scores.
Later: bolt-action rifle, optics and attachments, moving targets, drill editor, replay viewer, more ranges.
Out: bots or AI enemies, multiplayer, real gun brands, consoles, Steam release.

### Milestones
1. Setup: install UE5, Visual Studio 2022, Git LFS, GitHub Desktop, Blender; create the project.
2. Gray box range: untextured lane, movement, a placeholder gun that fires, paper targets that register hits.
3. Weapon feel: chamber, racking, shell ejection, reloads, recoil, malfunctions, sound.
4. Photoreal forest: free scanned assets, PCG tree scattering, Lumen lighting, fog (needs 32 GB RAM).
5. Bodycam camera.
6. Drills and scoring.
7. Polish: menus, settings, performance tuning.
