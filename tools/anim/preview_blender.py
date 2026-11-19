"""Plays a gun animation timeline on the exported gun in Blender and renders check frames.

Use inside Blender with the gun's .blend open (nothing is saved; reopen the file afterwards to undo the posing):
    exec(open(r"D:\\Claude area\\Holdover\\repo\\tools\\anim\\preview_blender.py").read())
    preview("Pistol_M9", "RackSlide", times=None, out_dir=r"D:\\Claude area\\Holdover\\art\\anim_checks")
times=None renders 9 frames spread over the action (always including holdAt and key events) and also writes
a 3x3 contact sheet. Format: docs/animation/gun_anim_format.md.

What it shows: the gun bones moving, the whole gun offset, both hands as simple palm + finger plates
(right red, left blue, thumb side marked), seen from the eye with a 90 degree field of view.
"""
import bpy, json, math, os
from mathutils import Matrix, Vector, Quaternion

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) if "__file__" in globals() else r"D:\Claude area\Holdover\repo"
MM = 0.001

# where the gun sits in front of the eye at its hold (camera space, mm, X forward, Y left, Z up): pistols in a
# compressed ready position, long guns shouldered with the sights near the eye line
HOLD_ORIGIN = {"Pistol_M9": [430, -40, -150], "Pistol_1911": [430, -40, -150],
               "Rifle_AK": [430, -60, -150], "Rifle_FAL": [440, -60, -160], "Carbine_AR": [420, -60, -140]}
BODY_TO_EYE = Vector((80, 0, 1650))    # eye above the pawn root, mm
START_STATES = ["SlideHome", "BoltHome", "HammerCocked", "SafetyOff", "MagIn"]
WRIST_BACK, WRIST_DOWN = 45.0, 12.0

# ---------------------------------------------------------------- key evaluation
def ease(name, u):
    if name == "linear": return u
    if name == "step": return 0.0
    return u * u * (3 - 2 * u)

def quat(v):
    return Quaternion(Vector(v["axis"]).normalized(), math.radians(v["deg"]))

def eval_keys(keys, t, kind, start):
    """kind 'loc' -> Vector (mm), 'rot' -> Quaternion; start = value used for a null first key"""
    def val(k):
        if k["v"] is None: return start
        return Vector(k["v"]) if kind == "loc" else quat(k["v"])
    if t <= keys[0]["t"]: return val(keys[0])
    for a, b in zip(keys, keys[1:]):
        if a["t"] <= t <= b["t"]:
            u = 0.0 if b["t"] == a["t"] else ease(a.get("ease", "smooth"), (t - a["t"]) / (b["t"] - a["t"]))
            va, vb = val(a), val(b)
            return va.lerp(vb, u) if kind == "loc" else va.slerp(vb, u)
    return val(keys[-1])

def weight_at(keys, pose, t, default):
    ks = [k for k in keys if k["pose"] == pose]
    if not ks: return default
    if t <= ks[0]["t"]: return ks[0]["weight"]
    for a, b in zip(ks, ks[1:]):
        if a["t"] <= t <= b["t"]:
            u = 0.0 if b["t"] == a["t"] else ease(a.get("ease", "smooth"), (t - a["t"]) / (b["t"] - a["t"]))
            return a["weight"] + (b["weight"] - a["weight"]) * u
    return ks[-1]["weight"]

def hand_matrix(pos, fdir, pnorm):
    x = Vector(fdir).normalized(); z = Vector(pnorm); z = (z - x * z.dot(x)).normalized(); y = z.cross(x)
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = Vector(pos)
    return m

# ---------------------------------------------------------------- scene objects
def find_armature(data):
    names = set()
    for st in data["states"].values(): names |= set(st)
    best = None
    for ob in bpy.data.objects:
        if ob.type == "ARMATURE" and names <= set(b.name for b in ob.data.bones):
            if best is None or data["gun"] in ob.name: best = ob
    if best is None: raise RuntimeError("no armature with the bones " + ", ".join(sorted(names)))
    return best

def color_mat(name, rgba):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = rgba
    return m

def make_hand(name, rgba, right):
    """palm plate 85x80x22 mm with four finger plates and a thumb, in hand space (X fingers, Z out of the palm)"""
    import bmesh
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    def box(cx, cy, cz, sx, sy, sz):
        r = bmesh.ops.create_cube(bm, size=1.0)
        for v in r["verts"]:
            v.co = Vector((v.co.x * sx + cx, v.co.y * sy + cy, v.co.z * sz + cz)) * MM
    box(0, 0, -11, 85, 80, 22)                                   # palm
    for i, y in enumerate((-30, -10, 10, 30)):                    # fingers
        box(42 + 40, y, -9, 80 - abs(i - 1.5) * 8, 17, 16)
    side = -1 if right else 1                                     # thumb on the hand's -Y side (right) / +Y (left)
    box(10, side * 55, -14, 55, 20, 18)
    bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(color_mat(name + "_mat", rgba)); ob.color = rgba
    return ob

VIEWS = {   # check cameras, camera-space mm relative to the gun's hold origin: (camera position, look-at point)
    "eye": None,
    "right": ((-80, -520, 160), (90, 0, -40)),
    "left": ((-80, 520, 160), (90, 0, -40)),
    "top": ((60, -60, 600), (80, 0, -40)),
}

def aim_camera(cam, gun, view):
    if VIEWS.get(view) is None:
        cam.location = (0, 0, 0); cam.rotation_euler = (math.radians(90), 0, math.radians(-90)); cam.data.angle = math.radians(90)
        return
    o = Vector(HOLD_ORIGIN[gun]); pos, look = VIEWS[view]
    cam.location = (o + Vector(pos)) * MM
    d = (o + Vector(look)) * MM - cam.location
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    cam.data.angle = math.radians(55)

def setup_view(arm):
    sc = bpy.context.scene
    for ob in sc.objects:
        ob.hide_render = True
    kids = [o for o in sc.objects if o.parent == arm or any(m.type == "ARMATURE" and m.object == arm for m in getattr(o, "modifiers", []))]
    for o in kids + [arm]:
        o.hide_render = False; o.hide_viewport = False; o.hide_set(False)
        for c in o.users_collection:
            c.hide_render = False
    for n in ("AnimCheckCam", "AnimCheckHandR", "AnimCheckHandL"):
        if n in bpy.data.objects: bpy.data.objects.remove(bpy.data.objects[n])
    cam_data = bpy.data.cameras.new("AnimCheckCam"); cam_data.lens_unit = "FOV"; cam_data.sensor_fit = "HORIZONTAL"
    cam_data.angle = math.radians(90); cam_data.clip_start = 0.01
    cam = bpy.data.objects.new("AnimCheckCam", cam_data); sc.collection.objects.link(cam)
    cam.rotation_euler = (math.radians(90), 0, math.radians(-90))   # look along +X, Z up
    cam.location = (0, 0, 0); sc.camera = cam
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"; sc.display.shading.color_type = "OBJECT"
    sc.display.shading.show_shadows = False
    sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = 960, 540, 100
    sc.render.film_transparent = False
    for o in kids: o.color = (0.35, 0.35, 0.38, 1)
    return cam

# ---------------------------------------------------------------- posing
def bone_deltas(data, act, arm, t, start):
    """gun-space delta matrix (mm) per bone at time t, own motion only"""
    out = {}
    tracks = act.get("bones", {})
    for b in arm.data.bones:
        piv = b.head_local / MM
        st = start.get(b.name, {})
        sl = Vector(st.get("loc", (0, 0, 0)))
        sr = quat(st["rot"]) if "rot" in st else Quaternion()
        tr = tracks.get(b.name, {})
        loc = eval_keys(tr["loc"], t, "loc", sl) if "loc" in tr else sl
        rot = eval_keys(tr["rot"], t, "rot", sr) if "rot" in tr else sr
        out[b.name] = Matrix.Translation(loc) @ Matrix.Translation(piv) @ rot.to_matrix().to_4x4() @ Matrix.Translation(-piv)
    return out

def start_pose(data):
    p = {}
    for s in START_STATES:
        for bone, v in data["states"].get(s, {}).items():
            p.setdefault(bone, {}).update(v)
    return p

def chain(arm, name, deltas):
    m = Matrix(); b = arm.data.bones[name]
    while b:
        m = deltas[b.name] @ m; b = b.parent
    return m

def gun_world(data, act, t):
    """gun space (mm) -> camera space (mm)"""
    hold = Matrix.Translation(Vector(HOLD_ORIGIN[data["gun"]]))
    g = act.get("gun")
    if not g: return hold
    piv_gun = None
    sock = g.get("pivot", "GripHand")
    piv_gun = SOCKETS_CACHE.get(sock, Vector())
    p = hold @ piv_gun
    loc = eval_keys(g["loc"], t, "loc", Vector()) if "loc" in g else Vector()
    rot = eval_keys(g["rot"], t, "rot", Quaternion()) if "rot" in g else Quaternion()
    off = Matrix.Translation(loc) @ Matrix.Translation(p) @ rot.to_matrix().to_4x4() @ Matrix.Translation(-p)
    return off @ hold

SOCKETS_CACHE = {}

def hand_target(k, data, side, G):
    if k.get("hold"):
        h = data["hold"]["hand" + side]
        return G @ hand_matrix(h["pos"], h["fingersDir"], h["palmNormal"])
    m = hand_matrix(k["pos"], k["fingersDir"], k["palmNormal"])
    if k["space"] == "gun": return G @ m
    if k["space"] == "body": return Matrix.Translation(-BODY_TO_EYE) @ m
    return m

def hand_at(data, act, side, t, G):
    keys = act.get("hand" + side)
    if not keys:
        return hand_target({"hold": True}, data, side, G)
    if t <= keys[0]["t"]: return hand_target(keys[0], data, side, G)
    for a, b in zip(keys, keys[1:]):
        if a["t"] <= t <= b["t"]:
            u = 0.0 if b["t"] == a["t"] else ease(a.get("ease", "smooth"), (t - a["t"]) / (b["t"] - a["t"]))
            ma, mb = hand_target(a, data, side, G), hand_target(b, data, side, G)
            loc = ma.translation.lerp(mb.translation, u)
            q = ma.to_quaternion().slerp(mb.to_quaternion(), u)
            m = q.to_matrix().to_4x4(); m.translation = loc
            return m
    return hand_target(keys[-1], data, side, G)

def pose_frame(data, act, arm, hands, t, start):
    G = gun_world(data, act, t)
    deltas = bone_deltas(data, act, arm, t, start)
    H = {s: hand_at(data, act, s, t, G) for s in ("L", "R")}
    # carried bones: follow the hand with the offset given by the attach entry's grip (hand pose with the part at rest)
    carried = {}
    for side in ("L", "R"):
        cur = None
        for a in act.get("attach", []):
            if a["hand"] == "hand" + side and a["t"] <= t: cur = a
        if cur and cur["bone"] and cur["bone"] in arm.data.bones:
            g = cur["grip"]
            carried[cur["bone"]] = G.inverted() @ H[side] @ hand_matrix(g["pos"], g["fingersDir"], g["palmNormal"]).inverted()
    arm.matrix_world = Matrix.Diagonal((MM, MM, MM, 1)) @ G @ Matrix.Diagonal((1 / MM, 1 / MM, 1 / MM, 1))
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix()
    bpy.context.view_layer.update()
    order = sorted(arm.pose.bones, key=lambda pb: len(pb.parent_recursive))
    for pb in order:
        d = carried.get(pb.name) or chain(arm, pb.name, deltas)
        mm = Matrix(d); mm.translation = d.translation * MM
        pb.matrix = mm @ pb.bone.matrix_local
        bpy.context.view_layer.update()
    for s in ("L", "R"):
        m = Matrix(H[s]); m.translation = m.translation * MM
        hands[s].matrix_world = m
        # fade the marker toward white as the grip opens (rough finger cue)
        w = 0.0
        fk = act.get("fingers" + s, [])
        for pose in ("Grip", "SupportWrap", "PalmMag", "PinchRack", "OverhandRack"):
            w = max(w, weight_at(fk, pose, t, 1.0 if pose in ("Grip", "SupportWrap") and not fk else 0.0))
        base = (0.85, 0.15, 0.1) if s == "R" else (0.1, 0.3, 0.9)
        hands[s].color = tuple(c * (0.5 + 0.5 * w) + (1 - 0.5 - 0.5 * w) * 0.9 for c in base) + (1,)

def pick_times(act, n=9):
    """evenly spread, with the holdAt moment swapped in for the nearest frame"""
    d = act["duration"]
    ts = [round(d * i / (n - 1), 3) for i in range(n)]
    if "holdAt" in act:
        i = min(range(n), key=lambda i: abs(ts[i] - act["holdAt"])); ts[i] = act["holdAt"]
    return sorted(ts)

def contact_sheet(paths, out_path, cols=3):
    import numpy as np
    imgs = [bpy.data.images.load(p) for p in paths]
    w, h = imgs[0].size
    rows = (len(imgs) + cols - 1) // cols
    sheet = np.zeros((rows * h, cols * w, 4), dtype=np.float32); sheet[..., 3] = 1
    for i, im in enumerate(imgs):
        px = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)
        r, c = rows - 1 - i // cols, i % cols
        sheet[r * h:(r + 1) * h, c * w:(c + 1) * w] = px
    out = bpy.data.images.new("contact", cols * w, rows * h, alpha=True)
    out.pixels = sheet.ravel().tolist(); out.filepath_raw = out_path; out.file_format = "PNG"; out.save()
    for im in imgs: bpy.data.images.remove(im)
    bpy.data.images.remove(out)

def preview(gun, action, times=None, out_dir=None, repo=None, view="right"):
    repo = repo or REPO
    data = json.load(open(os.path.join(repo, "Holdover", "Data", "GunAnims", gun + ".json")))
    act = data["actions"][action]
    arm = find_armature(data)
    SOCKETS_CACHE.clear()
    for b in arm.data.bones:
        SOCKETS_CACHE[b.name] = b.head_local / MM
    cam = setup_view(arm)
    aim_camera(cam, gun, view)
    hands = {"R": make_hand("AnimCheckHandR", (0.85, 0.15, 0.1, 1), True), "L": make_hand("AnimCheckHandL", (0.1, 0.3, 0.9, 1), False)}
    start = start_pose(data)
    times = times or pick_times(act)
    out_dir = out_dir or os.path.join(os.path.dirname(bpy.data.filepath), "anim_checks")
    os.makedirs(out_dir, exist_ok=True)
    paths = []
    for t in times:
        pose_frame(data, act, arm, hands, t, start)
        p = os.path.join(out_dir, "%s_%s_%s_%05.3f.png" % (gun, action, view, t))
        bpy.context.scene.render.filepath = p
        bpy.ops.render.render(write_still=True)
        paths.append(p)
    sheet = os.path.join(out_dir, "%s_%s_%s_sheet.png" % (gun, action, view))
    contact_sheet(paths, sheet)
    return {"sheet": sheet, "frames": paths, "times": times}
