# Setting up Holdover on your PC

Do these once. After that, Claude can build the game on your PC remotely and fix any errors.

## 1. Install the tools

1. Open the **Epic Games Launcher**, go to **Unreal Engine > Library**, click the **+** next to "Engine Versions" and install **5.8**. In its **Options**, leave the defaults ticked (you don't need Android, iOS or Linux).
2. Install **Visual Studio 2026 Community** from visualstudio.microsoft.com. In the installer, tick the workload **Game development with C++**, and on the right make sure **Unreal Engine installer** and the latest **Windows 11 SDK** are ticked. Then click Install.
3. Install **GitHub Desktop** (desktop.github.com) if you don't have it, and sign in.

## 2. Get the project

1. In GitHub Desktop: **File > Clone repository**, pick **mstrmg/holdover**, and set the local path to `D:\Claude area\Holdover\Holdover`.
2. Open the folder `D:\Claude area\Holdover\Holdover\Holdover`, right-click **Holdover.uproject** and choose **Generate Visual Studio project files** (on Windows 11 it may be under "Show more options").
3. Double-click **Holdover.uproject**. It will say the Holdover module is missing and ask to rebuild it. Click **Yes**. The first build takes a few minutes.
   - If it says the build failed, don't worry: tell Claude, and Claude will run the build on your PC and fix the errors.

## 3. Add the body (optional, but needed to see your arms and feet)

1. In the Unreal editor, click **Add** (top of the Content Browser) > **Add Feature or Content Pack**.
2. Pick **Third Person** under "Blueprint Feature" and click **Add to Project**.
3. For the first-person arms the game also needs Epic's first-person hold animations: **Add > Add Feature or Content Pack > First Person** (they land in `Content/Characters/Mannequins/Anims/Pistol` and `.../Rifle`). Without them the arms show a stiff default pose.
4. That's it: the game finds the mannequin by itself. Without it, the gun still works and floats in front of you.

## 4. Play

1. Press the green **Play** button at the top of the editor (or Alt+P).
2. You start at the firing line of a gray range. Click once inside the game window so it takes the mouse.
3. Keys are in [CONTROLS.md](CONTROLS.md). Remember to rack (R) before your first shot.
4. Press Esc to stop playing.

## 5. Save the test level (optional)

The game builds the range itself in an empty map, so there's nothing to save. If you want a level you can open again in the editor: **File > Save Current Level As**, make a folder `Maps` and save it as `L_TestRange`. Tell Claude when you've done that.

## Letting Claude build on your PC

When Claude asks to build, accept the prompt on your PC. Claude then runs the build, reads the errors and fixes them. Your PC needs to stay on while that happens.

## Re-importing the gun models (Claude does this)

The Blender exports in `D:\Claude area\Holdover\art\weapons\<gun>\export\` are imported into `/Game/Guns/<GunId>/` by `tools/unreal/import_guns.py` (close the editor first):

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Claude area\Holdover\Holdover\Holdover\Holdover.uproject" -run=pythonscript -script="D:\Claude area\Holdover\Holdover\tools\unreal\import_guns.py Pistol_M9" -unattended -nop4 -nosplash -NullRHI
```

Leave out the gun id to import all five. The game picks the models up by name; without them it shows gray stand-ins.

## Test switches (console, ` key)

- `holdover.ForceAim 1`: aim without holding the right mouse button.
- `holdover.DebugView 1/2/3`: look at yourself from the side, front or behind.
- `holdover.TestShots 1`: takes pistol and rifle screenshots (ready and aimed) into `Saved/Screenshots/test`, then quits.
