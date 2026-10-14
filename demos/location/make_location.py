"""Builds the location demo assets: a walled yard (.gm) with its character patch (.ptc) and the
sailor set up as a location character.

Usage: make_location.py <output resource dir>
Writes models/yard/{yard.gm,yard_patch.ptc}, textures/yard/*.tga.tx, the sailor as
models/characters/sailor.gm with textures/characters/*.tga.tx, and animation/sailor.{an,ani}.
Everything is generated, so the demo has no third-party assets.

The yard: cobbled ground inside four walls, a wooden platform in one corner with a ramp up to it
and a few crates. The patch is the floor characters walk on: a grid of 0.5 m cells over the
yard, leaving out the cells under and next to the crates, walls and the platform's sides.
"""
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

# the format writers and the sailor are shared by the demos
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "common"))
import sailor  # noqa: E402
from storm_formats import Mesh, write_gm, write_ptc, write_tx  # noqa: E402

YARD = 7.5  # the patch covers -YARD..YARD in x and z
WALL_INSIDE = 7.85  # inner faces of the walls: the character's centre stops 0.35 m before them
WALL_THICKNESS = 0.4
WALL_HEIGHT = 2.4
CELL = 0.5
# keeps the character's centre off the faces of the crates and of the platform's sides
MARGIN = 0.5

PLATFORM = (2.0, WALL_INSIDE, 2.0, WALL_INSIDE)  # x0, x1, z0, z1
PLATFORM_HEIGHT = 0.8
RAMP = (3.5, 5.0, -1.0, 2.0)  # rises along +z to the platform: 15 degrees, a walk, not stairs
CRATES = [(-4.0, -3.0), (-4.5, 3.5), (2.5, -4.5), (-1.5, 5.5)]  # 1 m crates, centres
CRATE = 1.0

GROUND, WOOD = 0, 1  # patch materials


# ---------------------------------------------------------------- textures

def cobbles():
    rng = random.Random(7)
    image = Image.new("RGB", (256, 256), (78, 74, 68))
    draw = ImageDraw.Draw(image)
    rows = 8
    for row in range(rows):
        y = row * 32
        x = -rng.randint(0, 24)
        while x < 256:
            w = rng.randint(26, 40)
            shade = rng.randint(118, 150)
            color = (shade, shade - 6, shade - 14)
            for dx in (-256, 0, 256):  # wrap around so the tile repeats seamlessly
                draw.rounded_rectangle([x + dx + 2, y + 2, x + dx + w - 2, y + 30], radius=7, fill=color)
            x += w
    return blend_noise(image.filter(ImageFilter.GaussianBlur(0.8)), 20)


def blend_noise(image, amount):
    noise = Image.effect_noise(image.size, amount).convert("RGB")
    return Image.blend(image, Image.composite(image, noise, Image.new("L", image.size, 200)), 0.5)


def stone_wall():
    rng = random.Random(3)
    image = Image.new("RGB", (256, 256), (96, 90, 80))
    draw = ImageDraw.Draw(image)
    for row in range(8):
        y = row * 32
        offset = 0 if row % 2 == 0 else 32
        for col in range(-1, 4):
            x = col * 64 + offset
            shade = rng.randint(150, 182)
            draw.rectangle([x + 2, y + 2, x + 62, y + 30], fill=(shade, shade - 8, shade - 22))
    return blend_noise(image.filter(ImageFilter.GaussianBlur(0.6)), 24)


def planks(border=False):
    rng = random.Random(5)
    image = Image.new("RGB", (256, 256), (70, 46, 26))
    draw = ImageDraw.Draw(image)
    for i in range(8):
        shade = rng.randint(120, 150)
        draw.rectangle([0, i * 32 + 2, 255, i * 32 + 30], fill=(shade, int(shade * 0.72), int(shade * 0.45)))
        for _ in range(10):  # grain
            y = i * 32 + rng.randint(4, 28)
            x = rng.randint(0, 200)
            draw.line([x, y, x + rng.randint(30, 90), y], fill=(shade - 22, int(shade * 0.6), int(shade * 0.36)))
    if border:  # the frame of a crate face
        draw.rectangle([0, 0, 255, 255], outline=(64, 42, 22), width=22)
        draw.line([0, 0, 255, 255], fill=(96, 66, 36), width=20)
    return blend_noise(image.filter(ImageFilter.GaussianBlur(0.5)), 16)


# ---------------------------------------------------------------- yard model

def quad_uv(mesh, corners, normal, tile):
    """A quad with texture coordinates from world position: x, z on floors, the horizontal axis
    and the height on walls, so the texture keeps its scale and stays upright everywhere."""
    ids = []
    for p in corners:
        if abs(normal[1]) > 0.5:
            uv = (p[0] / tile, p[2] / tile)
        else:
            uv = ((p[2] if abs(normal[0]) > 0.5 else p[0]) / tile, -p[1] / tile)
        ids.append(mesh.vertex(p, normal, uv))
    mesh.quad(*ids)


def box(mesh, lo, hi, tile, faces="+x-x+y+z-z"):
    """Faces of an axis-aligned box (no bottom by default)."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    sides = {
        "+x": ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0)),
        "-x": ([(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)], (-1, 0, 0)),
        "+y": ([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0, 1, 0)),
        "+z": ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
        "-z": ([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], (0, 0, -1)),
    }
    for i in range(0, len(faces), 2):
        corners, normal = sides[faces[i:i + 2]]
        quad_uv(mesh, corners, normal, tile)


def crate_box(mesh, cx, cz, size, y0=0.0):
    """A crate with the whole texture on every face."""
    h = size / 2
    x0, x1, z0, z1, y1 = cx - h, cx + h, cz - h, cz + h, y0 + size
    faces = [
        ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0)),
        ([(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)], (-1, 0, 0)),
        ([(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)], (0, 1, 0)),
        ([(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)], (0, 0, 1)),
        ([(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)], (0, 0, -1)),
    ]
    for corners, normal in faces:
        uvs = [(0, 1), (0, 0), (1, 0), (1, 1)]
        mesh.quad(*[mesh.vertex(p, normal, uv) for p, uv in zip(corners, uvs)])


def yard_meshes():
    ground = Mesh("ground", "cobbles.tga")
    # a grid instead of one quad: per-vertex lighting and fog need vertices across the surface
    cells = 16
    index = {}
    for j in range(cells + 1):
        for i in range(cells + 1):
            x = (i / cells - 0.5) * 2 * WALL_INSIDE
            z = (j / cells - 0.5) * 2 * WALL_INSIDE
            index[i, j] = ground.vertex((x, 0.0, z), (0, 1, 0), (x / 2.0, z / 2.0))
    for j in range(cells):
        for i in range(cells):
            ground.quad(index[i, j], index[i + 1, j], index[i + 1, j + 1], index[i, j + 1])

    walls = Mesh("walls", "stone_wall.tga")
    a, b, h = WALL_INSIDE, WALL_INSIDE + WALL_THICKNESS, WALL_HEIGHT
    box(walls, (-b, 0, a), (b, h, b), 2.0)
    box(walls, (-b, 0, -b), (b, h, -a), 2.0)
    box(walls, (a, 0, -a), (b, h, a), 2.0, faces="+x-x+y")
    box(walls, (-b, 0, -a), (-a, h, a), 2.0, faces="+x-x+y")

    deck = Mesh("deck", "planks.tga")
    sides = Mesh("platform_sides", "stone_wall.tga")
    x0, x1, z0, z1 = PLATFORM
    box(deck, (x0, 0, z0), (x1, PLATFORM_HEIGHT, z1), 2.0, faces="+y")
    box(sides, (x0, 0, z0), (x1, PLATFORM_HEIGHT, z1), 2.0, faces="-x-z")
    # the ramp: a sloped deck and two triangular sides
    rx0, rx1, rz0, rz1 = RAMP
    slope = (0.0, rz1 - rz0, -PLATFORM_HEIGHT)
    length = math.hypot(slope[1], slope[2])
    normal = (0.0, slope[1] / length, slope[2] / length)
    corners = [(rx0, 0.0, rz0), (rx1, 0.0, rz0), (rx1, PLATFORM_HEIGHT, rz1), (rx0, PLATFORM_HEIGHT, rz1)]
    deck.quad(*[deck.vertex(p, normal, (p[0] / 2.0, (p[2] - rz0) * length / (rz1 - rz0) / 2.0)) for p in corners])
    for x, nx in ((rx0, -1.0), (rx1, 1.0)):
        tri = [(x, 0.0, rz0), (x, 0.0, rz1), (x, PLATFORM_HEIGHT, rz1)]
        sides.triangle(*[sides.vertex(p, (nx, 0, 0), (p[2] / 2.0, -p[1] / 2.0)) for p in tri])

    crates = Mesh("crates", "crate.tga")
    for cx, cz in CRATES:
        crate_box(crates, cx, cz, CRATE)
    return [ground, walls, deck, sides, crates]


# ---------------------------------------------------------------- character patch

def inside(x, z, rect, grow=0.0):
    x0, x1, z0, z1 = rect
    return x0 - grow < x < x1 + grow and z0 - grow < z < z1 + grow


def cell_kind(cx, cz):
    """What the cell centred at (cx, cz) is: "platform", "ramp", "ground" or None for no floor."""
    if inside(cx, cz, PLATFORM):
        return "platform"
    if inside(cx, cz, RAMP):
        return "ramp"
    if inside(cx, cz, PLATFORM, MARGIN):
        return None
    rx0, rx1, rz0, rz1 = RAMP
    if inside(cx, cz, (rx0 - MARGIN, rx1 + MARGIN, rz0, rz1)):
        return None
    for x, z in CRATES:
        if abs(cx - x) < CRATE / 2 + MARGIN and abs(cz - z) < CRATE / 2 + MARGIN:
            return None
    return "ground"


def floor_height(kind, z):
    if kind == "platform":
        return PLATFORM_HEIGHT
    if kind == "ramp":
        rz0, rz1 = RAMP[2], RAMP[3]
        return PLATFORM_HEIGHT * (z - rz0) / (rz1 - rz0)
    return 0.0


def patch_triangles():
    """Two triangles per walkable cell. Neighbouring cells of different heights share no
    vertices, so the drop from the platform is a wall; the ramp shares its ends with the
    ground and the platform deck, so it is the way up."""
    triangles = []
    count = round(2 * YARD / CELL)
    for j in range(count):
        for i in range(count):
            x0 = -YARD + i * CELL
            z0 = -YARD + j * CELL
            kind = cell_kind(x0 + CELL / 2, z0 + CELL / 2)
            if kind is None:
                continue
            corners = [(x, floor_height(kind, z), z) for x, z in
                       ((x0, z0), (x0 + CELL, z0), (x0 + CELL, z0 + CELL), (x0, z0 + CELL))]
            material = GROUND if kind == "ground" else WOOD
            triangles.append(((corners[0], corners[1], corners[2]), material))
            triangles.append(((corners[0], corners[2], corners[3]), material))
    return triangles


# ---------------------------------------------------------------- character

def write_ani(path):
    """The sailor's actions under the names Character looks for (location/src/character.cpp).

    "Walk speed" is how fast the character moves while the action plays; it matches the
    stride, so the feet do not slide; the character moves along its facing by this speed, so
    walking back has it negative and plays the walk backwards (type = reverse).
    """
    text = "; the sailor as a location character; times are frames of sailor.an\nanimation = sailor.an\n"

    def section(name, cycle, extra=""):
        nonlocal text
        text += f"\n[{name}]\nstart_time = {cycle.start}\nend_time = {cycle.end}\nloop = true\n{extra}"

    def speed(cycle, sign=1):
        return f'data = "Walk speed", "{sign * cycle.speed:.3f}"\n'

    section("idle_1", sailor.IDLE)
    section("walk", sailor.WALK, speed(sailor.WALK))
    section("back walk", sailor.WALK, "type = reverse\n" + speed(sailor.WALK, -1))
    section("run", sailor.RUN, speed(sailor.RUN))
    section("back run", sailor.RUN, "type = reverse\n" + speed(sailor.RUN, -1))
    section("turn left", sailor.TURN)
    section("turn right", sailor.TURN)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="ascii")


def main():
    resource = Path(sys.argv[1])
    textures = resource / "textures" / "yard"
    write_tx(cobbles(), textures / "cobbles.tga.tx")
    write_tx(stone_wall(), textures / "stone_wall.tga.tx")
    write_tx(planks(), textures / "planks.tga.tx")
    write_tx(planks(border=True), textures / "crate.tga.tx")
    # with the BSP, traces hit the yard: shadows fall on it, the camera stops at the walls
    write_gm(yard_meshes(), resource / "models" / "yard" / "yard.gm", bsp=True)
    count = write_ptc(patch_triangles(), resource / "models" / "yard" / "yard_patch.ptc",
                      materials=("ground", "wood"))

    # Character loads models\characters\<model> with textures from textures\characters\
    sailor.write_textures(resource / "textures" / "characters")
    cycles = [sailor.IDLE, sailor.WALK, sailor.RUN, sailor.TURN]
    sailor.write_sailor(resource / "models" / "characters" / "sailor.gm", resource / "animation" / "sailor.an",
                        "", cycles)
    write_ani(resource / "animation" / "sailor.ani")
    print(f"patch {count} triangles; walk {sailor.WALK.speed:.3f} m/s, run {sailor.RUN.speed:.3f} m/s")


if __name__ == "__main__":
    main()
