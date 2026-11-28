"""Remove the first forest range's assets that nothing uses any more (2026-11-27, Miles asked for the old map to be
deleted once the woods range was done). Headless, editor closed, AFTER backing the files up (woods pipeline step;
the PC copy is in art/old_forest_backup):
  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="<path>/retire_old_forest.py" -unattended -nop4 -nosplash -NullRHI
Only assets with no referencer outside the retired set are deleted; the rest are listed and kept.
"""
import unreal

eal = unreal.EditorAssetLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()

FOLDERS = ["/Game/Range/Forest"]
ASSETS = ["/Game/Range/Materials/M_RangeGround", "/Game/Range/Materials/MI_Rock_Mossy", "/Game/Range/Materials/MI_Ground_Leaves",
          "/Game/Range/Materials/MI_Bark_Hardwood"]
TEXTURE_SETS = ["ScatteredLeaves009", "Ground037", "Ground076", "Gravel042", "Rock063", "Bark014"]


def log(m):
    unreal.log("[retire_old_forest] " + m)


def main():
    reg.scan_paths_synchronous(["/Game"], True)
    cands = set()
    for f in FOLDERS:
        if eal.does_directory_exist(f):
            cands.update(p.split(".")[0] for p in eal.list_assets(f, recursive=True, include_folder=False))
    cands.update(a for a in ASSETS if eal.does_asset_exist(a))
    for t in TEXTURE_SETS:
        d = "/Game/Range/Textures/" + t
        if eal.does_directory_exist(d):
            cands.update(p.split(".")[0] for p in eal.list_assets(d, recursive=True, include_folder=False))
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
    # Repeatedly drop candidates that something outside the set still uses (and so on down the chain).
    changed = True
    while changed:
        changed = False
        for c in sorted(cands):
            refs = [str(r) for r in (reg.get_referencers(c, opts) or [])]
            outside = [r for r in refs if r not in cands and r != c]
            if outside:
                cands.discard(c)
                changed = True
                log("keep %s (used by %s)" % (c, ", ".join(outside[:3])))
    deleted = 0
    for c in sorted(cands):
        if eal.delete_asset(c):
            deleted += 1
            log("deleted " + c)
        else:
            log("could not delete " + c)
    for f in FOLDERS:
        if eal.does_directory_exist(f) and not eal.list_assets(f, recursive=True, include_folder=False):
            eal.delete_directory(f)
    log("done: %d deleted" % deleted)


main()
