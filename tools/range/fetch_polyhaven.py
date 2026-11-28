"""Download the CC0 Poly Haven models and textures the forest range uses (Miles OK'd free downloads, 2026-11-27).

Run with Blender's bundled Python (no extra modules needed):
  "C:\\Program Files\\Blender Foundation\\Blender 5.3\\5.3\\python\\bin\\python.exe" -I tools/range/fetch_polyhaven.py
Files land in art/polyhaven/<id>/ (glTF + 2K jpg textures for models, 2K jpg maps for textures).
Already-downloaded files are skipped. All Poly Haven assets are CC0 (public domain).
"""
import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.environ.get("HOLDOVER_ART", os.path.normpath(os.path.join(HERE, "..", "..", "..", "art")))
if not os.path.isdir(ART):
    ART = r"D:\Claude area\Holdover\art"
OUT = os.path.join(ART, "polyhaven")
RES = "2k"
UA = {"User-Agent": "HoldoverAssetFetch/1.0"}

MODELS = [
    # rocks
    "boulder_01", "rock_07", "rock_09", "rock_moss_set_01", "rock_moss_set_02", "stone_01",
    # dead wood, roots
    "dead_tree_trunk", "dead_tree_trunk_02", "tree_stump_01", "tree_stump_02",
    "root_cluster_01", "root_cluster_02", "single_root", "dry_branches_medium_01",
    # ground plants
    "fern_02", "moss_01", "shrub_01", "shrub_02", "shrub_03", "shrub_04", "nettle_plant", "celandine_01",
    "grass_medium_01", "grass_medium_02",
    # range extras
    "old_military_crate", "Barrel_02", "hatchet",
]
TEXTURES = [
    "forest_leaves_02", "forest_leaves_04", "leaves_forest_ground", "forest_ground_05", "dirt_floor",
    "leafy_grass", "mossy_rock", "gravel", "moss_wood", "bark_brown_02",
]
TEX_MAPS = ["Diffuse", "nor_gl", "Rough", "Displacement", "AO"]


def get_json(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
        return json.loads(r.read().decode("utf-8"))


def download(url, dest):
    if os.path.isfile(dest) and os.path.getsize(dest) > 0:
        return 0
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    tmp = dest + ".part"
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=300) as r, open(tmp, "wb") as f:
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
    os.replace(tmp, dest)
    return os.path.getsize(dest)


def fetch_model(asset_id):
    files = get_json("https://api.polyhaven.com/files/" + asset_id)
    gl = files.get("gltf", {}).get(RES, {}).get("gltf")
    if not gl:
        print("  no %s gltf for %s" % (RES, asset_id))
        return 0
    folder = os.path.join(OUT, asset_id)
    total = download(gl["url"], os.path.join(folder, os.path.basename(gl["url"])))
    for rel, info in gl.get("include", {}).items():
        total += download(info["url"], os.path.join(folder, rel.replace("/", os.sep)))
    # The jpg glTF has no transparency: leaves need the separate alpha mask.
    alpha = files.get("Alpha", {}).get(RES, {})
    entry = alpha.get("jpg") or alpha.get("png")
    if entry:
        total += download(entry["url"], os.path.join(folder, "textures", os.path.basename(entry["url"])))
    return total


def fetch_texture(asset_id):
    files = get_json("https://api.polyhaven.com/files/" + asset_id)
    total = 0
    for m in TEX_MAPS:
        entry = files.get(m, {}).get(RES, {}).get("jpg") or files.get(m, {}).get(RES, {}).get("png")
        if entry:
            total += download(entry["url"], os.path.join(OUT, "textures", asset_id, os.path.basename(entry["url"])))
    return total


def main():
    grand = 0
    for a in MODELS:
        try:
            n = fetch_model(a)
            grand += n
            print("model %-24s %6.1f MB" % (a, n / 1e6))
        except Exception as e:  # keep going; report at the end
            print("model %-24s FAILED %s" % (a, e))
    for t in TEXTURES:
        try:
            n = fetch_texture(t)
            grand += n
            print("texture %-22s %6.1f MB" % (t, n / 1e6))
        except Exception as e:
            print("texture %-22s FAILED %s" % (t, e))
    print("total downloaded this run: %.1f MB -> %s" % (grand / 1e6, OUT))


if __name__ == "__main__":
    sys.exit(main())
