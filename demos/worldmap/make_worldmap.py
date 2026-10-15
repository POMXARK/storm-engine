"""Generates the world map of the worldmap demo: an archipelago of four islands on a 2000 x 2000
sea, the ships that sail it, the clouds, the storm and the interface of the WORLDMAP entity.

Usage: make_worldmap.py <assets dir with PT_Serif-Web-*.ttf> <output resource dir>

The entity (src/libs/worldmap) loads everything by fixed names:
  models/WorldMap/islands/islands.gm   the base model: never drawn, only its labels are read -
                                       "locators" min / max give the size of the world, "islands"
                                       the place of every island (the label name is the model
                                       name), "labels" the points the island names are printed
                                       at, "merchant" the ports merchants sail between
  models/WorldMap/islands/<isle>.gm    the land; the ship hits what stands over y = 0.5 within
                                       its box (GEOM::Clip, so with the BSP)
  models/WorldMap/islands/<isle>_area.gm  a flat disc around the island in world coordinates:
                                       the ship inside sets worldMap.island
  models/WorldMap/{Ship,WindRose}.gm   the player's ship and the compass; encounter ships are
                                       loaded by the names the script sends (merchant, pirate)
  textures/WorldMap/Geometry/*.tga.tx  the textures of these models
  textures/WorldMap/Sea/...            the sea under the whole world, 32 frames of the ripples
                                       added over it, the glints of the sun
  textures/WorldMap/*.tga.tx           clouds, storm clouds, lightnings, rain, gun smoke, the wake
  textures/WorldMap/Interfaces/*       the clock with the sky of the day, the wind, the morale and
                                       food plate, the encounter icons
  textures/fonts, ini/fonts.ini        the log, island names and the date
The models are drawn without lighting (WdmModelDrawStd takes the texture as it is), so the light of
the sun is baked into every texture.
"""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(HERE.parent / "common"))
from storm_formats import Mesh, write_fonts, write_gm, write_tx  # noqa: E402

WORLD = 2000.0  # the side of the world square, centred on (0, 0)
# name, centre x, z, mean coast radius, peak height, seed of the coast and ridges
ISLANDS = [
    ("gull", -430.0, 320.0, 150.0, 34.0, 1),
    ("crag", 400.0, 430.0, 105.0, 52.0, 2),
    ("palm", 290.0, -360.0, 175.0, 22.0, 3),
    ("reef", -360.0, -470.0, 70.0, 12.0, 4),
]
AREA = 1.7  # the island area is this many coast radii round its centre
LAND_STEP = 5.0
ISLAND_TEXTURE = 512
SEA_TEXTURE = 512
WAVE_FRAMES = 32
WAVE_SIZE = 128
SUN = np.array([-0.45, 0.75, 0.48])  # the sun of the map shines from the north west
SUN = SUN / np.linalg.norm(SUN)
# merchant ports: off the shores, where traders sail between
MERCHANT = [(-250.0, 260.0), (270.0, 380.0), (150.0, -260.0), (-300.0, -360.0), (0.0, 0.0)]
FONTS = {
    "demo_log": ("PT_Serif-Web-Regular.ttf", 20, 1),
    "wdm_label": ("PT_Serif-Web-Bold.ttf", 20, 2),
    "wdm_date": ("PT_Serif-Web-Bold.ttf", 15, 1),
}


def rng_for(seed):
    return np.random.default_rng(seed)


def smooth_noise(rng, size, cells):
    noise = rng.random((cells, cells)).astype(np.float32)
    return np.asarray(Image.fromarray(noise, "F").resize((size, size), Image.BICUBIC))


def blend(a, b, t):
    t = np.clip(t, 0.0, 1.0)[..., None]
    return np.asarray(a, float) * (1 - t) + np.asarray(b, float) * t


# ---------------------------------------------------------------- islands

class Island:
    def __init__(self, name, x, z, radius, peak, seed):
        self.name, self.x, self.z, self.radius, self.peak = name, x, z, radius, peak
        rng = rng_for(seed)
        self.phases = rng.uniform(0, 2 * math.pi, 6)
        self.extent = radius * 1.4

    def coast(self, angle):
        p = self.phases
        return self.radius * (1.0 + 0.18 * np.sin(2 * angle + p[0]) + 0.1 * np.sin(3 * angle + p[1])
                              + 0.06 * np.sin(5 * angle + p[2]))

    def height(self, x, z):
        """Height over the sea in local coordinates; below zero it goes on as the sea floor."""
        x = np.asarray(x, float)
        z = np.asarray(z, float)
        angle = np.arctan2(z, x)
        s = np.hypot(x, z) / self.coast(angle)
        p = self.phases
        ridges = 1.0 + 0.15 * np.sin(4 * angle + 3 * s + p[3]) + 0.08 * np.sin(9 * angle - 5 * s + p[4])
        land = self.peak * np.clip(1.0 - s, 0.0, 1.0) ** 1.3 * ridges
        sea = -18.0 * np.clip((s - 1.0) / 0.55, 0.0, 1.0) ** 0.8
        return np.where(s <= 1.0, land + 0.8, sea + 0.8 * np.clip(1 - (s - 1) * 20, 0, 1))

    def normals(self, x, z, eps=0.5):
        dhx = (self.height(x + eps, z) - self.height(x - eps, z)) / (2 * eps)
        dhz = (self.height(x, z + eps) - self.height(x, z - eps)) / (2 * eps)
        n = np.stack([-dhx, np.ones_like(dhx), -dhz], axis=-1)
        return n / np.linalg.norm(n, axis=-1, keepdims=True)

    def uv(self, x, z):
        e = self.extent
        return ((x + e) / (2 * e), (e - z) / (2 * e))

    def mesh(self):
        """The land as a height field cut along the coast: every cell is two triangles, and the
        ones the coast crosses are clipped at y = 0, so the beach meets the sea in a smooth line
        and nothing of the model is under the sea drawn round it."""
        e, step = self.extent, LAND_STEP
        n = int(round(2 * e / step)) + 1
        xs = np.linspace(-e, e, n)
        gx, gz = np.meshgrid(xs, xs)
        gh = self.height(gx, gz)
        mesh = Mesh(self.name, f"isl_{self.name}.tga")
        index = {}

        def vertex(x, z, y):
            key = (round(x, 3), round(z, 3))
            if key not in index:
                normal = tuple(self.normals(np.array(x), np.array(z)).tolist())
                index[key] = mesh.vertex((x, y, z), normal, self.uv(x, z))
            return index[key]

        def clip(points):
            """The part of a triangle (x, z, h) above the sea, as a polygon."""
            out = []
            for k, a in enumerate(points):
                b = points[(k + 1) % len(points)]
                if a[2] >= 0.0:
                    out.append(a)
                if (a[2] >= 0.0) != (b[2] >= 0.0):
                    t = a[2] / (a[2] - b[2])
                    out.append((a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]), 0.0))
            return out

        for j in range(n - 1):
            for i in range(n - 1):
                c = [(gx[b, a], gz[b, a], float(gh[b, a])) for a, b in ((i, j), (i, j + 1), (i + 1, j + 1), (i + 1, j))]
                if max(p[2] for p in c) <= 0.0:
                    continue
                for tri in ((c[0], c[1], c[2]), (c[0], c[2], c[3])):
                    poly = [vertex(x, z, h) for x, z, h in clip(list(tri))]
                    for k in range(1, len(poly) - 1):
                        if len({poly[0], poly[k], poly[k + 1]}) == 3:
                            mesh.triangle(poly[0], poly[k], poly[k + 1])
        return mesh

    def texture(self, rng):
        size = ISLAND_TEXTURE
        e = self.extent
        xs = -e + (np.arange(size) + 0.5) * 2 * e / size
        gx, gz = np.meshgrid(xs, xs[::-1])  # row 0 is the north edge (v = 0 at z = +extent)
        h = self.height(gx, gz)
        n = self.normals(gx, gz)
        slope = 1.0 - n[..., 1]
        noise = sum(smooth_noise(rng, size, c) * w for c, w in ((12, 0.5), (48, 0.3), (192, 0.2)))
        top = self.peak + 0.8
        color = blend([150, 200, 190], [222, 204, 150], (h - 0.2) / 0.6)  # the surf line and the sand
        green = blend([62, 112, 48], [118, 128, 64], noise * 1.6 - 0.4)
        color = blend(color, green, (h - 2.0 - 2.0 * noise) / 2.0)
        forest = blend([34, 82, 36], [48, 96, 40], noise)
        color = blend(color, forest, (h / top - 0.3 - 0.4 * noise) * 4.0)
        color = blend(color, [124, 114, 100], (slope - 0.35) / 0.15 + (h / top - 0.75) * 6.0)
        color *= (0.85 + 0.3 * noise)[..., None]
        light = np.clip(n @ SUN, 0.0, 1.0)
        color *= (0.5 + 0.65 * light)[..., None]
        return Image.fromarray(np.clip(color, 0, 255).astype(np.uint8), "RGB")

    def area(self, segments=32):
        """A flat disc round the island in world coordinates (the area model is not moved)."""
        mesh = Mesh(self.name + "_area", f"isl_{self.name}.tga")
        r = self.radius * AREA
        centre = mesh.vertex((self.x, 0.0, self.z), (0, 1, 0), (0.5, 0.5))
        ring = [mesh.vertex((self.x + r * math.cos(a), 0.0, self.z + r * math.sin(a)), (0, 1, 0), (0.5, 0.5))
                for a in (2 * math.pi * k / segments for k in range(segments))]
        for k in range(segments):
            mesh.triangle(centre, ring[k], ring[(k + 1) % segments])
        return mesh


def base_model(islands, path):
    """Never drawn: a speck under the sea that carries the labels of the world."""
    mesh = Mesh("base", "isl_gull.tga")
    a = mesh.vertex((0, -50, 0), (0, 1, 0), (0, 0))
    b = mesh.vertex((1, -50, 0), (0, 1, 0), (0, 0))
    c = mesh.vertex((0, -50, 1), (0, 1, 0), (0, 0))
    mesh.triangle(a, b, c)
    half = WORLD / 2
    labels = [("locators", "min", (-half, 0.0, -half)), ("locators", "max", (half, 0.0, half))]
    for isl in islands:
        labels.append(("islands", isl.name, (isl.x, 0.0, isl.z)))
        labels.append(("labels", isl.name, (isl.x, isl.peak + 12.0, isl.z)))
    for k, (x, z) in enumerate(MERCHANT):
        labels.append(("merchant", f"port{k}", (x, 0.0, z)))
    write_gm([mesh], path, labels=labels)


# ---------------------------------------------------------------- ships

def palette(cells):
    """A texture of flat colour cells, each in four shades of the baked light: column = colour,
    row = shade. Returns the image and uv(colour, shade) of a cell centre."""
    cell = 16
    image = Image.new("RGB", (cell * len(cells), cell * 4))
    draw = ImageDraw.Draw(image)
    for i, rgb in enumerate(cells):
        for s, k in enumerate((1.0, 0.8, 0.6, 0.42)):
            draw.rectangle((i * cell, s * cell, i * cell + cell - 1, s * cell + cell - 1),
                           fill=tuple(int(c * k) for c in rgb))

    def uv(colour, shade):
        return ((colour + 0.5) / len(cells), (shade + 0.5) / 4)
    return image, uv


def shade_of(normal):
    light = float(np.dot(np.asarray(normal) / np.linalg.norm(normal), SUN))
    return 0 if light > 0.6 else 1 if light > 0.25 else 2 if light > -0.2 else 3


def face(mesh, uv, colour, points, double=False):
    """A flat polygon (a fan from the first point) in one colour cell, shaded by its normal."""
    p = [np.asarray(q, float) for q in points]
    n = np.cross(p[1] - p[0], p[2] - p[0])
    n = n / (np.linalg.norm(n) or 1.0)
    sides = [n, -n] if double else [n]
    for nn in sides:
        coords = uv(colour, shade_of(nn))
        ids = [mesh.vertex(tuple(q), tuple(nn), coords) for q in p]
        for k in range(1, len(ids) - 1):
            mesh.triangle(ids[0], ids[k], ids[k + 1])


HULL, DECK, SAIL, MAST, TRIM = range(5)


def ship_model(path, texture, sails, length=20.0, masts=2):
    """A low hull with a deck, masts and square sails; the bow looks along +z (WdmShip moves the
    model along its z axis), the size gives the collision box (half length = box z / 4)."""
    image, uv = palette([(96, 64, 38), (176, 140, 92), sails, (70, 50, 32), (150, 40, 32)])
    write_tx(image, texture.parent / (texture.name + ".tx"), mip_chain=False)
    mesh = Mesh("ship", texture.name)
    half, beam, top, keel = length / 2, length * 0.17, 1.6, -0.6
    # the deck outline from the stern to the pointed bow
    outline = [(-beam * 0.8, -half), (beam * 0.8, -half), (beam, -half * 0.3), (beam * 0.9, half * 0.45),
               (0.0, half), (-beam * 0.9, half * 0.45), (-beam, -half * 0.3)]
    face(mesh, uv, DECK, [(x, top, z) for x, z in outline][::-1])
    for k in range(len(outline)):
        (x0, z0), (x1, z1) = outline[k], outline[(k + 1) % len(outline)]
        face(mesh, uv, HULL, [(x0, top, z0), (x1, top, z1), (x1 * 0.7, keel, z1 * 0.95), (x0 * 0.7, keel, z0 * 0.95)])
        # a stripe of paint along the side under the rail
        face(mesh, uv, TRIM, [(x0, top + 0.01, z0), (x1, top + 0.01, z1), (x1 * 0.97, top - 0.4, z1),
                              (x0 * 0.97, top - 0.4, z0)])
    spots = [-half * 0.3, half * 0.3] if masts == 2 else [0.0]
    for z in spots:
        w = 0.35
        for a, b in (((-w, -w), (w, -w)), ((w, -w), (w, w)), ((w, w), (-w, w)), ((-w, w), (-w, -w))):
            face(mesh, uv, MAST, [(a[0], top, z + a[1]), (b[0], top, z + b[1]), (b[0], top + 14, z + b[1]),
                                  (a[0], top + 14, z + a[1])])
        # two square sails bellied forward, a yard over each
        for y0, y1, w0 in ((4.0, 8.5, beam * 1.25), (9.0, 12.5, beam)):
            face(mesh, uv, SAIL, [(-w0, y0, z + 0.9), (w0, y0, z + 0.9), (w0 * 0.9, y1, z + 0.5),
                                  (-w0 * 0.9, y1, z + 0.5)], double=True)
    # the jib from the foremast to the bow
    face(mesh, uv, SAIL, [(0.0, top + 12, spots[-1]), (0.0, top + 1.5, half), (0.0, top + 3, spots[-1] + 1.0)],
         double=True)
    write_gm([mesh], path)


def wind_rose(path, texture):
    """The compass: an eight-pointed star the entity draws in the corner, 0.028 of its size."""
    image, uv = palette([(232, 220, 190), (150, 120, 80), (190, 50, 40)])
    write_tx(image, texture.parent / (texture.name + ".tx"), mip_chain=False)
    mesh = Mesh("rose", texture.name)
    for k in range(8):
        a = math.pi / 2 - k * math.pi / 4  # k = 0 is the north (+z)
        r = 190.0 if k % 2 == 0 else 110.0
        tip = (r * math.cos(a), 0.0, r * math.sin(a))
        for side in (-1, 1):
            b = a + side * math.pi / 8
            shoulder = (40 * math.cos(b), 0.0, 40 * math.sin(b))
            colour = 2 if k == 0 else (0 if side > 0 else 1)
            face(mesh, uv, colour, [(0.0, 12.0, 0.0), tip, shoulder] if side > 0 else
                 [(0.0, 12.0, 0.0), shoulder, tip])
    write_gm([mesh], path)


# ---------------------------------------------------------------- the sea

def sea_texture(islands, rng):
    """The sea over the world square (u along x, v from +z down): deep water, lighter over the
    banks round the islands."""
    size = SEA_TEXTURE
    xs = (np.arange(size) + 0.5) / size * WORLD - WORLD / 2
    gx, gz = np.meshgrid(xs, -xs)
    depth = np.full(gx.shape, -60.0)
    for isl in islands:
        depth = np.maximum(depth, isl.height(gx - isl.x, gz - isl.z))
    noise = sum(smooth_noise(rng, size, c) * w for c, w in ((8, 0.6), (32, 0.4)))
    deep = blend([20, 52, 92], [26, 66, 108], noise)
    color = blend(deep, [36, 120, 140], (depth + 20.0) / 14.0)
    color = blend(color, [70, 168, 170], (depth + 6.0) / 6.0)
    return Image.fromarray(np.clip(color, 0, 255).astype(np.uint8), "RGB")


def wave_frames():
    """Ripples added over the sea: tileable sines with whole frequencies, a full cycle in 32
    frames; mostly black (adds nothing), light on the crests."""
    size = WAVE_SIZE
    y, x = np.mgrid[0:size, 0:size] / size * 2 * np.pi
    waves = [(5, 2, 0.0), (-3, 6, 1.3), (7, -4, 2.1), (2, 9, 0.7), (9, 3, 4.0), (-6, -7, 5.2)]
    frames = []
    for f in range(WAVE_FRAMES):
        t = 2 * np.pi * f / WAVE_FRAMES
        v = sum(np.sin(kx * x + ky * y + ph + t * (1 + (i % 2))) for i, (kx, ky, ph) in enumerate(waves))
        crest = np.clip((v - 2.0) / 3.0, 0.0, 1.0) ** 2.0
        c = (crest[..., None] * np.array([90, 120, 130])).astype(np.uint8)
        frames.append(Image.fromarray(c, "RGB"))
    return frames


def soft_spot(size, color, power=2.0, alpha=True):
    y, x = (np.mgrid[0:size, 0:size] + 0.5) / size * 2 - 1
    k = np.clip(1 - np.hypot(x, y), 0, 1) ** power
    if alpha:
        rgba = np.dstack([np.full_like(k, c) for c in color] + [k * 255])
        return Image.fromarray(rgba.astype(np.uint8), "RGBA")
    return Image.fromarray((k[..., None] * np.array(color)).astype(np.uint8), "RGB")


def flare(size=32):
    image = soft_spot(size, (255, 250, 230), 3.0)
    draw = ImageDraw.Draw(image)
    c = size / 2
    draw.line((c, 2, c, size - 2), fill=(255, 255, 240, 200))
    draw.line((2, c, size - 2, c), fill=(255, 255, 240, 200))
    return image.filter(ImageFilter.GaussianBlur(0.6))


# ---------------------------------------------------------------- weather

def cloud(rng, size, tint):
    """A heap of soft puffs, white with alpha, the bottom shaded."""
    k = np.zeros((size, size))
    yy, xx = (np.mgrid[0:size, 0:size] + 0.5) / size
    for _ in range(14):
        cx, cy = rng.uniform(0.25, 0.75), rng.uniform(0.35, 0.65)
        r = rng.uniform(0.1, 0.22)
        k = np.maximum(k, np.clip(1 - np.hypot(xx - cx, (yy - cy) * 1.3) / r, 0, 1) ** 0.8)
    shade = 1.0 - 0.35 * np.clip((yy - 0.4) / 0.3, 0, 1)
    rgb = np.array(tint)[None, None, :] * shade[..., None]
    return np.dstack([rgb, k * 160]).astype(np.uint8)


def atlas2x2(tiles):
    s = tiles[0].shape[0]
    out = np.zeros((2 * s, 2 * s, 4), np.uint8)
    for i, t in enumerate(tiles):
        out[(i // 2) * s:(i // 2 + 1) * s, (i % 2) * s:(i % 2 + 1) * s] = t
    return Image.fromarray(out, "RGBA")


def light_ramp(low, high, size=64):
    """The light the cloud technique multiplies by, read along the view angle: dim to bright."""
    t = np.linspace(0, 1, size)
    rgb = np.array(low)[None, :] * (1 - t[:, None]) + np.array(high)[None, :] * t[:, None]
    row = np.repeat(rgb[None, :, :], size, axis=0)
    return Image.fromarray(np.dstack([row, np.full((size, size), 255)]).astype(np.uint8), "RGBA")


def lightnings(rng, size=64):
    """Four bolts side by side, the alpha comes from the vertex colour."""
    image = Image.new("RGB", (size * 4, size * 4))
    draw = ImageDraw.Draw(image)
    for f in range(4):
        x, y = f * size + size / 2, 0.0
        while y < size * 4:
            nx, ny = x + rng.uniform(-size * 0.15, size * 0.15), y + rng.uniform(size * 0.2, size * 0.4)
            nx = min(max(nx, f * size + 4), f * size + size - 4)
            draw.line((x, y, nx, ny), fill=(220, 230, 255), width=3)
            x, y = nx, ny
    return image.filter(ImageFilter.GaussianBlur(1.0))


def rain(rng, size=64):
    image = Image.new("RGBA", (size, size), (200, 210, 230, 0))
    draw = ImageDraw.Draw(image)
    for _ in range(40):
        x, y = rng.uniform(0, size), rng.uniform(0, size)
        draw.line((x, y, x - 2, y + 10), fill=(200, 210, 230, int(rng.uniform(80, 180))), width=1)
    return image


# ---------------------------------------------------------------- interface

def ring_alpha(size, cx, cy, r0, r1):
    y, x = np.mgrid[0:size[1], 0:size[0]] + 0.5
    d = np.hypot(x - cx, y - cy)
    return np.clip(np.minimum(d - r0, r1 - d) + 0.5, 0, 1)


def sky_strip(rng, width=512, height=128):
    """The sky of a whole day from midnight to midnight: the clock shows a quarter of it centred
    on the hour, the sun crossing at noon, the moon at midnight."""
    hours = (np.arange(width) + 0.5) / width * 24
    day = np.clip((np.cos((hours - 12) / 24 * 2 * np.pi) + 0.25) / 0.6, 0, 1)
    dawn = np.exp(-((hours - 6) / 1.2) ** 2) + np.exp(-((hours - 19) / 1.2) ** 2)
    v = (np.arange(height) + 0.5) / height
    top = blend([12, 18, 50], [70, 130, 210], day)
    bottom = blend([28, 34, 70], [170, 210, 240], day)
    bottom = blend(bottom, [240, 150, 80], dawn * 0.8)
    color = top[None, :, :] * (1 - v[:, None, None]) + bottom[None, :, :] * v[:, None, None]
    image = Image.fromarray(np.clip(color, 0, 255).astype(np.uint8), "RGB")
    draw = ImageDraw.Draw(image)
    for _ in range(60):  # stars in the night half
        x = rng.uniform(0, width)
        if 0.25 * width < x < 0.75 * width:
            continue
        y = rng.uniform(0, height * 0.7)
        draw.point((x, y), fill=(230, 230, 255))
    for cx, r, fill in ((width / 2, 14, (255, 236, 150)), (6, 9, (225, 225, 235)), (width - 6, 9, (225, 225, 235))):
        draw.ellipse((cx - r, height * 0.4 - r, cx + r, height * 0.4 + r), fill=fill)
    return image


def mask(size, alpha):
    return Image.fromarray(np.dstack([np.full(alpha.shape + (3,), 255), alpha * 255]).astype(np.uint8), "RGBA")


def wind_pointer(width=32, height=128):
    """The arrow turned by the wind round the centre of the clock, its head up."""
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.polygon([(width / 2, 4), (width - 4, 26), (width / 2 + 3, 22), (width / 2 + 3, height / 2),
                  (width / 2 - 3, height / 2), (width / 2 - 3, 22), (4, 26)], fill=(250, 240, 210, 255),
                 outline=(60, 40, 20, 255))
    return image


def wind_bar(size=128):
    """The wind force: a half ring under the clock the mask opens from the left as it blows."""
    alpha = ring_alpha((size, size), size / 2, 0, 50, 62)
    x = (np.arange(size) + 0.5) / size
    rgb = blend([90, 200, 120], [230, 80, 60], x)[None, :, :].repeat(size, axis=0)
    return Image.fromarray(np.dstack([rgb, alpha * 255]).astype(np.uint8), "RGBA")


def wind_bar_mask(size=128):
    x = (np.arange(size) + 0.5) / size
    alpha = np.repeat((x < 0.5)[None, :].astype(float), size, axis=0)
    return mask((size, size), alpha)


def frame(width=128, height=256):
    """The rim of the clock (the sky circle at y 64..192) and the date plate under it."""
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse((2, 66, width - 3, height - 63), outline=(70, 46, 24, 255), width=5)
    draw.ellipse((5, 69, width - 6, height - 66), outline=(200, 160, 90, 255), width=2)
    draw.rounded_rectangle((10, 212, width - 11, 240), 6, fill=(40, 28, 16, 200), outline=(200, 160, 90, 255))
    return image


def morale_plate(width=128, height=64):
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((8, 8, width - 9, 26), 6, outline=(200, 160, 90, 255), width=2)
    return image


def morale_bar(width=128, height=64):
    """Slid sideways by the morale: red on the left, green on the right."""
    x = (np.arange(width) + 0.5) / width
    rgb = blend([200, 60, 40], [80, 190, 90], (x - 0.3) / 0.4)[None, :, :].repeat(height, axis=0)
    return Image.fromarray(np.dstack([rgb, np.full((height, width), 255)]).astype(np.uint8), "RGBA")


def morale_mask(width=128, height=64):
    alpha = np.zeros((height, width))
    alpha[11:24, 11:width - 11] = 1.0
    return mask((width, height), alpha)


def plate(width=128, height=64):
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    ImageDraw.Draw(image).rounded_rectangle((4, 8, width - 5, height - 9), 8, fill=(40, 28, 16, 200),
                                            outline=(200, 160, 90, 255), width=2)
    return image


def ensigns(cell=64):
    colours = [(200, 40, 40), (40, 70, 170), (230, 230, 230), (30, 30, 30)]
    image = Image.new("RGBA", (cell * len(colours), cell), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    for i, c in enumerate(colours):
        draw.rectangle((i * cell + 6, 14, i * cell + cell - 7, cell - 15), fill=c + (255,), outline=(20, 20, 20, 255))
    return image


def big_icons(font_path, cell=64):
    """The encounter icon in the corner: unknown, merchant, warring, pursuer, then spares."""
    image = Image.new("RGBA", (cell * 8, cell), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype(str(font_path), 36)
    for i, (mark, colour) in enumerate([("?", (220, 220, 220)), ("$", (240, 200, 90)), ("X", (240, 140, 60)),
                                        ("!", (230, 60, 50)), ("~", (120, 170, 230)), ("", 0), ("", 0), ("", 0)]):
        if not mark:
            continue
        x0 = i * cell
        draw.ellipse((x0 + 4, 4, x0 + cell - 5, cell - 5), fill=(30, 22, 14, 210), outline=colour + (255,), width=3)
        draw.text((x0 + cell / 2, cell / 2), mark, font=font, fill=colour + (255,), anchor="mm")
    return image


# ---------------------------------------------------------------- main

def main():
    assets, resource = Path(sys.argv[1]), Path(sys.argv[2])
    models = resource / "models" / "WorldMap"
    tex = resource / "textures" / "WorldMap"
    geometry = tex / "Geometry"
    rng = rng_for(7)

    islands = [Island(*spec) for spec in ISLANDS]
    for isl in islands:
        write_tx(isl.texture(rng), geometry / f"isl_{isl.name}.tga.tx")
        write_gm([isl.mesh()], models / "islands" / f"{isl.name}.gm", bsp=True)
        write_gm([isl.area()], models / "islands" / f"{isl.name}_area.gm", bsp=True)
    base_model(islands, models / "islands" / "islands.gm")

    ship_model(models / "Ship.gm", geometry / "ship_player.tga", (236, 228, 205))
    ship_model(models / "merchant.gm", geometry / "ship_merchant.tga", (200, 170, 120), length=22.0, masts=1)
    ship_model(models / "pirate.gm", geometry / "ship_pirate.tga", (52, 48, 46))
    wind_rose(models / "WindRose.gm", geometry / "wind_rose.tga")
    write_tx(soft_spot(64, (40, 40, 40), 1.0, alpha=False).point(lambda c: 255 - c), geometry / "compass_shadow.tga.tx")

    write_tx(sea_texture(islands, rng), tex / "Sea" / "sea.tga.tx")
    for f, image in enumerate(wave_frames()):
        write_tx(image, tex / "Sea" / "Ani" / f"wave_{f:04d}.tga.tx")
    write_tx(flare(), tex / "Sea" / "flare.tga.tx")

    write_tx(atlas2x2([cloud(rng, 128, (250, 250, 250)) for _ in range(4)]), tex / "clouds.tga.tx")
    write_tx(light_ramp((170, 175, 185), (255, 255, 255)), tex / "cloudslight.tga.tx")
    write_tx(light_ramp((60, 62, 72), (150, 152, 165)), tex / "stormlight.tga.tx")
    write_tx(lightnings(rng), tex / "lightnings.tga.tx")
    write_tx(rain(rng), tex / "rain.tga.tx")
    write_tx(soft_spot(64, (90, 90, 90), 1.2), tex / "csmoke.tga.tx")
    write_tx(soft_spot(64, (150, 170, 170), 1.5, alpha=False), tex / "watermark.tga.tx")

    ui = tex / "Interfaces"
    write_tx(sky_strip(rng), ui / "sky.tga.tx")
    write_tx(mask((128, 128), ring_alpha((128, 128), 64, 64, -1, 58)), ui / "sky_mask.tga.tx")
    write_tx(wind_pointer(), ui / "wind_pointer.tga.tx")
    write_tx(wind_bar(), ui / "bar.tga.tx")
    write_tx(wind_bar_mask(), ui / "bar_mask.tga.tx")
    write_tx(frame(), ui / "back.tga.tx")
    write_tx(morale_plate(), ui / "morale.tga.tx")
    write_tx(morale_bar(), ui / "morale_bar.tga.tx")
    write_tx(morale_mask(), ui / "morale_mask.tga.tx")
    write_tx(plate(), ui / "coord.tga.tx")
    write_tx(ensigns(), ui / "WorldMapEnsigns.tga.tx")
    write_tx(big_icons(assets / "PT_Serif-Web-Bold.ttf"), ui / "BigIcons.tga.tx")

    write_fonts({name: (assets / ttf, size, outline) for name, (ttf, size, outline) in FONTS.items()}, resource,
                "; PT Serif (ParaType, SIL Open Font License 1.1), rendered by make_worldmap.py")
    print("world map:", ", ".join(f"{i.name} r{i.radius:.0f}" for i in islands))


if __name__ == "__main__":
    main()
