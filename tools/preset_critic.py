"""Preset critic: scores every factory preset from the test phrases that
ilanaPresetRender writes, and says what is wrong with the weak ones.

    ilanaPresetRender build/critic
    python tools/preset_critic.py build/critic [--refs build/critic-refs ...]

It measures level, peak and clipping, DC, brightness, stereo width, movement
over a held note, attack, sustain and release, velocity response, how much
each macro changes the sound, and how cleanly a fast line separates. Each
preset is compared with the other presets in its category (and, with --refs,
with reference renders of other synths' presets in the same category) and
with a few hard rules. The result is build/critic/report.csv, a ranked
report.md, and the common causes across the library.
"""

import csv
import math
import sys
import wave
from pathlib import Path

import numpy as np

RATE = 48000
FOCUS = ["Bass", "Pad", "Lead", "Keys", "Pluck"]


def read_wav(path):
    with wave.open(str(path), "rb") as w:
        channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(frames)
    if width == 3:
        data = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        ints = (data[:, 0].astype(np.int32) | (data[:, 1].astype(np.int32) << 8) | (data[:, 2].astype(np.int32) << 16))
        ints = np.where(ints >= 1 << 23, ints - (1 << 24), ints)
        audio = ints.astype(np.float64) / float(1 << 23)
    elif width == 2:
        audio = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768.0
    else:
        audio = np.frombuffer(raw, dtype=np.int32).astype(np.float64) / float(1 << 31)
    audio = audio.reshape(-1, channels)
    if channels == 1:
        audio = np.repeat(audio, 2, axis=1)
    return audio[:, 0], audio[:, 1]


def db(x):
    return 20.0 * math.log10(max(x, 1e-9))


def rms(x):
    return float(np.sqrt(np.mean(x * x))) if len(x) else 0.0


def loudness(mid):
    """Level of the loudest 400 ms windows (the top half), in dB."""
    size = int(0.4 * RATE)
    if len(mid) < size:
        return db(rms(mid))
    powers = sorted((float(np.mean(mid[i:i + size] ** 2)) for i in range(0, len(mid) - size, size // 4)), reverse=True)
    top = powers[: max(1, len(powers) // 2)]
    return 10.0 * math.log10(max(np.mean(top), 1e-18))


def spectrum(mid, start=0.0, end=None):
    a = int(start * RATE)
    b = len(mid) if end is None else int(end * RATE)
    segment = mid[a:b]
    size = 8192
    if len(segment) < size:
        segment = np.pad(segment, (0, size - len(segment)))
    window = np.hanning(size)
    frames = [np.abs(np.fft.rfft(segment[i:i + size] * window)) for i in range(0, len(segment) - size + 1, size // 2)]
    return np.mean(frames, axis=0)


def centroid(spec):
    freqs = np.fft.rfftfreq((len(spec) - 1) * 2, 1.0 / RATE)
    total = float(np.sum(spec[1:]))
    return float(np.sum(freqs[1:] * spec[1:]) / total) if total > 0 else 0.0


def band_fraction(spec, low, high):
    freqs = np.fft.rfftfreq((len(spec) - 1) * 2, 1.0 / RATE)
    power = spec * spec
    total = float(np.sum(power[1:]))
    mask = (freqs >= low) & (freqs < high)
    return float(np.sum(power[mask]) / total) if total > 0 else 0.0


def envelope(mid, hop=240):
    frames = len(mid) // hop
    return np.array([rms(mid[i * hop:(i + 1) * hop]) for i in range(frames)]), hop / RATE


def frame_features(mid, start, end, size=2048):
    a, b = int(start * RATE), int(end * RATE)
    cents, levels = [], []
    window = np.hanning(size)
    for i in range(a, b - size, size // 2):
        frame = mid[i:i + size]
        spec = np.abs(np.fft.rfft(frame * window))
        level = rms(frame)
        if level < 1e-5:
            continue
        cents.append(centroid(spec))
        levels.append(db(level))
    return np.array(cents), np.array(levels)


def log_spectrum_distance(a, b):
    """Mean absolute difference of two long-term spectra in dB, weighted to
    where there is energy (a macro that only changes silence scores 0)."""
    la, lb = 20 * np.log10(a + 1e-9), 20 * np.log10(b + 1e-9)
    weight = np.maximum(a, b)
    weight = weight / (np.sum(weight) + 1e-12)
    return float(np.sum(np.abs(la - lb) * weight))


def measure(folder):
    m = {}
    nl, nr = read_wav(folder / "note.wav")
    note = 0.5 * (nl + nr)
    cl, cr = read_wav(folder / "chord.wav")
    chord = 0.5 * (cl + cr)
    side = 0.5 * (cl - cr)
    ll, lr = read_wav(folder / "line.wav")
    line = 0.5 * (ll + lr)
    vl, vr = read_wav(folder / "vel.wav")
    vel = 0.5 * (vl + vr)

    everything = np.concatenate([nl, nr, cl, cr, ll, lr])
    m["peak"] = float(np.max(np.abs(everything)))
    m["clip_fraction"] = float(np.mean(np.abs(everything) >= 0.999))
    m["dc"] = float(abs(np.mean(np.concatenate([note, chord]))))
    m["level_db"] = max(loudness(note), loudness(chord), loudness(line))
    m["width"] = rms(side) / max(rms(chord), 1e-9)
    # Width under 150 Hz only (a bass's low end should be mono; unison up top is fine).
    side_spec, mid_spec = spectrum(side, 0.05, 2.0), spectrum(chord, 0.05, 2.0)
    freqs = np.fft.rfftfreq((len(mid_spec) - 1) * 2, 1.0 / RATE)
    low = (freqs > 20) & (freqs < 150)
    m["low_width"] = float(np.sqrt(np.sum(side_spec[low] ** 2) / max(np.sum(mid_spec[low] ** 2), 1e-18)))

    spec = spectrum(note, 0.05, 2.0)
    m["centroid_hz"] = centroid(spec)
    m["low_fraction"] = band_fraction(spec, 20, 150)
    m["high_fraction"] = band_fraction(spec, 8000, 24000)

    env, step = envelope(note)
    peak_env = float(np.max(env)) if len(env) else 0.0
    if peak_env > 1e-6:
        above = np.nonzero(env >= 0.9 * peak_env)[0]
        m["attack_s"] = float(above[0] * step) if len(above) else 0.0
        at = lambda t: float(env[min(len(env) - 1, int(t / step))])
        m["sustain_db"] = db(at(1.8)) - db(peak_env)
        after = env[int(2.0 / step):]
        below = np.nonzero(after < peak_env * 10 ** (-60 / 20))[0]
        m["release_s"] = float(below[0] * step) if len(below) else float(len(after) * step) + 10.0
    else:
        m["attack_s"], m["sustain_db"], m["release_s"] = 0.0, -120.0, 0.0

    cents, levels = frame_features(note, 0.2, 2.0)
    if len(cents) > 3:
        m["movement"] = float(np.std(np.log2(np.maximum(cents, 20.0)))) + float(np.std(levels)) / 12.0
    else:
        m["movement"] = 0.0

    # Velocity: 20 against 127, level and brightness.
    soft, hard = vel[int(0.0 * RATE):int(0.6 * RATE)], vel[int(3.0 * RATE):int(3.6 * RATE)]
    m["vel_db"] = db(rms(hard)) - db(rms(soft))
    m["vel_bright"] = math.log2(max(centroid(spectrum(hard)), 20.0) / max(centroid(spectrum(soft)), 20.0))

    # The line: how far the level dips between notes (a mushy line barely dips).
    line_env, line_step = envelope(line)
    dips = []
    for i in range(1, 8):
        boundary = int((i * 0.25 - 0.02) / line_step)
        before = line_env[max(0, boundary - 8):boundary]
        if len(before):
            dips.append(db(float(np.max(line_env[max(0, boundary - 20):boundary]))) - db(float(np.min(before)) + 1e-9))
    m["line_dip_db"] = float(np.median(dips)) if dips else 0.0

    # Macros: how much each changes the held note.
    for k in range(1, 5):
        lo_l, lo_r = read_wav(folder / f"macro{k}_lo.wav")
        hi_l, hi_r = read_wav(folder / f"macro{k}_hi.wav")
        lo, hi = 0.5 * (lo_l + lo_r), 0.5 * (hi_l + hi_r)
        spec_lo, spec_hi = spectrum(lo, 0.05, 1.5), spectrum(hi, 0.05, 1.5)
        level_change = abs(db(rms(hi)) - db(rms(lo)))
        width_change = abs(rms(0.5 * (hi_l - hi_r)) / max(rms(hi), 1e-9) - rms(0.5 * (lo_l - lo_r)) / max(rms(lo), 1e-9))
        movement_lo = frame_features(lo, 0.1, 1.5)[0]
        movement_hi = frame_features(hi, 0.1, 1.5)[0]
        motion_change = abs((np.std(np.log2(np.maximum(movement_hi, 20))) if len(movement_hi) > 3 else 0)
                            - (np.std(np.log2(np.maximum(movement_lo, 20))) if len(movement_lo) > 3 else 0))
        m[f"macro{k}_effect"] = (log_spectrum_distance(spec_lo, spec_hi) + level_change + 10.0 * width_change
                                 + 6.0 * motion_change)
    return m


def load_index(root):
    with open(root / "index.csv", newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def robust_stats(values):
    values = np.array([v for v in values if np.isfinite(v)])
    if len(values) == 0:
        return 0.0, 1.0
    median = float(np.median(values))
    mad = float(np.median(np.abs(values - median))) * 1.4826
    return median, max(mad, 1e-6)


def judge(rows, refs):
    by_category = {}
    for row in rows:
        by_category.setdefault(row["category"], []).append(row)
    for row in refs:
        by_category.setdefault("ref:" + row["category"], []).append(row)

    def stats(category, key):
        pool = by_category.get("ref:" + category) or by_category.get(category, [])
        return robust_stats([r["m"][key] for r in pool])

    for row in rows:
        m, category = row["m"], row["category"]
        issues = []  # (severity points, cause, detail)

        if m["level_db"] < -45:
            issues.append((40, "silent or nearly silent", f"{m['level_db']:.1f} dB"))
        if m["clip_fraction"] > 0.001:
            issues.append((30, "clips", f"{m['clip_fraction'] * 100:.2f} % of samples at full scale"))
        if m["dc"] > 0.01:
            issues.append((15, "DC offset", f"{m['dc']:.3f}"))

        peers = [r["m"]["level_db"] for r in by_category.get(category, []) if r is not row]
        level_median = float(np.median(peers)) if len(peers) >= 4 else float(np.median([r["m"]["level_db"] for r in rows]))
        off = m["level_db"] - level_median
        if abs(off) > 6:
            issues.append((15, "too quiet" if off < 0 else "too loud", f"{off:+.1f} dB against its category"))
        elif abs(off) > 3:
            issues.append((5, "a little quiet" if off < 0 else "a little loud", f"{off:+.1f} dB against its category"))

        names = [row.get(f"macro{k}", "") for k in range(1, 5)]
        dead = [f"{k} ({names[k - 1] or 'unnamed'})" for k in range(1, 5) if m[f"macro{k}_effect"] < 1.0]
        if dead:
            issues.append((8 * len(dead), "macros that barely change the sound", "macro " + ", ".join(dead)))

        if category in ("Keys", "Pluck", "Bass", "Lead") and m["vel_db"] < 1.5 and abs(m["vel_bright"]) < 0.08:
            issues.append((8, "no velocity response", f"{m['vel_db']:+.1f} dB, {m['vel_bright']:+.2f} oct brightness"))

        if category == "Pluck" and m["sustain_db"] > -12:
            issues.append((12, "doesn't decay like a pluck", f"{m['sustain_db']:.1f} dB after 1.8 s"))
        if category == "Pad" and m["movement"] < 0.02:
            issues.append((8, "static (no movement over a held note)", f"movement {m['movement']:.3f}"))
        if category == "Pad" and m["attack_s"] < 0.02 and m["width"] < 0.15:
            issues.append((5, "thin for a pad (instant attack, narrow)", f"attack {m['attack_s'] * 1000:.0f} ms, width {m['width']:.2f}"))
        if category == "Bass":
            if m["low_width"] > 0.2:
                issues.append((6, "wide bass (low end not mono)", f"width under 150 Hz {m['low_width']:.2f}"))
            if m["low_fraction"] < 0.15:
                issues.append((8, "weak low end for a bass", f"{m['low_fraction'] * 100:.0f} % under 150 Hz"))
        if category in ("Bass", "Pluck", "Lead") and m["line_dip_db"] < 3:
            issues.append((6, "fast notes smear together", f"{m['line_dip_db']:.1f} dB between notes"))
        if category in ("Bass", "Pluck", "Lead") and m["release_s"] > 8:
            issues.append((3, "very long tail", f"{m['release_s']:.1f} s to -60 dB after note-off"))

        for key, label, points in (("centroid_hz", "brightness", 5), ("high_fraction", "harsh top end", 6), ("width", "width", 4)):
            median, spread = stats(category, key)
            value = m[key]
            if key == "centroid_hz":
                value, median = math.log2(max(value, 20)), math.log2(max(median, 20))
                spread = max(spread / max(2 ** median, 1.0), 0.15)
            z = (value - median) / spread
            if key == "high_fraction" and z > 3 and m[key] > 0.02:
                issues.append((points, label, f"{m[key] * 100:.1f} % above 8 kHz (category median {median * 100:.1f} %)"))
            elif key == "centroid_hz" and abs(z) > 3:
                issues.append((points, "much darker than its category" if z < 0 else "much brighter than its category",
                               f"{m['centroid_hz']:.0f} Hz"))

        clap = row.get("clap")
        if clap is not None and category in FOCUS:
            score = float(clap["category_score"])
            # Only where the model is reliable: most reference presets in this
            # category must pass (it hears few real pads or keys as such).
            ref_scores = [float(r["clap"]["category_score"]) for r in by_category.get("ref:" + category, []) if r.get("clap")]
            reliable = len(ref_scores) >= 5 and float(np.median(ref_scores)) >= 0.5
            if reliable and np.isfinite(score) and score < 0.3:
                issues.append((8, "doesn't sound like its category", f"CLAP hears it as {clap['heard_as']} ({score:.2f} {category})"))
            names = [float(r["clap"]["name_score"]) for r in by_category.get(category, []) if r.get("clap")]
            if len(names) >= 8:
                median, spread = robust_stats(names)
                if (float(clap["name_score"]) - median) / spread < -2.5:
                    issues.append((3, "doesn't sound like its name", f"name match {float(clap['name_score']):.2f}, category median {median:.2f}"))

        row["issues"] = sorted(issues, key=lambda issue: -issue[0])
        row["score"] = max(0, 100 - sum(points for points, _, _ in issues))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    root = Path(sys.argv[1])
    refs_roots = [Path(sys.argv[i + 1]) for i, arg in enumerate(sys.argv[:-1]) if arg == "--refs"]

    rows = load_index(root)
    for row in rows:
        row["m"] = measure(root / row["index"])
        print(f"measured {row['index']} {row['name']}", flush=True)
    if (root / "clap.csv").exists():
        with open(root / "clap.csv", newline="", encoding="utf-8") as f:
            clap = {r["index"]: r for r in csv.DictReader(f)}
        for row in rows:
            row["clap"] = clap.get(row["index"])
    refs = []
    for refs_root in refs_roots:
        if not (refs_root / "index.csv").exists():
            continue
        these = load_index(refs_root)
        for row in these:
            row["m"] = measure(refs_root / row["index"])
        if (refs_root / "clap.csv").exists():
            with open(refs_root / "clap.csv", newline="", encoding="utf-8") as f:
                ref_clap = {r["index"]: r for r in csv.DictReader(f)}
            for row in these:
                row["clap"] = ref_clap.get(row["index"])
        refs += these

    judge(rows, refs)

    keys = list(rows[0]["m"].keys())
    with open(root / "report.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["index", "name", "category", "score", "issues"] + keys)
        for row in sorted(rows, key=lambda r: r["score"]):
            writer.writerow([row["index"], row["name"], row["category"], row["score"],
                             "; ".join(f"{cause} ({detail})" for _, cause, detail in row["issues"])]
                            + [f"{row['m'][k]:.4f}" for k in keys])

    causes = {}
    for row in rows:
        for _, cause, _ in row["issues"]:
            causes.setdefault(cause, []).append(row)

    lines = ["# Preset critic report", ""]
    lines.append(f"{len(rows)} presets measured. Score 100 means no issue found; each issue takes points off.")
    lines.append("")
    lines.append("## Common causes")
    lines.append("")
    lines.append("| Cause | Presets | In Bass/Pad/Lead/Keys/Pluck |")
    lines.append("|---|---:|---:|")
    for cause, members in sorted(causes.items(), key=lambda item: -len(item[1])):
        focus = sum(1 for r in members if r["category"] in FOCUS)
        lines.append(f"| {cause} | {len(members)} | {focus} |")
    lines.append("")
    for category in FOCUS + sorted({r["category"] for r in rows} - set(FOCUS)):
        members = sorted((r for r in rows if r["category"] == category), key=lambda r: r["score"])
        if not members:
            continue
        scores = [r["score"] for r in members]
        lines.append(f"## {category} ({len(members)} presets, median score {int(np.median(scores))})")
        lines.append("")
        lines.append("| Score | Preset | Issues |")
        lines.append("|---:|---|---|")
        for r in members:
            text = "; ".join(f"{cause} ({detail})" for _, cause, detail in r["issues"]) or "none found"
            lines.append(f"| {r['score']} | {r['name']} | {text} |")
        lines.append("")
    (root / "report.md").write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {root / 'report.csv'} and {root / 'report.md'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
