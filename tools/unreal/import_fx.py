"""Imports the firing effects into the project and builds their materials.

Run headless after tools/fx/gen_fx_textures.py and tools/audio/gen_gun_audio.py (Unreal Editor closed):
  UnrealEditor-Cmd.exe <Holdover.uproject> -run=pythonscript -script="<path>/import_fx.py" -unattended -nop4 -nosplash -NullRHI

Sources (<repo parent>/art/):  fx/textures/T_MuzzleFlash.png, fx/textures/T_MuzzleSmoke.png,
                               audio/Guns/<GunId>/*.wav, audio/Brass/*.wav
Creates:  /Game/FX/T_MuzzleFlash, /Game/FX/T_MuzzleSmoke, /Game/FX/M_MuzzleFlash, /Game/FX/M_MuzzleSmoke,
          /Game/Audio/Guns/<GunId>/SW_*, /Game/Audio/Brass/SW_*
The game code finds all of these by name (Weapons/HoldoverMuzzleFX.cpp, Weapons/HoldoverGunData.cpp, Audio/HoldoverAudio.cpp).
"""
import glob
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.environ.get("HOLDOVER_ART_ROOT", os.path.normpath(os.path.join(HERE, "..", "..", "..", "art")))
if not os.path.isdir(ART):
    ART = r"D:\Claude area\Holdover\art"

assets = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[import_fx] " + msg)


def setp(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as e:  # noqa
        log("could not set %s.%s: %s" % (type(obj).__name__, name, e))
        return False


def import_files(files, dest):
    tasks = []
    for f in files:
        task = unreal.AssetImportTask()
        task.filename = f
        task.destination_path = dest
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)
    if tasks:
        assets.import_asset_tasks(tasks)
    log("imported %d files into %s" % (len(tasks), dest))


# ---------------------------------------------------------------- textures
def import_textures():
    src = os.path.join(ART, "fx", "textures")
    import_files([os.path.join(src, "T_MuzzleFlash.png"), os.path.join(src, "T_MuzzleSmoke.png")], "/Game/FX")
    flash = unreal.load_asset("/Game/FX/T_MuzzleFlash")
    smoke = unreal.load_asset("/Game/FX/T_MuzzleSmoke")
    for tex, srgb in ((flash, True), (smoke, False)):
        if tex:
            setp(tex, "srgb", srgb)
            setp(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
            setp(tex, "max_texture_size", 2048)
            setp(tex, "address_x", unreal.TextureAddress.TA_CLAMP)
            setp(tex, "address_y", unreal.TextureAddress.TA_CLAMP)
            if not srgb:
                setp(tex, "compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    return flash, smoke


# ---------------------------------------------------------------- materials
SUBUV = """float F = floor(Frame + 0.5);
float Col = fmod(F, 4.0);
float Row = floor(F / 4.0);
return (frac(UV) + float2(Col, Row)) / 4.0;"""


def new_material(path):
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    return assets.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def subuv_sample(mat, tex, x):
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, x - 900, 0)
    frame = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x - 900, 120)
    frame.set_editor_property("parameter_name", "Frame")
    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x - 650, 40)
    custom.set_editor_property("code", SUBUV)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    i_uv = unreal.CustomInput()
    i_uv.set_editor_property("input_name", "UV")
    i_fr = unreal.CustomInput()
    i_fr.set_editor_property("input_name", "Frame")
    custom.set_editor_property("inputs", [i_uv, i_fr])
    mel.connect_material_expressions(uv, "", custom, "UV")
    mel.connect_material_expressions(frame, "", custom, "Frame")
    sample = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x - 400, 0)
    sample.set_editor_property("texture", tex)
    if tex and not tex.get_editor_property("srgb"):
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    mel.connect_material_expressions(custom, "", sample, "UVs")
    return sample


def build_flash_material(tex):
    mat = new_material("/Game/FX/M_MuzzleFlash")
    setp(mat, "blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    setp(mat, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    setp(mat, "two_sided", True)
    # A one-frame flash must not be smeared away by temporal anti-aliasing.
    setp(mat, "enable_responsive_aa", True)
    setp(mat, "apply_fogging", False)
    sample = subuv_sample(mat, tex, 0)

    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 250)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 0.8, 0.55, 1))
    inten = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 400)
    inten.set_editor_property("parameter_name", "Intensity")
    inten.set_editor_property("default_value", 1.0)

    # Cut the faint compression haze in the black parts of the atlas, or the card's outline shows in bright light.
    floor_c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 150)
    floor_c.set_editor_property("r", 0.02)
    sub = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -300, 60)
    mel.connect_material_expressions(sample, "RGB", sub, "A")
    mel.connect_material_expressions(floor_c, "", sub, "B")
    zero = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 150)
    clamp = mel.create_material_expression(mat, unreal.MaterialExpressionMax, -220, 60)
    mel.connect_material_expressions(sub, "", clamp, "A")
    mel.connect_material_expressions(zero, "", clamp, "B")
    m1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 100)
    mel.connect_material_expressions(clamp, "", m1, "A")
    mel.connect_material_expressions(tint, "", m1, "B")
    m2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 0, 150)
    mel.connect_material_expressions(m1, "", m2, "A")
    mel.connect_material_expressions(inten, "", m2, "B")

    # Soft where a card cuts through the gun or the ground.
    fade = mel.create_material_expression(mat, unreal.MaterialExpressionDepthFade, 0, 350)
    setp(fade, "fade_distance_default", 3.0)
    m3 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 150, 200)
    mel.connect_material_expressions(m2, "", m3, "A")
    mel.connect_material_expressions(fade, "", m3, "B")

    # Brightness relative to how the eye is adapted, so the flash reads the same in bright sun and deep shade.
    out = m3
    try:
        eye = mel.create_material_expression(mat, unreal.MaterialExpressionEyeAdaptationInverse, 300, 200)
        if mel.connect_material_expressions(m3, "", eye, "LightValueMultiplier"):
            out = eye
        else:
            log("EyeAdaptationInverse input name not found; flash brightness is absolute")
    except Exception as e:  # noqa
        log("no EyeAdaptationInverse node: %s" % e)
    mel.connect_material_property(out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    log("built M_MuzzleFlash")


def build_smoke_material(tex):
    mat = new_material("/Game/FX/M_MuzzleSmoke")
    setp(mat, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    setp(mat, "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    setp(mat, "two_sided", True)
    setp(mat, "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    sample = subuv_sample(mat, tex, 0)

    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 250)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.6, 0.6, 0.6, 1))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 350)
    rough.set_editor_property("r", 1.0)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    opacity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 450)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.3)
    m1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 300)
    mel.connect_material_expressions(sample, "R", m1, "A")
    mel.connect_material_expressions(opacity, "", m1, "B")
    fade = mel.create_material_expression(mat, unreal.MaterialExpressionDepthFade, 0, 450)
    setp(fade, "fade_distance_default", 15.0)
    m2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 150, 350)
    mel.connect_material_expressions(m1, "", m2, "A")
    mel.connect_material_expressions(fade, "", m2, "B")
    mel.connect_material_property(m2, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    log("built M_MuzzleSmoke")


# ---------------------------------------------------------------- sounds
def import_sounds():
    root = os.path.join(ART, "audio")
    for gun_dir in sorted(glob.glob(os.path.join(root, "Guns", "*"))):
        gun = os.path.basename(gun_dir)
        import_files(sorted(glob.glob(os.path.join(gun_dir, "*.wav"))), "/Game/Audio/Guns/" + gun)
    import_files(sorted(glob.glob(os.path.join(root, "Brass", "*.wav"))), "/Game/Audio/Brass")


def build_brass():
    """Polished cartridge brass for the fired cases (the Blender export came in nearly black)."""
    path = "/Game/FX/M_Brass"
    mat = new_material(path)
    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    col.set_editor_property("constant", unreal.LinearColor(0.78, 0.55, 0.24, 1))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    met = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 150)
    met.set_editor_property("r", 1.0)
    mel.connect_material_property(met, "", unreal.MaterialProperty.MP_METALLIC)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 250)
    rough.set_editor_property("r", 0.32)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    brass = unreal.load_asset(path)
    for gun_dir in eal.list_assets("/Game/Guns", recursive=True, include_folder=False):
        name = gun_dir.split("/")[-1].split(".")[0]
        if not name.startswith("SM_Casing_"):
            continue
        mesh = unreal.load_asset(gun_dir)
        slots = mesh.get_editor_property("static_materials")
        for i, s in enumerate(slots):
            old = s.get_editor_property("material_interface")
            log("%s slot %d was %s" % (name, i, old.get_name() if old else None))
            mesh.set_material(i, brass)
        eal.save_loaded_asset(mesh)


def main():
    log("art folder: " + ART)
    flash, smoke = import_textures()
    build_flash_material(flash)
    build_smoke_material(smoke)
    build_brass()
    import_sounds()
    for folder in ("/Game/FX", "/Game/Audio"):
        eal.save_directory(folder, only_if_is_dirty=False, recursive=True)
    log("done")


main()
