"""Writes a 1.5 s 440 Hz stereo 16-bit WAV used by the smoke test."""
import math
import struct
import sys
import wave

RATE = 44100
SECONDS = 1.5
FADE = 2000  # samples, avoids clicks

with wave.open(sys.argv[1], "wb") as out:
    out.setnchannels(2)
    out.setsampwidth(2)
    out.setframerate(RATE)
    total = int(RATE * SECONDS)
    frames = bytearray()
    for i in range(total):
        envelope = min(1.0, i / FADE, (total - i) / FADE)
        sample = int(8000 * envelope * math.sin(2 * math.pi * 440 * i / RATE))
        frames += struct.pack("<hh", sample, sample)
    out.writeframes(bytes(frames))
