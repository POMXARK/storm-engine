"""Builds the particles demo assets: the sprite atlas every particle system draws from, three
particle systems and a rock in the sea to light a fire on.

Usage: make_particles.py <output resource dir>
Writes textures/particles_list.tga.tx (the atlas the particle service always loads) and its
lighting map particles_listnm.tga.tx, particles/{campfire,splash,explosion}.xps,
models/rock/rock.gm with textures/rock/stone.tga.tx. The sea and the sky come from
../sea/make_sea.py. Everything is generated, so the demo has no third-party assets.
"""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "common"))
from storm_formats import Colors, Frames, Graph, Mesh, write_gm, write_tx, write_xps  # noqa: E402

ATLAS = 512
CELL = 128
CELLS = ATLAS // CELL


# ---------------------------------------------------------------- sprites

def fbm(size, rng, octaves=5):
    """Tiling value noise in 0..1, octaves of upscaled random grids."""
    total = np.zeros((size, size))
    weight = 0.0
    for o in range(octaves):
        n = 4 * 2 ** o
        grid = rng.random((n, n))
        layer = np.asarray(Image.fromarray((grid * 255).astype(np.uint8)).resize((size, size), Image.BICUBIC), float)
        total += layer / 255 / 2 ** o
        weight += 1 / 2 ** o
    return total / weight


def cell_coords():
    y, x = np.mgrid[0:CELL, 0:CELL].astype(float)
    return (x + 0.5) / CELL * 2 - 1, (y + 0.5) / CELL * 2 - 1


def smoke_puff(rng):
    # a soft round cloud with a ragged edge
    x, y = cell_coords()
    r = np.hypot(x, y)
    alpha = np.clip(1 - r, 0, 1) ** 1.5 * (0.45 + 0.75 * fbm(CELL, rng))
    shade = 0.8 + 0.2 * fbm(CELL, rng)
    return shade, np.clip(alpha, 0, 1)


def flame(rng):
    # a tongue of fire, wide at its root and pointed at the tip; the engine maps the top rows of
    # a frame to the bottom of the billboard, so the tip is drawn at the bottom of the cell
    x, y = cell_coords()
    up = -y
    width = 0.55 * np.clip((1 - up) / 2, 0, 1) ** 0.7 + 0.05
    body = np.clip(1 - (x / width) ** 2 - np.clip(-up - 0.6, 0, 1) * 4, 0, 1)
    flicker = 0.6 + 0.6 * fbm(CELL, rng)
    alpha = np.clip(body * flicker, 0, 1)
    # the core is white hot, the colour graph tints the rest
    shade = np.clip(0.6 + 0.6 * body, 0, 1)
    return shade, alpha


def spray(rng):
    # a clump of droplets in a thin veil of water
    x, y = cell_coords()
    alpha = 0.25 * np.clip(1 - np.hypot(x, y), 0, 1) * fbm(CELL, rng)
    for _ in range(14):
        cx, cy = rng.uniform(-0.6, 0.6, 2)
        rad = rng.uniform(0.06, 0.16)
        alpha = np.maximum(alpha, np.clip(1 - np.hypot(x - cx, y - cy) / rad, 0, 1) ** 0.7)
    return np.full_like(x, 0.95), alpha


def spark():
    x, y = cell_coords()
    r = np.hypot(x, y)
    alpha = np.clip(1 - r, 0, 1) ** 3
    return np.clip(1.2 - r, 0, 1), alpha


def atlas():
    """4x4 cells: row 0 smoke, row 1 flames, row 2 spray, a spark in row 3."""
    rng = np.random.default_rng(5)
    rgba = np.zeros((ATLAS, ATLAS, 4))
    sprites = [[smoke_puff(rng) for _ in range(4)], [flame(rng) for _ in range(4)],
               [spray(rng) for _ in range(4)], [spark()]]
    for row, cells in enumerate(sprites):
        for col, (shade, alpha) in enumerate(cells):
            block = rgba[row * CELL:(row + 1) * CELL, col * CELL:(col + 1) * CELL]
            block[..., :3] = shade[..., None]
            block[..., 3] = alpha
    return Image.fromarray((np.clip(rgba, 0, 1) * 255).astype(np.uint8), "RGBA")


def frames(row, count):
    """Texture frames of `count` cells of an atlas row; a pixel inset keeps neighbours out."""
    inset = 1 / ATLAS
    size = 1 / CELLS
    return Frames((col * size + inset, row * size + inset, size - 2 * inset, size - 2 * inset)
                  for col in range(count))


def lighting_map():
    """particles_listnm: the shader multiplies the colour by dot(map, rotation) + 0.6; a flat
    normal leaves the dot at zero, so every particle gets the same 0.6 whatever its spin."""
    return Image.new("RGB", (ATLAS, ATLAS), (128, 128, 255))


# ---------------------------------------------------------------- particle systems

def billboard(name, rate, count, life, speed, size, color, transparency, row, frame_count,
              mass=0.0, gravity=0.0, spin=0.0, friction=0.0, additive=False, spread=(0.0, 0.0, 0.0)):
    """One kind of billboard of an emitter. rate is per second, life and speed are (min, max)
    ranges, size, transparency and friction curves over the life (transparency 0 is opaque,
    100 invisible); spread is the box the particles are born in around the emitter."""
    return {
        "Name": name,
        "Emission rate": Graph.const(rate),
        "Max Particles count": float(count),
        "Life time": Graph.const(*life),
        "Velocity": Graph.const(*speed),
        "Mass": Graph.const(mass),
        # gravity is mass * 9.8 * percent; a negative mass rises
        "ForceField_gravity": Graph.const(gravity),
        "Spin": Graph.const(-spin, spin),
        # percent of the speed lost
        "Friction": friction if isinstance(friction, Graph) else Graph.const(friction),
        "Size": size,
        "Color": color,
        "Transparency": transparency,
        "Texture": frames(row, frame_count),
        # each particle picks one frame of the row and keeps it
        "Animation frame": Graph.const(0.0, frame_count - 0.01),
        # 100 adds the colour to the screen (fire, sparks), 0 blends it over (smoke, water)
        "Add Technique Power": Graph.const(100.0 if additive else 0.0),
        "Track X": Graph.const(-spread[0], spread[0]),
        "Track Y": Graph.const(-spread[1], spread[1]),
        "Track Z": Graph.const(-spread[2], spread[2]),
        "Attached emitter": "none",
    }


def emitter(name, life, looping, cone):
    """A point emitter shooting along its +y within `cone` degrees; it emits for `life` seconds,
    then stops or starts over."""
    return {
        "Name": name,
        "Life time": life,
        "Looping": looping,
        "Position": (0.0, 0.0, 0.0),
        "Physics Dir X": Graph.const(-cone, cone),
        "Physics Dir Y": Graph.const(0.0),
        "Physics Dir Z": Graph.const(-cone, cone),
    }


def fade(points):
    return Graph.life(points)


def campfire():
    flames = billboard(
        "flames", rate=45, count=70, life=(0.5, 0.9), speed=(1.5, 2.6),
        size=Graph.life([(0, 0.6), (30, 1.1), (100, 0.3)]),
        color=Colors([(0.0, (1.0, 0.85, 0.45), (1.0, 0.95, 0.6)), (0.5, (1.0, 0.45, 0.1), (1.0, 0.6, 0.2)),
                      (1.0, (0.6, 0.1, 0.02), (0.8, 0.2, 0.05))]),
        transparency=fade([(0, 40), (15, 0), (70, 20), (100, 100)]),
        row=1, frame_count=4, additive=True, spread=(0.35, 0.0, 0.35))
    smoke = billboard(
        "smoke", rate=7, count=45, life=(4.0, 6.0), speed=(0.8, 1.4),
        size=Graph.life([(0, 0.5), (100, 3.5)]),
        color=Colors([(0.0, (0.3, 0.28, 0.26), (0.4, 0.38, 0.35)), (1.0, (0.55, 0.55, 0.55), (0.65, 0.65, 0.65))]),
        transparency=fade([(0, 100), (10, 45), (60, 70), (100, 100)]),
        row=0, frame_count=4, mass=-1.0, gravity=3.0, spin=25.0, spread=(0.3, 0.6, 0.3))
    sparks = billboard(
        "sparks", rate=5, count=20, life=(1.0, 2.0), speed=(2.5, 4.5),
        size=Graph.const(0.05, 0.08),
        color=Colors([(0.0, (1.0, 0.8, 0.4), (1.0, 0.9, 0.5)), (1.0, (1.0, 0.3, 0.05), (1.0, 0.4, 0.1))]),
        transparency=fade([(0, 0), (70, 10), (100, 100)]),
        row=3, frame_count=1, mass=1.0, gravity=15.0, additive=True, spread=(0.3, 0.0, 0.3))
    # the flames lean little, the smoke and the sparks spread wider
    return [(emitter("fire", 1.0, True, 8.0), [flames]),
            (emitter("smoke", 1.0, True, 15.0), [smoke]),
            (emitter("sparks", 1.0, True, 25.0), [sparks])]


def splash():
    # a cannonball hitting the water: a column of spray falling back and a cloud of mist
    column = billboard(
        "spray", rate=450, count=80, life=(1.2, 1.8), speed=(6.0, 10.0),
        size=Graph.life([(0, 0.45), (100, 1.2)]),
        color=Colors([(0.0, (0.85, 0.92, 0.95), (1.0, 1.0, 1.0)), (1.0, (0.75, 0.85, 0.9), (0.9, 0.95, 1.0))]),
        transparency=fade([(0, 0), (60, 15), (100, 100)]),
        row=2, frame_count=4, mass=1.0, gravity=100.0, spin=90.0, spread=(0.4, 0.0, 0.4))
    mist = billboard(
        "mist", rate=90, count=20, life=(2.0, 3.0), speed=(1.0, 2.5),
        size=Graph.life([(0, 1.0), (100, 4.0)]),
        color=Colors([(0.0, (0.9, 0.93, 0.95), (1.0, 1.0, 1.0)), (1.0, (0.9, 0.93, 0.95), (1.0, 1.0, 1.0))]),
        transparency=fade([(0, 50), (20, 55), (100, 100)]),
        row=0, frame_count=4, spin=30.0, spread=(0.8, 0.3, 0.8))
    return [(emitter("column", 0.15, False, 18.0), [column]),
            (emitter("mist", 0.15, False, 60.0), [mist])]


def explosion():
    # a powder blast: a short fireball, burning debris thrown up and falling, a pall of smoke
    fireball = billboard(
        "fireball", rate=300, count=40, life=(0.4, 0.8), speed=(3.0, 7.0),
        size=Graph.life([(0, 1.2), (40, 2.5), (100, 3.0)]),
        color=Colors([(0.0, (1.0, 0.9, 0.6), (1.0, 1.0, 0.8)), (0.4, (1.0, 0.55, 0.15), (1.0, 0.7, 0.25)),
                      (1.0, (0.5, 0.12, 0.03), (0.7, 0.2, 0.05))]),
        transparency=fade([(0, 0), (50, 20), (100, 100)]),
        row=1, frame_count=4, spin=120.0, additive=True, spread=(0.5, 0.3, 0.5))
    debris = billboard(
        "debris", rate=600, count=70, life=(1.0, 2.2), speed=(8.0, 15.0),
        size=Graph.const(0.08, 0.14),
        color=Colors([(0.0, (1.0, 0.85, 0.5), (1.0, 0.95, 0.6)), (1.0, (1.0, 0.3, 0.05), (1.0, 0.45, 0.1))]),
        transparency=fade([(0, 0), (80, 20), (100, 100)]),
        row=3, frame_count=1, mass=1.0, gravity=100.0, additive=True)
    smoke = billboard(
        "smoke", rate=160, count=25, life=(4.0, 6.0), speed=(1.5, 3.5),
        size=Graph.life([(0, 1.5), (100, 6.0)]),
        color=Colors([(0.0, (0.15, 0.13, 0.12), (0.25, 0.22, 0.2)), (1.0, (0.4, 0.4, 0.4), (0.5, 0.5, 0.5))]),
        transparency=fade([(0, 30), (15, 20), (100, 100)]),
        row=0, frame_count=4, mass=-1.0, gravity=4.0, spin=20.0, friction=Graph.life([(0, 0), (100, 60)]),
        spread=(1.0, 0.5, 1.0))
    return [(emitter("blast", 0.12, False, 80.0), [fireball]),
            (emitter("debris", 0.12, False, 55.0), [debris]),
            (emitter("smoke", 0.15, False, 80.0), [smoke])]


# ---------------------------------------------------------------- rock

def stone_texture(rng):
    shade = 0.55 + 0.45 * fbm(256, rng, octaves=6)
    rgb = np.array([128, 122, 112], float)[None, None, :] * shade[..., None] * 1.3
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), "RGB")


def rock_mesh(rng):
    """A rounded rock rising 2.5 m out of the water, its flat top at the origin for the fire."""
    mesh = Mesh("rock", "rock\\stone.tga")
    rings, sectors = 12, 32
    bumps = rng.uniform(0.8, 1.2, sectors)
    index = []
    for i in range(rings + 1):
        t = i / rings
        # from the top centre out and down to below the waterline
        radius = 7.0 * math.sin(t * math.pi / 2) ** 0.8
        height = 2.5 * math.cos(t * math.pi / 2) ** 1.5 - 3.0 * t ** 3
        index.append([])
        for j in range(sectors + 1):
            a = 2 * math.pi * j / sectors
            k = 1.0 if i == 0 else bumps[j % sectors] * rng.uniform(0.95, 1.05)
            pos = (radius * k * math.cos(a), height - 2.5 + (0.15 * rng.standard_normal() if 0 < i < rings else 0),
                   radius * k * math.sin(a))
            index[i].append(pos)
    p = np.array(index)
    du = np.gradient(p, axis=0)
    dv = np.gradient(p, axis=1)
    n = np.cross(du, dv)
    n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-9)
    ids = []
    for i in range(rings + 1):
        ids.append([])
        for j in range(sectors + 1):
            # outward from a point under the rock
            normal = n[i, j] if np.dot(n[i, j], p[i, j] - (0.0, -6.0, 0.0)) >= 0 else -n[i, j]
            if i == 0:
                normal = np.array([0.0, 1.0, 0.0])
            ids[i].append(mesh.vertex(p[i, j], normal, (j / sectors * 4, i / rings * 2)))
    for i in range(rings):
        for j in range(sectors):
            mesh.quad(ids[i][j], ids[i + 1][j], ids[i + 1][j + 1], ids[i][j + 1])
    return mesh


def main(out):
    out = Path(out)
    write_tx(atlas(), out / "textures" / "particles_list.tga.tx")
    write_tx(lighting_map(), out / "textures" / "particles_listnm.tga.tx")
    write_xps(campfire(), out / "particles" / "campfire.xps")
    write_xps(splash(), out / "particles" / "splash.xps")
    write_xps(explosion(), out / "particles" / "explosion.xps")

    rng = np.random.default_rng(11)
    write_tx(stone_texture(rng), out / "textures" / "rock" / "stone.tga.tx")
    write_gm([rock_mesh(rng)], out / "models" / "rock" / "rock.gm")


if __name__ == "__main__":
    main(sys.argv[1])
