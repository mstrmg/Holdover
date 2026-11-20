"""Import the Blender range props, forest kit and (later) forest terrain for the forest range.

Run headless:
  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="<path>/import_range.py" -unattended -nop4 -nosplash -NullRHI
or from the editor: Tools > Execute Python Script.

Source files (on Miles's PC, next to the repo):
  art/props/<prop>/*.fbx             -> /Game/Range/Props/        (benches, stools, signs, ammo can...)
  art/props/forest/*.fbx             -> /Game/Range/Forest/Kit/   (rocks, logs, stumps, branches)
  art/range/forest/SM_ForestTerrain.fbx (Blender terrain, optional) -> /Game/Range/Forest/Ground/

It also makes the empty folders the forest range looks in for Fab plants:
  /Game/Range/Forest/Trees, Saplings, Plants, Grass  (see docs/maps/FOREST_RANGE.md)

Each asset keeps its FBX file name. The script prints every imported name at the end, so the
names the game looks for (HoldoverForestRange.cpp, PlaceRangeProps) can be checked.
"""
import glob
import os

import unreal

MAX_TEXTURE = 2048  # project rule: 2K max

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.environ.get("HOLDOVER_ART", os.path.normpath(os.path.join(HERE, "..", "..", "..", "art")))
if not os.path.isdir(ART):
    ART = r"D:\Claude area\Holdover\art"

# (glob pattern, destination, is terrain). Props sit one folder deep (art/props/shooting_bench/SM_ShootingBench.fbx).
SOURCES = [
    (os.path.join(ART, "props", "*", "*.fbx"), "/Game/Range/Props", False),
    (os.path.join(ART, "props", "forest", "*.fbx"), "/Game/Range/Forest/Kit", False),
    (os.path.join(ART, "range", "forest", "*.fbx"), "/Game/Range/Forest/Ground", True),
]
PLANT_FOLDERS = ["/Game/Range/Forest/Trees", "/Game/Range/Forest/Saplings",
                 "/Game/Range/Forest/Plants", "/Game/Range/Forest/Grass"]

assets = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[import_range] " + msg)


def setp(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as e:  # noqa
        log("option %s.%s not set: %s" % (type(obj).__name__, name, e))


def use_legacy_fbx():
    # Interchange ignores FbxImportUI options; use the classic FBX importer so the options apply.
    for cvar in ("Interchange.FeatureFlags.Import.FBX", "Interchange.FeatureFlags.Import.FBX.ToLevel"):
        try:
            unreal.SystemLibrary.execute_console_command(None, cvar + " 0")
        except Exception as e:  # noqa
            log("could not set %s: %s" % (cvar, e))


def fbx_options(is_terrain):
    ui = unreal.FbxImportUI()
    setp(ui, "import_mesh", True)
    setp(ui, "import_as_skeletal", False)
    setp(ui, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    setp(ui, "import_animations", False)
    # Materials come from make_range_materials.py (matched by slot name); importing the FBX ones
    # only leaves unsaved checker materials behind.
    setp(ui, "import_materials", False)
    setp(ui, "import_textures", False)
    setp(ui, "automated_import_should_detect_type", False)
    d = ui.get_editor_property("static_mesh_import_data")
    setp(d, "combine_meshes", True)
    setp(d, "auto_generate_collision", not is_terrain)
    try:  # rocks, logs and stumps carry a LOD1 (the property was renamed in 5.8)
        d.set_editor_property("import_mesh_lods", True)
    except Exception:  # noqa
        setp(d, "import_mesh_lo_ds", True)
    setp(d, "convert_scene", True)
    setp(d, "convert_scene_unit", True)  # Blender metres -> cm
    setp(d, "import_uniform_scale", 1.0)
    if is_terrain:  # the terrain's vertex colours say which surface is where
        setp(d, "vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    return ui


def import_fbx(path, dest, is_terrain):
    if is_terrain:
        # Re-importing over the old terrain keeps its old material slots, so start from scratch.
        old = "%s/%s" % (dest, os.path.splitext(os.path.basename(path))[0])
        if eal.does_asset_exist(old) and not eal.delete_asset(old):
            log("WARNING could not delete %s; its material slots may be stale" % old)
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = fbx_options(is_terrain)
    assets.import_asset_tasks([task])
    return list(task.imported_object_paths or [])


def finish_mesh(obj_path, is_terrain):
    mesh = eal.load_asset(obj_path)
    if not isinstance(mesh, unreal.StaticMesh):
        if isinstance(mesh, unreal.Texture2D):
            mesh.set_editor_property("max_texture_size", MAX_TEXTURE)
            eal.save_loaded_asset(mesh)
        return
    if is_terrain:
        # Walk and shoot on the real triangles, not a box.
        body = mesh.get_editor_property("body_setup")
        if body:
            setp(body, "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        # Nanite keeps the 210k-triangle ground cheap and gives Lumen a good surface to light.
        try:
            ns = mesh.get_editor_property("nanite_settings")
            ns.enabled = True
            mesh.set_editor_property("nanite_settings", ns)
        except Exception as e:  # noqa
            log("nanite not set on terrain: %s" % e)
    eal.save_loaded_asset(mesh)


def main():
    use_legacy_fbx()
    for folder in PLANT_FOLDERS:
        if not eal.does_directory_exist(folder):
            eal.make_directory(folder)
            log("made folder " + folder)

    imported = []
    for pattern, dest, is_terrain in SOURCES:
        files = sorted(f for f in glob.glob(pattern)
                       if dest != "/Game/Range/Props" or os.sep + "forest" + os.sep not in f)
        if not files:
            log("nothing to import for " + pattern)
            continue
        for f in files:
            for p in import_fbx(f, dest, is_terrain):
                finish_mesh(p, is_terrain)
                imported.append(p)

    log("imported %d assets:" % len(imported))
    for p in sorted(imported):
        log("  " + p)


main()
