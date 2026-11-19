# AKM-style rifle rebuild, part A: reset, materials, receiver group. mm, X forward, Y left, Z up, bore Z=0, bolt face X=0.
import bpy, math
from mathutils import Vector, Matrix
globals().update(bpy.app.driver_namespace["hb"])
O = bpy.data.objects
# --- reset old model (backup saved by caller) ---
for cn in ("Rifle_AK", "EXPORT_Rifle_AK", "_cutters", "_tmp"):
    cc = bpy.data.collections.get(cn)
    if cc:
        for ob in list(cc.objects):
            d = ob.data; bpy.data.objects.remove(ob, do_unlink=True)
            if d is not None and getattr(d, "users", 1) == 0:
                (bpy.data.meshes if isinstance(d, bpy.types.Mesh) else bpy.data.armatures).remove(d)
for m in list(bpy.data.meshes):
    if m.users == 0: bpy.data.meshes.remove(m)
C = coll("Rifle_AK"); CUT = coll("_cutters")
# --- materials ---
STEEL = worn("M_AK_Steel", (0.022, 0.022, 0.024), 0.55, 0.42, (0.20, 0.20, 0.21), 1.0, 0.3, edge=0.45, scratch=0.7, wear_lo=0.5, wear_hi=0.72)
BARREL = worn("M_AK_Barrel", (0.016, 0.016, 0.017), 0.7, 0.36, (0.22, 0.22, 0.23), 1.0, 0.28, edge=0.45, scratch=0.5, wear_lo=0.5, wear_hi=0.72)
WOOD = wood("M_AK_Wood", (0.045, 0.012, 0.005), (0.115, 0.032, 0.012), (0.075, 0.020, 0.008), rough=0.34, ring_scale=300, distortion=4, coat=0.5, along="X")
def bake_join(name, parts, mat):
    for p in parts: bake(p)
    ob = join(name, parts) if len(parts) > 1 else parts[0]
    ob.name = name; ob.data.name = name
    ob.data.materials.clear(); ob.data.materials.append(mat)
    for p in ob.data.polygons: p.material_index = 0
    return ob
def rivet(nm, x, z, side, r=2.2, h=0.7):
    y0 = 15.5 if side > 0 else -15.5 - h
    return cyl_y(nm, y0, y0 + h, r, x, z, C, STEEL, seg=14)
# ---------------- receiver ----------------
rec = profile("Receiver", [(-200, -38), (25, -38), (25, 10), (-200, 10)], -15.5, 15.5, C, STEEL)
port = box("c_port", -95, -22, -17, 0.5, -11, 6.5, CUT)            # ejection port (right side)
slot = box("c_slot", -125, 27, -17, -10, 1, 6.5, CUT)              # charging handle slot
well = box("c_well", -79.5, -9.5, -14.6, 14.6, -40, -12, CUT)      # magazine well
dimL = ellipsoid("c_dimL", (-36, 17.6, -24), (13, 3, 5.5), 0, CUT); dimR = ellipsoid("c_dimR", (-36, -17.6, -24), (13, 3, 5.5), 0, CUT)
cut(rec, [port, slot, well, dimL, dimR]); bevel(rec, 0.8, 2, 50)
parts = [rec]
pts = [(12, -14), (4, -28), (19, -28), (-186, -12), (-193, -27), (-181, -30), (-92, -34), (-147, -34), (-62, -33), (-70, -33)]
for k, (x, z) in enumerate(pts):
    parts.append(rivet("rvL%d" % k, x, z, 1)); parts.append(rivet("rvR%d" % k, x, z, -1))
for k, (x, z) in enumerate([(-112, -27), (-128, -21)]):            # trigger and hammer pins (left side heads)
    parts.append(cyl_y("pin%d" % k, 15.5, 16.4, 2.6, x, z, C, STEEL, seg=16))
parts.append(cyl_y("selAxle", 15.5, 16.6, 3.2, -178, -4, C, STEEL, seg=16))
gb = box("guideBtn", -199, -191, -4, 4, 18, 27.6, C, STEEL); bevel(gb, 0.8, 2); parts.append(gb)
rec = bake_join("Receiver", parts, STEEL)
# ---------------- dust cover ----------------
def dc_sec(hw, zb, zt, r):
    pts = [(hw, zb)]
    for k in range(9):
        a = math.radians(90 * k / 8); pts.append((hw - r + r*math.cos(a), zt - r + r*math.sin(a)))
    for k in range(9):
        a = math.radians(90 + 90 * k / 8); pts.append((-hw + r + r*math.cos(a), zt - r + r*math.sin(a)))
    pts.append((-hw, zb)); return pts
dc = xsec("DustCover", dc_sec(16.5, 6.5, 26, 9), -201, 23.7, C, STEEL); dc_parts = [dc]
for k, x in enumerate((-168, -134, -100, -66, -32)):
    dc_parts.append(xsec("dcRib%d" % k, dc_sec(16.9, 10, 26.4, 9.3), x - 2.5, x + 2.5, C, STEEL))
bevel(dc_parts[0], 0.6, 2, 50)
for p in dc_parts[1:]: bevel(p, 0.35, 2, 30)
dc = bake_join("DustCover", dc_parts, STEEL)
# ---------------- bolt carrier (with piston + charging handle) and bolt ----------------
car = box("bc_body", -140, 18, -10, 10, 3, 22, C, STEEL); bevel(car, 1.5, 2)
pis = cyl("bc_piston", 18, 270, 5.5, 0, 20, C, STEEL, seg=24)
head = cyl("bc_head", 268, 283, 9.4, 0, 20, C, STEEL, seg=24)
arm = box("bc_arm", 8, 16, -27, -9, 2, 6, C, STEEL); bevel(arm, 1, 2)
knob = lathe("bc_knob", [(0, 0.1), (1.2, 4.0), (3.5, 5.6), (6, 5.6), (8.3, 4.0), (9.5, 0.1)], C, STEEL, seg=24)
knob.rotation_euler = (0, 0, math.radians(-90)); knob.location = (12*MM, -24.5*MM, 5*MM); apply_tf(knob)
car = bake_join("BoltCarrier", [car, pis, head, arm, knob], STEEL); set_origin(car, 0, 0, 10)
bolt = cyl("b_body", -95, -0.4, 9, 0, 0, C, STEEL, seg=32)
ext = box("b_ext", -30, -6, -10.6, -7.5, -3, 3, C, STEEL); bevel(ext, 0.6, 2)
bolt = bake_join("Bolt", [bolt, ext], STEEL); set_origin(bolt, -50, 0, 0)
# ---------------- selector (right side, shown on SAFE) ----------------
hub = [(-178 + 8.5*math.cos(math.radians(a)), -4 + 8.5*math.sin(math.radians(a))) for a in range(90, 271, 15)]
sel_pts = [(-178, 4.5)] + [(-120, 4.5), (-40, 4.5), (-34, 0), (-35, -9), (-43, -9), (-48, -3), (-150, -3), (-166, -9)] + [(-178, -12.5)] + hub[::-1][1:-1]
sel_pts = chaikin(sel_pts, 1, closed=True)
sel = profile("s_lever", sel_pts, -17.4, -15.8, C, STEEL); bevel(sel, 0.4, 2)
tab = box("s_tab", -43, -35.5, -21, -17, -9, -5.5, C, STEEL); bevel(tab, 0.5, 2)
sel = bake_join("Selector", [sel, tab], STEEL); set_origin(sel, -178, -16.6, -4)
# ---------------- trigger guard, trigger, magazine catch ----------------
tg = profile("TriggerGuard", [(-91, -37.5), (-92.5, -58), (-97, -63), (-145, -63), (-150.5, -58), (-151, -37.5),
                              (-148, -37.5), (-147.5, -57), (-144, -60), (-98, -60), (-95.5, -57), (-94, -37.5)], -5, 5, C, STEEL)
bevel(tg, 0.6, 2); tg = bake_join("TriggerGuard", [tg], STEEL)
tp = chaikin([(-113, -32), (-113, -37), (-112, -45), (-114, -52), (-119, -56), (-122.5, -55), (-119.5, -50), (-118.5, -44), (-119.5, -37), (-119.5, -32)], 2)
trg = profile("Trigger", tp, -3, 3, C, STEEL); bevel(trg, 0.5, 2); trg = bake_join("Trigger", [trg], STEEL); set_origin(trg, -118, 0, -30)
cp = chaikin([(-79.6, -37.6), (-86, -37.6), (-87.5, -46), (-91, -57), (-86.5, -58.5), (-83, -48), (-80.5, -42)], 1)
mc = profile("MagRelease", cp, -6.5, 6.5, C, STEEL); bevel(mc, 0.5, 2); mc = bake_join("MagRelease", [mc], STEEL); set_origin(mc, -84, 0, -40)
for o in C.objects: finish(o, 35)
result = {"objs": sorted(o.name for o in C.objects)}
