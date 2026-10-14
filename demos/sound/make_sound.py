"""Generates the sounds, the music, the buoy and the log font of the sound demo.

Usage: make_sound.py <assets dir> <output resource dir>
<assets dir> must contain PT_Serif-Web-Regular.ttf (see ../dialog/fetch_assets.sh).
Every sound is synthesized here, nothing is recorded or downloaded:
  sounds/demo/bell.wav             a buoy bell struck twice, looped and placed in 3D by the script
  sounds/demo/cannon{1,2,3}.wav    variants of one alias, the service picks one at random
  sounds/demo/gull{1,2,3}.wav      calls the ambient scheme plays at random intervals
  sounds/demo/waves.wav            a seamless loop of surf
  sounds/music/calm.wav, battle.wav  "Drunken Sailor" (traditional) as a slow and a fast arrangement
  ini/aliases/demo.ini, ini/sound_scheme.ini, models/demo/buoy.gm and its texture, the log font.
The sea, the sky and the ship come from ../sea/make_sea.py and ../ship/make_ship.py.
"""
import math
import sys
import wave
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "common"))
from storm_formats import Mesh, write_fonts, write_gm, write_tx  # noqa: E402

RATE = 44100
FONTS = {
    "demo_log": ("PT_Serif-Web-Regular.ttf", 20, 1),
}


def write_wav(path, samples):
    """samples: float array in -1..1, shape (n,) for mono or (n, 2) for stereo."""
    path.parent.mkdir(parents=True, exist_ok=True)
    data = np.clip(samples, -1.0, 1.0)
    channels = 1 if data.ndim == 1 else data.shape[1]
    with wave.open(str(path), "wb") as out:
        out.setnchannels(channels)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes((data * 32767.0).astype("<i2").tobytes())


def seconds(length):
    return np.arange(int(length * RATE)) / RATE


def normalize(x, peak=0.9):
    return x * (peak / max(1e-9, np.max(np.abs(x))))


def lowpass(x, cutoff):
    """One-pole low pass, applied forward over the whole signal through a cumulative filter."""
    a = math.exp(-2.0 * math.pi * cutoff / RATE)
    out = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1.0 - a) * v + a * acc
        out[i] = acc
    return out


def shaped_noise(length, rng, low, high, channels=1):
    """Noise with its spectrum cut to low..high Hz. Made in the frequency domain, so it is periodic
    and loops without a seam."""
    n = int(length * RATE)
    out = []
    for _ in range(channels):
        spectrum = rng.normal(size=n // 2 + 1) + 1j * rng.normal(size=n // 2 + 1)
        freqs = np.fft.rfftfreq(n, 1.0 / RATE)
        gain = ((freqs >= low) & (freqs <= high)) / np.sqrt(np.maximum(freqs, 1.0))
        out.append(np.fft.irfft(spectrum * gain, n))
    noise = np.stack(out, axis=1) if channels > 1 else out[0]
    return noise / np.max(np.abs(noise))


# --- effects ---------------------------------------------------------------------------------------

def bell_strike(t, base):
    # partials of a church-type bell: hum, prime, minor third, fifth, octave and upper partials,
    # the higher the faster they die out
    partials = ((0.5, 0.5, 2.6), (1.0, 1.0, 2.0), (1.19, 0.6, 1.6), (1.5, 0.35, 1.3), (2.0, 0.45, 1.1),
                (2.52, 0.25, 0.7), (2.66, 0.2, 0.6), (3.01, 0.15, 0.45), (4.1, 0.1, 0.3))
    s = sum(a * np.sin(2 * np.pi * base * r * t) * np.exp(-t / d) for r, a, d in partials)
    return s * np.minimum(1.0, t * 400.0)


def bell():
    # one loop of 5 s: a strike and a softer second one, as a buoy rocking on the swell
    t = seconds(10.0)
    s = bell_strike(t, 330.0) + 0.6 * np.where(t > 1.9, bell_strike(np.maximum(t - 1.9, 0.0), 330.0), 0.0)
    return loop_fold(s, 5.0, 0.8)


def cannon(rng, pitch):
    t = seconds(2.6)
    boom = np.sin(2 * np.pi * (48.0 * pitch) * t * (1.0 + 0.6 * np.exp(-t / 0.08))) * np.exp(-t / 0.35)
    blast = lowpass(rng.normal(size=len(t)), 900.0 * pitch) * np.exp(-t / 0.12)
    rumble = lowpass(rng.normal(size=len(t)), 180.0) * np.exp(-t / 0.9)
    s = 1.0 * boom + 2.5 * blast + 3.0 * rumble
    return normalize(s * np.minimum(1.0, t * 2000.0), 0.95)


def gull(rng, calls):
    parts = []
    for i in range(calls):
        length = rng.uniform(0.32, 0.5)
        t = seconds(length)
        # a squawk falls in pitch; the harmonics and the fast vibrato make it hoarse
        f = rng.uniform(1500, 1900) * (1.0 - 0.35 * t / length) * (1.0 + 0.03 * np.sin(2 * np.pi * 38 * t))
        phase = 2 * np.pi * np.cumsum(f) / RATE
        tone = sum(a * np.sin(k * phase) for k, a in ((1, 1.0), (2, 0.5), (3, 0.35), (4, 0.15)))
        env = np.sin(np.pi * t / length) ** 0.6
        parts.append(tone * env)
        parts.append(np.zeros(int(rng.uniform(0.08, 0.2) * RATE)))
    return normalize(np.concatenate(parts), 0.7)


def waves(rng):
    # 12 s of surf: low noise swelling twice and a hiss on the crests, in stereo
    length = 12.0
    t = seconds(length)
    body = shaped_noise(length, rng, 40, 700, 2)
    hiss = shaped_noise(length, rng, 800, 4000, 2)
    swell = 0.55 + 0.45 * np.sin(2 * np.pi * t / 6.0) ** 2
    crest = np.sin(np.pi * t / 6.0 - 0.3) ** 8
    s = body * swell[:, None] + 0.12 * hiss * crest[:, None]
    return normalize(s, 0.6)


# --- music -----------------------------------------------------------------------------------------

NOTE = {"C": -9, "D": -7, "E": -5, "F": -4, "G": -2, "A": 0, "B": 2}


def freq(name):
    """"A4", "D5"... equal temperament, A4 = 440 Hz."""
    return 440.0 * 2.0 ** ((NOTE[name[0]] + 12 * (int(name[1:]) - 4)) / 12.0)


# "What shall we do with a drunken sailor": (note, beats), eight bars of four beats
TUNE = [
    ("A4", 1), ("A4", .5), ("A4", .5), ("A4", 1), ("A4", .5), ("A4", .5), ("A4", 1), ("D4", 1), ("F4", 1), ("A4", 1),
    ("G4", 1), ("G4", .5), ("G4", .5), ("G4", 1), ("G4", .5), ("G4", .5), ("G4", 1), ("C4", 1), ("E4", 1), ("G4", 1),
    ("A4", 1), ("A4", .5), ("A4", .5), ("A4", 1), ("A4", .5), ("A4", .5), ("A4", 1), ("B4", 1), ("C5", 1), ("D5", 1),
    ("C5", 1), ("A4", 1), ("G4", 1), ("E4", 1), ("D4", 2), ("D4", 2),
]
# a chord per bar: root and the notes of the pad
CHORDS = [("D3", ("D4", "F4", "A4"))] * 2 + [("C3", ("C4", "E4", "G4"))] * 2 + \
         [("D3", ("D4", "F4", "A4"))] * 2 + [("C3", ("C4", "E4", "G4")), ("D3", ("D4", "F4", "A4"))]


def place(track, start, sound):
    i = int(start * RATE)
    end = min(len(track), i + len(sound))
    track[i:end] += sound[:end - i]


def pluck(f, length):
    # a plucked string: harmonics that fade faster the higher they are
    t = seconds(length)
    s = sum(np.sin(2 * np.pi * f * k * t) * np.exp(-t * (1.5 + 2.2 * k)) / k for k in range(1, 8))
    return s * np.minimum(1.0, t * 300.0)


def brass(f, length):
    # a reedy lead: odd and even harmonics with a soft attack and release
    t = seconds(length)
    s = sum(np.sin(2 * np.pi * f * k * t + 0.003 * np.sin(2 * np.pi * 5.5 * t) * k * f) / k for k in range(1, 9))
    env = np.minimum(1.0, t * 40.0) * np.minimum(1.0, (length - t) * 25.0)
    return s * env


def pad(notes, length):
    t = seconds(length)
    s = sum(np.sin(2 * np.pi * freq(n) * t) + 0.3 * np.sin(4 * np.pi * freq(n) * t) for n in notes)
    env = np.minimum(1.0, t * 1.5) * np.minimum(1.0, (length - t) * 1.5)
    return s * env


def kick(length=0.4):
    t = seconds(length)
    return np.sin(2 * np.pi * (50.0 + 90.0 * np.exp(-t / 0.03)) * t) * np.exp(-t / 0.15)


def snare(rng, length=0.25):
    t = seconds(length)
    return (0.6 * rng.normal(size=len(t)) + 0.5 * np.sin(2 * np.pi * 190 * t)) * np.exp(-t / 0.06)


def hat(rng, length=0.06):
    t = seconds(length)
    noise = rng.normal(size=len(t))
    return np.diff(noise, prepend=0.0) * np.exp(-t / 0.015)


def stereo(mono, pan):
    """pan -1 (left) .. 1 (right), equal power."""
    angle = (pan + 1.0) * math.pi / 4.0
    return np.stack([mono * math.cos(angle), mono * math.sin(angle)], axis=1)


def calm_music():
    # 76 beats a minute, the tune twice: plucked melody, the second time an octave lower,
    # a pad of the chords and a soft bass on the bar
    beat = 60.0 / 76.0
    bars = len(CHORDS)
    length = 2 * bars * 4 * beat
    left_right = np.zeros((int(length * RATE) + RATE, 2))
    for rep in range(2):
        offset = rep * bars * 4 * beat
        pos = offset
        for note, beats in TUNE:
            f = freq(note) / (2.0 if rep else 1.0)
            place(left_right, pos, stereo(0.30 * pluck(f, 2.5), 0.25))
            pos += beats * beat
        for bar, (root, notes) in enumerate(CHORDS):
            start = offset + bar * 4 * beat
            place(left_right, start, stereo(0.06 * pad(notes, 4 * beat), -0.3))
            place(left_right, start, stereo(0.35 * pluck(freq(root) / 2.0, 3.0), 0.0))
            place(left_right, start + 2 * beat, stereo(0.2 * pluck(freq(root), 2.0), -0.2))
    return loop_fold(left_right, length)


def battle_music(rng):
    # 132 beats a minute, the tune twice on a reed lead over drums and a driving bass
    beat = 60.0 / 132.0
    bars = len(CHORDS)
    length = 2 * bars * 4 * beat
    left_right = np.zeros((int(length * RATE) + RATE, 2))
    for rep in range(2):
        offset = rep * bars * 4 * beat
        pos = offset
        for note, beats in TUNE:
            f = freq(note) * (2.0 if rep else 1.0)
            place(left_right, pos, stereo(0.16 * brass(f, beats * beat * 0.92), 0.15))
            pos += beats * beat
        for bar, (root, _notes) in enumerate(CHORDS):
            start = offset + bar * 4 * beat
            for eighth in range(8):
                t = start + eighth * beat / 2
                note = freq(root) / 2.0 * (2.0 if eighth % 4 == 3 else 1.0)
                place(left_right, t, stereo(0.25 * brass(note, beat * 0.45), -0.1))
                place(left_right, t, stereo(0.08 * hat(rng), 0.5))
            for b in range(4):
                place(left_right, start + b * beat, stereo(0.7 * (kick() if b % 2 == 0 else 0.6 * snare(rng)),
                                                         0.0 if b % 2 == 0 else -0.15))
    return loop_fold(left_right, length)


def loop_fold(track, length, peak=0.85):
    """Folds what rings past the end of the loop onto its start, so the sound loops seamlessly."""
    n = int(length * RATE)
    out = track[:n].copy()
    tail = track[n:]
    out[:len(tail)] += tail
    return normalize(out, peak)


# --- buoy ------------------------------------------------------------------------------------------

def buoy_texture():
    # left half: the body, red with a white band; right half: dark iron of the frame and the bell
    image = Image.new("RGB", (128, 128), (170, 30, 25))
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 48, 63, 79), fill=(235, 230, 220))
    draw.rectangle((64, 0, 127, 127), fill=(70, 60, 45))
    for y in range(0, 128, 4):
        draw.line((0, y, 63, y), fill=(150, 25, 20) if y % 8 else (180, 40, 30))
    return image


def buoy_meshes(segments=20):
    """A floating cone with a frame of four posts and a bell on top, 6.5 m tall."""
    body = Mesh("body", "demo\\buoy.tga")
    # profile of the body: radius over height, from under the water to the deck of the buoy
    profile = [(1.6, -1.0), (1.8, 0.0), (1.7, 0.8), (1.2, 1.2), (0.0, 1.25)]
    for ring in range(len(profile) - 1):
        (r0, y0), (r1, y1) = profile[ring], profile[ring + 1]
        dr, dy = r1 - r0, y1 - y0
        ln = math.hypot(dr, dy)
        for s in range(segments):
            a0, a1 = 2 * math.pi * s / segments, 2 * math.pi * (s + 1) / segments
            v = []
            for a, r, y, vv in ((a0, r0, y0, ring), (a1, r0, y0, ring), (a1, r1, y1, ring + 1), (a0, r1, y1, ring + 1)):
                n = (math.cos(a) * dy / ln, -dr / ln, math.sin(a) * dy / ln)
                v.append(body.vertex((r * math.cos(a), y, r * math.sin(a)), n, (s / segments * 0.49, vv / 4.0)))
            body.quad(*v)
    iron = Mesh("frame", "demo\\buoy.tga")

    def box(lo, hi):
        corners = [(x, y, z) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
        faces = (((0, 1, 3, 2), (-1, 0, 0)), ((4, 6, 7, 5), (1, 0, 0)), ((0, 4, 5, 1), (0, -1, 0)),
                 ((2, 3, 7, 6), (0, 1, 0)), ((0, 2, 6, 4), (0, 0, -1)), ((1, 5, 7, 3), (0, 0, 1)))
        for face, normal in faces:
            iron.quad(*(iron.vertex(corners[i], normal, (0.75, 0.5)) for i in face))

    for x in (-0.7, 0.7):
        for z in (-0.7, 0.7):
            box((x - 0.07, 1.1, z - 0.07), (x + 0.07, 4.2, z + 0.07))
    box((-0.8, 4.2, -0.8), (0.8, 4.35, 0.8))
    # the bell hangs under the top of the frame
    for s in range(segments):
        a0, a1 = 2 * math.pi * s / segments, 2 * math.pi * (s + 1) / segments
        quad = []
        for a, r, y in ((a0, 0.45, 3.1), (a1, 0.45, 3.1), (a1, 0.25, 4.15), (a0, 0.25, 4.15)):
            quad.append(iron.vertex((r * math.cos(a), y, r * math.sin(a)), (math.cos(a), 0.2, math.sin(a)),
                                    (0.75, 0.5)))
        iron.quad(*quad)
    # modelled at 5 m, drawn half as big again to be seen from the ship's camera
    for mesh in (body, iron):
        mesh.vertices = [(tuple(1.5 * c for c in pos), n, uv, bone) for pos, n, uv, bone in mesh.vertices]
    return [body, iron]


# --- configs ---------------------------------------------------------------------------------------

ALIASES = """; generated by make_sound.py
; an alias is a name the scripts play instead of a file: the service picks one of the names at
; random (by the weight after the comma) and takes the distances, the volume and the priority from here
[cannon]
name = demo\\cannon1.wav, 1.0
name = demo\\cannon2.wav, 1.0
name = demo\\cannon3.wav, 1.0
minDistance = 15.0
maxDistance = 400.0
volume = 1.0
prior = 64

[gull]
name = demo\\gull1.wav, 1.0
name = demo\\gull2.wav, 1.0
name = demo\\gull3.wav, 0.5
volume = 0.45
"""

SCHEMES = """; generated by make_sound.py
; a scheme is the ambient background of a place: ch = sound or alias, min and max seconds between
; plays, volume; ch_loop = a sound looped from the moment the scheme is set (the demo loops its surf
; itself, see sound.c)
[sea_day]
ch = gull, 3, 9, 0.45
"""


def main():
    assets, resource = Path(sys.argv[1]), Path(sys.argv[2])
    rng = np.random.default_rng(1720)
    sounds = resource / "sounds"
    write_wav(sounds / "demo" / "bell.wav", bell())
    for i, pitch in enumerate((1.0, 0.85, 1.15), 1):
        write_wav(sounds / "demo" / f"cannon{i}.wav", cannon(rng, pitch))
    for i, calls in enumerate((2, 3, 1), 1):
        write_wav(sounds / "demo" / f"gull{i}.wav", gull(rng, calls))
    write_wav(sounds / "demo" / "waves.wav", waves(rng))
    write_wav(sounds / "music" / "calm.wav", calm_music())
    write_wav(sounds / "music" / "battle.wav", battle_music(rng))

    (resource / "ini" / "aliases").mkdir(parents=True, exist_ok=True)
    (resource / "ini" / "aliases" / "demo.ini").write_text(ALIASES.replace("\n", "\r\n"))
    (resource / "ini" / "sound_scheme.ini").write_text(SCHEMES.replace("\n", "\r\n"))

    write_tx(buoy_texture(), resource / "textures" / "demo" / "buoy.tga.tx")
    write_gm(buoy_meshes(), resource / "models" / "demo" / "buoy.gm")
    write_fonts({name: (assets / ttf, size, outline) for name, (ttf, size, outline) in FONTS.items()}, resource,
                "generated by make_sound.py from PT Serif (SIL Open Font License)")


if __name__ == "__main__":
    main()
