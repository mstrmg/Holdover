"""Bake the woods range ground tiles (see Range/HoldoverWoodsGround.h). Headless, editor closed:
  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="<path>/build_woods_ground.py" -unattended -nop4 -nosplash -NullRHI
Run make_woods_materials.py first (the tiles point at MI_WoodsGround)."""
import unreal

n = unreal.HoldoverWoodsGround.bake_woods_ground()
unreal.log("[woods_ground] python: %d meshes" % n)
