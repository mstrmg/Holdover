import bpy, math
globals().update(bpy.app.driver_namespace["hb"])
M = bpy.app.driver_namespace["fal_M"]; G = coll("Rifle_FAL"); CUT = coll("_cutters"); TMP = coll("_tmp")
def to_G(o):
    for c in list(o.users_collection): c.objects.unlink(o)
    G.objects.link(o); return o

# ---- barrel, flash hider ----
bar = lathe("Barrel", [(286,4.0),(286,11),(330,11),(336,9.2),(560,8.4),(565,7.6),(782,7.4),(790,6.6),(790,4.0),(286,4.0)], G, M["dark"], 32); finish(bar, 40)
fh = lathe("FlashHider", [(780,3.9),(780,10.0),(842,10.0),(842,3.9),(780,3.9)], TMP, M["dark"], 32)
cut(fh, [rot(box("c_fhs%d" % i, 798, 840, -1.6, 1.6, 0, 14, CUT), (1,0,0), i*60, (0,0,0)) for i in range(6)] + [cyl("c_fhin", 795, 850, 8.6, 0, 0, CUT)]); bake(fh)
to_G(fh); finish(fh, 40)

# ---- handguard (synthetic, vented) ----
hg = xsec("Handguard", rounded_rect_yz(54, 50, 16, 0, -2, 6), 292, 520, TMP, M["poly"])
cut(hg, [cyl("c_hgin", 280, 540, 13.0, 0, 0, CUT)] + [box("c_hgv%d_%d" % (i, s), 312+i*22, 324+i*22, -40, 40, -6+s*0, 4, CUT) for i in range(9) for s in (0,)]); bake(hg)
bevel(hg, 1.5, 3, 40); bake(hg)
hgcap = lathe("hgcap", [(284,10.8),(284,22),(292,22),(292,10.8),(284,10.8)], TMP, M["park"], 32); hgcap.location = (0,0,-2*MM); apply_tf(hgcap)
hgfr = lathe("hgfront", [(520,8.4),(520,21),(528,21),(528,8.4),(520,8.4)], TMP, M["park"], 32); hgfr.location = (0,0,-2*MM); apply_tf(hgfr)
hg = to_G(join("Handguard", [hg, hgcap, hgfr])); finish(hg, 40)

# ---- gas block, regulator, front sight, gas tube ----
gbk = profile("GasBlock", chaikin([(528,-12),(576,-12),(580,0),(580,28),(570,34),(534,34),(528,24)],1,closed=True), -11, 11, TMP, M["park"])
cut(gbk, [cyl("c_gbb", 500, 600, 7.7, 0, 0, CUT)]); bevel(gbk, 1.0, 2); bake(gbk)
fsp = cyl_z("frontpost", 30, 36.5, 1.3, 566, 0, TMP, M["park"], seg=12)
fse = [profile("fse%d" % i, chaikin([(556,28),(578,28),(574,42),(560,42)],1,closed=True), y0, y0+2.5, TMP, M["park"]) for i, y0 in enumerate((6.5, -9))]
swv = profile("FrontSwivel", [(560,-24),(578,-24),(578,-12),(560,-12)], -2, 2, TMP, M["park"]); cut(swv, [box("c_sw", 564, 574, -5, 5, -21, -15, CUT)]); bake(swv)
lug = box("BayoLug", 540, 560, -5, 5, -22, -12, TMP, M["park"])
gt = cyl("gastube", 300, 532, 5.0, 0, 16, TMP, M["park"], seg=16)
gbk = to_G(join("GasBlock", [gbk, fsp] + fse + [swv, lug, gt])); finish(gbk)
reg = lathe("GasRegulator", [(580,0),(580,7),(592,7),(594,5.5),(594,0)], TMP, M["park"], 24); reg.location = (0,0,20*MM); apply_tf(reg)
cut(reg, [box("c_regn%d" % i, 586, 600, -8, 8, 18+(i-1)*4.0, 18.8+(i-1)*4.0, CUT) for i in range(3)]); bake(reg)
to_G(reg); finish(reg, 40); set_origin(reg, 587, 0, 20)

# ---- buttstock (synthetic) with ridged buttplate ----
sp = chaikin([(4,10),(-280,-4),(-292,-6),(-292,-150),(-282,-152),(-150,-110),(-60,-70),(-20,-52),(4,-44)], 2, closed=True)
st = profile("Stock", sp, -22, 22, TMP, M["poly"])
intersect(st, topview("c_taper", [(10,-15),(-60,-17),(-292,-21),(-292,21),(-60,17),(10,15)], -200, 50, CUT)); bake(st)
bevel(st, 4.0, 4, 35); bake(st)
bp = profile("Buttplate", chaikin([(-291,-5),(-300,-6),(-300,-151),(-291,-152)],1,closed=True), -21, 21, TMP, M["poly"])
intersect(bp, xsec("c_bpx", rounded_rect_yz(42, 152, 8, 0, -78, 4), -310, -280, CUT)); bake(bp)
cut(bp, [box("c_bpr%d" % i, -310, -298, -30, 30, -146+i*7, -143+i*7, CUT) for i in range(20)]); bevel(bp, 0.8, 2); bake(bp)
swr = profile("RearSwivel", [(-200,-128),(-180,-118),(-180,-112),(-200,-122)], -23.5, -21.5, TMP, M["park"])
st = to_G(join("Stock", [st, bp, swr])); finish(st, 40)

# ---- 20-round magazine (straight box, rocks in from the front lug) ----
mb = profile("Magazine", chaikin([(152,-44),(228,-44),(232,-120),(234,-160),(156,-164),(152,-120)],1,closed=True), -14, 14, TMP, M["park"]); bevel(mb, 2.0, 3, 35)
cut(mb, [box("c_mtop", 158, 224, -10.5, 10.5, -47, -38, CUT)]); bake(mb)
mt = profile("magtop", [(154,-44),(228,-44),(228,-34),(154,-34)], -11.5, 11.5, TMP, M["park"])
cut(mt, [box("c_mt2", 158, 224, -9, 9, -50, -30, CUT)]); bake(mt)
ribs = []
for sy in (1, -1):
    y0, y1 = sorted((13.6*sy, 14.8*sy))
    for z in (-70, -100, -130):
        r_ = profile("mrib", [(160,z-2),(226,z-2),(226,z+2),(160,z+2)], y0, y1, TMP, M["park"]); bevel(r_, 0.4, 2); bake(r_); ribs.append(r_)
fp = profile("floor", chaikin([(152,-160),(236,-156),(236,-166),(152,-170)],1,closed=True), -15.5, 15.5, TMP, M["park"]); bevel(fp, 0.8, 2); bake(fp)
flug = box("mflug", 226, 234, -5, 5, -42, -34, TMP, M["park"])
mag = to_G(join("Magazine", [mb, mt] + ribs + [fp, flug])); finish(mag, 40); set_origin(mag, 230, 0, -38)
fol = box("MagFollower", 160, 222, -8.5, 8.5, -44, -40, G, M["dark"]); bevel(fol, 0.8, 2); bake(fol); finish(fol); set_origin(fol, 190, 0, -42); parent_keep(fol, mag)

# ---- ammo 7.62x51 ----
A = coll("Ammo_762x51")
brass = plain_mat("M_Brass", (0.78,0.56,0.24), 1.0, 0.28); copper = plain_mat("M_Copper", (0.75,0.42,0.28), 1.0, 0.3); primer = plain_mat("M_Primer", (0.7,0.68,0.62), 1.0, 0.35)
def prim(n): return lathe(n, [(-0.05,0),(-0.05,2.7),(0.15,2.7),(0.15,0)], A, primer, 16)
case = lathe("case", [(0,0),(0,6.0),(1.4,6.0),(1.4,5.0),(2.4,5.0),(3.0,5.97),(39.6,5.4),(43.5,4.3),(51.2,4.3),(51.2,0)], A, brass, 24)
bul = lathe("bullet", [(44,0),(44,3.9),(56,3.9),(60,3.75),(64,3.2),(67.5,2.3),(70,1.2),(71,0.4),(71.1,0)], A, copper, 24)
rnd = join("Round_762x51", [case, bul, prim("p1")]); finish(rnd, 30)
cs = lathe("casing", [(0,0),(0,6.0),(1.4,6.0),(1.4,5.0),(2.4,5.0),(3.0,5.97),(39.6,5.4),(43.5,4.3),(51.2,4.3),(51.2,3.95),(43.5,3.95),(39.6,5.0),(4,5.5),(4,0)], A, brass, 24)
cas = join("Casing_762x51", [cs, prim("p2")]); finish(cas, 30)
bpy.ops.wm.save_mainfile()
result = {o.name: len(o.data.polygons) for o in G.objects if o.type == "MESH"}
