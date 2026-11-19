# AR-15-style carbine (16" barrel, flat-top, A2-style front sight, M4-style handguards and stock), part A: receivers + action.
# mm, X forward, Y left, Z up, bore Z=0, bolt face X=0. Black anodized aluminum, dark phosphate steel, black polymer.
import bpy, math
from mathutils import Vector, Matrix
globals().update(bpy.app.driver_namespace["hb"])
O = bpy.data.objects
for cn in ("Carbine_AR", "EXPORT_Carbine_AR", "_cutters", "_tmp", "Ammo_556"):
    cc = bpy.data.collections.get(cn)
    if cc:
        for ob in list(cc.objects):
            d = ob.data; bpy.data.objects.remove(ob, do_unlink=True)
            if d is not None and getattr(d, "users", 1) == 0:
                (bpy.data.meshes if isinstance(d, bpy.types.Mesh) else bpy.data.armatures).remove(d)
C = coll("Carbine_AR"); CUT = coll("_cutters"); CUT.hide_render = True
ANOD = worn("M_AR_Anodized", (0.02, 0.02, 0.022), 0.25, 0.48, (0.42, 0.42, 0.43), 1.0, 0.32, edge=0.45, scratch=0.6, wear_lo=0.5, wear_hi=0.72)
PHOS = worn("M_AR_Phosphate", (0.028, 0.029, 0.031), 0.5, 0.55, (0.22, 0.22, 0.23), 1.0, 0.3, edge=0.45, scratch=0.5, wear_lo=0.5, wear_hi=0.72)
POLY = worn("M_AR_Polymer", (0.017, 0.017, 0.018), 0.0, 0.62, (0.05, 0.05, 0.052), 0.0, 0.45, edge=0.35, scratch=0.4, wear_lo=0.55, wear_hi=0.8)
def bake_join(name, parts, mat):
    for p in parts: bake(p)
    ob = join(name, parts) if len(parts) > 1 else parts[0]
    ob.name = name; ob.data.name = name
    ob.data.materials.clear(); ob.data.materials.append(mat)
    for p in ob.data.polygons: p.material_index = 0
    return ob
# ---------------- upper receiver ----------------
up = profile("u_body", [(-158, -17), (20, -17), (20, 15), (-158, 15)], -15.5, 15.5, C, ANOD)
ux = [(15.5, -18), (15.5, 5), (12.5, 11.5), (10.8, 15.2), (-10.8, 15.2), (-12.5, 11.5), (-15.5, 5), (-15.5, -18)]
intersect(up, xsec("c_ux", ux, -170, 30, CUT))
ucut = [box("c_port", -95, -28, -17, 0.5, -8, 10, CUT), box("c_chch", -170, -130, -5.5, 5.5, 9, 16, CUT)]
cut(up, ucut); bevel(up, 0.8, 2, 40)
rail_sec = [(7.8, 14.5), (7.8, 18.2), (10.6, 21), (10.6, 23.2), (9.4, 25), (-9.4, 25), (-10.6, 23.2), (-10.6, 21), (-7.8, 18.2), (-7.8, 14.5)]
rail = xsec("u_rail", rail_sec, -150, 18, C, ANOD)
cut(rail, [box("c_rs%d" % i, -146 + i*10, -140.7 + i*10, -12, 12, 21.6, 26, CUT) for i in range(16)]); bevel(rail, 0.3, 1, 40)
fa1 = cyl("u_fahouse", -142, -100, 7.8, -15, 6, C, ANOD, seg=28)
fa2 = cyl("u_faplunger", -152, -141, 6.2, -15, 6, C, ANOD, seg=28)
dfl = xloft("u_deflect", [-114, -111, -105, -99, -96], lambda x: [(y - 15.2, z) for y, z in superq(5.5*math.sin(math.pi*(x+114)/18)+0.3, -4, 11, n_top=2.4, n_bot=2.4, N=24)], C, ANOD)
upper = bake_join("UpperReceiver", [up, rail, fa1, fa2, dfl], ANOD)
dc = profile("DustCover", chaikin([(-96, -9.5), (-27, -9.5), (-27, 10.5), (-96, 10.5)], 1, closed=True), -16.6, -15.7, C, ANOD)
dcr = box("dc_rib", -94, -29, -17.4, -16.5, 2, 5, C, ANOD); bevel(dcr, 0.4, 2)
dch = cyl("dc_hinge", -98, -25, 1.4, -16.6, -10, C, PHOS, seg=12)
dc = bake_join("DustCover", [dc, dcr, dch], ANOD); set_origin(dc, -62, -16.6, -10)
# ---------------- lower receiver ----------------
lp = [(-158, -16.8), (22, -16.8), (22, -24), (17, -31), (11, -33), (9, -62), (4, -66), (-66, -66), (-70, -62), (-72, -42), (-150, -42), (-158, -36),
      (-158, -30), (-170, -30), (-176, -22)]
lo = profile("l_body", chaikin(lp, 1, closed=True), -14.5, 14.5, C, ANOD)
tower = cyl("l_tower", -176, -157, 17, 0, 0, C, ANOD, seg=40)
intersect(tower, box("c_twr", -180, -150, -20, 20, -20, 8.5, CUT))   # flat top so it clears the charging handle
lw = box("c_lwell", -64.5, 7.5, -11.6, 11.6, -70, -10, CUT)
cut(lo, [lw]); bevel(lo, 0.8, 2, 40)
pins = []
for (x, z, r) in ((16, -24.5, 4.2), (-150, -24, 4.2), (-90, -31, 2.4), (-112, -29, 2.4)):
    pins.append(cyl_y("l_pinL", 14.5, 15.3, r, x, z, C, ANOD, seg=16)); pins.append(cyl_y("l_pinR", -15.3, -14.5, r, x, z, C, ANOD, seg=16))
tg = profile("l_guard", chaikin([(-71, -41.5), (-73, -53), (-78, -56), (-138, -56), (-141, -52), (-141, -41.5), (-138, -41.5), (-138, -51), (-136, -52.5), (-78, -52.5), (-75, -51), (-74, -41.5)], 1, closed=True), -5.5, 5.5, C, ANOD)
bevel(tg, 0.5, 2)
nut = lathe("l_castle", [(-182, 15.2), (-182, 17.5), (-176.5, 17.5), (-176.5, 15.2), (-182, 15.2)], C, PHOS, seg=40)
cut(nut, [rot(box("c_cn%d" % i, -183, -179, -2, 2, 15, 19, CUT), "X", i*45, (0, 0, 0)) for i in range(8)])
tube = cyl("l_buffertube", -362, -182, 14.6, 0, 0, C, ANOD, seg=40)
lower = bake_join("LowerReceiver", [lo, tower, tg, tube] + pins, ANOD)
castle = bake_join("CastleNut", [nut], PHOS)
# ---------------- action parts ----------------
bc1 = cyl("bc_body", -150, -12, 15, 0, 2, C, PHOS, seg=40)
bc2 = box("bc_key", -118, -88, -4.5, 4.5, 14, 21, C, PHOS); bevel(bc2, 0.8, 2)
bcg = bake_join("BoltCarrier", [bc1, bc2], PHOS); set_origin(bcg, -80, 0, 2)
bolt = cyl("Bolt", -12, -0.4, 9.6, 0, 0, C, PHOS, seg=32); bolt = bake_join("Bolt", [bolt], PHOS); set_origin(bolt, -6, 0, 0)
chs = box("ch_shaft", -160, -128, -5, 5, 10, 15.5, C, ANOD)
chw = topview("ch_wings", chaikin([(-171, -24), (-161, -24), (-158, -8), (-158, 8), (-161, 24), (-171, 24), (-172, 8), (-172, -8)], 1, closed=True), 9.5, 16.5, C, ANOD)
chl = topview("ch_latch", chaikin([(-172, 10), (-162, 10), (-162, 26), (-168, 27), (-172, 22)], 1, closed=True), 10, 15.5, C, ANOD)
for p in (chs, chw, chl): bevel(p, 0.8, 2, 40)
chh = bake_join("ChargingHandle", [chs, chw, chl], ANOD); set_origin(chh, -165, 0, 13)
trg = profile("Trigger", chaikin([(-87, -34), (-87, -42), (-88, -49), (-92, -55), (-97, -56.5), (-98, -54.5), (-94, -49), (-93, -42), (-93, -34)], 2), -3, 3, C, PHOS)
bevel(trg, 0.5, 2); trg = bake_join("Trigger", [trg], PHOS); set_origin(trg, -90, 0, -31)
sh = cyl_y("s_hub", 14.5, 16.2, 5.5, -128, -26, C, ANOD, seg=24)
sl = profile("s_lever", chaikin([(-130, -23.5), (-110, -24.5), (-108, -27), (-110, -29), (-130, -28.5)], 1, closed=True), 15.8, 17.8, C, ANOD); bevel(sl, 0.4, 2)
sel = bake_join("Selector", [sh, sl], ANOD); set_origin(sel, -128, 16, -26)
mrb = cyl_y("MagRelease", -18.5, -14.4, 5.2, -73, -36, C, ANOD, seg=24)
for i in range(4):
    pass
mrb = bake_join("MagRelease", [mrb], ANOD); set_origin(mrb, -73, -16, -36)
bct = profile("BoltCatch", chaikin([(-76, -20), (-62, -21), (-60, -29), (-66, -36), (-72, -34), (-74, -26)], 1, closed=True), 14.6, 16.8, C, ANOD)
bevel(bct, 0.5, 2); bct = bake_join("BoltCatch", [bct], ANOD); set_origin(bct, -68, 16, -22)
for o in C.objects: finish(o, 35)
result = {"objs": sorted(o.name for o in C.objects)}
