"""Builds the melee demo assets: the walled yard of the location demo, two fencers (the sailor and a
pirate in red stripes) with the fight actions of sailor_fight, the saber they hold and the bars the
location draws over an enemy.

Usage: make_melee.py <assets dir> <output resource dir>
<assets dir> must contain PT_Serif-Web-Regular.ttf (see ../dialog/fetch_assets.sh).
Writes models/yard/*, textures/yard/*, models/characters/{sailor,pirate}.gm with
textures/characters/*, animation/fencer.{an,ani}, models/Ammo/saber.gm with textures/Ammo/*,
textures/LocEfx/state_bars.tga.tx, the log font and ini/fonts.ini.
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
# the format writers and the sailor are shared by the demos, the yard is the location demo's
sys.path.insert(0, str(HERE.parent / "common"))
sys.path.insert(0, str(HERE.parent / "location"))
import make_location  # noqa: E402
import sailor  # noqa: E402
import sailor_fight  # noqa: E402
from storm_formats import Mesh, write_fonts, write_gm, write_ptc, write_tx  # noqa: E402

FONTS = {"demo_log": ("PT_Serif-Web-Regular.ttf", 20, 1)}
PIRATE = dict(stripe=(150, 28, 26), trousers=(52, 40, 30), hair=(24, 20, 18))


# ---------------------------------------------------------------- saber

def steel():
    image = Image.new("RGB", (64, 64), (196, 202, 210))
    draw = ImageDraw.Draw(image)
    for x in range(64):  # a bright edge along u, the fuller darker
        shade = 230 - abs(x - 32) * 2
        draw.line([(x, 0), (x, 63)], fill=(shade, shade + 4, shade + 10))
    return image.filter(ImageFilter.GaussianBlur(0.8))


def grip():
    image = Image.new("RGB", (64, 64), (70, 44, 26))
    draw = ImageDraw.Draw(image)
    for y in range(0, 64, 8):  # leather wrapping
        draw.line([(0, y), (63, y + 4)], fill=(48, 30, 18), width=2)
    return image


def brass():
    return sailor.noisy((64, 64), (186, 150, 70), 18)


def taper_box(mesh, y0, y1, w0, w1, d0, d1):
    """A box along y from y0 to y1, w half-width in x, d half-depth in z at each end."""
    corners = []
    for y, w, d in ((y0, w0, d0), (y1, w1, d1)):
        corners.append([(-w, y, -d), (w, y, -d), (w, y, d), (-w, y, d)])
    for i in range(4):
        a, b = corners[0][i], corners[0][(i + 1) % 4]
        c, d = corners[1][(i + 1) % 4], corners[1][i]
        mid = [(a[k] + b[k]) / 2 for k in range(3)]
        n = (mid[0], 0.0, mid[2])
        length = (n[0] ** 2 + n[2] ** 2) ** 0.5
        n = (n[0] / length, 0.0, n[2] / length)
        mesh.quad(*[mesh.vertex(p, n, (u, v)) for p, (u, v) in
                    zip((a, b, c, d), ((0, 0), (1, 0), (1, 1), (0, 1)))])
    for ring, ny in ((corners[0], -1.0), (corners[1], 1.0)):
        mesh.quad(*[mesh.vertex(p, (0, ny, 0), (0.5, 0.5)) for p in ring])


def saber_meshes():
    """A saber along +y: the grip from 0 to 0.13, a brass guard, a slightly curved blade to 0.85."""
    handle, guard, blade = Mesh("grip", "saber_grip.tga"), Mesh("guard", "saber_brass.tga"), Mesh("blade", "saber_steel.tga")
    taper_box(handle, -0.04, 0.13, 0.016, 0.014, 0.018, 0.016)
    taper_box(guard, 0.13, 0.15, 0.06, 0.06, 0.016, 0.016)
    taper_box(guard, -0.06, -0.04, 0.02, 0.02, 0.02, 0.02)  # the pommel
    # the blade: flat in x, the edge along +z; segments bend it back a little towards the point
    steps = 6
    for i in range(steps):
        y0 = 0.15 + 0.7 * i / steps
        y1 = 0.15 + 0.7 * (i + 1) / steps
        w0 = 0.0035
        d0 = 0.017 * (1 - 0.6 * i / steps)
        d1 = 0.017 * (1 - 0.6 * (i + 1) / steps) if i + 1 < steps else 0.001
        taper_box(blade, y0, y1, w0, w0 * 0.8, d0, d1)
    return [handle, guard, blade]


# ---------------------------------------------------------------- bars over the enemy

def state_bars():
    """LocEfx\\state_bars.tga (location.cpp, DrawEnemyBars): v 0..0.5 the frame drawn behind,
    0.5..0.75 the life and 0.75..1 the energy, cut to their share of 3..97 % of the width."""
    image = Image.new("RGBA", (256, 128), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([2, 10, 253, 54], radius=6, fill=(20, 16, 12, 170), outline=(220, 200, 150, 230), width=2)
    draw.rectangle([0, 64 + 14, 255, 95], fill=(200, 36, 30, 255))
    draw.rectangle([0, 64 + 14, 255, 64 + 18], fill=(240, 110, 90, 255))
    draw.rectangle([0, 96, 255, 96 + 12], fill=(230, 190, 60, 255))
    return image


# ---------------------------------------------------------------- animation

def write_ani(path):
    """The fencer's actions: the walking ones of the location demo, the fight ones of sailor_fight."""
    text = "; a fencer as a location character; times are frames of fencer.an\nanimation = fencer.an\n"

    def section(name, cycle, extra=""):
        nonlocal text
        loop = "true" if cycle.loop else "false"
        text += f"\n[{name}]\nstart_time = {cycle.start}\nend_time = {cycle.end}\nloop = {loop}\n{extra}"

    def speed(cycle, sign=1):
        return f'data = "Walk speed", "{sign * cycle.speed:.3f}"\n'

    section("idle_1", sailor.IDLE)
    section("walk", sailor.WALK, speed(sailor.WALK))
    section("back walk", sailor.WALK, "type = reverse\n" + speed(sailor.WALK, -1))
    section("run", sailor.RUN, speed(sailor.RUN))
    section("back run", sailor.RUN, "type = reverse\n" + speed(sailor.RUN, -1))
    section("turn left", sailor.TURN)
    section("turn right", sailor.TURN)
    for name, cycle, extra in sailor_fight.ani_sections():
        section(name, cycle, extra)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="ascii")


def main():
    assets, resource = Path(sys.argv[1]), Path(sys.argv[2])

    yard = resource / "textures" / "yard"
    write_tx(make_location.cobbles(), yard / "cobbles.tga.tx")
    write_tx(make_location.stone_wall(), yard / "stone_wall.tga.tx")
    write_tx(make_location.planks(), yard / "planks.tga.tx")
    write_tx(make_location.planks(border=True), yard / "crate.tga.tx")
    write_gm(make_location.yard_meshes(), resource / "models" / "yard" / "yard.gm", bsp=True)
    write_ptc(make_location.patch_triangles(), resource / "models" / "yard" / "yard_patch.ptc",
              materials=("ground", "wood"))

    sailor_fight.self_check()
    characters = resource / "textures" / "characters"
    sailor.write_textures(characters)
    sailor.write_textures(characters, prefix="pirate_", **PIRATE)
    cycles = [sailor.IDLE, sailor.WALK, sailor.RUN, sailor.TURN] + sailor_fight.CYCLES
    labels = sailor_fight.blade_labels()
    an = resource / "animation" / "fencer.an"
    sailor.write_sailor(resource / "models" / "characters" / "sailor.gm", an, "", cycles, labels)
    sailor.write_sailor(resource / "models" / "characters" / "pirate.gm", an, "pirate_", cycles, labels)
    write_ani(resource / "animation" / "fencer.ani")

    # BLADE loads models\Ammo\<name> with textures from textures\Ammo\; the trail runs between
    # the "start" and "end" labels, BLADE traces every frame if the model has none
    ammo = resource / "textures" / "Ammo"
    write_tx(steel(), ammo / "saber_steel.tga.tx")
    write_tx(grip(), ammo / "saber_grip.tga.tx")
    write_tx(brass(), ammo / "saber_brass.tga.tx")
    write_gm(saber_meshes(), resource / "models" / "Ammo" / "saber.gm",
             labels=[("blade", "start", (0.0, 0.2, 0.0)), ("blade", "end", (0.0, 0.85, 0.0))])

    write_tx(state_bars(), resource / "textures" / "LocEfx" / "state_bars.tga.tx")
    write_fonts({name: (assets / ttf, size, outline) for name, (ttf, size, outline) in FONTS.items()}, resource,
                "generated by make_melee.py from PT Serif (SIL Open Font License)")
    print(f"fencer: {len(cycles)} actions, attack_fast {sailor_fight.ATTACK_FAST_1.frames} frames, "
          f"fight walk {sailor_fight.FIGHT_WALK.speed:.3f} m/s")


if __name__ == "__main__":
    main()
