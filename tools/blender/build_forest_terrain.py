"""Build the forest range ground in Blender and export it for Unreal.

Run inside Blender (any open file; it works in its own collection):
    exec(open(r"D:\\Claude area\\Holdover\\Holdover\\tools\\blender\\build_forest_terrain.py").read())

Reads the layout numbers from tools/range/forest_layout.py (the same numbers the game uses),
so berms, pad, walkway and path line up with where the game puts benches, targets and trees.

Output:
    art/range/forest/forest_terrain.blend
    art/range/forest/SM_ForestTerrain.fbx   (import with tools/unreal/import_range.py)

Blender axes: X downrange, Y to the shooter's LEFT (FBX import into Unreal flips Y, so it
becomes Unreal's +Y = right), Z up, 1 Blender metre = 100 Unreal units.
Material slots are named ForestFloor, LaneGrass, Gravel, DirtPath, BermEarth; the game swaps in
MI_<slot name> from /Game/Range/Forest/Ground when those materials exist.
"""
import math
import os

import bpy
from mathutils import Vector, noise

REPO = os.environ.get("HOLDOVER_REPO", r"D:\Claude area\Holdover\Holdover")
ART = os.environ.get("HOLDOVER_ART", r"D:\Claude area\Holdover\art")
LAYOUT = os.path.join(REPO, "tools", "range", "forest_layout.py")
OUT_DIR = os.path.join(ART, "range", "forest")

if "berm_height" not in globals():
    exec(compile(open(LAYOUT).read(), LAYOUT, "exec"), globals())

CELL = 0.6  # metres between ground vertices

SURFACES = [  # slot name, viewport/base colour (sRGB-ish, used until Megascans materials exist)
    ("ForestFloor", (0.20, 0.16, 0.10)),
    ("LaneGrass", (0.27, 0.36, 0.13)),
    ("Gravel", (0.48, 0.46, 0.42)),
    ("DirtPath", (0.36, 0.27, 0.17)),
    ("BermEarth", (0.38, 0.27, 0.17)),
]


def n3(x, y, z, scale):
    return noise.noise(Vector((x / scale, y / scale, z)))


def clearing_mask(x, y):
    """0 inside the cleared range, rising to 1 eight metres into the forest (same as the game)."""
    ahead = BACK_BERM_TOE + CLEAR_PAST_BERM_TOE
    side = LANE_HALF_WIDTH + CLEAR_SIDE_EXTRA
    dx = max(CLEAR_BEHIND - x, 0.0, x - ahead)
    dy = max(abs(y) - side, 0.0)
    m = smoothstep(0.0, 8.0, math.hypot(dx, dy))
    return m * (0.35 + 0.65 * smoothstep(1.5, 5.0, path_distance(x, y)))


def ground_height(x, y):
    """Height in metres at layout point (x downrange, y to the shooter's right)."""
    berm = berm_height(x, y)
    mask = clearing_mask(x, y)

    # Rolling forest floor, the same scales as the game, plus finer lumps (roots, old stumps, hollows).
    roll = (0.80 * n3(x, y, 0.5, 30.0) + 0.25 * n3(x + 17.3, y - 4.1, 1.5, 9.0) + 0.05 * n3(x, y, 2.5, 2.5))
    lumps = 0.06 * n3(x, y, 4.5, 1.1) + 0.03 * n3(x, y, 6.5, 0.45)
    h = berm + roll * FOREST_FLOOR_ROLL * mask + lumps * mask

    # Berms: lumpy, settled earth rather than perfect ramps.
    if berm > 0.02:
        w = smoothstep(0.02, 0.8, berm)
        h += w * (0.14 * n3(x, y, 8.5, 2.4) + 0.05 * n3(x, y, 9.5, 0.7))
        # Backstop face: shallow pockets dug out by bullets around target height.
        if x > BACK_BERM_TOE - 1 and abs(y) < LANE_HALF_WIDTH + 2 and 0.3 < berm < 3.0:
            h -= 0.07 * max(0.0, n3(x, y, 10.5, 0.35)) * w

    surf = surface_at(x, y)
    if surf == GRAVEL:
        h += 0.015 + 0.006 * n3(x, y, 12.5, 0.3)  # pad sits a touch proud, crunchy surface
    elif surf == DIRT and x > 0:
        # Walkway: trodden a few centimetres into the grass, two faint ruts.
        d = abs(y - WALKWAY_Y)
        h -= 0.035 * (1.0 - smoothstep(0.0, WALKWAY_HALF, d)) + 0.01 * math.cos(d * 7.0)
    elif surf == DIRT:
        h -= 0.04 * (1.0 - smoothstep(0.0, 1.1, path_distance(x, y)))
    elif surf == GRASS and berm < 0.02:
        # Lane: almost flat, but not a table top. Dead flat near targets so the stands sit right.
        near_target = min(abs(x - d) for d in TARGET_DISTANCES) < 1.5 and abs(y) < 6
        if not near_target:
            h += 0.012 * n3(x, y, 14.5, 3.0)
    return h


def material(name, color):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1.0)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.95
    return mat


def build():
    nx = int(round((GROUND_AHEAD + GROUND_BEHIND) / CELL))
    ny = int(round(2 * GROUND_HALF_WIDTH / CELL))
    verts = []
    for j in range(ny + 1):
        y = -GROUND_HALF_WIDTH + j * CELL
        for i in range(nx + 1):
            x = -GROUND_BEHIND + i * CELL
            # Blender Y is the shooter's left, so it is minus the layout's y.
            verts.append((x, -y, ground_height(x, y)))
    faces, mats = [], []
    for j in range(ny):
        for i in range(nx):
            a = j * (nx + 1) + i
            # Rows run toward Blender -Y (layout +y), so this order is counter-clockwise from above (normal up).
            faces.append((a, a + nx + 1, a + nx + 2, a + 1))
            x = -GROUND_BEHIND + (i + 0.5) * CELL
            y = -GROUND_HALF_WIDTH + (j + 0.5) * CELL
            mats.append(surface_at(x, y))

    old = bpy.data.objects.get("SM_ForestTerrain")
    if old:
        me_old = old.data
        bpy.data.objects.remove(old, do_unlink=True)
        if me_old and me_old.users == 0:
            bpy.data.meshes.remove(me_old)

    me = bpy.data.meshes.new("SM_ForestTerrain")
    me.from_pydata(verts, [], faces)
    for name, color in SURFACES:
        me.materials.append(material(name, color))
    me.polygons.foreach_set("material_index", mats)
    me.polygons.foreach_set("use_smooth", [True] * len(faces))

    # World-space UVs: ground textures repeat every 2 m.
    uv = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv = (co.x / 2.0, co.y / 2.0)
    me.update()

    coll = bpy.data.collections.get("ForestTerrain")
    if not coll:
        coll = bpy.data.collections.new("ForestTerrain")
        bpy.context.scene.collection.children.link(coll)
    ob = bpy.data.objects.new("SM_ForestTerrain", me)
    coll.objects.link(ob)
    return ob


def export(ob):
    os.makedirs(OUT_DIR, exist_ok=True)
    vl = bpy.context.view_layer
    for o in vl.objects:
        o.select_set(False)
    ob.select_set(True)
    vl.objects.active = ob
    path = os.path.join(OUT_DIR, "SM_ForestTerrain.fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_scale_options="FBX_SCALE_ALL",
                             axis_forward="X", axis_up="Z", mesh_smooth_type="FACE", use_tspace=True, path_mode="STRIP")
    return path


ob = build()
fbx = export(ob)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "forest_terrain.blend"), copy=True)
result = {"fbx": fbx, "verts": len(ob.data.vertices), "faces": len(ob.data.polygons),
          "zmin": min(v.co.z for v in ob.data.vertices), "zmax": max(v.co.z for v in ob.data.vertices)}
