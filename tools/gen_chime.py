"""Writes resources/chime.wav: two quick rising bell notes (E6, then B6).

Run from the repository root: python tools/gen_chime.py
"""

import math
import struct
import wave
from pathlib import Path

RATE = 44100
LENGTH = 0.75  # seconds
PEAK = 0.6  # of full scale, so the volume setting has room above it

# (start in seconds, frequency in Hz, loudness)
NOTES = [(0.0, 1318.51, 0.8), (0.09, 1975.53, 1.0)]

# A bell is mostly its fundamental, with overtones that die away faster
PARTIALS = [(1.0, 1.0, 0.30), (2.0, 0.35, 0.12), (3.0, 0.12, 0.06)]  # (ratio, level, decay seconds)
ATTACK = 0.004


def sample(t: float) -> float:
    total = 0.0
    for start, freq, loud in NOTES:
        dt = t - start
        if dt < 0:
            continue
        attack = min(dt / ATTACK, 1.0)
        for ratio, level, decay in PARTIALS:
            total += loud * level * attack * math.exp(-dt / decay) * math.sin(2 * math.pi * freq * ratio * dt)
    return total


def main() -> None:
    count = int(RATE * LENGTH)
    samples = [sample(i / RATE) for i in range(count)]

    # Fade the last 50 ms so the file ends on silence
    fade = int(RATE * 0.05)
    for i in range(fade):
        samples[count - fade + i] *= 1 - i / fade

    scale = PEAK / max(abs(s) for s in samples)
    frames = b"".join(struct.pack("<h", round(s * scale * 32767)) for s in samples)

    out = Path(__file__).resolve().parent.parent / "resources" / "chime.wav"
    with wave.open(str(out), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(frames)
    print(f"Wrote {out} ({count / RATE:.2f} s)")


if __name__ == "__main__":
    main()
