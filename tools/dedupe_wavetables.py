#!/usr/bin/env python3
"""Finds near-duplicate wavetables in the library and optionally deletes them.

    python tools/dedupe_wavetables.py "content/wavetables/Surge Packs" \
        [--factory factory-tables.bin] [--report DEDUPE.md] [--apply]

"Basically the same" is judged on the sound, not the file: for each table the
spectrum of 11 frames spread evenly along it (so how the table moves counts)
is reduced to the first 12 harmonics plus 9 bands above them, in dB, ignoring
level and phase. Two tables are duplicates when the mean dB difference is
under SAME_FOLDER (same folder, i.e. numbered variants of one idea) or under
CROSS_FOLDER (anywhere else), and under FACTORY against a built-in factory table. Quality over quantity: when several tables are
alike, the one that moves the most (most frames, up to 64) is kept and the
rest go; ties are broken by name so the result is deterministic.

--factory: a dump of the built-in factory tables (from `TableFactory::generate`:
lines "T index frames size|name|category" followed by float32 frames); they
always win, so a library table that only repeats one of them is dropped.
Needs numpy. Reads 16-bit and 32-bit float mono .wav files with a 'clm ' chunk
(tools/import_surge_wavetables.py writes them).
"""

import argparse
import struct
import sys
from pathlib import Path

import numpy as np

SAME_FOLDER = 8.0
CROSS_FOLDER = 4.5
FACTORY = 3.0
POSITIONS = 11
BAND_EDGES = [13, 16, 20, 25, 32, 40, 50, 64, 90, 128]


def read_wav(path):
    d = path.read_bytes()
    i, bits, data, frame = 12, 0, None, 0
    while i + 8 <= len(d):
        cid, sz = d[i:i + 4], struct.unpack("<I", d[i + 4:i + 8])[0]
        body = d[i + 8:i + 8 + sz]
        if cid == b"fmt ":
            bits = struct.unpack("<H", body[14:16])[0]
        elif cid == b"data":
            data = body
        elif cid == b"clm ":
            digits = ""
            for c in body.decode("latin1")[3:]:
                if not c.isdigit():
                    break
                digits += c
            frame = int(digits or 0)
        i += 8 + sz + (sz & 1)
    x = (np.frombuffer(data, dtype=np.int16).astype(np.float32) / 32768 if bits == 16
         else np.frombuffer(data, dtype=np.float32))
    return x.reshape(-1, frame)


def read_factory(path):
    out, raw, i = [], path.read_bytes(), 0
    while i < len(raw):
        end = raw.index(b"\n", i)
        head, name, category = raw[i + 2:end].decode().split("|")
        _, frames, size = (int(v) for v in head.split())
        i = end + 1
        out.append(("factory/" + name, np.frombuffer(raw[i:i + frames * size * 4], dtype=np.float32).reshape(frames, size)))
        i += frames * size * 4
    return out


def describe(frames):
    n = len(frames)
    rows = []
    for p in np.round(np.linspace(0, n - 1, POSITIONS)).astype(int):
        s = np.abs(np.fft.rfft(frames[p].astype(np.float64)))
        s = np.pad(s, (0, max(0, 130 - len(s))))
        e = np.concatenate([s[1:13], [np.sqrt((s[a:b] ** 2).mean()) for a, b in zip(BAND_EDGES[:-1], BAND_EDGES[1:])]])
        e = e / (np.linalg.norm(e) + 1e-12)
        rows.append(20 * np.log10(e + 1e-3))
    return np.array(rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("library", type=Path)
    ap.add_argument("--factory", type=Path)
    ap.add_argument("--report", type=Path)
    ap.add_argument("--same", type=float, default=SAME_FOLDER)
    ap.add_argument("--cross", type=float, default=CROSS_FOLDER)
    ap.add_argument("--factory-limit", type=float, default=FACTORY)
    ap.add_argument("--apply", action="store_true", help="delete the duplicates' files")
    args = ap.parse_args()

    entries = []  # (name, folder, frame count, descriptor, is_factory)
    if args.factory:
        for name, frames in read_factory(args.factory):
            entries.append((name, "factory", len(frames), describe(frames), True))
    files = sorted(args.library.rglob("*.wav"))
    for f in files:
        rel = f.relative_to(args.library).as_posix()
        frames = read_wav(f)
        entries.append((rel, str(Path(rel).parent), len(frames), describe(frames), False))

    order = sorted(range(len(entries)), key=lambda i: (not entries[i][4], -min(entries[i][2], 64), entries[i][0]))
    kept, dropped = [], []  # dropped: (index, kept index it repeats, distance)
    for i in order:
        match = None
        for j in kept:
            limit = (args.factory_limit if entries[j][4] else args.same if entries[i][1] == entries[j][1] else args.cross)
            d = float(np.abs(entries[i][3] - entries[j][3]).mean())
            if d < limit and (match is None or d < match[1]):
                match = (j, d)
        if match is None or entries[i][4]:
            kept.append(i)
        else:
            dropped.append((i, match[0], match[1]))

    library_kept = [i for i in kept if not entries[i][4]]
    print("library tables: %d, kept %d, duplicates %d (%d of them repeat a factory table)"
          % (len(files), len(library_kept), len(dropped), sum(1 for _, j, _ in dropped if entries[j][4])))

    if args.report:
        groups = {}
        for i, j, d in dropped:
            groups.setdefault(j, []).append((i, d))
        lines = ["# Duplicate wavetables", "",
                 "Run `tools/dedupe_wavetables.py` (see its docstring for the rule). %d of %d library tables kept, %d removed."
                 % (len(library_kept), len(files), len(dropped)), ""]
        for j, members in sorted(groups.items(), key=lambda kv: (-len(kv[1]), entries[kv[0]][0])):
            lines.append("- **kept** %s (%d frames)" % (entries[j][0], entries[j][2]))
            for i, d in sorted(members, key=lambda m: entries[m[0]][0]):
                lines.append("  - removed %s (%d frames, %.1f dB apart)" % (entries[i][0], entries[i][2], d))
        args.report.write_text("\n".join(lines) + "\n", encoding="utf-8")

    if args.apply:
        for i, _, _ in dropped:
            (args.library / entries[i][0]).unlink()
        for folder in sorted({p.parent for p in args.library.rglob("*")}, key=lambda p: -len(p.parts)):
            if folder.is_dir() and not any(folder.iterdir()):
                folder.rmdir()


if __name__ == "__main__":
    sys.exit(main())
