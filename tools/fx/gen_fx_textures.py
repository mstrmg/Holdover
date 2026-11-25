"""Generates the muzzle flash and smoke textures (no third-party images: everything is procedural noise).

Run with any Python 3 that has numpy (Blender's bundled Python works):
  "C:\\Program Files\\Blender Foundation\\Blender 5.3\\5.3\\python\\bin\\python.exe" -I tools/fx/gen_fx_textures.py [out_dir]
Default output: <repo>/../art/fx/textures/ (on Miles's PC: D:\\Claude area\\Holdover\\art\\fx\\textures).

T_MuzzleFlash.png  2048 x 2048, a 4 x 4 atlas of 512 px frames, colour already in (sRGB), black = nothing (additive):
    row 0  hot cores           (round, ragged, with short rays)
    row 1  flame plumes        (muzzle at the left edge, the tongue reaching right)
    row 2  bursts seen end-on  (ragged petals of gas spreading round the bore)
    row 3  fireballs           (billowing ball of burning gas, cooler and redder)
T_MuzzleSmoke.png  2048 x 2048, 4 x 4 atlas of soft smoke puffs; density in every channel.
"""
import math
import os
import struct
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(os.path.join(REPO, "..", "art", "fx", "textures"))
F = 512          # frame size
ATLAS = 4        # frames per row/column
SEED = 1911

rng = np.random.default_rng(SEED)


# ---------------------------------------------------------------- noise
def lattice_noise(x, y, period_x, period_y, grid):
    """Smooth value noise sampled at (x, y) on a random lattice that wraps with the given periods."""
    x0 = np.floor(x).astype(np.int64)
    y0 = np.floor(y).astype(np.int64)
    fx = x - x0
    fy = y - y0
    sx = fx * fx * fx * (fx * (fx * 6 - 15) + 10)
    sy = fy * fy * fy * (fy * (fy * 6 - 15) + 10)
    gx, gy = grid.shape
    def g(ix, iy):
        return grid[np.mod(ix, period_x) % gx, np.mod(iy, period_y) % gy]
    a = g(x0, y0)
    b = g(x0 + 1, y0)
    c = g(x0, y0 + 1)
    d = g(x0 + 1, y0 + 1)
    return (a * (1 - sx) + b * sx) * (1 - sy) + (c * (1 - sx) + d * sx) * sy


def fbm(x, y, octaves=5, freq=4.0, gain=0.55, wrap_x=None, seed=None):
    """Fractal noise in roughly [0, 1]. wrap_x: make it repeat along x with that period (for angles)."""
    r = np.random.default_rng(seed if seed is not None else rng.integers(1 << 30))
    total = np.zeros_like(x, dtype=np.float64)
    amp = 1.0
    norm = 0.0
    f = freq
    for _ in range(octaves):
        size = int(max(4, math.ceil(f)) + 2)
        grid = r.random((size * 2 + 8, size * 2 + 8))
        px = int(round(f)) if wrap_x else 1 << 30
        total += amp * lattice_noise(x * f, y * f, max(px, 1), 1 << 30, grid)
        norm += amp
        amp *= gain
        f *= 2.0
    return total / norm


def frame_coords():
    v, u = np.mgrid[0:F, 0:F].astype(np.float64)
    u = (u + 0.5) / F * 2 - 1      # -1 .. 1, left to right
    v = 1 - (v + 0.5) / F * 2      # -1 .. 1, bottom to top
    return u, v


def edge_window(u, v, margin=0.06):
    """Fades to zero at the frame border so frames never bleed into each other."""
    wu = np.clip((1 - np.abs(u)) / margin, 0, 1)
    wv = np.clip((1 - np.abs(v)) / margin, 0, 1)
    return (wu * wv) ** 2


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


# ---------------------------------------------------------------- colour of burning gas by temperature
RAMP = [  # temperature 0..1 -> linear RGB
    (0.00, (0.00, 0.00, 0.00)),
    (0.12, (0.45, 0.07, 0.01)),
    (0.30, (0.95, 0.28, 0.04)),
    (0.50, (1.00, 0.52, 0.12)),
    (0.70, (1.00, 0.76, 0.34)),
    (0.86, (1.00, 0.92, 0.66)),
    (1.00, (1.00, 0.99, 0.94)),
]


def ramp(t):
    t = np.clip(t, 0, 1)
    out = np.zeros(t.shape + (3,))
    for (t0, c0), (t1, c1) in zip(RAMP[:-1], RAMP[1:]):
        m = (t >= t0) & (t <= t1)
        a = ((t - t0) / (t1 - t0))[m][:, None]
        out[m] = np.array(c0) * (1 - a) + np.array(c1) * a
    return out


def flame_rgb(intensity, temperature):
    """Colour times brightness; brightness above 1 is clipped by 8-bit storage, so keep peaks near 1."""
    rgb = ramp(temperature) * np.clip(intensity, 0, 1.0)[..., None]
    return rgb


# ---------------------------------------------------------------- frames
def ragged_lobes(th, r, count, width, length, seed_k):
    """Several uneven tongues of gas round a centre (angles th, radius r), each with a torn edge."""
    out = np.zeros_like(r)
    for _ in range(count):
        a0 = rng.random() * 2 * math.pi
        w = rng.uniform(*width)
        ln = rng.uniform(*length)
        d = np.angle(np.exp(1j * (th - a0)))
        out = np.maximum(out, np.exp(-(d / w) ** 2) * ln)
    return out


def core_frame(k):
    u, v = frame_coords()
    r = np.sqrt(u * u + v * v)
    th = np.arctan2(v, u)
    ang = (th / (2 * math.pi)) % 1.0
    lobes = ragged_lobes(th, r, rng.integers(3, 7), (0.25, 0.55), (0.25, 0.6), k)
    rag = fbm(ang, k * 1.7 + 0 * r, octaves=5, freq=7, wrap_x=True)
    edge = (0.22 + lobes * 0.55) * (0.7 + 0.6 * rag)
    body = np.exp(-(r / edge) ** 2 * 2.6)
    turb = fbm(u * 1.4 + 3 * k, v * 1.4, octaves=6, freq=5)
    turb = smoothstep(0.2, 0.8, turb)
    hot = np.exp(-(r / (0.1 + 0.03 * k)) ** 2)
    inten = body * (0.35 + 0.65 * turb) + 0.85 * hot
    inten = inten / max(inten.max(), 1e-6)
    temp = np.clip(0.12 + 0.88 * inten ** 0.85, 0, 1)
    return flame_rgb(inten ** 1.15, temp) * edge_window(u, v)[..., None]


def plume_frame(k):
    u, v = frame_coords()
    x = (u + 1) * 0.5                     # 0 at the muzzle (left edge), 1 at the right edge
    reach = rng.uniform(0.6, 0.97)        # how far this tongue gets
    # strong domain warp: the jet tears into flickering tongues
    n1 = fbm(x * 1.2 + k * 3, v * 0.7, octaves=5, freq=3)
    n2 = fbm(x * 1.2 + 5, v * 0.7 + k * 3, octaves=5, freq=3)
    wy = v + 0.55 * (n1 - 0.5) * (0.2 + x)
    wx = x + 0.12 * (n2 - 0.5)
    width = 0.06 + 0.5 * np.clip(wx, 0, 1) ** 0.75 * np.clip(1 - wx / reach, 0, 1) ** 0.6
    across = np.exp(-(np.abs(wy) / np.maximum(width, 1e-3)) ** 2 * 2.2)
    along = np.clip(1 - wx / reach, 0, 1) ** 1.6 * smoothstep(0.0, 0.04, x)
    tear = fbm(wx * 2.2, wy * 1.4 + 11 * k, octaves=6, freq=5)
    tear = smoothstep(0.3, 0.75, tear)
    inten = across * along * (0.15 + 0.85 * tear)
    inten += 0.6 * np.exp(-((x - 0.03) / 0.08) ** 2 - (v / 0.09) ** 2)   # hot throat at the muzzle
    inten = inten / max(inten.max(), 1e-6)
    temp = np.clip(0.08 + 0.92 * inten ** 0.9 * (1 - 0.35 * x), 0, 1)
    return flame_rgb(inten, temp) * edge_window(u, v)[..., None]


def star_frame(k):
    u, v = frame_coords()
    r = np.sqrt(u * u + v * v)
    th = np.arctan2(v, u)
    n = [4, 5, 6, 7][k]
    phase = rng.random() * 2 * math.pi
    petals = np.zeros_like(r)
    for i in range(n):
        a0 = phase + 2 * math.pi * i / n + rng.uniform(-0.3, 0.3)
        w = rng.uniform(0.2, 0.42)
        ln = rng.uniform(0.35, 1.0)
        d = np.angle(np.exp(1j * (th - a0)))
        petals = np.maximum(petals, np.exp(-(d / w) ** 2) * ln)
    ang = (th / (2 * math.pi)) % 1.0
    rag = fbm(ang, k * 2.3 + 0 * r, octaves=5, freq=9, wrap_x=True)
    edge = 0.14 + petals * 0.75 * (0.6 + 0.7 * rag)
    inten = np.exp(-(r / edge) ** 2 * 2.2)
    detail = smoothstep(0.25, 0.8, fbm(u * 1.5 + k, v * 1.5, octaves=6, freq=6))
    inten *= 0.25 + 0.75 * detail
    inten += 0.7 * np.exp(-(r / 0.09) ** 2)
    inten = inten / max(inten.max(), 1e-6)
    temp = np.clip(0.1 + 0.9 * inten ** 0.85, 0, 1)
    return flame_rgb(inten, temp) * edge_window(u, v)[..., None]


def fireball_frame(k):
    u, v = frame_coords()
    # warped coordinates make the ball billow
    wu = u + 0.4 * (fbm(u + k * 5, v, octaves=4, freq=2.5) - 0.5)
    wv = v + 0.4 * (fbm(u, v + k * 5 + 3, octaves=4, freq=2.5) - 0.5)
    r = np.sqrt(wu * wu + wv * wv)
    th = np.arctan2(v, u)
    ang = (th / (2 * math.pi)) % 1.0
    edge = 0.62 * (0.75 + 0.5 * fbm(ang, k * 2.0 + 0 * r, octaves=4, freq=5, wrap_x=True))
    billow = fbm(wu * 1.2 + 9 * k, wv * 1.2, octaves=6, freq=3.5)
    dens = np.clip(billow * 1.6 - (r / edge) ** 2 * 1.1 + 0.05, 0, 1)
    detail = smoothstep(0.2, 0.85, fbm(wu * 2 + k, wv * 2, octaves=6, freq=6))
    inten = smoothstep(0.0, 0.9, dens) ** 1.6 * (0.3 + 0.7 * detail)
    inten = inten / max(inten.max(), 1e-6)
    temp = np.clip(0.1 + 0.6 * inten ** 0.9, 0, 0.7)
    return flame_rgb(inten, temp) * edge_window(u, v)[..., None]

def smoke_frame(k):
    u, v = frame_coords()
    wu = u + 0.45 * (fbm(u + k, v, octaves=4, freq=2) - 0.5)
    wv = v + 0.45 * (fbm(u, v + k + 17, octaves=4, freq=2) - 0.5)
    r = np.sqrt(wu * wu + wv * wv)
    cloud = fbm(wu * 1.1 + k * 13, wv * 1.1, octaves=6, freq=2.5, gain=0.45)
    wisps = fbm(wu * 2.5, wv * 0.8 + k * 7, octaves=4, freq=3, gain=0.45)
    dens = np.clip(cloud * 1.3 + 0.2 * wisps - r ** 2 * 1.1 - 0.2, 0, 1)
    dens = smoothstep(0.0, 0.75, dens)
    dens *= edge_window(u, v, 0.12)
    return dens / max(dens.max(), 1e-6)


# ---------------------------------------------------------------- PNG writer (no PIL needed)
def write_png(path, rgba):
    h, w, _ = rgba.shape
    raw = b"".join(b"\x00" + rgba[y].tobytes() for y in range(h))
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def to_srgb8(lin):
    lin = np.clip(lin, 0, 1)
    s = np.where(lin <= 0.0031308, lin * 12.92, 1.055 * np.power(lin, 1 / 2.4) - 0.055)
    return np.round(s * 255).astype(np.uint8)


def main():
    os.makedirs(OUT, exist_ok=True)
    makers = [core_frame, plume_frame, star_frame, fireball_frame]
    flash = np.zeros((F * ATLAS, F * ATLAS, 3))
    for row, make in enumerate(makers):
        for col in range(ATLAS):
            flash[row * F:(row + 1) * F, col * F:(col + 1) * F] = make(col)
            print("flash frame", row * ATLAS + col)
    rgba = np.zeros((F * ATLAS, F * ATLAS, 4), dtype=np.uint8)
    rgba[..., :3] = to_srgb8(flash)
    rgba[..., 3] = to_srgb8(flash.max(axis=2))
    write_png(os.path.join(OUT, "T_MuzzleFlash.png"), rgba)

    smoke = np.zeros((F * ATLAS, F * ATLAS))
    for i in range(ATLAS * ATLAS):
        row, col = divmod(i, ATLAS)
        smoke[row * F:(row + 1) * F, col * F:(col + 1) * F] = smoke_frame(i)
        print("smoke frame", i)
    s8 = np.round(np.clip(smoke, 0, 1) * 255).astype(np.uint8)
    rgba = np.zeros((F * ATLAS, F * ATLAS, 4), dtype=np.uint8)
    rgba[..., 0] = s8
    rgba[..., 1] = s8
    rgba[..., 2] = s8
    rgba[..., 3] = s8
    write_png(os.path.join(OUT, "T_MuzzleSmoke.png"), rgba)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
