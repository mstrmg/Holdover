"""Build (and cache) the Fab Megaplants mesh data once, one plant folder at a time.

Megaplants arrive without built mesh data. The first time anything loads them, Unreal builds every
trunk and branch mesh, which takes many GB; inside a game run on a 16 GB PC that ran out of memory.
Run this headless first, so the game later loads the finished data from the cache:

  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="<path>/prebuild_megaplants.py" -unattended -nop4 -nosplash -NullRHI

Optional: set HOLDOVER_PLANT_FOLDER to one folder (for example /Game/Megaplant_Library/Tree_English_Oak)
to build just that one; run once per folder if memory is tight. Without it, every folder under
/Game/Megaplant_Library and /Game/Fab/Megascans/Plants is built in turn, with memory freed in between.
"""
import os
import time

import unreal

ROOTS = ["/Game/Megaplant_Library", "/Game/Fab/Megascans/Plants"]
eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def log(msg):
    unreal.log("[prebuild_megaplants] " + msg)


def plant_folders():
    one = os.environ.get("HOLDOVER_PLANT_FOLDER")
    if one:
        return [one]
    out = []
    for root in ROOTS:
        if eal.does_directory_exist(root):
            out.extend(sorted(registry.get_sub_paths(root, False)))
    return out


def build(folder):
    t0 = time.time()
    paths = [p for p in eal.list_assets(folder, recursive=True, include_folder=False)]
    meshes = 0
    for p in paths:
        data = eal.find_asset_data(p)
        cls = str(data.asset_class_path.asset_name)
        if cls not in ("StaticMesh", "SkeletalMesh"):
            continue
        asset = eal.load_asset(p)  # loading builds the mesh data and stores it in the cache
        if asset:
            meshes += 1
        del asset
    unreal.SystemLibrary.collect_garbage()
    log("%s: %d meshes built in %.0f s" % (folder, meshes, time.time() - t0))


def main():
    registry.scan_paths_synchronous(ROOTS, False)
    folders = plant_folders()
    log("folders: " + ", ".join(folders))
    for f in folders:
        build(f)
    log("done")


main()
