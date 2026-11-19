# AKM-style rifle, part B: barrel, rear sight, gas system, handguards, front sight, muzzle brake, cleaning rod.
import bpy, math
from mathutils import Vector, Matrix
globals().update(bpy.app.driver_namespace["hb"])
O = bpy.data.objects; C = coll("Rifle_AK"); CUT = coll("_cutters")
STEEL = bpy.data.materials["M_AK_Steel"]; BARREL = bpy.data.materials["M_AK_Barrel"]; WOOD = bpy.data.materials["M_AK_Wood"]
def bake_join(name, parts, mat):
    for p in parts: bake(p)
    ob = join(name, parts) if len(parts) > 1 else parts[0]
    ob.name = name; ob.data.name = name
    ob.data.materials.clear(); ob.data.materials.append(mat)
    for p in ob.data.polygons: p.material_index = 0
    return ob
# ---------------- barrel (tube, bore open at muzzle) ----------------
bar = lathe("Barrel", [(0, 12.5), (24, 12.5), (24, 11), (100, 11), (100, 10), (378, 10), (378, 9), (415, 9), (415, 7.5), (419, 7.5), (419, 3.9), (300, 3.9)], C, BARREL, seg=40)
# ---------------- rear sight block ----------------
rsb = profile("rsb_body", [(25.3, -14), (100, -14), (100, 24), (97, 32.5), (84, 32.5), (80, 29.5), (25.3, 29.5)], -13, 13, C, STEEL)
rsx = xsec("c_rsbround", superq(13, -14, 32.5, n_top=6, n_bot=2.8, N=48), 20, 105, CUT)
intersect(rsb, rsx); bevel(rsb, 1.0, 2, 40)
rsl = box("rsb_lever", 86, 99, -15.6, -13, 12, 21, C, STEEL); bevel(rsl, 0.8, 2)
rsk = cyl_y("rsb_leverknob", -17.5, -15, 3.2, 92, 16.5, C, STEEL, seg=16)
rsb = bake_join("RearSightBlock", [rsb, rsl, rsk], STEEL)
# leaf (hinged at front, notch at rear) and slider
lp = [(28, 29.7), (90, 29.7), (90, 32.3), (31.5, 32.3), (31.5, 38.5), (28, 38.5)]
leaf = profile("RearSightLeaf", lp, -7.5, 7.5, C, STEEL)
notch = box("c_notch", 27, 33, -1.1, 1.1, 37, 40, CUT)
cut(leaf, [notch]); bevel(leaf, 0.3, 1, 50)
leaf = bake_join("RearSightLeaf", [leaf], STEEL); set_origin(leaf, 88, 0, 31)
sl = box("sl_body", 60, 70, -10.2, 10.2, 29.9, 33.6, C, STEEL); bevel(sl, 0.8, 2)
slb1 = cyl_y("sl_btnL", 10.2, 11.6, 2.3, 65, 31.7, C, STEEL, seg=14); slb2 = cyl_y("sl_btnR", -11.6, -10.2, 2.3, 65, 31.7, C, STEEL, seg=14)
sl = bake_join("RearSightSlider", [sl, slb1, slb2], STEEL); set_origin(sl, 65, 0, 31.7)
# ---------------- gas tube + upper handguard ----------------
gt = cyl("GasTube", 97, 266, 10, 0, 20, C, STEEL, seg=32)
gt = bake_join("GasTube", [gt], STEEL)
def up_sec(x):
    s = 1.0
    if x < 106: s = 0.93 + 0.07*(x-103)/3
    if x > 255: s = 0.93 + 0.07*(258-x)/3
    a = 15*s; zt = 20 + 11.5*s; zb = 8.7
    return superq(a, zb, zt, n_top=2.3, n_bot=7, N=40)
xs = [103, 103.6, 104.5, 106] + list(range(110, 255, 5)) + [255, 256.6, 257.5, 258]
uh = xloft("UpperHandguard", xs, up_sec, C, WOOD)
# ---------------- lower handguard + retainer band ----------------
A_TAB = [(100.3, 13.5), (114, 17), (180, 17), (198, 20.5), (230, 20.5), (245, 17.2)]
ZB_TAB = [(100.3, -14), (118, -26), (236, -26), (245, -22.5)]
def lo_sec(x):
    return superq(lerp_tab(A_TAB, x), lerp_tab(ZB_TAB, x), 8.5, n_top=9, n_bot=2.6, N=40)
xs = [100.3] + list(range(104, 245, 4)) + [245]
lh = xloft("LowerHandguard", xs, lo_sec, C, WOOD)
band = xsec("rb_band", superq(17.8, -24, 8.6, n_top=9, n_bot=2.6, N=40), 245.3, 259, C, STEEL); bevel(band, 0.8, 2, 40)
band = bake_join("HandguardRetainer", [band], STEEL)
# ---------------- gas block ----------------
g1 = xsec("gb_ring", rounded_rect_yz(25, 26, 7), 262, 296, C, STEEL)
g2 = cyl("gb_top", 262, 281, 12.3, 0, 20, C, STEEL, seg=32)
g3 = lathe("gb_nose", [(281, 12.3), (291, 5.5), (293, 0.1)], C, STEEL, seg=32); move(g3, 0, 0, 20)
g4 = box("gb_web", 262, 293, -8, 8, 0, 20, C, STEEL)
g5 = box("gb_rodguide", 267, 291, -5, 5, -20.5, -8, C, STEEL)
for g in (g1, g4, g5): bevel(g, 1.0, 2, 40)
g6 = box("gb_swtab", 275, 283, 12, 14.5, -9, 0, C, STEEL)
g7 = torus_x("gb_swivel", 5.6, 1.3, C, STEEL); g7.rotation_euler = (0, 0, math.radians(90)); apply_tf(g7); move(g7, 279, 15.8, -11.5)
gb = bake_join("GasBlock", [g1, g2, g3, g4, g5, g6, g7], STEEL)
# ---------------- front sight base ----------------
f1 = xsec("fs_base", rounded_rect_yz(24, 28, 7), 378, 413, C, STEEL); bevel(f1, 1.0, 2, 40)
f2 = profile("fs_tower", [(392, 10), (412, 10), (412, 22), (395, 22)], -7.5, 7.5, C, STEEL); bevel(f2, 0.8, 2)
ear = [(392, 12), (412, 12), (412, 31), (409, 41), (401.5, 41), (392, 25)]
f3 = profile("fs_earL", ear, 8.2, 11.6, C, STEEL); f4 = profile("fs_earR", ear, -11.6, -8.2, C, STEEL)
for f in (f3, f4): bevel(f, 0.6, 2)
f5 = cyl_z("fs_drum", 19, 24, 4.6, 404, 0, C, STEEL, seg=24)
f6 = cyl_z("fs_post", 23.5, 37, 1.1, 404, 0, C, STEEL, seg=12)
f7 = box("fs_lug", 397, 413, -5, 5, -24, -10, C, STEEL); bevel(f7, 1.0, 2)
f8 = cyl_y("fs_detent", -13.2, -11.5, 2.2, 409, -2, C, STEEL, seg=14)
fsb = bake_join("FrontSightBase", [f1, f2, f3, f4, f5, f6, f7, f8], STEEL)
# ---------------- slant muzzle brake ----------------
mb = lathe("MuzzleBrake", [(413.3, 4.6), (413.3, 10.5), (444.5, 10.5), (447, 9.6), (447, 4.6), (413.3, 4.6)], C, STEEL, seg=40)
sc = box("c_slant", 447, 500, -30, 30, -10.5, 40, CUT); rot(sc, "Y", -21, (447, 0, -10.5))
cut(mb, [sc]); bevel(mb, 0.5, 2, 40)
mb = bake_join("MuzzleBrake", [mb], STEEL)
# ---------------- cleaning rod ----------------
r1 = cyl("cr_rod", 255, 410, 3.0, 0, -16, C, STEEL, seg=16)
r2 = cyl("cr_head", 404, 414, 3.6, 0, -16, C, STEEL, seg=16)
rod = bake_join("CleaningRod", [r1, r2], STEEL)
for n in ("Barrel", "UpperHandguard", "LowerHandguard"):
    bake(O[n])
for o in C.objects: finish(o, 35)
result = {"objs": sorted(o.name for o in C.objects)}
