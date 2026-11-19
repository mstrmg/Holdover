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
GROUND_AHEAD = 120.0       # ... to X = +120
GROUND_HALF_WIDTH = 90.0   # and Y = -90 .. +90
LANE_HALF_WIDTH = 13.0     # mown lane floor between the side berms
BACK_BERM_TOE = 56.0
BACK_BERM_HEIGHT = 5.0
SIDE_BERM_HEIGHT = 2.5
PAD_DEPTH = 7.0            # gravel pad from X = -7 to +1
FOREST_FLOOR_ROLL = 0.7

TARGET_DISTANCES = [5, 7, 10, 15, 25, 50]   # paper targets (unchanged from the gray range)
TARGET_SPACING = 1.6                        # sideways spacing, metres
DISTANCE_SIGNS = [5, 10, 25, 50]

CLEAR_BEHIND = -12.0
CLEAR_SIDE_EXTRA = 5.0
CLEAR_PAST_BERM_TOE = 10.0
WALKWAY_Y = -10.0
WALKWAY_HALF = 0.7

ACCESS_PATH = [(-7, -7), (-12, -9.5), (-20, -11), (-30, -15), (-42, -19), (-56, -20), (-70, -18), (-95, -23)]

BENCHES = [(-0.3, 1.1), (-0.3, -4.2), (-0.3, 6.4)]
STOOLS = [(-1.1, -4.2), (-1.1, 6.4)]
BRASS_BIN = (-2.4, 3.4)
REST_BENCH = (-10.0, 4.0)
FIRING_LINE_SIGN = (0.0, -LANE_HALF_WIDTH - 0.6)
CEASE_FIRE_SIGN = (-8.5, -9.0)
SHOOTER = (-1.5, 0.0)

FOREST, GRASS, GRAVEL, DIRT, EARTH = range(5)


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
    ay, l = abs(y), LANE_HALF_WIDTH
    side = berm_profile(ay, l, l + 3.5, l + 4.5, l + 8.0, SIDE_BERM_HEIGHT)
    side *= smoothstep(-6, -3, x) * (1 - smoothstep(BACK_BERM_TOE + 10, BACK_BERM_TOE + 14, x))
    back = berm_profile(x, BACK_BERM_TOE, BACK_BERM_TOE + 5, BACK_BERM_TOE + 6.5, BACK_BERM_TOE + 13, BACK_BERM_HEIGHT)
    back *= 1 - smoothstep(l + 6, l + 12, ay)
    return max(side, back)


def _segment_distance(p, a, b):
    abx, aby = b[0] - a[0], b[1] - a[1]
    t = ((p[0] - a[0]) * abx + (p[1] - a[1]) * aby) / max(abx * abx + aby * aby, 1e-4)
    t = min(max(t, 0.0), 1.0)
    return math.hypot(p[0] - (a[0] + abx * t), p[1] - (a[1] + aby * t))


def path_distance(x, y):
    return min(_segment_distance((x, y), ACCESS_PATH[i], ACCESS_PATH[i + 1]) for i in range(len(ACCESS_PATH) - 1))


def cleared_for_trees(x, y, margin=0.0):
    if (CLEAR_BEHIND - margin < x < BACK_BERM_TOE + CLEAR_PAST_BERM_TOE + margin
            and abs(y) < LANE_HALF_WIDTH + CLEAR_SIDE_EXTRA + margin):
        return True
    return path_distance(x, y) < 2.0 + margin


def surface_at(x, y):
    ay = abs(y)
    if x < -PAD_DEPTH and path_distance(x, y) < 1.1:
        return DIRT
    if -PAD_DEPTH <= x <= 1 and ay <= LANE_HALF_WIDTH:
        return GRAVEL
    if berm_height(x, y) > 0.25:
        if x > BACK_BERM_TOE - 1 and ay < LANE_HALF_WIDTH + 4:
            return EARTH
        if ay < LANE_HALF_WIDTH + 4.5:
            return GRASS
        return FOREST
    if 1 < x < BACK_BERM_TOE + 0.5 and ay <= LANE_HALF_WIDTH + 0.5:
        if abs(y - WALKWAY_Y) < WALKWAY_HALF and x < BACK_BERM_TOE - 2:
            return DIRT
        return GRASS
    return FOREST
