#!/usr/bin/env python3
"""Builds the before / after listening clips for the string-knob audit.

Each clip plays one knob at 0%, 50% and 100% (2.4 s each) on one exciter and
note. Needs two ilanaKnobAudit sweeps (tools/knob_audit.py render): one from
the old build and one from the new.

  tools/knob_ab_clips.py <before dir> <after dir> <out dir>
"""
import os, sys, wave
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
import knob_audit as ka

CLIPS = [
    # (file name, exciter, note, knob), several rows are joined one after another
    ('01-stiff-burst-C4', [('burst', 'C4', 'stiff')]),
    ('02-register-burst-C2-then-C6', [('burst', 'C2', 'register'), ('burst', 'C6', 'register')]),
    ('03-couple-piano-C4-3-strings', [('piano', 'C4', 'couple3')]),
    ('04-buzz-burst-C4', [('burst', 'C4', 'buzz')]),
    ('05-buzz-bow-C4', [('bow', 'C4', 'buzz')]),
    ('06-rattle-hammer-C4', [('hammer', 'C4', 'rattle')]),
    ('07-slap-hammer-C4-off-then-on', [('hammer', 'C4', 'slap')]),
    ('08-position-burst-C4', [('burst', 'C4', 'position')]),
    ('09-pick-pos-burst-C4', [('burst', 'C4', 'pickpos')]),
    ('10-pickup-burst-C4', [('burst', 'C4', 'pickup')]),
    ('11-hardness-burst-C4', [('burst', 'C4', 'hardness')]),
    ('12-hammer-piano-C4', [('piano', 'C4', 'hammer')]),
    ('13-hammer-reed-C4', [('reed', 'C4', 'hammer')]),
    ('14-damp-reed-C4', [('reed', 'C4', 'damp')]),
    ('15-sustain-noise-C4', [('noise', 'C4', 'sustain')]),
]
NAMES = {v: k for k, v in ka.EXCITES.items()}
NOTE = {v: k for k, v in ka.NOTES.items()}

def clip(folder, ex, note, knob):
    vals = [0.0, 1.0] if knob == 'slap' else [0.0, 0.5, 1.0]
    out = []
    for v in vals:
        x = ka.read(f'{folder}/{ka.name(NAMES[ex], NOTE[note], knob, v)}.wav')[:int(2.4 * ka.SR)]
        fade = int(0.05 * ka.SR)
        x = x.copy(); x[-fade:] *= np.linspace(1, 0, fade)
        out.append(np.pad(x, (0, int(2.4 * ka.SR) - len(x))))
    return np.concatenate(out)

def write(path, x):
    peak = max(1e-9, np.abs(x).max())
    x = x * (0.89 / peak) if peak > 0.89 else x
    w = wave.open(path, 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(ka.SR)
    w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes()); w.close()

if __name__ == '__main__':
    before, after, out = sys.argv[1:4]
    for side, folder in (('before', before), ('after', after)):
        os.makedirs(f'{out}/{side}', exist_ok=True)
        for name, rows in CLIPS:
            write(f'{out}/{side}/{name}.wav', np.concatenate([clip(folder, *r) for r in rows]))
