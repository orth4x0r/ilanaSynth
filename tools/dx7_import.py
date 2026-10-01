"""Turns a DX7 voice into an ilanaSynth voicing line (src/PresetVoicing.h).

    python tools/dx7_import.py <bank.syx> <voice 1-32> "<preset name>" [--hold 1.5] [--total 3]

The six operators become oscillators 1-6 (sine, ratio or fixed tuning), the
algorithm becomes the FM matrix, and each operator gets its own envelope
(ENV 6-11) fitted to the operator's gain curve as the msfa engine (Dexed,
build/reference/dx7/render/dx7render.exe) plays the voice at middle C. The
modulation depths come straight from those gains: msfa and ilanaSynth both
measure phase modulation in cycles. Key level scaling comes from the gains
an octave either side. The reference WAV for the fit is written next to the
envelope CSV in build/reference/dx7/.

Prints the voicing line; fit the level afterwards (tune_presets.py).
"""

import argparse
import csv
import math
import subprocess
import sys
from pathlib import Path

import numpy as np
from scipy.optimize import minimize

ROOT = Path(__file__).resolve().parent.parent
RENDER = ROOT / "build" / "reference" / "dx7" / "render" / "dx7render.exe"
OUT = ROOT / "build" / "reference" / "dx7"
PREFIXES = ["osc1", "osc2", "sub", "osc4", "osc5", "osc6"]
SINE_TABLE = 8

# msfa's algorithm table (fm_core.cc): per algorithm, operator flags in
# msfa's order, which runs OP6 first.
ALGORITHMS = [
    [0xc1, 0x11, 0x11, 0x14, 0x01, 0x14], [0x01, 0x11, 0x11, 0x14, 0xc1, 0x14],
    [0xc1, 0x11, 0x14, 0x01, 0x11, 0x14], [0xc1, 0x11, 0x94, 0x01, 0x11, 0x14],
    [0xc1, 0x14, 0x01, 0x14, 0x01, 0x14], [0xc1, 0x94, 0x01, 0x14, 0x01, 0x14],
    [0xc1, 0x11, 0x05, 0x14, 0x01, 0x14], [0x01, 0x11, 0xc5, 0x14, 0x01, 0x14],
    [0x01, 0x11, 0x05, 0x14, 0xc1, 0x14], [0x01, 0x05, 0x14, 0xc1, 0x11, 0x14],
    [0xc1, 0x05, 0x14, 0x01, 0x11, 0x14], [0x01, 0x05, 0x05, 0x14, 0xc1, 0x14],
    [0xc1, 0x05, 0x05, 0x14, 0x01, 0x14], [0xc1, 0x05, 0x11, 0x14, 0x01, 0x14],
    [0x01, 0x05, 0x11, 0x14, 0xc1, 0x14], [0xc1, 0x11, 0x02, 0x25, 0x05, 0x14],
    [0x01, 0x11, 0x02, 0x25, 0xc5, 0x14], [0x01, 0x11, 0x11, 0xc5, 0x05, 0x14],
    [0xc1, 0x14, 0x14, 0x01, 0x11, 0x14], [0x01, 0x05, 0x14, 0xc1, 0x14, 0x14],
    [0x01, 0x14, 0x14, 0xc1, 0x14, 0x14], [0xc1, 0x14, 0x14, 0x14, 0x01, 0x14],
    [0xc1, 0x14, 0x14, 0x01, 0x14, 0x04], [0xc1, 0x14, 0x14, 0x14, 0x04, 0x04],
    [0xc1, 0x14, 0x14, 0x04, 0x04, 0x04], [0xc1, 0x05, 0x14, 0x01, 0x14, 0x04],
    [0x01, 0x05, 0x14, 0xc1, 0x14, 0x04], [0x04, 0xc1, 0x11, 0x14, 0x01, 0x14],
    [0xc1, 0x14, 0x01, 0x14, 0x04, 0x04], [0x04, 0xc1, 0x11, 0x14, 0x04, 0x04],
    [0xc1, 0x14, 0x04, 0x04, 0x04, 0x04], [0xc4, 0x04, 0x04, 0x04, 0x04, 0x04],
]


def unpack(bank, voice):
    """The packed 128-byte voice as a dict (operators listed op1..op6)."""
    p = bank[6 + 128 * (voice - 1): 6 + 128 * voice]
    ops = []
    for k in range(6):  # VMEM stores OP6 first
        s = p[k * 17: k * 17 + 17]
        ops.append(dict(rates=list(s[0:4]), levels=list(s[4:8]), bp=s[8], ld=s[9], rd=s[10],
                        lc=s[11] & 3, rc=(s[11] >> 2) & 3, rs=s[12] & 7, det=(s[12] >> 3) & 15,
                        ams=s[13] & 3, kvs=(s[13] >> 2) & 7, ol=s[14], mode=s[15] & 1,
                        coarse=(s[15] >> 1) & 31, fine=s[16]))
    ops.reverse()
    return dict(ops=ops, alg=(p[110] & 31) + 1, fb=p[111] & 7, name=bytes(p[118:128]).decode("ascii", "replace"),
                transpose=p[117], lfo=dict(speed=p[112], delay=p[113], pmd=p[114], amd=p[115],
                                            wave=(p[116] >> 1) & 7, pms=(p[116] >> 4) & 7))


def routing(alg):
    """(modulations {(source, target)}, carriers, feedback op), ops numbered 1-6."""
    flags = ALGORITHMS[alg - 1]
    buses = {1: set(), 2: set()}
    mods, carriers, feedback = set(), [], None
    for index, f in enumerate(flags):
        op = 6 - index
        inbus, outbus, add = (f >> 4) & 3, f & 3, bool(f & 4)
        if inbus:
            for source in buses[inbus]:
                mods.add((source, op))
        if (f & 0xc0) == 0xc0:
            feedback = op
        if outbus == 0:
            carriers.append(op)
        else:
            buses[outbus] = (buses[outbus] | {op}) if add else {op}
    return mods, carriers, feedback


def render(bank_path, voice, note, velocity, hold, total, tag, transpose=0):
    # Files are named by the key played; the engine hears it transposed, as
    # a DX7 does (msfa leaves TRANSPOSE to the host).
    wav = OUT / f"{tag}_{note}.wav"
    env = OUT / f"{tag}_{note}.csv"
    subprocess.run([str(RENDER), str(bank_path), str(voice), str(note + transpose), str(velocity), str(hold), str(total),
                    str(wav), str(env)], check=True)
    with open(env, newline="") as f:
        rows = list(csv.reader(f))[1:]
    data = np.array([[float(x) for x in r] for r in rows])
    return data[:, 0], data[:, 1:]


def adsr(t, a, d, s, r, curve, hold):
    e = 2.0 ** (-2.0 * curve)
    v = np.empty_like(t)
    attack = t < a
    v[attack] = (t[attack] / a) ** e
    decay = (t >= a) & (t < a + d)
    v[decay] = s + (1 - s) * (1 - (t[decay] - a) / d) ** e
    v[(t >= a + d)] = s
    held = t < hold
    at_hold = v[held][-1] if np.any(held) else 0.0
    rel = t >= hold
    p = np.clip((t[rel] - hold) / r, 0, 1)
    v[rel] = at_hold * (1 - p) ** e
    return v


def fit_envelope(t, g, hold, log_weight=0.15):
    peak = float(g[t < hold].max())
    e = g / peak
    reach = np.argmax(e >= 0.95)
    a0 = max(0.0006, float(t[reach]))
    s0 = float(np.clip(e[t < hold][-1], 0, 1))
    after = np.where((t >= a0) & (e <= s0 + 0.37 * (1 - s0)))[0]
    d0 = max(0.01, float(t[after[0]] - a0) * 2.5) if len(after) else hold
    tail = np.where((t >= hold) & (e <= 0.001))[0]
    r0 = max(0.01, float(t[tail[0]] - hold)) if len(tail) else 2.0

    def loss(x):
        lim = lambda v: math.exp(min(3.0, max(-8.0, v)))
        a, d, s, r, c = lim(x[0]), lim(x[1]), min(1, max(0, x[2])), lim(x[3]), max(-1, min(1, x[4]))
        m = adsr(t, a, d, s, r, c, hold)
        w = 1.0 / (t + 0.2)
        lin = np.average((m - e) ** 2, weights=w)
        log = np.average((np.log10(m + 1e-3) - np.log10(e + 1e-3)) ** 2, weights=w)
        return lin + log_weight * log

    best = None
    for c0 in (-1.0, -0.5, 0.0, 0.5):
        res = minimize(loss, [math.log(a0), math.log(d0), s0, math.log(r0), c0], method="Nelder-Mead",
                       options=dict(maxiter=3000, xatol=1e-4, fatol=1e-7))
        if best is None or res.fun < best.fun:
            best = res
    x = best.x
    x = [min(3.0, max(-8.0, x[0])), min(3.0, max(-8.0, x[1])), x[2], min(3.0, max(-8.0, x[3])), x[4]]
    env = dict(attack=min(5.0, max(0.001, math.exp(x[0]))), decay=min(5.0, max(0.005, math.exp(x[1]))),
               sustain=float(min(1.0, max(0.0, x[2]))), release=min(10.0, max(0.005, math.exp(x[3]))),
               curve=float(max(-1.0, min(1.0, x[4]))))

    # The release on its own (the joint fit favours the long held part):
    # match the fall in dB after the key goes up, with the fitted curve.
    rel = t >= hold
    if rel.sum() > 4:
        def release_loss(lr):
            m = adsr(t, env["attack"], env["decay"], env["sustain"], math.exp(min(3.0, max(-8.0, lr[0]))), env["curve"], hold)
            return np.mean((np.log10(m[rel] + 1e-4) - np.log10(e[rel] + 1e-4)) ** 2)
        res = minimize(release_loss, [math.log(env["release"])], method="Nelder-Mead")
        env["release"] = min(10.0, max(0.005, math.exp(min(3.0, max(-8.0, res.x[0])))))

    model = adsr(t, env["attack"], env["decay"], env["sustain"], env["release"], env["curve"], hold)
    checks = " ".join(f"{tt:.1f}s {20 * math.log10(max(1e-5, np.interp(tt, t, e))):.0f}/{20 * math.log10(max(1e-5, np.interp(tt, t, model))):.0f}"
                      for tt in (0.1, 0.3, 1.0, hold - 0.05, hold + 0.2, hold + 0.6))
    return peak, env, f"{best.fun:.4f} dB ref/ours: {checks}"


def fm_id(source, target):
    legacy = {(2, 1): "fm_amount", (1, 1): "fm_feedback", (1, 2): "fm_1to2", (1, 3): "fm_1to3", (2, 2): "fm_fb2",
              (2, 3): "fm_2to3", (3, 1): "fm_3to1", (3, 2): "fm_3to2", (3, 3): "fm_fb3"}
    if (source, target) in legacy:
        return legacy[(source, target)]
    return f"fm_fb{source}" if source == target else f"fm_{source}to{target}"


def fmt(value):
    return f"{value:.4g}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bank")
    ap.add_argument("voice", type=int)
    ap.add_argument("preset")
    ap.add_argument("--hold", type=float, default=1.5)
    ap.add_argument("--total", type=float, default=3.5)
    ap.add_argument("--velocity", type=int, default=100)
    ap.add_argument("--note", type=int, default=60)
    args = ap.parse_args()

    bank = Path(args.bank).read_bytes()
    v = unpack(bank, args.voice)
    tag = "".join(ch if ch.isalnum() else "_" for ch in v["name"].strip()).lower()
    mods, carriers, feedback = routing(v["alg"])

    t, gains = render(args.bank, args.voice, args.note, args.velocity, args.hold, args.total, tag, v["transpose"] - 24)
    _, low = render(args.bank, args.voice, args.note - 12, args.velocity, args.hold, args.total, tag, v["transpose"] - 24)
    _, high = render(args.bank, args.voice, args.note + 12, args.velocity, args.hold, args.total, tag, v["transpose"] - 24)

    sets = {}
    # A clean slate: no recipe routings, every FM cell off, plain outputs.
    for slot in range(1, 65):
        sets[f"mod{slot}_src"] = 0
    for s in range(1, 7):
        for d in range(1, 7):
            sets[fm_id(s, d)] = 0
    sets.update({"fm_mode": 0, "hard_sync": 0, "ring_mod": 0, "subosc_on": 0, "noise_level": 0, "amp_velocity": 0.3,
                 "f1_type": 0, "f1_cutoff": 20000, "f1_reso": 0, "f1_env": 0, "f1_drive": 1, "f2_cutoff": 20000,
                 "f2_drive": 1, "res_on": 0,
                 "voice_mode": 0, "glide": 0, "drift": 0})

    peaks, report = {}, []
    held = t < args.hold
    for op in range(1, 7):
        g = gains[:, op - 1]
        peak = float(g[held].max())
        peaks[op] = peak
        p = PREFIXES[op - 1]
        o = v["ops"][op - 1]
        used = peak > 0.002 and (op in carriers or any(src == op for src, _ in mods))
        sets[f"{p}_on"] = 1 if used else 0
        if not used:
            continue
        env_number = 5 + op  # ENV 6..11
        # A modulator's depth is heard in dB (the partials it makes), so its
        # fit weighs the level in dB more than a carrier's does.
        _, env, err = fit_envelope(t, g, args.hold, 0.15 if op in carriers else 1.5)
        for key, value in env.items():
            sets[f"env{env_number}_{key}"] = round(value, 4)
        sets[f"env{env_number}_velocity"] = 0
        sets[f"env{env_number}_delay"] = 0
        sets.update({f"{p}_mode": 0, f"{p}_table": SINE_TABLE, f"{p}_frame": 0, f"{p}_unison": 1, f"{p}_pan": 0,
                     f"{p}_warp": 0, f"{p}_amp_env": env_number - 1, f"{p}_semi": 0, f"{p}_ratio_snap": 0,
                     f"{p}_fb_type": 0, f"{p}_warp2": 0, f"{p}_pd_env": 0, f"{p}_spectral": 0, f"{p}_chord": 0})
        if o["mode"] == 0:
            ratio = (0.5 if o["coarse"] == 0 else o["coarse"]) * (1 + o["fine"] / 100)
            sets[f"{p}_tune"] = 1
            sets[f"{p}_ratio"] = round(ratio, 5)
        else:
            sets[f"{p}_tune"] = 2
            sets[f"{p}_fixed_hz"] = round(10 ** ((o["coarse"] & 3) + o["fine"] / 100), 3)
        sets[f"{p}_fine"] = o["det"] - 7  # about a cent a step in the middle
        # Key level scaling from the peaks an octave either side.
        db_low = 20 * math.log10(max(1e-6, float(low[:, op - 1][held].max())) / peak)
        db_high = 20 * math.log10(max(1e-6, float(high[:, op - 1][held].max())) / peak)
        sets[f"{p}_key_level"] = round(max(-1, min(1, (db_high - db_low) / 2 / 6)), 3)
        env_text = " ".join(f"{k} {val:.3g}" for k, val in env.items())
        report.append(f"op{op} {'carrier' if op in carriers else 'mod'} peak {peak:.3f} env {env_text} fit {err}")

    # Modulator depth: msfa's gain is the phase deviation in cycles, as
    # ilanaSynth's level x envelope x matrix amount is.
    # Carriers keep their balance; together they stay well under the voice's
    # drive stage (it saturates), and the preset's level trim sets loudness.
    carrier_peak = sum(peaks[c] for c in carriers if sets[f"{PREFIXES[c - 1]}_on"]) / 0.5 * 0.8
    for op in range(1, 7):
        p = PREFIXES[op - 1]
        if not sets[f"{p}_on"]:
            continue
        if op in carriers:
            sets[f"{p}_out"] = 1
            sets[f"{p}_level"] = round(0.8 * peaks[op] / carrier_peak, 4)
        else:
            sets[f"{p}_out"] = 0
            sets[f"{p}_level"] = round(min(1.0, peaks[op]), 4)
    for source, target in sorted(mods):
        if sets[f"{PREFIXES[source - 1]}_on"]:
            depth = peaks[source] / min(1.0, peaks[source]) if peaks[source] > 0 else 0
            sets[fm_id(source, target)] = round(min(1.0, depth), 4)
    if feedback and v["fb"] and sets[f"{PREFIXES[feedback - 1]}_on"]:
        # msfa: deviation = gain x 2^-(8 - fb); ilanaSynth: amount x level x env.
        level = sets[f"{PREFIXES[feedback - 1]}_level"]
        sets[fm_id(feedback, feedback)] = round(min(1.0, peaks[feedback] * 2.0 ** -(8 - v["fb"]) / max(1e-6, level)), 4)

    if v["transpose"] != 24:
        for op in range(1, 7):
            sets[f"{PREFIXES[op - 1]}_semi"] = v["transpose"] - 24

    print(f"# {v['name'].strip()} (alg {v['alg']}, fb {v['fb']}, lfo {v['lfo']}) carriers {sorted(carriers)} mods {sorted(mods)}",
          file=sys.stderr)
    for line in report:
        print("#   " + line, file=sys.stderr)
    # Macros (the recipe's routings were cleared above): BRIGHT raises every
    # modulator's depth, TONE closes the filter, DRIFT detunes like an old
    # synth, SPACE opens the reverb.
    bright = ", ".join(f"Osc{op} Level {fmt(min(1.0, 0.6 * sets[PREFIXES[op - 1] + '_level'] + 0.03))}"
                       for op in range(1, 7) if sets[f"{PREFIXES[op - 1]}_on"] and op not in carriers)
    macros = [f"m1=BRIGHT: {bright}" if bright else "m1=BRIGHT: FM Amount 0.3",
              "m2=TONE: Filter1 Cutoff -0.55", "m3=DRIFT: Drift 0.6", "m4=SPACE: Reverb Mix 0.35"]
    print(f"{args.preset} | " + " ".join(f"{k}={fmt(val) if isinstance(val, float) else val}" for k, val in sets.items())
          + " | " + "; ".join(macros))
    return 0


if __name__ == "__main__":
    sys.exit(main())
