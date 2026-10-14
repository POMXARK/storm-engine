"""Builds the ship demo assets: a two-masted ship split into the nodes the engine expects, its
textures, the sail textures and the sail parameters.

Usage: make_ship.py <output resource dir>
Writes models/ships/Brig/Brig*.gm, textures/ships/brig/*.tga.tx, the textures the sail entity
loads by fixed names (textures/ships/parus_hole.tga.tx, emptygerald.tga.tx) and ini/rigging.ini, ini/mast.ini.
The sea and the sky come from ../sea/make_sea.py. Everything is generated, so the demo has no
third-party assets.
"""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "common"))
from storm_formats import Mesh, write_gm, write_tx  # noqa: E402

SHIP = "Brig"

# hull in metres: the model origin is on the waterline, +z is the bow
LENGTH = 26.0
BEAM = 7.0
KEEL = -2.0
DECK = 3.4
# masts stand on the deck; the yards of every mast are child nodes of it
MASTS = {
    "mast1": dict(z=3.5, height=21.0, yards=[("rey_a1", 9.0, 11.0, 7.0), ("rey_b1", 16.0, 8.5, 6.0)]),
    "mast2": dict(z=-5.5, height=18.0, yards=[("rey_a2", 8.0, 9.5, 6.0), ("rey_b2", 14.0, 7.5, 5.0)]),
}
BOWSPRIT_TIP = (0.0, DECK + 3.5, LENGTH / 2 + 7.0)
# six guns a side, from the stern quarter to the bow; the muzzles are a little off the side so a
# ball does not start inside the hull it is fired from
GUNS = 6
GUN_AFT, GUN_FORE = -8.0, 7.0
GUN_SIDE = BEAM / 2 + 0.4
GUN_HEIGHT = 2.6


# ---------------------------------------------------------------- textures

def planks(width, height, rows, base, rng, seam=2):
    """Horizontal planks with grain, slightly different in tone, dark seams between them."""
    y, x = np.mgrid[0:height, 0:width].astype(float)
    row = (y * rows / height).astype(int)
    tone = rng.uniform(0.85, 1.1, rows + 1)[row]
    grain = 0.9 + 0.1 * np.sin(x / width * 40 + np.sin(y / 3.0) * 2 + row * 7)
    shade = tone * grain
    seam_mask = (y * rows % height) < seam * rows
    shade[seam_mask] *= 0.45
    rgb = np.clip(np.array(base, float)[None, None, :] * shade[..., None], 0, 255)
    return Image.fromarray(rgb.astype(np.uint8), "RGB")


def hull_texture(rng):
    # dark tarred wood below a painted band at the gunwale
    img = np.asarray(planks(512, 256, 16, (96, 66, 40), rng), float)
    band = np.zeros(256, bool)
    band[0:40] = True
    img[band] = img[band] * np.array([1.6, 0.55, 0.45])
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB")


def deck_texture(rng):
    return planks(256, 256, 10, (170, 135, 95), rng).rotate(90)


def spar_texture(rng):
    return planks(64, 256, 1, (120, 85, 50), rng, seam=0)


def canvas():
    """Off-white sailcloth with seams across it; alpha is opaque."""
    size = 256
    y, x = np.mgrid[0:size, 0:size].astype(float)
    rng = np.random.default_rng(3)
    shade = 0.92 + 0.05 * rng.random((size, size))
    shade[(x % 32) < 2] *= 0.82
    rgb = np.clip(np.array([236, 228, 206], float) * shade[..., None], 0, 255)
    return Image.fromarray(rgb.astype(np.uint8), "RGB")


# ---------------------------------------------------------------- geometry

def grid(mesh, points, uvs, outward):
    """Adds a grid of points [i][j] with normals from the grid tangents, turned to `outward(p)`."""
    rows, cols = len(points), len(points[0])
    p = np.array(points, float)
    du = np.gradient(p, axis=0)
    dv = np.gradient(p, axis=1)
    n = np.cross(du, dv)
    n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-9)
    index = []
    for i in range(rows):
        index.append([])
        for j in range(cols):
            normal = n[i, j]
            if np.dot(normal, outward(p[i, j])) < 0:
                normal = -normal
            index[i].append(mesh.vertex(p[i, j], normal, uvs[i][j]))
    for i in range(rows - 1):
        for j in range(cols - 1):
            mesh.quad(index[i][j], index[i + 1][j], index[i + 1][j + 1], index[i][j + 1])


def half_beam(z):
    """Half width of the hull at z: a pointed bow and a flat transom at the stern."""
    s = 2 * z / LENGTH
    if s >= 0:
        return BEAM / 2 * (1 - s ** 2) ** 0.6
    return BEAM / 2 * (1 - 0.35 * s ** 2)


def sheer(z):
    # the deck rises towards the bow and the stern
    s = 2 * z / LENGTH
    return DECK + 0.5 * s ** 2 + 0.4 * max(s, 0) ** 3


def hull_meshes():
    sections, depth = 40, 10
    zs = [-LENGTH / 2 + LENGTH * i / sections for i in range(sections + 1)]

    hull = Mesh("hull", f"ships\\{SHIP.lower()}\\hull.tga")
    for side in (1, -1):
        points, uvs = [], []
        for z in zs:
            w, top = half_beam(z), sheer(z)
            row, uv = [], []
            for j in range(depth + 1):
                t = j / depth
                # round bilge: wide near the deck, narrowing to the keel
                x = side * w * (1 - (1 - t) ** 2.2)
                y = KEEL + (top - KEEL) * t
                row.append((x, y, z))
                uv.append((z / 6.0, 1 - t))
            points.append(row)
            uvs.append(uv)
        grid(hull, points, uvs, lambda p: (p[0], 0.3 * (p[1] - KEEL), 0.0))
    # transom
    z0 = zs[0]
    w0, top0 = half_beam(z0), sheer(z0)
    transom = [(-w0, top0, z0), (w0, top0, z0), (0.0, KEEL, z0)]
    a, b, c = (hull.vertex(p, (0, 0, -1), (p[0] / 6.0, 0.5)) for p in transom)
    hull.triangle(a, b, c)

    deck = Mesh("deck", f"ships\\{SHIP.lower()}\\deck.tga")
    points, uvs = [], []
    for z in zs:
        w, top = half_beam(z), sheer(z) - 0.25
        points.append([(-w, top, z), (0.0, top + 0.08, z), (w, top, z)])
        uvs.append([(-w / 4, z / 4), (0, z / 4), (w / 4, z / 4)])
    grid(deck, points, uvs, lambda p: (0, 1, 0))

    spars = Mesh("bowsprit", f"ships\\{SHIP.lower()}\\spar.tga")
    cylinder(spars, (0.0, sheer(LENGTH / 2) - 0.4, LENGTH / 2 - 3.0), BOWSPRIT_TIP, 0.22, 0.12)
    return [hull, deck, spars]


def cylinder(mesh, a, b, r_a, r_b, segments=10):
    a, b = np.array(a, float), np.array(b, float)
    axis = (b - a) / np.linalg.norm(b - a)
    side = np.cross(axis, (0, 1, 0) if abs(axis[1]) < 0.9 else (1, 0, 0))
    side /= np.linalg.norm(side)
    up = np.cross(side, axis)
    points, uvs = [], []
    for k, (end, r) in enumerate(((a, r_a), (b, r_b))):
        row, uv = [], []
        for i in range(segments + 1):
            ang = 2 * math.pi * i / segments
            row.append(end + r * (math.cos(ang) * side + math.sin(ang) * up))
            uv.append((i / segments, 4 * k))
        points.append(row)
        uvs.append(uv)
    center = (a + b) / 2

    def outward(p):
        d = p - center
        return d - np.dot(d, axis) * axis

    grid(mesh, points, uvs, outward)


def mast_meshes(height):
    mast = Mesh("mast", f"ships\\{SHIP.lower()}\\spar.tga")
    cylinder(mast, (0, -0.5, 0), (0, height, 0), 0.32, 0.16)
    return [mast]


def yard_meshes(width):
    yard = Mesh("yard", f"ships\\{SHIP.lower()}\\spar.tga")
    cylinder(yard, (-width / 2 - 0.3, 0, 0.35), (width / 2 + 0.3, 0, 0.35), 0.14, 0.14)
    return [yard]


def square_sail(group, width, drop):
    """Corner locators of a square sail hanging from a yard: sail1, sail2 the top corners, sail3
    and sail4 the bottom ones (sail.cpp maps them to hardPoints 0, 1, 3, 2)."""
    top, bottom = width / 2, width / 2 + 0.6
    return [(group, "sail1", (-top, -0.2, 0.5)), (group, "sail2", (top, -0.2, 0.5)),
            (group, "sail3", (bottom, -drop, 0.5)), (group, "sail4", (-bottom, -drop, 0.5))]


def main():
    resource = Path(sys.argv[1])
    rng = np.random.default_rng(11)
    textures = resource / "textures" / "ships"
    write_tx(hull_texture(rng), textures / SHIP.lower() / "hull.tga.tx")
    write_tx(deck_texture(rng), textures / SHIP.lower() / "deck.tga.tx")
    write_tx(spar_texture(rng), textures / SHIP.lower() / "spar.tga.tx")
    write_tx(canvas(), textures / SHIP.lower() / "sail.tga.tx")
    # the sail entity multiplies the alpha of its sails by this texture to cut holes, so it is
    # opaque; the coat of arms texture is blended over the sail by its alpha, so it is empty
    write_tx(Image.new("RGBA", (16, 16), (255, 255, 255, 255)), textures / "parus_hole.tga.tx")
    write_tx(Image.new("RGBA", (16, 16), (0, 0, 0, 0)), textures / "emptygerald.tga.tx")

    models = resource / "models" / "ships" / SHIP
    # the jib: a triangle from the bowsprit to the fore mast, held by the hull node (no yard, it does
    # not turn); sail1 the clew, sail2 the head and sail3 the tack
    fore = MASTS["mast1"]
    jib = [("sailt9", "sail1", (0.0, DECK + 2.0, fore["z"] + 2.5)),
           ("sailt9", "sail2", (0.0, DECK + fore["height"] - 3.0, fore["z"] + 0.4)),
           ("sailt9", "sail3", (BOWSPRIT_TIP[0], BOWSPRIT_TIP[1] - 0.3, BOWSPRIT_TIP[2] - 0.6))]
    masts = [("geometry", name, (0.0, DECK - 0.3, m["z"])) for name, m in MASTS.items()]
    # SEA_AI finds the guns of a side by the label group named as the side in Cannons.Borts of the
    # ship type, and fires each along the z axis of its label
    guns = [(bort, f"gun{i}", (side * GUN_SIDE, GUN_HEIGHT, GUN_AFT + (GUN_FORE - GUN_AFT) * i / (GUNS - 1)), (side, 0.0))
            for bort, side in (("cannonl", -1.0), ("cannonr", 1.0)) for i in range(GUNS)]
    # the BSP is what cannonballs (and the camera) trace against: without it a ball flies through
    write_gm(hull_meshes(), models / f"{SHIP}.gm", labels=masts + jib + guns, bsp=True)
    group = 1
    for name, m in MASTS.items():
        yards = [("geometry", yard, (0.0, y, 0.0)) for yard, y, _, _ in m["yards"]]
        write_gm(mast_meshes(m["height"]), models / f"{SHIP}_{name}.gm", labels=yards, bsp=True)
        for yard, _, width, drop in m["yards"]:
            write_gm(yard_meshes(width), models / f"{SHIP}_{yard}.gm", labels=square_sail(f"sails{group}", width, drop))
            group += 1

    ini = resource / "ini" / "rigging.ini"
    ini.parent.mkdir(parents=True, exist_ok=True)
    ini.write_text(RIGGING_INI)
    # a mast broken by a ball becomes a mast entity; it throws when the file is missing
    (resource / "ini" / "mast.ini").write_text(MAST_INI)


# parameters of a falling mast (mast/src/mast.cpp): every key has a default, the file must exist
MAST_INI = """[MAST]
; downfall_acceleration, downfall_maxspeed, fMaxAngle, fMaxDeep ... as in mast.cpp
"""


# parameters of the sail entity (rigging/src/sail_config.cpp); what is left out takes its default.
# Colours are 0xRRGGBBAA as decimal numbers
RIGGING_INI = """[SAILS]
; how the sails catch the wind: speed share by the angle to the wind (side, back) per sail shape
TriangleWindSpeed = 0.2,0.6,0.8
TrapecidalWindSpeed = 0.4,0.5,0.6
SquareWindSpeed = 0.4,0.1,0.6
; ripples running over the cloth
fSsailWindDepend = 0.3
fTsailWindDepend = 0.1
FLEXSPEED = 0.000005
MAXSUMWIND = 0.001
fWindAdding = 0.5
WINDVECTOR_QNT = 200
WINDVECTOR_TINCR = 30
WINDVECTOR_TADD = 8
WINDVECTOR_SINCR = 40
WINDVECTOR_SADD = 10
; yards turn to the wind, sails roll up and down at this speed
WINDANGLDISCRETE = 0.0001
MAXTURNANGL = 0.6
TURNSTEPANGL = 0.002
ROLLINGSPEED = 0.0006
Diffuse = 2576980479
Ambient = 3435973887
Specular = 0
Emissive = 0
Power = 0
"""


if __name__ == "__main__":
    main()
