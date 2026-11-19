import bpy, math
globals().update(bpy.app.driver_namespace["hb"])
G = coll("Rifle_FAL"); CUT = coll("_cutters"); TMP = coll("_tmp")
CUT.hide_render = True; TMP.hide_render = True
def to_G(o):
    for c in list(o.users_collection): c.objects.unlink(o)
    G.objects.link(o); return o
M = dict(
  park=worn("M_FAL_Park", (0.11,0.115,0.12), 0.55, 0.55, (0.45,0.45,0.47), 1.0, 0.3, edge=1.0, wear_lo=0.4, wear_hi=0.55),
  dark=worn("M_FAL_DarkPark", (0.05,0.052,0.056), 0.5, 0.5, (0.42,0.42,0.44), 1.0, 0.3, edge=0.8),
  poly=worn("M_FAL_Synthetic", (0.022,0.022,0.022), 0.0, 0.62, (0.07,0.07,0.07), 0.0, 0.45, edge=0.7, scratch=0.5),
)
bpy.app.driver_namespace["fal_M"] = M

# ---- upper receiver (forged, flat sided) ----
up = profile("UpperReceiver", chaikin([(0,-22),(290,-22),(290,12),(282,16),(200,16),(195,14),(10,14),(0,10)],1,closed=True), -15, 15, TMP, M["park"]); bevel(up, 1.2, 2, 40)
cs = [box("c_upin", 6, 286, -12.5, 12.5, -23, 11, CUT),
      box("c_ejport", 112, 200, -20, -9, -10, 11, CUT),
      box("c_chslot", 110, 282, 9, 20, -4.5, 0.5, CUT),
      box("c_lighten1", 215, 280, 14, 20, -18, -6, CUT), box("c_lighten2", 215, 280, -20, -14, -18, -6, CUT)]
cut(up, cs); bake(up)
defl = profile("Deflector", chaikin([(100,-6),(118,-6),(118,12),(108,12),(100,4)],1,closed=True), -24, -14, TMP, M["park"]); bevel(defl, 1.0, 2); bake(defl)
cover = xsec("TopCover", [(13,14)] + [(13*math.cos(math.radians(a)), 14+6*math.sin(math.radians(a))) for a in range(15, 166, 15)] + [(-13,14)], 12, 196, TMP, M["park"]); bake(cover)
rsb = profile("RearSightBase", [(18,14),(52,14),(52,22),(18,22)], -11, 11, TMP, M["park"]); bevel(rsb, 0.8, 2); bake(rsb)
ears = [profile("rsear%d" % i, chaikin([(20,20),(46,20),(42,40),(26,40)],1,closed=True), y0, y0+2.5, TMP, M["park"]) for i, y0 in enumerate((8, -10.5))]
up = to_G(join("UpperReceiver", [up, defl, cover, rsb] + ears)); finish(up)
ap = profile("RearSightAperture", chaikin([(30,20),(38,20),(38,40),(30,40)],1,closed=True), -4.5, 4.5, G, M["park"])
cut(ap, [cyl("c_peep", 25, 45, 1.6, 0, 36, CUT, seg=16)]); bevel(ap, 0.5, 2); bake(ap); finish(ap); set_origin(ap, 34, 0, 21)

# ---- lower receiver / trigger housing ----
lo = profile("LowerReceiver", chaikin([(0,-22),(236,-22),(236,-34),(226,-46),(150,-46),(146,-52),(108,-52),(96,-46),(10,-44),(0,-36)],1,closed=True), -14, 14, TMP, M["park"])
bevel(lo, 1.2, 2, 40)
cut(lo, [box("c_magwell", 152, 228, -12, 12, -60, -20, CUT), box("c_trslot", 120, 145, -4, 4, -60, -40, CUT)]); bake(lo)
pins = []
for sy in (1, -1):
    y0, y1 = sorted((13.8*sy, 15.0*sy))
    for (x, z, r) in ((236,-28,4.0),(132,-36,2.4),(112,-32,2.4),(70,-34,2.0),(30,-30,2.0)):
        pins.append(cyl_y("pin", y0, y1, r, x, z, TMP, M["park"], seg=16))
tdl = profile("TakedownLever", [(-2,-30),(10,-30),(14,-24),(10,-18),(-2,-20)], 14, 16.5, TMP, M["park"]); bevel(tdl, 0.5, 2); bake(tdl)
lo = to_G(join("LowerReceiver", [lo] + pins + [tdl])); finish(lo)

# ---- pistol grip, trigger guard, trigger, selector, mag release, bolt catch ----
pg = profile("PistolGrip", chaikin([(96,-44),(140,-46),(142,-54),(122,-128),(117,-136),(100,-139),(84,-135),(80,-126),(98,-52)],2,closed=True), -15, 15, TMP, M["poly"])
bevel(pg, 5.0, 4, 30); bake(pg)
cut(pg, [box("c_pgg%d" % i, 60, 160, -16, -14.2, -125+i*8, -122+i*8, CUT) for i in range(9)] + [box("c_pgh%d" % i, 60, 160, 14.2, 16, -125+i*8, -122+i*8, CUT) for i in range(9)]); bake(pg)
to_G(pg); finish(pg, 40)
tg = profile("TriggerGuard", chaikin([(140,-46),(206,-46),(206,-56),(200,-68),(188,-72),(156,-72),(144,-66),(140,-58)],2,closed=True), -6, 6, TMP, M["park"])
cut(tg, [profile("c_tgh", chaikin([(146,-40),(200,-40),(198,-60),(188,-66),(158,-66),(148,-60)],2,closed=True), -10, 10, CUT)]); bevel(tg, 0.8, 2); bake(tg); to_G(tg); finish(tg)
trig = profile("Trigger", chaikin([(130,-42),(138,-42),(138,-50),(135,-58),(130,-64),(125,-66),(123,-64),(127,-58),(131,-50)],2,closed=True), -3, 3, G, M["park"]); bevel(trig, 0.9, 2); bake(trig); finish(trig)
set_origin(trig, 134, 0, -44)
sel = profile("Selector", chaikin([(98,-36),(104,-36),(104,-56),(98,-56)],1,closed=True), 14.2, 16.4, TMP, M["park"])
sax = cyl_y("selax", 14.0, 17.0, 6.0, 101, -38, TMP, M["park"], seg=20)
sel = to_G(join("Selector", [sel, sax])); bevel(sel, 0.5, 2, 50); bake(sel); finish(sel); set_origin(sel, 101, 15, -38)
mr = profile("MagRelease", chaikin([(204,-44),(212,-44),(212,-62),(204,-62)],1,closed=True), -5, 5, G, M["park"]); bevel(mr, 0.8, 2); bake(mr); finish(mr); set_origin(mr, 208, 0, -46)
bct = profile("BoltCatch", chaikin([(196,-30),(222,-30),(224,-36),(198,-36)],1,closed=True), 14, 16, G, M["park"]); bevel(bct, 0.5, 2); bake(bct); finish(bct); set_origin(bct, 220, 15, -33)

# ---- bolt carrier, bolt, charging handle (left, non-reciprocating) ----
bc = profile("BoltCarrier", [(20,-12),(196,-12),(196,9),(20,9)], -10, 10, TMP, M["dark"]); bevel(bc, 1.0, 2); bake(bc)
bcfront = cyl("bcrod", 196, 290, 6.0, 0, 6, TMP, M["dark"], seg=20)
bc = to_G(join("BoltCarrier", [bc, bcfront])); finish(bc)
bolt = profile("Bolt", [(196,-9),(250,-9),(254,-6),(254,6),(250,9),(196,9)], -9, 9, TMP, M["dark"]); bevel(bolt, 0.8, 2); bake(bolt)
bext = box("bext", 210, 252, -11, -9, 1, 5, TMP, M["dark"])
bolt = to_G(join("Bolt", [bolt, bext])); finish(bolt); set_origin(bolt, 200, 0, -4)
chd = profile("ChargingHandle", chaikin([(262,-4),(278,-4),(278,1),(262,1)],1,closed=True), 9, 30, TMP, M["park"])
knob = cyl_y("chknob", 24, 40, 6.5, 270, -1.5, TMP, M["park"], seg=20)
chd = to_G(join("ChargingHandle", [chd, knob])); bevel(chd, 0.8, 2, 50); bake(chd); finish(chd)
bpy.ops.wm.save_mainfile()
result = {o.name: len(o.data.polygons) for o in G.objects if o.type == "MESH"}
