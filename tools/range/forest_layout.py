"""Forest range layout: the numbers behind docs/maps/FOREST_RANGE.md.

Mirrors AHoldoverForestRange (Holdover/Source/Holdover/Range/HoldoverForestRange.cpp).
Keep the two in sync. Units are metres. The shooter stands at the origin facing +X
(downrange); +Y is to the shooter's right.

Used by plan_forest_range.py (top-down plan picture) and, later, by the Blender terrain
script, so the sculpted ground matches what the game expects.
"""
import math

# ---- Layout -------------------------------------------------------------------------
GROUND_BEHIND = 90.0       # ground built from X = -90 ...
GROUND_AHEAD = 260.0       # ... to X = +260
GROUND_HALF_WIDTH = 90.0   # and Y = -90 .. +90
LANE_HALF_WIDTH = 13.0     # mown lane floor between the side berms
BACK_BERM_TOE = 190.0     # range lengthened 3.5x on Miles's ask (was 56)
BACK_BERM_HEIGHT = 5.0
SIDE_BERM_HEIGHT = 1.8      # lower than the first design, so the lane reads less like a corridor
PAD_DEPTH = 7.0            # gravel pad from X = -7 to +1
FOREST_FLOOR_ROLL = 1.2      # rolling forest floor (was 0.7; Miles: "a lot less flat", 2026-11-22)

TARGET_DISTANCES = [5, 7, 10, 15, 25, 50, 75, 100, 150]   # paper targets
TARGET_SPACING = 1.6                        # sideways spacing, metres
DISTANCE_SIGNS = [5, 10, 25, 50, 75, 100, 150]

CLEAR_BEHIND = -12.0
CLEAR_SIDE_EXTRA = 3.0
CLEAR_PAST_BERM_TOE = 10.0
WALKWAY_Y = -9.0           # worn path down the left side to the targets: its middle line ...
WALKWAY_WANDER = 1.6       # ... wanders this far either side of it (Miles: no straight lines, 2026-11-22)
WALKWAY_HALF = 0.5         # half width where it is most worn; it narrows and fades out in places

# ---- Landform (Miles, 2026-11-22: "a lot less flat, hills and mounds, the far end higher than the player") ----
# The range sits in the bottom of a shallow valley: the ground climbs gently downrange to the backstop
# and keeps climbing behind it, climbs into the forest on both sides, and a little behind the pad. The
# forest is hilly; the lane and clearing get low mounds and hollows, kept off the lines of fire.
FAR_RISE = 6.0             # metres the ground climbs from the firing line to the backstop toe
FAR_RISE_POWER = 1.6       # > 1: the climb steepens toward the backstop, so it never humps into a line of sight
BEHIND_RISE = 3.5          # climb behind the pad, out to the back of the map
SIDE_RISE = 8.0            # climb from the tree line to the map's side edges
SIDE_RISE_RUN = 60.0       # over this many metres
HILL_HEIGHT = 3.5          # rolling hills in the forest
HILL_SCALE = 55.0          # metres across a hill
LANE_MOUND = 0.7           # low mounds and hollows in the lane and clearing, away from the lines of fire

ACCESS_PATH = [(-7, -7), (-12, -9.5), (-20, -11), (-30, -15), (-42, -19), (-56, -20), (-70, -18), (-95, -23)]

BENCHES = [(-0.3, 1.1), (-0.3, -4.2), (-0.3, 6.4)]
STOOLS = [(-1.1, -4.2), (-1.1, 6.4)]
BRASS_BIN = (-2.4, 3.4)
REST_BENCH = (-10.0, 4.0)
FIRING_LINE_SIGN = (0.0, -LANE_HALF_WIDTH - 0.6)
CEASE_FIRE_SIGN = (-8.5, -9.0)
SHOOTER = (-1.5, 0.0)

FOREST, GRASS, GRAVEL, DIRT, EARTH = range(5)

# ---- Irregular edges -------------------------------------------------------------------
# The range is carved out of the forest by hand, not with a ruler: the lane edge, the berms and the
# tree line all wander. wobble() is a smooth wave in [-1, 1] made from three sines, so the game
# (C++, same formula) and Blender get exactly the same shape.
LANE_EDGE_WANDER = 4.0      # metres the lane edge drifts in and out (Miles: no straight border, 2026-11-22)
LANE_EDGE_RIPPLE = 1.2      # plus a shorter ripple, so no stretch of the edge runs straight
LANE_EDGE_MIN = 9.5         # never closer to the centre than this (targets stand within 6.4 m)
BERM_WANDER = 0.45          # share the side berm height and width drift
TREE_LINE_WANDER = 8.0      # metres the tree line drifts in and out
BACK_TOE_WANDER = 1.5       # metres the backstop toe drifts


def wobble(t, phase):
    return (0.55 * math.sin(t * 0.083 + phase) + 0.30 * math.sin(t * 0.197 + phase * 2.3)
            + 0.15 * math.sin(t * 0.471 + phase * 3.7))


def lane_edge(x, y):
    """Lane half width at x on the side of y (the lane floor ends here and the side berm starts)."""
    ph = 1.1 if y >= 0 else 4.2
    e = LANE_HALF_WIDTH + LANE_EDGE_WANDER * wobble(x * 1.9, ph) + LANE_EDGE_RIPPLE * wobble(x * 4.3, ph + 2.0)
    return max(e, LANE_EDGE_MIN)


def back_toe(y):
    return BACK_BERM_TOE + BACK_TOE_WANDER * wobble(y, 6.1)


def smoothstep(a, b, x):
    t = min(max((x - a) / (b - a), 0.0), 1.0)
    return t * t * (3 - 2 * t)


def berm_profile(d, toe, crest_start, crest_end, back_toe, height):
    if d <= toe or d >= back_toe:
        return 0.0
    if d < crest_start:
        return height * smoothstep(toe, crest_start, d)
    if d <= crest_end:
        return height
    return height * (1 - smoothstep(crest_end, back_toe, d))


def berm_height(x, y):
    ay, l = abs(y), lane_edge(x, y)
    ph = 2.9 if y >= 0 else 5.3
    h = SIDE_BERM_HEIGHT * (1.0 + BERM_WANDER * wobble(x, ph))
    w = 1.0 + BERM_WANDER * wobble(x, ph + 1.7)
    side = berm_profile(ay, l, l + 3.5 * w, l + 4.5 * w, l + 8.0 * w, h)
    side *= smoothstep(-6, -3, x) * (1 - smoothstep(BACK_BERM_TOE + 10, BACK_BERM_TOE + 14, x))
    side *= berm_gate(x, y)
    t = back_toe(y)
    back = berm_profile(x, t, t + 5, t + 6.5, t + 13, BACK_BERM_HEIGHT)
    back *= 1 - smoothstep(l + 6, l + 12, ay)
    return max(side, back)


def valley(x, y):
    """Large-scale climb of the ground (metres) around the range; 0 on the firing pad."""
    t = max(x - 2.0, 0.0) / (BACK_BERM_TOE - 2.0)
    far = FAR_RISE * t ** FAR_RISE_POWER
    behind = BEHIND_RISE * smoothstep(0.0, GROUND_BEHIND + CLEAR_BEHIND, CLEAR_BEHIND - x)
    out = max(abs(y) - tree_line(x, y), 0.0)
    side = SIDE_RISE * (1.0 + 0.35 * wobble(x * 0.9, 1.4 if y >= 0 else 3.6)) * smoothstep(0.0, SIDE_RISE_RUN, out)
    return far + behind + side


def fire_distance(x, y):
    """Metres from (x, y) sideways to the nearest line of fire (bench to each target, on to 3 m past it)."""
    best = 1e9
    n = len(TARGET_DISTANCES)
    for i, d in enumerate(TARGET_DISTANCES):
        if x > d + 3.0:
            continue
        ty = (i - (n - 1) * 0.5) * TARGET_SPACING
        best = min(best, abs(y - ty * (x - SHOOTER[0]) / (d - SHOOTER[0])))
    return best


def mound_weight(x, y):
    """0..1: where the lane may have mounds. None on the pad, near lines of fire, on the walkway or the backstop face."""
    w = smoothstep(2.0, 8.0, x) * smoothstep(1.5, 5.0, fire_distance(x, y))
    w *= 1.0 - smoothstep(back_toe(y) - 8.0, back_toe(y) - 3.0, x)
    w *= smoothstep(0.4, 2.5, abs(y - walkway_y(x)))
    return w


def walkway_y(x):
    return WALKWAY_Y + WALKWAY_WANDER * wobble(x * 2.6, 0.8)


def on_walkway(x, y):
    """The worn path: it wanders, narrows, and fades out under the grass in places."""
    worn = 0.5 + 0.5 * wobble(x * 3.4, 2.0)
    return worn > 0.3 and abs(y - walkway_y(x)) < WALKWAY_HALF * (0.4 + 0.8 * worn)


def berm_gate(x, y):
    """The side berms come and go: in long stretches they flatten out and the forest meets the lane directly."""
    return smoothstep(-0.35, 0.25, wobble(x * 1.4, (2.9 if y >= 0 else 5.3) + 0.6))


def litter_island(x, y):
    """Patches of leaf litter out in the lane, so the mown grass isn't one clean strip."""
    return wobble(x * 3.1 + y * 0.7, 1.3) * wobble(y * 2.9 - x * 0.4, 2.6) > 0.22


def _segment_distance(p, a, b):
    abx, aby = b[0] - a[0], b[1] - a[1]
    t = ((p[0] - a[0]) * abx + (p[1] - a[1]) * aby) / max(abx * abx + aby * aby, 1e-4)
    t = min(max(t, 0.0), 1.0)
    return math.hypot(p[0] - (a[0] + abx * t), p[1] - (a[1] + aby * t))


def path_distance(x, y):
    return min(_segment_distance((x, y), ACCESS_PATH[i], ACCESS_PATH[i + 1]) for i in range(len(ACCESS_PATH) - 1))


def tree_line(x, y):
    """How far out from the centre line the trees start at x (side of y)."""
    return lane_edge(x, y) + CLEAR_SIDE_EXTRA + TREE_LINE_WANDER * wobble(x * 1.6, 7.3 if y >= 0 else 8.9)


def cleared_for_trees(x, y, margin=0.0):
    ahead = BACK_BERM_TOE + CLEAR_PAST_BERM_TOE + TREE_LINE_WANDER * wobble(y, 9.4)
    if CLEAR_BEHIND - margin < x < ahead + margin and abs(y) < tree_line(x, y) + margin:
        return True
    return path_distance(x, y) < 2.0 + margin


def surface_at(x, y):
    ay = abs(y)
    if x < -PAD_DEPTH and path_distance(x, y) < 1.1:
        return DIRT
    l = lane_edge(x, y)
    pad_edge = LANE_HALF_WIDTH - 1.0 + 0.6 * wobble(y * 3.0, 0.4)
    if -PAD_DEPTH + 0.5 * wobble(y * 2.0, 3.3) <= x <= 1 + 0.4 * wobble(y * 2.0, 1.9) and ay <= pad_edge:
        return GRAVEL
    t = back_toe(y)
    if berm_height(x, y) > 0.25:
        if x > t - 1 and ay < l + 4:
            return EARTH
        if ay < l + 4.5 + 2.0 * wobble(x, 3.9 if y >= 0 else 6.6):
            return GRASS
        return FOREST
    if 1 < x < t + 0.5 and ay <= l + 0.5:
        if x < t - 2 and on_walkway(x, y):
            return DIRT
        if leaf_litter(x, y, l - ay):
            return FOREST
        if x > 3 and abs(y - walkway_y(x)) > WALKWAY_HALF + 0.5 and litter_island(x, y):
            return FOREST
        return GRASS
    return FOREST


def leaf_litter(x, y, from_edge):
    """Tongues of leaf litter reaching a few metres into the lane from the forest side."""
    reach = 1.5 + 5.0 * (0.5 + 0.5 * wobble(x * 2.3, 2.2 if y >= 0 else 5.1))
    return from_edge < reach * (0.5 + 0.5 * wobble(x * 5.1, 0.9))
