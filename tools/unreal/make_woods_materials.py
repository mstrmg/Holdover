"""Materials for the woods range. Headless, editor closed, after import_woods_assets.py:
  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="<path>/make_woods_materials.py" -unattended -nop4 -nosplash -NullRHI

1. M_WoodsGround / MI_WoodsGround (/Game/Range/Woods/Materials): the baked ground's material. Leaf litter is the base;
   grass (vertex R), worn soil (G), gravel (B) and moss (A) are laid over it. Borders are broken by noise and by
   each texture's own height map (leaves sit on top of soil, gravel stones poke through), and every layer mixes a
   turned and a far copy of itself in patches so the repeat is hard to see.
2. M_WoodsWorld + MI_WoodsTimber / MI_WoodsRoof: textures projected in world space, for the shelter's posts,
   beams and mossy roof boards (built from stretched cubes, which have no usable UVs).
3. Ticks the usage flags (instanced static meshes, Nanite) on every material the imported Poly Haven and Fab
   plants use, so they draw in the game's instanced batches instead of the gray default.
"""
import os

import unreal

MAT = "/Game/Range/Woods/Materials"
TEX = "/Game/Range/Woods/Textures"
OLD_TEX = "/Game/Range/Textures"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
ST = unreal.MaterialSamplerType


def log(m):
    unreal.log("[woods_materials] " + m)


def tex(tid, kind):
    for p in ("%s/%s/T_%s_%s" % (TEX, tid, tid, kind), "%s/%s/T_%s_%s" % (OLD_TEX, tid, tid, kind)):
        if eal.does_asset_exist(p):
            return eal.load_asset(p)
    log("missing texture %s %s" % (tid, kind))
    return None


def material(name):
    if not eal.does_directory_exist(MAT):
        eal.make_directory(MAT)
    path = "%s/%s" % (MAT, name)
    if eal.does_asset_exist(path):
        m = eal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return tools.create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())


def instance(name, parent):
    path = "%s/%s" % (MAT, name)
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        name, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    return mi


def usages(m):
    for n in ("MATUSAGE_INSTANCED_STATIC_MESHES", "MATUSAGE_NANITE", "MATUSAGE_STATIC_LIGHTING"):
        if hasattr(unreal.MaterialUsage, n):
            mel.set_material_usage(m, getattr(unreal.MaterialUsage, n))


class Graph:
    def __init__(self, mat):
        self.m = mat

    def e(self, cls, x, y, **props):
        n = mel.create_material_expression(self.m, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def c(self, a, a_out, b, b_in):
        mel.connect_material_expressions(a, a_out, b, b_in)

    def scalar(self, name, v, x, y):
        return self.e(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)

    def vector(self, name, v, x, y):
        return self.e(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*v))

    def op(self, cls, a, b=None, x=0, y=0, cb=None, ao="", bo=""):
        n = self.e(cls, x, y)
        self.c(a, ao, n, "A")
        if b is not None:
            self.c(b, bo, n, "B")
        elif cb is not None:
            n.set_editor_property("const_b", cb)
        return n

    def one(self, cls, a, x, y, ao=""):
        n = self.e(cls, x, y)
        self.c(a, ao, n, "")
        return n

    def lerp(self, a, b, t, x, y, ao="", bo="", to=""):
        n = self.e(unreal.MaterialExpressionLinearInterpolate, x, y)
        self.c(a, ao, n, "A")
        self.c(b, bo, n, "B")
        self.c(t, to, n, "Alpha")
        return n

    def sample(self, name, kind, texture, uvs, x, y):
        stype = {"Color": ST.SAMPLERTYPE_COLOR, "Normal": ST.SAMPLERTYPE_NORMAL}.get(kind, ST.SAMPLERTYPE_MASKS)
        n = self.e(unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, sampler_type=stype,
                   sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS, texture=texture)
        self.c(uvs, "", n, "UVs")
        return n


# (layer, texture id, tiling per 2 m UV unit, tint, vertex channel or None for the base, height-blend strength)
LAYERS = [
    ("Litter", "forest_leaves_02", 1.0, (0.23, 0.22, 0.19), None, 0.0, 0.45),
    ("Grass", "leafy_grass", 1.1, (0.19, 0.27, 0.13), "R", 0.5, 0.1),
    ("Soil", "dirt_floor", 0.9, (0.30, 0.27, 0.24), "G", 0.6, 0.3),
    ("Gravel", "gravel", 1.4, (0.85, 0.84, 0.80), "B", 0.6, 0.2),
    ("Moss", "Ground068", 0.8, (0.38, 0.48, 0.28), "A", 0.4, 0.1),
]
ALT_LITTER = "forest_leaves_04"  # mixed into the leaf litter in patches


def ground_master():
    mat = material("M_WoodsGround")
    g = Graph(mat)
    wp = g.e(unreal.MaterialExpressionWorldPosition, -3400, -1600)
    wxy = g.e(unreal.MaterialExpressionComponentMask, -3200, -1600, r=True, g=True, b=False, a=False)
    g.c(wp, "", wxy, "")
    metres = g.op(unreal.MaterialExpressionDivide, wxy, x=-3000, y=-1600, cb=100.0)

    def noise(scale, levels, y):
        n = g.e(unreal.MaterialExpressionNoise, -2700, y, scale=1.0 / scale, levels=levels, output_min=0.0, output_max=1.0, quality=1)
        g.c(metres, "", n, "Position")
        return n

    edge_n = noise(0.8, 3, -1900)
    macro_n = noise(9.0, 3, -1600)
    swap_n = noise(4.0, 2, -2200)
    alt_n = noise(6.0, 2, -2500)

    vc = g.e(unreal.MaterialExpressionVertexColor, -2400, -2700)
    edge_amt = g.scalar("EdgeNoise", 0.55, -2700, -2900)
    sharp = g.scalar("EdgeSharpness", 3.5, -2700, -2800)
    far_scale = g.scalar("FarScale", 0.19, -2700, -1300)
    far_mix_p = g.scalar("FarMix", 0.55, -2700, -1200)
    dark_p = g.scalar("PatchDarkness", 0.35, -2700, -1100)
    one = g.e(unreal.MaterialExpressionConstant, -3200, -700, r=1.0)
    uv = g.e(unreal.MaterialExpressionTextureCoordinate, -3400, 0)

    centred = g.op(unreal.MaterialExpressionSubtract, edge_n, x=-2400, y=-1900, cb=0.5)
    jitter = g.op(unreal.MaterialExpressionMultiply, centred, edge_amt, x=-2250, y=-1900)
    far_mix = g.op(unreal.MaterialExpressionMultiply, macro_n, far_mix_p, x=-2400, y=-1250)
    patches = g.one(unreal.MaterialExpressionOneMinus, g.op(unreal.MaterialExpressionMultiply, macro_n, dark_p, x=-2400, y=-1100), -2250, -1100)
    swap = g.one(unreal.MaterialExpressionSaturate, g.op(unreal.MaterialExpressionMultiply,
                 g.op(unreal.MaterialExpressionSubtract, swap_n, x=-2400, y=-2200, cb=0.35), x=-2250, y=-2200, cb=2.5), -2100, -2200)
    alt = g.one(unreal.MaterialExpressionSaturate, g.op(unreal.MaterialExpressionMultiply,
                g.op(unreal.MaterialExpressionSubtract, alt_n, x=-2400, y=-2500, cb=0.45), x=-2250, y=-2500, cb=3.0), -2100, -2500)

    layers = []
    for i, (name, tid, tiling, tint, ch, hstr, desat) in enumerate(LAYERS):
        y0 = i * 900
        til = g.scalar(name + "_Tiling", tiling, -3200, y0)
        luv = g.op(unreal.MaterialExpressionMultiply, uv, til, x=-3000, y=y0)
        fuv = g.op(unreal.MaterialExpressionMultiply, luv, far_scale, x=-3000, y=y0 + 120)
        tuv = g.op(unreal.MaterialExpressionMultiply, luv, x=-3000, y=y0 + 240, cb=0.83)
        rot = g.e(unreal.MaterialExpressionRotator, -2850, y0 + 240, speed=1.3 + 0.41 * i)
        g.c(tuv, "", rot, "Coordinate")
        g.c(one, "", rot, "Time")
        ca = g.sample(name + "_Color", "Color", tex(tid, "Color"), luv, -2600, y0)
        cb_ = g.sample(name + "_ColorTurned", "Color", tex(tid, "Color"), rot, -2600, y0 + 80)
        cf = g.sample(name + "_ColorFar", "Color", tex(tid, "Color"), fuv, -2600, y0 + 160)
        rough = g.sample(name + "_Rough", "Rough", tex(tid, "Rough") or tex(tid, "Roughness"), luv, -2600, y0 + 320)
        nrm = g.sample(name + "_Normal", "Normal", tex(tid, "Normal") or tex(tid, "NormalGL"), luv, -2600, y0 + 480)
        ht_tex = tex(tid, "Height")
        height = g.sample(name + "_Height", "Height", ht_tex, luv, -2600, y0 + 640) if ht_tex else None
        col = g.lerp(ca, cb_, swap, -2350, y0, ao="RGB", bo="RGB")
        if name == "Litter" and tex(ALT_LITTER, "Color"):
            alt_c = g.sample("Litter_AltColor", "Color", tex(ALT_LITTER, "Color"), rot, -2600, y0 - 120)
            col = g.lerp(col, alt_c, alt, -2200, y0 - 60, bo="RGB")
        col = g.lerp(col, cf, far_mix, -2100, y0, bo="RGB")
        # Less saturated: lerp toward the colour's own grey (luminance).
        lumw = g.e(unreal.MaterialExpressionConstant3Vector, -2300, y0 - 340, constant=unreal.LinearColor(0.3, 0.59, 0.11, 1))
        lum = g.op(unreal.MaterialExpressionDotProduct, col, lumw, x=-2150, y=y0 - 300)
        col = g.lerp(col, lum, g.scalar(name + "_Desaturate", desat, -2300, y0 - 260), -2050, y0 - 40)
        col = g.op(unreal.MaterialExpressionMultiply, col, g.vector(name + "_Tint", tint + (1.0,), -2300, y0 - 160), x=-1950, y=y0)
        alpha = None
        if ch:
            a = g.op(unreal.MaterialExpressionAdd, vc, jitter, x=-1950, y=y0 + 300, ao=ch)
            if height is not None:
                hs = g.scalar(name + "_HeightBlend", hstr, -2300, y0 + 700)
                hc = g.op(unreal.MaterialExpressionSubtract, height, x=-2200, y=y0 + 640, cb=0.5, ao="R")
                a = g.op(unreal.MaterialExpressionAdd, a, g.op(unreal.MaterialExpressionMultiply, hc, hs, x=-2050, y=y0 + 640), x=-1800, y=y0 + 300)
            a = g.op(unreal.MaterialExpressionSubtract, a, x=-1700, y=y0 + 300, cb=0.5)
            a = g.op(unreal.MaterialExpressionMultiply, a, sharp, x=-1600, y=y0 + 300)
            a = g.op(unreal.MaterialExpressionAdd, a, x=-1500, y=y0 + 300, cb=0.5)
            alpha = g.one(unreal.MaterialExpressionSaturate, a, -1400, y0 + 300)
        layers.append((col, rough, nrm, alpha))

    col, rough, nrm, _ = layers[0]
    ro, no = "R", "RGB"
    for i, (lc, lr, ln, la) in enumerate(layers[1:], start=1):
        x = -1100 + i * 140
        col = g.lerp(col, lc, la, x, -500)
        rough = g.lerp(rough, lr, la, x, 0, ao=ro, bo="R")
        nrm = g.lerp(nrm, ln, la, x, 500, ao=no, bo="RGB")
        ro, no = "", ""
    col = g.op(unreal.MaterialExpressionMultiply, col, patches, x=-200, y=-500)
    floor = g.scalar("RoughnessFloor", 0.75, -300, 100)
    rough = g.op(unreal.MaterialExpressionMax, rough, floor, x=-100, y=0, ao=ro)
    spec = g.scalar("Specular", 0.3, -100, 200)
    mel.connect_material_property(col, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", MP.MP_SPECULAR)
    mel.connect_material_property(nrm, "", MP.MP_NORMAL)
    usages(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    mi = instance("MI_WoodsGround", mat)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    log("ground material done")


def world_master():
    """Box-projected textures in world space (cm -> metres via TileMetres)."""
    mat = material("M_WoodsWorld")
    g = Graph(mat)
    wp = g.e(unreal.MaterialExpressionWorldPosition, -1800, 0)
    size = g.scalar("TileMetres", 1.5, -1800, 150)
    sizecm = g.op(unreal.MaterialExpressionMultiply, size, x=-1650, y=150, cb=100.0)
    p = g.op(unreal.MaterialExpressionDivide, wp, sizecm, x=-1500, y=0)
    nrm_w = g.e(unreal.MaterialExpressionVertexNormalWS, -1800, 400)
    absn = g.one(unreal.MaterialExpressionAbs, nrm_w, -1650, 400)

    def mask(src, r, gg, b, x, y):
        n = g.e(unreal.MaterialExpressionComponentMask, x, y, r=r, g=gg, b=b, a=False)
        g.c(src, "", n, "")
        return n

    uv_x = mask(p, False, True, True, -1350, -200)   # face looking along X: use YZ
    uv_y = mask(p, True, False, True, -1350, 0)      # along Y: XZ
    uv_z = mask(p, True, True, False, -1350, 200)    # along Z: XY
    wx = mask(absn, True, False, False, -1350, 400)
    wy = mask(absn, False, True, False, -1350, 500)
    wz = mask(absn, False, False, True, -1350, 600)

    def tri(name, kind, y):
        t = {"Color": tex("Wood049", "Color"), "Rough": tex("Wood049", "Roughness"), "Normal": tex("Wood049", "NormalGL")}[kind]
        sx = g.sample(name, kind, t, uv_x, -1100, y)
        sy = g.sample(name, kind, t, uv_y, -1100, y + 100)
        sz = g.sample(name, kind, t, uv_z, -1100, y + 200)
        out = "RGB" if kind != "Rough" else "R"
        a = g.lerp(sx, sy, g.one(unreal.MaterialExpressionSaturate, g.op(unreal.MaterialExpressionMultiply,
                   g.op(unreal.MaterialExpressionSubtract, wy, wx, x=-950, y=y + 300), x=-850, y=y + 300, cb=6.0), -750, y + 300), -650, y, ao=out, bo=out)
        b = g.lerp(a, sz, g.one(unreal.MaterialExpressionSaturate, g.op(unreal.MaterialExpressionMultiply,
                   g.op(unreal.MaterialExpressionSubtract, wz, g.op(unreal.MaterialExpressionMax, wx, wy, x=-1000, y=y + 400), x=-950, y=y + 400), x=-850, y=y + 400, cb=6.0), -750, y + 400), -550, y, bo=out)
        return b

    col = tri("Color", "Color", -600)
    rough = tri("Rough", "Rough", 200)
    nrm = tri("Normal", "Normal", 1000)
    col = g.op(unreal.MaterialExpressionMultiply, col, g.vector("Tint", (1, 1, 1, 1), -600, -800), x=-400, y=-600)
    # Moss on surfaces that face up (roof boards, beam tops).
    moss_tex = tex("Ground068", "Color")
    if moss_tex:
        ms = g.sample("MossColor", "Color", moss_tex, uv_z, -1100, -1000)
        up = g.one(unreal.MaterialExpressionSaturate, g.op(unreal.MaterialExpressionMultiply,
                   g.op(unreal.MaterialExpressionSubtract, mask(nrm_w, False, False, True, -1350, -1100), x=-950, y=-1100, cb=0.6), x=-850, y=-1100, cb=3.0), -750, -1100)
        amt = g.op(unreal.MaterialExpressionMultiply, up, g.scalar("MossAmount", 0.0, -950, -1200), x=-650, y=-1100)
        col = g.lerp(col, g.op(unreal.MaterialExpressionMultiply, ms, g.vector("MossTint", (0.5, 0.55, 0.4, 1), -950, -1300), x=-650, y=-1000, ao="RGB"), amt, -300, -700)
    mel.connect_material_property(col, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.connect_material_property(nrm, "", MP.MP_NORMAL)
    usages(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    for name, tint, moss, tile in (("MI_WoodsTimber", (0.55, 0.48, 0.42), 0.15, 1.2), ("MI_WoodsRoof", (0.42, 0.40, 0.36), 0.85, 1.6)):
        mi = instance(name, mat)
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1))
        mel.set_material_instance_scalar_parameter_value(mi, "MossAmount", moss)
        mel.set_material_instance_scalar_parameter_value(mi, "TileMetres", tile)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
    log("world material done")


def plant_usages():
    roots = ["/Game/Range/Woods/PolyHaven", "/Game/Fab", "/Game/Megaplant_Library"]
    bases = {}
    for r in roots:
        if not eal.does_directory_exist(r):
            continue
        for p in eal.list_assets(r, recursive=True, include_folder=False):
            cls = str(eal.find_asset_data(p).asset_class_path.asset_name)
            if cls not in ("Material", "MaterialInstanceConstant"):
                continue
            a = eal.load_asset(p)
            b = a.get_base_material() if a else None
            if b:
                bases[b.get_path_name()] = b
            if cls == "MaterialInstanceConstant" and "PolyHaven" in p and b:
                log("PH material %s parent %s blend %s twosided %s" % (p.split(".")[0], b.get_path_name(),
                    b.get_editor_property("blend_mode"), b.get_editor_property("two_sided")))
    for path, m in sorted(bases.items()):
        usages(m)
        mel.recompile_material(m)
        pkg = path.split(".")[0]
        if pkg.startswith("/Game/"):
            eal.save_loaded_asset(m, only_if_is_dirty=False)
        log("usage flags on " + pkg)


FOLIAGE = ["fern_02", "shrub_01", "shrub_02", "shrub_03", "shrub_04", "nettle_plant", "celandine_01",
           "grass_medium_01", "grass_medium_02", "moss_01"]
PH_ART = os.path.join(os.environ.get("HOLDOVER_ART", "D:/Claude area/Holdover/art"), "polyhaven")


def foliage_master():
    """Two-sided foliage, cut out by an alpha mask: light shines through the leaves."""
    mat = material("M_WoodsFoliage")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    g = Graph(mat)
    uv = g.e(unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    col = g.sample("Color", "Color", tex("forest_leaves_02", "Color"), uv, -900, -300)
    alpha = g.sample("Alpha", "Rough", tex("forest_leaves_02", "Rough"), uv, -900, -100)
    arm = g.sample("ARM", "Rough", tex("forest_leaves_02", "Rough"), uv, -900, 100)
    nrm = g.sample("Normal", "Normal", tex("forest_leaves_02", "Normal"), uv, -900, 300)
    tint = g.vector("Tint", (1, 1, 1, 1), -900, -500)
    c = g.op(unreal.MaterialExpressionMultiply, col, tint, x=-600, y=-300, ao="RGB")
    sss = g.op(unreal.MaterialExpressionMultiply, c, g.vector("Translucency", (0.55, 0.6, 0.3, 1), -600, -500), x=-400, y=-450)
    rs = g.scalar("RoughnessChannelIsG", 1.0, -900, 500)
    rough = g.lerp(arm, arm, rs, -600, 100, ao="R", bo="G")
    mel.connect_material_property(c, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(alpha, "R", MP.MP_OPACITY_MASK)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.connect_material_property(nrm, "RGB", MP.MP_NORMAL)
    mel.connect_material_property(sss, "", MP.MP_SUBSURFACE_COLOR)
    usages(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def find_tex(folder, key):
    for p in eal.list_assets(folder, recursive=True, include_folder=False):
        if str(eal.find_asset_data(p).asset_class_path.asset_name).startswith("Texture") and key in p.split("/")[-1].lower():
            return eal.load_asset(p)
    return None


def foliage_instances(master):
    import os
    for aid in FOLIAGE:
        folder = "/Game/Range/Woods/PolyHaven/" + aid
        if not eal.does_directory_exist(folder):
            continue
        src = os.path.join(PH_ART, aid, "textures", "%s_alpha_2k.jpg" % aid)
        alpha_path = "%s/T_%s_alpha" % (folder, aid)
        if not eal.does_asset_exist(alpha_path) and os.path.isfile(src):
            t = unreal.AssetImportTask()
            t.filename = src
            t.destination_path = folder
            t.destination_name = "T_%s_alpha" % aid
            t.automated = True
            t.replace_existing = True
            t.save = True
            tools.import_asset_tasks([t])
        alpha = eal.load_asset(alpha_path) if eal.does_asset_exist(alpha_path) else None
        if alpha:
            alpha.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            alpha.set_editor_property("srgb", False)
            eal.save_loaded_asset(alpha)
        col = find_tex(folder, "_diff")
        nrm = find_tex(folder, "nor_gl")
        arm = find_tex(folder, "rough")
        if not (col and alpha):
            log("foliage %s: missing colour or alpha" % aid)
            continue
        mi = instance("MI_PH_" + aid, master)
        mel.set_material_instance_texture_parameter_value(mi, "Color", col)
        mel.set_material_instance_texture_parameter_value(mi, "Alpha", alpha)
        if nrm:
            mel.set_material_instance_texture_parameter_value(mi, "Normal", nrm)
        if arm:
            mel.set_material_instance_texture_parameter_value(mi, "ARM", arm)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        n = 0
        for p in eal.list_assets(folder, recursive=True, include_folder=False):
            if str(eal.find_asset_data(p).asset_class_path.asset_name) != "StaticMesh":
                continue
            sm = eal.load_asset(p)
            mats = list(sm.get_editor_property("static_materials"))
            for m in mats:
                m.set_editor_property("material_interface", mi)
            sm.set_editor_property("static_materials", mats)
            eal.save_loaded_asset(sm)
            n += 1
        log("foliage %s: MI on %d meshes" % (aid, n))


SURFACES = {  # Poly Haven solid models: id -> (tint, metal scale)
    "Barrel_02": ((1, 1, 1), 1.0), "boulder_01": ((0.85, 0.85, 0.82), 0.0), "dead_tree_trunk": ((0.75, 0.72, 0.68), 0.0),
    "dead_tree_trunk_02": ((0.75, 0.72, 0.68), 0.0), "dry_branches_medium_01": ((0.7, 0.66, 0.6), 0.0), "hatchet": ((1, 1, 1), 1.0),
    "old_military_crate": ((1, 1, 1), 1.0), "rock_07": ((0.85, 0.85, 0.82), 0.0), "rock_09": ((0.85, 0.85, 0.82), 0.0),
    "rock_moss_set_01": ((0.85, 0.88, 0.82), 0.0), "rock_moss_set_02": ((0.85, 0.88, 0.82), 0.0), "root_cluster_01": ((0.75, 0.72, 0.68), 0.0),
    "root_cluster_02": ((0.75, 0.72, 0.68), 0.0), "single_root": ((0.75, 0.72, 0.68), 0.0), "stone_01": ((0.85, 0.85, 0.82), 0.0),
    "tree_stump_01": ((0.75, 0.72, 0.68), 0.0), "tree_stump_02": ((0.75, 0.72, 0.68), 0.0),
}


def surface_master():
    """Opaque scanned surface: colour x tint, packed AO/roughness/metal (glTF order: G roughness, B metal)."""
    mat = material("M_WoodsSurface")
    g = Graph(mat)
    uv = g.e(unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    col = g.sample("Color", "Color", tex("forest_leaves_02", "Color"), uv, -900, -300)
    arm = g.sample("ARM", "Rough", tex("forest_leaves_02", "Rough"), uv, -900, 0)
    nrm = g.sample("Normal", "Normal", tex("forest_leaves_02", "Normal"), uv, -900, 300)
    c = g.op(unreal.MaterialExpressionMultiply, col, g.vector("Tint", (1, 1, 1, 1), -900, -500), x=-600, y=-300, ao="RGB")
    metal = g.op(unreal.MaterialExpressionMultiply, arm, g.scalar("MetalScale", 0.0, -900, 150), x=-600, y=100, ao="B")
    mel.connect_material_property(c, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(arm, "G", MP.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", MP.MP_METALLIC)
    mel.connect_material_property(nrm, "RGB", MP.MP_NORMAL)
    usages(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def surface_instances(master):
    for aid, (tint, metal) in SURFACES.items():
        folder = "/Game/Range/Woods/PolyHaven/" + aid
        if not eal.does_directory_exist(folder):
            continue
        col = find_tex(folder, "_diff")
        nrm = find_tex(folder, "nor_gl")
        arm = find_tex(folder, "rough")
        if not col:
            log("surface %s: no colour texture" % aid)
            continue
        mi = instance("MI_PH_" + aid, master)
        mel.set_material_instance_texture_parameter_value(mi, "Color", col)
        if nrm:
            mel.set_material_instance_texture_parameter_value(mi, "Normal", nrm)
        if arm:
            mel.set_material_instance_texture_parameter_value(mi, "ARM", arm)
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1))
        mel.set_material_instance_scalar_parameter_value(mi, "MetalScale", metal)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        n = 0
        for p in eal.list_assets(folder, recursive=True, include_folder=False):
            if str(eal.find_asset_data(p).asset_class_path.asset_name) != "StaticMesh":
                continue
            sm = eal.load_asset(p)
            mats = list(sm.get_editor_property("static_materials"))
            for m in mats:
                m.set_editor_property("material_interface", mi)
            sm.set_editor_property("static_materials", mats)
            eal.save_loaded_asset(sm)
            n += 1
        log("surface %s: MI on %d meshes" % (aid, n))


ground_master()
world_master()
foliage_instances(foliage_master())
surface_instances(surface_master())
plant_usages()
log("done")
