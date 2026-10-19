#!/usr/bin/env python3
"""Fit the plucked-string model (Physical oscillator, string mode) to a real instrument.

usage:
  python tools/fit_string.py notes   <recording.aif> <first midi note> [count]   list the notes found in a run
  python tools/fit_string.py fit     <name> [iterations]                         coordinate search
  python tools/fit_string.py render  <name> <out dir>                            model clips for the A/B page

A recording is one University of Iowa MIS run (chromatic notes, one file).
The notes are cut at their onsets; each is compared with the model rendered
by ilanaKnobAudit (Init patch, osc1 string mode) at the same pitch and a
matching velocity: 1/3-octave levels in four time segments, harmonic levels,
and the decay rate of partials 1-6. Level is normalised away (only the shape
is fitted). A parallel coordinate search keeps whatever lowers the error.
Progress goes to build/fit-string/<name>/log.txt, the best settings to
best.json there.
"""
import concurrent.futures
import json
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import knob_audit as ka  # noqa: E402
from analyse_note import read_aiff  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "build", "fit-string")
TOOL = os.environ.get("KNOB_TOOL", os.path.join(ROOT, "build", "ilanaKnobAudit_artefacts", "Release", "ilanaKnobAudit"))
SR = ka.SR
HOLD, TOTAL = 3.0, 4.0

# instrument -> recording notes: (file, first midi, count, velocity) and the exciter to start from
INSTRUMENTS = {
    "guitar": {
        "runs": [("Guitar.mf.sulD.D3B3.stereo.aif", 50, 10, 90), ("Guitar.mf.sulE.E2B2.stereo.aif", 40, 8, 90),
                 ("Guitar.mf.sulG.C5Db5.stereo.aif", 72, 2, 90)],
        "pick": {"E2": 40, "B2": 47, "D3": 50, "A3": 57, "C5": 72},  # notes compared (a subset of the runs)
        "base": {"osc1_excite": 0, "osc1_unison": 1, "subosc_on": 0},
    },
}

# name, low, high, step
PARAMETERS = [
    ("osc1_string_decay", 0.0, 1.0, 0.3),
    ("osc1_string_damp", 0.0, 1.0, 0.3),
    ("osc1_string_stiffness", 0.0, 1.0, 0.3),
    ("osc1_string_excite_pos", 0.0, 1.0, 0.3),
    ("osc1_string_pick_hardness", 0.0, 1.0, 0.3),
    ("osc1_string_pickup", 0.0, 1.0, 0.3),
]
START = {"osc1_string_decay": 0.6, "osc1_string_damp": 0.35, "osc1_string_stiffness": 0.3,
         "osc1_string_excite_pos": 0.2, "osc1_string_pick_hardness": 0.5, "osc1_string_pickup": 0.0}


def segment(x, count):
    """Cut a run of notes at their onsets; returns a list of (start, end) sample indexes."""
    hop = int(0.01 * SR)
    env = np.array([np.sqrt(np.mean(x[i:i + hop] ** 2)) for i in range(0, len(x) - hop, hop)])
    db = 20 * np.log10(env + 1e-9)
    onsets, quiet_run = [], 10**6
    for i in range(len(db)):
        if db[i] < -75:
            quiet_run += 1
        else:
            if quiet_run >= 20:
                onsets.append(i)
            quiet_run = 0
    spans = []
    for k, o in enumerate(onsets[:count]):
        end = onsets[k + 1] - 1 if k + 1 < len(onsets) else len(db)
        spans.append((max(0, (o - 1) * hop), min(end * hop, (o + 600) * hop)))
    return spans


def to_rate(x, rate):
    if rate == SR:
        return x
    if rate % SR == 0:                       # exact multiple: average the samples (a cheap low-pass)
        k = rate // SR
        return x[:len(x) // k * k].reshape(-1, k).mean(axis=1)
    n = int(len(x) * SR / rate)
    return np.interp(np.arange(n) * rate / SR, np.arange(len(x)), x)


def partial_decay(x, note):
    """dB per second of partials 1..6 over 0.15-2 s (more negative = faster)."""
    f0 = 440.0 * 2 ** ((note - 69) / 12)
    out = []
    frames = np.arange(0.15, 2.0, 0.15)
    for n in range(1, 7):
        c = n * f0
        if c * 1.1 > SR * 0.45:
            out.append(0.0)
            continue
        levels = []
        for t in frames:
            seg = x[int(t * SR):int((t + 0.12) * SR)]
            if len(seg) < 1024:
                levels.append(-120.0)
                continue
            N = 1 << 15
            spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), N))
            f = np.fft.rfftfreq(N, 1 / SR)
            m = (f > c * 0.97) & (f < c * 1.04)
            levels.append(20 * np.log10(spec[m].max() + 1e-9))
        levels = np.array(levels)
        keep = levels > levels.max() - 55
        out.append(float(np.polyfit(frames[keep], levels[keep], 1)[0]) if keep.sum() >= 3 else -60.0)
    return np.array(out)


def features(x, note):
    x = x / (np.abs(x[:int(0.3 * SR)]).max() + 1e-9)
    b = ka.bands(x)
    ref = b[1].max()                       # normalise by the loudest band of the 30-150 ms segment
    shape = np.concatenate([(seg - ref) for seg in b[:3]])
    harm = ka.harmonics(x, note)
    harm = harm - harm.max()
    return {"bands": shape, "harm": harm, "decay": partial_decay(x, note)}


def distance(a, b):
    e_bands = np.sqrt(np.mean((np.clip(a["bands"], -80, 0) - np.clip(b["bands"], -80, 0)) ** 2))
    e_harm = np.sqrt(np.mean((np.clip(a["harm"], -60, 0) - np.clip(b["harm"], -60, 0)) ** 2))
    e_decay = np.sqrt(np.mean((a["decay"] - b["decay"]) ** 2))
    return {"bands": e_bands, "harm": e_harm, "decay": e_decay, "total": e_bands + 0.7 * e_harm + 1.2 * e_decay}


def reference(name):
    path = os.path.join(OUT, name, "reference.npz")
    if os.path.exists(path):
        z = np.load(path, allow_pickle=True)
        return z["data"].item()
    spec = INSTRUMENTS[name]
    if "_" in name and not os.path.exists(os.path.join(OUT, name.split("_")[0], "reference.npz")):
        reference(name.split("_")[0])
    if "_" in name:
        src = np.load(os.path.join(OUT, name.split("_")[0], "reference.npz"), allow_pickle=True)["data"].item()
        os.makedirs(os.path.dirname(path), exist_ok=True)
        np.savez(path, data=np.array(src, dtype=object))
        return src
    data = {}
    want = set(spec["pick"].values())
    for fname, first, count, velocity in spec["runs"]:
        x, rate = read_aiff(os.path.join(ROOT, "build", "reference", name.split("_")[0], fname))
        x = to_rate(x, rate)
        for k, (a, b) in enumerate(segment(x, count)):
            note = first + k
            if note in want:
                y = x[a:b]
                peak = np.abs(y[:int(0.3 * SR)]).max()
                start = int(np.argmax(np.abs(y) > 0.15 * peak))
                data[note] = {"audio": y[max(0, start - int(0.002 * SR)):], "velocity": velocity}
    for note in data:
        data[note]["features"] = features(data[note]["audio"], note)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    np.savez(path, data=np.array(data, dtype=object))
    return data


INSTRUMENTS["cello"] = {
    "runs": [("Cello.arco.mf.sulC.C2Ab2.aiff", 36, 9, 90), ("Cello.arco.mf.sulD.D3B3.aiff", 50, 10, 90),
             ("Cello.arco.mf.sulA.C4C5.aiff", 60, 12, 90)],
    "pick": {"D2": 38, "Ab2": 44, "D3": 50, "A3": 57, "E4": 64, "A4": 69},
    "base": {"osc1_excite": 4, "osc1_unison": 1, "subosc_on": 0},
    "params": [("osc1_bow_pressure", 0.0, 1.0, 0.3), ("osc1_bow_speed", 0.0, 1.0, 0.3), ("osc1_string_decay", 0.0, 1.0, 0.3),
               ("osc1_string_damp", 0.0, 1.0, 0.3), ("osc1_string_stiffness", 0.0, 1.0, 0.3),
               ("osc1_string_excite_pos", 0.0, 1.0, 0.3), ("osc1_string_pickup", 0.0, 1.0, 0.3)],
    "start": {"osc1_bow_pressure": 0.5, "osc1_bow_speed": 0.5, "osc1_string_decay": 0.8, "osc1_string_damp": 0.35,
              "osc1_string_stiffness": 0.3, "osc1_string_excite_pos": 0.2, "osc1_string_pickup": 0.0},
}

# the fitted guitar plus the Guitar body (phase 3): only the body's knobs are searched
INSTRUMENTS["guitar_body"] = {**INSTRUMENTS["guitar"],
    "base": {"osc1_excite": 0, "osc1_unison": 1, "subosc_on": 0, "res_on": 1, "body_type": 5},
    "params": [("res_amount", 0.0, 1.0, 0.3), ("res_decay", 0.0, 1.0, 0.3),
               ("body_material", 0.0, 1.0, 0.3), ("body_size", 0.0, 1.0, 0.3)],
    "start": {**{"osc1_string_decay": 0.9085, "osc1_string_damp": 0.425, "osc1_string_stiffness": 0.505, "osc1_string_excite_pos": 0.525, "osc1_string_pick_hardness": 0.0037, "osc1_string_pickup": 0.785}, "res_amount": 0.3, "res_decay": 0.3, "body_material": 0.2, "body_size": 0.5}}

for _e in (1, 2, 3, 5):                      # the same recording fitted from another exciter: guitar_2 etc.
    INSTRUMENTS[f"guitar_{_e}"] = {**INSTRUMENTS["guitar"], "base": {"osc1_excite": _e, "osc1_unison": 1, "subosc_on": 0}}


def job_line(tag, note, velocity, settings):
    s = ";".join(f"{k}={v}" for k, v in settings.items())
    return f"{tag}|{note}|{velocity}|{HOLD}|{TOTAL}|{s}"


def render_many(work, jobs, folder):
    """jobs: list of (tag, note, velocity, settings); returns {tag: audio}."""
    os.makedirs(folder, exist_ok=True)
    n = 4
    procs = []
    for i in range(n):
        p = os.path.join(folder, f"jobs{i}.txt")
        open(p, "w").write("\n".join(job_line(*j) for j in jobs[i::n]) + "\n")
        procs.append(subprocess.Popen([TOOL, p, folder], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
    for p in procs:
        p.wait()
    return {tag: ka.read(os.path.join(folder, f"{tag}.wav")) for tag, *_ in jobs}


def evaluate(name, ref, candidates, folder):
    """candidates: list of settings dicts; returns a list of (error dict, per-note errors)."""
    jobs = []
    for ci, settings in enumerate(candidates):
        for note, r in ref.items():
            jobs.append((f"c{ci}_n{note}", note, r["velocity"], {**INSTRUMENTS[name]["base"], **settings}))
    audio = render_many(name, jobs, folder)
    results = []
    for ci in range(len(candidates)):
        errs = [distance(ref[note]["features"], features(audio[f"c{ci}_n{note}"], note)) for note in ref]
        results.append({k: float(np.mean([e[k] for e in errs])) for k in errs[0]})
    return results


def fit(name, iterations):
    """Cycles over the parameters; each gets a line search over its whole range (9 values),
    then a finer one around the best, and keeps the best. Several cycles."""
    ref = reference(name)
    folder = os.path.join(OUT, name)
    os.makedirs(folder, exist_ok=True)
    best_path = os.path.join(folder, "best.json")
    params = INSTRUMENTS[name].get("params", PARAMETERS)
    current = json.load(open(best_path)) if os.path.exists(best_path) else dict(INSTRUMENTS[name].get("start", START))
    score = evaluate(name, ref, [current], os.path.join(folder, "r"))[0]
    log = open(os.path.join(folder, "log.txt"), "a")
    print(f"start {score}", file=log, flush=True)
    width = 1.0
    for it in range(iterations):
        improved = False
        for p, lo, hi, _ in params:
            cands = []
            for v in np.linspace(max(lo, current[p] - width / 2), min(hi, current[p] + width / 2), 9):
                v = round(float(v), 4)
                if v != current[p]:
                    cands.append({**current, p: v})
            res = evaluate(name, ref, cands, os.path.join(folder, "r"))
            i = int(np.argmin([r["total"] for r in res]))
            if res[i]["total"] < score["total"] - 1e-3:
                current, score, improved = cands[i], res[i], True
                json.dump(current, open(best_path, "w"), indent=1)
                print(f"cycle {it} {p} -> {current[p]}: {score}", file=log, flush=True)
        if not improved:
            width /= 2
            print(f"cycle {it}: no gain, width {width}", file=log, flush=True)
            if width < 0.06:
                break
        else:
            width = max(width * 0.6, 0.12)
    json.dump(current, open(best_path, "w"), indent=1)
    print(json.dumps({"best": current, "score": score}))


def render_clips(name, out):
    import wave
    ref = reference(name)
    folder = os.path.join(OUT, name)
    best = json.load(open(os.path.join(folder, "best.json")))
    start = dict(START)
    jobs = []
    for note, r in ref.items():
        jobs.append((f"old_{note}", note, r["velocity"], {**INSTRUMENTS[name]["base"]}))
        jobs.append((f"new_{note}", note, r["velocity"], {**INSTRUMENTS[name]["base"], **best}))
    audio = render_many(name, jobs, os.path.join(folder, "clips"))
    os.makedirs(out, exist_ok=True)

    def write(path, x):
        x = x[:int(TOTAL * SR)]
        fade = int(0.05 * SR)
        x = x.copy()
        x[-fade:] *= np.linspace(1, 0, fade)
        x = x * (0.7 / max(1e-9, np.abs(x).max()))
        w = wave.open(path, "wb")
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())
        w.close()

    for note, r in ref.items():
        write(os.path.join(out, f"{name}_{note}_recording.wav"), r["audio"])
        write(os.path.join(out, f"{name}_{note}_old.wav"), audio[f"old_{note}"])
        write(os.path.join(out, f"{name}_{note}_new.wav"), audio[f"new_{note}"])


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "notes":
        x, rate = read_aiff(sys.argv[2])
        x = to_rate(x, rate)
        first = int(sys.argv[3])
        spans = segment(x, int(sys.argv[4]) if len(sys.argv) > 4 else 99)
        for k, (a, b) in enumerate(spans):
            print(first + k, round(a / SR, 2), round(b / SR, 2))
    elif cmd == "fit":
        fit(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 12)
    elif cmd == "render":
        render_clips(sys.argv[2], sys.argv[3])
