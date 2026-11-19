"""Generates the gun animation timelines in Holdover/Data/GunAnims/<GunId>.json.

Format: docs/animation/gun_anim_format.md. Run from the repo root:  python tools/anim/gen_gun_anims.py
Everything is in gun space (mm, degrees, seconds; X forward, Y left, Z up), using the bone pivots and sockets from
docs/weapons/<gun>.md. Edit this file, not the JSON.
"""
import json, math, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "Holdover", "Data", "GunAnims")

# ---------------------------------------------------------------- small vector / rigid-transform helpers
def add(a, b): return [a[i] + b[i] for i in range(3)]
def sub(a, b): return [a[i] - b[i] for i in range(3)]
def mul(a, s): return [x * s for x in a]
def dot(a, b): return sum(a[i] * b[i] for i in range(3))
def cross(a, b): return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
def norm(a):
    l = math.sqrt(dot(a, a)); return [x / l for x in a]
def mat_vec(m, v): return [dot(m[i], v) for i in range(3)]
def mat_mul(a, b): return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def transpose(m): return [[m[j][i] for j in range(3)] for i in range(3)]
def rot_y(deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg)); return [[c, 0, s], [0, 1, 0], [-s, 0, c]]
def rot_x(deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg)); return [[1, 0, 0], [0, c, -s], [0, s, c]]
def frame(fwd, palm):
    """hand rotation matrix (columns = hand X fwd, Y side, Z palm) from fwd and palm directions"""
    x = norm(fwd); z = norm(sub(palm, mul(x, dot(palm, x)))); y = cross(z, x)
    return [[x[i], y[i], z[i]] for i in range(3)]
def r3(v): return [round(c, 2) + 0.0 for c in v]

# ---------------------------------------------------------------- timeline helpers
CUR = None   # key value null = "whatever the channel was when the action started"

def K(t, v, interp=None):
    k = [round(t, 4), (None if v is None else round(v, 3))]
    if interp: k.append(interp)
    return k

def hand(t, space, pos, fwd, palm, socket=None, bone=None, interp=None):
    f = norm(fwd); p = norm(sub(palm, mul(f, dot(palm, f))))
    k = {"t": round(t, 4), "space": space, "pos": r3(pos), "fwd": r3(f), "palm": r3(p)}
    if socket: k["socket"] = socket
    if bone: k["bone"] = bone
    if interp: k["interp"] = interp
    return k

def hold(t, interp=None):
    k = {"t": round(t, 4), "hold": True}
    if interp: k["interp"] = interp
    return k

def hand_for_part(part_pos, part_rot, grip):
    """hand target that holds a part (e.g. a magazine) whose pivot sits at part_pos with rotation part_rot (gun space),
    given grip = (pos, fwd, palm) of the hand in the part's own frame (part at rest)"""
    gpos, gfwd, gpalm = grip
    return add(part_pos, mat_vec(part_rot, gpos)), mat_vec(part_rot, gfwd), mat_vec(part_rot, gpalm)

class Action:
    def __init__(self, duration):
        self.d = {"duration": round(duration, 4)}
    def bone(self, name, chan, keys):
        self.d.setdefault("bones", {}).setdefault(name, {})[chan] = keys; return self
    def gun(self, chan, keys, pivot=None):
        g = self.d.setdefault("gun", {})
        if pivot: g["pivot"] = pivot
        g[chan] = keys; return self
    def hand(self, side, keys):
        self.d["hand" + side] = keys; return self
    def fingers(self, side, pose, keys):
        self.d.setdefault("fingers" + side, {})[pose] = keys; return self
    def attach(self, t, side, what):
        self.d.setdefault("attach", []).append([round(t, 4), "hand" + side, what]); return self
    def ev(self, t, name):
        self.d.setdefault("events", []).append([round(t, 4), name]); return self
    def out(self):
        if "events" in self.d: self.d["events"].sort(key=lambda e: e[0])
        if "attach" in self.d: self.d["attach"].sort(key=lambda e: e[0])
        return self.d

# ---------------------------------------------------------------- shared finger poses (degrees per joint, see spec)
FINGER_POSES = {
    #              thumb             index            middle           ring             pinky
    "Grip":         {"thumb": [20, 25, 20], "index": [10, 5, 5],   "middle": [70, 80, 45], "ring": [75, 85, 45], "pinky": [80, 85, 40]},
    "IndexTrigger": {"index": [45, 55, 25]},
    "SupportWrap":  {"thumb": [10, 10, 5],  "index": [55, 60, 35], "middle": [65, 70, 40], "ring": [70, 75, 40], "pinky": [75, 75, 35]},
    "PinchRack":    {"thumb": [35, 20, 15], "index": [60, 80, 40], "middle": [75, 90, 50], "ring": [80, 90, 50], "pinky": [80, 90, 45]},
    "OverhandRack": {"thumb": [5, 10, 10],  "index": [45, 50, 25], "middle": [50, 55, 30], "ring": [50, 55, 30], "pinky": [50, 55, 25]},
    "PalmMag":      {"thumb": [30, 20, 10], "index": [15, 10, 5],  "middle": [60, 70, 35], "ring": [65, 75, 35], "pinky": [70, 75, 30]},
    "ThumbPress":   {"thumb": [-10, 0, 0]},
    "Relaxed":      {"thumb": [5, 5, 5],    "index": [10, 15, 10], "middle": [15, 20, 10], "ring": [20, 20, 10], "pinky": [20, 25, 10]},
}

# ---------------------------------------------------------------- gun data (from docs/weapons/*.md)
GUNS = {
    "Pistol_M9": dict(
        kind="pistol", da=True, decocker=True, grip_safety=False,
        slide="Slide", slide_travel=60.0, slide_lock=58.0,
        barrel=dict(follow=9.0, drop_tZ=-1.5, tilt_rotY=0.0),
        hammer="Hammer", hammer_cocked=-60.0,
        trigger="Trigger", trigger_chan="rotY", trigger_sa=5.0, trigger_da=15.0,
        safety="Safety", safety_on=-60.0, safety_off=0.0, safety_side="slide",
        slide_stop="SlideStop", slide_stop_up=8.0, slide_stop_pos=[50, 13.5, -19],
        mag_release="MagRelease", mag_release_chan="tY", mag_release_push=-2.5, mag_release_pos=[54, 14, -49],
        mag_view_yaw=0.0,
        mag="Magazine", mag_pivot=[42, 0, -18], mag_rake=15.0, mag_clear=125.0, mag_len=130.0,
        grip_socket="GripHand", grip_rake=15.0,
        rear_serrations=[18, 0, 10], slide_top_z=15.0, slide_half_width=15.0,
        ejection_port=[75, -13, 5],
        recoil=dict(rotY=-7.0, tX=-14.0, rotZ=0.8, settle=0.24),
        mag_pouch=[80, 120, 40], holster=[10, -190, 40],
    ),
    "Pistol_1911": dict(
        kind="pistol", da=False, decocker=False, grip_safety=True,
        slide="Slide", slide_travel=62.0, slide_lock=60.0,
        barrel=dict(follow=3.0, drop_tZ=-2.0, tilt_rotY=-1.5),
        hammer="Hammer", hammer_cocked=-55.0,
        trigger="Trigger", trigger_chan="tX", trigger_sa=-3.0, trigger_da=-3.0,
        safety="ThumbSafety", safety_on=0.0, safety_off=20.0, safety_side="frame",
        slide_stop="SlideStop", slide_stop_up=6.0, slide_stop_pos=[92, 12, -17],
        mag_release="MagRelease", mag_release_chan="tY", mag_release_push=-2.5, mag_release_pos=[66, 12, -31],
        mag_view_yaw=60.0,
        mag="Magazine", mag_pivot=[59, 0, -18], mag_rake=18.0, mag_clear=120.0, mag_len=118.0,
        grip_socket="GripHand", grip_rake=18.0,
        rear_serrations=[22, 0, 10], slide_top_z=11.0, slide_half_width=11.6,
        ejection_port=[98, -12, 5],
        recoil=dict(rotY=-9.0, tX=-16.0, rotZ=0.9, settle=0.26),
        mag_pouch=[80, 120, 40], holster=[10, -190, 40],
    ),
    "Rifle_AK": dict(
        kind="rifle", style="ak", hold_open=False,
        carrier="BoltCarrier", carrier_travel=110.0, bolt="Bolt", bolt_chan="rotX", bolt_unlock=35.0, unlock_dist=8.0,
        charging_handle=None, handle_pos=[12, -30, 5], handle_moves_with_carrier=True,
        trigger="Trigger", trigger_chan="rotY", trigger_pull=12.0,
        selector="Selector", sel_safe=0.0, sel_fire=24.0, sel_auto=12.0, selector_pos=[-178, -16.6, -4],
        bolt_catch=None,
        mag_release="MagRelease", mag_release_chan="rotY", mag_release_push=-12.0, mag_release_pos=[-84, 0, -40],
        mag="Magazine", mag_style="rock", mag_pivot=[-12, 0, -20], mag_rock=-20.0, mag_clear=60.0, mag_len=200.0,
        mag_grip_center=[-45, 0, -110],
        dust_cover=None,
        grip_socket="GripHand", grip_rake=20.0, support_socket="SupportHand", support_radius=24.0,
        recoil=dict(rotY=-4.0, tX=-18.0, rotZ=0.6, settle=0.22),
        mag_pouch=[120, 100, 90],
    ),
    "Rifle_FAL": dict(
        kind="rifle", style="fal", hold_open=True,
        carrier="BoltCarrier", carrier_travel=125.0, bolt="Bolt", bolt_chan="rotY", bolt_unlock=4.0, unlock_dist=10.0,
        charging_handle="ChargingHandle", handle_pos=[14, 30, 2], handle_moves_with_carrier=False,
        trigger="Trigger", trigger_chan="rotY", trigger_pull=10.0,
        selector="Selector", sel_safe=0.0, sel_fire=-90.0, sel_auto=None, selector_pos=[-170, 16.5, -38],
        bolt_catch="BoltCatch", bolt_catch_up=6.0, bolt_catch_pos=[-35, 16, -32],
        mag_release="MagRelease", mag_release_chan="rotY", mag_release_push=-12.0, mag_release_pos=[-86, 0, -48],
        mag="Magazine", mag_style="rock", mag_pivot=[-2, 0, -20], mag_rock=-15.0, mag_clear=50.0, mag_len=190.0,
        mag_grip_center=[-40, 0, -110],
        dust_cover=None,
        grip_socket="GripHand", grip_rake=20.0, support_socket="SupportHand", support_radius=26.0,
        recoil=dict(rotY=-5.5, tX=-24.0, rotZ=0.7, settle=0.26),
        mag_pouch=[120, 100, 90],
    ),
    "Carbine_AR": dict(
        kind="rifle", style="ar", hold_open=True,
        carrier="BoltCarrier", carrier_travel=95.0, bolt="Bolt", bolt_chan="rotX", bolt_unlock=22.0, unlock_dist=8.0,
        charging_handle="ChargingHandle", handle_pos=[-168, 20, 13], handle_moves_with_carrier=False,
        trigger="Trigger", trigger_chan="rotY", trigger_pull=8.0,
        selector="Selector", sel_safe=0.0, sel_fire=-90.0, sel_auto=None, selector_pos=[-128, 16, -26],
        bolt_catch="BoltCatch", bolt_catch_up=-8.0, bolt_catch_pos=[-68, 16, -22],
        mag_release="MagRelease", mag_release_chan="tY", mag_release_push=3.0, mag_release_pos=[-73, -16, -36],
        mag="Magazine", mag_style="straight", mag_pivot=[-29, 0, -11], mag_axis=[0, 0, -1], mag_clear=170.0, mag_len=190.0,
        mag_grip_center=[-29, 0, -100],
        dust_cover="DustCover", dust_open=100.0,
        grip_socket="GripHand", grip_rake=25.0, support_socket="SupportHand", support_radius=22.0,
        recoil=dict(rotY=-3.0, tX=-12.0, rotZ=0.4, settle=0.18),
        mag_pouch=[120, 100, 90],
    ),
}

# ---------------------------------------------------------------- hand holds
def grip_hold(g):
    """shooting (right) hand on the pistol grip: palm on the right panel, fingers around the front strap"""
    r = math.radians(g["grip_rake"])
    axis = [-math.sin(r), 0, -math.cos(r)]          # down the grip
    perp = [math.cos(r), 0, -math.sin(r)]           # forward, square to the grip
    fwd = norm(add(mul(perp, 0.87), mul(axis, 0.5)))
    return {"socket": g["grip_socket"], "pos": [0, -17, 0], "fwd": r3(fwd), "palm": [0, 1, 0]}

def support_hold(g):
    if g["kind"] == "pistol":   # thumbs-forward grip: left palm on the left panel, fingers over the shooting hand's fingers
        return {"socket": g["grip_socket"], "pos": [8, 24, -12], "fwd": r3(norm([0.55, -0.3, -0.78])), "palm": [0, -1, 0]}
    # rifle: left hand under the handguard, palm up, fingers curling round the right side
    return {"socket": g["support_socket"], "pos": [0, 0, -g["support_radius"]], "fwd": r3(norm([0.35, -0.85, 0.2])), "palm": [0, 0, 1]}

def H(t, h, interp=None):
    return hand(t, "gun", h["pos"], h["fwd"], h["palm"], socket=h["socket"], interp=interp)

# ---------------------------------------------------------------- mag geometry
def mag_axis(g):
    if g.get("mag_axis"): return norm(g["mag_axis"])
    r = math.radians(g.get("mag_rake", 0.0)); return [-math.sin(r), 0, -math.cos(r)]

def mag_grip(g):
    """left hand on the magazine, in the mag's own (rest) frame relative to its pivot"""
    if g["kind"] == "pistol":   # palm under the base plate, index finger along the front
        ax = mag_axis(g); fwdv = [math.cos(math.radians(g["mag_rake"])), 0, -math.sin(math.radians(g["mag_rake"]))]
        return (mul(ax, g["mag_len"] + 4), fwdv, mul(ax, -1))
    c = sub(g["mag_grip_center"], g["mag_pivot"])          # rifle: hand wrapped round the mag body from the left
    return (add(c, [0, 30, 0]), norm([0.25, -0.35, -0.9]), [0, -1, 0])

def mag_pose_out(g, s):
    """(translation, rotation) of the Magazine bone when pulled s mm out (pistols/AR: along the axis)"""
    return mul(mag_axis(g), s), [[1, 0, 0], [0, 1, 0], [0, 0, 1]]

def mag_tracks_along(a, g, keys):
    """keys: [(t, s_mm, interp)] -> tX/tZ tracks of the Magazine bone along its axis"""
    ax = mag_axis(g)
    a.bone(g["mag"], "tX", [K(t, ax[0] * s, i) for t, s, i in keys])
    a.bone(g["mag"], "tZ", [K(t, ax[2] * s, i) for t, s, i in keys])

def hand_on_mag(t, g, s=0.0, rock=0.0, extra=None, tilt=0.0, interp=None):
    """left-hand target holding the mag at s mm out (along axis) or rocked `rock` degrees about its pivot,
    then rolled `tilt` degrees about the gun's X axis (to show the top round)"""
    trans = mul(mag_axis(g), s) if g.get("mag_style", "straight") != "rock" else (extra or [0, 0, 0])
    R = mat_mul(rot_x(tilt), rot_y(rock))
    gp, gf, gpl = mag_grip(g)
    pos = add(add(g["mag_pivot"], trans), mat_vec(R, gp))
    return hand(t, "gun", pos, mat_vec(R, gf), mat_vec(R, gpl), interp=interp)

# ---------------------------------------------------------------- recoil + fire cycle pieces
def recoil(a, g, t0):
    rc = g["recoil"]; s = rc["settle"]
    a.gun("rotY", [K(0, 0), K(t0, 0, "lin"), K(t0 + 0.028, rc["rotY"]), K(t0 + s, 0)], pivot=g["grip_socket"])
    a.gun("tX", [K(0, 0), K(t0, 0, "lin"), K(t0 + 0.022, rc["tX"]), K(t0 + s, 0)])
    a.gun("rotZ", [K(0, 0), K(t0, 0, "lin"), K(t0 + 0.035, rc["rotZ"]), K(t0 + s, 0)])

# ---------------------------------------------------------------- PISTOL actions
def pistol_actions(g):
    A = {}
    gh, sh = grip_hold(g), support_hold(g)
    trig, tch = g["trigger"], g["trigger_chan"]
    ax = mag_axis(g)

    def fire(da=False):
        # the game code cycles slide, barrel and hammer from recoil, so this only keys the trigger press
        # (and the M9's double-action hammer rise up to the break), the trigger finger and the whole-gun recoil
        brk = 0.30 if da else 0.06
        a = Action(brk + g["recoil"]["settle"] + 0.06)
        pull = g["trigger_da"] if da else g["trigger_sa"]
        if da:
            a.bone(trig, tch, [K(0, 0, "lin"), K(brk, pull, "easeOut"), K(brk + 0.14, 0)])
            a.bone(g["hammer"], "rotY", [K(0, 0, "lin"), K(brk - 0.004, g["hammer_cocked"], "lin"), K(brk, 0)])
        else:
            a.bone(trig, tch, [K(0, 0, "easeIn"), K(brk, pull, "easeOut"), K(brk + 0.10, 0)])
        if g["grip_safety"]:
            a.bone("GripSafety", "rotY", [K(0, -6)])
        recoil(a, g, brk)
        a.fingers("R", "IndexTrigger", [K(0, 0.7, "easeIn"), K(brk, 1), K(brk + 0.10, 0.7)])
        a.ev(brk - 0.002, "HammerDrop").ev(brk, "ShotBreak").ev(brk + 0.09, "TriggerReset")
        return a.out()

    A["Fire"] = fire()
    if g["da"]:
        A["FireDA"] = fire(da=True)

    # dry fire (hammer cocked, nothing in the chamber)
    a = Action(0.25)
    a.bone(trig, tch, [K(0, 0, "easeIn"), K(0.07, g["trigger_sa"], "easeOut"), K(0.20, 0)])
    a.bone(g["hammer"], "rotY", [K(0.068, g["hammer_cocked"], "lin"), K(0.072, 0)])
    a.fingers("R", "IndexTrigger", [K(0, 0.7), K(0.07, 1), K(0.2, 0.7)])
    a.ev(0.072, "HammerDrop").ev(0.072, "Sound:DryFire")
    A["DryFire"] = a.out()

    # rack the slide, overhand (whatever is in the chamber comes out, the next round goes in)
    def rack(lock_open=False, from_locked=False):
        a = Action(0.85 if lock_open else 0.78)
        tr, lk = g["slide_travel"], g["slide_lock"]
        top = [g["rear_serrations"][0], 0, g["slide_top_z"] + 14]          # palm centre over the rear of the slide
        a.gun("tX", [K(0, 0), K(0.16, -90), K(0.58, -90), K(0.75, 0)], pivot=g["grip_socket"])
        a.gun("tZ", [K(0, 0), K(0.16, -35), K(0.58, -35), K(0.75, 0)])
        a.gun("rotX", [K(0, 0), K(0.16, -18), K(0.58, -18), K(0.75, 0)])
        start = -lk if from_locked else 0
        grab, pulled = 0.22, 0.36
        a.bone(g["slide"], "tX", [K(grab, start, "easeIn"), K(pulled, -tr, "lin")] +
               ([K(0.40, -tr, "lin"), K(0.43, -lk)] if lock_open else [K(0.385, 0)]))
        # hand rides on the slide: same X offset as the slide while gripping
        fwd, palm = [0, -1, -0.15], [0, 0, -1]
        hk = [hold(0), hand(0.14, "gun", add(top, [0, 30, 25]), fwd, palm, interp="easeOut"),
              hand(grab, "gun", add(top, [start, 0, 0]), fwd, palm),
              hand(pulled, "gun", add(top, [-tr, 0, 0]), fwd, palm, interp="easeOut")]
        if lock_open:   # hold it back while the right thumb pushes the slide stop up, then ease it onto the stop
            hk += [hand(0.42, "gun", add(top, [-tr, 0, 0]), fwd, palm), hand(0.50, "gun", add(top, [-lk + 25, 40, 30]), fwd, palm)]
            a.bone(g["slide_stop"], "rotY", [K(0.36, 0), K(0.40, g["slide_stop_up"])])
            a.fingers("R", "ThumbPress", [K(0.30, 0), K(0.36, 1), K(0.44, 1), K(0.50, 0)])
            a.ev(0.43, "SlideLock").ev(0.43, "Sound:SlideLock")
        else:           # let go at the back: the slide flies forward on its own
            hk += [hand(0.44, "gun", add(top, [-tr - 30, 45, 40]), fwd, palm)]
        hk += [hold(0.72, "easeOut")]
        a.hand("L", hk)
        a.fingers("L", "SupportWrap", [K(0, 1), K(0.10, 0), K(0.62, 0), K(0.72, 1)])
        a.fingers("L", "OverhandRack", [K(0.10, 0), K(grab, 1), K(pulled + 0.02, 1), K(pulled + 0.08, 0)])
        b = g["barrel"]; f = grab + (pulled - grab) * b["follow"] / tr
        if not from_locked:
            a.bone("Barrel", "tX", [K(grab, 0, "easeIn"), K(f, -b["follow"])] + ([] if lock_open else [K(0.378, -b["follow"], "lin"), K(0.385, 0)]))
            a.bone("Barrel", "tZ", [K(f, 0, "lin"), K(f + 0.01, b["drop_tZ"])] + ([] if lock_open else [K(0.376, b["drop_tZ"], "lin"), K(0.381, 0)]))
            if b["tilt_rotY"]:
                a.bone("Barrel", "rotY", [K(f, 0, "lin"), K(f + 0.01, b["tilt_rotY"])] + ([] if lock_open else [K(0.376, b["tilt_rotY"], "lin"), K(0.381, 0)]))
        elif not lock_open:
            a.bone("Barrel", "tX", [K(0.36, -b["follow"], "lin"), K(0.385, 0)])
            a.bone("Barrel", "tZ", [K(0.374, b["drop_tZ"], "lin"), K(0.381, 0)])
            if b["tilt_rotY"]:
                a.bone("Barrel", "rotY", [K(0.374, b["tilt_rotY"], "lin"), K(0.381, 0)])
            a.bone(g["slide_stop"], "rotY", [K(0.30, g["slide_stop_up"]), K(0.32, 0)])
        a.bone(g["hammer"], "rotY", [K(grab, CUR, "lin"), K(grab + 0.05, g["hammer_cocked"])])
        a.ev(grab + 0.02, "Sound:SlideBack").ev(pulled - 0.03, "SlideToRear").ev(grab + 0.05, "HammerCock")
        if not lock_open:
            a.ev(0.38, "InBattery").ev(0.385, "Sound:SlideForward")
            if not from_locked:
                a.d["holdAt"] = pulled      # manual controls: slide stays at the rear while the rack key is held
        return a.out()

    A["RackSlide"] = rack()
    A["LockOpen"] = rack(lock_open=True)
    A["SlingshotRelease"] = rack(from_locked=True)

    # slide release with the support-hand thumb on the slide stop
    a = Action(0.45)
    stop = g["slide_stop_pos"]
    tp = hand(0.14, "gun", add(stop, [-28, 14, -26]), [0.75, -0.2, 0.62], [0, -1, 0.3], interp="easeOut")
    a.hand("L", [hold(0), tp, hand(0.20, "gun", add(stop, [-28, 14, -30]), [0.75, -0.2, 0.62], [0, -1, 0.3]), hold(0.40)])
    a.fingers("L", "ThumbPress", [K(0.06, 0), K(0.14, 1), K(0.24, 1), K(0.34, 0)])
    a.bone(g["slide_stop"], "rotY", [K(0.17, g["slide_stop_up"], "lin"), K(0.20, 0)])
    a.bone(g["slide"], "tX", [K(0.20, -g["slide_lock"], "lin"), K(0.225, 0)])
    b = g["barrel"]
    a.bone("Barrel", "tX", [K(0.21, -b["follow"], "lin"), K(0.225, 0)])
    a.bone("Barrel", "tZ", [K(0.216, b["drop_tZ"], "lin"), K(0.222, 0)])
    if b["tilt_rotY"]:
        a.bone("Barrel", "rotY", [K(0.216, b["tilt_rotY"], "lin"), K(0.222, 0)])
    a.ev(0.20, "SlideRelease").ev(0.22, "InBattery").ev(0.225, "Sound:SlideForward")
    A["SlideRelease"] = a.out()

    # magazine out: right thumb presses the release; drop (empty reload) or catch with the left hand (tactical)
    mr = g["mag_release_pos"]
    def mag_out(retain):
        a = Action(0.95 if retain else 0.42)
        a.fingers("R", "ThumbPress", [K(0, 0), K(0.10, 1), K(0.20, 1), K(0.28, 0)])
        a.bone(g["mag_release"], g["mag_release_chan"], [K(0.08, 0, "lin"), K(0.11, g["mag_release_push"]), K(0.20, g["mag_release_push"]), K(0.24, 0)])
        a.gun("rotX", [K(0, 0), K(0.10, -12), K(0.30, -8), K(0.42, 0)], pivot=g["grip_socket"])
        a.ev(0.11, "MagDetach").ev(0.11, "Sound:MagRelease")
        if retain:
            mag_tracks_along(a, g, [(0.11, 0, "easeOut"), (0.16, 18, None), (0.30, 18, "easeIn"), (0.45, g["mag_clear"] + 10, None)])
            a.hand("L", [hold(0), hand_on_mag(0.28, g, 18, interp="easeOut"), hand_on_mag(0.30, g, 18), hand_on_mag(0.45, g, g["mag_clear"] + 10),
                         hand(0.80, "body", g["mag_pouch"], [0, 0, -1], [-1, 0, 0], interp="easeIn")])
            a.fingers("L", "SupportWrap", [K(0, 1), K(0.12, 0)]).fingers("L", "PalmMag", [K(0.22, 0), K(0.30, 1)])
            a.attach(0.45, "L", g["mag"]).ev(0.80, "MagStow").ev(0.82, "Sound:MagPouch")
        else:
            mag_tracks_along(a, g, [(0.11, 0, "easeIn"), (0.20, g["mag_clear"], None)])
            a.ev(0.20, "MagDrop")
            a.hand("L", [hold(0), hand(0.12, "gun", add(sh["pos"], [10, 60, -40]), sh["fwd"], sh["palm"], socket=sh["socket"]), hold(0.40)])
            a.fingers("L", "SupportWrap", [K(0, 1), K(0.08, 0.3), K(0.34, 1)])
        return a.out()
    A["MagOut"] = mag_out(retain=True)
    A["MagDrop"] = mag_out(retain=False)

    # magazine in: from the pouch, line up with the well, push home, palm slap
    def mag_in(t0=0.0):
        a = Action(t0 + 0.95)
        a.attach(t0, "L", g["mag"]).ev(t0, "MagFromPouch").ev(t0, "Sound:MagPouch")
        a.gun("rotX", [K(t0, CUR), K(t0 + 0.25, -10), K(t0 + 0.75, -10), K(t0 + 0.9, 0)], pivot=g["grip_socket"])
        a.gun("rotY", [K(t0, CUR), K(t0 + 0.25, 8), K(t0 + 0.75, 8), K(t0 + 0.9, 0)])
        a.hand("L", [hand(t0, "body", g["mag_pouch"], [0, 0, -1], [-1, 0, 0]),
                     hand_on_mag(t0 + 0.42, g, 60, interp="easeOut"),
                     hand_on_mag(t0 + 0.48, g, 30, interp="lin"),
                     hand_on_mag(t0 + 0.56, g, 0),
                     hand_on_mag(t0 + 0.60, g, -2),
                     hold(t0 + 0.85)])
        mag_tracks_along(a, g, [(t0 + 0.42, 60, "easeOut"), (t0 + 0.48, 30, "lin"), (t0 + 0.56, 0, None)])
        a.attach(t0 + 0.56, "L", None)
        a.fingers("L", "PalmMag", [K(t0, 1), K(t0 + 0.56, 1), K(t0 + 0.66, 0)]).fingers("L", "SupportWrap", [K(t0 + 0.62, 0), K(t0 + 0.82, 1)])
        a.ev(t0 + 0.56, "MagAttach").ev(t0 + 0.57, "MagSeat").ev(t0 + 0.57, "Sound:MagIn")
        return a

    A["MagIn"] = mag_in().out()

    # tactical reload = mag out (retained) + mag in
    tac = Action(0.95 + 0.95)
    mo, mi = A["MagOut"], mag_in(0.85).out()
    merged = merge(mo, mi, 0.0)
    merged["duration"] = mi["duration"]
    A["ReloadTactical"] = merged

    # empty reload from slide lock = drop + mag in + slingshot
    mi2 = mag_in(0.30).out()
    sl = A["SlingshotRelease"]
    A["ReloadEmpty"] = merge(merge(A["MagDrop"], mi2, 0.0), sl, 1.10)

    # mag check: thumb the release, catch the mag, lift it into view to read the witness holes, put it back
    a = Action(1.6)
    a.gun("rotX", [K(0, 0), K(0.2, -15), K(1.3, -15), K(1.55, 0)], pivot=g["grip_socket"])
    a.gun("tX", [K(0, 0), K(0.2, -60), K(1.3, -60), K(1.55, 0)])
    a.fingers("R", "ThumbPress", [K(0.12, 0), K(0.2, 1), K(0.28, 1), K(0.34, 0)])
    a.bone(g["mag_release"], g["mag_release_chan"], [K(0.18, 0, "lin"), K(0.21, g["mag_release_push"]), K(0.28, g["mag_release_push"]), K(0.31, 0)])
    clear = g["mag_clear"] + 10
    # in view: mag upright in the palm, top tipped 20 degrees toward the eye, turned by mag_view_yaw to show the holes
    yaw = math.radians(g["mag_view_yaw"])
    vf = norm([math.cos(yaw) * 0.94, math.sin(yaw) * 0.94, 0.34]); vp = [-0.34 * math.cos(yaw), -0.34 * math.sin(yaw), 0.94]
    view = [330, 30, -230]
    a.hand("L", [hold(0), hand_on_mag(0.25, g, 0, interp="easeOut"), hand_on_mag(0.42, g, clear),
                 hand(0.70, "camera", view, vf, vp), hand(0.90, "camera", view, vf, vp),
                 hand_on_mag(1.12, g, clear, interp="lin"), hand_on_mag(1.24, g, 20, interp="lin"), hand_on_mag(1.30, g, 0),
                 hand_on_mag(1.33, g, -2), hold(1.55)])
    a.fingers("L", "SupportWrap", [K(0, 1), K(0.15, 0), K(1.36, 0), K(1.55, 1)]).fingers("L", "PalmMag", [K(0.15, 0), K(0.25, 1), K(1.30, 1), K(1.38, 0)])
    a.attach(0.25, "L", g["mag"]).attach(1.30, "L", None)
    a.ev(0.21, "Sound:MagRelease").ev(0.25, "MagDetach").ev(0.70, "MagCheckView").ev(1.30, "MagAttach").ev(1.31, "MagSeat").ev(1.31, "Sound:MagIn")
    a.d["holdAt"] = 0.75
    A["MagCheck"] = a.out()

    # press check: pinch the front of the slide, open 12 mm, tilt to see brass, close
    a = Action(1.1)
    open_mm = 12.0
    a.gun("rotX", [K(0, 0), K(0.2, -25), K(0.8, -25), K(1.0, 0)], pivot=g["grip_socket"])
    a.gun("tX", [K(0, 0), K(0.2, -70), K(0.8, -70), K(1.0, 0)])
    pinch = [g["rear_serrations"][0] + 10, 0, g["slide_top_z"] - 4]
    pf, pp = [0, -1, -0.4], [0, 0, -1]
    a.hand("L", [hold(0), hand(0.22, "gun", add(pinch, [0, 0, 18]), pf, pp, interp="easeOut"),
                 hand(0.36, "gun", add(pinch, [-open_mm, 0, 18]), pf, pp), hand(0.70, "gun", add(pinch, [-open_mm, 0, 18]), pf, pp),
                 hand(0.78, "gun", add(pinch, [0, 0, 18]), pf, pp), hold(1.0)])
    a.fingers("L", "SupportWrap", [K(0, 1), K(0.12, 0), K(0.85, 0), K(1.0, 1)]).fingers("L", "PinchRack", [K(0.12, 0), K(0.22, 1), K(0.78, 1), K(0.88, 0)])
    a.bone(g["slide"], "tX", [K(0.24, 0), K(0.36, -open_mm), K(0.70, -open_mm), K(0.78, 0)])
    b = g["barrel"]
    a.bone("Barrel", "tX", [K(0.24, 0), K(0.30, -min(open_mm, b["follow"])), K(0.72, -min(open_mm, b["follow"])), K(0.78, 0)])
    a.ev(0.45, "ChamberView").ev(0.78, "Sound:SlideSeat")
    a.d["holdAt"] = 0.45
    A["PressCheck"] = a.out()

    # safety on / off
    def safety(on):
        a = Action(0.45)
        val_from, val_to = (g["safety_off"], g["safety_on"]) if on else (g["safety_on"], g["safety_off"])
        a.bone(g["safety"], "rotY", [K(0.12, val_from, "easeIn"), K(0.20, val_to)])
        a.fingers("R", "ThumbPress", [K(0, 0), K(0.10, 1), K(0.24, 1), K(0.36, 0)])
        a.gun("rotX", [K(0, 0), K(0.10, 4), K(0.30, 4), K(0.45, 0)], pivot=g["grip_socket"])
        a.ev(0.20, "SafetyOn" if on else "SafetyOff").ev(0.20, "Sound:SafetyClick")
        if on and g["decocker"]:   # the M9 lever also drops the hammer safely
            a.bone(g["hammer"], "rotY", [K(0.19, CUR, "lin"), K(0.21, 0)]).ev(0.21, "HammerDrop").ev(0.21, "Sound:Decock")
        return a.out()
    A["SafetyOn"] = safety(True)
    A["SafetyOff"] = safety(False)
    if g["decocker"]:
        A["Decock"] = merge(A["SafetyOn"], safety(False), 0.40)

    # tap-rack: palm the base of the mag up, then rack
    a = Action(0.30)
    a.hand("L", [hold(0), hand_on_mag(0.10, g, 8, interp="easeOut"), hand_on_mag(0.14, g, -2), hold(0.28)])
    a.fingers("L", "SupportWrap", [K(0, 1), K(0.06, 0), K(0.2, 0), K(0.28, 1)]).fingers("L", "PalmMag", [K(0.06, 0), K(0.1, 1), K(0.16, 1), K(0.22, 0)])
    a.ev(0.14, "MagSeat").ev(0.14, "Sound:MagTap")
    A["TapRack"] = merge(a.out(), A["RackSlide"], 0.24)

    # stovepipe: sweep the slide with the support-hand edge (rack)
    A["ClearStovepipe"] = dict(A["RackSlide"], events=sorted(A["RackSlide"]["events"] + [[0.30, "ClearMalfunction"]], key=lambda e: e[0]))
    A["ClearStovepipe"].pop("holdAt", None)

    # double feed: lock open, strip the mag, rack x3, mag in, rack
    seq = A["LockOpen"]
    seq = merge(seq, A["MagDrop"], seq["duration"])
    t = seq["duration"]
    for i in range(3):
        seq = merge(seq, A["SlingshotRelease"] if i == 0 else A["RackSlide"], t); t = seq["duration"]
    seq = merge(seq, A["MagIn"], t); t = seq["duration"]
    seq = merge(seq, A["RackSlide"], t)
    seq["events"].append([round(A["LockOpen"]["duration"] + 1.0, 4), "ClearMalfunction"]); seq["events"].sort(key=lambda e: e[0])
    A["ClearDoubleFeed"] = seq

    # draw from the right hip holster / holster
    hip = g["holster"]
    def draw(reverse):
        a = Action(1.0)
        keys_gun = [K(0, 0), K(0.35, -1), K(1.0, 0)]
        a.gun("tZ", [K(0, -260), K(0.30, -200), K(0.55, -60), K(0.9, 0)] if not reverse else [K(0, 0), K(0.35, -60), K(0.7, -200), K(1.0, -260)], pivot=g["grip_socket"])
        a.gun("tY", [K(0, -150), K(0.55, -40), K(0.9, 0)] if not reverse else [K(0, 0), K(0.45, -40), K(1.0, -150)])
        a.gun("rotY", [K(0, 80), K(0.30, 70), K(0.55, 10), K(0.9, 0)] if not reverse else [K(0, 0), K(0.45, 10), K(0.7, 70), K(1.0, 80)])
        a.gun("tX", [K(0, -150), K(0.55, -120), K(0.9, 0)] if not reverse else [K(0, 0), K(0.45, -120), K(1.0, -150)])
        if not reverse:
            a.hand("L", [hand(0, "body", [60, 40, 120], [0.3, -0.2, 0.9], [0, -1, 0]), hold(0.75, "easeOut")])
            a.fingers("L", "Relaxed", [K(0, 1), K(0.55, 0)]).fingers("L", "SupportWrap", [K(0.55, 0), K(0.8, 1)])
            a.ev(0.0, "HolsterOut").ev(0.05, "Sound:Draw")
        else:
            a.hand("L", [hold(0), hand(0.40, "body", [60, 40, 120], [0.3, -0.2, 0.9], [0, -1, 0])])
            a.fingers("L", "SupportWrap", [K(0, 1), K(0.25, 0)]).fingers("L", "Relaxed", [K(0.2, 0), K(0.45, 1)])
            a.ev(0.95, "HolsterIn").ev(0.92, "Sound:Holster")
        a.fingers("R", "IndexTrigger", [K(0, 0)])
        return a.out()
    A["Draw"] = draw(False)
    A["Holster"] = draw(True)

    # look the gun over (idle flavour)
    a = Action(2.6)
    a.gun("tX", [K(0, 0), K(0.4, -140), K(2.2, -140), K(2.6, 0)], pivot=g["grip_socket"])
    a.gun("rotX", [K(0, 0), K(0.5, -55), K(1.2, -55), K(1.6, 40), K(2.1, 40), K(2.6, 0)])
    a.gun("rotZ", [K(0, 0), K(0.5, 20), K(1.2, 20), K(1.6, -15), K(2.1, -15), K(2.6, 0)])
    a.hand("L", [hold(0), hand(0.35, "camera", [380, 90, -150], [0.4, -0.3, 0.86], [0, -1, 0]), hold(2.4)])
    a.fingers("L", "SupportWrap", [K(0, 1), K(0.2, 0), K(2.3, 0), K(2.55, 1)]).fingers("L", "Relaxed", [K(0.15, 0), K(0.35, 1), K(2.3, 1), K(2.45, 0)])
    A["Inspect"] = a.out()

    states = {
        "SlideHome": {g["slide"]: {"tX": 0}, "Barrel": {"tX": 0, "tZ": 0}, g["slide_stop"]: {"rotY": 0}},
        "SlideLocked": {g["slide"]: {"tX": -g["slide_lock"]}, "Barrel": dict({"tX": -g["barrel"]["follow"], "tZ": g["barrel"]["drop_tZ"]}, **({"rotY": g["barrel"]["tilt_rotY"]} if g["barrel"]["tilt_rotY"] else {})), g["slide_stop"]: {"rotY": g["slide_stop_up"]}},
        "HammerDown": {g["hammer"]: {"rotY": 0}}, "HammerCocked": {g["hammer"]: {"rotY": g["hammer_cocked"]}},
        "SafetyOn": {g["safety"]: {"rotY": g["safety_on"]}}, "SafetyOff": {g["safety"]: {"rotY": g["safety_off"]}},
        "MagIn": {g["mag"]: {"tX": 0, "tZ": 0}}, "MagOut": {g["mag"]: {"tX": ax[0] * 400, "tZ": ax[2] * 400}},
    }
    if g["grip_safety"]:
        states["GripSafetyPressed"] = {"GripSafety": {"rotY": -6}}; states["GripSafetyReleased"] = {"GripSafety": {"rotY": 0}}
    return A, states, {"handR": grip_hold(g), "handL": support_hold(g), "fingersR": {"Grip": 1, "IndexTrigger": 0}, "fingersL": {"SupportWrap": 1}}

# ---------------------------------------------------------------- RIFLE actions
def rifle_actions(g):
    A = {}
    gh, sh = grip_hold(g), support_hold(g)
    trig = g["trigger"]; car = g["carrier"]; tr = g["carrier_travel"]

    def bolt_unlock(a, t0, t_unl, relock_t=None, relock_from=None):
        """bolt turns/tilts to unlock between t0 and t_unl, and back when the carrier returns home"""
        k = [K(t0, 0, "lin"), K(t_unl, g["bolt_unlock"])]
        if relock_t is not None:
            k += [K(relock_from, g["bolt_unlock"], "lin"), K(relock_t, 0)]
        a.bone(g["bolt"], g["bolt_chan"], k)

    def dust_cover(a, t):
        if g.get("dust_cover"):
            a.bone(g["dust_cover"], "rotX", [K(t, CUR, "lin"), K(t + 0.03, g["dust_open"])]).ev(t, "DustCoverOpen")

    def fire():
        # the game code cycles carrier, bolt and dust cover from recoil: only trigger, finger and whole-gun recoil here
        brk = 0.05
        a = Action(brk + g["recoil"]["settle"] + 0.08)
        a.bone(trig, "rotY", [K(0, 0, "easeIn"), K(brk, g["trigger_pull"], "easeOut"), K(brk + 0.10, 0)])
        recoil(a, g, brk)
        a.fingers("R", "IndexTrigger", [K(0, 0.7, "easeIn"), K(brk, 1), K(brk + 0.10, 0.7)])
        a.ev(brk - 0.002, "HammerDrop").ev(brk, "ShotBreak").ev(brk + 0.09, "TriggerReset")
        return a.out()
    A["Fire"] = fire()

    a = Action(0.22)
    a.bone(trig, "rotY", [K(0, 0, "easeIn"), K(0.07, g["trigger_pull"], "easeOut"), K(0.18, 0)])
    a.fingers("R", "IndexTrigger", [K(0, 0.7), K(0.07, 1), K(0.18, 0.7)])
    a.ev(0.07, "Sound:DryFire").ev(0.07, "HammerDrop")
    A["DryFire"] = a.out()

    # charge: pull the charging handle fully back and let it go
    hp = g["handle_pos"]
    def charge(lock_open=False, from_locked=False):
        a = Action(1.0 if not lock_open else 1.15)
        grab, pulled = 0.30, 0.48
        start = -(tr - 4) if from_locked else 0
        if g["style"] == "ak":
            # right hand leaves the grip, reaches over to the handle on the right side, rips it back, lets go
            side, fingers_hold = "R", "Grip"
            hf, hpalm = [0.3, 0.2, -0.93], [0, 1, 0]
            off = [0, -14, 6]
        elif g["style"] == "fal":
            # support hand comes back to the left-side handle (the handle does not move when firing)
            side, fingers_hold = "L", "SupportWrap"
            hf, hpalm = [0.3, -0.2, -0.93], [0, -1, 0]
            off = [0, 14, 6]
        else:
            # AR: support hand to the T-handle at the back of the upper, pinch and pull straight back
            side, fingers_hold = "L", "SupportWrap"
            hf, hpalm = [0.2, -0.95, 0.1], [-0.25, 0, -1]
            off = [-6, 10, 14]
        p0 = add(hp, add(off, [start if g["handle_moves_with_carrier"] else 0, 0, 0]))
        handle_bone = car if g["handle_moves_with_carrier"] else g["charging_handle"]
        hk = [hold(0), hand(0.22, "gun", add(p0, [10, 15 if side == "L" else -25, 30]), hf, hpalm, interp="easeOut"),
              hand(grab, "gun", p0, hf, hpalm), hand(pulled, "gun", add(p0, [-tr - (0 if g["handle_moves_with_carrier"] else 0), 0, 0]), hf, hpalm, interp="easeOut")]
        if g["handle_moves_with_carrier"]:
            a.bone(car, "tX", [K(grab, start, "easeIn"), K(pulled, -tr, "lin")] + ([K(pulled + 0.04, -tr)] if lock_open else [K(pulled + 0.035, 0)]))
        else:
            a.bone(g["charging_handle"], "tX", [K(grab, 0, "easeIn"), K(pulled, -tr, "lin"),
                                                 K(pulled + (0.10 if g["style"] == "ar" else 0.035), 0)])
            a.bone(car, "tX", [K(grab, start, "easeIn"), K(pulled, -tr, "lin")] +
                   ([K(pulled + 0.03, -(tr - 4))] if lock_open else [K(pulled + 0.035, 0)]))
        if g["style"] == "ar":   # AR handle is ridden home by the hand
            hk += [hand(pulled + 0.10, "gun", p0, hf, hpalm)]
        else:
            hk += [hand(pulled + 0.06, "gun", add(p0, [-tr - 20, 25 if side == "L" else -35, 20]), hf, hpalm)]
        if lock_open and g.get("bolt_catch"):
            a.bone(g["bolt_catch"], "rotY", [K(pulled - 0.02, 0), K(pulled + 0.02, g["bolt_catch_up"])])
            a.ev(pulled + 0.03, "BoltLock").ev(pulled + 0.03, "Sound:BoltLock")
            a.fingers("R", "ThumbPress", [K(0.35, 0), K(0.45, 1), K(0.55, 1), K(0.65, 0)])
        hk += [hold(0.90 if not lock_open else 1.05, "easeOut")]
        a.hand(side, hk)
        a.fingers(side, fingers_hold, [K(0, 1), K(0.15, 0), K(0.75, 0), K(0.92, 1)])
        a.fingers(side, "PinchRack", [K(0.15, 0), K(grab, 1), K(pulled + 0.04, 1), K(pulled + 0.12, 0)])
        a.gun("rotX", [K(0, 0), K(0.25, 10 if side == "R" else -12), K(0.75, 10 if side == "R" else -12), K(0.95, 0)], pivot=g["grip_socket"])
        f_unl = grab + (pulled - grab) * g["unlock_dist"] / tr
        if not from_locked:
            bolt_unlock(a, grab, f_unl, None if lock_open else pulled + 0.035, None if lock_open else pulled + 0.03)
        elif not lock_open:
            a.bone(g["bolt"], g["bolt_chan"], [K(pulled + 0.03, g["bolt_unlock"], "lin"), K(pulled + 0.035, 0)])
            if g.get("bolt_catch"):
                a.bone(g["bolt_catch"], "rotY", [K(grab + 0.02, g["bolt_catch_up"]), K(grab + 0.05, 0)])
        dust_cover(a, grab)
        a.ev(grab + 0.04, "Sound:ChargeBack").ev(pulled - 0.03, "SlideToRear")
        if not lock_open:
            a.ev(pulled + 0.03, "InBattery").ev(pulled + 0.035, "Sound:BoltForward")
            if not from_locked:
                a.d["holdAt"] = pulled
        return a.out()
    A["ChargeHandle"] = charge()
    if g["hold_open"]:
        A["LockOpen"] = charge(lock_open=True)
        A["ChargeFromLock"] = charge(from_locked=True)

    # bolt release (guns with a hold-open): slap the bolt catch / press the FAL catch
    if g["hold_open"]:
        a = Action(0.55)
        bc = g["bolt_catch_pos"]
        a.hand("L", [hold(0), hand(0.22, "gun", add(bc, [-10, 30, 10]), [0.1, -0.6, -0.8], [0, -1, 0], interp="easeOut"),
                     hand(0.27, "gun", add(bc, [-6, 22, 4]), [0.1, -0.6, -0.8], [0, -1, 0]), hold(0.50)])
        a.fingers("L", "SupportWrap", [K(0, 1), K(0.1, 0), K(0.36, 0), K(0.5, 1)]).fingers("L", "ThumbPress", [K(0.1, 0), K(0.2, 1), K(0.3, 1), K(0.4, 0)])
        a.bone(g["bolt_catch"], "rotY", [K(0.25, g["bolt_catch_up"], "lin"), K(0.27, 0)])
        a.bone(car, "tX", [K(0.27, -(tr - 4), "lin"), K(0.305, 0)])
        a.bone(g["bolt"], g["bolt_chan"], [K(0.30, g["bolt_unlock"], "lin"), K(0.305, 0)])
        a.ev(0.27, "BoltRelease").ev(0.30, "InBattery").ev(0.305, "Sound:BoltForward")
        A["BoltRelease"] = a.out()

    # magazines
    rock = g["mag_style"] == "rock"
    mr = g["mag_release_pos"]
    def mag_unlatch(a, t):
        a.bone(g["mag_release"], g["mag_release_chan"], [K(t - 0.04, 0, "lin"), K(t, g["mag_release_push"]), K(t + 0.08, g["mag_release_push"]), K(t + 0.12, 0)])
        a.ev(t, "MagDetach").ev(t, "Sound:MagRelease")

    def mag_out(retain):
        a = Action(1.0 if retain else 0.6)
        t_rel = 0.25
        a.gun("rotX", [K(0, 0), K(0.2, 20), K(0.6 if retain else 0.4, 20), K(0.85 if retain else 0.58, 0)], pivot=g["grip_socket"])
        if rock:
            a.hand("L", [hold(0), hand_on_mag(0.20, g, rock=0, interp="easeOut"), hand_on_mag(0.32, g, rock=g["mag_rock"]),
                         hand_on_mag(0.42, g, rock=g["mag_rock"], extra=[25, 0, -g["mag_clear"]])] +
                   ([hand(0.85, "body", g["mag_pouch"], [0, 0, -1], [-1, 0, 0], interp="easeIn")] if retain else [hold(0.58)]))
            a.bone(g["mag"], "rotY", [K(t_rel, 0, "easeOut"), K(0.32, g["mag_rock"])])
            a.bone(g["mag"], "tX", [K(0.32, 0), K(0.42, 25)]); a.bone(g["mag"], "tZ", [K(0.32, 0), K(0.42, -g["mag_clear"])])
            a.fingers("L", "ThumbPress", [K(0.12, 0), K(0.2, 1), K(0.3, 1), K(0.36, 0)])
        else:
            # AR: right index finger presses the release; mag drops or the left hand strips it
            a.fingers("R", "IndexTrigger", [K(0.12, 0), K(0.18, 0)])
            if retain:
                mag_tracks_along(a, g, [(t_rel, 0, "easeOut"), (0.42, g["mag_clear"], None)])
                a.hand("L", [hold(0), hand_on_mag(0.22, g, 0, interp="easeOut"), hand_on_mag(0.42, g, g["mag_clear"]),
                             hand(0.85, "body", g["mag_pouch"], [0, 0, -1], [-1, 0, 0], interp="easeIn")])
            else:
                mag_tracks_along(a, g, [(t_rel, 0, "easeIn"), (0.36, g["mag_clear"], None)])
                a.hand("L", [hold(0), hand(0.2, "gun", add(sh["pos"], [-40, 0, -30]), sh["fwd"], sh["palm"], socket=sh["socket"]), hold(0.55)])
        mag_unlatch(a, t_rel)
        if retain:
            a.attach(0.42, "L", g["mag"]).ev(0.85, "MagStow").ev(0.87, "Sound:MagPouch")
            a.fingers("L", "SupportWrap", [K(0, 1), K(0.12, 0)]).fingers("L", "PalmMag", [K(0.15, 0), K(0.22, 1)])
        else:
            a.ev(0.42 if rock else 0.36, "MagDrop")
            if rock:
                a.fingers("L", "SupportWrap", [K(0, 1), K(0.12, 0), K(0.45, 0), K(0.58, 1)]).fingers("L", "PalmMag", [K(0.12, 0), K(0.2, 1), K(0.4, 1), K(0.45, 0)])
        return a.out()
    A["MagOut"] = mag_out(True)
    A["MagDrop"] = mag_out(False)

    def mag_in(t0=0.0):
        a = Action(t0 + 1.1)
        a.attach(t0, "L", g["mag"]).ev(t0, "MagFromPouch").ev(t0, "Sound:MagPouch")
        a.gun("rotX", [K(t0, CUR), K(t0 + 0.3, 18), K(t0 + 0.85, 18), K(t0 + 1.05, 0)], pivot=g["grip_socket"])
        hk = [hand(t0, "body", g["mag_pouch"], [0, 0, -1], [-1, 0, 0])]
        if rock:   # hook the front lug, then rock the back up until it clicks
            hk += [hand_on_mag(t0 + 0.45, g, rock=g["mag_rock"], extra=[25, 0, -40], interp="easeOut"),
                   hand_on_mag(t0 + 0.55, g, rock=g["mag_rock"], extra=[0, 0, 0]),
                   hand_on_mag(t0 + 0.66, g, rock=0), hold(t0 + 0.95)]
            a.bone(g["mag"], "tX", [K(t0 + 0.45, 25), K(t0 + 0.55, 0)]); a.bone(g["mag"], "tZ", [K(t0 + 0.45, -40), K(t0 + 0.55, 0)])
            a.bone(g["mag"], "rotY", [K(t0 + 0.45, g["mag_rock"]), K(t0 + 0.55, g["mag_rock"], "easeIn"), K(t0 + 0.66, 0)])
            a.bone(g["mag_release"], g["mag_release_chan"], [K(t0 + 0.62, 0, "lin"), K(t0 + 0.65, g["mag_release_push"] * 0.5, "lin"), K(t0 + 0.67, 0)])
            seat = t0 + 0.66
        else:
            hk += [hand_on_mag(t0 + 0.45, g, 60, interp="easeOut"), hand_on_mag(t0 + 0.52, g, 30, interp="lin"), hand_on_mag(t0 + 0.60, g, 0),
                   hand_on_mag(t0 + 0.66, g, -3), hand_on_mag(t0 + 0.70, g, 0), hold(t0 + 0.95)]
            mag_tracks_along(a, g, [(t0 + 0.45, 60, "easeOut"), (t0 + 0.52, 30, "lin"), (t0 + 0.60, 0, None)])
            seat = t0 + 0.62
        a.hand("L", hk)
        a.attach(seat, "L", None)
        a.fingers("L", "PalmMag", [K(t0, 1), K(seat, 1), K(seat + 0.12, 0)]).fingers("L", "SupportWrap", [K(seat + 0.08, 0), K(t0 + 0.95, 1)])
        a.ev(seat, "MagAttach").ev(seat, "MagSeat").ev(seat, "Sound:MagIn")
        return a
    A["MagIn"] = mag_in().out()
    A["ReloadTactical"] = merge(A["MagOut"], mag_in(0.9).out(), 0.0)
    if g["hold_open"]:
        A["ReloadEmpty"] = merge(merge(A["MagDrop"], mag_in(0.45).out(), 0.0), A["BoltRelease"], 1.5)
    else:   # AK: no hold-open, so drop, insert, then charge
        A["ReloadEmpty"] = merge(merge(A["MagDrop"], mag_in(0.45).out(), 0.0), A["ChargeHandle"], 1.5)

    # mag check: pull the mag, tilt it to look at the top round, reinsert
    a = Action(1.7)
    a.gun("rotX", [K(0, 0), K(0.2, 20), K(1.4, 20), K(1.65, 0)], pivot=g["grip_socket"])
    if rock:
        a.hand("L", [hold(0), hand_on_mag(0.2, g, rock=0, interp="easeOut"), hand_on_mag(0.34, g, rock=g["mag_rock"]),
                     hand_on_mag(0.48, g, rock=g["mag_rock"], extra=[25, 0, -70]),
                     hand_on_mag(0.62, g, rock=g["mag_rock"], extra=[25, 0, -70], tilt=40), hand_on_mag(0.95, g, rock=g["mag_rock"], extra=[25, 0, -70], tilt=40),
                     hand_on_mag(1.1, g, rock=g["mag_rock"], extra=[25, 0, -70]),
                     hand_on_mag(1.2, g, rock=g["mag_rock"]), hand_on_mag(1.32, g, rock=0), hold(1.6)])
        a.bone(g["mag"], "rotY", [K(0.25, 0), K(0.34, g["mag_rock"]), K(1.2, g["mag_rock"], "easeIn"), K(1.32, 0)])
        a.bone(g["mag"], "tX", [K(0.34, 0), K(0.48, 25), K(1.1, 25), K(1.2, 0)]); a.bone(g["mag"], "tZ", [K(0.34, 0), K(0.48, -70), K(1.1, -70), K(1.2, 0)])
        seat = 1.32
    else:
        a.hand("L", [hold(0), hand_on_mag(0.2, g, 0, interp="easeOut"), hand_on_mag(0.42, g, 90), hand_on_mag(0.56, g, 90, tilt=40),
                     hand_on_mag(0.95, g, 90, tilt=40), hand_on_mag(1.1, g, 90), hand_on_mag(1.25, g, 0), hold(1.6)])
        mag_tracks_along(a, g, [(0.25, 0, "easeOut"), (0.42, 90, None), (1.1, 90, "easeIn"), (1.25, 0, None)])
        seat = 1.25
    mag_unlatch(a, 0.25)
    a.attach(0.25, "L", g["mag"]).attach(seat, "L", None)
    a.fingers("L", "SupportWrap", [K(0, 1), K(0.12, 0), K(1.4, 0), K(1.6, 1)]).fingers("L", "PalmMag", [K(0.12, 0), K(0.2, 1), K(seat, 1), K(seat + 0.1, 0)])
    a.ev(0.62, "MagCheckView").ev(seat, "MagAttach").ev(seat, "MagSeat").ev(seat, "Sound:MagIn")
    a.d["holdAt"] = 0.7
    A["MagCheck"] = a.out()

    # chamber (press) check: ease the bolt back 25 mm with the handle, look, let it close, tap forward
    a = Action(1.4)
    open_mm = 25.0
    if g["style"] == "ak":
        side, hf, hpalm, off = "R", [0.3, 0.2, -0.93], [0, 1, 0], [0, -14, 6]
    elif g["style"] == "fal":
        side, hf, hpalm, off = "L", [0.3, -0.2, -0.93], [0, -1, 0], [0, 14, 6]
    else:
        side, hf, hpalm, off = "L", [0.2, -0.95, 0.1], [-0.25, 0, -1], [-6, 10, 14]
    p0 = add(hp, off)
    a.hand(side, [hold(0), hand(0.3, "gun", p0, hf, hpalm, interp="easeOut"), hand(0.45, "gun", add(p0, [-open_mm, 0, 0]), hf, hpalm),
                  hand(0.95, "gun", add(p0, [-open_mm, 0, 0]), hf, hpalm), hand(1.05, "gun", p0, hf, hpalm), hold(1.35)])
    hold_pose = "Grip" if side == "R" else "SupportWrap"
    a.fingers(side, hold_pose, [K(0, 1), K(0.15, 0), K(1.15, 0), K(1.35, 1)]).fingers(side, "PinchRack", [K(0.15, 0), K(0.3, 1), K(1.05, 1), K(1.15, 0)])
    if g["handle_moves_with_carrier"]:
        a.bone(car, "tX", [K(0.3, 0), K(0.45, -open_mm), K(0.95, -open_mm), K(1.05, 0)])
    else:
        a.bone(g["charging_handle"], "tX", [K(0.3, 0), K(0.45, -open_mm), K(0.95, -open_mm), K(1.05, 0)])
        a.bone(car, "tX", [K(0.3, 0), K(0.45, -open_mm), K(0.95, -open_mm), K(1.05, 0)])
    a.bone(g["bolt"], g["bolt_chan"], [K(0.3, 0), K(0.36, g["bolt_unlock"]), K(1.0, g["bolt_unlock"]), K(1.05, 0)])
    a.gun("rotX", [K(0, 0), K(0.3, -30 if g["style"] != "fal" else 30), K(1.0, -30 if g["style"] != "fal" else 30), K(1.3, 0)], pivot=g["grip_socket"])
    dust_cover(a, 0.3)
    a.ev(0.6, "ChamberView").ev(1.05, "Sound:BoltForward")
    a.d["holdAt"] = 0.6
    A["PressCheck"] = a.out()

    # safety / selector
    def safety(on):
        a = Action(0.45)
        fr, to = (g["sel_fire"], g["sel_safe"]) if on else (g["sel_safe"], g["sel_fire"])
        a.bone(g["selector"], "rotY", [K(0.12, fr, "easeIn"), K(0.22, to)])
        if g["style"] == "ak":   # long lever on the right: right index finger / thumb sweeps it
            a.fingers("R", "IndexTrigger", [K(0.0, 0), K(0.1, 0)])
            a.hand("R", [hold(0), hand(0.12, "gun", add(g["selector_pos"], [40, -22, -14]), [0.2, 0.3, -0.93], [0, 1, 0], interp="easeOut"),
                         hand(0.22, "gun", add(g["selector_pos"], [40, -22, -20 if not on else -8]), [0.2, 0.3, -0.93], [0, 1, 0]), hold(0.42)])
        else:
            a.fingers("R", "ThumbPress", [K(0, 0), K(0.10, 1), K(0.26, 1), K(0.38, 0)])
        a.ev(0.22, "SafetyOn" if on else "SafetyOff").ev(0.22, "Sound:SafetyClick")
        return a.out()
    A["SafetyOn"] = safety(True)
    A["SafetyOff"] = safety(False)

    # tap-rack
    a = Action(0.34)
    if rock:
        a.hand("L", [hold(0), hand_on_mag(0.12, g, rock=0, extra=[0, 0, -8], interp="easeOut"), hand_on_mag(0.16, g, rock=0), hold(0.32)])
    else:
        a.hand("L", [hold(0), hand_on_mag(0.12, g, 8, interp="easeOut"), hand_on_mag(0.16, g, -2), hold(0.32)])
    a.fingers("L", "SupportWrap", [K(0, 1), K(0.06, 0), K(0.24, 0), K(0.32, 1)]).fingers("L", "PalmMag", [K(0.06, 0), K(0.12, 1), K(0.18, 1), K(0.24, 0)])
    a.ev(0.16, "MagSeat").ev(0.16, "Sound:MagTap")
    A["TapRack"] = merge(a.out(), A["ChargeHandle"], 0.28)
    A["ClearStovepipe"] = dict(A["ChargeHandle"], events=sorted(A["ChargeHandle"]["events"] + [[0.40, "ClearMalfunction"]], key=lambda e: e[0]))
    A["ClearStovepipe"].pop("holdAt", None)
    seq = A.get("LockOpen", A["ChargeHandle"])
    seq = merge(seq, A["MagDrop"], seq["duration"]); t = seq["duration"]
    for i in range(3):
        seq = merge(seq, A["ChargeHandle"], t); t = seq["duration"]
    seq = merge(seq, A["MagIn"], t); t = seq["duration"]
    seq = merge(seq, A["ChargeHandle"], t)
    seq["events"].append([round(A.get("LockOpen", A["ChargeHandle"])["duration"] + 1.0, 4), "ClearMalfunction"]); seq["events"].sort(key=lambda e: e[0])
    A["ClearDoubleFeed"] = seq

    # low ready <-> shouldered
    def shoulder(up):
        a = Action(0.55)
        lo = {"tZ": -170, "tX": -60, "rotY": 30}
        for ch, v in lo.items():
            a.gun(ch, [K(0, v), K(0.5, 0)] if up else [K(0, 0), K(0.5, v)], pivot=g["grip_socket"])
        a.ev(0.5 if up else 0.0, "Shouldered" if up else "LowReady")
        return a.out()
    A["Draw"] = shoulder(True)
    A["Holster"] = shoulder(False)

    a = Action(2.6)
    a.gun("tX", [K(0, 0), K(0.4, -80), K(2.2, -80), K(2.6, 0)], pivot=g["grip_socket"])
    a.gun("rotX", [K(0, 0), K(0.5, -45), K(1.2, -45), K(1.6, 35), K(2.1, 35), K(2.6, 0)])
    a.gun("rotZ", [K(0, 0), K(0.5, 15), K(1.2, 15), K(1.6, -12), K(2.1, -12), K(2.6, 0)])
    A["Inspect"] = a.out()

    states = {
        "BoltHome": {car: {"tX": 0}, g["bolt"]: {g["bolt_chan"]: 0}},
        "BoltLocked": {car: {"tX": -(tr - 4)}, g["bolt"]: {g["bolt_chan"]: g["bolt_unlock"]}},
        "SafetyOn": {g["selector"]: {"rotY": g["sel_safe"]}}, "SafetyOff": {g["selector"]: {"rotY": g["sel_fire"]}},
        "MagIn": {g["mag"]: {"tX": 0, "tZ": 0, "rotY": 0}},
        "MagOut": {g["mag"]: {"tZ": -500}},
    }
    if g.get("bolt_catch"):
        states["BoltLocked"][g["bolt_catch"]] = {"rotY": g["bolt_catch_up"]}; states["BoltHome"][g["bolt_catch"]] = {"rotY": 0}
    if g.get("sel_auto") is not None:
        states["FullAuto"] = {g["selector"]: {"rotY": g["sel_auto"]}}
    if g.get("dust_cover"):
        states["DustCoverOpen"] = {g["dust_cover"]: {"rotX": g["dust_open"]}}; states["DustCoverClosed"] = {g["dust_cover"]: {"rotX": 0}}
    return A, states, {"handR": grip_hold(g), "handL": support_hold(g), "fingersR": {"Grip": 1, "IndexTrigger": 0}, "fingersL": {"SupportWrap": 1}}

# ---------------------------------------------------------------- merging actions in sequence
def shift_keys(keys, dt):
    return [[round(k[0] + dt, 4)] + k[1:] for k in keys]

def merge(a, b, dt):
    """returns action a with action b appended, starting at time dt (b's tracks win where both key a channel after dt)"""
    out = json.loads(json.dumps(a))
    out.pop("holdAt", None)          # chained drills play straight through
    out["duration"] = round(max(a["duration"], dt + b["duration"]), 4)
    for bone, chans in b.get("bones", {}).items():
        for ch, keys in chans.items():
            old = out.setdefault("bones", {}).setdefault(bone, {}).get(ch, [])
            out["bones"][bone][ch] = [k for k in old if k[0] < dt] + shift_keys(keys, dt)
    if "gun" in b:
        g = out.setdefault("gun", {})
        for ch, keys in b["gun"].items():
            if ch == "pivot": g.setdefault("pivot", keys); continue
            old = g.get(ch, [])
            g[ch] = [k for k in old if k[0] < dt] + shift_keys(keys, dt)
    for side in ("L", "R"):
        hk = b.get("hand" + side)
        if hk:
            old = [k for k in out.get("hand" + side, []) if k["t"] < dt]
            out["hand" + side] = old + [dict(k, t=round(k["t"] + dt, 4)) for k in hk]
        for pose, keys in b.get("fingers" + side, {}).items():
            old = out.setdefault("fingers" + side, {}).get(pose, [])
            out["fingers" + side][pose] = [k for k in old if k[0] < dt] + shift_keys(keys, dt)
    out["events"] = sorted(out.get("events", []) + [[round(e[0] + dt, 4), e[1]] for e in b.get("events", [])], key=lambda e: e[0])
    att = out.get("attach", []) + [[round(e[0] + dt, 4)] + e[1:] for e in b.get("attach", [])]
    if att: out["attach"] = sorted(att, key=lambda e: e[0])
    return out

# ---------------------------------------------------------------- checks
def check(gun, data):
    errs = []
    for name, a in data["actions"].items():
        d = a["duration"]
        def keys_ok(where, keys):
            ts = [k[0] for k in keys]
            if ts != sorted(ts): errs.append("%s %s %s: keys out of order" % (gun, name, where))
            if ts and (ts[0] < 0 or ts[-1] > d + 1e-6): errs.append("%s %s %s: key outside 0..%s" % (gun, name, where, d))
        for bone, chans in a.get("bones", {}).items():
            for ch, keys in chans.items(): keys_ok(bone + "." + ch, keys)
        for ch, keys in a.get("gun", {}).items():
            if ch != "pivot": keys_ok("gun." + ch, keys)
        for side in ("L", "R"):
            hk = a.get("hand" + side, [])
            ts = [k["t"] for k in hk]
            if ts != sorted(ts): errs.append("%s %s hand%s: keys out of order" % (gun, name, side))
            if ts and ts[-1] > d + 1e-6: errs.append("%s %s hand%s: key after the end" % (gun, name, side))
            for pose, keys in a.get("fingers" + side, {}).items(): keys_ok("fingers%s.%s" % (side, pose), keys)
        for e in a.get("events", []):
            if e[0] > d + 1e-6: errs.append("%s %s: event %s after the end" % (gun, name, e[1]))
    return errs

# ---------------------------------------------------------------- export to the game's format (docs/animation/gun_anim_format.md)
# Above, tracks are authored per channel (tX, rotY, ...) with [t, value, interp] keys. The game wants one location
# track and one rotation track per bone, rotations as axis + degrees, keys as {"t", "v", "ease"}, hand targets with
# absolute gun-space positions, and finger weights as {"t", "pose", "weight"} keys. This section converts.

SOCKETS = {   # socket bones used as hand anchors (gun space, mm), from docs/weapons/<gun>.md
    "Pistol_M9":   {"GripHand": [18, 0, -80]},
    "Pistol_1911": {"GripHand": [45, 0, -56]},
    "Rifle_AK":    {"GripHand": [-188, 0, -84], "SupportHand": [180, 0, -12]},
    "Rifle_FAL":   {"GripHand": [-195, 0, -90], "SupportHand": [190, 0, -10]},
    "Carbine_AR":  {"GripHand": [-171, 0, -73], "SupportHand": [150, 0, -10]},
}
BODY_Z = 980.0     # body space origin is the pawn root at the feet; the belt numbers above are from the pelvis
SAMPLE = 1.0 / 60  # where channels with different curves are combined, sample at 60 per second
EASE = {None: "smooth", "ease": "smooth", "easeIn": "smooth", "easeOut": "smooth", "lin": "linear", "step": "step"}

def shape(interp, u):
    if interp == "lin": return u
    if interp == "step": return 0.0
    if interp == "easeIn": return u * u
    if interp == "easeOut": return 1 - (1 - u) * (1 - u)
    return u * u * (3 - 2 * u)

def resolve_cur(keys):
    """a null value after the first key means 'hold the previous value'; only a first key may stay null"""
    out = []
    for k in keys:
        if k[1] is None and out:
            k = [k[0], out[-1][1]] + k[2:]
        out.append(k)
    return out

def ch_eval(keys, t):
    if t <= keys[0][0]: return keys[0][1]
    for k in keys:
        if abs(k[0] - t) < 1e-9: return k[1]
    for i in range(len(keys) - 1):
        a, b = keys[i], keys[i + 1]
        if a[0] <= t <= b[0]:
            if b[0] == a[0]: return b[1]
            u = (t - a[0]) / (b[0] - a[0])
            return a[1] + (b[1] - a[1]) * shape(a[2] if len(a) > 2 else None, u)
    return keys[-1][1]

def ch_interp_at(keys, t):
    """the curve a channel is on just after time t: (interp, starts exactly at t, ends at next union time?)"""
    for i in range(len(keys) - 1):
        if keys[i][0] <= t < keys[i + 1][0]:
            return (keys[i][2] if len(keys[i]) > 2 else None), keys[i][0], keys[i + 1][0], keys[i][1] != keys[i + 1][1]
    return None, None, None, False

def combine(chans, names, to_value):
    """chans: {name: keys}; returns [{"t","v","ease"}] with v = to_value({name: value})"""
    chans = {n: resolve_cur(k) for n, k in chans.items() if n in names and k}
    if not chans: return None
    times = sorted({k[0] for ks in chans.values() for k in ks})
    out = []
    def val(t):
        vals = {}
        for n in names:
            if n not in chans: vals[n] = 0.0; continue
            v = ch_eval(chans[n], t)
            vals[n] = v
        if any(v is None for v in vals.values()):
            if all(vals[n] is None for n in chans): return None
            raise ValueError("a null key shares a moment with a numeric key: %s" % vals)
        return to_value(vals)
    for i, t in enumerate(times):
        if i == len(times) - 1:
            out.append({"t": round(t, 4), "v": val(t), "ease": "smooth"}); break
        tn = times[i + 1]
        moving = []
        for n, ks in chans.items():
            ip, a0, a1, changes = ch_interp_at(ks, t)
            if changes: moving.append((ip, a0, a1))
        ips = {EASE[m[0]] for m in moving}
        exact = all(m[0] in (None, "ease", "lin", "step") for m in moving) and len(ips) <= 1 and \
            all(m[0] == "lin" or (abs(m[1] - t) < 1e-9 and abs(m[2] - tn) < 1e-9) for m in moving)
        if exact:
            out.append({"t": round(t, 4), "v": val(t), "ease": ips.pop() if ips else "linear"})
        else:   # curves differ: sample this stretch
            n = max(2, int(math.ceil((tn - t) / SAMPLE)))
            for j in range(n):
                tj = t + (tn - t) * j / n
                out.append({"t": round(tj, 4), "v": val(tj), "ease": "linear"})
    # drop keys that repeat the value of both neighbours (holds)
    slim = []
    for i, k in enumerate(out):
        if 0 < i < len(out) - 1 and k["v"] == out[i - 1]["v"] == out[i + 1]["v"]: continue
        slim.append(k)
    return slim

def loc_value(v): return [round(v[c], 3) + 0.0 for c in ("tX", "tY", "tZ")]

def axis_angle(R):
    """rotation matrix -> {"axis", "deg"}"""
    c = max(-1.0, min(1.0, (R[0][0] + R[1][1] + R[2][2] - 1) / 2))
    ang = math.acos(c)
    if ang < 1e-9: return {"axis": [0, 0, 1], "deg": 0.0}
    ax = [R[2][1] - R[1][2], R[0][2] - R[2][0], R[1][0] - R[0][1]]
    l = math.sqrt(dot(ax, ax))
    if l < 1e-9:   # 180 degrees
        ax = norm([math.sqrt(max(0, (R[i][i] + 1) / 2)) for i in range(3)])
    else:
        ax = [x / l for x in ax]
    return {"axis": r3(ax), "deg": round(math.degrees(ang), 3)}

def rot_z(deg):
    c, s_ = math.cos(math.radians(deg)), math.sin(math.radians(deg)); return [[c, -s_, 0], [s_, c, 0], [0, 0, 1]]

def rot_value_for(used):
    def f(v):
        nz = [c for c in ("rotX", "rotY", "rotZ") if c in used]
        if len(nz) == 1:   # one axis: keep the sign so the curve passes cleanly through zero
            ax = {"rotX": [1, 0, 0], "rotY": [0, 1, 0], "rotZ": [0, 0, 1]}[nz[0]]
            return {"axis": ax, "deg": round(v[nz[0]], 3)}
        R = mat_mul(rot_z(v["rotZ"]), mat_mul(rot_y(v["rotY"]), rot_x(v["rotX"])))   # roll, then pitch, then yaw
        return axis_angle(R)
    return f

def export_channels(chans):
    out = {}
    loc = combine(chans, ("tX", "tY", "tZ"), loc_value)
    if loc: out["loc"] = loc
    used = [c for c in ("rotX", "rotY", "rotZ") if chans.get(c)]
    rot = combine(chans, ("rotX", "rotY", "rotZ"), rot_value_for(used)) if used else None
    if rot: out["rot"] = rot
    return out

def export_hand_key(k, gun):
    if k.get("hold"):
        o = {"t": k["t"], "hold": True}
    else:
        pos = k["pos"]
        if k.get("socket"): pos = add(SOCKETS[gun][k["socket"]], pos)
        if k["space"] == "body": pos = add(pos, [0, 0, BODY_Z])
        o = {"t": k["t"], "space": k["space"], "pos": r3(pos), "fingersDir": k["fwd"], "palmNormal": k["palm"]}
    o["ease"] = EASE[k.get("interp")]
    return o

def export_fingers(tracks):
    out = []
    for pose, keys in tracks.items():
        assert pose in FINGER_POSES, pose
        for k in keys:
            out.append({"t": k[0], "pose": pose, "weight": k[1], "ease": EASE[k[2] if len(k) > 2 else None]})
    return sorted(out, key=lambda k: (k["t"], k["pose"]))

def strip_attached(a):
    """while a hand carries a bone (attach .. detach) the bone follows the hand, so its own keys in between are dropped"""
    spans = {}
    open_ = {}
    for t, side, what in a.get("attach", []):
        if what is not None:
            open_[side] = (what, t)
        elif side in open_:
            w, t0 = open_.pop(side); spans.setdefault(w, []).append((t0, t))
    for side, (w, t0) in open_.items():
        spans.setdefault(w, []).append((t0, 1e9))
    for bone, sp in spans.items():
        chans = a.get("bones", {}).get(bone)
        if not chans: continue
        for ch in list(chans):
            chans[ch] = [k for k in chans[ch] if not any(t0 < k[0] < t1 for t0, t1 in sp)]
            if not chans[ch]: del chans[ch]
        if not chans: del a["bones"][bone]

def export_action(a, gun):
    a = json.loads(json.dumps(a))
    strip_attached(a)
    o = {"duration": a["duration"]}
    if "holdAt" in a: o["holdAt"] = round(a["holdAt"], 4)
    if a.get("bones"):
        o["bones"] = {b: export_channels(ch) for b, ch in sorted(a["bones"].items())}
        o["bones"] = {b: v for b, v in o["bones"].items() if v}
    if a.get("gun"):
        # the whole-gun offset starts every action at zero, so a leading "current value" key is simply 0
        gch = {k: [[kk[0], 0.0 if kk[1] is None else kk[1]] + kk[2:] for kk in v] for k, v in a["gun"].items() if k != "pivot"}
        o["gun"] = dict({"pivot": a["gun"].get("pivot", "GripHand")}, **export_channels(gch))
    for side in ("L", "R"):
        if a.get("hand" + side): o["hand" + side] = [export_hand_key(k, gun) for k in a["hand" + side]]
        if a.get("fingers" + side): o["fingers" + side] = export_fingers(a["fingers" + side])
    if a.get("attach"): o["attach"] = [{"t": t, "hand": side, "bone": w} for t, side, w in a["attach"]]
    if a.get("events"): o["events"] = [{"t": t, "name": n} for t, n in a["events"]]
    return o

def export_state(st):
    out = {}
    for bone, chans in st.items():
        o = {}
        if any(c in chans for c in ("tX", "tY", "tZ")):
            o["loc"] = [round(chans.get(c, 0.0), 3) for c in ("tX", "tY", "tZ")]
        used = [c for c in ("rotX", "rotY", "rotZ") if c in chans]
        if used:
            o["rot"] = rot_value_for(used)({c: chans.get(c, 0.0) for c in ("rotX", "rotY", "rotZ")})
        out[bone] = o
    return out

def export_hold(h, gun):
    o = {}
    for side in ("L", "R"):
        k = h["hand" + side]
        f = norm(k["fwd"]); pn = norm(sub(k["palm"], mul(f, dot(k["palm"], f))))
        o["hand" + side] = {"space": "gun", "pos": r3(add(SOCKETS[gun][k["socket"]], k["pos"])),
                            "fingersDir": r3(f), "palmNormal": r3(pn)}
        o["fingers" + side] = [{"pose": p, "weight": w} for p, w in h["fingers" + side].items()]
    return o

def check_export(gun, data):
    errs = []
    for name, a in data["actions"].items():
        d = a["duration"]
        def ok(where, keys):
            ts = [k["t"] for k in keys]
            if ts != sorted(ts): errs.append("%s %s %s: keys out of order" % (gun, name, where))
            if ts and (ts[0] < -1e-9 or ts[-1] > d + 1e-6): errs.append("%s %s %s: key outside 0..%s" % (gun, name, where, d))
            for k in keys:
                if k.get("ease", "smooth") not in ("linear", "smooth", "step"): errs.append("%s %s %s: bad ease" % (gun, name, where))
        for b, tr in a.get("bones", {}).items():
            for c, keys in tr.items(): ok(b + "." + c, keys)
        for c in ("loc", "rot"):
            if c in a.get("gun", {}): ok("gun." + c, a["gun"][c])
        for side in ("L", "R"):
            ok("hand" + side, a.get("hand" + side, []))
            for k in a.get("hand" + side, []):
                if not k.get("hold"):
                    if abs(dot(k["fingersDir"], k["palmNormal"])) > 0.02: errs.append("%s %s hand%s t=%s: palm not square to fingers" % (gun, name, side, k["t"]))
            for k in a.get("fingers" + side, []):
                if not 0 <= k["weight"] <= 1: errs.append("%s %s fingers%s: weight out of 0..1" % (gun, name, side))
        if "holdAt" in a and not 0 < a["holdAt"] < d: errs.append("%s %s: holdAt outside the action" % (gun, name))
        for e in a.get("events", []):
            if e["t"] > d + 1e-6: errs.append("%s %s: event %s after the end" % (gun, name, e["name"]))
    return errs

def dump(o, ind=0):
    """JSON with one key, vector or small object per line, so diffs and reading stay easy"""
    pad = "  " * ind
    if isinstance(o, dict) and ("t" in o or "axis" in o or "pose" in o or (o and all(k in ("loc", "rot") for k in o) and not any(isinstance(v, list) and v and isinstance(v[0], dict) for v in o.values()))):
        return json.dumps(o, separators=(", ", ": "))
    if isinstance(o, list) and all(not isinstance(x, (dict, list)) for x in o):
        return json.dumps(o, separators=(", ", ": "))
    if isinstance(o, dict):
        if not o: return "{}"
        return "{\n" + ",\n".join(pad + "  " + json.dumps(k) + ": " + dump(v, ind + 1) for k, v in o.items()) + "\n" + pad + "}"
    if isinstance(o, list):
        if not o: return "[]"
        return "[\n" + ",\n".join(pad + "  " + dump(v, ind + 1) for v in o) + "\n" + pad + "]"
    return json.dumps(o)

def main():
    os.makedirs(OUT, exist_ok=True)
    all_errs = []
    for gun, g in GUNS.items():
        acts, states, holdd = (pistol_actions if g["kind"] == "pistol" else rifle_actions)(g)
        all_errs += check(gun, {"actions": acts})
        data = {"gun": gun, "version": 1, "hold": export_hold(holdd, gun),
                "states": {k: export_state(v) for k, v in states.items()},
                "actions": {k: export_action(v, gun) for k, v in acts.items()}}
        all_errs += check_export(gun, data)
        with open(os.path.join(OUT, gun + ".json"), "w", newline="\n") as f:
            f.write(dump(data) + "\n")
        print("%-12s %2d actions: %s" % (gun, len(acts), ", ".join(acts)))
    if all_errs:
        print("\n".join(all_errs)); sys.exit(1)

if __name__ == "__main__":
    main()
