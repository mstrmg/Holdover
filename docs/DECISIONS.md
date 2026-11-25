# Gameplay decisions to review

On 2026-11-19 Miles asked for a big plan worked through without waiting for him. Normally every gameplay choice needs his yes (see CLAUDE.md). The choices below were made with realistic defaults instead. Each one is easy to change: say which ones you want different.

## Controls
1. Key layout as in [CONTROLS.md](CONTROLS.md). R is rack, T is slide stop, Q drops the mag, G stows it, E inserts, C and X are the checks, V is safety, F picks up.
2. Racking is hold-to-pull: hold R and the slide stays back, release and it flies forward. This lets you lock the slide open (hold R, tap T) and inspect.
3. A new or reset gun has a full magazine and an empty chamber. You always rack before the first shot.
4. Each gun has its own belt pouch with 4 spare magazines. Magazines come out of the pouch in order; a stowed partial mag goes to the back.
5. Assisted mode (one reload key) is reserved but not mapped yet.

## The eye view
6. Eye height 165 cm, eyes 12 cm in front of the body's centre, 90 degree field of view.
7. Hands and gun are drawn at 70% size by Unreal's First Person rendering so they don't clip into walls. It isn't noticeable at the normal field of view. (1 = off.)
8. Walking moves the head up and down by only 0.35 cm. Jogging is 380 cm/s, walking 150, walking while aiming 90.
9. Aiming brings the gun's real sights to the eye (no zoom, no crosshair). Not aiming, the gun is held high, just below the eye line with the muzzle near the target (Miles: "a lot higher in the screen"); it was a low ready before.
10. Your own head is hidden from your view, but your shadow still has one.
10b. Gun lag (Miles asked for it; the amounts are my picks): turning at a normal pace (about a quarter turn per second) makes the barrel trail about 4 degrees behind your view, never more than 6, and it settles in about a fifth of a second with a tiny swing past. The rifle feels 1.6 times heavier than the pistol. While aiming the lag is 60% of that. Starting, stopping and strafing shift the gun up to 2.5 cm. One strength slider (0 to 2) will go in the settings menu.
10f. Hold poses redone with real wrists (Miles, 2026-11-23: the pistol's right wrist bent impossibly far sideways, and the AK's front fingers wrapped all the way around the handguard). Each arm is now posed so the wrist stays inside what a real hand can do (bent at most about 25° to the side and 30° up or down, measured in Blender), the elbow is a plain hinge, and the forearm's twist is shared along the forearm instead of all at the wrist. Pistol: isosceles stance, arms almost straight, shoulders rolled slightly forward, rear sight 52 cm from the eye (was 43). AK: the body is only turned about 20° (Epic's rifle pose had the right shoulder pulled back about 50°), the stock sits in the right shoulder pocket, the right elbow points out and down with the hand wrapping the grip and the finger on the trigger, and the handguard rests in the left palm with the fingers lying along its far side and the thumb along the near side. (Fixed the same day after Miles saw the front fingers "bent crazy": they had been bent sideways at the knuckles, which real fingers can't do; each finger now only curls at its joints.) In game an arm the pose holds straight may stay that straight, so the elbows and wrists keep the posed angles.
10e. Hand-made hold poses (Miles chose "Pose the hands in Blender"). Both hands and every finger were posed around the real M9 and AK models in Blender (`art/hands/hands_pose.blend` on Miles's PC) and saved as `Holdover/Data/HoldPoses/Pistol.json` and `Rifle.json`. Pistol: two-hand thumbs-forward grip, trigger finger on the trigger, support fingers wrapped over the shooting fingers, arms nearly straight with the rear sight 43 cm from the eye (Epic's animation held it about 26 cm away). AK: shooting hand on the pistol grip with the finger on the trigger, support hand cupping the handguard from below, thumb along its left side, rear sight 32 cm from the eye with the cheek on the stock. In game both hands are locked to the gun and only the elbows bend to reach it (at the ready too); during reloads and checks the support hand still follows the gun timelines. The old idle-animation arms come back if `Use Hold Poses` is turned off.
10d. The arms you see are a separate pair of first-person arms (Miles chose Unreal's ready-made arms). They play Epic's first-person pistol and rifle hold animations, the right hand is locked to the grip, and the arms hang from just below the eyes and stretch to the gun. The pistol is held two-handed; on the rifle the left hand sits under the handguard. The arms of the full body are hidden; the shadow keeps them. Known rough spots: the left forearm looks big when aiming the AK, and the left hand during reloads and racking still follows the older hand targets.
10c. Aiming in eases in and out (95% there in 0.4 s for the pistol, 0.5 s for the rifle; Miles found 0.25/0.32 too quick), the gun pushes out about 3 to 5 cm along the way, and the muzzle lines up slightly before the gun arrives. Letting go half-way reverses smoothly.

## Guns
11. Pistol slot: M9-style, 15-round mags, 9 mm at 360 m/s, sights zeroed at 25 m, safety/decocker.
12. Rifle slot: AK-style, 30-round mags, 7.62x39 at 715 m/s, zeroed at 50 m, selector Safe, Auto, Semi, no bolt hold-open.
13. Recoil is a spring: the gun kicks up and back in your hands, with a little random sideways push, then settles. The pistol kicks harder in pitch than the rifle.
14. Malfunctions (stovepipe, double feed, dud) are built but switched off (chance 0) until the guns feel right.
15. Mag check and press check currently show a short line of text ("12 rounds", "Chamber: live"). This is temporary: once the gun models show real rounds you'll read it from the gun instead.

## Bullets and targets
16. Every bullet is simulated with gravity and air drag; sights are zeroed per gun so the bullet hits where the sights point at the zero distance.
17. Bullets go through paper and stop in the backer board or berm.
18. Paper targets at 5, 7, 10, 15, 25 and 50 m with rings scored 10 down to 3. A hole that touches a line scores the higher ring.
19. Brass and dropped magazines stay on the ground, up to 150 pieces; the oldest disappear after that.

## The test range
20. Until the forest exists, the game builds a gray range by itself in an empty map: flat ground, 3 m side walls, a dirt berm at the back, a firing line, a bench and distance posts, with sun, sky and light fog.
21. You start standing at the firing line.

## Firing (2026-11-25)

Miles asked for the whole firing experience on all five guns. These are the choices made along the way; all numbers are in the gun data and easy to change.

22. Keys 3, 4 and 5 pick the 1911, FAL and AR (Miles said yes via the project chat).
23. Muzzle flash: each shot is faint (barely out of the barrel), normal, or normal plus a fireball of gas lighting up further out. Chances per gun: M9 25% faint / 30% fireball (of the rest), 1911 35% / 20%, AK 10% / 35%, FAL 25% / 25%, AR 35% / 12%. Size, length, brightness, colour (yellow-white to deep orange) and how long it lingers (one frame, sometimes two or three dimmer ones) change every shot.
24. Each muzzle shapes its flash: pistols puff evenly with an odd lick to one side, the AK's slant brake throws flame up and to the right, the FAL's slotted hider gives thin jets all round, the AR's birdcage gives a small star with nothing underneath (it is the most hidden, like the real one).
25. The flash lights the hands, gun and ground for a frame: about 45% of sunlight on the hands for pistols, 70% for the AK and FAL, 50% for the AR, falling off with distance.
26. Smoke: two to four puffs from the muzzle that shoot forward, stall, swell, rise and drift on a light breeze for one to two and a half seconds, plus a thin wisp from the ejection port. The 1911 and AK smoke the most.
27. Recoil, pistols: a sharp flip about the wrist (about 6 to 10 degrees), a small push to the right and a slight roll, a 2 to 3 cm slam back, all back on target in about a quarter of a second. The 1911 lifts further and pushes slower than the M9. The head barely moves.
28. Recoil, rifles: the head rides the stock, so most of the kick moves your view with the gun (85% for the AK and FAL, 80% for the AR) instead of the rifle jumping in front of your eye. AK: loose, rolling push that wanders right in strings. FAL: the hardest shove and the biggest climb. AR: light, short and straight.
29. Strings and full auto stack up a slower climb and sideways wander of the whole aim that recovers in about half a second when you stop.
30. A very short tremor of the view with each shot (strongest on the FAL). No other camera effects.
31. Cases: M9 and 1911 throw right, up and slightly back (1 to 2 m); the AK throws hard right and forward (3 to 5 m); the FAL throws hardest, right and slightly forward; the AR throws right and slightly back (about 4 o'clock). They cartwheel, bounce off hard ground two or three times and clink, louder the harder they land.
32. Sounds are generated, not recorded: the shot at your ears (blast, crack, supersonic snap for rifles, chest thump, ground bounce), the echo coming back off the trees, the berm and the far hill, the action cycling (the AR has its buffer-spring "sproing"), and brass, rounds and magazines landing on hard, soft, wood and metal surfaces. Real recordings (CC0) can replace them later without code changes.
33. The trigger finger's press now starts at the moment the shot breaks, so the recoil, slide, flash and sound all start on the same frame (the old timeline started the kick 50 to 60 ms after the shot).