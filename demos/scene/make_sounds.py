"""Builds the sounds of the scene demo into resource/sounds/demo: the surf and the bell of the buoy,
synthesized by the generator of the sound demo, nothing recorded or downloaded. They are mono: a sound
source of the scene is placed in 3D, and the service turns a mono sound round the listener.

Usage: make_sounds.py <resource dir>
"""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "sound"))
from make_sound import bell, normalize, waves, write_wav  # noqa: E402


def main():
    sounds = Path(sys.argv[1]) / "sounds" / "demo"
    rng = np.random.default_rng(1720)
    write_wav(sounds / "waves.wav", normalize(waves(rng).mean(axis=1), 0.6))
    write_wav(sounds / "bell.wav", bell())


if __name__ == "__main__":
    main()
