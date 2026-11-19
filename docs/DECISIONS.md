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
