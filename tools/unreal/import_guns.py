"""Import the Blender gun exports into /Game/Guns/<GunId>/.

Run headless (gun id optional; no id = all guns):
  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="<path>/import_guns.py Pistol_M9" -unattended -nop4 -nosplash -NullRHI
The gun id can also come from the HOLDOVER_GUN environment variable.

Source files: <repo parent>/art/weapons/<folder>/export/
  SK_<GunId>.fbx, SM_<GunId>_Mag.fbx, SM_Round_<cal>.fbx, SM_Casing_<cal>.fbx,
  T_<GunId>_BaseColor.png, T_<GunId>_ORM.png (R=AO, G=Roughness, B=Metallic), T_<GunId>_Normal.png
Exports are forward X, up Z, 1 Blender m = 100 UU.
"""
import os
import sys

import unreal

GUNS = {
    # GunId: (art folder, caliber)
    "Pistol_M9": ("pistol_m9", "9mm"),
    "Pistol_1911": ("pistol_1911", "45ACP"),
    "Rifle_AK": ("rifle_ak", "762x39"),
    "Rifle_FAL": ("rifle_fal", "762x51"),
    "Carbine_AR": ("carbine_ar", "556"),
}
REQUIRED_BONES = ["Muzzle", "GripHand", "SightRear", "SightFront"]
MASTER = "/Game/Guns/M_Gun"
MAX_TEXTURE = 2048  # project rule: 2K max
FORCE_FRONT_X = os.environ.get("HOLDOVER_FRONT_X", "0") == "1"

HERE = os.path.dirname(os.path.abspath(__file__))
ART_ROOT = os.environ.get(
    "HOLDOVER_ART",
    os.path.normpath(os.path.join(HERE, "..", "..", "..", "art", "weapons")),
)
if not os.path.isdir(ART_ROOT):
    ART_ROOT = r"D:\Claude area\Holdover\art\weapons"

assets = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[import_guns] " + msg)


def use_legacy_fbx():
    # Interchange ignores FbxImportUI options; use the classic FBX importer so the options below apply.
    for cvar in ("Interchange.FeatureFlags.Import.FBX", "Interchange.FeatureFlags.Import.FBX.ToLevel"):
        try:
            unreal.SystemLibrary.execute_console_command(None, cvar + " 0")
        except Exception as e:  # noqa
            log("could not set %s: %s" % (cvar, e))


def run_task(filename, dest, options=None, name=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = dest
    if name:
        task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    if options is not None:
        task.options = options
    assets.import_asset_tasks([task])
    paths = list(task.imported_object_paths or [])
    log("imported %s -> %s" % (os.path.basename(filename), paths))
    return paths


def setp(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as e:  # noqa
        log("option %s.%s not set: %s" % (type(obj).__name__, name, e))


def fbx_options(skeletal):
    ui = unreal.FbxImportUI()
    setp(ui, "import_mesh", True)
    setp(ui, "import_as_skeletal", skeletal)
    setp(ui, "mesh_type_to_import",
         unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else unreal.FBXImportType.FBXIT_STATIC_MESH)
    setp(ui, "import_animations", False)
    setp(ui, "import_materials", False)
    setp(ui, "import_textures", False)
    setp(ui, "create_physics_asset", False)
    setp(ui, "automated_import_should_detect_type", False)
    d = ui.get_editor_property("skeletal_mesh_import_data" if skeletal else "static_mesh_import_data")
    if skeletal:
        setp(d, "import_morph_targets", False)
        setp(d, "use_t0_as_ref_pose", True)
    else:
        setp(d, "combine_meshes", True)
        setp(d, "auto_generate_collision", True)
    setp(d, "convert_scene", True)
    setp(d, "force_front_x_axis", FORCE_FRONT_X)
    setp(d, "convert_scene_unit", True)  # Blender exports are in metres -> cm
    setp(d, "import_uniform_scale", 1.0)
    return ui


def import_texture(path, dest, kind):
    name = os.path.splitext(os.path.basename(path))[0]
    run_task(path, dest, None, name)
    tex = eal.load_asset(dest + "/" + name)
    if not tex:
        log("ERROR texture missing after import: " + name)
        return None
    if kind == "Normal":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif kind == "ORM":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)
    tex.set_editor_property("max_texture_size", MAX_TEXTURE)
    eal.save_loaded_asset(tex)
    return tex


def set_usages(mat):
    # Without these flags the gun (skeletal mesh, First Person rendered) falls back to the gray default material.
    wanted = [n for n in dir(unreal.MaterialUsage) if n.startswith("MATUSAGE_") and any(
        k in n for k in ("SKELETAL_MESH", "FIRST_PERSON", "STATIC_MESH", "MORPH_TARGETS"))]
    log("material usages set: " + ", ".join(wanted))
    for n in wanted:
        mel.set_material_usage(mat, getattr(unreal.MaterialUsage, n))
    for prop in ("used_with_skeletal_mesh", "used_with_first_person"):
        try:
            mat.set_editor_property(prop, True)
        except Exception:  # noqa
            pass


def ensure_master():
    if eal.does_asset_exist(MASTER):
        mat = eal.load_asset(MASTER)
        set_usages(mat)
        mel.recompile_material(mat)
        eal.save_loaded_asset(mat)
        return mat
    mat = assets.create_asset("M_Gun", "/Game/Guns", unreal.Material, unreal.MaterialFactoryNew())
    TP = unreal.MaterialExpressionTextureSampleParameter2D

    base = mel.create_material_expression(mat, TP, -600, -200)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

    orm = mel.create_material_expression(mat, TP, -600, 100)
    orm.set_editor_property("parameter_name", "ORM")
    orm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)

    nrm = mel.create_material_expression(mat, TP, -600, 400)
    nrm.set_editor_property("parameter_name", "Normal")
    nrm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)

    MP = unreal.MaterialProperty
    mel.connect_material_property(base, "RGB", MP.MP_BASE_COLOR)
    mel.connect_material_property(orm, "R", MP.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(orm, "G", MP.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", MP.MP_METALLIC)
    mel.connect_material_property(nrm, "RGB", MP.MP_NORMAL)
    set_usages(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("created master material " + MASTER)
    return mat


def fix_master_defaults(mat, textures):
    # Each texture parameter's default must match its sampler type (color / masks / normal). The engine's
    # DefaultTexture is a color texture, so on the ORM and Normal slots M_Gun failed to compile and every gun
    # rendered with the gray default material. Use this gun's own textures as the defaults.
    MP = unreal.MaterialProperty
    slots = {"BaseColor": MP.MP_BASE_COLOR, "ORM": MP.MP_ROUGHNESS, "Normal": MP.MP_NORMAL}
    changed = False
    for param, prop in slots.items():
        tex = textures.get(param)
        if not tex:
            continue
        try:
            node = mel.get_material_property_input_node(mat, prop)
        except Exception as e:  # noqa
            log("WARNING cannot read M_Gun %s node: %s" % (param, e))
            continue
        if node and node.get_editor_property("texture") != tex:
            node.set_editor_property("texture", tex)
            changed = True
    if changed:
        mel.recompile_material(mat)
        eal.save_loaded_asset(mat)
        log("M_Gun parameter defaults set to " + ", ".join(t.get_name() for t in textures.values() if t))


def make_instance(gun, dest, textures):
    name = "MI_" + gun
    path = dest + "/" + name
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset(
        name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    master = ensure_master()
    fix_master_defaults(master, textures)
    mel.set_material_instance_parent(mi, master)
    for param, tex in textures.items():
        if tex:
            mel.set_material_instance_texture_parameter_value(mi, param, tex)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def assign(mesh, mi):
    mats = list(mesh.get_editor_property("materials" if isinstance(mesh, unreal.SkeletalMesh) else "static_materials"))
    for m in mats:
        m.set_editor_property("material_interface", mi)
    mesh.set_editor_property("materials" if isinstance(mesh, unreal.SkeletalMesh) else "static_materials", mats)
    eal.save_loaded_asset(mesh)
    return len(mats)


def verify(gun, sk):
    try:
        b = sk.get_bounds()
        ext = b.box_extent
        log("%s bounds size cm: X=%.1f Y=%.1f Z=%.1f (origin %s)" % (
            gun, ext.x * 2, ext.y * 2, ext.z * 2, b.origin))
    except Exception as e:  # noqa
        log("%s bounds unavailable: %s" % (gun, e))
    try:
        skel = sk.get_editor_property("skeleton")
        pose = unreal.AnimPoseExtensions.get_reference_pose(skel)
        names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
        log("%s bones: %s" % (gun, ", ".join(names)))
        missing = [n for n in REQUIRED_BONES if n not in names]
        log("%s required bones %s" % (gun, "OK" if not missing else "MISSING " + ", ".join(missing)))
        locs = {}
        for n in REQUIRED_BONES:
            if n in names:
                t = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.WORLD)
                locs[n] = t.translation
                log("%s %s at (%.1f, %.1f, %.1f)" % (gun, n, t.translation.x, t.translation.y, t.translation.z))
        if "SightFront" in locs and "SightRear" in locs:
            v = locs["SightFront"] - locs["SightRear"]
            axis = max((abs(v.x), "X"), (abs(v.y), "Y"), (abs(v.z), "Z"))[1]
            sign = "+" if getattr(v, axis.lower()) > 0 else "-"
            log("%s orientation: rear->front sight along %s%s %s (vector %.1f, %.1f, %.1f)" % (
                gun, sign, axis, "(good)" if sign + axis == "+X" else "(WRONG)", v.x, v.y, v.z))
        if "SightFront" in locs and "SightRear" in locs and "Muzzle" in locs:
            up = (locs["SightFront"].z + locs["SightRear"].z) * 0.5 > locs["Muzzle"].z
            log("%s sights %s the bore" % (gun, "above (good)" if up else "below (WRONG: upside down?)"))
    except Exception as e:  # noqa
        log("%s bone check failed: %s" % (gun, e))


def import_gun(gun):
    folder, cal = GUNS[gun]
    src = os.path.join(ART_ROOT, folder, "export")
    dest = "/Game/Guns/" + gun
    log("=== %s from %s" % (gun, src))
    # Re-importing over an existing mesh keeps its old import settings, so start the gun folder clean.
    if eal.does_directory_exist(dest):
        eal.delete_directory(dest)
        log("cleared old " + dest)

    textures = {}
    for kind in ("BaseColor", "ORM", "Normal"):
        p = os.path.join(src, "T_%s_%s.png" % (gun, kind))
        textures[kind] = import_texture(p, dest, kind) if os.path.isfile(p) else None
        if not textures[kind]:
            log("WARNING no %s texture for %s" % (kind, gun))
    mi = make_instance(gun, dest, textures)

    sk_file = os.path.join(src, "SK_%s.fbx" % gun)
    run_task(sk_file, dest, fbx_options(True), "SK_" + gun)
    sk = eal.load_asset(dest + "/SK_" + gun)
    if isinstance(sk, unreal.SkeletalMesh):
        log("SK_%s: %d material slots set to MI_%s" % (gun, assign(sk, mi), gun))
        verify(gun, sk)
    else:
        log("ERROR SK_%s did not import as a skeletal mesh (%s)" % (gun, type(sk).__name__))

    for name in ("SM_%s_Mag" % gun, "SM_Round_%s" % cal, "SM_Casing_%s" % cal):
        f = os.path.join(src, name + ".fbx")
        if not os.path.isfile(f):
            log("WARNING missing file " + f)
            continue
        run_task(f, dest, fbx_options(False), name)
        sm = eal.load_asset(dest + "/" + name)
        if isinstance(sm, unreal.StaticMesh):
            n = assign(sm, mi)
            ext = sm.get_bounds().box_extent
            log("%s: %d slots, size cm X=%.2f Y=%.2f Z=%.2f" % (name, n, ext.x * 2, ext.y * 2, ext.z * 2))
        else:
            log("ERROR %s did not import as a static mesh (%s)" % (name, type(sm).__name__))

    eal.save_directory(dest, only_if_is_dirty=False, recursive=True)
    log("=== %s done" % gun)


def main():
    use_legacy_fbx()
    args = [a for a in sys.argv[1:] if a in GUNS]
    env = os.environ.get("HOLDOVER_GUN", "")
    if not args and env:
        args = [g for g in env.split(",") if g in GUNS]
    guns = args or list(GUNS)
    for gun in guns:
        import_gun(gun)
    log("ALL DONE: " + ", ".join(guns))


main()
