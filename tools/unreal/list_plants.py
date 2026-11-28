"""List every plant mesh (Megaplants, Megascans) with its size, so the forest builder can sort them.

Headless, editor closed:
  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="<path>/list_plants.py" -unattended -nop4 -nosplash -NullRHI
Writes Saved/plant_list.tsv (path, class, height m, width m, depth m, origin z m).
"""
import os

import unreal

ROOTS = ["/Game/Megaplant_Library", "/Game/Fab"]
eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def main():
    registry.scan_paths_synchronous(ROOTS, False)
    rows = []
    for root in ROOTS:
        if not eal.does_directory_exist(root):
            continue
        for p in eal.list_assets(root, recursive=True, include_folder=False):
            cls = str(eal.find_asset_data(p).asset_class_path.asset_name)
            if cls not in ("StaticMesh", "SkeletalMesh"):
                continue
            a = eal.load_asset(p)
            if not a:
                continue
            b = a.get_bounds()
            e = b.box_extent
            o = b.origin
            rows.append("%s\t%s\t%.2f\t%.2f\t%.2f\t%.2f" % (p.split(".")[0], cls, e.z * 0.02, e.x * 0.02, e.y * 0.02, (o.z - e.z) * 0.01))
    out = os.path.join(unreal.Paths.project_saved_dir(), "plant_list.tsv")
    with open(out, "w") as f:
        f.write("\n".join(rows))
    unreal.log("[list_plants] %d meshes -> %s" % (len(rows), out))


main()
