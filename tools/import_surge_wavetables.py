#!/usr/bin/env python3
"""Converts the third-party wavetable packs bundled with Surge XT into ilanaSynth library files.

    python tools/import_surge_wavetables.py SURGE_DIR OUT_DIR

SURGE_DIR is resources/data/wavetables_3rdparty from github.com/surge-synthesizer/surge.
OUT_DIR is content/wavetables/Surge Packs: one folder per pack, the pack's own
sub-folders kept, one mono .wav per table with a Serum-style 'clm ' chunk that
names the frame size, so the library loads every file as frames and never has
to guess. The input is read, never changed. Surge's .wav (with an 'srge' chunk)
and .wt ('vawt') files are read; a plain .wav is used when its length splits
into 2048, 1024, 512, 4096 or 256 sample frames, and skipped when it does not.
Samples (not wavetables) and tables over 256 frames are thinned or skipped.
Only the standard library is used; the output is deterministic.
"""

import array
import struct
import sys
from pathlib import Path

MAX_FRAMES = 256
GUESS_SIZES = (2048, 1024, 512, 4096, 256)


def read_wav(path):
    """-> (samples as float list or int16 array, kind 'i16'|'f32', frame size or 0)."""
    d = path.read_bytes()
    if d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        return None
    i, fmt, data, tagged = 12, None, None, 0
    while i + 8 <= len(d):
        cid, sz = d[i:i + 4], struct.unpack("<I", d[i + 4:i + 8])[0]
        body = d[i + 8:i + 8 + sz]
        if cid == b"fmt ":
            tag, ch, rate, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            if tag == 0xFFFE and len(body) >= 26:
                tag = struct.unpack("<H", body[24:26])[0]
            fmt = (tag, ch, bits)
        elif cid == b"data":
            data = body
        elif cid == b"srge" and len(body) >= 8:
            tagged = struct.unpack("<II", body[:8])[1]
        elif cid == b"clm " and not tagged:
            text = body.decode("latin1")
            if text.startswith("<!>"):
                digits = ""
                for c in text[3:]:
                    if not c.isdigit():
                        break
                    digits += c
                tagged = int(digits) if digits else 0
        i += 8 + sz + (sz & 1)
    if fmt is None or data is None:
        return None
    tag, ch, bits = fmt
    if tag == 1 and bits == 16:
        a = array.array("h")
        a.frombytes(data[:len(data) // 2 * 2])
        kind = "i16"
    elif tag == 3 and bits == 32:
        a = array.array("f")
        a.frombytes(data[:len(data) // 4 * 4])
        kind = "f32"
    else:
        return None
    if ch > 1:  # mix down
        n = len(a) // ch
        a = array.array(a.typecode, [type(a[0])(sum(a[k * ch + c] for c in range(ch)) / ch) if kind == "f32"
                                    else int(sum(a[k * ch + c] for c in range(ch)) / ch) for k in range(n)])
    return a, kind, tagged


def read_wt(path):
    d = path.read_bytes()
    if d[:4] != b"vawt" or len(d) < 12:
        return None
    size, count, flags = struct.unpack("<IHH", d[4:12])
    if flags & 1:  # a sample, not a wavetable
        return None
    if flags & 4:
        a = array.array("h")
        a.frombytes(d[12:12 + size * count * 2])
        kind = "i16"
        if not flags & 8:  # stored at half scale
            a = array.array("h", [max(-32768, min(32767, v * 2)) for v in a])
    else:
        a = array.array("f")
        a.frombytes(d[12:12 + size * count * 4])
        kind = "f32"
    if len(a) != size * count:
        return None
    return a, kind, size


def write_wav(path, samples, kind, frame):
    bits, tag = (16, 1) if kind == "i16" else (32, 3)
    data = samples.tobytes()
    clm = ("<!>%d 00000000 wavetable (Surge pack)" % frame).encode("ascii")
    clm += b"\0" * (len(clm) & 1)
    fmt = struct.pack("<HHIIHH", tag, 1, 44100, 44100 * bits // 8, bits // 8, bits)
    body = b"WAVE" + b"fmt " + struct.pack("<I", 16) + fmt
    body += b"clm " + struct.pack("<I", len(clm)) + clm
    body += b"data" + struct.pack("<I", len(data)) + data
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


def convert(src, dst):
    got = read_wt(src) if src.suffix.lower() == ".wt" else read_wav(src)
    if got is None:
        return "unreadable or a sample"
    samples, kind, frame = got
    if frame == 0:
        for g in GUESS_SIZES:
            if len(samples) % g == 0 and 1 <= len(samples) // g <= MAX_FRAMES:
                frame = g
                break
    if frame == 0 or len(samples) % frame != 0:
        return "no frame size"
    count = len(samples) // frame
    if count > MAX_FRAMES:  # thin evenly, first and last frames kept
        keep = [round(k * (count - 1) / (MAX_FRAMES - 1)) for k in range(MAX_FRAMES)]
        thinned = array.array(samples.typecode)
        for k in keep:
            thinned.extend(samples[k * frame:(k + 1) * frame])
        samples = thinned
    write_wav(dst, samples, kind, frame)
    return None


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    root, out = Path(sys.argv[1]), Path(sys.argv[2])
    done, skipped = 0, []
    for src in sorted(root.rglob("*")):
        if src.suffix.lower() not in (".wav", ".wt"):
            continue
        rel = src.relative_to(root).with_suffix(".wav")
        why = convert(src, out / rel)
        if why:
            skipped.append((str(src.relative_to(root)), why))
        else:
            done += 1
    print("converted %d, skipped %d" % (done, len(skipped)))
    for name, why in skipped:
        print("  skipped %s: %s" % (name, why))


if __name__ == "__main__":
    main()
