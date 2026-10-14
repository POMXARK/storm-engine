"""Builds the character demo assets: the skinned sailor (.gm), his skeleton and animations (.an + .ani),
textures (.tx) and a floor.

Usage: make_character.py <output resource dir>
Writes models/demo/{sailor,floor}.gm, animation/demo/sailor.{an,ani} and textures/demo/*.tga.tx.
Everything is generated, so the demo has no third-party assets. The sailor is shared with other
demos (demos/common/sailor.py); this demo plays his "idle" and "walk" cycles.
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

# the format writers and the sailor are shared by the demos
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "common"))
import sailor  # noqa: E402
from storm_formats import Mesh, write_gm, write_tx  # noqa: E402


def floor_tiles():
    image = Image.new("RGB", (256, 256), (150, 146, 138))
    draw = ImageDraw.Draw(image)
    draw.rectangle([0, 0, 255, 255], outline=(96, 92, 86), width=4)
    draw.rectangle([128, 128, 255, 255], fill=(138, 134, 126), outline=(96, 92, 86), width=4)
    draw.rectangle([0, 0, 127, 127], fill=(138, 134, 126), outline=(96, 92, 86), width=4)
    noise = Image.effect_noise((256, 256), 16).convert("RGB")
    return Image.blend(image, Image.composite(image, noise, Image.new("L", image.size, 210)), 0.5)


def floor_meshes(size=40.0, cells=20, tiles=20.0):
    mesh = Mesh("floor", "demo\\floor.tga")
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


def write_ani(path, cycles):
    text = "; sailor skeleton and its actions; times are frames of sailor.an\nanimation = demo\\sailor.an\n"
    for cycle in cycles:
        text += f"\n[{cycle.name}]\nstart_time = {cycle.start}\nend_time = {cycle.end}\nloop = true\n"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="ascii")


def main():
    resource = Path(sys.argv[1])
    sailor.write_textures(resource / "textures" / "demo")
    write_tx(floor_tiles(), resource / "textures" / "demo" / "floor.tga.tx")

    cycles = [sailor.IDLE, sailor.WALK]
    sailor.write_sailor(resource / "models" / "demo" / "sailor.gm", resource / "animation" / "demo" / "sailor.an",
                        "demo\\", cycles)
    write_ani(resource / "animation" / "demo" / "sailor.ani", cycles)
    write_gm(floor_meshes(), resource / "models" / "demo" / "floor.gm")
    print(f"walk speed {sailor.WALK.speed:.3f} m/s")


if __name__ == "__main__":
    main()
