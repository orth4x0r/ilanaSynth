#!/usr/bin/env python3
"""Packs the library tables into the file the synth carries inside itself.

    python tools/build_builtin_wavetables.py "content/wavetables/Surge Packs" content/wavetables/builtin-wavetables.bin

CMake embeds the output (juce_add_binary_data, src/dsp/BuiltInTables.cpp reads
it). Run it again whenever the folder changes (after tools/dedupe_wavetables.py);
the output is deterministic. Needs numpy.

File layout, little endian:
    "ILBT", u32 version (1), u32 count
    per table (sorted by path, case-insensitive):
        u16 path length, path (UTF-8, '/' separated, no extension),
        u16 frame size, u16 frames, u32 offset (from the file start), u32 size
    the tables' data: zlib of the 16-bit samples, each frame stored as the
    difference to the sample before it (the first sample as is).
"""

import struct
import sys
import zlib
from pathlib import Path

import numpy as np


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
    if bits == 16:
        return np.frombuffer(data, dtype="<i2").astype(np.int16), frame
    x = np.frombuffer(data, dtype="<f4")
    return np.clip(np.round(x * 32767.0), -32768, 32767).astype(np.int16), frame


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    root, out = Path(sys.argv[1]), Path(sys.argv[2])
    files = sorted(root.rglob("*.wav"), key=lambda p: p.relative_to(root).as_posix().lower())
    tables = []
    for f in files:
        samples, frame = read_wav(f)
        assert frame > 0 and len(samples) % frame == 0, f
        frames = len(samples) // frame
        assert 1 <= frames <= 256 and frame <= 65535, f
        delta = np.diff(samples.astype(np.int32), prepend=0).astype("<i2")
        tables.append((f.relative_to(root).with_suffix("").as_posix(), frame, frames, zlib.compress(delta.tobytes(), 9)))

    head = 12 + sum(2 + len(p.encode()) + 12 for p, *_ in tables)
    index, blobs, offset = b"", b"", head
    for path, frame, frames, blob in tables:
        raw = path.encode()
        index += struct.pack("<H", len(raw)) + raw + struct.pack("<HHII", frame, frames, offset, len(blob))
        blobs += blob
        offset += len(blob)
    out.write_bytes(b"ILBT" + struct.pack("<II", 1, len(tables)) + index + blobs)
    print("%d tables, %.1f MB" % (len(tables), (head + len(blobs)) / 1e6))


if __name__ == "__main__":
    main()
