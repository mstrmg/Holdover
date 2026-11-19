# Controls (full manual mode)

Every gun operation is its own key. Nothing happens automatically: a fresh gun has a full magazine in it but an **empty chamber**, so you have to rack it before the first shot.

The keys are built in code (`Player/HoldoverInputSet.cpp`). A rebinding menu comes later (roadmap item 42).

| Key | What it does |
|---|---|
| W A S D | Walk |
| Shift (hold) | Jog |
| Mouse | Look |
| Left mouse | Trigger. Hold for full auto on rifles set to auto. |
| Right mouse (hold) | Aim: bring the sights up to your eye |
| R (hold) | Rack the slide (pistol) or pull the charging handle (rifle). Hold to keep it back, release to let it fly forward. If the slide is locked open, R slingshots it. |
| T | Slide stop / bolt release. Slide locked open: drops it forward. While holding R: locks the slide open. |
| Q | Drop the magazine on the ground (it stays there with its rounds) |
| G | Pull the magazine out and put it back in your belt pouch (a retention reload) |
| E | Take a magazine from the pouch and insert it |
| C (hold) | Magazine check: look at the round count |
| X (hold) | Press check: look into the chamber |
| V | Safety (pistol: safety/decocker on and off; rifle: steps the selector Safe, Auto, Semi) |
| F | Pick up what you're looking at (a dropped magazine, a live round) |
| 1 | Pistol (M9-style) |
| 2 | Rifle (AK-style) |
| Backspace | Reset: refill pouches, fresh mag in each gun, clear targets and brass |

Each gun starts with 4 full magazines in the pouch plus one in the gun.

## Common sequences

- **First shot:** R (tap) to chamber a round, then Left mouse.
- **Empty reload, pistol:** the slide locks back on the last shot. Q (drop empty), E (new mag), T (slide stop) or R (slingshot).
- **Tactical reload:** G (stow the partial mag), E (fresh mag). The partial mag goes to the back of the pouch.
- **Unload:** Q, then hold R to eject the round from the chamber.
- **Malfunction:** tap-rack is E (make sure the mag is seated) then R. Malfunctions are switched off until they are tuned.

## Debug views (open the console with the ` key)

- `holdover.DebugGun 1`: the gun's state on screen (chamber, mag count, slide).
- `holdover.DebugBullets 1`: draws each bullet's path.
- `holdover.DebugShots 1`: score and distance for each hit.
