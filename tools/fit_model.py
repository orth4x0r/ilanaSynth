"""Shared reference measurements for physical models.

usage:
  python tools/fit_model.py measure recording.aif [recording...]
  python tools/fit_model.py compare reference.aif render.wav
  python tools/fit_model.py check-body [reference-dir] [render-dir]

The piano fitter imports load() from here. BODY uses modal peak frequency,
relative onset level, and each mode's decay in dB/s. MP3 references need
python-soundfile; WAV and AIFF use the repository's existing reader.
"""

import json
import sys
from pathlib import Path

import numpy as np

from analyse_note import load as _load


def load(path):
    if str(path).lower().endswith(('.mp3', '.ogg')):
        import soundfile as sf
        x, rate = sf.read(str(path), dtype='float64')
        if x.ndim == 2:
            x = x.mean(axis=1)
        peak = np.max(np.abs(x))
        start = np.argmax(np.abs(x) > peak * 0.05)
        return x[max(0, start - int(rate * 0.002)):], rate
    return _load(str(path))


def modes(path, count=8):
    x, rate = load(path)
    size = min(32768, 1 << int(np.floor(np.log2(max(2048, len(x) // 2)))))
    start = int(0.02 * rate)
    segment = np.zeros(size)
    chunk = x[start:start + size]
    segment[:len(chunk)] = chunk
    spectrum = np.abs(np.fft.rfft(segment * np.hanning(size)))
    frequency = np.fft.rfftfreq(size, 1 / rate)
    limit = min(len(spectrum) - 2, int(12000 * size / rate))
    candidates = [i for i in range(max(2, int(80 * size / rate)), limit)
                  if spectrum[i] > spectrum[i - 1] and spectrum[i] >= spectrum[i + 1]
                  and spectrum[i] > spectrum.max() * 0.015]
    candidates.sort(key=lambda i: spectrum[i], reverse=True)
    selected = []
    for index in candidates:
        if all(abs(frequency[index] - frequency[j]) > max(18, frequency[j] * 0.018) for j in selected):
            selected.append(index)
        if len(selected) == count:
            break
    selected.sort(key=lambda i: frequency[i])
    if not selected:
        return []
    anchor = max(selected, key=lambda i: spectrum[i])
    top = spectrum[anchor]
    result = []
    for index in selected:
        hz = float(frequency[index])
        times, levels = [], []
        width = min(8192, size)
        for t in (0.03, 0.12, 0.3, 0.6, 1.0, 1.5):
            begin = int(t * rate)
            if begin + width > len(x):
                continue
            frame = x[begin:begin + width] * np.hanning(width)
            bins = np.fft.rfftfreq(width, 1 / rate)
            values = np.abs(np.fft.rfft(frame))
            band = np.abs(bins - hz) < max(12, hz * 0.012)
            if np.any(band):
                times.append(t)
                levels.append(20 * np.log10(max(1e-10, np.max(values[band]))))
        slope = float(np.polyfit(times, levels, 1)[0]) if len(times) >= 3 else 0.0
        result.append({'hz': round(hz, 2), 'ratio': round(hz / frequency[anchor], 3),
                       'onset_db': round(float(20 * np.log10(spectrum[index] / top)), 1),
                       'decay_db_s': round(slope, 1)})
    return result


def compare(reference, rendered):
    ref, ours = modes(reference), modes(rendered)
    if not ref or not ours:
        return {'error': 'no modes found'}
    pairs = []
    for mode in ref:
        match = min(ours, key=lambda candidate: abs(np.log(candidate['ratio'] / mode['ratio'])))
        pairs.append({'ref_ratio': mode['ratio'], 'our_ratio': match['ratio'],
                      'ratio_cents': round(float(1200 * np.log2(match['ratio'] / mode['ratio'])), 1),
                      'decay_db_s': round(match['decay_db_s'] - mode['decay_db_s'], 1)})
    return {'mean_ratio_cents': round(float(np.mean([abs(p['ratio_cents']) for p in pairs])), 1),
            'mean_decay_db_s': round(float(np.mean([abs(p['decay_db_s']) for p in pairs])), 1),
            'pairs': pairs}


def main(args):
    if not args or args[0] not in ('measure', 'compare', 'check-body'):
        print(__doc__)
        return 2
    if args[0] == 'measure':
        for path in args[1:]:
            print(json.dumps({'file': str(path), 'modes': modes(path)}, indent=2))
    elif args[0] == 'compare' and len(args) == 3:
        print(json.dumps(compare(args[1], args[2]), indent=2))
    elif args[0] == 'check-body' and len(args) <= 3:
        references = Path(args[1] if len(args) > 1 else 'build/reference/body')
        renders = Path(args[2] if len(args) > 2 else 'build/fit/body/modal')
        # Iowa marimba, Thai gong and brass bell; Freesound struck wine glass.
        sources = {'bar': 'bar_C4.aif', 'plate': 'plate_C4.aif',
                   'bell': 'bell_C5.aif', 'glass': 'wineglass_G4.mp3'}
        passed = True
        for name, filename in sources.items():
            score = compare(references / filename, renders / (name + '.wav'))
            good = (score.get('mean_ratio_cents', 1e9) <= 20.0
                    and score.get('mean_decay_db_s', 1e9) <= 5.0)
            passed &= good
            print(f"{name:5s} {'PASS' if good else 'FAIL'}  "
                  f"ratio {score.get('mean_ratio_cents')} cents, "
                  f"decay {score.get('mean_decay_db_s')} dB/s")
        return 0 if passed else 1
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
