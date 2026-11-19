# 1911-style .45 pistol (Government 5"), walnut double-diamond grips, blued steel.
# mm, X forward, Y left, Z up, bore Z=0, slide rear face X=0, breech face X=79.
import bpy, math
from mathutils import Vector, Matrix
globals().update(bpy.app.driver_namespace["hb"])
O = bpy.data.objects
for cn in ("Pistol_1911", "EXPORT_Pistol_1911", "_cutters", "_tmp", "Ammo_45ACP"):
    cc = bpy.data.collections.get(cn)
    if cc:
        for ob in list(cc.objects):
            d = ob.data; bpy.data.objects.remove(ob, do_unlink=True)
            if d is not None and getattr(d, "users", 1) == 0:
                (bpy.data.meshes if isinstance(d, bpy.types.Mesh) else bpy.data.armatures).remove(d)
C = coll("Pistol_1911"); CUT = coll("_cutters"); CUT.hide_render = True
STEEL = worn("M_1911_Steel", (0.018, 0.02, 0.026), 0.85, 0.3, (0.3, 0.3, 0.31), 1.0, 0.22, edge=0.5, scratch=0.7, wear_lo=0.48, wear_hi=0.7)
BAR = worn("M_1911_Barrel", (0.16, 0.16, 0.165), 1.0, 0.3, (0.4, 0.4, 0.41), 1.0, 0.22, edge=0.5, scratch=0.5, wear_lo=0.48, wear_hi=0.7)
W = wood("M_1911_Walnut", (0.05, 0.02, 0.008), (0.19, 0.085, 0.035), (0.1, 0.045, 0.018), rough=0.42, ring_scale=700, distortion=5, coat=0.3, along="Z")
# checkering (1.2 mm diamonds) with smooth diamonds around the screws
nt = W.node_tree; N = nt.nodes; L = nt.links
tc = next(n for n in N if n.bl_idname == "ShaderNodeTexCoord"); bump = next(n for n in N if n.bl_idname == "ShaderNodeBump")
def band(deg):
    mp = N.new("ShaderNodeMapping"); mp.inputs["Rotation"].default_value = (0, math.radians(deg), 0); L.new(tc.outputs["Object"], mp.inputs["Vector"])
    wv = N.new("ShaderNodeTexWave"); wv.wave_type = "BANDS"; wv.bands_direction = "X"; wv.wave_profile = "TRI"
    wv.inputs["Scale"].default_value = 2*math.pi/(20*0.0012); wv.inputs["Distortion"].default_value = 0.0; wv.inputs["Detail"].default_value = 0.0
    L.new(mp.outputs["Vector"], wv.inputs["Vector"]); return wv
w1 = band(45); w2 = band(-45)
mn = N.new("ShaderNodeMath"); mn.operation = "MINIMUM"; L.new(w1.outputs["Fac"], mn.inputs[0]); L.new(w2.outputs["Fac"], mn.inputs[1])
sep = N.new("ShaderNodeSeparateXYZ"); L.new(tc.outputs["Object"], sep.inputs["Vector"])
def diamond(cx, cz, size):
    a = N.new("ShaderNodeMath"); a.operation = "SUBTRACT"; L.new(sep.outputs["X"], a.inputs[0]); a.inputs[1].default_value = cx*MM
    b = N.new("ShaderNodeMath"); b.operation = "SUBTRACT"; L.new(sep.outputs["Z"], b.inputs[0]); b.inputs[1].default_value = cz*MM
    aa = N.new("ShaderNodeMath"); aa.operation = "ABSOLUTE"; L.new(a.outputs[0], aa.inputs[0])
    bb = N.new("ShaderNodeMath"); bb.operation = "ABSOLUTE"; L.new(b.outputs[0], bb.inputs[0])
    s = N.new("ShaderNodeMath"); s.operation = "ADD"; L.new(aa.outputs[0], s.inputs[0]); L.new(bb.outputs[0], s.inputs[1])
    lt = N.new("ShaderNodeMath"); lt.operation = "LESS_THAN"; L.new(s.outputs[0], lt.inputs[0]); lt.inputs[1].default_value = size*MM
    return lt
d1 = diamond(40, -47, 12); d2 = diamond(24, -104, 12)
mx = N.new("ShaderNodeMath"); mx.operation = "MAXIMUM"; L.new(d1.outputs[0], mx.inputs[0]); L.new(d2.outputs[0], mx.inputs[1])
hm = N.new("ShaderNodeMix"); hm.data_type = "FLOAT"; L.new(mx.outputs[0], hm.inputs["Factor"]); L.new(mn.outputs[0], hm.inputs["A"]); hm.inputs["B"].default_value = 0.75
bump.inputs["Strength"].default_value = 0.55; bump.inputs["Distance"].default_value = 0.0003
L.new(hm.outputs["Result"], bump.inputs["Height"])
def bake_join(name, parts, mat):
    for p in parts: bake(p)
    ob = join(name, parts) if len(parts) > 1 else parts[0]
    ob.name = name; ob.data.name = name
    ob.data.materials.clear(); ob.data.materials.append(mat)
    for p in ob.data.polygons: p.material_index = 0
    return ob
RK = math.radians(18); D = Vector((-math.sin(RK), -math.cos(RK))); F = Vector((math.cos(RK), -math.sin(RK))); M0 = Vector((59, -13)); MP = Vector((59, -18))
def gp(t, u): p = M0 + D*t + F*u; return (p.x, p.y)
# ---------------- slide ----------------
sl = profile("s_body", [(0, -12.5), (189, -12.5), (189, -17), (207, -17), (207, 11), (0, 11)], -11.6, 11.6, C, STEEL)
sx = [(11.6, -18), (11.6, 6.5), (11.1, 8.6), (9.6, 10.2), (7, 11), (-7, 11), (-9.6, 10.2), (-11.1, 8.6), (-11.6, 6.5), (-11.6, -18)]
intersect(sl, xsec("c_sx", sx, -5, 215, CUT))
nose = profile("c_nose", [(186, -18), (210, -18), (210, -14.5), (186, -12.4)], -20, 20, CUT)
cs = [box("c_port", 82, 115, -20, 1.5, -1.5, 14, CUT), cyl("c_bore", 175, 215, 8.7, 0, 0, CUT), cyl("c_bush", 203, 215, 10.7, 0, 0, CUT),
      cyl("c_plug", 195, 215, 5.0, 0, -11.2, CUT, seg=24), box("c_ssn", 70, 76, -15, 15, -13, -10.5, CUT)]
for i in range(12):
    x = 7 + i*2.6
    cs += [box("c_sL%d" % i, x, x + 1.1, 10.7, 13, -10, 8, CUT), box("c_sR%d" % i, x, x + 1.1, -13, -10.7, -10, 8, CUT)]
cut(sl, cs); bevel(sl, 0.5, 2, 40)
rs = profile("s_rsight", [(13, 10.8), (24, 10.8), (24, 15), (22, 17), (13, 17)], -5.5, 5.5, C, STEEL)
cut(rs, [box("c_rnotch", 10, 27, -1.3, 1.3, 15.5, 20, CUT)]); bevel(rs, 0.4, 2)
fs = profile("s_fsight", chaikin([(183, 10.6), (192, 10.6), (191, 15.5), (186, 15.8), (184.5, 15.2)], 1, closed=True), -1.4, 1.4, C, STEEL)
fpp = box("s_fpstop", -0.6, 0.2, -5.5, 5.5, -7, 6, C, STEEL); fpd = cyl("s_fpin", -0.9, 0, 1.8, 0, 0, C, STEEL, seg=12)
slide = bake_join("Slide", [sl, rs, fs, fpp, fpd], STEEL)
bush = lathe("Bushing", [(203.3, 8.75), (203.3, 10.6), (208, 10.6), (209, 10.2), (209, 8.75), (203.3, 8.75)], C, STEEL, seg=40)
plug = lathe("RecoilPlug", [(196, 0.1), (196, 4.9), (208.5, 4.9), (209.5, 4.3), (209.5, 0.1)], C, STEEL, seg=24); move(plug, 0, 0, -11.2)
bush = bake_join("Bushing", [bush], STEEL); plug = bake_join("RecoilPlug", [plug], STEEL)
# ---------------- barrel ----------------
b1 = lathe("b_tube", [(79, 8.65), (207, 8.65), (207, 5.75), (150, 5.75)], C, BAR, seg=40)
b2 = box("b_hood", 79, 106, -7.4, 7.4, 2, 9.6, C, BAR); bevel(b2, 0.6, 2)
b3 = profile("b_link", [(80, -6), (95, -6), (95, -15), (91, -20), (85, -20), (80, -15)], -3.5, 3.5, C, BAR)
b4 = cyl("b_breech", 79, 79.6, 5.9, 0, 0, C, BAR, seg=24)
barrel = bake_join("Barrel", [b1, b2, b3], BAR); bpy.data.objects.remove(b4, do_unlink=True); set_origin(barrel, 92, 0, -17)
# ---------------- frame ----------------
front = [gp(t, 19.5) for t in range(30, 113, 8)] + [gp(112, 19.5)]
fpts = [(1, -12.4), (188.5, -12.4), (190, -14), (190, -21), (186, -23.5), (110, -23.5), (108, -27), (108, -42), (104, -48.5), (97, -51), (78, -51), (70, -48.5)] + \
       front + [gp(113.5, 17), gp(113.5, -27)] + [gp(112, -29.5), (2, -75), (6, -45), (8, -30), (4, -25), (1, -21)]
fr = profile("f_body", chaikin(fpts, 1, closed=True), -10.8, 10.8, C, STEEL)
fx = [(10.8, -130), (10.8, -15.5), (10.2, -13.2), (9.4, -12.4), (-9.4, -12.4), (-10.2, -13.2), (-10.8, -15.5), (-10.8, -130)]
intersect(fr, xsec("c_fx", fx, -20, 200, CUT))
grd = profile("c_guard", chaikin([(74.5, -24.5), (104.5, -24.5), (104.5, -41), (100.5, -46.5), (96, -47.8), (79, -47.8), (73.5, -44.5)], 1, closed=True), -20, 20, CUT)
mw = box("c_mwell", -16.8, 16.8, -7.0, 7.0, -130, 8, CUT); mw.data.transform(Matrix.Rotation(RK, 4, "Y")); move(mw, MP.x, 0, MP.y)
hs = box("c_hslot", -3, 12, -4.6, 4.6, -32, -11, CUT)
ts = box("c_trslot", 80, 96, -4.2, 4.2, -26, -22, CUT)
cut(fr, [grd, mw, hs, ts]); bevel(fr, 0.6, 2, 40)
pins = []
for side in (1, -1):
    for (x, z, r) in ((5, -21, 2.0), (22, -24, 1.7), (92, -17, 2.0)):
        y0 = 10.8 if side > 0 else -11.5
        pins.append(cyl_y("f_pin", y0, y0 + 0.7, r, x, z, C, STEEL, seg=14))
msh = [box("c_msh%d" % i, -30, 30, -20, 20, -0.5, 0.5, CUT) for i in range(9)]
for i, c_ in enumerate(msh):        # grooves across the mainspring housing (lower back strap)
    c_.data.transform(Matrix.Rotation(RK, 4, "Y")); p = gp(70 + i*4.8, -31.2); move(c_, p[0], 0, p[1])
cut(fr, msh)
frame = bake_join("Frame", [fr] + pins, STEEL)
# ---------------- grip panels + screws ----------------
gpts = chaikin([(67.7, -37), (40.7, -120), (1, -105.6), (12, -29), (24, -27)], 2, closed=True)
parts = []
for side in (1, -1):
    y0, y1 = (10.75, 15.4) if side > 0 else (-15.4, -10.75)
    p = profile("gp%d" % side, gpts, y0, y1, C, W); bevel(p, 1.8, 4, 50); parts.append(p)
panels = bake_join("GripPanels", parts, W)
scr = []
for side in (1, -1):
    for (x, z) in ((40, -47), (24, -104)):
        y0, y1 = (15.0, 16.1) if side > 0 else (-16.1, -15.0)
        s_ = cyl_y("scr", y0, y1, 3.5, x, z, C, STEEL, seg=20)
        sa, sb = (15.6, 17) if side > 0 else (-17, -15.6)
        cut(s_, [box("c_scs", x - 5, x + 5, sa, sb, z - 0.45, z + 0.45, CUT)]); scr.append(s_)
screws = bake_join("GripScrews", scr, STEEL)
# ---------------- small parts ----------------
hm = profile("Hammer", chaikin([(2, -26), (8, -24), (8, -18), (2, -13), (1.5, 0), (-0.5, 5), (-5, 9), (-12, 10.5), (-15, 8), (-11, 5), (-6, 1), (-4, -10), (-3, -22)], 1, closed=True), -3.6, 3.6, C, STEEL)
cut(hm, [box("c_hmg%d" % i, -16 + i*1.6, -15.4 + i*1.6, -5, 5, 6, 12, CUT) for i in range(6)]); bevel(hm, 0.5, 2)
hm = bake_join("Hammer", [hm], STEEL); set_origin(hm, 5, 0, -21)
tg = profile("Trigger", chaikin([(84.5, -24.5), (92, -24.5), (92, -29), (90, -41), (85.5, -41), (84.5, -29)], 1, closed=True), -4.4, 4.4, C, STEEL)
cut(tg, [box("c_tgg%d" % i, 90.5, 93, -5, 5, -38 + i*2, -37.2 + i*2, CUT) for i in range(6)]); bevel(tg, 0.6, 2)
tg = bake_join("Trigger", [tg], STEEL); set_origin(tg, 88, 0, -33)
gsp = chaikin([(6, -14.5), (-9, -16), (-10, -19.5), (-3, -21), (4, -26), (5.5, -45), (1.5, -70), (-2.5, -71), (1.5, -45), (0, -27), (5, -23)], 1, closed=True)
gs = profile("GripSafety", gsp, -6.5, 6.5, C, STEEL); bevel(gs, 0.8, 2)
gs = bake_join("GripSafety", [gs], STEEL); set_origin(gs, 2, 0, -18)
tsl = profile("t_lever", chaikin([(12, -16.5), (40, -16.8), (42, -19.5), (36, -21.5), (12, -24)], 1, closed=True), 10.9, 12.6, C, STEEL)
tsh = box("t_shelf", 30, 41, 12.6, 15, -19.5, -16.8, C, STEEL); tsx = cyl_y("t_hub", 10.9, 12.8, 3.6, 12, -20.3, C, STEEL, seg=16)
for p in (tsl, tsh): bevel(p, 0.4, 2)
tsf = bake_join("ThumbSafety", [tsl, tsh, tsx], STEEL); set_origin(tsf, 12, 12, -20.3)
ssl = profile("ss_lever", chaikin([(92, -14.5), (60, -13.4), (56, -16), (60, -21), (92, -20)], 1, closed=True), 10.9, 12.8, C, STEEL)
ssx = cyl_y("ss_hub", 10.9, 13.0, 3.5, 92, -17, C, STEEL, seg=16)
for p in (ssl,): bevel(p, 0.4, 2)
ss = bake_join("SlideStop", [ssl, ssx], STEEL); set_origin(ss, 92, 12, -17)
mr = cyl_y("MagRelease", 10.5, 12.6, 4.6, 66, -31, C, STEEL, seg=24); bevel(mr, 0.6, 2)
mr = bake_join("MagRelease", [mr], STEEL); set_origin(mr, 66, 12, -31)
# ---------------- 7-round magazine ----------------
mb = box("m_body", -16.5, 16.5, -6.7, 6.7, -113, 0, C, STEEL)
cut(mb, [box("c_mtop", -15, 15, -5.2, 5.2, -9, 1, CUT)] + [cyl_y("c_mw%d" % i, -8, 8, 1.4, -11, -28 - i*12.5, CUT, seg=12) for i in range(6)])
mbp = box("m_base", -17.5, 17.5, -7.6, 7.6, -116.5, -113, C, STEEL); bevel(mbp, 0.8, 2); bevel(mb, 0.6, 2)
lips = [box("m_lip%d" % s, -15, 4, s*4.2 - 0.9 if s > 0 else -5.1, s*4.2 + 0.9 if s > 0 else -3.3, -1.6, 0, C, STEEL) for s in (1, -1)]
mag = bake_join("Magazine", [mb, mbp] + lips, STEEL)
fol = box("MagFollower", -14.5, 14.5, -5, 5, -9, -5.5, C, STEEL); bevel(fol, 0.6, 2); fol = bake_join("MagFollower", [fol], STEEL)
for o in (mag, fol):
    o.data.transform(Matrix.Rotation(RK, 4, "Y")); o.location = (MP.x*MM, 0, MP.y*MM)
set_origin(fol, 0, 0, 0); parent_keep(fol, mag)
for o in C.objects: finish(o, 35)
# ---------------- ammo .45 ACP ----------------
A = coll("Ammo_45ACP")
brass = plain_mat("M_Brass", (0.78, 0.56, 0.24), 1.0, 0.28); copper = plain_mat("M_Copper", (0.75, 0.42, 0.28), 1.0, 0.3); primer = plain_mat("M_Primer", (0.7, 0.68, 0.62), 1.0, 0.35)
def prim(n): return lathe(n, [(-0.05, 0), (-0.05, 2.6), (0.15, 2.6), (0.15, 0)], A, primer, 16)
case = lathe("case", [(0, 0), (0, 6.03), (1.2, 6.03), (1.2, 5.0), (2.0, 5.0), (2.6, 6.0), (22.8, 5.95), (22.8, 0)], A, brass, 24)
bul = lathe("bullet", [(19, 0), (19, 5.72), (25, 5.72), (27, 5.3), (29, 4.4), (30.8, 3.0), (32, 1.4), (32.4, 0)], A, copper, 24)
rnd = join("Round_45ACP", [case, bul, prim("p1")]); finish(rnd, 30)
c2 = lathe("casing", [(0, 0), (0, 6.03), (1.2, 6.03), (1.2, 5.0), (2.0, 5.0), (2.6, 6.0), (22.8, 5.95), (22.8, 5.6), (3.5, 5.6), (3.5, 0)], A, brass, 24)
cas = join("Casing_45ACP", [c2, prim("p2")]); finish(cas, 30)
result = {"objs": sorted(o.name for o in C.objects)}
