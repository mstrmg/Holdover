"""Give the imported range props, forest kit and Blender terrain real materials.

Run after import_range.py (editor closed), with forward slashes in the script path:
  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="D:/Claude area/Holdover/Holdover-forest/tools/unreal/make_range_materials.py" -unattended -nop4 -nosplash -NullRHI

What it does:
1. Imports the CC0 ambientCG textures the Blender props use (art/textures/<Id>/) into /Game/Range/Textures/<Id>/.
2. Makes two master materials in /Game/Range/Materials: M_RangeSurface (colour, roughness, normal,
   metalness textures with tint and tiling) and M_RangeFlat (plain colour).
3. Makes one material instance per Blender material (MI_Wood_Weathered, MI_Sign_Text...) and
   five ground instances (MI_ForestFloor, MI_LaneGrass, MI_Gravel, MI_DirtPath, MI_BermEarth in
   /Game/Range/Forest/Ground) used by the game's own ground when there is no Blender terrain.
4. Makes M_RangeGround, the Blender terrain's material: it blends the five ground surfaces by the
   terrain's vertex colours (R grass, G gravel, B dirt, A berm earth, rest forest floor), breaks the
   borders up with noise so they are not straight lines, and mixes in a large-scale copy of each
   texture, a turned copy, and slow light/dark patches so the 2 m texture repeat is hard to see. Its instance is
   MI_RangeGround in /Game/Range/Forest/Ground (the game picks it up by the terrain's slot name).
5. Assigns them to every static mesh under /Game/Range by material slot name.

Megascans surfaces can replace the ground textures later: change the layer textures on MI_RangeGround.
"""
import os
import re

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.environ.get("HOLDOVER_ART", os.path.normpath(os.path.join(HERE, "..", "..", "..", "art")))
if not os.path.isdir(ART):
    ART = r"D:\Claude area\Holdover\art"
TEX_ROOT = os.path.join(ART, "textures")
MAX_TEXTURE = 2048

MAT_DIR = "/Game/Range/Materials"
TEX_DIR = "/Game/Range/Textures"
GROUND_DIR = "/Game/Range/Forest/Ground"

# Blender material name -> (ambientCG id, tint (linear), tiling, metal scale)
TEXTURED = {
    "Wood_Weathered": ("Wood049", (1.0, 1.0, 1.0), 1.0, 0.0),
    "Steel_Plate": ("Metal038", (1.0, 1.0, 1.0), 1.0, 1.0),
    "Rock_Mossy": ("Rock063", (0.8, 0.85, 2.0), 1.0, 0.0),  # Rock063 averages olive-yellow; pulled toward grey stone (v6 read gold)
    "Ground_Leaves": ("ScatteredLeaves009", (1.0, 1.0, 1.0), 1.0, 0.0),
    "Cardboard": ("Cardboard002", (1.0, 1.0, 1.0), 1.0, 0.0),
    "Bark_Hardwood": ("Bark014", (1.0, 1.0, 1.0), 1.0, 0.0),
}
# Blender material name -> (base colour (linear), roughness, metallic). Values from range_props.blend.
FLAT = {
    "Sign_White": ((0.80, 0.80, 0.78), 0.6, 0.0),
    "Sign_Text": ((0.02, 0.02, 0.02), 0.7, 0.0),
    "Sign_Red": ((0.55, 0.04, 0.04), 0.6, 0.0),
    "Sign_Yellow": ((0.75, 0.60, 0.05), 0.6, 0.0),
    "Rubber_Mat": ((0.05, 0.06, 0.05), 0.95, 0.0),
    "Plastic_Bin": ((0.05, 0.20, 0.07), 0.5, 0.0),
    "Paint_Olive": ((0.13, 0.16, 0.08), 0.55, 0.4),
    "Clip_Black": ((0.02, 0.02, 0.02), 0.4, 0.6),
    "Chain_Steel": ((0.20, 0.20, 0.20), 0.5, 0.9),
    "Canvas_Sandbag": ((0.42, 0.36, 0.24), 0.95, 0.0),
    "Paper_Target": ((0.80, 0.79, 0.74), 0.9, 0.0),
    "Paper_NoShoot": ((0.80, 0.79, 0.74), 0.9, 0.0),
}
# Terrain surfaces (slot names of SM_ForestTerrain) -> (ambientCG id, tint, tiling). Terrain UVs repeat every 2 m.
GROUND = {
    # Tints keep the ground's brightness near real soil and grass (v3 read pale, nearly white in full sun).
    "ForestFloor": ("ScatteredLeaves009", (0.55, 0.55, 0.48), 1.0),  # last year's leaves, darkened for shade
    "LaneGrass": ("Ground037", (0.27, 0.38, 0.25), 1.0),             # short mown grass, kept dull (v2 lime, v5 pale, v9 yellow)
    "Gravel": ("Gravel042", (0.62, 0.62, 0.60), 1.5),
    "DirtPath": ("Ground068", (0.70, 0.66, 0.62), 1.0),
    "BermEarth": ("Ground076", (0.60, 0.54, 0.48), 0.8),
}

assets = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty


def log(msg):
    unreal.log("[range_materials] " + msg)


def ensure_dir(path):
    if not eal.does_directory_exist(path):
        eal.make_directory(path)


# ---------------------------------------------------------------- textures
_tex_cache = {}


def texture(acg_id, kind):
    """kind: Color, Roughness, NormalGL, Metalness."""
    key = (acg_id, kind)
    if key in _tex_cache:
        return _tex_cache[key]
    name = "T_%s_%s" % (acg_id, kind)
    dest = "%s/%s" % (TEX_DIR, acg_id)
    path = "%s/%s" % (dest, name)
    tex = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if not tex:
        src = os.path.join(TEX_ROOT, acg_id, "%s_1K-JPG_%s.jpg" % (acg_id, kind))
        if not os.path.isfile(src):
            _tex_cache[key] = None
            return None
        ensure_dir(dest)
        task = unreal.AssetImportTask()
        task.filename = src
        task.destination_path = dest
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        assets.import_asset_tasks([task])
        tex = eal.load_asset(path)
        if not tex:
            log("ERROR could not import " + src)
            _tex_cache[key] = None
            return None
    if kind == "NormalGL":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)  # OpenGL -> DirectX normal
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif kind in ("Roughness", "Metalness"):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)
    tex.set_editor_property("max_texture_size", MAX_TEXTURE)
    eal.save_loaded_asset(tex)
    _tex_cache[key] = tex
    return tex


# ---------------------------------------------------------------- masters
def set_usages(mat):
    for n in dir(unreal.MaterialUsage):
        if n.startswith("MATUSAGE_") and any(k in n for k in ("INSTANCED_STATIC_MESHES", "NANITE", "STATIC_LIGHTING")):
            try:
                mel.set_material_usage(mat, getattr(unreal.MaterialUsage, n))
            except Exception as e:  # noqa
                log("usage %s not set: %s" % (n, e))


def existing(name):
    path = "%s/%s" % (MAT_DIR, name)
    return eal.load_asset(path) if eal.does_asset_exist(path) else None


def new_material(name):
    ensure_dir(MAT_DIR)
    return assets.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())


def make_surface_master(defaults):
    # Kept if it already exists (instances point at it); delete it by hand to rebuild the graph.
    mat = existing("M_RangeSurface")
    if mat:
        return mat
    mat = new_material("M_RangeSurface")
    E = mel.create_material_expression
    TP = unreal.MaterialExpressionTextureSampleParameter2D

    uv = E(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    tiling = E(mat, unreal.MaterialExpressionScalarParameter, -1400, 150)
    tiling.set_editor_property("parameter_name", "Tiling")
    tiling.set_editor_property("default_value", 1.0)
    uvmul = E(mat, unreal.MaterialExpressionMultiply, -1200, 50)
    mel.connect_material_expressions(uv, "", uvmul, "A")
    mel.connect_material_expressions(tiling, "", uvmul, "B")

    def sampler(param, stype, tex, y):
        s = E(mat, TP, -900, y)
        s.set_editor_property("parameter_name", param)
        s.set_editor_property("sampler_type", stype)
        s.set_editor_property("texture", tex)  # default must match the sampler type or the material fails
        mel.connect_material_expressions(uvmul, "", s, "UVs")
        return s

    ST = unreal.MaterialSamplerType
    col = sampler("BaseColor", ST.SAMPLERTYPE_COLOR, defaults["Color"], -400)
    rough = sampler("Roughness", ST.SAMPLERTYPE_MASKS, defaults["Roughness"], -50)
    metal = sampler("Metalness", ST.SAMPLERTYPE_MASKS, defaults["Metalness"], 300)
    nrm = sampler("Normal", ST.SAMPLERTYPE_NORMAL, defaults["NormalGL"], 650)

    tint = E(mat, unreal.MaterialExpressionVectorParameter, -900, -650)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    tinted = E(mat, unreal.MaterialExpressionMultiply, -500, -450)
    mel.connect_material_expressions(col, "RGB", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")

    mscale = E(mat, unreal.MaterialExpressionScalarParameter, -900, 500)
    mscale.set_editor_property("parameter_name", "MetalScale")
    mscale.set_editor_property("default_value", 0.0)
    metal_out = E(mat, unreal.MaterialExpressionMultiply, -500, 320)
    mel.connect_material_expressions(metal, "R", metal_out, "A")
    mel.connect_material_expressions(mscale, "", metal_out, "B")

    mel.connect_material_property(tinted, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(rough, "R", MP.MP_ROUGHNESS)
    mel.connect_material_property(metal_out, "", MP.MP_METALLIC)
    mel.connect_material_property(nrm, "RGB", MP.MP_NORMAL)
    set_usages(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def make_flat_master():
    mat = existing("M_RangeFlat")
    if mat:
        return mat
    mat = new_material("M_RangeFlat")
    E = mel.create_material_expression
    col = E(mat, unreal.MaterialExpressionVectorParameter, -500, -100)
    col.set_editor_property("parameter_name", "BaseColor")
    col.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1))
    rough = E(mat, unreal.MaterialExpressionScalarParameter, -500, 100)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.6)
    metal = E(mat, unreal.MaterialExpressionScalarParameter, -500, 250)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.0)
    mel.connect_material_property(col, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", MP.MP_METALLIC)
    set_usages(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


# ---------------------------------------------------------------- blended terrain
# Layer order in M_RangeGround: forest floor is the base, the others are laid over it by vertex colour.
GROUND_LAYERS = [  # (layer name, GROUND key, vertex colour channel or None for the base)
    ("Forest", "ForestFloor", None),
    ("Earth", "BermEarth", "A"),
    ("Grass", "LaneGrass", "R"),
    ("Dirt", "DirtPath", "B"),
    ("Gravel", "Gravel", "G"),
]


def make_ground_master():
    """M_RangeGround: five-surface vertex-colour blend with broken edges and anti-tiling. Rebuilt every run."""
    mat = existing("M_RangeGround")
    if mat:
        mel.delete_all_material_expressions(mat)  # rebuild the graph in place; MI_RangeGround keeps pointing at it
    else:
        mat = new_material("M_RangeGround")
    E = mel.create_material_expression
    con = mel.connect_material_expressions
    ST = unreal.MaterialSamplerType
    shared = unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS  # 20 textures, past the 16-sampler limit
    X = [-3000]  # running column for tidy layout

    def scalar(name, value, x, y):
        e = E(mat, unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e

    def op(cls, a, b=None, x=0, y=0, const_b=None, a_out="", b_out=""):
        e = E(mat, cls, x, y)
        con(a, a_out, e, "A")
        if b is not None:
            con(b, b_out, e, "B")
        elif const_b is not None:
            e.set_editor_property("const_b", const_b)
        return e

    def one_input(cls, src, x, y, src_out=""):
        e = E(mat, cls, x, y)
        con(src, src_out, e, "")
        return e

    def lerp(a, b, alpha, x, y, a_out="", b_out=""):
        e = E(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
        con(a, a_out, e, "A")
        con(b, b_out, e, "B")
        con(alpha, "", e, "Alpha")
        return e

    # World position in metres (x, y) feeds the noises.
    wp = E(mat, unreal.MaterialExpressionWorldPosition, -3000, -1400)
    wxy = E(mat, unreal.MaterialExpressionComponentMask, -2800, -1400)
    wxy.set_editor_property("r", True)
    wxy.set_editor_property("g", True)
    wxy.set_editor_property("b", False)
    wxy.set_editor_property("a", False)
    con(wp, "", wxy, "")
    metres = op(unreal.MaterialExpressionDivide, wxy, x=-2600, y=-1400, const_b=100.0)

    def noise(scale_m, levels, y):
        n = E(mat, unreal.MaterialExpressionNoise, -2200, y)
        n.set_editor_property("scale", 1.0 / scale_m)
        n.set_editor_property("levels", levels)
        n.set_editor_property("output_min", 0.0)
        n.set_editor_property("output_max", 1.0)
        n.set_editor_property("quality", 1)
        con(metres, "", n, "Position")
        return n

    edge_noise = noise(0.9, 3, -1600)    # breaks surface borders up
    macro_noise = noise(7.0, 2, -1300)   # slow light/dark patches and the far-texture mix
    swap_noise = noise(3.5, 2, -1750)    # where the turned copy of each texture takes over

    edge_amount = scalar("EdgeNoise", 0.7, -2200, -1900)
    contrast = scalar("EdgeSharpness", 3.0, -2200, -1800)
    macro_mix = scalar("FarTextureMix", 0.6, -2200, -1150)
    macro_dark = scalar("PatchDarkness", 0.32, -2200, -1050)
    far_scale = scalar("FarTextureScale", 0.21, -2200, -950)

    # Alpha for a layer: saturate((w + (n - 0.5) * EdgeNoise - 0.5) * EdgeSharpness + 0.5)
    centred = op(unreal.MaterialExpressionSubtract, edge_noise, x=-1900, y=-1600, const_b=0.5)
    jitter = op(unreal.MaterialExpressionMultiply, centred, edge_amount, x=-1700, y=-1600)
    vc = E(mat, unreal.MaterialExpressionVertexColor, -1900, -2000)

    def alpha(channel, y):
        a = op(unreal.MaterialExpressionAdd, vc, jitter, x=-1500, y=y, a_out=channel)
        a = op(unreal.MaterialExpressionSubtract, a, x=-1350, y=y, const_b=0.5)
        a = op(unreal.MaterialExpressionMultiply, a, contrast, x=-1200, y=y)
        a = op(unreal.MaterialExpressionAdd, a, x=-1050, y=y, const_b=0.5)
        return one_input(unreal.MaterialExpressionSaturate, a, -900, y)

    uv = E(mat, unreal.MaterialExpressionTextureCoordinate, -3000, 0)
    far_mix = op(unreal.MaterialExpressionMultiply, macro_noise, macro_mix, x=-1900, y=-1150)
    # Brightness patches: 1 - PatchDarkness * noise
    dark = op(unreal.MaterialExpressionMultiply, macro_noise, macro_dark, x=-1900, y=-1050)
    patches = one_input(unreal.MaterialExpressionOneMinus, dark, -1700, -1050)
    # A turned, slightly rescaled copy of each colour texture, blended in patches: breaks the rows a
    # single repeating texture makes on big open areas like the backstop.
    one = E(mat, unreal.MaterialExpressionConstant, -2800, -700)
    one.set_editor_property("r", 1.0)
    swap = op(unreal.MaterialExpressionSubtract, swap_noise, x=-1900, y=-1750, const_b=0.3)
    swap = op(unreal.MaterialExpressionMultiply, swap, x=-1750, y=-1750, const_b=2.5)
    swap = one_input(unreal.MaterialExpressionSaturate, swap, -1600, -1750)

    layers = []
    for i, (lname, gkey, channel) in enumerate(GROUND_LAYERS):
        acg, tint, tiling = GROUND[gkey]
        y0 = i * 700
        til = scalar(lname + "_Tiling", tiling, -2800, y0)
        luv = op(unreal.MaterialExpressionMultiply, uv, til, x=-2600, y=y0)
        fuv = op(unreal.MaterialExpressionMultiply, luv, far_scale, x=-2600, y=y0 + 120)

        def sampler(param, stype, tex, uvs, y, _l=lname):
            s = E(mat, unreal.MaterialExpressionTextureSampleParameter2D, -2300, y)
            s.set_editor_property("parameter_name", "%s_%s" % (_l, param))
            s.set_editor_property("sampler_type", stype)
            s.set_editor_property("sampler_source", shared)
            s.set_editor_property("texture", tex)
            con(uvs, "", s, "UVs")
            return s

        tuv = op(unreal.MaterialExpressionMultiply, luv, x=-2600, y=y0 + 240, const_b=0.87)
        rot = E(mat, unreal.MaterialExpressionRotator, -2450, y0 + 240)
        rot.set_editor_property("speed", 1.1 + 0.37 * i)  # fixed angle: Time is a constant 1, not game time
        con(tuv, "", rot, "Coordinate")
        con(one, "", rot, "Time")
        col_a = sampler("Color", ST.SAMPLERTYPE_COLOR, texture(acg, "Color"), luv, y0)
        col_b = sampler("ColorTurned", ST.SAMPLERTYPE_COLOR, texture(acg, "Color"), rot, y0 + 60)
        col_near = lerp(col_a, col_b, swap, -2050, y0, a_out="RGB", b_out="RGB")
        col_far = sampler("ColorFar", ST.SAMPLERTYPE_COLOR, texture(acg, "Color"), fuv, y0 + 150)
        rough = sampler("Roughness", ST.SAMPLERTYPE_MASKS, texture(acg, "Roughness"), luv, y0 + 300)
        nrm = sampler("Normal", ST.SAMPLERTYPE_NORMAL, texture(acg, "NormalGL"), luv, y0 + 450)
        tint_p = E(mat, unreal.MaterialExpressionVectorParameter, -2000, y0 - 100)
        tint_p.set_editor_property("parameter_name", lname + "_Tint")
        tint_p.set_editor_property("default_value", unreal.LinearColor(tint[0], tint[1], tint[2], 1))
        col = lerp(col_near, col_far, far_mix, -1900, y0, b_out="RGB")
        col = op(unreal.MaterialExpressionMultiply, col, tint_p, x=-1700, y=y0)
        layers.append((col, rough, nrm, alpha(channel, y0 + 200) if channel else None))

    base_col, base_rough, base_nrm, _ = layers[0]
    col, rough, nrm = base_col, base_rough, base_nrm
    rough_out, nrm_out = "R", "RGB"
    for i, (lc, lr, ln, la) in enumerate(layers[1:], start=1):
        x = -700 + i * 120
        col = lerp(col, lc, la, x, -400)
        rough = lerp(rough, lr, la, x, 0, a_out=rough_out, b_out="R")
        nrm = lerp(nrm, ln, la, x, 400, a_out=nrm_out, b_out="RGB")
        rough_out, nrm_out = "", ""
    col = op(unreal.MaterialExpressionMultiply, col, patches, x=0, y=-400)

    # Soil and grass are rough and only faintly shiny: without a floor and lower specular the lane turned
    # pale cream toward the horizon (sky reflected at a grazing angle, v6).
    rough_floor = scalar("RoughnessFloor", 0.8, 200, 0)
    rough = op(unreal.MaterialExpressionMax, rough, rough_floor, x=400, y=0, a_out=rough_out)
    spec = scalar("Specular", 0.25, 400, 200)

    mel.connect_material_property(col, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", MP.MP_SPECULAR)
    mel.connect_material_property(nrm, "", MP.MP_NORMAL)
    set_usages(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def ground_instance(master):
    mi = instance("MI_RangeGround", GROUND_DIR, master)
    for lname, gkey, _ in GROUND_LAYERS:
        acg, tint, tiling = GROUND[gkey]
        for param, kind in (("Color", "Color"), ("ColorTurned", "Color"), ("ColorFar", "Color"),
                            ("Roughness", "Roughness"), ("Normal", "NormalGL")):
            tex = texture(acg, kind)
            if tex:
                mel.set_material_instance_texture_parameter_value(mi, "%s_%s" % (lname, param), tex)
        mel.set_material_instance_vector_parameter_value(mi, lname + "_Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1))
        mel.set_material_instance_scalar_parameter_value(mi, lname + "_Tiling", tiling)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def instance(name, folder, parent):
    ensure_dir(folder)
    path = "%s/%s" % (folder, name)
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset(
        name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    return mi


def textured_instance(name, folder, master, acg_id, tint, tiling, metal_scale):
    mi = instance(name, folder, master)
    for param, kind in (("BaseColor", "Color"), ("Roughness", "Roughness"), ("Normal", "NormalGL"), ("Metalness", "Metalness")):
        tex = texture(acg_id, kind)
        if tex:
            mel.set_material_instance_texture_parameter_value(mi, param, tex)
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1))
    mel.set_material_instance_scalar_parameter_value(mi, "Tiling", tiling)
    mel.set_material_instance_scalar_parameter_value(mi, "MetalScale", metal_scale if texture(acg_id, "Metalness") else 0.0)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def flat_instance(name, folder, master, color, rough, metal):
    mi = instance(name, folder, master)
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(color[0], color[1], color[2], 1))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def slot_key(name):
    return re.sub(r"([._]\d+)+$", "", str(name))


def main():
    defaults = {k: texture("Metal038", k) for k in ("Color", "Roughness", "NormalGL", "Metalness")}
    if not all(defaults.values()):
        log("ERROR missing Metal038 textures in " + TEX_ROOT)
        return
    surface = make_surface_master(defaults)
    flat = make_flat_master()

    by_slot = {}
    for name, (acg, tint, tiling, metal) in TEXTURED.items():
        by_slot[name] = textured_instance("MI_" + name, MAT_DIR, surface, acg, tint, tiling, metal)
    for name, (color, rough, metal) in FLAT.items():
        by_slot[name] = flat_instance("MI_" + name, MAT_DIR, flat, color, rough, metal)
    for name, (acg, tint, tiling) in GROUND.items():
        by_slot[name] = textured_instance("MI_" + name, GROUND_DIR, surface, acg, tint, tiling, 0.0)
    if all(texture(GROUND[k][0], kind) for _, k, _ in GROUND_LAYERS for kind in ("Color", "Roughness", "NormalGL")):
        by_slot["RangeGround"] = ground_instance(make_ground_master())
    else:
        log("ERROR missing ground textures, MI_RangeGround not made")
    log("made %d material instances" % len(by_slot))

    unmatched = set()
    for path in eal.list_assets("/Game/Range", recursive=True, include_folder=False):
        data = eal.find_asset_data(path)
        if str(data.asset_class_path.asset_name) != "StaticMesh":
            continue
        mesh = eal.load_asset(path)
        mats = list(mesh.get_editor_property("static_materials"))
        changed = 0
        for m in mats:
            key = slot_key(m.get_editor_property("material_slot_name"))
            mi = by_slot.get(key)
            if mi:
                m.set_editor_property("material_interface", mi)
                changed += 1
            else:
                unmatched.add("%s (%s)" % (key, mesh.get_name()))
        if changed:
            mesh.set_editor_property("static_materials", mats)
            eal.save_loaded_asset(mesh)
            log("%s: %d of %d slots" % (mesh.get_name(), changed, len(mats)))
    if unmatched:
        log("slots with no material: " + ", ".join(sorted(unmatched)))


main()
