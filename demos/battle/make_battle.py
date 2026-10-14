"""Generates the assets of the battle demo that the other demos do not have.

Usage: make_battle.py <output resource dir>
  textures/demo/ball.tga.tx   the cannonball sprite AIBalls draws for every ball in flight
  textures/blot.tga.tx        four shot holes in a 2x2 atlas, the blots entity picks one per hit
  sounds/demo/hit{1,2}.wav    a ball crashing through planks
  sounds/demo/splash.wav      a ball falling into the sea
  sounds/demo/crack.wav       a mast breaking
The sea, the ship, the particles, the cannon shots, the music and the log font come from the
generators of the sea, ship, particles and sound demos.
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
# the generators of the other demos are imported: no __pycache__ left in their folders
sys.dont_write_bytecode = True
sys.path.insert(0, str(HERE.parent / "common"))
sys.path.insert(0, str(HERE.parent / "sound"))
from make_sound import lowpass, normalize, seconds, write_wav  # noqa: E402
from storm_formats import write_tx  # noqa: E402


def ball_texture(size=64):
    # a dark iron ball lit from the top left; the sprite is turned as it flies, so the light is soft
    y, x = (np.mgrid[0:size, 0:size] + 0.5) / size * 2.0 - 1.0
    r2 = x * x + y * y
    inside = r2 < 1.0
    z = np.sqrt(np.clip(1.0 - r2, 0.0, 1.0))
    light = np.clip(-0.45 * x - 0.55 * y + 0.7 * z, 0.0, 1.0)
    shade = 28 + 70 * light ** 1.5
    rgba = np.zeros((size, size, 4), np.uint8)
    rgba[..., 0] = shade
    rgba[..., 1] = shade * 0.95
    rgba[..., 2] = shade * 0.9
    rgba[..., 3] = np.where(inside, 255, 0)
    return Image.fromarray(rgba, "RGBA")


def blot_texture(size=256, seed=5):
    # each quarter is one hole: a black core, splintered charred wood around it fading out
    rng = np.random.default_rng(seed)
    half = size // 2
    rgba = np.zeros((size, size, 4), np.uint8)
    y, x = (np.mgrid[0:half, 0:half] + 0.5) / half * 2.0 - 1.0
    angle = np.arctan2(y, x)
    r = np.sqrt(x * x + y * y)
    for qy in range(2):
        for qx in range(2):
            # a ragged edge: the radius changes with the angle by a few random harmonics
            edge = 0.32 + sum(rng.uniform(0.01, 0.05) * np.sin(k * angle + rng.uniform(0, 6.3)) for k in (3, 5, 7, 11))
            core = r < edge
            soot = np.clip(1.0 - (r - edge) / (0.95 - edge), 0.0, 1.0) ** 1.6
            splinters = (np.sin(9 * angle + rng.uniform(0, 6.3)) > 0.6) & (r < edge + 0.18)
            alpha = np.where(core, 1.0, np.maximum(0.85 * soot, np.where(splinters, 0.9, 0.0)))
            shade = np.where(core, 8, np.where(splinters, 70, 30))
            tile = np.stack([shade, shade * 0.85, shade * 0.7, alpha * 255], axis=-1)
            rgba[qy * half:(qy + 1) * half, qx * half:(qx + 1) * half] = np.clip(tile, 0, 255)
    return Image.fromarray(rgba, "RGBA")


def hit(rng, pitch):
    # a crack of planks: a sharp click, splintering noise and a hollow knock of the hull
    t = seconds(1.2)
    click = rng.normal(size=len(t)) * np.exp(-t / 0.006)
    splinter = lowpass(rng.normal(size=len(t)), 3000.0 * pitch) * np.exp(-t / 0.09)
    splinter *= 1.0 + 0.8 * np.sign(np.sin(2 * np.pi * 37 * t))
    knock = np.sin(2 * np.pi * 95.0 * pitch * t) * np.exp(-t / 0.18)
    return normalize(0.8 * click + 1.4 * splinter + 1.2 * knock, 0.9)


def splash(rng):
    # a thump into the water and the spray falling back
    t = seconds(1.6)
    plop = np.sin(2 * np.pi * (180.0 - 120.0 * np.minimum(t / 0.08, 1.0)) * t) * np.exp(-t / 0.07)
    spray = lowpass(rng.normal(size=len(t)), 2500.0) * (np.minimum(1.0, t * 30.0) * np.exp(-t / 0.45))
    return normalize(1.0 * plop + 1.3 * spray, 0.8)


def crack(rng):
    # a mast giving way: a long creak rising in pitch, then a snap
    t = seconds(2.2)
    creak_f = 140.0 + 260.0 * (t / 1.4) ** 2
    creak = np.sign(np.sin(2 * np.pi * np.cumsum(creak_f) / 44100.0)) * np.where(t < 1.4, t / 1.4, 0.0)
    creak = lowpass(creak, 1200.0)
    snap_t = np.maximum(t - 1.4, 0.0)
    snap = np.where(t > 1.4, rng.normal(size=len(t)) * np.exp(-snap_t / 0.15), 0.0)
    return normalize(0.6 * creak + 1.5 * lowpass(snap, 2500.0), 0.9)


def main(out):
    out = Path(out)
    rng = np.random.default_rng(1805)
    write_tx(ball_texture(), out / "textures" / "demo" / "ball.tga.tx")
    write_tx(blot_texture(), out / "textures" / "blot.tga.tx")
    for i, pitch in enumerate((1.0, 0.8), 1):
        write_wav(out / "sounds" / "demo" / f"hit{i}.wav", hit(rng, pitch))
    write_wav(out / "sounds" / "demo" / "splash.wav", splash(rng))
    write_wav(out / "sounds" / "demo" / "crack.wav", crack(rng))


if __name__ == "__main__":
    main(sys.argv[1])
