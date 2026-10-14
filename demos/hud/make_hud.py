"""Generates the textures of the sea battle HUD the battle_interface entity draws.

Usage: make_hud.py <assets dir with PT_Serif-Web-Bold.ttf> <output resource dir>
  textures/hud/frame.tga.tx    the dark ring round the minimap with the player's ship in the middle
  textures/hud/compass.tga.tx  the compass card: ticks and N E S W, turned with the ship's heading
  textures/hud/cannons.tga.tx  a band round the minimap: the navigator cuts the arcs of the
                               broadsides out of it and tints them by the reload state
  textures/hud/wind.tga.tx     the wind arrow on the rim, turned to where the wind comes from
  textures/hud/icons.tga.tx    a 4 x 2 atlas: the charge, three sail states, three wind strengths
  textures/hud/panel.tga.tx    the frame of the ship panel
  textures/hud/state.tga.tx    the hull (left half) and sails (right half) gauges of the panel
  textures/hud/ship.tga.tx     the ship picture of the panel
  textures/hud/border.tga.tx   the battle border round the screen while an enemy is near: red,
                               opaque at the screen edge (v = 0) and clear at the inner edge
The navigator maps its square textures on a square of navigatorWidth pixels centred on the
minimap, so the radii below are fractions of the texture size that match the radii of the script
(mapRadius / navigatorWidth). Everything is drawn four times larger and scaled down for smooth
edges; there is nothing random, so the files come out the same on every run.
"""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(HERE.parent / "common"))
from storm_formats import write_tx  # noqa: E402

SS = 4              # supersampling
MAP = 0.35          # radius of the minimap (mapRadius / navigatorWidth in hud.c)
CANNON_IN = 0.365   # the band of the broadside arcs
CANNON_OUT = 0.415
RIM = 0.5

GOLD = (214, 178, 96, 255)
DARK = (14, 26, 38, 205)
PAPER = (240, 228, 196, 255)
RED = (200, 52, 40, 255)


def canvas(width, height=None):
    height = height or width
    return Image.new("RGBA", (width * SS, height * SS), (0, 0, 0, 0))


def done(image):
    return image.resize((image.width // SS, image.height // SS), Image.LANCZOS)


def ring(draw, size, r_in, r_out, fill):
    c = size * SS / 2
    draw.ellipse([c - r_out * size * SS, c - r_out * size * SS, c + r_out * size * SS, c + r_out * size * SS],
                 fill=fill)
    draw.ellipse([c - r_in * size * SS, c - r_in * size * SS, c + r_in * size * SS, c + r_in * size * SS],
                 fill=(0, 0, 0, 0))


def circle(draw, size, r, width, fill):
    c = size * SS / 2
    draw.ellipse([c - r * size * SS, c - r * size * SS, c + r * size * SS, c + r * size * SS],
                 outline=fill, width=int(width * SS))


def polar(size, r, angle):
    # the angle runs clockwise from the top of the screen, as the navigator turns its sprites
    c = size * SS / 2
    return c + math.sin(angle) * r * size * SS, c - math.cos(angle) * r * size * SS


def frame(size=256):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    ring(draw, size, MAP, RIM - 0.004, DARK)
    circle(draw, size, MAP + 0.004, 2.0, GOLD)
    circle(draw, size, RIM - 0.008, 2.0, GOLD)
    # the player's ship in the middle, bow up: the minimap turns round it
    c = size * SS / 2
    s = size * SS * 0.022
    draw.polygon([(c, c - 2.2 * s), (c + s, c - 0.4 * s), (c + 0.8 * s, c + 1.8 * s),
                  (c - 0.8 * s, c + 1.8 * s), (c - s, c - 0.4 * s)], fill=PAPER, outline=(0, 0, 0, 255))
    return done(image)


def compass(font_path, size=256):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    for deg in range(0, 360, 15):
        a = math.radians(deg)
        long_tick = deg % 45 == 0
        if deg % 90 == 0:
            continue
        r0 = 0.462 if long_tick else 0.474
        draw.line([polar(size, r0, a), polar(size, 0.488, a)], fill=PAPER, width=int((1.6 if long_tick else 1.0) * SS))
    font = ImageFont.truetype(str(font_path), int(0.075 * size * SS))
    for deg, letter in ((0, "N"), (90, "E"), (180, "S"), (270, "W")):
        x, y = polar(size, 0.458, math.radians(deg))
        # each letter stands upright towards the centre, as on a compass card
        glyph = Image.new("RGBA", (int(0.1 * size * SS), int(0.1 * size * SS)), (0, 0, 0, 0))
        g = ImageDraw.Draw(glyph)
        g.text((glyph.width / 2, glyph.height / 2), letter, font=font, anchor="mm",
               fill=RED if letter == "N" else PAPER, stroke_width=SS, stroke_fill=(0, 0, 0, 255))
        glyph = glyph.rotate(-deg, resample=Image.BICUBIC)
        image.alpha_composite(glyph, (int(x - glyph.width / 2), int(y - glyph.height / 2)))
    return done(image)


def cannons(size=256):
    # white: the navigator tints it with the ready, reloading and damaged colours of the script
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    ring(draw, size, CANNON_IN, CANNON_OUT, (255, 255, 255, 235))
    # gun ports: darker notches every 6 degrees
    for deg in range(0, 360, 6):
        a = math.radians(deg)
        draw.line([polar(size, CANNON_IN, a), polar(size, CANNON_OUT, a)], fill=(255, 255, 255, 120), width=SS)
    return done(image)


def wind(width=32, height=256):
    # the sprite spans the whole navigator: the arrow sits on the rim at the top and points inwards
    image = canvas(width, height)
    draw = ImageDraw.Draw(image)
    cx = width * SS / 2
    top, tip = 0.012 * height * SS, 0.115 * height * SS
    half = 0.32 * width * SS
    draw.polygon([(cx - half, top), (cx + half, top), (cx, tip)], fill=(150, 215, 255, 255), outline=(0, 0, 0, 255))
    draw.line([(cx, top), (cx, tip)], fill=(20, 60, 90, 255), width=SS)
    return done(image)


def icons(font_path, cell=64):
    image = canvas(cell * 4, cell * 2)
    draw = ImageDraw.Draw(image)
    s = cell * SS

    def at(n, x, y):
        return (n % 4) * s + x * s, (n // 4) * s + y * s

    def plate(n):
        x0, y0 = at(n, 0.06, 0.06)
        x1, y1 = at(n, 0.94, 0.94)
        draw.rounded_rectangle([x0, y0, x1, y1], radius=0.14 * s, fill=DARK, outline=GOLD, width=int(1.5 * SS))

    # 0: round shot, three balls in a pile
    plate(0)
    for bx, by in ((0.36, 0.62), (0.64, 0.62), (0.5, 0.38)):
        x, y = at(0, bx, by)
        r = 0.14 * s
        draw.ellipse([x - r, y - r, x + r, y + r], fill=(70, 70, 76, 255), outline=PAPER, width=SS)
        draw.ellipse([x - r * 0.5, y - r * 0.6, x - r * 0.1, y - r * 0.2], fill=(160, 160, 170, 255))

    # 1-3: the sails furled, half set, full set
    for n, sail in ((1, 0.0), (2, 0.5), (3, 1.0)):
        plate(n)
        mx, my0 = at(n, 0.5, 0.16)
        _, my1 = at(n, 0.5, 0.84)
        draw.line([(mx, my0), (mx, my1)], fill=PAPER, width=int(2 * SS))
        for yard in (0.26, 0.56):
            x0, y = at(n, 0.24, yard)
            x1, _ = at(n, 0.76, yard)
            draw.line([(x0, y), (x1, y)], fill=PAPER, width=int(1.6 * SS))
            if sail > 0:
                depth = 0.26 * sail
                x2, y2 = at(n, 0.27, yard + depth)
                x3, _ = at(n, 0.73, yard + depth)
                belly = 0.06 * s * sail
                draw.polygon([(x0 + 0.03 * s, y), (x1 - 0.03 * s, y), (x3 + belly, y2), (x2 + belly, y2)],
                             fill=(236, 226, 200, 255), outline=(0, 0, 0, 255))
            else:
                draw.line([(x0, y + 0.03 * s), (x1, y + 0.03 * s)], fill=(236, 226, 200, 255), width=int(3 * SS))

    # 4-6: a pennant hanging, flying, stretched out
    for n, strength in ((4, 0.0), (5, 0.5), (6, 1.0)):
        plate(n)
        px, py0 = at(n, 0.26, 0.16)
        _, py1 = at(n, 0.26, 0.86)
        draw.line([(px, py0), (px, py1)], fill=PAPER, width=int(2 * SS))
        length = 0.5 * s
        droop = (1.0 - strength) * 1.25
        pts_top, pts_bottom = [], []
        for i in range(13):
            t = i / 12
            a = droop * t
            ripple = 0.03 * s * math.sin(t * 9.0) * (0.3 + strength)
            x = px + math.cos(a) * length * t
            y = py0 + 0.02 * s + math.sin(a) * length * t + ripple
            w = (1.0 - t) * 0.16 * s
            pts_top.append((x, y))
            pts_bottom.append((x - math.sin(a) * w, y + math.cos(a) * w))
        draw.polygon(pts_top + pts_bottom[::-1], fill=RED, outline=(0, 0, 0, 255))
    # 7: unused
    return done(image)


def panel(size=128):
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    s = size * SS
    draw.rounded_rectangle([0.03 * s, 0.03 * s, 0.97 * s, 0.97 * s], radius=0.12 * s, fill=(14, 26, 38, 70),
                           outline=GOLD, width=int(2.5 * SS))
    draw.rounded_rectangle([0.07 * s, 0.07 * s, 0.93 * s, 0.93 * s], radius=0.09 * s,
                           outline=(214, 178, 96, 110), width=SS)
    return done(image)


def state(width=32, height=128):
    # two gauges side by side; the panel cuts them from the top as the hull and the sails wear away
    image = canvas(width, height)
    draw = ImageDraw.Draw(image)
    s = SS
    for half, (top, bottom) in enumerate((((230, 80, 60), (120, 24, 18)), ((250, 244, 222), (160, 150, 120)))):
        x0 = half * width / 2 * s + 2 * s
        x1 = (half + 1) * width / 2 * s - 2 * s
        steps = height * s
        for y in range(steps):
            t = y / steps
            col = tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3)) + (255,)
            draw.line([(x0, y), (x1, y)], fill=col)
        draw.rectangle([x0, 0, x1, steps - 1], outline=(0, 0, 0, 255), width=s)
    return done(image)


def ship(size=128):
    # a brig broadside on: hull, two masts with their courses and topsails
    image = canvas(size)
    draw = ImageDraw.Draw(image)
    s = size * SS

    def p(x, y):
        return x * s, y * s

    sail = (238, 230, 205, 255)
    ink = (0, 0, 0, 255)
    for mx in (0.38, 0.64):
        draw.line([p(mx, 0.12), p(mx, 0.72)], fill=(90, 60, 36, 255), width=int(2 * SS))
        draw.polygon([p(mx - 0.16, 0.42), p(mx + 0.16, 0.42), p(mx + 0.18, 0.64), p(mx - 0.14, 0.64)], fill=sail,
                     outline=ink)
        draw.polygon([p(mx - 0.12, 0.2), p(mx + 0.12, 0.2), p(mx + 0.14, 0.39), p(mx - 0.1, 0.39)], fill=sail,
                     outline=ink)
    draw.polygon([p(0.78, 0.5), p(0.95, 0.66), p(0.68, 0.66)], fill=sail, outline=ink)
    draw.line([p(0.64, 0.14), p(0.97, 0.64)], fill=(90, 60, 36, 255), width=SS)
    draw.polygon([p(0.06, 0.66), p(0.97, 0.66), p(0.86, 0.82), p(0.14, 0.82)], fill=(104, 64, 34, 255), outline=ink)
    draw.line([p(0.1, 0.71), p(0.92, 0.71)], fill=(222, 190, 110, 255), width=int(1.5 * SS))
    for gx in (0.24, 0.36, 0.48, 0.6, 0.72):
        draw.rectangle([p(gx, 0.73), p(gx + 0.035, 0.765)], fill=ink)
    # flag
    draw.polygon([p(0.64, 0.12), p(0.74, 0.14), p(0.64, 0.17)], fill=RED)
    return done(image)


def border(width=8, height=64):
    # the border strip maps v from the screen edge (0) to its inner edge (0.96); the vertex colour
    # only scales the alpha, so the colour lives here
    image = Image.new("RGBA", (width, height))
    for y in range(height):
        t = min(y / (0.96 * (height - 1)), 1.0)
        alpha = int(round(255 * (1.0 - t) ** 2))
        for x in range(width):
            image.putpixel((x, y), (200, 40, 28, alpha))
    return image


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    assets, resource = Path(sys.argv[1]), Path(sys.argv[2])
    font = assets / "PT_Serif-Web-Bold.ttf"
    out = resource / "textures/hud"
    # the HUD is drawn pixel for pixel: no mip chains
    write_tx(frame(), out / "frame.tga.tx", mip_chain=False)
    write_tx(compass(font), out / "compass.tga.tx", mip_chain=False)
    write_tx(cannons(), out / "cannons.tga.tx", mip_chain=False)
    write_tx(wind(), out / "wind.tga.tx", mip_chain=False)
    write_tx(icons(font), out / "icons.tga.tx", mip_chain=False)
    write_tx(panel(), out / "panel.tga.tx", mip_chain=False)
    write_tx(state(), out / "state.tga.tx", mip_chain=False)
    write_tx(ship(), out / "ship.tga.tx", mip_chain=False)
    write_tx(border(), out / "border.tga.tx", mip_chain=False)


if __name__ == "__main__":
    main()
