import bpy, math
globals().update(bpy.app.driver_namespace["hb"])
G = coll("Pistol_1911"); CUT = coll("_cutters"); TMP = coll("_tmp")
CUT.hide_render = True; TMP.hide_render = True
def to_G(o):
    for c in list(o.users_collection): c.objects.unlink(o)
    G.objects.link(o); return o
RAKE = 18.0
T = math.tan(math.radians(RAKE))
M = dict(
  steel=worn("M_1911_Steel", (0.03,0.032,0.036), 1.0, 0.33, (0.5,0.5,0.52), 1.0, 0.22, edge=1.1),
  barrel=worn("M_1911_Barrel", (0.32,0.32,0.33), 1.0, 0.28, (0.6,0.6,0.62), 1.0, 0.2, edge=0.6),
)
# walnut with checkering and double-diamond (smooth diamonds around the screws)
w = wood("M_1911_Walnut", (0.07,0.03,0.012), (0.27,0.12,0.05), (0.15,0.065,0.025), rough=0.4, ring_scale=900, distortion=4, coat=0.3, along="Z")
nt = w.node_tree; N = nt.nodes; L = nt.links
b = next(n for n in N if n.bl_idname == "ShaderNodeBsdfPrincipled")
bump = next(n for n in N if n.bl_idname == "ShaderNodeBump")
tc = next(n for n in N if n.bl_idname == "ShaderNodeTexCoord")
def wave(rot_deg):
    mp = N.new("ShaderNodeMapping"); mp.inputs["Rotation"].default_value = (math.radians(rot_deg), 0, 0)
    L.new(tc.outputs["Object"], mp.inputs["Vector"])
    wv = N.new("ShaderNodeTexWave"); wv.wave_type = "BANDS"; wv.bands_direction = "Z"; wv.wave_profile = "TRI"
    wv.inputs["Scale"].default_value = 0.0; wv.inputs["Distortion"].default_value = 0.0
    # 1 band per 1.2 mm: Scale is in 1/m * (2*pi)?? use explicit: scale so period = 1.2 mm
    wv.inputs["Scale"].default_value = 1.0/0.0012/ (2*math.pi) * 2*math.pi
    L.new(mp.outputs["Vector"], wv.inputs["Vector"]); return wv
w1 = wave(45); w2 = wave(-45)
mul = N.new("ShaderNodeMath"); mul.operation = "MINIMUM"; L.new(w1.outputs["Fac"], mul.inputs[0]); L.new(w2.outputs["Fac"], mul.inputs[1])
# diamond mask: |y'| + |z'| in panel plane around each screw (X,Z in mm): (8,-55) and (25,-112) rotated by rake
sep = N.new("ShaderNodeSeparateXYZ"); L.new(tc.outputs["Object"], sep.inputs["Vector"])
def diamond(cx, cz, size):
    dx = N.new("ShaderNodeMath"); dx.operation = "SUBTRACT"; L.new(sep.outputs["X"], dx.inputs[0]); dx.inputs[1].default_value = cx*0.001
    dz = N.new("ShaderNodeMath"); dz.operation = "SUBTRACT"; L.new(sep.outputs["Z"], dz.inputs[0]); dz.inputs[1].default_value = cz*0.001
    ax = N.new("ShaderNodeMath"); ax.operation = "ABSOLUTE"; L.new(dx.outputs[0], ax.inputs[0])
    az = N.new("ShaderNodeMath"); az.operation = "ABSOLUTE"; L.new(dz.outputs[0], az.inputs[0])
    s = N.new("ShaderNodeMath"); s.operation = "ADD"; L.new(ax.outputs[0], s.inputs[0]); L.new(az.outputs[0], s.inputs[1])
    lt = N.new("ShaderNodeMath"); lt.operation = "LESS_THAN"; L.new(s.outputs[0], lt.inputs[0]); lt.inputs[1].default_value = size*0.001
    return lt
d1 = diamond(9, -60, 11); d2 = diamond(24, -112, 11)
mx = N.new("ShaderNodeMath"); mx.operation = "MAXIMUM"; L.new(d1.outputs[0], mx.inputs[0]); L.new(d2.outputs[0], mx.inputs[1])
h = N.new("ShaderNodeMix"); h.data_type = "FLOAT"; L.new(mx.outputs[0], h.inputs["Factor"]); L.new(mul.outputs[0], h.inputs["A"]); h.inputs["B"].default_value = 0.6
bump.inputs["Strength"].default_value = 0.6; bump.inputs["Distance"].default_value = 0.0003
L.new(h.outputs["Result"], bump.inputs["Height"])
M["wood"] = w
bpy.app.driver_namespace["m1911_M"] = M

# ---- slide ----
sp = [(0,-16),(206,-16),(211,-12),(211,6),(208,10.5),(3,10.5),(0,8)]
sl = profile("Slide_new", sp, -11.75, 11.75, TMP, M["steel"]); bevel(sl, 0.8, 2, 40)
intersect(sl, xsec("c_xs", [(-11.75,-17),(11.75,-17),(11.75,4),(11.2,7),(9.8,9.3),(7.5,10.6),(4,11.2),(-4,11.2),(-7.5,10.6),(-9.8,9.3),(-11.2,7),(-11.75,4)], -5, 220, CUT))
cs = [box("c_cav", 4, 207, -9.2, 9.2, -17, 3, CUT), cyl("c_bch", 4, 215, 9.0, 0, 0, CUT), cyl("c_plug", 190, 215, 5.2, 0, -10.5, CUT, seg=20),
      box("c_ej", 62, 98, -20, -6, -1, 12, CUT), cyl("c_fp", -2, 6, 1.6, 0, 0, CUT, seg=12)]
for i in range(10):
    x = 9 + i*3.0
    cs += [box("c_sL%d" % i, x, x+1.4, 10.9, 13, -13.5, 7, CUT), box("c_sR%d" % i, x, x+1.4, -13, -10.9, -13.5, 7, CUT)]
cut(sl, cs); bake(sl)
rs = profile("RearSight", [(5,10),(15,10),(15,13.5),(13,15.5),(5,15.5)], -6, 6, TMP, M["steel"]); cut(rs, [box("c_n", 0, 20, -1.4, 1.4, 13.2, 20, CUT)]); bevel(rs, 0.4, 2); bake(rs)
fs = profile("FrontSight", chaikin([(195,10),(204,10),(203,15.5),(198,15.5)],1,closed=True), -1.3, 1.3, TMP, M["steel"])
bush = lathe("Bushing", [(205,8.7),(205,10.6),(211.5,10.6),(212.5,9.8),(212.5,8.7),(205,8.7)], TMP, M["steel"], 32)
plug = lathe("RecoilPlug", [(196,0),(196,5.0),(212,5.0),(212.5,4.4),(212.5,0)], TMP, M["steel"], 20); plug.location = (0,0,-10.5*MM); apply_tf(plug)
ext = box("Extractor", 2, 8, -12.2, -11.5, 2, 6, TMP, M["steel"])
sl = to_G(join("Slide", [sl, rs, fs, bush, plug, ext])); finish(sl)

# ---- barrel ----
bar = lathe("Barrel", [(4,5.7),(4,8.9),(30,8.9),(32,8.6),(212,8.6),(212,5.7),(4,5.7)], G, M["barrel"], 32)
hood = box("hood", 4, 30, -7.5, 7.5, 3, 9.6, TMP, M["barrel"])
lugs = box("lugs", 30, 44, -6, 6, 6, 9.6, TMP, M["barrel"])
link = profile("link", [(30,-6),(40,-6),(40,-14),(30,-14)], -3, 3, TMP, M["barrel"])
bar = join("Barrel", [bar, hood, lugs, link]); finish(bar, 40); set_origin(bar, 4, 0, 0)

# ---- frame ----
fp = [(6,-16),(204,-16),(206,-19),(206,-27),(124,-27)] + chaikin([(124,-27),(122,-34),(122,-52),(117,-56),(80,-56),(70,-52),(66,-47)],2)[1:] + \
     [(62,-60),(33+T*0,-137),(-21,-137)] + chaikin([(-21,-137),(-9,-80),(6,-44),(5,-30),(-4,-24),(-8,-22),(0,-18),(6,-16)],2)[1:-1]
fr = profile("Frame_new", fp, -11, 11, TMP, M["steel"]); bevel(fr, 1.0, 2, 35)
cs = [profile("c_guard", chaikin([(117,-30),(117,-50),(113,-52),(82,-52),(75,-48),(72,-42),(72,-30)],2,closed=True), -20, 20, CUT),
      box("c_ham", -14, 10, -4.5, 4.5, -32, -14, CUT), box("c_gs", -30, 8, -6, 6, -70, -24, CUT), box("c_trig", 76, 96, -4, 4, -32, -24, CUT)]
mw = box("c_mw", -14, 14, -11.2, 11.2, -150, 2, CUT); rot(mw, (0,1,0), RAKE, (0,0,0)); mw.data.transform(__import__("mathutils").Matrix.Translation((18*MM, 0, -20*MM)))
cs.append(mw)
cut(fr, cs); bake(fr)
# mainspring housing checkering (grooves) and frame front-strap
pins = []
for sy in (1, -1):
    y0, y1 = sorted((10.8*sy, 11.6*sy))
    for (x, z, r) in ((3,-22,1.9),(30,-25,1.6),(90,-24,1.5)):
        pins.append(cyl_y("pin", y0, y1, r, x, z, TMP, M["steel"], seg=14))
fr = to_G(join("Frame", [fr] + pins)); finish(fr)

# ---- wood grip panels (flat with chamfered edges), 2 screws each ----
gp = chaikin([(-6,-44),(48,-50),(36,-98),(30,-126),(-14,-126),(-12,-100),(-8,-70)], 1, closed=True)
parts = []
for s in (1, -1):
    y0, y1 = (11, 15.2) if s > 0 else (-15.2, -11)
    p = profile("Panel%d" % s, gp, y0, y1, TMP, M["wood"]); bevel(p, 1.6, 3, 50); bake(p); parts.append(p)
    for (x, z) in ((9, -60), (24, -112)):
        a, b2 = sorted((14.6*s, 15.8*s))
        sc_ = cyl_y("scr", a, b2, 3.6, x, z, TMP, M["steel"], seg=20)
        sa, sb = sorted((15.2*s, 17*s))
        cut(sc_, [box("c_sl", x-5, x+5, sa, sb, z-0.5, z+0.5, CUT)]); bake(sc_); parts.append(sc_)
grips = to_G(join("GripPanels", parts)); finish(grips, 50)

# ---- small parts ----
ham = profile("Hammer", chaikin([(-1,-27),(6,-25),(5,-19),(-0.5,-14),(-0.5,0),(-4,6),(-12,9),(-16,7),(-12,1),(-8,-12),(-7,-23)],1,closed=True), -3.5, 3.5, G, M["steel"])
bevel(ham, 0.7, 2); bake(ham); finish(ham); set_origin(ham, 3, 0, -22)
trig = profile("Trigger", chaikin([(86,-26),(96,-26),(96,-30),(92,-46),(84,-46),(84,-30)],1,closed=True), -4.5, 4.5, G, M["steel"])
bevel(trig, 0.8, 2); bake(trig); finish(trig); set_origin(trig, 90, 0, -36)
gs = profile("GripSafety", chaikin([(-2,-20),(4,-24),(2,-40),(-6,-62),(-12,-66),(-12,-56),(-8,-40),(-16,-24),(-8,-19)],1,closed=True), -6, 6, G, M["steel"])
bevel(gs, 1.0, 2); bake(gs); finish(gs); set_origin(gs, -3, 0, -21)
ts = profile("ThumbSafety", chaikin([(6,-26),(14,-28),(36,-24),(40,-20),(32,-18),(10,-18)],1,closed=True), 11, 13.4, TMP, M["steel"])
tsx = cyl_y("tsax", 10.8, 13.6, 3.5, 12, -22, TMP, M["steel"], seg=16)
ts = to_G(join("ThumbSafety", [ts, tsx])); bevel(ts, 0.4, 2, 50); bake(ts); finish(ts); set_origin(ts, 12, 12, -22)
ss = profile("SlideStop", chaikin([(70,-16),(102,-17),(104,-22),(92,-24),(70,-22),(66,-18)],1,closed=True), 11, 13.2, TMP, M["steel"])
ssx = cyl_y("ssax", 10.8, 13.4, 3.2, 101, -21, TMP, M["steel"], seg=16)
ss = to_G(join("SlideStop", [ss, ssx])); bevel(ss, 0.4, 2, 50); bake(ss); finish(ss); set_origin(ss, 101, 12, -21)
mr = cyl_y("MagRelease", 10.5, 12.8, 4.6, 64, -40, G, M["steel"], seg=20); finish(mr, 40); set_origin(mr, 64, 12, -40)

# ---- 7-round magazine (raked) ----
mag = box("Magazine", -14.5, 14.5, -10.5, 10.5, -118, 0, TMP, M["steel"]); bevel(mag, 1.0, 2)
cut(mag, [box("c_min", -13.2, 13.2, -9.2, 9.2, -116, 3, CUT)]); bake(mag)
holes = [cyl_y("c_wh%d" % i, -12, 12, 1.6, -6, -30-i*12, CUT, seg=12) for i in range(6)]
cut(mag, holes); bake(mag)
bp = box("mbase", -16, 16, -11.5, 11.5, -121, -118, TMP, M["steel"]); bevel(bp, 0.8, 2); bake(bp)
mag = to_G(join("Magazine", [mag, bp])); finish(mag)
fol = box("MagFollower", -13, 13, -9, 9, -6, -2, G, M["steel"]); bevel(fol, 0.6, 2); bake(fol); finish(fol)
for o in (mag, fol):
    o.data.transform(__import__("mathutils").Matrix.Rotation(math.radians(RAKE), 4, "Y"))
    o.location = (18*MM, 0, -20*MM)
parent_keep(fol, mag)

# ---- ammo .45 ACP ----
A = coll("Ammo_45ACP")
brass = plain_mat("M_Brass", (0.78,0.56,0.24), 1.0, 0.28); copper = plain_mat("M_Copper", (0.75,0.42,0.28), 1.0, 0.3); primer = plain_mat("M_Primer", (0.7,0.68,0.62), 1.0, 0.35)
def prim(n): return lathe(n, [(-0.05,0),(-0.05,2.6),(0.15,2.6),(0.15,0)], A, primer, 16)
case = lathe("case", [(0,0),(0,6.03),(1.2,6.03),(1.2,5.0),(2.0,5.0),(2.6,6.0),(22.8,5.95),(22.8,0)], A, brass, 24)
bul = lathe("bullet", [(19,0),(19,5.72),(25,5.72),(27,5.3),(29,4.4),(30.8,3.0),(32,1.4),(32.4,0)], A, copper, 24)
rnd = join("Round_45ACP", [case, bul, prim("p1")]); finish(rnd, 30)
c2 = lathe("casing", [(0,0),(0,6.03),(1.2,6.03),(1.2,5.0),(2.0,5.0),(2.6,6.0),(22.8,5.95),(22.8,5.6),(3.5,5.6),(3.5,0)], A, brass, 24)
cas = join("Casing_45ACP", [c2, prim("p2")]); finish(cas, 30)
bpy.ops.wm.save_mainfile()
result = {o.name: len(o.data.polygons) for o in G.objects if o.type == "MESH"}
