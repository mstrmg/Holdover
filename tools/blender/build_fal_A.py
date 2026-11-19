# FAL-style battle rifle (metric pattern), part A: reset, materials, receivers and action parts.
# mm, X forward, Y left, Z up, bore Z=0, bolt face X=0. Gray parkerized steel, black synthetic furniture.
import bpy, math
from mathutils import Vector, Matrix
globals().update(bpy.app.driver_namespace["hb"])
O = bpy.data.objects
for cn in ("Rifle_FAL", "EXPORT_Rifle_FAL", "_cutters", "_tmp"):
    cc = bpy.data.collections.get(cn)
    if cc:
        for ob in list(cc.objects):
            d = ob.data; bpy.data.objects.remove(ob, do_unlink=True)
            if d is not None and getattr(d, "users", 1) == 0:
                (bpy.data.meshes if isinstance(d, bpy.types.Mesh) else bpy.data.armatures).remove(d)
for m in list(bpy.data.meshes):
    if m.users == 0: bpy.data.meshes.remove(m)
C = coll("Rifle_FAL"); CUT = coll("_cutters"); CUT.hide_render = True
PARK = worn("M_FAL_Park", (0.072, 0.075, 0.079), 0.45, 0.62, (0.24, 0.24, 0.25), 1.0, 0.32, edge=0.45, scratch=0.6, wear_lo=0.5, wear_hi=0.72)
DARK = worn("M_FAL_Dark", (0.03, 0.031, 0.034), 0.6, 0.45, (0.25, 0.25, 0.26), 1.0, 0.3, edge=0.45, scratch=0.5, wear_lo=0.5, wear_hi=0.72)
POLY = worn("M_FAL_Synthetic", (0.018, 0.018, 0.019), 0.0, 0.6, (0.05, 0.05, 0.052), 0.0, 0.45, edge=0.35, scratch=0.4, wear_lo=0.55, wear_hi=0.8)
def bake_join(name, parts, mat):
    for p in parts: bake(p)
    ob = join(name, parts) if len(parts) > 1 else parts[0]
    ob.name = name; ob.data.name = name
    ob.data.materials.clear(); ob.data.materials.append(mat)
    for p in ob.data.polygons: p.material_index = 0
    return ob
# ---------------- upper receiver ----------------
u1 = profile("u_body", [(-205, -22), (-10, -22), (-10, 17), (-205, 17)], -16, 16, C, PARK)
u2 = xloft("u_front", [-10.5, 0, 12, 24, 25], lambda x: superq(16 - 0.6*max(0, x-12)/13, -22, 19 - 2*max(0, x-12)/13, n_top=2.6, n_bot=7, N=40), C, PARK)
u3 = lathe("u_ring", [(24, 17.5), (55, 17.5), (55, 14.2)], C, PARK, seg=40)
cuts = [box("c_uport", -95, -20, -17, 0.5, -7, 15, CUT),                      # ejection port (right)
        box("c_uchs", -150, 26, 11.5, 17, -2.5, 6.5, CUT),                   # charging handle slot (left)
        box("c_uwell", -79.5, -0.5, -14.0, 14.0, -24, -12, CUT),             # magazine well top
        box("c_urecL", -196, -118, 15, 17, -16, 8, CUT), box("c_urecR", -196, -118, -17, -15, -16, 8, CUT)]
cut(u1, cuts); bevel(u1, 1.0, 2, 40)
cut(u2, [box("c_uchs2", -12, 26, 11.5, 17, -2.5, 6.5, CUT), box("c_uwell2", -12, -0.5, -14.0, 14.0, -24, -12, CUT)])
d1 = xloft("u_deflector", [-113, -110, -104, -98, -95], lambda x: [(y - 16, z) for y, z in superq(6*math.sin(math.pi*(x+113)/18)+0.3, -5, 14, n_top=2.4, n_bot=2.4, N=24)], C, PARK)
rs1 = box("rs_base", -196, -160, -10, 10, 16, 28, C, PARK); bevel(rs1, 1, 2)
ear = chaikin([(-195, 26), (-163, 26), (-166, 41), (-171, 49.5), (-187, 49.5), (-192, 41)], 1, closed=True)
rs2 = profile("rs_earL", ear, 8.5, 11.5, C, PARK); rs3 = profile("rs_earR", ear, -11.5, -8.5, C, PARK)
for r_ in (rs2, rs3): bevel(r_, 0.6, 2)
pins = [cyl_y("u_pin%d" % i, y0, y0 + 0.8, 2.4, x, z, C, PARK, seg=14) for i, (x, z, y0) in enumerate([(-30, -16, 16), (-30, -16, -16.8), (-200.5, -14, 16), (-200.5, -14, -16.8)])]
up = bake_join("UpperReceiver", [u1, u2, u3, d1, rs1, rs2, rs3] + pins, PARK)
# top cover (dust cover)
def tc_sec(hw, zb, zt):
    pts = [(hw, zb)] + [(hw*math.cos(math.radians(a)), zb + (zt-zb)*math.sin(math.radians(a))**0.6) for a in range(10, 171, 10)] + [(-hw, zb)]
    return pts
tc = xsec("TopCover", tc_sec(16.4, 15.5, 24.5), -159, -8, C, PARK); bevel(tc, 0.5, 2, 40)
tc = bake_join("TopCover", [tc], PARK); set_origin(tc, -80, 0, 20)
# rear sight aperture (slides on the base, flips for range)
ap = profile("RearSightAperture", chaikin([(-184, 27.5), (-176, 27.5), (-176, 51), (-184, 51)], 1, closed=True), -4.5, 4.5, C, PARK)
cut(ap, [cyl("c_peep", -190, -170, 1.4, 0, 47, CUT, seg=16)]); bevel(ap, 0.4, 2)
ap = bake_join("RearSightAperture", [ap], PARK); set_origin(ap, -180, 0, 28)
# ---------------- lower receiver (trigger frame) ----------------
lo = profile("l_body", chaikin([(-205, -21.8), (30, -21.8), (30, -40), (22, -47), (-80, -47), (-86, -52), (-200, -52), (-206, -44)], 1, closed=True), -15, 15, C, PARK)
cut(lo, [box("c_lwell", -79.5, -0.5, -14.0, 14.0, -60, -20, CUT)]); bevel(lo, 1.0, 2, 40)
lp = []
for (x, z, r) in ((-199, -40, 4.2), (-122, -38, 2.5), (-140, -34, 2.5), (-45, -34, 2.2), (10, -32, 2.2)):
    lp.append(cyl_y("l_pinL", 15, 15.8, r, x, z, C, PARK, seg=14)); lp.append(cyl_y("l_pinR", -15.8, -15, r, x, z, C, PARK, seg=14))
tdl = profile("l_takedown", chaikin([(-204, -40), (-190, -40), (-186, -33), (-190, -26), (-204, -28)], 1, closed=True), 15.3, 17.6, C, PARK); bevel(tdl, 0.5, 2)
tgp = [(-93, -51), (-95, -64), (-102, -72.5), (-150, -73.5), (-160, -66), (-163, -51), (-157.5, -51), (-155.5, -63.5), (-149, -68.5), (-104, -68), (-99.5, -63), (-98, -51)]
tg = profile("l_guard", chaikin(tgp, 1, closed=True), -5.5, 5.5, C, PARK); bevel(tg, 0.6, 2)
lo = bake_join("LowerReceiver", [lo, tdl, tg] + lp, PARK)
# ---------------- action parts ----------------
car = box("bc_body", -150, -82.5, -11, 11, -9, 14.5, C, DARK); bevel(car, 1.5, 2)
car2 = box("bc_top", -83, 5, -11, 11, 9.3, 16, C, DARK); bevel(car2, 1.2, 2)
car = bake_join("BoltCarrier", [car, car2], DARK); set_origin(car, 0, 0, 2)
bo = box("b_body", -82, -0.4, -10, 10, -9.5, 9, C, DARK); bevel(bo, 1.2, 2)
bx = box("b_ext", -30, -4, -11.3, -9.6, -2.5, 3.5, C, DARK); bevel(bx, 0.5, 2)
bo = bake_join("Bolt", [bo, bx], DARK); set_origin(bo, -40, 0, 0)
ch1 = box("ch_arm", 6, 16, 11, 26, -1.5, 5.5, C, PARK); bevel(ch1, 1, 2)
ch2 = topview("ch_knob2", chaikin([(5, 25.5), (25, 25.5), (27, 30), (24, 34.5), (6, 34.5), (4, 30)], 1, closed=True), -2.5, 6.5, C, PARK)
bevel(ch2, 1.5, 3, 40)
chh = bake_join("ChargingHandle", [ch1, ch2], PARK); set_origin(chh, 11, 20, 2)
sp = [(-170 + 7*math.cos(math.radians(a)), -38 + 7*math.sin(math.radians(a))) for a in range(-60, 241, 20)]
sel = profile("s_hub", sp, 15.1, 17.4, C, PARK)
sl = profile("s_lever", chaikin([(-172, -33), (-194, -30), (-197, -33.5), (-194, -37), (-172, -42)], 1, closed=True), 16.6, 18.6, C, PARK)
for s_ in (sel, sl): bevel(s_, 0.4, 2)
sel = bake_join("Selector", [sel, sl], PARK); set_origin(sel, -170, 16.5, -38)
bcat = profile("BoltCatch", chaikin([(-58, -36), (-30, -34), (-28, -29), (-56, -30)], 1, closed=True), 15.1, 16.9, C, PARK); bevel(bcat, 0.4, 2)
bcat = bake_join("BoltCatch", [bcat], PARK); set_origin(bcat, -35, 16, -32)
trg = profile("Trigger", chaikin([(-117, -46), (-117, -52), (-116, -58), (-120, -64), (-125, -65.5), (-126, -63), (-122, -58), (-123, -52), (-123, -46)], 2), -3, 3, C, PARK)
bevel(trg, 0.5, 2); trg = bake_join("Trigger", [trg], PARK); set_origin(trg, -121, 0, -44)
mr = profile("MagRelease", chaikin([(-81.5, -46.5), (-89, -46.5), (-90.5, -54), (-93, -62), (-88.5, -63.5), (-85, -55)], 1, closed=True), -5, 5, C, PARK)
bevel(mr, 0.5, 2); mr = bake_join("MagRelease", [mr], PARK); set_origin(mr, -86, 0, -48)
for o in C.objects: finish(o, 35)
result = {"objs": sorted(o.name for o in C.objects)}
