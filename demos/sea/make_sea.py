"""Builds the sea demo assets: three skyboxes (day, sunset, storm), the animated wave height frames
of the sea and its foam, trash and light sprites.

Usage: make_sea.py <output resource dir>
Writes textures/weather/skies/<preset>/sky_{fr,lf,bk,rt,up}.tga.tx, sea/sea0000..0063.tga,
textures/weather/sea/pena/pena.tga.tx, textures/seatrash.tga.tx and textures/sealight.tga.tx.
Everything is generated, so the demo has no third-party assets.
"""
import math
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "common"))
from storm_formats import write_tx  # noqa: E402

SKY_SIZE = 512

# weather/src/sky.cpp: the sky is half a cube, x and z from -1 to 1 and y from 0 (horizon) to 1,
# mirrored below the horizon for the sea reflection; each face maps its texture (u, v) to a point
# of the cube, the pixel shows the sky in the direction of that point
SKY_FACES = {
    "sky_fr": lambda u, v: (-1 + 2 * u, 1 - v, -np.ones_like(u)),
    "sky_lf": lambda u, v: (np.ones_like(u), 1 - v, -1 + 2 * u),
    "sky_bk": lambda u, v: (1 - 2 * u, 1 - v, np.ones_like(u)),
    "sky_rt": lambda u, v: (-np.ones_like(u), 1 - v, 1 - 2 * u),
    "sky_up": lambda u, v: (1 - 2 * u, np.ones_like(u), -1 + 2 * v),
}

# the presets match the weather set by program/sea.c: sun angles are Weather.Sun.HeightAngle and
# AzimuthAngle, so the sun painted on the sky is where the light comes from
SKY_PRESETS = {
    "day": dict(zenith=(48, 104, 186), horizon=(176, 206, 228), sun_height=0.7, sun_azimuth=math.radians(150),
                sun=(255, 246, 220), glow=0.35, clouds=0.42, cloud_light=(250, 250, 248), cloud_dark=(150, 162, 180)),
    "sunset": dict(zenith=(40, 60, 120), horizon=(250, 150, 80), sun_height=0.06, sun_azimuth=math.radians(200),
                   sun=(255, 200, 120), glow=0.9, clouds=0.38, cloud_light=(255, 170, 110), cloud_dark=(90, 70, 100)),
    "storm": dict(zenith=(70, 76, 86), horizon=(120, 126, 132), sun_height=0.5, sun_azimuth=math.radians(150),
                  sun=(150, 150, 150), glow=0.05, clouds=0.9, cloud_light=(140, 144, 150), cloud_dark=(52, 56, 64)),
}


def value_noise(x, y, period, seed):
    """Smooth lattice noise in [0, 1] repeating every `period` units in x and y."""
    lattice = np.random.default_rng(seed).random((period, period))
    x0 = np.floor(x).astype(int)
    y0 = np.floor(y).astype(int)
    fx = x - x0
    fy = y - y0
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    x0 %= period
    y0 %= period
    x1 = (x0 + 1) % period
    y1 = (y0 + 1) % period
    top = lattice[y0, x0] * (1 - fx) + lattice[y0, x1] * fx
    bottom = lattice[y1, x0] * (1 - fx) + lattice[y1, x1] * fx
    return top * (1 - fy) + bottom * fy


def fbm(x, y, period, seed, octaves=5):
    total = np.zeros_like(x)
    weight = 0.0
    for octave in range(octaves):
        scale = 2 ** octave
        total += value_noise(x * scale, y * scale, period * scale, seed + octave) / scale
        weight += 1 / scale
    return total / weight


def smoothstep(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0, 1)
    return t * t * (3 - 2 * t)


def sky_face(face, preset):
    p = SKY_PRESETS[preset]
    u, v = np.meshgrid((np.arange(SKY_SIZE) + 0.5) / SKY_SIZE, (np.arange(SKY_SIZE) + 0.5) / SKY_SIZE)
    x, y, z = SKY_FACES[face](u, v)
    length = np.sqrt(x * x + y * y + z * z)
    x, y, z = x / length, y / length, z / length

    # gradient from the horizon haze to the zenith colour
    t = (1 - y) ** 4
    color = np.array(p["zenith"], float) * (1 - t)[..., None] + np.array(p["horizon"], float) * t[..., None]

    # weather.cpp turns (5000, 0, 0) up by the height angle and then about y by the azimuth
    h, a = p["sun_height"], p["sun_azimuth"]
    sun = (math.cos(h) * math.cos(a), math.sin(h), -math.cos(h) * math.sin(a))
    cos_sun = np.clip(x * sun[0] + y * sun[1] + z * sun[2], 0, 1)
    sun_color = np.array(p["sun"], float)
    glow = p["glow"] * cos_sun ** 6 + 0.6 * p["glow"] * cos_sun ** 64
    color += sun_color * glow[..., None]

    # clouds are noise on a plane above the camera, thinning out towards the horizon
    plane = 1.0 / np.maximum(y, 0.02)
    cx = x * plane * 1.2 + 100
    cz = z * plane * 1.2 + 100
    # fbm stays near 0.5, stretch it so the coverage threshold spans clear sky to overcast
    density = 0.5 + 2.2 * (fbm(cx, cz, 64, seed={"day": 1, "sunset": 7, "storm": 13}[preset]) - 0.5)
    cover = smoothstep(1 - p["clouds"], 1 - p["clouds"] + 0.3, density)
    cover *= smoothstep(0.03, 0.3, y)
    shade = smoothstep(0.35, 0.85, fbm(cx + 3.7, cz + 1.3, 64, seed=99, octaves=3))
    cloud = (np.array(p["cloud_dark"], float) * (1 - shade)[..., None]
             + np.array(p["cloud_light"], float) * shade[..., None])
    cloud += sun_color * (0.5 * p["glow"] * cos_sun ** 8)[..., None]
    color = color * (1 - cover)[..., None] + cloud * cover[..., None]

    return Image.fromarray(np.clip(color, 0, 255).astype(np.uint8), "RGB")


# sea/src/sea.cpp: FRAMES heightmaps of XWIDTH x YWIDTH, played in a loop and tiled over the sea;
# the height is the first (blue) byte of every 32-bit pixel of an uncompressed TGA
SEA_FRAMES = 64
SEA_WIDTH = 128


def sea_frames():
    """Sum of travelling waves with whole wave numbers and whole periods per loop, so the
    heightmap tiles in space and the animation loops seamlessly; most waves run along +x."""
    rng = np.random.default_rng(5)
    xs, ys = np.meshgrid(np.arange(SEA_WIDTH), np.arange(SEA_WIDTH))
    waves = []
    for _ in range(28):
        k = rng.uniform(1.5, 9.0)
        angle = rng.normal(0.0, 0.5)
        kx = int(round(k * math.cos(angle))) or 1
        ky = int(round(k * math.sin(angle)))
        length = math.hypot(kx, ky)
        # deep water: frequency grows with the square root of the wave number
        cycles = max(1, int(round(1.6 * math.sqrt(length))))
        waves.append((kx, ky, cycles, length ** -1.4, rng.uniform(0, 2 * math.pi)))

    frames = []
    for frame in range(SEA_FRAMES):
        height = np.zeros((SEA_WIDTH, SEA_WIDTH))
        for kx, ky, cycles, amp, phase in waves:
            arg = 2 * math.pi * ((kx * xs + ky * ys) / SEA_WIDTH - cycles * frame / SEA_FRAMES) + phase
            # sharper crests and flat troughs, like real swell
            height += amp * (np.cos(arg) + 0.25 * np.cos(2 * arg))
        frames.append(height)
    low = min(f.min() for f in frames)
    high = max(f.max() for f in frames)
    return [((f - low) / (high - low) * 255).astype(np.uint8) for f in frames]


def write_sea_tga(height, path):
    # uncompressed true colour, 32 bits, top-left origin
    header = struct.pack("<BBB5sHHHHBB", 0, 0, 2, b"\0" * 5, 0, 0, SEA_WIDTH, SEA_WIDTH, 32, 0x28)
    pixels = np.stack([height, height, height, np.full_like(height, 255)], axis=-1)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + pixels.tobytes())


def foam():
    size = 256
    x, y = np.meshgrid(np.arange(size) / size * 16, np.arange(size) / size * 16)
    n = fbm(x, y, 16, seed=21, octaves=4)
    # bubbly streaks: bright ridges of the noise
    ridges = 1 - np.abs(2 * n - 1)
    alpha = smoothstep(0.55, 0.9, ridges) * 255
    rgb = np.full((size, size, 3), 245.0)
    return Image.fromarray(np.dstack([rgb, alpha]).astype(np.uint8), "RGBA")


def sprite(color, radius):
    size = 64
    x, y = np.meshgrid(np.arange(size) - size / 2 + 0.5, np.arange(size) - size / 2 + 0.5)
    alpha = smoothstep(radius, radius * 0.4, np.sqrt(x * x + y * y)) * 255
    rgb = np.broadcast_to(np.array(color, float), (size, size, 3))
    return Image.fromarray(np.dstack([rgb, alpha]).astype(np.uint8), "RGBA")


def main():
    resource = Path(sys.argv[1])
    textures = resource / "textures"
    for preset in SKY_PRESETS:
        for face in SKY_FACES:
            write_tx(sky_face(face, preset), textures / "weather" / "skies" / preset / f"{face}.tga.tx")
    for index, height in enumerate(sea_frames()):
        write_sea_tga(height, resource / "sea" / f"sea{index:04d}.tga")
    write_tx(foam(), textures / "weather" / "sea" / "pena" / "pena.tga.tx")
    write_tx(sprite((200, 190, 160), 20), textures / "seatrash.tga.tx")
    write_tx(sprite((255, 230, 160), 28), textures / "sealight.tga.tx")


if __name__ == "__main__":
    main()
