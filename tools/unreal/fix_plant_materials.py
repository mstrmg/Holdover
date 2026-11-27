"""Let the Fab plant materials draw on instanced meshes.

The game places grass and small plants as instanced static meshes (thousands in a few batches). A material
only works there if its "Used with Instanced Static Meshes" box is ticked; Fab's Wild Grass arrives without it,
so the game drew the grass with the gray default material (log: "missing usage flag InstancedStaticMeshes").
In the editor that box is ticked automatically, but not in a -game run or a packaged build.

This ticks the box on the parent material of every material under the plant folders and saves it.
Run headless with the editor closed:

  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="<path>/fix_plant_materials.py" -unattended -nop4 -nosplash -NullRHI
"""
import unreal

ROOTS = ["/Game/Fab", "/Game/Megaplant_Library"]
USAGES = [getattr(unreal.MaterialUsage, n) for n in ("MATUSAGE_INSTANCED_STATIC_MESHES", "MATUSAGE_NANITE")
          if hasattr(unreal.MaterialUsage, n)]
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def log(msg):
    unreal.log("[fix_plant_materials] " + msg)


def main():
    registry.scan_paths_synchronous(ROOTS, False)
    bases = {}
    for root in ROOTS:
        if not eal.does_directory_exist(root):
            continue
        for p in eal.list_assets(root, recursive=True, include_folder=False):
            cls = str(eal.find_asset_data(p).asset_class_path.asset_name)
            if cls not in ("Material", "MaterialInstanceConstant"):
                continue
            asset = eal.load_asset(p)
            base = asset.get_base_material() if asset else None
            if base:
                bases[base.get_path_name()] = base
    log("%d parent materials found" % len(bases))
    changed = saved = 0
    for path, mat in sorted(bases.items()):
        for usage in USAGES:
            res = mel.set_material_usage(mat, usage)
            log("%s %s -> %s" % (path, usage, res))
        changed += 1
        mel.recompile_material(mat)
        pkg = path.split(".")[0]
        if pkg.startswith("/Game/"):
            if eal.save_loaded_asset(mat, only_if_is_dirty=False):
                saved += 1
                log("ticked and saved: " + pkg)
            else:
                log("WARNING could not save: " + pkg)
        else:
            log("WARNING parent lives outside the project, not saved: " + pkg)
    log("done: %d parent materials changed, %d saved" % (changed, saved))


main()
