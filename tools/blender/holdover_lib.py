# Holdover Blender helpers for weapon building (mm units, X forward, Z up, bore axis Z=0).
import bpy, bmesh, math, os, shutil, time
import numpy as np
from mathutils import Vector, Matrix
MM = 0.001

def coll(name, parent=None):
    c = bpy.data.collections.get(name)
    if not c:
        c = bpy.data.collections.new(name)
        (parent or bpy.context.scene.collection).children.link(c)
    return c

def new_obj(name, bm, c, material=None):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); c.objects.link(ob)
    if material: me.materials.append(material)
    return ob

def profile(name, pts, y0, y1, c, material=None):
    bm = bmesh.new()
    vs = [bm.verts.new((x*MM, y0*MM, z*MM)) for x, z in pts]
    f = bm.faces.new(vs)
    r = bmesh.ops.extrude_face_region(bm, geom=[f])
    nv = [e for e in r["geom"] if isinstance(e, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=nv, vec=(0, (y1-y0)*MM, 0))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, c, material)

def topview(name, pts_xy, z0, z1, c, material=None):
    bm = bmesh.new()
    vs = [bm.verts.new((x*MM, y*MM, z0*MM)) for x, y in pts_xy]
    f = bm.faces.new(vs)
    r = bmesh.ops.extrude_face_region(bm, geom=[f])
    nv = [e for e in r["geom"] if isinstance(e, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=nv, vec=(0, 0, (z1-z0)*MM))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, c, material)

def xsec(name, pts_yz, x0, x1, c, material=None):
    bm = bmesh.new()
    vs = [bm.verts.new((x0*MM, y*MM, z*MM)) for y, z in pts_yz]
    f = bm.faces.new(vs); r = bmesh.ops.extrude_face_region(bm, geom=[f])
    nv = [e for e in r["geom"] if isinstance(e, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=nv, vec=((x1-x0)*MM, 0, 0)); bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, c, material)

def box(name, x0, x1, y0, y1, z0, z1, c, material=None):
    return profile(name, [(x0,z0),(x1,z0),(x1,z1),(x0,z1)], y0, y1, c, material)

def lathe(name, prof, c, material=None, seg=32):
    bm = bmesh.new(); rings = []
    for a, r in prof:
        rings.append([bm.verts.new((a*MM, r*math.cos(2*math.pi*i/seg)*MM, r*math.sin(2*math.pi*i/seg)*MM)) for i in range(seg)])
    for k in range(len(rings)-1):
        for i in range(seg):
            j = (i+1) % seg
            bm.faces.new((rings[k][i], rings[k][j], rings[k+1][j], rings[k+1][i]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bnd = [e for e in bm.edges if e.is_boundary]
    if bnd: bmesh.ops.holes_fill(bm, edges=bnd, sides=0)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, c, material)

def apply_tf(ob):
    ob.data.transform(ob.matrix_basis); ob.matrix_basis = Matrix.Identity(4)

def cyl(name, x0, x1, r, y, z, c, material=None, seg=32):
    ob = lathe(name, [(x0, r), (x1, r)], c, material, seg); ob.location = (0, y*MM, z*MM); apply_tf(ob); return ob

def cyl_y(name, y0, y1, r, x, z, c, material=None, seg=24):
    ob = lathe(name, [(y0, r), (y1, r)], c, material, seg)
    ob.rotation_euler = (0, 0, math.radians(90)); ob.location = (x*MM, 0, z*MM); apply_tf(ob); return ob

def cyl_z(name, z0, z1, r, x, y, c, material=None, seg=24):
    ob = lathe(name, [(z0, r), (z1, r)], c, material, seg)
    ob.rotation_euler = (0, math.radians(-90), 0); ob.location = (x*MM, y*MM, 0); apply_tf(ob); return ob

def rot(ob, axis, deg, pivot_mm):
    p = Vector(pivot_mm)*MM
    ob.data.transform(Matrix.Translation(p) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-p))
    return ob

def bevel(ob, w, seg=2, angle=40):
    m = ob.modifiers.new("Bevel", "BEVEL"); m.width = w*MM; m.segments = seg
    m.limit_method = "ANGLE"; m.angle_limit = math.radians(angle); return m

def cut(ob, cutters, union=False):
    for ct in cutters:
        m = ob.modifiers.new("Bool_"+ct.name, "BOOLEAN"); m.operation = "UNION" if union else "DIFFERENCE"
        m.object = ct; m.solver = "EXACT"; ct.hide_set(True); ct.hide_render = True

def intersect(ob, other):
    m = ob.modifiers.new("Int_"+other.name, "BOOLEAN"); m.operation = "INTERSECT"; m.object = other; m.solver = "EXACT"
    other.hide_set(True); other.hide_render = True

def snap_planes(ob, tol=3e-5):
    """remove tiny boolean precision errors that cause sliver-triangle shading streaks"""
    me = ob.data; n = len(me.vertices)
    if n == 0: return
    co = np.empty(n*3, np.float64); me.vertices.foreach_get("co", co); co = co.reshape(-1, 3)
    for ax in range(3):
        v = co[:, ax]
        keys, counts = np.unique(np.round(v, 6), return_counts=True)
        anchors = keys[counts >= 6]
        if len(anchors) == 0: continue
        idx = np.searchsorted(anchors, v)
        lo = anchors[np.clip(idx-1, 0, len(anchors)-1)]; hi = anchors[np.clip(idx, 0, len(anchors)-1)]
        near = np.where(np.abs(v-lo) < np.abs(v-hi), lo, hi)
        d = np.abs(v - near); mask = (d > 1e-9) & (d < tol)
        v[mask] = near[mask]; co[:, ax] = v
    me.vertices.foreach_set("co", co.astype(np.float32).ravel()); me.update()

def bake(ob, delete_cutters=True):
    cutters = [m.object for m in ob.modifiers if m.type == "BOOLEAN"]
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    old = ob.data; ob.modifiers.clear(); ob.data = me; me.name = ob.name
    if old.users == 0: bpy.data.meshes.remove(old)
    if delete_cutters:
        for ct in cutters:
            if ct and ct.name in bpy.data.objects:
                d = ct.data; bpy.data.objects.remove(ct, do_unlink=True)
                if d and d.users == 0: bpy.data.meshes.remove(d)
    clean_slots(ob); snap_planes(ob)
    return ob

def clean_slots(ob):
    for i in reversed(range(len(ob.material_slots))):
        if ob.material_slots[i].material is None:
            for p in ob.data.polygons:
                if p.material_index == i: p.material_index = 0
                elif p.material_index > i: p.material_index -= 1
            ob.data.materials.pop(index=i)

def join(name, obs):
    base = obs[0]
    with bpy.context.temp_override(active_object=base, selected_editable_objects=obs, selected_objects=obs):
        bpy.ops.object.join()
    base.name = name; base.data.name = name; clean_slots(base); return base

def finish(ob, angle=35):
    for p in ob.data.polygons: p.use_smooth = True
    ob.data.set_sharp_from_angle(angle=math.radians(angle))
    if not any(m.type == "WEIGHTED_NORMAL" for m in ob.modifiers):
        m = ob.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL"); m.keep_sharp = True

def set_origin(ob, x, y, z):
    p = Vector((x*MM, y*MM, z*MM)); ob.data.transform(Matrix.Translation(-p)); ob.location = ob.location + p

def empty(name, x, y, z, c, size=0.01):
    e = bpy.data.objects.get(name) or bpy.data.objects.new(name, None)
    e.empty_display_size = size; e.empty_display_type = "ARROWS"; e.location = (x*MM, y*MM, z*MM)
    if e.name not in c.objects: c.objects.link(e)
    return e

def parent_keep(child, parent):
    bpy.context.view_layer.update(); mw = child.matrix_world.copy(); child.parent = parent; child.matrix_world = mw

def chaikin(pts, it=2, closed=False, r=0.25):
    for _ in range(it):
        out = []; n = len(pts)
        if not closed: out.append(pts[0])
        for i in (range(n) if closed else range(n-1)):
            a = pts[i]; b = pts[(i+1) % n]
            out.append((a[0]+(b[0]-a[0])*r, a[1]+(b[1]-a[1])*r)); out.append((a[0]+(b[0]-a[0])*(1-r), a[1]+(b[1]-a[1])*(1-r)))
        if not closed: out.append(pts[-1])
        pts = out
    return pts

def ellipsoid(name, center, radii, rot_y_deg, c, useg=64, vseg=48):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=useg, v_segments=vseg, radius=1.0)
    ob = new_obj(name, bm, c); ob.scale = tuple(r*MM for r in radii)
    ob.rotation_euler = (0, math.radians(rot_y_deg), 0); ob.location = tuple(v*MM for v in center); apply_tf(ob); return ob

def rounded_rect_yz(w, h, r, cy=0, cz=0, seg=4):
    """cross-section points (y,z) of a rounded rectangle w x h centered at (cy,cz)"""
    pts = []
    for cyy, czz, a0 in ((w/2-r, h/2-r, 0), (-w/2+r, h/2-r, 90), (-w/2+r, -h/2+r, 180), (w/2-r, -h/2+r, 270)):
        for k in range(seg+1):
            a = math.radians(a0 + 90*k/seg)
            pts.append((cy+cyy+r*math.cos(a), cz+czz+r*math.sin(a)))
    return pts

# ---------------- materials ----------------
def plain_mat(name, color, metal=0.0, rough=0.5):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.bl_idname == "ShaderNodeBsdfPrincipled")
    b.inputs["Base Color"].default_value = (*color, 1); b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough; m.diffuse_color = (*color, 1); return m

def worn(name, base_col, base_met, base_rough, wear_col, wear_met, wear_rough, edge=1.0, scratch=1.0, bevel_r=0.0005, wear_lo=0.36, wear_hi=0.5):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True; nt = m.node_tree; nt.nodes.clear(); N = nt.nodes; L = nt.links
    def n(t, **kw):
        node = N.new(t)
        for k, v in kw.items(): setattr(node, k, v)
        return node
    out = n("ShaderNodeOutputMaterial"); bsdf = n("ShaderNodeBsdfPrincipled")
    geo = n("ShaderNodeNewGeometry"); bev = n("ShaderNodeBevel"); bev.inputs["Radius"].default_value = bevel_r; bev.samples = 12
    dot = n("ShaderNodeVectorMath", operation="DOT_PRODUCT"); L.new(geo.outputs["Normal"], dot.inputs[0]); L.new(bev.outputs["Normal"], dot.inputs[1])
    inv = n("ShaderNodeMath", operation="SUBTRACT"); inv.inputs[0].default_value = 1.0; L.new(dot.outputs["Value"], inv.inputs[1])
    eg = n("ShaderNodeMath", operation="MULTIPLY", use_clamp=True); eg.inputs[1].default_value = 10.8*edge; L.new(inv.outputs[0], eg.inputs[0])
    tc = n("ShaderNodeTexCoord")
    bn = n("ShaderNodeTexNoise"); bn.inputs["Scale"].default_value = 70.0; bn.inputs["Detail"].default_value = 8.0; L.new(tc.outputs["Object"], bn.inputs["Vector"])
    bm_ = n("ShaderNodeMath", operation="MULTIPLY"); L.new(eg.outputs[0], bm_.inputs[0]); L.new(bn.outputs["Fac"], bm_.inputs[1])
    wear = n("ShaderNodeMapRange"); wear.inputs["From Min"].default_value = wear_lo; wear.inputs["From Max"].default_value = wear_hi; L.new(bm_.outputs[0], wear.inputs["Value"])
    mp = n("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (40, 1800, 1800); L.new(tc.outputs["Object"], mp.inputs["Vector"])
    sn = n("ShaderNodeTexNoise"); sn.inputs["Scale"].default_value = 1.0; sn.inputs["Detail"].default_value = 3.0; L.new(mp.outputs["Vector"], sn.inputs["Vector"])
    sr = n("ShaderNodeMapRange"); sr.inputs["From Min"].default_value = 0.74; sr.inputs["From Max"].default_value = 0.78; L.new(sn.outputs["Fac"], sr.inputs["Value"])
    sm = n("ShaderNodeMath", operation="MULTIPLY"); sm.inputs[1].default_value = 0.35*scratch; L.new(sr.outputs["Result"], sm.inputs[0])
    tot = n("ShaderNodeMath", operation="ADD", use_clamp=True); L.new(wear.outputs["Result"], tot.inputs[0]); L.new(sm.outputs[0], tot.inputs[1])
    ao = n("ShaderNodeAmbientOcclusion"); ao.inputs["Distance"].default_value = 0.004; ao.samples = 12; ao.only_local = True
    rn = n("ShaderNodeTexNoise"); rn.inputs["Scale"].default_value = 60.0; L.new(tc.outputs["Object"], rn.inputs["Vector"])
    rv = n("ShaderNodeMapRange"); rv.inputs["To Min"].default_value = base_rough-0.08; rv.inputs["To Max"].default_value = base_rough+0.1; L.new(rn.outputs["Fac"], rv.inputs["Value"])
    col = n("ShaderNodeMix", data_type="RGBA"); col.inputs["A"].default_value = (*base_col, 1); col.inputs["B"].default_value = (*wear_col, 1); L.new(tot.outputs[0], col.inputs["Factor"])
    gr = n("ShaderNodeMix", data_type="RGBA", blend_type="MULTIPLY"); gr.inputs["B"].default_value = (0.55,0.53,0.5,1)
    gf = n("ShaderNodeMath", operation="SUBTRACT"); gf.inputs[0].default_value = 1.0; L.new(ao.outputs["AO"], gf.inputs[1])
    L.new(gf.outputs[0], gr.inputs["Factor"]); L.new(col.outputs["Result"], gr.inputs["A"])
    ro = n("ShaderNodeMix", data_type="FLOAT"); ro.inputs["B"].default_value = wear_rough; L.new(rv.outputs["Result"], ro.inputs["A"]); L.new(tot.outputs[0], ro.inputs["Factor"])
    me = n("ShaderNodeMix", data_type="FLOAT"); me.inputs["A"].default_value = base_met; me.inputs["B"].default_value = wear_met; L.new(tot.outputs[0], me.inputs["Factor"])
    bu = n("ShaderNodeBump"); bu.inputs["Strength"].default_value = 0.25; bu.inputs["Distance"].default_value = 0.00005; L.new(sm.outputs[0], bu.inputs["Height"])
    L.new(gr.outputs["Result"], bsdf.inputs["Base Color"]); L.new(ro.outputs["Result"], bsdf.inputs["Roughness"])
    L.new(me.outputs["Result"], bsdf.inputs["Metallic"]); L.new(bu.outputs["Normal"], bsdf.inputs["Normal"]); L.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    m.diffuse_color = (*base_col, 1); return m

def wood(name, dark, light, mid, rough=0.38, ring_scale=260.0, distortion=6.0, coat=0.35, along="X"):
    """procedural wood: rings around the given axis (grain runs along it)"""
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True; nt = m.node_tree; nt.nodes.clear(); N = nt.nodes; L = nt.links
    out = N.new("ShaderNodeOutputMaterial"); b = N.new("ShaderNodeBsdfPrincipled"); b.inputs["Roughness"].default_value = rough
    try: b.inputs["Coat Weight"].default_value = coat; b.inputs["Coat Roughness"].default_value = 0.2
    except Exception: pass
    tc = N.new("ShaderNodeTexCoord"); mp = N.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = {"X": (0.15, 1, 1), "Y": (1, 0.15, 1), "Z": (1, 1, 0.15)}[along]
    nz = N.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 18.0; nz.inputs["Detail"].default_value = 4.0
    mix = N.new("ShaderNodeMix"); mix.data_type = "VECTOR"; mix.inputs["Factor"].default_value = 0.012*distortion
    wave = N.new("ShaderNodeTexWave"); wave.wave_type = "RINGS"; wave.rings_direction = along
    wave.inputs["Scale"].default_value = ring_scale/10.0; wave.inputs["Distortion"].default_value = 2.0
    wave.inputs["Detail"].default_value = 3.0; wave.inputs["Detail Scale"].default_value = 1.5
    ramp = N.new("ShaderNodeValToRGB"); e = ramp.color_ramp.elements
    e[0].position = 0.0; e[0].color = (*dark, 1); e[1].position = 1.0; e[1].color = (*light, 1); e.new(0.5).color = (*mid, 1)
    pores = N.new("ShaderNodeTexNoise"); pores.inputs["Scale"].default_value = 2500.0
    pm = N.new("ShaderNodeMix"); pm.data_type = "RGBA"; pm.blend_type = "MULTIPLY"; pm.inputs["Factor"].default_value = 0.25
    bump = N.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.08; bump.inputs["Distance"].default_value = 0.0002
    L.new(tc.outputs["Object"], mp.inputs["Vector"]); L.new(mp.outputs["Vector"], nz.inputs["Vector"])
    L.new(mp.outputs["Vector"], mix.inputs["A"]); L.new(nz.outputs["Color"], mix.inputs["B"])
    L.new(mix.outputs["Result"], wave.inputs["Vector"]); L.new(wave.outputs["Fac"], ramp.inputs["Fac"])
    L.new(ramp.outputs["Color"], pm.inputs["A"]); L.new(tc.outputs["Object"], pores.inputs["Vector"]); L.new(pores.outputs["Color"], pm.inputs["B"])
    L.new(pm.outputs["Result"], b.inputs["Base Color"]); L.new(wave.outputs["Fac"], bump.inputs["Height"])
    L.new(bump.outputs["Normal"], b.inputs["Normal"]); L.new(b.outputs["BSDF"], out.inputs["Surface"])
    m.diffuse_color = (*mid, 1); return m

# ---------------- preview renders ----------------
def setup_preview(target=(0.09, 0, -0.055)):
    sc = bpy.context.scene; P = coll("_preview")
    def light(name, loc, energy, size):
        l = bpy.data.objects.get(name)
        if not l:
            l = bpy.data.objects.new(name, bpy.data.lights.new(name, "AREA")); P.objects.link(l)
        l.data.energy = energy; l.data.size = size; l.location = loc
        l.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    light("Key", (target[0]+0.1, 0.6, 0.45), 14, 0.6); light("Fill", (target[0]-0.4, -0.5, 0.15), 5, 0.9); light("Rim", (target[0], -0.15, 0.7), 7, 0.5)
    cam = bpy.data.objects.get("PreviewCam")
    if not cam: cam = bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam")); P.objects.link(cam)
    sc.camera = cam
    w = sc.world or bpy.data.worlds.new("World"); sc.world = w; w.use_nodes = True
    bg = next(n for n in w.node_tree.nodes if n.bl_idname == "ShaderNodeBackground")
    bg.inputs[0].default_value = (0.5, 0.52, 0.55, 1); bg.inputs[1].default_value = 0.35
    sc.render.engine = "CYCLES"; sc.cycles.device = "GPU"; sc.cycles.samples = 64; sc.cycles.use_denoising = True
    sc.render.resolution_x = 1920; sc.render.resolution_y = 1080
    return cam

def shoot(folder, name, loc, tgt, lens=85, samples=64):
    sc = bpy.context.scene; cam = bpy.data.objects["PreviewCam"]; sc.cycles.samples = samples
    cam.data.lens = lens; cam.location = loc
    cam.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    os.makedirs(os.path.join(folder, "renders"), exist_ok=True)
    sc.render.filepath = os.path.join(folder, "renders", name + ".png"); bpy.ops.render.render(write_still=True)

# ---------------- UV + bake ----------------
def unwrap(obs, margin=0.003):
    area = next(a for w in bpy.context.window_manager.windows for a in w.screen.areas if a.type == "VIEW_3D")
    win = next(w for w in bpy.context.window_manager.windows if area in w.screen.areas[:])
    region = next(r for r in area.regions if r.type == "WINDOW")
    for o in bpy.context.view_layer.objects: o.select_set(False)
    for o in obs:
        o.hide_set(False); o.select_set(True)
        if not o.data.uv_layers: o.data.uv_layers.new(name="UVMap")
    with bpy.context.temp_override(window=win, area=area, region=region, active_object=obs[0], selected_objects=obs, selected_editable_objects=obs):
        bpy.context.view_layer.objects.active = obs[0]
        bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(angle_limit=math.radians(50), island_margin=0.0, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
        bpy.ops.uv.select_all(action="SELECT"); bpy.ops.uv.average_islands_scale()
        bpy.ops.uv.pack_islands(udim_source="CLOSEST_UDIM", rotate=True, margin_method="FRACTION", margin=margin)
        bpy.ops.object.mode_set(mode="OBJECT")

def set_flat(obs, flat):
    st = bpy.app.driver_namespace.setdefault("smooth_state", {})
    for o in obs:
        if flat:
            st[o.name] = [p.use_smooth for p in o.data.polygons]
            for p in o.data.polygons: p.use_smooth = False
        else:
            s = st.get(o.name)
            if s and len(s) == len(o.data.polygons): o.data.polygons.foreach_set("use_smooth", s)
            else:
                for p in o.data.polygons: p.use_smooth = True
        o.data.update()
        for m in o.modifiers:
            if m.type == "WEIGHTED_NORMAL": m.show_render = not flat; m.show_viewport = not flat

def bake_channel(prefix, obs, tex_dir, chan, samples=16, res=4096):
    """chan in COLOR, ROUGH, METAL, AO, NORMAL. Saves <prefix>_<chan>.png (raw, packed later)."""
    os.makedirs(tex_dir, exist_ok=True)
    name = "%s_raw_%s" % (prefix, chan)
    img = bpy.data.images.get(name) or bpy.data.images.new(name, res, res, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "sRGB" if chan == "COLOR" else "Non-Color"
    mats = sorted({s.material for o in obs for s in o.material_slots if s.material}, key=lambda m: m.name)
    temp = []
    for m in mats:
        nt = m.node_tree; N = nt.nodes; L = nt.links
        bsdf = next(n for n in N if n.bl_idname == "ShaderNodeBsdfPrincipled")
        out = next(n for n in N if n.bl_idname == "ShaderNodeOutputMaterial" and n.is_active_output)
        orig = out.inputs["Surface"].links[0].from_socket
        tn = N.new("ShaderNodeTexImage"); tn.image = img; tn.select = True; N.active = tn; temp.append((m, tn, orig))
        if chan != "NORMAL":
            em = N.new("ShaderNodeEmission")
            if chan == "AO":
                ao = N.new("ShaderNodeAmbientOcclusion"); ao.only_local = True; ao.samples = 16; ao.inputs["Distance"].default_value = 0.008
                L.new(ao.outputs["AO"], em.inputs["Color"]); temp.append((m, ao, None))
            else:
                inp = {"COLOR": "Base Color", "ROUGH": "Roughness", "METAL": "Metallic"}[chan]
                s = bsdf.inputs[inp]
                if s.is_linked: L.new(s.links[0].from_socket, em.inputs["Color"])
                else:
                    v = s.default_value
                    em.inputs["Color"].default_value = tuple(v) if chan == "COLOR" else (v, v, v, 1)
            L.new(em.outputs["Emission"], out.inputs["Surface"]); temp.append((m, em, None))
    sc = bpy.context.scene; sc.render.engine = "CYCLES"; sc.cycles.device = "GPU"; sc.cycles.samples = samples
    sc.render.bake.margin = 12; sc.render.bake.use_clear = True; sc.render.bake.use_selected_to_active = False
    for o in bpy.context.view_layer.objects: o.select_set(False)
    for o in obs: o.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    t = time.time()
    if chan == "NORMAL": bpy.ops.object.bake(type="NORMAL", normal_space="TANGENT")
    else: bpy.ops.object.bake(type="EMIT")
    img.filepath_raw = os.path.join(tex_dir, "%s_raw_%s.png" % (prefix, chan.lower())); img.file_format = "PNG"; img.save()
    for m, node, orig in temp:
        nt = m.node_tree
        if orig is not None:
            out = next(n for n in nt.nodes if n.bl_idname == "ShaderNodeOutputMaterial" and n.is_active_output)
            nt.links.new(orig, out.inputs["Surface"])
        nt.nodes.remove(node)
    return round(time.time() - t, 1)

def pack_textures(prefix, tex_dir):
    """writes <prefix>_BaseColor.png, <prefix>_ORM.png, <prefix>_Normal.png and builds material M_<prefix minus T_>"""
    I = bpy.data.images
    def px(im):
        a = np.empty(im.size[0]*im.size[1]*4, dtype=np.float32); im.pixels.foreach_get(a); return a.reshape(-1, 4)
    res = I["%s_raw_COLOR" % prefix].size[0]
    def save_as(src_name, dst_name, cs):
        src = I[src_name]
        dst = I.get(dst_name) or I.new(dst_name, res, res, alpha=False)
        dst.colorspace_settings.name = cs
        dst.pixels.foreach_set(px(src).ravel())
        dst.filepath_raw = os.path.join(tex_dir, dst_name + ".png"); dst.file_format = "PNG"; dst.save(); dst.filepath = dst.filepath_raw
        return dst
    bc = save_as("%s_raw_COLOR" % prefix, prefix + "_BaseColor", "sRGB")
    nm = save_as("%s_raw_NORMAL" % prefix, prefix + "_Normal", "Non-Color")
    ao = px(I["%s_raw_AO" % prefix]); r = px(I["%s_raw_ROUGH" % prefix]); m = px(I["%s_raw_METAL" % prefix])
    orm = np.stack([ao[:,0], r[:,0], m[:,0], np.ones(len(ao), np.float32)], 1); del ao, r, m
    oi = I.get(prefix + "_ORM") or I.new(prefix + "_ORM", res, res, alpha=False); oi.colorspace_settings.name = "Non-Color"
    oi.pixels.foreach_set(orm.ravel()); del orm
    oi.filepath_raw = os.path.join(tex_dir, prefix + "_ORM.png"); oi.file_format = "PNG"; oi.save(); oi.filepath = oi.filepath_raw
    mname = "M_" + prefix[2:]
    mat = bpy.data.materials.get(mname) or bpy.data.materials.new(mname)
    mat.use_nodes = True; nt = mat.node_tree; N = nt.nodes; L = nt.links; N.clear()
    out = N.new("ShaderNodeOutputMaterial"); b = N.new("ShaderNodeBsdfPrincipled")
    t1 = N.new("ShaderNodeTexImage"); t1.image = bc; t2 = N.new("ShaderNodeTexImage"); t2.image = oi; t3 = N.new("ShaderNodeTexImage"); t3.image = nm
    sep = N.new("ShaderNodeSeparateColor"); nmap = N.new("ShaderNodeNormalMap")
    L.new(t1.outputs["Color"], b.inputs["Base Color"]); L.new(t2.outputs["Color"], sep.inputs["Color"])
    L.new(sep.outputs["Green"], b.inputs["Roughness"]); L.new(sep.outputs["Blue"], b.inputs["Metallic"])
    L.new(t3.outputs["Color"], nmap.inputs["Color"]); L.new(nmap.outputs["Normal"], b.inputs["Normal"]); L.new(b.outputs["BSDF"], out.inputs["Surface"])
    for mm in bpy.data.materials: mm.use_fake_user = True
    return mat

# ---------------- export ----------------
def export_gun(gun, part_bone, bones, mag_parts, statics, folder, material, tex_prefix, mag_obj=None):
    O = bpy.data.objects; vl = bpy.context.view_layer
    EXP = os.path.join(folder, "export"); TEX = os.path.join(folder, "textures"); os.makedirs(EXP, exist_ok=True)
    E = coll("EXPORT_" + gun)
    vl.layer_collection.children[E.name].hide_viewport = False
    for ob in list(E.objects):
        d = ob.data; bpy.data.objects.remove(ob, do_unlink=True)
        if d is not None and d.users == 0:
            (bpy.data.meshes if isinstance(d, bpy.types.Mesh) else bpy.data.armatures).remove(d)
    arm_d = bpy.data.armatures.new("SK_%s_Skeleton" % gun); arm = O.new("root", arm_d); E.objects.link(arm)
    for o in vl.objects: o.select_set(False)
    arm.select_set(True); vl.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, head, parent, axis in bones:
        b = arm_d.edit_bones.new(name); b.head = Vector(head); b.tail = Vector(head) + Vector(axis).normalized()*0.01
        if parent: b.parent = arm_d.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    def tri_copy(src, matrix, name, mat=None):
        finish_needed = not any(m.type == "WEIGHTED_NORMAL" for m in src.modifiers)
        if finish_needed: finish(src)
        t = src.modifiers.new("_tri", "TRIANGULATE"); t.keep_custom_normals = True; t.min_vertices = 4
        dg = bpy.context.evaluated_depsgraph_get()
        me = bpy.data.meshes.new_from_object(src.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
        src.modifiers.remove(t); me.transform(matrix)
        if mat:
            me.materials.clear(); me.materials.append(mat)
            for p in me.polygons: p.material_index = 0
        ob = O.new(name, me); E.objects.link(ob); return ob
    copies = []
    for pn, bn in part_bone.items():
        cp = tri_copy(O[pn], O[pn].matrix_world, "part_"+pn, material)
        vg = cp.vertex_groups.new(name=bn); vg.add(list(range(len(cp.data.vertices))), 1.0, "REPLACE"); copies.append(cp)
    with bpy.context.temp_override(active_object=copies[0], selected_editable_objects=copies, selected_objects=copies):
        bpy.ops.object.join()
    sk = copies[0]; sk.name = "SK_"+gun; sk.data.name = "SK_"+gun; sk.parent = arm
    sk.modifiers.new("Armature", "ARMATURE").object = arm
    if mag_parts:
        inv = O[mag_obj].matrix_world.inverted()
        mp = [tri_copy(O[pn], inv @ O[pn].matrix_world, "sm_"+pn, material) for pn in mag_parts]
        with bpy.context.temp_override(active_object=mp[0], selected_editable_objects=mp, selected_objects=mp):
            bpy.ops.object.join()
        mp[0].name = "SM_%s_Mag" % gun; mp[0].data.name = mp[0].name
    for en, on in statics.items(): tri_copy(O[on], Matrix.Identity(4), en)
    def export(objs, path, types):
        for o in vl.objects: o.select_set(False)
        for o in objs: o.hide_set(False); o.select_set(True)
        vl.objects.active = objs[0]
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types=types, apply_scale_options="FBX_SCALE_ALL",
            axis_forward="X", axis_up="Z", add_leaf_bones=False, use_armature_deform_only=False, bake_anim=False,
            mesh_smooth_type="FACE", use_tspace=True, path_mode="STRIP", use_mesh_modifiers=True)
    export([arm, sk], os.path.join(EXP, "SK_%s.fbx" % gun), {"ARMATURE", "MESH"})
    names = (["SM_%s_Mag" % gun] if mag_parts else []) + list(statics)
    for nm in names: export([O[nm]], os.path.join(EXP, nm + ".fbx"), {"MESH"})
    for suf in ("BaseColor", "ORM", "Normal"):
        f = "%s_%s.png" % (tex_prefix, suf); shutil.copy2(os.path.join(TEX, f), os.path.join(EXP, f))
    for o in E.objects: o.hide_render = True
    vl.layer_collection.children[E.name].hide_viewport = True
    return {"tris": len(sk.data.polygons), "files": sorted(os.listdir(EXP))}


# ---------------- lofting ----------------
def loft(name, rings, c, material=None, cap=True):
    """rings: list of equal-length lists of (x,y,z) mm points; builds a closed tube surface, caps the ends"""
    bm = bmesh.new(); R = [[bm.verts.new((x*MM, y*MM, z*MM)) for x, y, z in r] for r in rings]; n = len(R[0])
    for k in range(len(R)-1):
        for i in range(n):
            j = (i+1) % n; bm.faces.new((R[k][i], R[k][j], R[k+1][j], R[k+1][i]))
    if cap: bm.faces.new(R[0]); bm.faces.new(R[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, c, material)

def superq(a, zb, zt, n_top=2.5, n_bot=2.5, N=40, cy=0.0, sy_front=None):
    """(y,z) section: half-width a, bottom zb, top zt, superellipse exponents per half"""
    cz = (zt+zb)/2; b = (zt-zb)/2; pts = []
    for i in range(N):
        t = 2*math.pi*i/N; ct, st = math.cos(t), math.sin(t)
        n = n_top if st >= 0 else n_bot
        y = a*math.copysign(abs(ct)**(2/n), ct); z = cz + b*math.copysign(abs(st)**(2/n), st)
        pts.append((cy+y, z))
    return pts

def lerp_tab(tab, x):
    """piecewise-linear lookup in [(x, value), ...] sorted by x"""
    if x <= tab[0][0]: return tab[0][1]
    for (x0, v0), (x1, v1) in zip(tab, tab[1:]):
        if x <= x1: t = (x-x0)/(x1-x0); return v0+(v1-v0)*t
    return tab[-1][1]

def xloft(name, xs, sec_fn, c, material=None):
    """loft along X: sec_fn(x) -> [(y,z)]"""
    return loft(name, [[(x, y, z) for y, z in sec_fn(x)] for x in xs], c, material)

def torus_x(name, R, r, c, material=None, seg=24, rseg=10):
    prof = [(r*math.cos(2*math.pi*k/rseg), R + r*math.sin(2*math.pi*k/rseg)) for k in range(rseg)]
    bm = bmesh.new(); rings = []
    for a, rr in prof:
        rings.append([bm.verts.new((a*MM, rr*math.cos(2*math.pi*i/seg)*MM, rr*math.sin(2*math.pi*i/seg)*MM)) for i in range(seg)])
    for k in range(rseg):
        for i in range(seg):
            j = (i+1) % seg; k2 = (k+1) % rseg
            bm.faces.new((rings[k][i], rings[k][j], rings[k2][j], rings[k2][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, c, material)

def move(ob, x, y, z):
    ob.data.transform(Matrix.Translation(Vector((x, y, z))*MM)); return ob

def overlaps(names, ignore=()):
    """pairs of objects whose evaluated surfaces intersect (BVH), for spotting parts poking through each other"""
    from mathutils.bvhtree import BVHTree
    dg = bpy.context.evaluated_depsgraph_get(); T = {}
    for n in names:
        o = bpy.data.objects[n]; oe = o.evaluated_get(dg); me = oe.to_mesh()
        T[n] = BVHTree.FromPolygons([o.matrix_world @ v.co for v in me.vertices], [tuple(p.vertices) for p in me.polygons])
        oe.to_mesh_clear()
    out = []
    for i, a in enumerate(names):
        for b in names[i+1:]:
            if (a, b) in ignore or (b, a) in ignore: continue
            k = len(T[a].overlap(T[b]))
            if k: out.append((a, b, k))
    return out


def triangulate(obs):
    """triangulate the mesh data itself (beauty) so the baked normal map matches the exported triangles exactly"""
    for o in obs:
        bm = bmesh.new(); bm.from_mesh(o.data)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3], quad_method="BEAUTY", ngon_method="BEAUTY")
        bm.to_mesh(o.data); bm.free(); o.data.update()

def bake_queue(prefix, names, tex_dir, logkey, blend_save=True, res=2048):
    """runs the full bake (ROUGH, METAL, COLOR, AO flat; NORMAL smooth; pack) on bpy.app.timers so MCP calls don't time out.
    progress goes to bpy.app.driver_namespace[logkey]; ends with 'done'."""
    log = bpy.app.driver_namespace.setdefault(logkey, []); log.clear()
    jobs = [("ROUGH", 16), ("METAL", 8), ("COLOR", 24), ("AO", 16), ("SMOOTH", 0), ("NORMAL", 8), ("PACK", 0)]
    obs = lambda: [bpy.data.objects[n] for n in names]
    triangulate(obs()); set_flat(obs(), True)
    def step():
        if not jobs: return None
        chan, smp = jobs.pop(0)
        try:
            if chan == "SMOOTH": set_flat(obs(), False); log.append("smooth")
            elif chan == "PACK":
                pack_textures(prefix, tex_dir); log.append("packed")
                if blend_save: bpy.ops.wm.save_mainfile()
                log.append("done")
            else: log.append("%s %s" % (chan, bake_channel(prefix, obs(), tex_dir, chan, smp, res)))
        except Exception as e:
            log.append("ERROR %s: %r" % (chan, e)); return None
        return 0.5 if jobs else None
    bpy.app.timers.register(step, first_interval=0.5)

bpy.app.driver_namespace["hb"] = {k: v for k, v in globals().items() if not k.startswith("__")}
