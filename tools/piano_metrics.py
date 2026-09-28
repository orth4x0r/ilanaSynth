"""M8.2: compare piano notes with a reference grand across the keyboard.

usage:
  python tools/piano_metrics.py <ours folder> [reference folder] [--notes 21,60,...] [--detail]

Ours: ours.<dyn>.n<midi>.wav (ILANA_NOTE_DEBUG with ILANA_NOTE_SET=keyboard).
Reference: piano_<midi>_<dyn>.wav (default build/reference/piano/SalamanderGrandPiano).

The error follows the ear more than tools/fit_piano.py did:
  envelope  third-octave spectral envelope at 0-60 ms, 0.3 s, 1 s and 3 s,
            each relative to the note's own onset level (so colour, not level)
  partials  the first partials' level against time (their decay curves),
            relative to the onset's strongest partial
  beating   how much the first partials' envelopes wobble around a smooth decay
  noise     what lies between the partials in the attack (knock, hammer)
  dynamics  pp -> mf -> ff loudness steps, per note
  balance   each note's mf loudness against C4's
  stretch   the partials' measured inharmonicity
Per-register means (bass < 48, middle, treble > 72) are printed with --detail.
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from analyse_note import load  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
REF = os.path.join(ROOT, "build", "reference", "piano", "SalamanderGrandPiano")
DYNAMICS = ["pp", "mf", "ff"]
ALL_NOTES = list(range(21, 109, 3))
BANDS = 400.0 * 2.0 ** (np.arange(-10, 16) / 3.0)  # 40 Hz .. 12.7 kHz
ENVELOPE_TIMES = [(0.0, 0.06), (0.25, 0.35), (0.9, 1.1), (2.8, 3.2)]
PARTIAL_TIMES = [0.03, 0.15, 0.4, 1.0, 2.0, 3.5]
WEIGHTS = {"envelope": 1.0, "partials": 1.0, "beating": 0.5, "noise": 0.3, "dynamics": 2.0, "balance": 1.0, "stretch": 0.3}

_cache = {}


def load_mono(path, seconds=6.0):
    x, rate = load(path)
    x = x[: int(seconds * rate)]
    # The top octave's samples stop early: pad with silence.
    return np.pad(x, (0, max(0, int(seconds * rate) - len(x)))), rate


def band_levels(frame, rate):
    spectrum = np.abs(np.fft.rfft(frame * np.hanning(len(frame)))) ** 2
    freqs = np.fft.rfftfreq(len(frame), 1 / rate)
    out = []
    for centre in BANDS:
        lo, hi = centre * 2 ** (-1 / 6), centre * 2 ** (1 / 6)
        band = (freqs >= lo) & (freqs < hi)
        out.append(10 * np.log10(spectrum[band].sum() + 1e-15) if band.any() else -150.0)
    return np.array(out)


def find_f0(x, rate, nominal):
    size = 1 << 15
    frame = x[int(0.05 * rate): int(0.05 * rate) + size]
    frame = np.pad(frame, (0, max(0, size - len(frame))))
    spectrum = np.abs(np.fft.rfft(frame * np.hanning(size)))
    freqs = np.fft.rfftfreq(size, 1 / rate)
    best, best_score = nominal, -1.0
    for cents in np.arange(-60, 61, 2):
        f0 = nominal * 2 ** (cents / 1200)
        score = 0.0
        for n in range(1, 7):
            k = int(round(n * f0 / freqs[1]))
            if k + 3 < len(spectrum):
                score += spectrum[k - 3:k + 4].max()
        if score > best_score:
            best, best_score = f0, score
    return best


def partial_track(x, rate, f0, partials):
    """Levels (dB) of partials 1..N at PARTIAL_TIMES, their frequencies at 0.15 s,
    and the between-partial level in the attack."""
    size = 1 << 13 if f0 > 200 else 1 << 14
    window = np.hanning(size)
    freqs = np.fft.rfftfreq(size, 1 / rate)
    levels, ratios, noise = [], [], None
    for t in PARTIAL_TIMES:
        start = int(t * rate)
        frame = x[start:start + size]
        frame = np.pad(frame, (0, max(0, size - len(frame))))
        spectrum = np.abs(np.fft.rfft(frame * window)) + 1e-12
        row, rrow, gaps = [], [], []
        for n in range(1, partials + 1):
            centre = n * f0 * np.sqrt(1 + 4e-4 * n * n) if f0 < 300 else n * f0
            band = (freqs >= centre * 0.96) & (freqs <= centre * 1.06)
            k = np.argmax(spectrum * band)
            row.append(20 * np.log10(spectrum[k]))
            rrow.append(freqs[k] / (n * f0))
            gap = (freqs > (n + 0.35) * f0) & (freqs < (n + 0.65) * f0)
            if gap.any():
                gaps.append(np.mean(spectrum[gap] ** 2))
        levels.append(row)
        if abs(t - 0.15) < 1e-9:
            ratios = rrow
        if noise is None:
            noise = 10 * np.log10(np.mean(gaps) + 1e-24)
    return np.array(levels), np.array(ratios), noise


def beating(x, rate, f0, partials=3):
    """Mean wobble (dB) of the first partials' envelopes about a quadratic fit, 0.3-4 s."""
    from scipy.signal import butter, sosfiltfilt
    out = []
    t = np.arange(len(x)) / rate
    for n in range(1, partials + 1):
        f = n * f0
        if f > rate * 0.4:
            break
        lo, hi = f * 0.985, f * 1.015 + 3
        sos = butter(2, [lo, hi], btype="band", fs=rate, output="sos")
        band = sosfiltfilt(sos, x)
        env = np.abs(band)
        hop = int(rate * 0.02)
        env = np.array([env[i:i + hop].max() for i in range(0, len(env) - hop, hop)])
        tt = t[:len(env) * hop:hop][:len(env)]
        mask = (tt > 0.3) & (tt < 4.0)
        db = 20 * np.log10(env[mask] + 1e-9)
        if len(db) < 20:
            continue
        fit = np.polyval(np.polyfit(tt[mask], db, 2), tt[mask])
        out.append(np.std(db - fit))
    return float(np.mean(out)) if out else 0.0


def features(path, midi):
    key = (path, os.path.getmtime(path))
    if key in _cache:
        return _cache[key]
    x, rate = load_mono(path)
    nominal = 440.0 * 2 ** ((midi - 69) / 12)
    f0 = find_f0(x, rate, nominal)
    partials = int(np.clip(4000 / f0, 3, 16))
    onset = band_levels(x[: int(0.06 * rate)], rate)
    reference = 10 * np.log10(np.sum(10 ** (onset / 10)))
    envelope = []
    for a, b in ENVELOPE_TIMES:
        frame = x[int(a * rate): int(b * rate)]
        envelope.append(np.maximum(band_levels(frame, rate) - reference, -70))
    levels, ratios, noise = partial_track(x, rate, f0, partials)
    loud = 10 * np.log10(np.mean(x[: int(0.5 * rate)] ** 2) + 1e-15)
    result = {
        "envelope": np.array(envelope),
        "levels": levels,
        "ratios": ratios,
        "noise": noise - levels[0].max(),
        "loud": loud,
        "beat": beating(x, rate, f0),
        "f0": f0,
    }
    _cache[key] = result
    return result


def compare(ours_folder, ref_folder=REF, notes=ALL_NOTES, dynamics=DYNAMICS, detail=False):
    parts = {k: [] for k in WEIGHTS}
    per_note = {}
    feats = {}
    for midi in notes:
        for dyn in dynamics:
            ours_path = os.path.join(ours_folder, f"ours.{dyn}.n{midi}.wav")
            ref_path = os.path.join(ref_folder, f"piano_{midi}_{dyn}.wav")
            if not (os.path.exists(ours_path) and os.path.exists(ref_path)):
                continue
            feats[(midi, dyn)] = (features(ours_path, midi), features(ref_path, midi))

    for (midi, dyn), (a, b) in feats.items():
        e = {}
        # Only bands the reference has above its floor.
        audible = b["envelope"] > -60
        e["envelope"] = float(np.mean(((a["envelope"] - b["envelope"])[audible]).clip(-40, 40) ** 2))
        pa, pb = a["levels"] - a["levels"][0].max(), b["levels"] - b["levels"][0].max()
        mask = pb[0] > -50
        e["partials"] = float(np.mean((np.maximum(pa, -60) - np.maximum(pb, -60))[:, mask] ** 2))
        e["beating"] = (a["beat"] - b["beat"]) ** 2
        e["noise"] = float(np.clip(a["noise"] - b["noise"], -40, 40) ** 2)
        n = min(len(a["ratios"]), len(b["ratios"]), 10)
        e["stretch"] = float(np.mean(((a["ratios"][:n] - b["ratios"][:n]) * 1200) ** 2)) if midi < 90 else 0.0
        e["stretch"] = min(e["stretch"], 1600.0)
        for k in ("envelope", "partials", "beating", "noise", "stretch"):
            parts[k].append(e[k])
        per_note[(midi, dyn)] = e

    for midi in notes:
        present = [d for d in dynamics if (midi, d) in feats]
        for lo, hi in (("pp", "mf"), ("mf", "ff")):
            if lo in present and hi in present:
                da = feats[(midi, hi)][0]["loud"] - feats[(midi, lo)][0]["loud"]
                db = feats[(midi, hi)][1]["loud"] - feats[(midi, lo)][1]["loud"]
                parts["dynamics"].append((da - db) ** 2)
        if (midi, "mf") in feats and (60, "mf") in feats:
            ba = feats[(midi, "mf")][0]["loud"] - feats[(60, "mf")][0]["loud"]
            bb = feats[(midi, "mf")][1]["loud"] - feats[(60, "mf")][1]["loud"]
            parts["balance"].append((ba - bb) ** 2)

    total = sum(WEIGHTS[k] * float(np.mean(v)) for k, v in parts.items() if v)
    if not detail:
        return total
    summary = {k: round(float(np.mean(v)), 2) for k, v in parts.items() if v}
    registers = {}
    for name, test in (("bass", lambda m: m < 48), ("middle", lambda m: 48 <= m <= 72), ("treble", lambda m: m > 72)):
        rows = [e for (m, _), e in per_note.items() if test(m)]
        if rows:
            registers[name] = {k: round(float(np.mean([r[k] for r in rows])), 1) for k in rows[0]}
    return total, summary, registers, feats


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    notes = ALL_NOTES
    for a in sys.argv[1:]:
        if a.startswith("--notes="):
            notes = [int(v) for v in a.split("=", 1)[1].split(",")]
    ours = args[0]
    ref = args[1] if len(args) > 1 else REF
    total, summary, registers, feats = compare(ours, ref, notes, detail=True)
    print(f"error {total:.1f}")
    print("parts", json.dumps(summary))
    for name, row in registers.items():
        print(f"  {name:7s}", json.dumps(row))
    if "--detail" in sys.argv:
        print("\nnote dyn   loud(ours-ref)  beat(ours/ref)  noise(ours/ref)  B-ish(p5 ratio ours/ref)")
        for (midi, dyn), (a, b) in sorted(feats.items()):
            r5a = a["ratios"][4] if len(a["ratios"]) > 4 else float("nan")
            r5b = b["ratios"][4] if len(b["ratios"]) > 4 else float("nan")
            print(f"{midi:4d} {dyn}  {a['loud'] - b['loud']:7.1f}   {a['beat']:5.2f}/{b['beat']:5.2f}   "
                  f"{a['noise']:6.1f}/{b['noise']:6.1f}   {r5a:.4f}/{r5b:.4f}")


if __name__ == "__main__":
    main()
