"""Generates the island of the island demo from one height function.

Usage: make_island.py <output resource dir>
  models/islands/isle/isle.gm         the land and the shallows down to SHALLOW_CUT, with the BSP
                                      the island traces (cameras, cannonballs, the AI path) run on
  models/islands/isle/isle_seabed.gm  the sea floor around it, the "seabed" node the island entity
                                      takes out of the model and draws under the sea
  textures/islands/isle/*.tga.tx      the island with its light baked in, the sand of the sea floor
  foam/islands/isle.tga, isle.ini     the depth map ships, touch and SEA_AI read, its box and the
                                      graph of points SEA_AI steers along around the island
The engine builds the depth map itself when there is none, tracing 2048 x 2048 rays down the model;
here it comes from the same height function, so it matches the model and costs nothing at start.
"""
import math
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(HERE.parent / "common"))
from storm_formats import Mesh, write_gm, write_tx  # noqa: E402

NAME = "isle"
SHORE = 170.0       # mean radius of the coast line, m
PEAK = 46.0         # height of the hill, m
SHELF = 0.7         # the sea floor falls from the coast to FLOOR over this part of the radius
FLOOR = -22.0
LAND_EXTENT = 400.0  # half size of the island model: the land, the shelf and the reef
LAND_STEP = 8.0
SEABED_EXTENT = 640.0  # half size of the sea floor model, and so of the depth map
SEABED_STEP = 20.0
SHALLOW_CUT = -8.0   # the island model keeps what is above, the sea floor model what is below
# the reef: a sand bar a few metres under the water south east of the island
REEF = dict(x=130.0, z=-320.0, length=75.0, width=18.0, angle=math.radians(30), top=-1.5)
MAP_SIZE = 1024
TEXTURE_SIZE = 1024
# island/src/island.cpp: the box of the depth map is the box of the models plus 50 m, a pixel
# holds 2 on land and 2 + 253 * depth / 20 under water, 20 m and deeper is 255
MAP_MARGIN = 50.0
HMAP_START = 2
HMAP_NUMBERS = 253
HMAP_MAXDEPTH = 20.0
# SEA_AI goes round the island along a ring of points, deep enough for a ship between any two
GRAPH_POINTS = 12
GRAPH_RADIUS = 1.9   # times the coast radius in the direction of the point
# the day sun of the sea demo (Weather.Sun.HeightAngle and AzimuthAngle of the demo script)
SUN_HEIGHT, SUN_AZIMUTH = 0.7, 2.618


def coast_radius(angle):
    return SHORE * (1.0 + 0.16 * np.sin(2 * angle + 0.7) + 0.09 * np.sin(3 * angle + 2.1)
                    + 0.05 * np.sin(5 * angle + 0.4))


def height(x, z):
    x = np.asarray(x, float)
    z = np.asarray(z, float)
    angle = np.arctan2(z, x)
    s = np.hypot(x, z) / coast_radius(angle)
    # a hill with a gentle beach: zero slope at the coast, steeper inland; ridges break the dome
    inland = np.clip(1.0 - s, 0.0, 1.0)
    ridges = 1.0 + 0.12 * np.sin(4 * angle + 3 * s) + 0.06 * np.sin(9 * angle - 5 * s)
    land = PEAK * inland ** 1.4 * ridges
    # under water the floor falls along a smooth step to FLOOR and then slowly deeper
    t = np.clip((s - 1.0) / SHELF, 0.0, 1.0)
    sea = FLOOR * t * t * (3 - 2 * t) - 6.0 * np.clip((np.hypot(x, z) - 420.0) / 220.0, 0.0, 1.0)
    h = np.where(s <= 1.0, land, sea)
    # the reef: an ellipse rising to REEF["top"] under the water
    c, sn = math.cos(REEF["angle"]), math.sin(REEF["angle"])
    dx, dz = x - REEF["x"], z - REEF["z"]
    u = (dx * c + dz * sn) / REEF["length"]
    v = (-dx * sn + dz * c) / REEF["width"]
    reef = REEF["top"] - 14.0 * (u * u + v * v)
    return np.maximum(h, reef)


def normals(x, z, eps=0.5):
    dhx = (height(x + eps, z) - height(x - eps, z)) / (2 * eps)
    dhz = (height(x, z + eps) - height(x, z - eps)) / (2 * eps)
    n = np.stack([-dhx, np.ones_like(dhx), -dhz], axis=-1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def grid_mesh(name, texture, extent, step, uv, keep=None, floor=None):
    """A height field over [-extent, extent]^2; keep(heights of a cell) drops cells, floor clamps
    the heights from above (the sea floor stays under the island model)."""
    n = int(round(2 * extent / step)) + 1
    xs = np.linspace(-extent, extent, n)
    gx, gz = np.meshgrid(xs, xs)
    gh = height(gx, gz)
    if floor is not None:
        gh = np.minimum(gh, floor)
    gn = normals(gx, gz)
    mesh = Mesh(name, texture)
    index = {}

    def vertex(i, j):
        if (i, j) not in index:
            x, z = gx[j, i], gz[j, i]
            index[(i, j)] = mesh.vertex((x, gh[j, i], z), tuple(gn[j, i]), uv(x, z))
        return index[(i, j)]

    for j in range(n - 1):
        for i in range(n - 1):
            # clockwise seen from above: D3D culls the other side (x right, z up the screen)
            corners = [(i, j), (i, j + 1), (i + 1, j + 1), (i + 1, j)]
            if keep is not None and not keep(np.array([gh[b, a] for a, b in corners])):
                continue
            mesh.quad(*(vertex(a, b) for a, b in corners))
    return mesh


def sun_vector():
    # weather.cpp turns (1, 0, 0) up by the height angle and then about y by the azimuth
    h, a = SUN_HEIGHT, SUN_AZIMUTH
    return np.array([math.cos(h) * math.cos(a), math.sin(h), -math.cos(h) * math.sin(a)])


def smooth_noise(rng, size, cells):
    noise = rng.random((cells, cells)).astype(np.float32)
    return np.asarray(Image.fromarray(noise, "F").resize((size, size), Image.BICUBIC))


def island_texture(rng):
    # the land seen from above, row j at z, column i at x; the light of the sun is baked in, so
    # the slopes facing away from it stay dark whatever lighting the model gets
    size = TEXTURE_SIZE
    xs = -LAND_EXTENT + (np.arange(size) + 0.5) * 2 * LAND_EXTENT / size
    gx, gz = np.meshgrid(xs, xs)
    h = height(gx, gz)
    n = normals(gx, gz)
    slope = 1.0 - n[..., 1]
    noise = sum(smooth_noise(rng, size, c) * w for c, w in ((16, 0.5), (64, 0.3), (256, 0.2)))

    sand = np.array([214, 196, 150], float)
    wet_sand = np.array([150, 132, 98], float)
    grass = np.array([78, 118, 52], float)
    dry_grass = np.array([128, 132, 70], float)
    rock = np.array([118, 108, 96], float)

    def blend(a, b, t):
        t = np.clip(t, 0.0, 1.0)[..., None]
        return a * (1 - t) + b * t

    color = blend(wet_sand, sand, (h + 1.5) / 2.0)
    green = blend(grass, dry_grass, noise * 1.6 - 0.4)
    color = blend(color, green, (h - 2.5 - 2.0 * noise) / 2.0)
    color = blend(color, rock, (slope - 0.18) / 0.12 + (h - 38.0) / 6.0 * (h > 32))
    # under the water the sand darkens with the depth
    color *= (1.0 - 0.35 * np.clip(-h / 8.0, 0.0, 1.0))[..., None]
    color *= (0.85 + 0.3 * noise)[..., None]

    light = np.clip(n @ sun_vector(), 0.0, 1.0)
    color *= (0.45 + 0.75 * light)[..., None]
    rgb = np.clip(color, 0, 255).astype(np.uint8)
    return Image.fromarray(rgb, "RGB")


def seabed_texture(rng, size=256):
    # rippled sand, tiled over the sea floor
    y, x = np.mgrid[0:size, 0:size] / size * 2 * np.pi
    ripples = 0.5 + 0.5 * np.sin(6 * x + 1.5 * np.sin(2 * y) + np.sin(3 * x + y))
    noise = smooth_noise(rng, size, 32)
    shade = 0.55 + 0.15 * ripples + 0.2 * noise
    color = np.array([150, 132, 98], float) * 0.65 * shade[..., None]
    return Image.fromarray(np.clip(color, 0, 255).astype(np.uint8), "RGB")


def island_meshes():
    def land_uv(x, z):
        return ((x + LAND_EXTENT) / (2 * LAND_EXTENT), (z + LAND_EXTENT) / (2 * LAND_EXTENT))

    land = grid_mesh(NAME, f"islands\\{NAME}\\{NAME}.tga", LAND_EXTENT, LAND_STEP, land_uv,
                     keep=lambda h: h.max() > SHALLOW_CUT)
    seabed = grid_mesh("seabed", f"islands\\{NAME}\\seabed.tga", SEABED_EXTENT, SEABED_STEP,
                       lambda x, z: (x / 40.0, z / 40.0), floor=SHALLOW_CUT - 0.5)
    return land, seabed


def depth_map():
    # island.cpp: the box is the union of the boxes of the model nodes, here the sea floor, grown
    # by MAP_MARGIN; pixel (i, j) lies at x = (i - size / 2) * step from the centre, z likewise
    size = 2 * SEABED_EXTENT + MAP_MARGIN
    step = size / MAP_SIZE
    xs = (np.arange(MAP_SIZE) - MAP_SIZE / 2) * step
    gx, gz = np.meshgrid(xs, xs)
    h = height(gx, gz)
    depth = np.clip(-h, 0.0, HMAP_MAXDEPTH)
    pixels = np.where(h >= 0.0, HMAP_START, HMAP_START + HMAP_NUMBERS * depth / HMAP_MAXDEPTH)
    return np.clip(np.round(pixels), HMAP_START, 255).astype(np.uint8), size


def write_depth_tga(path, pixels):
    # renderer/include/tga.h TGA_H, as ISLAND::SaveTga8 writes it: an 8-bit grey image; the
    # engine reads the pixels straight after the header, row by row from z = -size / 2
    height_, width = pixels.shape
    header = struct.pack("<BBB9xHHBB", 0, 0, 3, width, height_, 8, 8)
    path.write_bytes(header + pixels.tobytes())


def graph_points():
    points = []
    for k in range(GRAPH_POINTS):
        angle = 2 * math.pi * k / GRAPH_POINTS
        r = float(coast_radius(angle)) * GRAPH_RADIUS
        points.append((r * math.cos(angle), r * math.sin(angle)))
    return points


def write_ini(path, size, points):
    # sea_ai/include/ai_flow_graph.h: pnt<i> = x,z,<number of edges>,<edge as two point indices>...
    lines = ["[Main]",
             f"DepthFile = resource\\foam\\islands\\{NAME}.tga",
             "vBoxCenter = 0.000000,0.000000,0.000000",
             f"vBoxSize = {size / 2:.6f},0.000000,{size / 2:.6f}",
             "",
             "[GraphPoints]"]
    count = len(points)
    for k, (x, z) in enumerate(points):
        prev_k, next_k = (k - 1) % count, (k + 1) % count
        lines.append(f"pnt{k} = {x:.2f},{z:.2f},2,{prev_k},{k},{k},{next_k},")
    path.write_text("\r\n".join(lines) + "\r\n")


def main(out):
    out = Path(out)
    rng = np.random.default_rng(1640)
    textures = out / "textures" / "islands" / NAME
    write_tx(island_texture(rng), textures / f"{NAME}.tga.tx")
    write_tx(seabed_texture(rng), textures / "seabed.tga.tx")

    land, seabed = island_meshes()
    models = out / "models" / "islands" / NAME
    # the "geometry" label makes the model load isle_seabed.gm as its child node "seabed"
    write_gm([land], models / f"{NAME}.gm", bsp=True, labels=[("geometry", "seabed", (0.0, 0.0, 0.0))])
    write_gm([seabed], models / f"{NAME}_seabed.gm")

    foam = out / "foam" / "islands"
    foam.mkdir(parents=True, exist_ok=True)
    pixels, size = depth_map()
    write_depth_tga(foam / f"{NAME}.tga", pixels)
    # the engine packs the map into <name>.tga.zap on first load and prefers it to the tga after:
    # an old one would keep the old island
    (foam / f"{NAME}.tga.zap").unlink(missing_ok=True)
    points = graph_points()
    for x, z in points:
        assert height(x, z) < -14.0, f"graph point {x:.0f},{z:.0f} is too shallow"
    write_ini(foam / f"{NAME}.ini", size, points)
    land_share = (pixels == HMAP_START).mean()
    print(f"island: {len(land.triangles)} + {len(seabed.triangles)} triangles, land {land_share:.1%} of the map")


if __name__ == "__main__":
    main(sys.argv[1])
