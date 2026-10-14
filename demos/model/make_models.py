"""Builds the model demo assets: static geometry in the engine's .gm format and procedural .tx textures.

Usage: make_models.py <output resource dir>
Writes models/demo/{barrel,crate,deck}.gm and textures/demo/*.tga.tx. Nothing is downloaded: the wood
textures are generated, so the demo has no third-party assets.
"""
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

# the .tx/.gm writers are shared by the demos
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "common"))
from storm_formats import Mesh, write_gm, write_tx  # noqa: E402


# ---------------------------------------------------------------- textures

def wood(width, height, boards, base, rng, gap=3):
    """Vertical boards with grain streaks and dark gaps between them."""
    image = Image.new("RGB", (width, height), base)
    draw = ImageDraw.Draw(image)
    board_w = width / boards
    for i in range(boards):
        x0, x1 = round(i * board_w), round((i + 1) * board_w)
        shade = rng.uniform(0.82, 1.12)
        tone = tuple(min(255, int(c * shade)) for c in base)
        draw.rectangle([x0, 0, x1, height], fill=tone)
        for _ in range(int(board_w * 0.6)):
            x = rng.uniform(x0, x1)
            k = rng.uniform(0.75, 0.95)
            streak = tuple(int(c * k) for c in tone)
            draw.line([(x, 0), (x + rng.uniform(-3, 3), height)], fill=streak, width=1)
        draw.rectangle([x0, 0, x0 + gap - 1, height], fill=tuple(c // 3 for c in base))
    noise = Image.effect_noise((width, height), 18).convert("RGB")
    image = Image.blend(image, Image.composite(image, noise, Image.new("L", image.size, 200)), 0.5)
    return image.filter(ImageFilter.GaussianBlur(0.6))


def barrel_side(rng):
    image = wood(512, 512, 12, (150, 98, 52), rng)
    draw = ImageDraw.Draw(image)
    # iron hoops: two near each end, v grows from bottom to top
    for v in (0.06, 0.16, 0.84, 0.94):
        y = round((1 - v) * 512)
        draw.rectangle([0, y - 9, 512, y + 9], fill=(52, 50, 48))
        draw.line([(0, y - 9), (512, y - 9)], fill=(95, 92, 88), width=2)
        draw.line([(0, y + 8), (512, y + 8)], fill=(28, 27, 26), width=2)
    return image


def barrel_lid(rng):
    image = wood(256, 256, 5, (160, 110, 64), rng)
    draw = ImageDraw.Draw(image)
    draw.ellipse([2, 2, 253, 253], outline=(52, 50, 48), width=10)
    return image


def crate(rng):
    image = wood(256, 256, 4, (176, 136, 82), rng)
    draw = ImageDraw.Draw(image)
    frame = (128, 92, 50)
    draw.rectangle([0, 0, 255, 255], outline=frame, width=22)
    draw.line([(18, 238), (238, 18)], fill=frame, width=26)
    draw.rectangle([0, 0, 255, 255], outline=(60, 42, 24), width=3)
    return image


def deck(rng):
    return wood(512, 512, 8, (120, 86, 56), rng, gap=4).transpose(Image.ROTATE_90)


# ---------------------------------------------------------------- geometry

def barrel_meshes(height=1.2, r_end=0.36, r_mid=0.43, segments=32, rings=12):
    body = Mesh("body", "demo\\barrel_side.tga")
    # radius is a parabola over the height; the normal tilts by the slope of the profile
    for j in range(rings + 1):
        t = j / rings
        y = t * height
        r = r_end + (r_mid - r_end) * (1 - (2 * t - 1) ** 2)
        slope = (r_mid - r_end) * -4 * (2 * t - 1) / height  # dr/dy
        for i in range(segments + 1):
            a = 2 * math.pi * i / segments
            n = (math.cos(a), -slope, math.sin(a))
            length = math.sqrt(sum(c * c for c in n))
            body.vertex((r * math.cos(a), y, r * math.sin(a)), tuple(c / length for c in n), (3 * i / segments, 1 - t))
    row = segments + 1
    for j in range(rings):
        for i in range(segments):
            body.quad(j * row + i, j * row + i + 1, (j + 1) * row + i + 1, (j + 1) * row + i)

    lids = Mesh("lids", "demo\\barrel_lid.tga")
    for y, ny in ((0.03, -1.0), (height - 0.03, 1.0)):
        r = r_end - 0.01
        center = lids.vertex((0, y, 0), (0, ny, 0), (0.5, 0.5))
        ring = [lids.vertex((r * math.cos(a), y, r * math.sin(a)), (0, ny, 0),
                            (0.5 + 0.5 * math.cos(a), 0.5 + 0.5 * math.sin(a)))
                for a in (2 * math.pi * i / segments for i in range(segments + 1))]
        for i in range(segments):
            lids.triangle(center, ring[i], ring[i + 1])
    return [body, lids]


def box_meshes(size=0.8):
    mesh = Mesh("box", "demo\\crate.tga")
    h = size / 2
    for axis in range(3):
        for sign in (-1, 1):
            n = [0, 0, 0]
            n[axis] = sign
            u_axis, v_axis = [k for k in range(3) if k != axis]
            corners = []
            for du, dv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                p = [0.0, 0.0, 0.0]
                p[axis] = sign * h
                p[u_axis] = du * h
                p[v_axis] = dv * h
                p[1] += h  # stands on the ground
                corners.append(mesh.vertex(tuple(p), tuple(n), ((du + 1) / 2, (1 - dv) / 2)))
            mesh.quad(*corners)
    return [mesh]


def deck_meshes(size=120.0, cells=40, tiles=44.0):
    mesh = Mesh("planks", "demo\\deck.tga")
    # a grid instead of one quad: per-vertex lighting and fog need vertices across the surface
    index = {}
    for j in range(cells + 1):
        for i in range(cells + 1):
            x = (i / cells - 0.5) * size
            z = (j / cells - 0.5) * size
            index[i, j] = mesh.vertex((x, 0.0, z), (0, 1, 0), (tiles * i / cells, tiles * j / cells))
    for j in range(cells):
        for i in range(cells):
            mesh.quad(index[i, j], index[i + 1, j], index[i + 1, j + 1], index[i, j + 1])
    return [mesh]


def main():
    resource = Path(sys.argv[1])
    rng = random.Random(1715)  # fixed seed: identical assets on every run
    for name, make in (("barrel_side", barrel_side), ("barrel_lid", barrel_lid), ("crate", crate), ("deck", deck)):
        write_tx(make(rng), resource / "textures" / "demo" / f"{name}.tga.tx")
    write_gm(barrel_meshes(), resource / "models" / "demo" / "barrel.gm")
    write_gm(box_meshes(), resource / "models" / "demo" / "crate.gm")
    write_gm(deck_meshes(), resource / "models" / "demo" / "deck.gm")


if __name__ == "__main__":
    main()
