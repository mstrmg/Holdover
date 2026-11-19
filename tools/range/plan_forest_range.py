"""Draws the forest range plan as a 1920x1080 top-down picture.

  python tools/range/plan_forest_range.py [output.png]

Downrange is to the right. Tree positions are illustrative (the game grows its own
from the same rules and a fixed seed).
"""
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import forest_layout as L  # noqa: E402

W, H = 1920, 1080
X0, X1 = -38.0, 84.0
Y0, Y1 = -35.0, 35.0
S = 14.5  # pixels per metre
OX, OY = 40, 60

COLORS = {
    L.FOREST: (52, 46, 30),
    L.GRASS: (92, 120, 52),
    L.GRAVEL: (150, 145, 135),
    L.DIRT: (128, 98, 66),
    L.EARTH: (120, 86, 56),
}


def px(x, y):
    return OX + (x - X0) * S, OY + (y - Y0) * S


def font(size):
    for name in ("DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def main(out):
    img = Image.new("RGB", (W, H), (24, 26, 22))
    d = ImageDraw.Draw(img)

    # Ground with hill shading from the north-west.
    step = 0.25
    y = Y0
    while y < Y1:
        x = X0
        while x < X1:
            c = COLORS[L.surface_at(x + step / 2, y + step / 2)]
            h = L.berm_height(x, y)
            dzdx = (L.berm_height(x + 0.3, y) - L.berm_height(x - 0.3, y)) / 0.6
            dzdy = (L.berm_height(x, y + 0.3) - L.berm_height(x, y - 0.3)) / 0.6
            shade = 1.0 + 0.35 * (dzdx * 0.5 + dzdy * 0.7) + h * 0.02
            c = tuple(max(0, min(255, int(v * shade))) for v in c)
            a, b = px(x, y)
            d.rectangle([a, b, a + step * S + 1, b + step * S + 1], fill=c)
            x += step
        y += step

    # Berm contour lines every metre.
    for level in (1, 2, 3, 4):
        y = Y0
        while y < Y1:
            x = X0
            while x < X1:
                h0 = L.berm_height(x, y)
                if (h0 - level) * (L.berm_height(x + 0.5, y) - level) < 0 or (h0 - level) * (L.berm_height(x, y + 0.5) - level) < 0:
                    a, b = px(x, y)
                    d.point((a, b), fill=(70, 50, 30))
                x += 0.25
            y += 0.25

    # Trees: canopies first (translucent), then trunks.
    rng = random.Random(1907)
    canopy = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    cd = ImageDraw.Draw(canopy)
    trunks = []
    cell = 6.0
    x = -L.GROUND_BEHIND
    while x < L.GROUND_AHEAD:
        y = -L.GROUND_HALF_WIDTH
        while y < L.GROUND_HALF_WIDTH:
            tx, ty = x + rng.random() * cell, y + rng.random() * cell
            keep = rng.random()
            if keep < 0.9 and not L.cleared_for_trees(tx, ty) and X0 - 8 < tx < X1 + 8 and Y0 - 8 < ty < Y1 + 8:
                r = rng.uniform(3.0, 5.5)
                g = rng.choice([(28, 62, 18), (38, 80, 22), (48, 92, 26), (30, 70, 24)])
                a, b = px(tx, ty)
                cd.ellipse([a - r * S, b - r * S, a + r * S, b + r * S], fill=g + (120,))
                trunks.append((a, b, rng.uniform(0.18, 0.35) * S))
            y += cell
        x += cell
    # Saplings and ferns along the edge.
    cell = 4.5
    x = X0
    while x < X1:
        y = Y0
        while y < Y1:
            tx, ty = x + rng.random() * cell, y + rng.random() * cell
            edge = not L.cleared_for_trees(tx, ty) and L.cleared_for_trees(tx, ty, 7.0)
            if rng.random() < (0.8 if edge else 0.4) and not L.cleared_for_trees(tx, ty, -0.5):
                a, b = px(tx, ty)
                r = rng.uniform(0.8, 1.6) * S
                cd.ellipse([a - r, b - r, a + r, b + r], fill=(84, 130, 40, 150))
            y += cell
        x += cell
    canopy = canopy.filter(ImageFilter.GaussianBlur(1.2))
    box = (int(px(X0, Y0)[0]), int(px(X0, Y0)[1]), int(px(X1, Y1)[0]), int(px(X1, Y1)[1]))
    region = canopy.crop(box)
    img.paste(region, box[:2], region)
    d = ImageDraw.Draw(img)
    for a, b, r in trunks:
        d.ellipse([a - r, b - r, a + r, b + r], fill=(30, 22, 14))

    f_small, f_med, f_big = font(15), font(19), font(28)

    # Firing line timber.
    a, b = px(0.25, -L.LANE_HALF_WIDTH)
    c, e = px(0.25, L.LANE_HALF_WIDTH)
    d.line([a, b, c, e], fill=(90, 70, 45), width=4)

    # Benches and stools.
    for bx, by in L.BENCHES:
        a, b = px(bx - 0.375, by - 0.7)
        c, e = px(bx + 0.375, by + 0.7)
        d.rectangle([a, b, c, e], fill=(150, 105, 60), outline=(60, 40, 20))
    for sx, sy in L.STOOLS:
        a, b = px(sx, sy)
        d.ellipse([a - 4, b - 4, a + 4, b + 4], fill=(150, 105, 60))
    a, b = px(*L.BRASS_BIN)
    d.ellipse([a - 5, b - 5, a + 5, b + 5], fill=(40, 70, 40), outline=(10, 20, 10))
    a, b = px(L.REST_BENCH[0] - 0.2, L.REST_BENCH[1] - 0.9)
    c, e = px(L.REST_BENCH[0] + 0.2, L.REST_BENCH[1] + 0.9)
    d.rectangle([a, b, c, e], fill=(150, 105, 60))

    # Shooter and view.
    a, b = px(*L.SHOOTER)
    for edge in (px(30, -9.5), px(30, 9.5)):
        d.line([(a, b), edge], fill=(255, 230, 120), width=1)
    d.ellipse([a - 7, b - 7, a + 7, b + 7], fill=(255, 210, 80), outline=(40, 30, 0))

    # Targets.
    n = len(L.TARGET_DISTANCES)
    for i, dist in enumerate(L.TARGET_DISTANCES):
        ty = (i - (n - 1) * 0.5) * L.TARGET_SPACING
        a, b = px(dist, ty - 0.25)
        c, e = px(dist, ty + 0.25)
        d.line([a, b, c, e], fill=(250, 248, 240), width=5)
        d.text((a + 6, b - 18), "%d m" % dist, fill=(250, 248, 240), font=f_small)

    # Distance signs and range signs.
    for dist in L.DISTANCE_SIGNS:
        a, b = px(dist, -L.LANE_HALF_WIDTH + 1)
        d.rectangle([a - 4, b - 4, a + 4, b + 4], fill=(240, 240, 240), outline=(0, 0, 0))
    for (sx, sy), col in ((L.FIRING_LINE_SIGN, (210, 175, 20)), (L.CEASE_FIRE_SIGN, (190, 20, 20))):
        a, b = px(sx, sy)
        d.rectangle([a - 6, b - 4, a + 6, b + 4], fill=col, outline=(0, 0, 0))

    # Labels.
    def label(x, y, text, f=f_med, fill=(245, 245, 235)):
        a, b = px(x, y)
        bbox = d.textbbox((a, b), text, font=f)
        d.rectangle([bbox[0] - 4, bbox[1] - 2, bbox[2] + 4, bbox[3] + 2], fill=(20, 20, 16))
        d.text((a, b), text, fill=fill, font=f)

    label(59.5, -6, "Backstop berm\n5 m high")
    label(22, -L.LANE_HALF_WIDTH - 5.2, "Side berm, 2.5 m (both sides)")
    label(15, -9.4, "Walkway to the targets", f_small)
    label(-7, 9.5, "Gravel firing pad", f_small)
    label(-6.5, -2.7, "You", f_small, (255, 220, 120))
    label(-35, -27.5, "Path from the parking area", f_small)
    label(-11.5, 6.0, "Rest bench", f_small)
    label(-24.0, -6.2, "CEASE FIRE sign", f_small)
    label(2.0, -L.LANE_HALF_WIDTH - 1.6, "FIRING LINE sign", f_small)
    label(66, 22, "Dense summer hardwood forest\ntrees about 6 m apart, 15-28 m tall,\nsaplings and ferns thick at the edge", f_small)
    label(25, 9.3, "Mown lane, 26 m wide", f_small)

    # Title, scale bar, north... (downrange) arrow.
    d.text((OX, 14), "Holdover: forest range plan (top view, downrange is to the right)", fill=(240, 240, 230), font=f_big)
    a, b = px(-33, 31)
    d.rectangle([a, b, a + 10 * S, b + 6], fill=(240, 240, 230))
    d.text((a, b + 10), "10 m", fill=(240, 240, 230), font=f_small)

    img.save(out)
    print("wrote", out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "forest_range_plan.png")
