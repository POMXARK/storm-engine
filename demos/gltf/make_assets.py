"""Builds the assets of the glTF demo into resource/: the models of source/*.glb (Kenney Pirate Kit,
CC0) converted by tools/gltf2gm into models/pirate/*.gm and textures/pirate/*.tx, and a plane of
water under them (the kit has none).

Usage: make_assets.py <resource dir>
"""
import random
import sys
from pathlib import Path

from PIL import Image, ImageFilter

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent / "tools" / "gltf2gm"))
sys.path.insert(0, str(HERE.parent / "common"))
from gltf2gm import Textures, convert  # noqa: E402
from storm_formats import Mesh, write_gm, write_tx  # noqa: E402

FOLDER = "pirate"
WATER_SIZE = 160.0
WATER_LEVEL = 0.1
# metres a tile of the water texture covers
WATER_TILE = 8.0


def water_texture():
    random.seed(7)
    image = Image.new("RGB", (64, 64))
    image.putdata([(38 + (n := random.randint(-6, 6)), 120 + 2 * n, 150 + 2 * n) for _ in range(64 * 64)])
    return image.filter(ImageFilter.GaussianBlur(1.2))


def water(resource):
    write_tx(water_texture(), resource / "textures" / FOLDER / "water.tga.tx")
    mesh = Mesh("water", f"{FOLDER}\\water.tga")
    half, repeat = WATER_SIZE / 2, WATER_SIZE / WATER_TILE
    corners = [(-half, -half, 0, 0), (half, -half, repeat, 0), (half, half, repeat, repeat), (-half, half, 0, repeat)]
    ids = [mesh.vertex((x, WATER_LEVEL, z), (0, 1, 0), (u, v)) for x, z, u, v in corners]
    mesh.quad(*ids)
    write_gm([mesh], resource / "models" / FOLDER / "water.gm")


def main():
    resource = Path(sys.argv[1])
    textures = Textures(resource, FOLDER)
    for source in sorted((HERE / "source").glob("*.glb")):
        convert(source, resource, FOLDER, textures)
    water(resource)


if __name__ == "__main__":
    main()
