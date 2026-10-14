"""Generates the dialog demo resources: a pier with the sailor of the character demo standing on it,
the dialog window texture with its resource\\ini\\dialog.ini, the fonts and the click sound.

Usage: make_dialog.py <assets dir> <output resource dir>
<assets dir> must contain PT_Serif-Web-Regular.ttf and PT_Serif-Web-Bold.ttf (see fetch_assets.sh).
Writes models/pier/pier.gm, models/demo/sailor.gm, animation/demo/sailor.{an,ani}, textures/pier/*,
textures/demo/*, textures/dialog/dialog.tga.tx, textures/fonts/*, ini/{dialog,fonts}.ini and
sounds/interface/ok.wav.
"""
import math
import struct
import sys
import wave
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "common"))
import sailor  # noqa: E402
from storm_formats import Mesh, write_fonts, write_gm, write_tx  # noqa: E402

# the deck of the pier: x across, z along it, the sea at y = 0
DECK_TOP = 1.5
DECK_HALF_WIDTH = 2.0
DECK_NEAR, DECK_FAR = -14.0, 8.0

FONTS = {
    # name: (TrueType file, pixel size, outline); the dialog scales them by the window height / 600
    "dialog_text": ("PT_Serif-Web-Regular.ttf", 19, 1),
    "dialog_links": ("PT_Serif-Web-Regular.ttf", 18, 1),
    "dialog_name": ("PT_Serif-Web-Bold.ttf", 18, 1),
}


# ---------------------------------------------------------------- pier

def planks():
    """Weathered deck boards running along the texture's v."""
    rng = np.random.default_rng(7)
    image = Image.new("RGB", (256, 256))
    draw = ImageDraw.Draw(image)
    for i in range(8):
        tone = rng.uniform(0.85, 1.1)
        base = tuple(int(c * tone) for c in (120, 92, 64))
        draw.rectangle([i * 32, 0, i * 32 + 31, 255], fill=base)
        # grain along the board
        for _ in range(14):
            x = i * 32 + int(rng.uniform(3, 29))
            shade = tuple(int(c * rng.uniform(0.75, 0.9)) for c in base)
            draw.line([(x, 0), (x + int(rng.uniform(-2, 3)), 255)], fill=shade, width=1)
        draw.line([(i * 32, 0), (i * 32, 255)], fill=(52, 38, 26), width=3)
        # butt joint and nails
        y = int(rng.uniform(20, 236))
        draw.line([(i * 32, y), (i * 32 + 31, y)], fill=(60, 44, 30), width=2)
        for dy in (-6, 6):
            for dx in (8, 23):
                draw.point((i * 32 + dx, y + dy), fill=(40, 40, 44))
    noise = Image.effect_noise((256, 256), 18).convert("RGB")
    return Image.blend(image, Image.composite(image, noise, Image.new("L", image.size, 200)), 0.4)


def wood_post():
    rng = np.random.default_rng(3)
    image = Image.new("RGB", (64, 256), (78, 60, 44))
    draw = ImageDraw.Draw(image)
    for _ in range(30):
        x = int(rng.uniform(0, 64))
        draw.line([(x, 0), (x, 255)], fill=tuple(int(c * rng.uniform(0.7, 1.15)) for c in (78, 60, 44)))
    # dark and green below the high-water mark
    for y in range(140, 256):
        k = (y - 140) / 116
        draw.line([(0, y), (63, y)], fill=(int(50 - 20 * k), int(58 - 10 * k), int(40 - 8 * k)))
    return image


def box(mesh, lo, hi, uv_per_metre=1.0):
    """Four sides and the top of a box; the bottom is under the water and never seen."""
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    faces = [
        ((0, 0, -1), [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], x1 - x0, y1 - y0),
        ((0, 0, 1), [(x1, y0, z1), (x0, y0, z1), (x0, y1, z1), (x1, y1, z1)], x1 - x0, y1 - y0),
        ((-1, 0, 0), [(x0, y0, z1), (x0, y0, z0), (x0, y1, z0), (x0, y1, z1)], z1 - z0, y1 - y0),
        ((1, 0, 0), [(x1, y0, z0), (x1, y0, z1), (x1, y1, z1), (x1, y1, z0)], z1 - z0, y1 - y0),
        ((0, 1, 0), [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], x1 - x0, z1 - z0),
    ]
    for normal, corners, w, h in faces:
        uv = [(0, h * uv_per_metre), (w * uv_per_metre, h * uv_per_metre), (w * uv_per_metre, 0), (0, 0)]
        mesh.quad(*(mesh.vertex(p, normal, t) for p, t in zip(corners, uv)))


def pier_meshes():
    deck = Mesh("deck", "pier\\planks.tga")
    # a grid instead of one quad: per-vertex lighting and fog need vertices across the surface
    cols, rows = 4, int(DECK_FAR - DECK_NEAR)
    index = {}
    for j in range(rows + 1):
        for i in range(cols + 1):
            x = -DECK_HALF_WIDTH + 2 * DECK_HALF_WIDTH * i / cols
            z = DECK_NEAR + (DECK_FAR - DECK_NEAR) * j / rows
            # the texture has 8 boards across 256 px: two textures span the 4 m deck
            index[i, j] = deck.vertex((x, DECK_TOP, z), (0, 1, 0), (0.5 * x, 0.25 * z))
    for j in range(rows):
        for i in range(cols):
            deck.quad(index[i, j], index[i + 1, j], index[i + 1, j + 1], index[i, j + 1])
    # the edge beams the deck lies on
    for side in (-1, 1):
        x = side * DECK_HALF_WIDTH
        box(deck, (min(x, x - side * 0.2), DECK_TOP - 0.35, DECK_NEAR), (max(x, x - side * 0.2), DECK_TOP + 0.02, DECK_FAR),
            uv_per_metre=0.5)

    posts = Mesh("posts", "pier\\post.tga")
    z = DECK_NEAR + 0.5
    while z < DECK_FAR:
        for side in (-1, 1):
            x = side * (DECK_HALF_WIDTH + 0.05)
            box(posts, (x - 0.16, -4.0, z - 0.16), (x + 0.16, DECK_TOP + 0.45, z + 0.16), uv_per_metre=0.5)
        z += 3.0
    return [deck, posts]


def write_ani(path, cycles):
    text = "; sailor skeleton and its actions; times are frames of sailor.an\nanimation = demo\\sailor.an\n"
    for cycle in cycles:
        text += f"\n[{cycle.name}]\nstart_time = {cycle.start}\nend_time = {cycle.end}\nloop = true\n"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="ascii")


# ---------------------------------------------------------------- dialog window

# pixel rectangles of the window texture (left, top, right, bottom)
ATLAS_W, ATLAS_H = 256, 128
CORNER = 12
PANEL = (0, 0, 96, 96)
DIVIDER = (0, 104, 96, 112)
NAME = (104, 0, 200, 30)
NAME_CAP = 8
BUTTON_W, BUTTON_H = 16, 24
BUTTONS = {name: (104 + i * BUTTON_W, 40, 104 + (i + 1) * BUTTON_W, 40 + BUTTON_H)
           for i, name in enumerate(("uvUpNormal", "uvDownNormal", "uvUpLight", "uvDownLight"))}

GOLD = (214, 172, 92, 255)
GOLD_DARK = (120, 88, 40, 255)
WOOD = (34, 24, 16)


def panel(draw):
    """A dark translucent panel in a gold frame. The window is cut into nine pieces and the edges and
    the centre are stretched, so everything but the corners is uniform along the stretch."""
    x0, y0, x1, y1 = PANEL
    for y in range(y0, y1):
        # darker to the bottom
        k = (y - y0) / (y1 - y0)
        draw.line([(x0, y), (x1 - 1, y)], fill=(int(WOOD[0] * (1.1 - 0.3 * k)), int(WOOD[1] * (1.1 - 0.3 * k)),
                                                 int(WOOD[2] * (1.1 - 0.3 * k)), 225))
    draw.rectangle([x0, y0, x1 - 1, y1 - 1], outline=(10, 6, 2, 255), width=1)
    draw.rectangle([x0 + 3, y0 + 3, x1 - 4, y1 - 4], outline=GOLD, width=2)
    draw.rectangle([x0 + 6, y0 + 6, x1 - 7, y1 - 7], outline=GOLD_DARK, width=1)
    # a gold rivet in each corner
    for cx, cy in ((x0 + 4, y0 + 4), (x1 - 5, y0 + 4), (x0 + 4, y1 - 5), (x1 - 5, y1 - 5)):
        draw.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], fill=GOLD, outline=(60, 40, 10, 255))


def divider(draw):
    x0, y0, x1, y1 = DIVIDER
    mid = (y0 + y1) // 2
    draw.line([(x0, mid - 1), (x1 - 1, mid - 1)], fill=GOLD_DARK)
    draw.line([(x0, mid), (x1 - 1, mid)], fill=GOLD)


def name_plate(draw):
    x0, y0, x1, y1 = NAME
    draw.rounded_rectangle([x0, y0, x1 - 1, y1 - 1], radius=NAME_CAP - 1, fill=WOOD + (235,), outline=GOLD, width=2)


def arrow(draw, rect, up, lit):
    x0, y0, x1, y1 = rect
    color = (255, 222, 140, 255) if lit else GOLD
    cx = (x0 + x1) / 2
    if up:
        points = [(cx, y0 + 6), (x1 - 3, y1 - 8), (x0 + 3, y1 - 8)]
    else:
        points = [(x0 + 3, y0 + 8), (x1 - 3, y0 + 8), (cx, y1 - 6)]
    draw.polygon(points, fill=color, outline=(40, 26, 8, 255))


def window_texture():
    image = Image.new("RGBA", (ATLAS_W, ATLAS_H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    panel(draw)
    divider(draw)
    name_plate(draw)
    for name, rect in BUTTONS.items():
        arrow(draw, rect, up="Up" in name, lit="Light" in name)
    return image


def uv(rect, inset=0.5):
    """A pixel rectangle as the "left,top,right,bottom" uv the ini takes, half a texel inside so the
    filtering does not pull in the neighbouring piece."""
    x0, y0, x1, y1 = rect
    return (f"{(x0 + inset) / ATLAS_W:.6f},{(y0 + inset) / ATLAS_H:.6f},"
            f"{(x1 - inset) / ATLAS_W:.6f},{(y1 - inset) / ATLAS_H:.6f}")


def argb(a, r, g, b):
    # colours in dialog.ini are read as integers
    return (a << 24) | (r << 16) | (g << 8) | b


def dialog_ini():
    x0, y0, x1, y1 = PANEL
    c = CORNER
    pieces = {
        "uvLeftTop": (x0, y0, x0 + c, y0 + c), "uvRightTop": (x1 - c, y0, x1, y0 + c),
        "uvLeftBottom": (x0, y1 - c, x0 + c, y1), "uvRightBottom": (x1 - c, y1 - c, x1, y1),
        "uvLeft": (x0, y0 + c, x0 + c, y1 - c), "uvRight": (x1 - c, y0 + c, x1, y1 - c),
        "uvTop": (x0 + c, y0, x1 - c, y0 + c), "uvBottom": (x0 + c, y1 - c, x1 - c, y1),
        "uvCenter": (x0 + c, y0 + c, x1 - c, y1 - c), "uvDivider": DIVIDER,
        "uvChrNameLeft": (NAME[0], NAME[1], NAME[0] + NAME_CAP, NAME[3]),
        "uvChrNameCenter": (NAME[0] + NAME_CAP, NAME[1], NAME[2] - NAME_CAP, NAME[3]),
        "uvChrNameRight": (NAME[2] - NAME_CAP, NAME[1], NAME[2], NAME[3]),
    }
    lines = [
        "; generated by make_dialog.py; read by the dialog entity (dialog/src/dialog.cpp LoadFromIni)",
        "; positions and sizes are pixels of an 800x600 screen, scaled to the real one",
        "[DIALOG]",
        "mainfont = DIALOG_TEXT",
        "subfont = DIALOG_LINKS",
        "charnamefont = DIALOG_NAME",
        "mainFontScale = 1.0",
        "subFontScale = 1.0",
        "charnamescale = 1.0",
        f"mainFontColor = {argb(255, 240, 230, 205)}",
        f"subFontColor = {argb(255, 160, 145, 120)}",
        f"subFontColorSelect = {argb(255, 255, 214, 120)}",
        f"charnamecolor = {argb(255, 255, 214, 120)}",
        "; text lines and answer lines on one page",
        "maxtextlines = 5",
        "maxlinkslines = 5",
        "; the name, from the top left corner of the window",
        "charnameoffset = 20,-27",
        "",
        "[BACKPARAM]",
        "baseScreenSize = 800,600",
        "baseScreenOffset = 0,0",
        "texture = dialog\\dialog.tga",
    ]
    lines += [f"{key} = {uv(rect)}" for key, rect in pieces.items()]
    lines += [
        "; the name plate above the window: left cap, centre (as wide as the name), right cap",
        "chrNameOffset = 10,-30",
        f"ChrNameHeight = {NAME[3] - NAME[1]}",
        f"ChrNameLeftWidth = {NAME_CAP}",
        "ChrNameCenterWidth = 128",
        f"ChrNameRightWidth = {NAME_CAP}",
        "dividerHeight = 8",
        "dividerOffsetX = 16",
        "; the frame width and the window; its top follows the text, only the bottom stays",
        f"backBorderOffset = {c},{c},{c},{c}",
        "backPosition = 40,440,760,584",
        "",
        "[BUTTON]",
    ]
    lines += [f"{key} = {uv(rect)}" for key, rect in BUTTONS.items()]
    lines += [
        f"buttonSize = {BUTTON_W},{BUTTON_H}",
        "rightoffset = 2",
        "topoffset = 2",
        "bottomoffset = 2",
    ]
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------- sound

def click_wav(path, rate=44100):
    """The tick the dialog plays on every key: a short knock of filtered noise and a low tone."""
    rng = np.random.default_rng(1)
    t = np.arange(int(rate * 0.06)) / rate
    noise = np.convolve(rng.standard_normal(t.size), np.ones(6) / 6, mode="same")
    signal = (0.6 * noise + 0.5 * np.sin(2 * math.pi * 900 * t)) * np.exp(-t * 90)
    samples = (np.clip(signal, -1, 1) * 0.5 * 32767).astype("<i2")
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(samples.tobytes())


def main():
    assets, resource = Path(sys.argv[1]), Path(sys.argv[2])

    write_tx(planks(), resource / "textures" / "pier" / "planks.tga.tx")
    write_tx(wood_post(), resource / "textures" / "pier" / "post.tga.tx")
    write_gm(pier_meshes(), resource / "models" / "pier" / "pier.gm")

    sailor.write_textures(resource / "textures" / "demo")
    cycles = [sailor.IDLE]
    sailor.write_sailor(resource / "models" / "demo" / "sailor.gm", resource / "animation" / "demo" / "sailor.an",
                        "demo\\", cycles)
    write_ani(resource / "animation" / "demo" / "sailor.ani", cycles)

    # the window is drawn pixel for pixel over the screen, a mip chain would only blur it
    write_tx(window_texture(), resource / "textures" / "dialog" / "dialog.tga.tx", mip_chain=False)
    (resource / "ini").mkdir(parents=True, exist_ok=True)
    (resource / "ini" / "dialog.ini").write_text(dialog_ini(), encoding="ascii")
    write_fonts({name: (assets / ttf, size, outline) for name, (ttf, size, outline) in FONTS.items()}, resource,
                "generated by make_dialog.py from PT Serif (SIL Open Font License)")
    click_wav(resource / "sounds" / "interface" / "ok.wav")


if __name__ == "__main__":
    main()
