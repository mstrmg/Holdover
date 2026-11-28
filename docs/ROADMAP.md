# Holdover roadmap (working backlog)

The ordered task list the coding thread works down. Miles asked on 2026-11-19 for a big plan executed without waiting for him; gameplay choices made along the way are logged in [DECISIONS.md](DECISIONS.md) so he can review and overturn any of them.

Legend: [x] done and tested in the game, [~] code written but not yet built or tried on Miles's PC, [ ] not started. See also [CONTROLS.md](CONTROLS.md) and [SETUP.md](SETUP.md). Each item names the files it lives in once written.

## Phase A: playable gray box (milestone M1)

1. [~] Unreal 5.8 C++ project files: `Holdover.uproject`, targets, module, config (DX12, SM6, Lumen, Virtual Shadow Maps, no static lighting, TSR, 1920x1080 at 60 fps cap, "Bullet" trace channel, surface types).
2. [~] Starts with no assets: the game mode builds a gray range in the engine's empty map (ground, berm, side walls, firing line, bench, distance posts, sun, sky, fog).
3. [~] Eye-view character: camera at eye height, 90 degree horizontal field of view, steady head, walk and jog, full body shown when the Manny mannequin is in the project (head hidden from your own view, shadow keeps it).
4. [~] Controls built in code (no input assets to make): full manual mapping context; assisted context reserved.
5. [~] Gun state machine, data-driven per gun: chamber (empty, live, spent), magazine with its own round count, slide or bolt travel, last-round hold-open, slide stop, hammer, trigger reset, safety, semi and full auto.
6. [~] Racking as its own input: hold to pull the slide back, release to let it fly forward; racking a loaded chamber ejects a live round; slide stop locks or releases.
7. [~] Magazines: drop (stays on the ground with its rounds), insert from the belt pouch, pick up dropped mags, each mag keeps its count.
8. [~] Simulated bullets for every gun: gravity, air drag from bullet mass, diameter and drag coefficient, sights zeroed per gun, small random dispersion, pass-through on paper, impulses on physics objects.
9. [~] Paper targets with real holes at the true hit point and ring scoring (10 down to 3).
10. [x] Physical casing and live-round ejection, pooled with a cap. Cases leave the port at the moment the slide or bolt passes the ejector, in each gun's own direction, cartwheeling, and bounce with a clink per surface (`Weapons/HoldoverLoosePiece`).
11. [~] Gray-box stand-in gun built from simple shapes, so everything works before any model is imported.
12. [~] Basic aim (sights lined up with the eye) and basic recoil kick, so shooting can be tested.
13. [~] Debug views off by default: `holdover.DebugGun 1` (gun state on screen), `holdover.DebugBullets 1` (bullet paths), `holdover.DebugShots 1` (score per shot).
14. [ ] First build on Miles's PC; fix compile errors (Claude can run the build remotely once Unreal and Visual Studio are installed).
15. [ ] Import the M9 model and hook it to the gun code (bones move the slide, hammer, trigger, slide stop, magazine and follower).

## Phase B: hands and animation (M2)

16. [~] Manny full body from the Third Person content pack, driven by its locomotion animation; arms re-posed every frame by code.
17. [~] Two-bone arm IK (in C++ on the body mesh, no Control Rig asset needed) to hand targets, finger curl poses (grip, support wrap, pinch rack, thumb safety, palm mag, trigger finger, relaxed).
18. [~] Animation timeline player: reads `Holdover/Data/GunAnims/<Gun>.json` (format by the guns thread in `docs/animation/gun_anim_format.md`), plays gun bone tracks, hand targets, finger poses, whole-gun offsets and timed events.
19. [~] Timeline events drive the state machine (SlideToRear, InBattery, MagDetach, MagAttach, SlideLock, SlideRelease, EjectCasing...). The timeline says when; the gun state says what.
20. [~] Hold points for manual inputs (hold the slide at the rear, hold a mag in view).
21. [~] Actions for the M9: fire, rack, reload with retention, empty reload, mag check, press check, safety on and off (decocker), slide release, tap-rack, stovepipe and double-feed clearing.
22. [x] Recoil per gun (2026-11-25): a fast kick in the hands (flip, side push, roll, rearward slam, lift) plus a slower climb and wander of the whole aim that the shooter recovers, a blast tremor, and for rifles the head moving with the stock. Random every shot, per-gun numbers in the gun data (`Player/HoldoverCharacter`, `Weapons/HoldoverGunData`).
23. [~] Aiming: bring the gun up to the eye, sight alignment from the gun's real sight sockets, adjustable eye relief.
23b. [~] Gun weight (Miles, 2026-11-19): the barrel trails slightly behind where you look and settles, like bodycam footage; starting, stopping and strafing nudge the gun; heavier guns lag more; less lag while aiming. Aiming in and out eases smoothly with a slight push-out arc, muzzle lining up just before the gun arrives. Strength is a setting (`SwayStrength`, for the settings menu). `Player/HoldoverCharacter`.
24. [~] Malfunctions: stovepipe, double feed, failure to fire (dud), each with a chance per gun and per magazine and cleared only with the correct drill. (built, chances set to 0 for now)
25. [~] Mag check and press check show the real round count and chamber state (no counter on screen).
26. [~] Safety and decocker on the M9; selector on the AK, FAL and AR.
27. [~] Sounds (2026-11-25): layered shot, forest echo tail and action cycling per gun, brass, live-round and magazine landing sounds per surface (hard, soft, wood, metal). Synthesised by `tools/audio/gen_gun_audio.py` (no recordings yet); still to do: dry fire, racking, mag in/out, safety clicks.
28. [x] Muzzle flash, smoke and flash light (2026-11-25), driven from code instead of Niagara so every number is readable: faint, normal or fireball each shot, shaped by the muzzle device (`Weapons/HoldoverMuzzleFX`, textures from `tools/fx/gen_fx_textures.py`).

## Phase C: the other guns (M2)

29. [~] AK: rotating bolt, no hold-open, rock-in magazine, selector safety blocks the charging handle, full auto. (AK preset and controls written; no model yet)
30. [~] AR-15-style carbine: game data, firing, key 5 (2026-11-25). Still to do: forward assist, dust cover, hold pose made for it.
31. [~] FAL: game data, firing, key 4 (2026-11-25). Still to do: hold pose made for it.
32. [~] 1911: single action, thumb safety, slide stop, 7-round mag, firing, key 3 (2026-11-25). Still to do: grip safety logic.
33. [ ] Switching guns at the bench (put one down, pick another up).

## Phase D: eyes, body, ears (M3)

34. [ ] Light adaptation tuning between shade and sun.
35. [ ] Footsteps per surface; subtle walking head motion.
36. [ ] Proposals waiting for Miles: sight focus blur, breathing sway with hold breath, ear protection.

## Phase E: targets and drills (M4)

37. [ ] Steel plates that ring and swing; poppers that fall and reset.
38. [ ] No-shoot targets, hit zones (A, C, D).
39. [ ] Shot timer with start beep and splits; local personal bests saved to disk.
40. [ ] 6 to 8 drills (Bill drill, El Presidente style, Mozambique, dot torture, 25 m slow fire, reload drill, plate rack, malfunction drill).
41. [ ] Minimal range menu: pick gun, drill, distance; reset range.
42. [ ] Settings menu: mouse sensitivity, field of view, key rebinding, graphics quality, full manual or assisted.
43. [ ] Save and load settings and personal bests.

## Phase F: forest and polish (M5, M6)

44. [~] Forest map, rebuilt 2026-11-27 as the woods range to match Miles's reference photo: baked Nanite ground in a valley, layered Megaplant forest with saplings, ferns, low plants, scanned logs, stumps, roots and rocks, timber shelter over the firing line, haze and grading; 60 fps. `Range/HoldoverWoodsRange`, see [maps/WOODS_RANGE.md](maps/WOODS_RANGE.md). The first forest range was removed.
45. [ ] Assisted mode: one reload key, manual racking and jam clearing kept.
46. [ ] Performance pass to 60 fps at 1080p on the RX 9070 XT and Ryzen 5 3600.
