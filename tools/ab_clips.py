"""Builds the audio for the A/B listening page from the critic's renders.

    python tools/ab_clips.py build/critic-v3 build/ab

For each focus category it writes one mono 16-bit 22.05 kHz file,
<out>/<Category>.wav, with every preset's clip back to back, and
<out>/manifest.json: for each category, the presets' index, name, start and
length in seconds. Pads and keys play the held chord, basses, leads and
plucks the eight-note line. Every clip is matched in loudness (-20 dB RMS,
peak kept under -1 dB) so a choice is about the sound, not its level, and
fades in and out over 10 ms.
"""

import csv
import json
import struct
import sys
import wave
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from preset_critic import FOCUS, read_wav  # noqa: E402

OUT_RATE = 22050
CLIP = {"Pad": ("chord.wav", 3.0), "Keys": ("chord.wav", 3.0), "Bass": ("line.wav", 2.4),
        "Lead": ("line.wav", 2.4), "Pluck": ("line.wav", 2.4)}
GAP = 0.05


def resample(x, rate_in, rate_out):
    # Windowed-sinc-free: an FFT band-limit then linear read is plenty for
    # a listening clip at half rate (48 kHz -> 22.05 kHz).
    spectrum = np.fft.rfft(x)
    freqs = np.fft.rfftfreq(len(x), 1.0 / rate_in)
    spectrum[freqs > rate_out * 0.45] = 0.0
    filtered = np.fft.irfft(spectrum, len(x))
    t = np.arange(int(len(x) * rate_out / rate_in)) * (rate_in / rate_out)
    return np.interp(t, np.arange(len(x)), filtered)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    root, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    with open(root / "index.csv", newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    manifest = {}
    for category in FOCUS:
        name, seconds = CLIP[category]
        pieces, entries, position = [], [], 0.0
        for row in (r for r in rows if r["category"] == category):
            left, right = read_wav(root / row["index"] / name)
            mono = 0.5 * (left + right)[: int(seconds * 48000)]
            level = float(np.sqrt(np.mean(mono ** 2)))
            if level < 1e-5:
                continue
            clip = resample(mono, 48000, OUT_RATE)
            clip *= 10 ** (-20 / 20) / max(level, 1e-9)
            peak = float(np.max(np.abs(clip)))
            if peak > 0.89:
                clip *= 0.89 / peak
            fade = int(0.01 * OUT_RATE)
            ramp = np.linspace(0.0, 1.0, fade)
            clip[:fade] *= ramp
            clip[-fade:] *= ramp[::-1]
            entries.append({"index": int(row["index"]), "name": row["name"], "start": round(position, 4),
                            "length": round(len(clip) / OUT_RATE, 4)})
            pieces += [clip, np.zeros(int(GAP * OUT_RATE))]
            position += len(clip) / OUT_RATE + GAP
        audio = np.concatenate(pieces) if pieces else np.zeros(1)
        with wave.open(str(out / f"{category}.wav"), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(OUT_RATE)
            w.writeframes(np.clip(audio * 32767.0, -32768, 32767).astype("<i2").tobytes())
        manifest[category] = entries
        print(f"{category}: {len(entries)} clips, {(out / f'{category}.wav').stat().st_size / 1e6:.1f} MB")
    (out / "manifest.json").write_text(json.dumps(manifest, indent=0), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
