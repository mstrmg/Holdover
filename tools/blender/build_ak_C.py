# AKM-style rifle, part C: stock, buttplate, rear sling loop, pistol grip, magazine + follower.
import bpy, math
from mathutils import Vector, Matrix
globals().update(bpy.app.driver_namespace["hb"])
O = bpy.data.objects; C = coll("Rifle_AK"); CUT = coll("_cutters")
STEEL = bpy.data.materials["M_AK_Steel"]; WOOD = bpy.data.materials["M_AK_Wood"]
def bake_join(name, parts, mat):
    for p in parts: bake(p)
    ob = join(name, parts) if len(parts) > 1 else parts[0]
    ob.name = name; ob.data.name = name
    ob.data.materials.clear(); ob.data.materials.append(mat)
    for p in ob.data.polygons: p.material_index = 0
    return ob
# ---------------- stock ----------------
bottom = chaikin([(-437, -177), (-320, -112), (-262, -72), (-232, -50), (-210, -40.5), (-199, -37.3)], 2)
st = profile("Stock", [(-199, 5), (-441, -46)] + bottom, -21, 21, C, WOOD)
taper = topview("c_taper", [(-195, 15), (-280, 17.6), (-445, 21), (-445, -21), (-280, -17.6), (-195, -15)], -200, 30, CUT)
intersect(st, taper); bevel(st, 5, 4, 30)
st = bake_join("Stock", [st], WOOD)
bp = profile("bp_plate", [(-441, -45.5), (-437, -177.5), (-441.5, -178.8), (-446, -46.8)], -21.6, 21.6, C, STEEL); bevel(bp, 1.2, 2, 30)
s1 = cyl("bp_screw1", -446.6, -444.6, 3.0, 0, -62, C, STEEL, seg=16); s2 = cyl("bp_screw2", -443.0, -441.0, 3.0, 0, -165, C, STEEL, seg=16)
bp = bake_join("ButtPlate", [bp, s1, s2], STEEL)
sw1 = box("sw_plate", -418, -398, 19.6, 21.6, -151, -139, C, STEEL); bevel(sw1, 1, 2)
sw2 = box("sw_post", -411, -405, 21, 25, -148, -142, C, STEEL); bevel(sw2, 0.6, 2)
sw3 = torus_x("sw_ring", 6.2, 1.4, C, STEEL); sw3.rotation_euler = (0, math.radians(-90), 0); apply_tf(sw3); move(sw3, -408, 30, -145)
sw = bake_join("SlingSwivel", [sw1, sw2, sw3], STEEL)
# ---------------- pistol grip ----------------
rk = math.radians(18); d = Vector((-math.sin(rk), -math.cos(rk))); f = Vector((math.cos(rk), -math.sin(rk))); a0 = Vector((-173, -36))
DU = [(-14, 19.5), (0, 19.5), (40, 21), (80, 21), (100, 20.2), (108.6, 20)]
DV = [(-14, 13.5), (0, 13.5), (50, 14.8), (100, 14.3), (108.6, 14.3)]
SC = [(-14, 1), (102, 1), (104.5, 0.96), (106.5, 0.86), (108, 0.62), (108.7, 0.2)]
ts = [-14, -6, 0] + list(range(8, 101, 8)) + [102, 104.5, 106.5, 108, 108.7]
rings = []
for t in ts:
    du = lerp_tab(DU, t)*lerp_tab(SC, t); dv = lerp_tab(DV, t)*lerp_tab(SC, t); ax = a0 + d*t; ring = []
    for i in range(36):
        th = 2*math.pi*i/36; ct, stt = math.cos(th), math.sin(th)
        u = du*math.copysign(abs(ct)**(2/2.4), ct); v = dv*math.copysign(abs(stt)**(2/2.4), stt)*(1 - 0.07*u/max(du, 0.1))
        p = ax + f*u; ring.append((p.x, v, p.y))
    rings.append(ring)
gr = loft("PistolGrip", rings, C, WOOD); bake(gr)
# ---------------- magazine (curved 30-round, ribbed) ----------------
X0, Z0, R = -45.0, -14.0, 330.0
def arc(s, u, v):
    th = s/R; cx = X0 + R*(1 - math.cos(th)); cz = Z0 - R*math.sin(th)
    return (cx + u*math.cos(th), v, cz + u*math.sin(th))
sec = rounded_rect_yz(62, 28.6, 4, seg=4)
ss = [0, 2] + list(range(6, 207, 6)) + [207, 209.5]
body = loft("mg_body", [[arc(s, u, v) for u, v in sec] for s in ss], C, STEEL)
fsec = rounded_rect_yz(66, 30.8, 4, cy=1, seg=4)
fp = loft("mg_floor", [[arc(s, u, v) for u, v in fsec] for s in (209, 213, 216.5)], C, STEEL); bevel(fp, 1.2, 2, 40)
mparts = [body, fp]
for k, (uc, side) in enumerate([(-12, 1), (12, 1), (-12, -1), (12, -1)]):
    rs = [(uc + y, side*(13.8 + (z + 0.6))) for y, z in rounded_rect_yz(5, 1.2, 0.5, seg=2)]
    mparts.append(loft("mg_rib%d" % k, [[arc(s, u, v) for u, v in rs] for s in range(30, 199, 6)], C, STEEL))
rec = box("c_magtop", -73, -17, -11.6, 11.6, -26, -13, CUT); cut(body, [rec])
for side in (1, -1):
    mparts.append(box("mg_lip%d" % (side > 0), -72, -36, side*10.1, side*11.9, -16.2, -14, C, STEEL))
mparts.append(box("mg_flug", -14.6, -9.8, -6, 6, -24, -15, C, STEEL))
mparts.append(box("mg_rlug", -79.3, -74.4, -6, 6, -43.5, -38.6, C, STEEL))
mag = bake_join("Magazine", mparts, STEEL); set_origin(mag, -45, 0, -14)
fol = box("MagFollower", -72, -18.5, -11.2, 11.2, -25.5, -21.5, C, STEEL); bevel(fol, 0.8, 2)
fol = bake_join("MagFollower", [fol], STEEL); set_origin(fol, -45, 0, -23.5); parent_keep(fol, mag)
for n in ("Stock", "ButtPlate", "SlingSwivel", "PistolGrip", "Magazine", "MagFollower"): finish(O[n], 35)
result = {"objs": sorted(o.name for o in C.objects)}
