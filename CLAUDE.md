# Holdover

A single-player, photorealistic shooting range game in Unreal Engine 5, seen through the player's own eyes. No enemies or bots, only targets. Personal project, not for sale.

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
- Camera and the eye view
- UI, menus and anything the player sees or feels
- Adding or removing any feature

When unsure which one a change is, treat it as gameplay and ask.

Explain everything to Miles in plain language. When he must do something on his PC, give numbered steps.

## Technical conventions

- Engine: Unreal Engine 5.8 (5.7 minimum, needed for Megaplants and production-ready PCG), C++ project named `Holdover`. Compiler: Visual Studio 2026 Community (Epic's recommendation for 5.8; VS 2022 17.14+ if on 5.7).
- Camera: true first person through the player's eyes, using Unreal's First Person Rendering with the full-body ("advanced") setup. No bodycam effects, no HUD.
- Repo layout: the UE project lives in the `Holdover/` subfolder (`Holdover/Holdover.uproject`); on Miles's PC the repo is cloned at `D:\Claude area\Holdover\Holdover` (open `Holdover\Holdover.uproject` inside it), next to the Blender files in `D:\Claude area\Holdover\art`. `D:\Claude area\Holdover\UE5 Holdover` is a First Person template project, used only as a source of Content (the Manny mannequin, copied into the clone's untracked `Holdover\Content`) and never built. Builds run on Miles's PC through Remote Control; Git there has no GitHub sign-in, so push fixes from the cloud session.
- Core logic lives in C++ (`Holdover/Source/Holdover/`), with Blueprints only as thin wrappers, because Claude cannot read or edit binary Blueprint assets.
- Guns: a C++ state machine (chamber, magazine, slide/bolt, safety, malfunctions) configured by one data asset per gun.
- Hand and gun motion: procedural (code-driven), so no paid animations are needed. Gun/hand timelines are JSON files in `Holdover/Data/GunAnims/` (format: `docs/animation/gun_anim_format.md`, written by the guns thread). Arm IK and finger curls are done in C++ (`Player/HoldoverBodyRig`) on poseable copies of the Manny body, not with a Control Rig asset, because Claude can't edit binary assets.
- Gameplay timing lives in the timelines (when), the gun state machine decides what happens (what).
- With no assets at all, the game builds a gray test range and a gray-box gun from engine shapes, so it is always playable.
- Bullets: simulated projectiles with velocity and drop for every gun (no hitscan).
- Input: Enhanced Input, with separate mapping contexts for full manual and assisted modes.
- Casings and dropped mags: Chaos physics with pooling and a cap.
- Audio: MetaSounds, surface-dependent impacts and forest reverb.
- Lighting: Lumen, Virtual Shadow Maps, volumetric fog. Target resolution: 1080p at 60 fps (TSR or FSR upscaling only if needed).
- Textures: 2K (2048 px) maximum for every asset, including Megascans and Blender bakes.
- Preview renders: 1920x1080.
- Expose tuning values (recoil, sway, casing ejection speed, etc.) as `UPROPERTY(EditAnywhere)` so they can be tweaked without code changes.
- Large binary files go through Git LFS (see `.gitattributes`). Claude's cloud session cannot upload LFS files; Miles adds binaries from his PC.
- Free assets only: Quixel Megascans and Megaplants on Fab (free again since Nov 2026), Poly Haven, ambientCG, Sketchfab (check license), Freesound, Sonniss GDC bundles. Record each asset's source and license in `docs/ASSETS.md`.
- Target hardware (Miles's PC): Radeon RX 9070 XT, Ryzen 5 3600, 16 GB DDR4 (upgrade to 32 GB before M5), NVMe SSD. CPU and RAM are the bottlenecks: watch foliage density, shader count and editor memory.

## Docs

- `docs/SETUP.md`: Miles's install and first-run steps.
- `docs/CONTROLS.md`: the key layout.
- `docs/ROADMAP.md`: the ordered coding backlog with status.
- `docs/DECISIONS.md`: gameplay choices made without Miles's yes, for him to review.

## Plan

The living plan doc: https://claude.ai/artifact/WnGBCzQrguHhMrC7VW5WbT

### Vision
First-person target practice through the player's own eyes, in a photoreal forest, with guns operated by hand one step at a time. Realism comes from how eyes, hands, breathing and hearing behave, not from camera effects.

### The eye view
- Full body visible (arms, chest, feet, shadow). Natural FOV, 90 by default, adjustable.
- Steady head with only slight walking sway. No crosshair, no ammo counter.
- Light adaptation when moving between shade and sun.
- Proposed, awaiting Miles's yes: sight focus (front sight sharp, target soft), breathing sway with hold breath, ear protection (muffled vs. ringing).

### Gun mechanics (core identity)
- Chamber tracked separately from the magazine; a full mag with an empty chamber doesn't fire until racked.
- Racking is its own input; racking a loaded chamber ejects a live round.
- Bolt-action cycling in four steps (later).
- Physical shell ejection with per-surface bounce sounds; casings stay on the ground up to a limit.
- Slide or bolt locks back on the last round.
- Each magazine keeps its own round count; dropped mags stay; mags loaded round by round.
- Malfunctions: stovepipe, double feed, failure to fire, each cleared with the correct drill.
- Safety; press check and mag check.
- Per-gun recoil pattern plus randomness; gun moves in the hands and settles.
- Controls: full manual first; assisted mode (one reload key, manual racking and jam clearing) added later as a setting.

### Range, targets, scoring
- Forest lane through a dense summer hardwood forest (reference: forest-vibe.png in project files; add under `docs/reference/` from Miles's PC). 5 to 50 m.
- Paper (real holes, inspectable), steel plates and poppers, no-shoots.
- Time plus accuracy with hit zones; shot timer with splits; local personal bests.
- Version 1 guns: one original pistol, one carbine. Later: bolt-action rifle, optics, more ranges.
- Out: enemies or bots, multiplayer, real gun brands, consoles, selling the game.

### Milestones (also GitHub milestones)
0. M0 Setup: tools installed on Miles's PC, C++ project skeleton builds.
1. M1 First shot: gray box, full-body character, pistol with chamber/mag/rack/slide lock, simulated bullets, paper targets.
2. M2 Gun feel: procedural hands, recoil, ADS, shell ejection, safety, checks, malfunctions; add carbine.
3. M3 Eyes, body and ears: camera tuning, light adaptation, footsteps, approved proposals.
4. M4 Targets and drills: steel, poppers, no-shoots, shot timer, 6-8 drills, scoring, menu.
5. M5 The forest (needs 32 GB RAM): Megaplants/Megascans via PCG, lighting, haze, audio, 60 fps at 1080p.
6. M6 Assisted mode and polish: assisted setting, rebinding, graphics settings, performance.
