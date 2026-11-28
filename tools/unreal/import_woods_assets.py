"""Import the Poly Haven models and textures (art/polyhaven, fetched by tools/range/fetch_polyhaven.py) for the woods range.

Headless, editor closed:
  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="<path>/import_woods_assets.py" -unattended -nop4 -nosplash -NullRHI

- Models (glTF) -> /Game/Range/Woods/PolyHaven/<id>/ (meshes, their textures and materials as glTF makes them).
  Every mesh gets Nanite; rocks, logs, stumps and roots get per-polygon collision so bullets and the player hit them.
- Textures -> /Game/Range/Woods/Textures/<id>/T_<id>_<map> at 2K max, normal maps converted from OpenGL.
Already-imported assets are skipped (set HOLDOVER_REIMPORT=1 to redo them).
Logs every asset as "[woods_import] ...".
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.environ.get("HOLDOVER_ART", os.path.normpath(os.path.join(HERE, "..", "..", "..", "art")))
if not os.path.isdir(ART):
    ART = r"D:\Claude area\Holdover\art"
PH = os.path.join(ART, "polyhaven")
DEST = "/Game/Range/Woods/PolyHaven"
TEX_DEST = "/Game/Range/Woods/Textures"
REIMPORT = os.environ.get("HOLDOVER_REIMPORT") == "1"
SOLID = ("boulder", "rock", "stone", "trunk", "stump", "root", "crate", "barrel")

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(m):
    unreal.log("[woods_import] " + m)


def import_file(src, dest, name=None):
    t = unreal.AssetImportTask()
    t.filename = src
    t.destination_path = dest
    if name:
        t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = True
    tools.import_asset_tasks([t])
    return list(t.imported_object_paths or [])


def setup_mesh(path, solid):
    sm = eal.load_asset(path)
    if not isinstance(sm, unreal.StaticMesh):
        return
    ns = sm.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", True)
    try:
        ns.set_editor_property("preserve_area", True)  # foliage keeps its leaves when simplified far away
    except Exception:
        pass
    sm.set_editor_property("nanite_settings", ns)
    body = sm.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag",
                                 unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if solid else unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    eal.save_loaded_asset(sm)
    b = sm.get_bounds()
    log("mesh %s  %.2f x %.2f x %.2f m  solid=%s" % (path.split(".")[0], b.box_extent.x * 0.02, b.box_extent.y * 0.02, b.box_extent.z * 0.02, solid))


def import_models():
    for asset_id in sorted(os.listdir(PH)):
        folder = os.path.join(PH, asset_id)
        if asset_id == "textures" or not os.path.isdir(folder):
            continue
        gltf = [f for f in os.listdir(folder) if f.endswith(".gltf")]
        if not gltf:
            continue
        dest = "%s/%s" % (DEST, asset_id)
        if eal.does_directory_exist(dest) and eal.list_assets(dest, recursive=True) and not REIMPORT:
            log("skip (already in): " + asset_id)
        else:
            out = import_file(os.path.join(folder, gltf[0]), dest)
            log("imported %s: %d objects" % (asset_id, len(out)))
        solid = any(k in asset_id.lower() for k in SOLID)
        for p in eal.list_assets(dest, recursive=True, include_folder=False):
            if str(eal.find_asset_data(p).asset_class_path.asset_name) == "StaticMesh":
                setup_mesh(p, solid)


MAPS = {"diffuse": "Color", "diff": "Color", "nor_gl": "Normal", "rough": "Rough", "disp": "Height", "ao": "AO"}


def import_textures():
    root = os.path.join(PH, "textures")
    for tex_id in sorted(os.listdir(root)):
        for f in sorted(os.listdir(os.path.join(root, tex_id))):
            low = f.lower()
            kind = None
            for key, k in MAPS.items():
                if ("_%s_" % key) in low:
                    kind = k
                    break
            if not kind:
                continue
            name = "T_%s_%s" % (tex_id, kind)
            dest = "%s/%s" % (TEX_DEST, tex_id)
            path = "%s/%s" % (dest, name)
            if not eal.does_asset_exist(path) or REIMPORT:
                import_file(os.path.join(root, tex_id, f), dest, name)
            tex = eal.load_asset(path)
            if not tex:
                log("ERROR texture " + path)
                continue
            if kind == "Normal":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("flip_green_channel", True)
            elif kind in ("Rough", "AO", "Height"):
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                tex.set_editor_property("srgb", False)
            tex.set_editor_property("max_texture_size", 2048)
            eal.save_loaded_asset(tex)
            log("texture " + path)


import_models()
import_textures()
log("done")
