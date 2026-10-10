#!/usr/bin/env python3
"""Audits the Physical oscillator's string and exciter knobs.

Renders each knob at 0, .25, .5, .75 and 1 for a few exciters and notes with
ilanaKnobAudit, then measures how far each render sits from the knob's
minimum: spectral distance (RMS of 1/3-octave band levels in dB, three time
segments: attack, sustain, release tail) and level change. A repeat of the
baseline gives the noise floor (the burst is random). Usage:

  tools/knob_audit.py render <out dir>   renders all sweeps
  tools/knob_audit.py report <out dir>   prints the table (csv in <out>/audit.csv)
"""
import itertools, subprocess, sys, os, wave, csv
import numpy as np

TOOL = os.environ.get('KNOB_TOOL', 'build/ilanaKnobAudit_artefacts/Release/ilanaKnobAudit')
SR = 48000
EXCITES = {0: 'burst', 1: 'noise', 4: 'bow', 5: 'hammer', 7: 'tine', 8: 'reed', 9: 'piano', 10: 'feedback'}
NOTES = {36: 'C2', 60: 'C4', 84: 'C6'}
VALUES = [0.0, 0.25, 0.5, 0.75, 1.0]
HOLD, TOTAL = 1.5, 3.5
# knob -> (parameter suffix, extra settings)
KNOBS = {
    'decay': ('string_decay', ''), 'damp': ('string_damp', ''), 'sustain': ('string_sustain', ''),
    'stiff': ('string_stiffness', ''), 'register': ('register', ''), 'damper': ('damper', ''),
    'couple1': ('couple', ''), 'couple3': ('couple', 'osc1_unison=3'),
    'buzz': ('bridge_buzz', ''), 'rattle': ('fret_rattle', ''),
    'position': ('string_excite_pos', ''), 'hardness': ('string_pick_hardness', ''),
    'pickpos': ('string_pick_pos', ''), 'pickup': ('string_pickup', ''), 'slap': ('string_slap', ''),
    'hammer': ('hammer_hard', ''), 'bowpress': ('bow_pressure', ''), 'bowspeed': ('bow_speed', ''),
    'epdist': ('ep_distance', ''), 'epoffset': ('ep_position', ''),
    'fbgain': ('fb_gain', 'osc1_string_sustain=0.6'), 'fbdist': ('fb_distance', 'osc1_string_sustain=0.6'),
    'damperL': ('damper', 'amp_release=5'), 'registerHi': ('register', 'osc1_string_stiffness=0.3'),
}
BASE = {'osc1_unison': 1}

def name(ex, note, knob, v, rep=0):
    return f'{EXCITES[ex]}_{NOTES[note]}_{knob}_{int(v*100):03d}' + (f'_r{rep}' if rep else '')

def jobs(excites, notes, knobs):
    for ex, note, knob in itertools.product(excites, notes, knobs):
        suffix, extra = KNOBS[knob]
        vals = [0.0, 1.0] if knob == 'slap' else VALUES
        for rep, v in [(0, x) for x in vals] + [(1, vals[0])]:
            s = f'osc1_excite={ex};{extra};osc1_{suffix}={v}'
            yield f'{name(ex, note, knob, v, rep)}|{note}|100|{HOLD}|{TOTAL}|{s}'

def render(out):
    os.makedirs(out, exist_ok=True)
    only = os.environ.get('KNOBSET')
    ks = [k for k in KNOBS if not only or k in only.split(',')]
    ex = [e for e in EXCITES if not os.environ.get('EXSET') or EXCITES[e] in os.environ['EXSET'].split(',')]
    allj = list(jobs(ex, NOTES, ks))
    n = 4
    procs = []
    for i in range(n):
        p = os.path.join(out, f'jobs{i}.txt')
        open(p, 'w').write('\n'.join(allj[i::n]) + '\n')
        procs.append(subprocess.Popen([TOOL, p, out]))
    for p in procs:
        p.wait()

def read(path):
    w = wave.open(path)
    raw = w.readframes(w.getnframes())
    b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
    x = (b[:, 0].astype(np.int32) | b[:, 1].astype(np.int32) << 8 | b[:, 2].astype(np.int32) << 16)
    x = np.where(x >= 1 << 23, x - (1 << 24), x)
    return x / float(1 << 23)

SEGS = [(0.0, 0.15), (0.15, HOLD), (HOLD, TOTAL)]
EDGES = 1000 * 2.0 ** (np.arange(-10, 14) / 3.0 * 1.0)  # 1/3 octave from ~100 Hz
EDGES = 100 * 2.0 ** (np.arange(0, 8) / 1.0)

def bands(x):
    out = []
    for a, b in SEGS:
        seg = x[int(a * SR):int(b * SR)]
        if len(seg) < 256:
            out.append(np.full(len(EDGES) - 1, -120.0)); continue
        win = np.hanning(len(seg))
        spec = np.abs(np.fft.rfft(seg * win)) ** 2 / len(seg)
        f = np.fft.rfftfreq(len(seg), 1 / SR)
        lv = []
        for lo, hi in zip(EDGES[:-1], EDGES[1:]):
            m = (f >= lo) & (f < hi)
            lv.append(10 * np.log10(spec[m].sum() + 1e-12) if m.any() else -120.0)
        out.append(np.array(lv))
    return out

FRAME = 0.1
def envelope(x):
    n = int(FRAME * SR)
    return np.array([10 * np.log10(np.mean(x[i:i + n] ** 2) + 1e-12) for i in range(0, len(x) - n, n)])

def stretch(x, note):
    """Mean deviation in cents of partials 2..6 from exact harmonics (0.1-1.2 s)."""
    f0 = 440.0 * 2 ** ((note - 69) / 12)
    seg = x[int(0.1 * SR):int(1.2 * SR)]
    N = 1 << 18
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), N))
    f = np.fft.rfftfreq(N, 1 / SR)
    dev = []
    for n in range(2, 7):
        c = n * f0
        if c * 1.05 > SR * 0.4:
            break
        m = (f > c * 0.97) & (f < c * 1.06)
        if spec[m].max() < spec.max() * 0.003:
            continue
        fp = f[m][np.argmax(spec[m])]
        dev.append(1200 * np.log2(fp / c))
    return float(np.mean(dev)) if dev else 0.0

def harmonics(x, note):
    """Levels (dB) of partials 1..16 near n * f0, 0.05-0.8 s."""
    f0 = 440.0 * 2 ** ((note - 69) / 12)
    seg = x[int(0.05 * SR):int(0.8 * SR)]
    N = 1 << 17
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), N))
    f = np.fft.rfftfreq(N, 1 / SR)
    out = []
    for n in range(1, 17):
        c = n * f0
        if c * 1.1 > SR * 0.45:
            out.append(-120.0); continue
        m = (f > c * 0.96) & (f < c * 1.08)
        out.append(20 * np.log10(spec[m].max() + 1e-9))
    return np.array(out)

def harm_dist(a, b):
    top = max(a.max(), b.max())
    m = (a > top - 60) | (b > top - 60)
    return float(np.sqrt(np.mean((a[m] - b[m]) ** 2))) if m.any() else 0.0

def dist(a, b):
    """(spectral rms dB per segment, level change dB per segment)"""
    sp, lv = [], []
    for sa, sb in zip(a, b):
        mask = (sa > -80) | (sb > -80)
        sp.append(float(np.sqrt(np.mean((sa[mask] - sb[mask]) ** 2))) if mask.any() else 0.0)
        la = 10 * np.log10(np.sum(10 ** (sa / 10)) + 1e-12)
        lb = 10 * np.log10(np.sum(10 ** (sb / 10)) + 1e-12)
        lv.append(float(lb - la))
    return sp, lv

def report(out):
    rows = []
    for ex, note, knob in itertools.product(EXCITES, NOTES, KNOBS):
        vals = [0.0, 1.0] if knob == 'slap' else VALUES
        if not os.path.exists(f'{out}/{name(ex, note, knob, vals[0])}.wav'):
            continue
        try:
            ref = bands(read(f'{out}/{name(ex, note, knob, vals[0])}.wav'))
            rep = bands(read(f'{out}/{name(ex, note, knob, vals[0], 1)}.wav'))
            top = bands(read(f'{out}/{name(ex, note, knob, vals[-1])}.wav'))
        except Exception as e:
            continue
        nsp, nlv = dist(ref, rep)
        sp, lv = dist(ref, top)
        silent = max(float(np.max(s)) for s in ref + top) < -70
        xr = read(f'{out}/{name(ex, note, knob, vals[0])}.wav'); xt = read(f'{out}/{name(ex, note, knob, vals[-1])}.wav')
        xp = read(f'{out}/{name(ex, note, knob, vals[0], 1)}.wav')
        er, et, ep = envelope(xr), envelope(xt), envelope(xp)
        live = (er > -75) | (et > -75)
        env = float(np.max(np.abs(er - et)[live])) if live.any() else 0.0
        envn = float(np.max(np.abs(er - ep)[live])) if live.any() else 0.0
        st = abs(stretch(xt, note) - stretch(xr, note))
        hm = harm_dist(harmonics(xr, note), harmonics(xt, note)) - harm_dist(harmonics(xr, note), harmonics(xp, note))
        # worst segment, minus the noise floor
        effect = max(a - b for a, b in zip(sp, nsp))
        lvl = max(abs(x) - abs(y) for x, y in zip(lv, nlv))
        rows.append((EXCITES[ex], NOTES[note], knob, round(max(sp), 1), round(max(nsp), 1),
                     round(effect, 1), round(max(abs(x) for x in lv), 1), round(lvl, 1), silent,
                     round(env - envn, 1), round(st, 1), round(hm, 1)))
    with open(f'{out}/audit.csv', 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['exciter', 'note', 'knob', 'spec_db', 'noise_db', 'effect_db', 'level_db', 'level_eff_db', 'silent', 'env_db', 'stretch_cents', 'harm_db'])
        w.writerows(rows)
    return rows

def table(out):
    rows = list(csv.DictReader(open(f'{out}/audit.csv')))
    exs = [e for e in EXCITES.values() if any(r['exciter'] == e for r in rows)]
    print('score = max(spectral, envelope, harmonic-pattern dB, stretch cents / 8), at C2/C4/C6; <3 dead, 3-6 weak, >6 clear')
    print('%-10s' % 'knob' + ''.join('%-13s' % e for e in exs))
    for k in [k for k in KNOBS if any(r['knob'] == k for r in rows)]:
        line = '%-10s' % k
        for e in exs:
            v = [r for r in rows if r['knob'] == k and r['exciter'] == e]
            sc = [max(float(r['effect_db']), float(r['env_db']), float(r['harm_db']), float(r['stretch_cents']) / 8) for r in v]
            line += '%-13s' % ('/'.join('%.0f' % x for x in sc) if sc else '-')
        print(line)

if __name__ == '__main__':
    cmd, out = sys.argv[1], sys.argv[2]
    if cmd == 'render':
        render(out)
    elif cmd == 'table':
        report(out); table(out)
    else:
        for r in report(out):
            print(*r)
