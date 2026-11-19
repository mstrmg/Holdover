# FAL-style rifle, part B: barrel, handguard, gas block + front sight, regulator, flash hider, stock, grip, magazine, ammo.
import bpy, math
from mathutils import Vector, Matrix
globals().update(bpy.app.driver_namespace["hb"])
O = bpy.data.objects; C = coll("Rifle_FAL"); CUT = coll("_cutters")
PARK = bpy.data.materials["M_FAL_Park"]; DARK = bpy.data.materials["M_FAL_Dark"]; POLY = bpy.data.materials["M_FAL_Synthetic"]
def bake_join(name, parts, mat):
    for p in parts: bake(p)
    ob = join(name, parts) if len(parts) > 1 else parts[0]
    ob.name = name; ob.data.name = name
    ob.data.materials.clear(); ob.data.materials.append(mat)
    for p in ob.data.polygons: p.material_index = 0
    return ob
# ---------------- barrel ----------------
bar = lathe("Barrel", [(0, 14), (55, 14), (55, 12), (90, 10.5), (330, 10), (330, 9.2), (395, 9.2), (395, 8.6), (530, 8.4), (530, 7.6), (536, 7.6), (536, 3.9), (420, 3.9)], C, DARK, seg=40)
bake(bar)
# ---------------- handguard (synthetic, two end caps) ----------------
def hg_sec(x):
    s = 1.0
    if x < 64: s = 0.94 + 0.06*(x-61)/3
    if x > 310: s = 0.94 + 0.06*(313-x)/3
    return superq(24*s, 3 - 26*s, 3 + 26*s, n_top=3.2, n_bot=3.2, N=48)
xs = [61, 61.8, 63, 64] + list(range(70, 310, 6)) + [310, 311.2, 312.2, 313]
hg = xloft("Handguard", xs, hg_sec, C, POLY)
hc = []
for i in range(7):
    x0 = 82 + i*31
    hc.append(box("c_hgvL%d" % i, x0, x0 + 18, 21, 30, -5, 5, CUT)); hc.append(box("c_hgvR%d" % i, x0, x0 + 18, -30, -21, -5, 5, CUT))
hc += [cyl("c_hgt1", 66, 308, 2.0, 9, 29.6, CUT, seg=16), cyl("c_hgt2", 66, 308, 2.0, -9, 29.6, CUT, seg=16)]
cut(hg, hc); bevel(hg, 0.6, 2, 40)
hg = bake_join("Handguard", [hg], POLY)
cap1 = xsec("hg_cap1", superq(24.8, -23.8, 29.8, n_top=3.2, n_bot=3.2, N=48), 55.5, 62, C, PARK); bevel(cap1, 0.8, 2, 40)
cap2 = xsec("hg_cap2", superq(24.8, -23.8, 29.8, n_top=3.2, n_bot=3.2, N=48), 312, 318.5, C, PARK); bevel(cap2, 0.8, 2, 40)
caps = bake_join("HandguardCaps", [cap1, cap2], PARK)
# ---------------- gas block + front sight ----------------
g1 = xsec("gb_ring", rounded_rect_yz(26, 26, 7), 318.6, 392, C, PARK)
g2 = cyl("gb_cyl", 318.6, 392, 11, 0, 20, C, PARK, seg=32)
g3 = box("gb_web", 318.6, 392, -8, 8, 0, 20, C, PARK)
g4 = box("fs_base", 364, 392, -8.5, 8.5, 26, 35, C, PARK)
ear = chaikin([(364, 30), (392, 30), (392, 40), (387, 52), (372, 52), (364, 40)], 1, closed=True)
g5 = profile("fs_earL", ear, 8.2, 11.2, C, PARK); g6 = profile("fs_earR", ear, -11.2, -8.2, C, PARK)
g7 = cyl_z("fs_drum", 33, 37, 3.6, 379, 0, C, PARK, seg=20); g8 = cyl_z("fs_post", 36, 47, 1.2, 379, 0, C, PARK, seg=12)
g9 = box("gb_swtab", 355, 375, -2.5, 2.5, -20, -11, C, PARK)
g10 = torus_x("gb_swring", 6, 1.3, C, PARK); g10.rotation_euler = (0, 0, math.radians(90)); apply_tf(g10); move(g10, 365, 0, -25)
for g in (g1, g3, g4, g5, g6, g9): bevel(g, 0.8, 2, 40)
gb = bake_join("GasBlock", [g1, g2, g3, g4, g5, g6, g7, g8, g9, g10], PARK)
rg = lathe("GasRegulator", [(392.3, 0.1), (392.3, 9.5), (401, 9.5), (404, 7.5), (404, 0.1)], C, PARK, seg=32); move(rg, 0, 0, 20)
cut(rg, [box("c_rgslot", 401.5, 406, -8, 8, 19, 21, CUT)] + [box("c_rgn%d" % i, 393, 401, 8.6, 11, 17.5 + 0*i, 22.5, CUT) for i in range(1)])
rg = bake_join("GasRegulator", [rg], PARK); set_origin(rg, 398, 0, 20)
# ---------------- flash hider (birdcage) ----------------
fh = lathe("FlashHider", [(528, 4.5), (528, 11.5), (588, 11.5), (590, 10.4), (590, 8.6), (536, 8.6), (536, 4.5), (528, 4.5)], C, DARK, seg=40)
cut(fh, [rot(box("c_fhs%d" % i, 552, 586, -1.6, 1.6, 0, 15, CUT), "X", i*60 + 30, (0, 0, 0)) for i in range(6)]); bevel(fh, 0.4, 2, 40)
fl = box("fh_lug", 528, 548, -5, 5, -17, -9, C, DARK); bevel(fl, 0.8, 2)
fh = bake_join("FlashHider", [fh, fl], DARK)
# ---------------- stock + buttplate + rear swivel ----------------
bottom = chaikin([(-500, -150), (-300, -82), (-250, -62), (-222, -54), (-205.8, -51)], 2)
st = profile("Stock", [(-205.8, 17), (-505, -12)] + bottom, -23, 23, C, POLY)
intersect(st, topview("c_stap", [(-200, 14.8), (-280, 17.5), (-515, 22), (-515, -22), (-280, -17.5), (-200, -14.8)], -200, 40, CUT)); bevel(st, 4.5, 4, 30)
st = bake_join("Stock", [st], POLY)
bp = profile("bp_plate", [(-505, -11.5), (-500, -150.5), (-507, -151.5), (-512, -12.5)], -22.4, 22.4, C, POLY)
xb = lambda z: -512 + 5*(-12.5 - z)/139
cut(bp, [box("c_bpg%d" % i, xb(z) - 3, xb(z) + 1.2, -30, 30, z - 1.5, z + 1.5, CUT) for i, z in enumerate(range(-20, -146, -7))]); bevel(bp, 0.6, 2, 30)
bp = bake_join("ButtPlate", [bp], POLY)
s1 = box("sw_tab", -436, -424, -2.5, 2.5, -131, -124, C, PARK); bevel(s1, 0.6, 2)
s2 = torus_x("sw_ring", 6, 1.3, C, PARK); s2.rotation_euler = (0, 0, math.radians(90)); apply_tf(s2); move(s2, -430, 0, -136)
sw = bake_join("SlingSwivel", [s1, s2], PARK)
# ---------------- pistol grip ----------------
rk = math.radians(20); d = Vector((-math.sin(rk), -math.cos(rk))); f = Vector((math.cos(rk), -math.sin(rk))); a0 = Vector((-181, -40))
DU = [(-10, 18), (0, 18), (40, 19.5), (80, 19.5), (100, 19), (106.6, 19)]
DV = [(-10, 14.5), (0, 14.5), (50, 15.5), (100, 15), (106.6, 15)]
SC = [(-10, 1), (100, 1), (102.5, 0.96), (104.5, 0.86), (106, 0.62), (106.7, 0.2)]
ts = [-10, -4, 0] + list(range(8, 99, 8)) + [100, 102.5, 104.5, 106, 106.7]
rings = []
for t in ts:
    du = lerp_tab(DU, t)*lerp_tab(SC, t); dv = lerp_tab(DV, t)*lerp_tab(SC, t); ax = a0 + d*t; ring = []
    for i in range(40):
        th = 2*math.pi*i/40; ct, stt = math.cos(th), math.sin(th)
        u = du*math.copysign(abs(ct)**(2/2.4), ct); v = dv*math.copysign(abs(stt)**(2/2.4), stt)*(1 - 0.06*u/max(du, 0.1))
        if t > 20 and t < 95 and ct > 0.2: u += 0.9*math.sin(t*math.pi/12.5)**8*ct        # finger ridges on the front strap
        p = ax + f*u; ring.append((p.x, v, p.y))
    rings.append(ring)
gr = loft("PistolGrip", rings, C, POLY); bake(gr)
# ---------------- 20-round magazine ----------------
body = topview("mg_body", [(x - 40, y) for x, y in rounded_rect_yz(72, 27.6, 3.5, seg=4)], -196, -14, C, DARK)
cut(body, [box("c_mtop", -73, -7, -10.5, 10.5, -27, -13, CUT)])
mp = [body]
fpl = topview("mg_floor", [(x - 39.5, y) for x, y in rounded_rect_yz(77, 30.4, 3.5, seg=4)], -203, -196.2, C, DARK); bevel(fpl, 1.0, 2); mp.append(fpl)
for k, (xc, side) in enumerate([(-58, 1), (-24, 1), (-58, -1), (-24, -1)]):
    y0, y1 = sorted((13.4*side, 14.5*side))
    r_ = box("mg_rib%d" % k, xc - 2.5, xc + 2.5, y0, y1, -185, -60, C, DARK); bevel(r_, 0.4, 2); mp.append(r_)
for side in (1, -1):
    y0, y1 = sorted((9.4*side, 11*side)); mp.append(box("mg_lip%d" % (side > 0), -72, -30, y0, y1, -16.2, -14, C, DARK))
mp.append(box("mg_flug", -4.5, 1, -6, 6, -24, -15, C, DARK))
mp.append(box("mg_rlug", -79, -75.5, -6, 6, -52, -47.6, C, DARK))
mag = bake_join("Magazine", mp, DARK); set_origin(mag, -40, 0, -14)
fol = box("MagFollower", -72, -8, -10.2, 10.2, -27, -23, C, DARK); bevel(fol, 0.8, 2)
fol = bake_join("MagFollower", [fol], DARK); set_origin(fol, -40, 0, -25); parent_keep(fol, mag)
for o in C.objects: finish(o, 35)
# ---------------- ammo 7.62x51 ----------------
A = coll("Ammo_762x51")
for ob in list(A.objects): bpy.data.objects.remove(ob, do_unlink=True)
brass = plain_mat("M_Brass", (0.78, 0.56, 0.24), 1.0, 0.28); copper = plain_mat("M_Copper", (0.75, 0.42, 0.28), 1.0, 0.3); primer = plain_mat("M_Primer", (0.7, 0.68, 0.62), 1.0, 0.35)
def prim(n): return lathe(n, [(-0.05, 0), (-0.05, 2.7), (0.15, 2.7), (0.15, 0)], A, primer, 16)
case = lathe("case", [(0, 0), (0, 6.0), (1.4, 6.0), (1.4, 5.0), (2.4, 5.0), (3.0, 5.97), (39.6, 5.4), (43.5, 4.3), (51.2, 4.3), (51.2, 0)], A, brass, 24)
bul = lathe("bullet", [(44, 0), (44, 3.9), (56, 3.9), (60, 3.75), (64, 3.2), (67.5, 2.3), (70, 1.2), (71, 0.4), (71.1, 0)], A, copper, 24)
rnd = join("Round_762x51", [case, bul, prim("p1")]); finish(rnd, 30)
cs = lathe("casing", [(0, 0), (0, 6.0), (1.4, 6.0), (1.4, 5.0), (2.4, 5.0), (3.0, 5.97), (39.6, 5.4), (43.5, 4.3), (51.2, 4.3), (51.2, 3.95), (43.5, 3.95), (39.6, 5.0), (4, 5.5), (4, 0)], A, brass, 24)
cas = join("Casing_762x51", [cs, prim("p2")]); finish(cas, 30)
result = {"objs": sorted(o.name for o in C.objects)}
