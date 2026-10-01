"""Builds an A/B listening page: for each preset, the old render (A) against
the new one (B), embedded as small WAVs.

    python tools/ab_page.py <old dir> <new dir> <out.html> <title> <wav name> [names...]

The dirs come from ilanaPresetRender (index.csv plus <index>/<wav name>).
Clips are downmixed to mono and resampled to 24 kHz to keep the page small.
"""

import base64
import csv
import html
import io
import sys
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly


def clip(path):
    x, rate = sf.read(path, always_2d=True)
    y = resample_poly(x.mean(axis=1), 1, rate // 24000) if rate % 24000 == 0 else x.mean(axis=1)
    out_rate = 24000 if rate % 24000 == 0 else rate
    peak = np.abs(y).max()
    if peak > 0.99:
        y = y * 0.99 / peak
    buf = io.BytesIO()
    sf.write(buf, y, out_rate, format="WAV", subtype="PCM_16")
    return "data:audio/wav;base64," + base64.b64encode(buf.getvalue()).decode()


def main():
    old_dir, new_dir, out, title, wav = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4], sys.argv[5]
    with open(old_dir / "index.csv", newline="", encoding="utf-8") as f:
        rows = {r["name"]: r for r in csv.DictReader(f)}
    names = sys.argv[6:] or list(rows)
    template = (Path(__file__).parent / "ab_page_template.html").read_text(encoding="utf-8")
    items = []
    for name in names:
        r = rows[name]
        a, b = old_dir / r["index"] / wav, new_dir / r["index"] / wav
        if not (a.exists() and b.exists()):
            continue
        items.append(f'<li class="item" data-name="{html.escape(name)}">'
                     f'<div class="head"><span class="name">{html.escape(name)}</span>'
                     f'<span class="cat">{html.escape(r["category"])}</span></div>'
                     f'<div class="pair"><div class="side"><span class="tag">A · before</span>'
                     f'<audio controls preload="none" src="{clip(a)}"></audio></div>'
                     f'<div class="side"><span class="tag">B · DX7 rebuild</span>'
                     f'<audio controls preload="none" src="{clip(b)}"></audio></div></div>'
                     f'<div class="vote" role="group" aria-label="Vote for {html.escape(name)}">'
                     f'<button type="button" data-v="A">A better</button><button type="button" data-v="B">B better</button>'
                     f'<button type="button" data-v="both bad">Both bad</button></div></li>')
    out.write_text(template.replace("{{TITLE}}", html.escape(title)).replace("{{ITEMS}}", "\n".join(items)), encoding="utf-8")
    print(f"{len(items)} presets, {out.stat().st_size / 1e6:.1f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
