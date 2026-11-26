"""Synthesises every firing sound in Holdover from scratch (no recordings, so no licences to track).

Run with any Python 3 that has numpy (Blender's bundled Python works):
  "C:\\Program Files\\Blender Foundation\\Blender 5.3\\5.3\\python\\bin\\python.exe" -I tools/audio/gen_gun_audio.py [out_dir]
Default output: <repo>/../art/audio/ (on Miles's PC: D:\\Claude area\\Holdover\\art\\audio). 48 kHz, 16-bit mono WAV.

Guns/<GunId>/SW_<GunId>_Shot_NN.wav  the shot at the shooter's ears: a blast pressure wave (Friedlander shape), the
                                     crack of the gas and the supersonic bullet, a low thump, the first bounce off the
                                     ground, all driven hard like an eardrum or a microphone overloads.
Guns/<GunId>/SW_<GunId>_Tail_NN.wav  what comes back from the range: the trees scatter the blast into a rolling,
                                     darkening tail, with a distinct slap off the berm and the far hill.
Guns/<GunId>/SW_<GunId>_Mech_NN.wav  the slide or bolt cycling: unlock clack, slam at the rear, spring, slam home.
Brass/SW_Brass_<Pistol|Rifle>_<Hard|Soft|Wood|Metal>_NN.wav  a fired case landing (ringing thin brass tube).
Brass/SW_Round_<surface>_NN.wav, Brass/SW_Mag_<surface>_NN.wav  a live round, a dropped magazine.
"""
import math
import os
import sys
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(os.path.join(REPO, "..", "art", "audio"))
SR = 48000
rng = np.random.default_rng(1873)


# ---------------------------------------------------------------- basic tools
def t_axis(seconds):
    return np.arange(int(seconds * SR)) / SR


def fft_filter(x, gain_of_freq):
    """Filters by shaping the spectrum (zero-phase); gain_of_freq maps an array of Hz to gains."""
    n = len(x)
    size = 1 << int(math.ceil(math.log2(n * 2)))
    X = np.fft.rfft(x, size)
    f = np.fft.rfftfreq(size, 1 / SR)
    y = np.fft.irfft(X * gain_of_freq(f), size)[:n]
    return y


def lowpass(x, fc, order=2):
    return fft_filter(x, lambda f: 1 / np.sqrt(1 + (f / fc) ** (2 * order)))


def highpass(x, fc, order=2):
    return fft_filter(x, lambda f: 1 / np.sqrt(1 + (fc / np.maximum(f, 1e-3)) ** (2 * order)))


def bandpass(x, fc, octaves):
    return fft_filter(x, lambda f: np.exp(-(np.log2(np.maximum(f, 1) / fc) / octaves) ** 2))


def shelf(x, fc, gain_db_above):
    g = 10 ** (gain_db_above / 20)
    return fft_filter(x, lambda f: 1 + (g - 1) / (1 + (fc / np.maximum(f, 1e-3)) ** 2))


def noise(n):
    return rng.standard_normal(n)


def pad(x, seconds):
    out = np.zeros(int(seconds * SR))
    out[:min(len(x), len(out))] = x[:len(out)]
    return out


def add_at(dst, src, at_seconds, gain=1.0):
    i = int(at_seconds * SR)
    if i >= len(dst):
        return
    n = min(len(src), len(dst) - i)
    dst[i:i + n] += src[:n] * gain


def soft_clip(x, drive):
    return np.tanh(x * drive) / np.tanh(drive)


def fade_out(x, seconds):
    n = min(len(x), int(seconds * SR))
    if n > 0:
        x[-n:] *= np.linspace(1, 0, n) ** 2
    return x


def normalize(x, peak_db=-1.0):
    m = np.max(np.abs(x))
    return x if m < 1e-9 else x / m * 10 ** (peak_db / 20)


def write_wav(path, x):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    pcm = np.round(np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def modal(freqs, taus, amps, seconds, jitter=0.0):
    """Sum of decaying sine partials (a struck metal or wooden object)."""
    t = t_axis(seconds)
    y = np.zeros_like(t)
    for f, tau, a in zip(freqs, taus, amps):
        f2 = f * (1 + rng.uniform(-jitter, jitter))
        y += a * np.sin(2 * math.pi * f2 * t + rng.uniform(0, 2 * math.pi)) * np.exp(-t / tau)
    return y


def click(seconds=0.002, bright=6000):
    """A very short broadband tick that excites resonances."""
    n = int(seconds * SR)
    c = noise(n) * np.exp(-np.arange(n) / (n / 5))
    return lowpass(c, bright, 1)


# ---------------------------------------------------------------- guns
GUNS = {
    #               blast positive phase, thump Hz, thump decay, crack centre, supersonic, harshness (brake),
    #               level, tail strength, action kind
    "Pistol_M9":   dict(tplus=1.3e-3, boom=105, boom_tau=0.055, crack=2600, sonic=0.35, harsh=0.0, drive=3.0, tail=0.75, action="pistol", case="Pistol"),
    "Pistol_1911": dict(tplus=1.7e-3, boom=88, boom_tau=0.065, crack=1900, sonic=0.0, harsh=0.0, drive=2.6, tail=0.8, action="pistol_heavy", case="Pistol"),
    "Rifle_AK":    dict(tplus=2.2e-3, boom=72, boom_tau=0.09, crack=2300, sonic=1.0, harsh=0.6, drive=4.0, tail=1.0, action="ak", case="Rifle"),
    "Rifle_FAL":   dict(tplus=2.8e-3, boom=62, boom_tau=0.11, crack=2000, sonic=1.0, harsh=0.3, drive=4.5, tail=1.15, action="fal", case="Rifle"),
    "Carbine_AR":  dict(tplus=1.8e-3, boom=84, boom_tau=0.075, crack=3100, sonic=1.0, harsh=0.2, drive=4.0, tail=0.9, action="ar", case="Rifle"),
}


def shot(g):
    """The shot as heard by the shooter (dry: no range echo)."""
    dur = 0.6
    t = t_axis(dur)
    y = np.zeros_like(t)
    tp = g["tplus"] * rng.uniform(0.9, 1.1)

    # 1. Blast wave: instant rise, Friedlander decay with a negative phase.
    b = 1.6
    fr = (1 - t / tp) * np.exp(-b * t / tp)
    fr[t > tp * 6] *= np.exp(-(t[t > tp * 6] - tp * 6) / (tp * 2))
    y += 1.0 * fr

    # 2. Crack of the expanding gas: a bright noise burst that dies in a few tens of ms.
    n = noise(len(t)) * (np.exp(-t / rng.uniform(0.006, 0.01)) + 0.25 * np.exp(-t / 0.04))
    y += 0.55 * bandpass(n, g["crack"] * rng.uniform(0.9, 1.1), 1.6)
    # brakes throw the gas sideways: harsher, more upper mids
    if g["harsh"] > 0:
        y += 0.35 * g["harsh"] * bandpass(noise(len(t)) * np.exp(-t / 0.018), 4200, 1.0)

    # 3. Supersonic bullet: a hard N-wave snap right at the start.
    if g["sonic"] > 0:
        nw = np.zeros_like(t)
        L = int(0.00035 * SR)
        nw[:L] = np.linspace(1, -1, L)
        y += 0.6 * g["sonic"] * highpass(nw, 1500)

    # 4. The thump in the chest: one heavy low pressure swing (not a ringing tone) and a short noisy rumble.
    f0 = g["boom"] * rng.uniform(0.92, 1.08)
    cyc = t < 1.5 / f0
    thump = np.zeros_like(t)
    thump[cyc] = np.sin(2 * math.pi * f0 * t[cyc]) * np.hanning(cyc.sum() * 2)[cyc.sum():] if cyc.sum() > 1 else 0
    y += 0.6 * thump
    body = noise(len(t)) * np.exp(-t / (g["boom_tau"] * 0.45))
    y += 0.9 * lowpass(body, 600, 2) + 0.35 * bandpass(body, 1200, 1.5)
    # 5. First reflection off the ground in front (a few ms later, darker).
    ref = lowpass(y.copy(), 3500)
    add_at(y, ref, rng.uniform(0.0045, 0.007), 0.45)

    # Loud enough to overload the ear: drive it, then tame the extreme top.
    y = soft_clip(y / np.max(np.abs(y)), g["drive"])
    y = shelf(y, 7000, -6)
    y = highpass(y, 35, 2)
    y = fade_out(y, 0.25)
    return normalize(y, -0.5)


def tail(g):
    """The range answering back: scattering off a forest is a dense, rolling, darkening wash."""
    dur = 3.2
    t = t_axis(dur)
    y = np.zeros_like(t)
    src = shot(g)[: int(0.12 * SR)]
    src = lowpass(src, 5000)

    # Dense scattering from tree trunks: thousands of tiny reflections, sparse at first, then a smooth wash.
    ir = np.zeros_like(t)
    density = 60 + 2500 * (t / 0.5) ** 1.5
    hits = rng.random(len(t)) < np.clip(density / SR, 0, 1)
    ir[hits] = rng.standard_normal(hits.sum())
    pre = rng.uniform(0.025, 0.045)
    ir *= (t > pre) * np.exp(-np.maximum(t - pre, 0) / (0.55 * g["tail"]))
    # high frequencies die faster in a forest (leaves and air soak them up)
    low = lowpass(ir, 700)
    mid = bandpass(ir, 1800, 1.2) * np.exp(-t / 0.35)
    high = highpass(ir, 4000) * np.exp(-t / 0.12)
    ir = low * 1.2 + mid * 0.7 + high * 0.25

    # Discrete slaps: the backstop berm (about 50 m), side tree lines, the far hill.
    for delay, gain, fc in [(rng.uniform(0.26, 0.32), 0.55, 2500), (rng.uniform(0.11, 0.17), 0.35, 3500),
                            (rng.uniform(0.6, 0.85), 0.3, 900), (rng.uniform(1.1, 1.5), 0.15, 500)]:
        imp = np.zeros_like(t)
        imp[int(delay * SR)] = 1.0
        ir += lowpass(imp, fc) * gain * 40

    size = 1 << int(math.ceil(math.log2(len(t) + len(src))))
    y = np.fft.irfft(np.fft.rfft(ir, size) * np.fft.rfft(src, size), size)[: len(t)]

    # Rolling low thunder that swells and fades (the blast travelling down the valley).
    roll = lowpass(noise(len(t)), 180) * np.exp(-((t - 0.7) / 0.6) ** 2)
    y = y / max(np.max(np.abs(y)), 1e-9) + 0.25 * roll / max(np.max(np.abs(roll)), 1e-9)
    y = highpass(y, 40)
    y = fade_out(y, 0.8)
    # The tail sits well under the dry shot.
    return normalize(y, -9.0 + 4 * (g["tail"] - 1))


def metal_hit(seconds, base, spread, tau, n=6, bright=6000, level=1.0):
    freqs = [base * (1 + spread * i + rng.uniform(-0.1, 0.1) * spread) for i in range(n)]
    taus = [tau * rng.uniform(0.5, 1.2) / (1 + 0.3 * i) for i in range(n)]
    amps = [rng.uniform(0.4, 1.0) / (1 + 0.4 * i) for i in range(n)]
    y = modal(freqs, taus, amps, seconds)
    c = click(0.003, bright)
    y[: len(c)] += c * 1.5
    return y * level


def mech(g):
    """The action cycling under recoil, as heard over the shot (it mostly adds bite and a slam home)."""
    dur = 0.6
    y = np.zeros(int(dur * SR))
    kind = g["action"]
    if kind.startswith("pistol"):
        heavy = kind == "pistol_heavy"
        back = 0.032 if not heavy else 0.036
        home = back + (0.05 if not heavy else 0.055)
        add_at(y, metal_hit(0.15, 2300, 0.55, 0.03, bright=7000), back * rng.uniform(0.95, 1.05), 0.8)
        add_at(y, metal_hit(0.2, 1500 if not heavy else 1250, 0.5, 0.045, bright=5000), home * rng.uniform(0.97, 1.03), 1.0)
        add_at(y, lowpass(noise(int(0.02 * SR)) * np.exp(-np.arange(int(0.02 * SR)) / 200), 900), home, 0.6)
    else:
        cycle = {"ak": 0.1, "fal": 0.09, "ar": 0.08}[kind]
        back = cycle * 0.5
        home = back + {"ak": 0.06, "fal": 0.06, "ar": 0.055}[kind]
        add_at(y, metal_hit(0.2, 1700, 0.45, 0.04, bright=6000), 0.004, 0.5)          # unlock / carrier starts
        add_at(y, metal_hit(0.25, 1100, 0.5, 0.05, bright=4500), back, 0.9)           # carrier hits the rear
        add_at(y, metal_hit(0.3, 900, 0.5, 0.06, bright=4000), home, 1.0)             # slams home
        if kind == "ar":
            # the buffer spring in the stock tube: the famous "sproing"
            t = t_axis(0.35)
            spring = np.sin(2 * math.pi * rng.uniform(1900, 2300) * t) * np.exp(-t / 0.09)
            spring += 0.5 * np.sin(2 * math.pi * rng.uniform(3700, 4300) * t) * np.exp(-t / 0.05)
            add_at(y, spring * (1 - np.exp(-t / 0.004)), back, 0.35)
        if kind == "ak":
            # long recoil spring and loose carrier: a lower rattle
            t = t_axis(0.25)
            add_at(y, np.sin(2 * math.pi * rng.uniform(380, 460) * t) * np.exp(-t / 0.07), back, 0.3)
    y = highpass(y, 200)
    y = fade_out(y, 0.15)
    return normalize(y, -6.0)


# ---------------------------------------------------------------- brass, rounds and magazines landing
def brass(size, surface):
    """A fired case landing: thin brass tube ringing. Small pistol cases ring higher than long rifle ones."""
    dur = 0.6
    if size == "Pistol":
        base, spread = rng.uniform(5200, 6800), 0.42
    else:
        base, spread = rng.uniform(3000, 4200), 0.37
    ring = {"Hard": 0.09, "Metal": 0.18, "Wood": 0.04, "Soft": 0.012}[surface]
    bright = {"Hard": 9000, "Metal": 11000, "Wood": 5000, "Soft": 2500}[surface]
    y = metal_hit(dur, base, spread, ring, n=7, bright=bright)
    if surface == "Wood":
        add_at(y, modal([rng.uniform(450, 700), rng.uniform(1000, 1400)], [0.02, 0.012], [1.0, 0.5], 0.1), 0, 0.8)
    if surface == "Soft":
        thud = lowpass(noise(int(0.04 * SR)) * np.exp(-np.arange(int(0.04 * SR)) / 300), 1200)
        y = lowpass(y, 3000) * 0.4
        add_at(y, thud, 0, 1.0)
    # sometimes a quick second bounce
    if surface != "Soft" and rng.random() < 0.5:
        add_at(y, metal_hit(0.3, base * rng.uniform(0.97, 1.03), spread, ring * 0.6, n=5, bright=bright), rng.uniform(0.04, 0.11), rng.uniform(0.25, 0.5))
    y = highpass(y, 150)
    return normalize(fade_out(y, 0.1), -3.0 if surface != "Soft" else -9.0)


def live_round(surface):
    """A live round: heavier and full of powder, so it clunks more than it rings."""
    y = metal_hit(0.4, rng.uniform(2400, 3200), 0.4, {"Hard": 0.05, "Metal": 0.09, "Wood": 0.025, "Soft": 0.008}[surface], n=6, bright=6000)
    add_at(y, lowpass(noise(int(0.03 * SR)) * np.exp(-np.arange(int(0.03 * SR)) / 250), 1500), 0, 0.8)
    if surface == "Soft":
        y = lowpass(y, 2000)
    return normalize(fade_out(highpass(y, 120), 0.1), -4.0 if surface != "Soft" else -10.0)


def magazine(surface):
    """A magazine hitting the ground: a hollow steel box with rounds rattling inside."""
    dur = 0.8
    ring = {"Hard": 0.08, "Metal": 0.14, "Wood": 0.04, "Soft": 0.01}[surface]
    y = metal_hit(dur, rng.uniform(600, 900), 0.6, ring, n=8, bright=5000)
    rattle = np.zeros(int(dur * SR))
    for _ in range(rng.integers(3, 7)):
        add_at(rattle, metal_hit(0.1, rng.uniform(2500, 4000), 0.4, 0.015, n=4), rng.uniform(0.005, 0.08), rng.uniform(0.15, 0.35))
    y += rattle
    add_at(y, lowpass(noise(int(0.05 * SR)) * np.exp(-np.arange(int(0.05 * SR)) / 400), 800), 0, 1.0 if surface == "Soft" else 0.5)
    if surface == "Soft":
        y = lowpass(y, 1800)
    if surface != "Soft" and rng.random() < 0.6:
        add_at(y, metal_hit(0.4, rng.uniform(600, 900), 0.6, ring * 0.6, n=6), rng.uniform(0.1, 0.2), 0.35)
    return normalize(fade_out(highpass(y, 60), 0.15), -2.0 if surface != "Soft" else -6.0)


# ---------------------------------------------------------------- main
def main():
    count = 0
    for gun, g in GUNS.items():
        folder = os.path.join(OUT, "Guns", gun)
        for i in range(1, 6):
            write_wav(os.path.join(folder, "SW_%s_Shot_%02d.wav" % (gun, i)), shot(g)); count += 1
        for i in range(1, 4):
            write_wav(os.path.join(folder, "SW_%s_Tail_%02d.wav" % (gun, i)), tail(g)); count += 1
        for i in range(1, 5):
            write_wav(os.path.join(folder, "SW_%s_Mech_%02d.wav" % (gun, i)), mech(g)); count += 1
        print(gun, "done")
    folder = os.path.join(OUT, "Brass")
    for surface in ("Hard", "Soft", "Wood", "Metal"):
        for size in ("Pistol", "Rifle"):
            for i in range(1, 7):
                write_wav(os.path.join(folder, "SW_Brass_%s_%s_%02d.wav" % (size, surface, i)), brass(size, surface)); count += 1
        for i in range(1, 5):
            write_wav(os.path.join(folder, "SW_Round_%s_%02d.wav" % (surface, i)), live_round(surface)); count += 1
        for i in range(1, 4):
            write_wav(os.path.join(folder, "SW_Mag_%s_%02d.wav" % (surface, i)), magazine(surface)); count += 1
    print("wrote", count, "sounds to", OUT)


if __name__ == "__main__":
    main()
