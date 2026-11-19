# AR-15-style carbine, part B: barrel, handguards, front sight, flash hider, stock, grip, rear sight, magazine, ammo.
import bpy, math
from mathutils import Vector, Matrix
globals().update(bpy.app.driver_namespace["hb"])
O = bpy.data.objects; C = coll("Carbine_AR"); CUT = coll("_cutters")
ANOD = bpy.data.materials["M_AR_Anodized"]; PHOS = bpy.data.materials["M_AR_Phosphate"]; POLY = bpy.data.materials["M_AR_Polymer"]
def bake_join(name, parts, mat):
    for p in parts: bake(p)
    ob = join(name, parts) if len(parts) > 1 else parts[0]
    ob.name = name; ob.data.name = name
    ob.data.materials.clear(); ob.data.materials.append(mat)
    for p in ob.data.polygons: p.material_index = 0
    return ob
# ---------------- barrel, barrel nut, delta ring, gas tube ----------------
bar = lathe("Barrel", [(0, 12.5), (20, 12.5), (20, 11), (222, 11), (222, 10.4), (300, 10.4), (300, 9.4), (306, 9.4), (306, 10.0), (400, 10.0), (400, 7.5), (404, 7.5), (404, 2.85), (330, 2.85)], C, PHOS, seg=40)
bar = bake_join("Barrel", [bar], PHOS)
bn = lathe("bn", [(19.8, 12), (19.8, 17), (40, 17), (40, 12), (19.8, 12)], C, PHOS, seg=40)
cut(bn, [rot(box("c_bn%d" % i, 25, 35, -2.5, 2.5, 15, 19, CUT), "X", i*30, (0, 0, 0)) for i in range(12)])
dr = lathe("dr", [(40, 11.2), (40, 19), (44, 24.5), (52, 25), (53, 23), (53, 11.2), (40, 11.2)], C, PHOS, seg=48)
gt = cyl("gastube", 20, 226, 2.4, 0, 13.5, C, PHOS, seg=12)
nut = bake_join("BarrelNut", [bn, dr, gt], PHOS)
# ---------------- handguard (M4-style ribbed, two halves) ----------------
def hg_ring(x):
    s = 1.0
    if x < 56: s = 0.95 + 0.05*(x-53)/3
    if x > 213: s = 0.95 + 0.05*(216-x)/3
    pts = []
    for i in range(96):
        th = 2*math.pi*i/96
        r = 21.6 + 1.0*max(0.0, math.cos(14*th))**6
        pts.append((r*s*math.cos(th), 3 + r*s*math.sin(th)))
    return pts
xs = [53.2, 54.2, 55.4, 56] + list(range(60, 212, 4)) + [213, 214.6, 215.6, 216]
hg = xloft("Handguard", xs, hg_ring, C, POLY)
vents = []
for i in range(6):
    for side in (1, -1):
        v = cyl_z("c_hv", 15, 30, 3.0, 70 + i*24, side*9, CUT, seg=16); v.data.transform(Matrix.Diagonal((1.8, 1, 1, 1)) if False else Matrix.Identity(4)); vents.append(v)
cut(hg, vents)
seam = [box("c_seamL", 50, 220, 21, 27, 2.6, 3.4, CUT), box("c_seamR", 50, 220, -27, -21, 2.6, 3.4, CUT)]
cut(hg, seam)
hg = bake_join("Handguard", [hg], POLY)
cap = lathe("hgcap", [(214, 11.2), (214, 23.2), (222.2, 23.2), (222.2, 11.2), (214, 11.2)], C, PHOS, seg=48); move(cap, 0, 0, 3)
cap = bake_join("HandguardCap", [cap], PHOS)
# ---------------- A2-style front sight base ----------------
f1 = xsec("fs_ring", superq(13.5, -13, 16, n_top=2.4, n_bot=2.4, N=40), 222.4, 249, C, PHOS)
tri = chaikin([(222.4, 12), (249, 12), (245, 34), (241, 52), (230, 52), (226, 34)], 1, closed=True)
f2 = profile("fs_tower", tri, -6.5, 6.5, C, PHOS)
cut(f2, [profile("c_fswin", chaikin([(231, 18), (241, 18), (238.5, 35), (233.5, 35)], 1, closed=True), -10, 10, CUT)])
ear = chaikin([(228, 48), (243, 48), (243, 60), (240, 68.5), (231, 68.5), (228, 60)], 1, closed=True)
f3 = profile("fs_earL", ear, 5.5, 8.6, C, PHOS); f4 = profile("fs_earR", ear, -8.6, -5.5, C, PHOS)
f5 = box("fs_bridge", 228, 243, -8.6, 8.6, 46, 50.5, C, PHOS)
f6 = box("fs_postbase", 233.5, 238.5, -2.5, 2.5, 50, 58, C, PHOS)
f7 = profile("fs_post", [(234.6, 57), (237.4, 57), (237, 66), (235, 66)], -1.4, 1.4, C, PHOS)
f8 = box("fs_lug", 238, 250, -4.5, 4.5, -22, -11, C, PHOS)
f9 = box("fs_swtab", 226, 234, -2.5, 2.5, -20, -11, C, PHOS)
f10 = torus_x("fs_swring", 6, 1.3, C, PHOS); f10.rotation_euler = (0, 0, math.radians(90)); apply_tf(f10); move(f10, 230, 0, -25)
pins = [cyl_y("fs_pin%d" % i, -14.2, 14.2, 1.6, x, -6, C, PHOS, seg=12) for i, x in enumerate((227, 244))]
for f in (f1, f2, f3, f4, f5, f6, f8, f9): bevel(f, 0.6, 2, 40)
fsb = bake_join("FrontSightBase", [f1, f2, f3, f4, f5, f6, f7, f8, f9, f10] + pins, PHOS)
# ---------------- A2-style flash hider ----------------
fh = lathe("FlashHider", [(398.5, 5.2), (398.5, 11), (444, 11), (447, 10.2), (447, 6), (404.5, 6), (404.5, 5.2), (398.5, 5.2)], C, PHOS, seg=40)
cut(fh, [rot(box("c_fhs%d" % i, 418, 443, -1.5, 1.5, 0, 14, CUT), "X", a, (0, 0, 0)) for i, a in enumerate((-90, -45, 0, 45, 90))])
fh = bake_join("FlashHider", [fh], PHOS)
# ---------------- collapsible stock (M4-style) ----------------
sp = chaikin([(-212, 18.5), (-376, 18.5), (-384, 22), (-389, 21), (-389, -96), (-381, -101), (-300, -62), (-236, -27), (-212, -19)], 1, closed=True)
st = profile("Stock", sp, -19, 19, C, POLY)
stx = [(19, -110), (19, 0), (17.5, 12), (14, 19), (8, 23), (-8, 23), (-14, 19), (-17.5, 12), (-19, 0), (-19, -110)]
intersect(st, xsec("c_stx", stx, -400, -200, CUT))
cut(st, [box("c_stslot", -372, -350, -4, 4, -96, -76, CUT), box("c_stwin", -360, -250, -20, 20, -40, -14, CUT)])
bevel(st, 1.5, 2, 35)
lev = box("st_lever", -236, -214, -6, 6, -27, -19, C, POLY); bevel(lev, 0.8, 2)
pad = profile("st_pad", chaikin([(-389, 21), (-395, 21), (-395, -97), (-389, -97)], 1, closed=True), -19.5, 19.5, C, POLY); bevel(pad, 2, 3)
stock = bake_join("Stock", [st, lev, pad], POLY); set_origin(stock, -300, 0, 0)
# ---------------- A2-style pistol grip ----------------
rk = math.radians(25); d = Vector((-math.sin(rk), -math.cos(rk))); f = Vector((math.cos(rk), -math.sin(rk))); a0 = Vector((-152, -32))
DU = [(-12, 16), (0, 16), (40, 17.5), (90, 17.5), (104, 17), (110.6, 17)]
DV = [(-12, 12.5), (0, 12.5), (50, 13.5), (100, 13.2), (110.6, 13.2)]
SC = [(-12, 1), (104, 1), (106.5, 0.95), (108.5, 0.85), (110, 0.6), (110.7, 0.2)]
ts = [-12, -6, 0] + list(range(6, 103, 6)) + [104, 106.5, 108.5, 110, 110.7]
rings = []
for t in ts:
    du = lerp_tab(DU, t)*lerp_tab(SC, t); dv = lerp_tab(DV, t)*lerp_tab(SC, t); ax = a0 + d*t; ring = []
    for i in range(40):
        th = 2*math.pi*i/40; ct, stt = math.cos(th), math.sin(th)
        u = du*math.copysign(abs(ct)**(2/2.3), ct); v = dv*math.copysign(abs(stt)**(2/2.3), stt)*(1 - 0.12*u/max(du, 0.1))
        if ct > 0.3: u += 4.5*math.exp(-((t-38)/7.0)**2)*ct          # A2 finger nub
        p = ax + f*u; ring.append((p.x, v, p.y))
    rings.append(ring)
gr = loft("PistolGrip", rings, C, POLY)
cut(gr, [rot(box("c_grg%d" % i, -260, -40, -30, 30, -0.5, 0.5, CUT), "Y", -25, (0, 0, 0)) for i in range(0)])
gr = bake_join("PistolGrip", [gr], POLY)
# ---------------- folding rear sight on the rail ----------------
r1 = box("rs_clamp", -152, -128, -12, 12, 24.8, 31, C, ANOD); bevel(r1, 1, 2)
rear_ear = chaikin([(-148, 30), (-132, 30), (-134, 60), (-136.5, 72), (-143.5, 72), (-146, 60)], 1, closed=True)
r2 = profile("rs_earL", rear_ear, 6, 9, C, ANOD); r3 = profile("rs_earR", rear_ear, -9, -6, C, ANOD)
r4 = profile("rs_ap", chaikin([(-142, 52), (-138, 52), (-138, 71), (-142, 71)], 1, closed=True), -5.5, 5.5, C, ANOD)
cut(r4, [cyl("c_rsap", -150, -130, 1.3, 0, 66, CUT, seg=16)])
r5 = box("rs_web", -146, -134, -9, 9, 30, 40, C, ANOD)
for r_ in (r2, r3, r5): bevel(r_, 0.5, 2)
rsight = bake_join("RearSight", [r1, r2, r3, r4, r5], ANOD); set_origin(rsight, -140, 0, 31)
# ---------------- 30-round magazine (slight curve) ----------------
X0, Z0, R = -29.0, -11.0, 620.0
def arc(s, u, v):
    th = max(0.0, s - 60)/R; base = Vector((X0, Z0 - min(s, 60)))
    c = base + Vector((R*(1 - math.cos(th)), -R*math.sin(th)))
    return (c.x + u*math.cos(th), v, c.y + u*math.sin(th))
sec = rounded_rect_yz(61, 22.4, 3, seg=4)
ss = [0, 2] + list(range(6, 181, 6)) + [181, 183.5]
body = loft("mg_body", [[arc(s, u, v) for u, v in sec] for s in ss], C, ANOD)
cut(body, [box("c_mtop", -57, -3, -9, 9, -24, -10, CUT)])
mp = [body]
fps = rounded_rect_yz(64, 25.6, 3, cy=0.5, seg=4)
fp = loft("mg_floor", [[arc(s, u, v) for u, v in fps] for s in (183, 186, 189)], C, ANOD); bevel(fp, 1, 2, 40); mp.append(fp)
for k, (uc, side) in enumerate([(-14, 1), (12, 1), (-14, -1), (12, -1)]):
    rs = [(uc + y, side*(10.6 + (z + 0.5))) for y, z in rounded_rect_yz(4, 1.0, 0.4, seg=2)]
    mp.append(loft("mg_rib%d" % k, [[arc(s, u, v) for u, v in rs] for s in range(30, 175, 6)], C, ANOD))
for side in (1, -1):
    y0, y1 = sorted((7.6*side, 9.4*side)); mp.append(box("mg_lip%d" % (side > 0), -56, -22, y0, y1, -12.6, -11, C, ANOD))
mag = bake_join("Magazine", mp, ANOD); set_origin(mag, -29, 0, -11)
fol = box("MagFollower", -56, -4, -8.8, 8.8, -24, -19.5, C, POLY); bevel(fol, 0.8, 2)
fol = bake_join("MagFollower", [fol], POLY); set_origin(fol, -29, 0, -21.7); parent_keep(fol, mag)
for o in C.objects: finish(o, 35)
# ---------------- ammo 5.56x45 ----------------
A = coll("Ammo_556")
brass = plain_mat("M_Brass", (0.78, 0.56, 0.24), 1.0, 0.28); copper = plain_mat("M_Copper", (0.75, 0.42, 0.28), 1.0, 0.3); primer = plain_mat("M_Primer", (0.7, 0.68, 0.62), 1.0, 0.35)
def prim(n): return lathe(n, [(-0.05, 0), (-0.05, 2.2), (0.15, 2.2), (0.15, 0)], A, primer, 16)
case = lathe("case", [(0, 0), (0, 4.7), (1.1, 4.7), (1.1, 3.9), (2.0, 3.9), (2.6, 4.79), (36.5, 4.5), (39.6, 3.1), (44.7, 3.1), (44.7, 0)], A, brass, 24)
bul = lathe("bullet", [(40, 0), (40, 2.85), (47, 2.85), (50, 2.6), (53, 2.0), (55.5, 1.2), (57.2, 0.4), (57.4, 0)], A, copper, 24)
rnd = join("Round_556", [case, bul, prim("p1")]); finish(rnd, 30)
cs = lathe("casing", [(0, 0), (0, 4.7), (1.1, 4.7), (1.1, 3.9), (2.0, 3.9), (2.6, 4.79), (36.5, 4.5), (39.6, 3.1), (44.7, 3.1), (44.7, 2.8), (39.6, 2.8), (36.5, 4.2), (3.5, 4.4), (3.5, 0)], A, brass, 24)
cas = join("Casing_556", [cs, prim("p2")]); finish(cas, 30)
result = {"objs": sorted(o.name for o in C.objects)}
