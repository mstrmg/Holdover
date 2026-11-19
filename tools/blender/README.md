# Blender build scripts

Python scripts Claude uses to build the gun models in Blender (5.x). Copies live on Miles's PC in `D:\Claude area\Holdover\art\tools\`.

- `holdover_lib.py`: shared helpers (shapes, booleans, lofts, procedural worn steel and wood, UV unwrap, texture baking, FBX export). Load it inside Blender with:
  ```python
  LIB = r"D:\Claude area\Holdover\art\tools\holdover_lib.py"
  exec(compile(open(LIB).read(), LIB, "exec"), {"__name__": "holdover_lib"})
  ```
- `build_ak_A.py`, `build_ak_B.py`, `build_ak_C.py`: rebuild the AK-style rifle from scratch in `rifle_ak.blend` (run A, then B, then C).

Units are millimetres, X forward, Y left, Z up, bore axis at Z = 0. See `docs/weapons/README.md` for export conventions.
