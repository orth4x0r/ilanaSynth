#!/usr/bin/env python3
"""Builds ilanaSynth's sample and wavetable library from two CC0 sources.

    python tools/build_content.py [--only wavetables|samples] [--jobs N]

Sources (already downloaded, never modified):
    build/content/akwf   Adventure Kid Waveforms, github.com/KristofferKarlAxelEkstrand/AKWF-FREE
    build/content/vcsl   Versilian Community Sample Library, github.com/sgossner/VCSL

Outputs (cleared and rewritten on every run; only folders this script created,
which carry a ".build_content" marker, are ever deleted):
    Documents/ilanaSynth Wavetables/AKWF/<family>/*.wav   2048-sample frames, float32, 'clm ' chunk
    Documents/ilanaSynth Samples/<Category>/*.wav          48 kHz 24-bit, "<instrument> <note>.wav"
    Documents/ilanaSynth Samples/<Category>/Textures/*.wav long tails and seamless loops for granular
    build/content/MANIFEST.md

Only numpy and the standard library are used. The output is deterministic: no
randomness, sorted file walks, and fixed processing order.
"""

import argparse
import math
import os
import re
import shutil
import struct
import sys
import time
import wave
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
SRC_AKWF = ROOT / "build" / "content" / "akwf" / "AKWF"
SRC_VCSL = ROOT / "build" / "content" / "vcsl"
DOCS = Path(os.environ.get("USERPROFILE", str(Path.home()))) / "Documents"
WT_OUT = DOCS / "ilanaSynth Wavetables" / "AKWF"
SMP_OUT = DOCS / "ilanaSynth Samples"
MANIFEST = ROOT / "build" / "content" / "MANIFEST.md"
MARKER = ".build_content"

FRAME = 2048
WT_PEAK = 0.95
SMP_RATE = 48000
SMP_PEAK = 10 ** (-1 / 20)                   # -1 dBFS
TAIL_FLOOR = 10 ** (-70 / 20)                # -70 dBFS
ONSET = 10 ** (-50 / 20)                     # onset threshold for leading-silence trim
SIZE_LIMIT = 1.5 * 1024 ** 3

# Serum's 'clm ' text. 48 bytes, so the chunk is even-sized and needs no pad
# byte. ilanaSynth reads the integer after "<!>" as the frame size.
CLM_TEXT = b"<!>2048 10000000 wavetable (www.xferrecords.com)"


# --------------------------------------------------------------------------
# WAV I/O
# --------------------------------------------------------------------------

def read_pcm_wav(path):
    """Returns (rate, float64 array of shape (frames, channels))."""
    with wave.open(str(path), "rb") as w:
        nch, sw, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if sw == 1:
        a = (np.frombuffer(raw, np.uint8).astype(np.float64) - 128.0) / 128.0
    elif sw == 2:
        a = np.frombuffer(raw, "<i2").astype(np.float64) / 32768.0
    elif sw == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        v = np.where(v >= 1 << 23, v - (1 << 24), v)
        a = v.astype(np.float64) / 8388608.0
    elif sw == 4:
        a = np.frombuffer(raw, "<i4").astype(np.float64) / 2147483648.0
    else:
        raise ValueError(f"{path}: unsupported sample width {sw}")
    return rate, a.reshape(-1, nch)


def _riff(chunks):
    body = b"WAVE"
    for cid, data in chunks:
        body += cid + struct.pack("<I", len(data)) + data
        if len(data) & 1:
            body += b"\0"
    return b"RIFF" + struct.pack("<I", len(body)) + body


def write_wav24(path, x, rate):
    x = np.asarray(x, np.float64)
    if x.ndim == 1:
        x = x[:, None]
    nch = x.shape[1]
    q = np.clip(np.round(x * 8388607.0), -8388608, 8388607).astype("<i4")
    data = q.reshape(-1).view(np.uint8).reshape(-1, 4)[:, :3].tobytes()
    fmt = struct.pack("<HHIIHH", 1, nch, rate, rate * nch * 3, nch * 3, 24)
    Path(path).write_bytes(_riff([(b"fmt ", fmt), (b"data", data)]))


def write_wavetable(path, frames):
    """frames: (N, 2048) float array; written as mono float32 at 44.1 kHz."""
    data = np.asarray(frames, "<f4").reshape(-1).tobytes()
    fmt = struct.pack("<HHIIHH", 3, 1, 44100, 44100 * 4, 4, 32)
    Path(path).write_bytes(_riff([(b"fmt ", fmt), (b"clm ", CLM_TEXT), (b"data", data)]))


def read_riff_chunks(path):
    """Minimal RIFF parser (the wave module cannot read IEEE float files)."""
    raw = Path(path).read_bytes()
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE file")
    if struct.unpack("<I", raw[4:8])[0] != len(raw) - 8:
        raise ValueError("RIFF size mismatch")
    chunks, pos = {}, 12
    while pos + 8 <= len(raw):
        cid, size = raw[pos:pos + 4], struct.unpack("<I", raw[pos + 4:pos + 8])[0]
        chunks[cid] = raw[pos + 8:pos + 8 + size]
        pos += 8 + size + (size & 1)
    return chunks


# --------------------------------------------------------------------------
# Output folder ownership
# --------------------------------------------------------------------------

def claim_folder(path):
    """Deletes a folder this script made before (it has the marker) and makes it anew."""
    path = Path(path)
    if path.exists():
        if not (path / MARKER).exists():
            sys.exit(f"refusing to clear {path}: it exists but was not made by build_content.py")
        shutil.rmtree(path)
    path.mkdir(parents=True)
    (path / MARKER).write_text("Made by ilanaSynth tools/build_content.py; cleared and rewritten on every run.\n")


def clear_owned_children(parent):
    parent = Path(parent)
    if parent.exists():
        for child in sorted(parent.iterdir()):
            if child.is_dir() and (child / MARKER).exists():
                shutil.rmtree(child)


# --------------------------------------------------------------------------
# Wavetables
# --------------------------------------------------------------------------

# Folder -> output family. Tiny families are merged with a relative so every
# table has at least 16 real frames. The twenty numbered folders are an
# unsorted grab bag, so they are pooled and re-sorted as "Assorted".
FAMILY_MAP = {
    "AKWF_aguitar": "Acoustic Guitar", "AKWF_altosax": "Alto Sax", "AKWF_birds": "Birds",
    "AKWF_bitreduced": "Bit Reduced", "AKWF_bw_blended": "Blended",
    "AKWF_bw_saw": "Saw", "AKWF_bw_sawbright": "Saw", "AKWF_bw_sawgap": "Saw Gap",
    "AKWF_bw_sawrounded": "Saw Rounded", "AKWF_bw_sin": "Sine and Basic",
    "AKWF_bw_perfectwaves": "Sine and Basic", "AKWF_bw_squ": "Square",
    "AKWF_bw_squrounded": "Square Rounded", "AKWF_bw_tri": "Triangle", "AKWF_c604": "C604",
    "AKWF_cello": "Bowed Strings", "AKWF_violin": "Bowed Strings", "AKWF_clarinett": "Clarinet",
    "AKWF_clavinet": "Clavinet", "AKWF_dbass": "Double Bass", "AKWF_distorted": "Distorted",
    "AKWF_ebass": "Electric Bass", "AKWF_eguitar": "Electric Guitar", "AKWF_eorgan": "Electric Organ",
    "AKWF_epiano": "Electric Piano", "AKWF_flute": "Woodwinds", "AKWF_oboe": "Woodwinds",
    "AKWF_fmsynth": "FM Synth", "AKWF_granular": "Granular", "AKWF_hdrawn": "Hand Drawn",
    "AKWF_hvoice": "Human Voice", "AKWF_linear": "Linear", "AKWF_oscchip": "Osc Chip",
    "AKWF_overtone": "Overtone", "AKWF_piano": "Piano", "AKWF_pluckalgo": "Pluck and String Box",
    "AKWF_stringbox": "Pluck and String Box", "AKWF_raw": "Raw", "AKWF_sinharm": "Sine Harmonics",
    "AKWF_snippets": "Snippets", "AKWF_stereo": "Stereo", "AKWF_symetric": "Symmetric",
    "AKWF_theremin": "Theremin", "AKWF_vgame": "Video Game", "AKWF_vgamebasic": "Video Game Basic",
}
ASSORTED = "Assorted"
MIN_FRAMES, MAX_FRAMES = 16, 64


def cycle_to_frame(x):
    """One cycle of any length -> 2048 samples by FFT: keep harmonics below 1024, drop DC and Nyquist."""
    n = len(x)
    spec = np.fft.rfft(x)
    top = min((n - 1) // 2, FRAME // 2 - 1)    # highest harmonic kept; the even-n Nyquist bin is ambiguous
    out = np.zeros(FRAME // 2 + 1, complex)
    out[1:top + 1] = spec[1:top + 1] * (FRAME / n)
    return np.fft.irfft(out, FRAME)


def frame_features(frame):
    mag = np.abs(np.fft.rfft(frame))[1:FRAME // 2]
    total = mag.sum()
    if total <= 0:
        return None
    harm = np.arange(1, FRAME // 2)
    centroid = float((harm * mag).sum() / total)
    power = mag[:299] ** 2 + 1e-24
    flatness = float(np.exp(np.mean(np.log(power))) / np.mean(power))
    high = float(power[50:].sum() / power.sum())
    # Distance feature: log magnitude of the first 128 harmonics, relative to the strongest.
    m = mag[:128] / mag.max()
    feat = np.log10(m + 1e-3)
    unit = mag[:299] / np.linalg.norm(mag[:299])
    return dict(centroid=centroid, flatness=flatness, high=high, feat=feat, unit=unit)


def chain_order(feats):
    """Greedy nearest-neighbour path from the darkest frame, then 2-opt; returns (order, cost)."""
    n = len(feats)
    F = np.stack([f["feat"] for f in feats])
    D = np.sqrt(((F[:, None, :] - F[None, :, :]) ** 2).sum(-1))
    start = int(np.argmin([f["centroid"] for f in feats]))
    order, used = [start], np.zeros(n, bool)
    used[start] = True
    for _ in range(n - 1):
        d = np.where(used, np.inf, D[order[-1]])
        nxt = int(np.argmin(d))
        order.append(nxt)
        used[nxt] = True
    order = np.array(order)
    if n <= 400:                                   # 2-opt on an open path
        improved, passes = True, 0
        while improved and passes < 20:
            improved, passes = False, passes + 1
            for i in range(1, n - 1):
                # gain of reversing order[i:j+1] for every j > i
                a, b = order[i - 1], order[i]
                cj = order[i + 1:]
                dj = np.append(order[i + 2:], -1)
                old = D[a, b] + np.where(dj >= 0, D[cj, np.maximum(dj, 0)], 0.0)
                new = D[a, cj] + np.where(dj >= 0, D[b, np.maximum(dj, 0)], 0.0)
                gain = old - new
                j = int(np.argmax(gain))
                if gain[j] > 1e-9:
                    order[i:i + j + 2] = order[i:i + j + 2][::-1]
                    improved = True
    cost = float(D[order[:-1], order[1:]].sum())
    return order, D, cost


def split_path(order, D):
    """Cuts an ordered path into tables of 16..64 frames, preferring cuts at the biggest jumps."""
    n = len(order)
    if n <= MAX_FRAMES:
        return [order]
    jumps = D[order[:-1], order[1:]]               # jumps[i]: between i and i+1
    bias = float(np.quantile(jumps, 0.75))
    best = np.full(n + 1, -np.inf)
    back = np.zeros(n + 1, int)
    best[0] = 0.0
    for end in range(MIN_FRAMES, n + 1):
        for length in range(MIN_FRAMES, min(MAX_FRAMES, end) + 1):
            s = end - length
            if best[s] == -np.inf:
                continue
            gain = best[s] + (jumps[end - 1] - bias if end < n else 0.0)
            if gain > best[end]:
                best[end], back[end] = gain, s
    cuts, end = [], n
    while end > 0:
        cuts.append((back[end], end))
        end = back[end]
    return [order[s:e] for s, e in reversed(cuts)]


def tone_word(centroid):
    for limit, word in ((1.5, "Sine"), (3, "Soft"), (6, "Warm"), (12, "Mellow"), (24, "Bright"), (48, "Harsh")):
        if centroid < limit:
            return word
    return "Buzz"


def load_akwf():
    """Returns {family: [(label, frame, features)]} in sorted-file order, noise and duplicates removed."""
    families, kept_units, kept_frames, skipped = {}, [], [], {"noise": [], "duplicate": [], "silent": []}
    folders = sorted(p for p in SRC_AKWF.iterdir() if p.is_dir())
    # Named families first so a duplicate is dropped from the grab bag, not from its real family.
    folders.sort(key=lambda p: (p.name not in FAMILY_MAP, p.name))
    for folder in folders:
        fam = FAMILY_MAP.get(folder.name, ASSORTED if re.fullmatch(r"AKWF_\d{4}", folder.name) else None)
        if fam is None:
            skipped.setdefault("unmapped", []).append(folder.name)
            continue
        for wav in sorted(folder.glob("*.wav")):
            _, a = read_pcm_wav(wav)
            frame = cycle_to_frame(a[:, 0])        # stereo family: left channel
            f = frame_features(frame)
            label = f"{folder.name}/{wav.name}"
            if f is None or np.abs(frame).max() < 1e-4:
                skipped["silent"].append(label)
                continue
            if f["flatness"] > 0.2 and f["high"] > 0.3:
                skipped["noise"].append(label)
                continue
            # Duplicate: same magnitude spectrum AND the same waveform up to a circular shift and gain.
            if kept_units:
                near = np.flatnonzero(np.stack(kept_units) @ f["unit"] > 0.9995)
                spec = np.fft.rfft(frame)
                dup = False
                for i in near:
                    other = kept_frames[i]
                    xc = np.fft.irfft(spec * np.conj(np.fft.rfft(other)), FRAME)
                    shifted = np.roll(other, int(np.argmax(xc)))
                    gain = np.dot(frame, shifted) / np.dot(shifted, shifted)
                    if np.abs(frame - gain * shifted).max() < 0.01 * np.abs(frame).max():
                        dup = True
                        break
                if dup:
                    skipped["duplicate"].append(label)
                    continue
            kept_units.append(f["unit"])
            kept_frames.append(frame)
            families.setdefault(fam, []).append((label, frame, f))
    return families, skipped


def pad_to_min(frames, feats):
    """Inserts crossfaded frames at the largest jumps until there are 16 (for 12..15-frame sets)."""
    frames, feats = list(frames), list(feats)
    while len(frames) < MIN_FRAMES:
        F = np.stack([f["feat"] for f in feats])
        jumps = np.sqrt(((F[1:] - F[:-1]) ** 2).sum(-1))
        i = int(np.argmax(jumps))
        mid = 0.5 * (frames[i] + frames[i + 1])
        frames.insert(i + 1, mid)
        feats.insert(i + 1, frame_features(mid))
    return frames


def build_wavetables(log):
    t0 = time.time()
    claim_folder(WT_OUT)
    families, skipped = load_akwf()
    tables, notes = [], []
    for fam in sorted(families):
        items = families[fam]
        feats = [it[2] for it in items]
        if len(items) < 12:
            skipped.setdefault("too small", []).append(f"{fam} ({len(items)} frames)")
            continue
        chain, D, chain_cost = chain_order(feats)
        numbered = np.arange(len(items))
        numbered_cost = float(D[numbered[:-1], numbered[1:]].sum())
        # A family whose own numbering is already about as smooth as the best path is a sweep: keep it.
        if fam != ASSORTED and numbered_cost <= 1.25 * chain_cost:
            order, how = numbered, "numbering (sweep)"
        else:
            order, how = chain, "spectral nearest-neighbour path"
        parts = split_path(order, D)
        notes.append((fam, len(items), how, len(parts), numbered_cost / max(chain_cost, 1e-9)))
        famdir = WT_OUT / fam
        famdir.mkdir()
        for k, part in enumerate(parts, 1):
            frames = [items[i][1] for i in part]
            pfeats = [items[i][2] for i in part]
            if len(frames) < MIN_FRAMES:
                frames = pad_to_min(frames, pfeats)
            T = np.stack(frames)
            T -= T.mean(axis=1, keepdims=True)
            T *= WT_PEAK / np.abs(T).max()
            c0, c1 = pfeats[0]["centroid"], pfeats[-1]["centroid"]
            w0, w1 = tone_word(c0), tone_word(c1)
            desc = w0 if w0 == w1 else f"{w0} to {w1}"
            name = f"{fam} {k:02d} {desc}.wav" if len(parts) > 1 else f"{fam} {desc}.wav"
            path = famdir / name
            write_wavetable(path, T)
            tables.append(dict(family=fam, path=path, frames=len(T), size=path.stat().st_size,
                               sources=[items[i][0] for i in part]))
    log(f"wavetables: {len(tables)} tables in {time.time() - t0:.1f} s")
    return tables, notes, skipped


# --------------------------------------------------------------------------
# Samples
# --------------------------------------------------------------------------

NOTE_RE = re.compile(r"([A-G])(#?)(-?\d)")
DYN = {"ppp": 1, "pp": 2, "p": 3, "mp": 4, "mf": 5, "f": 6, "ff": 7, "fff": 8,
       "quiet": 3, "soft": 3, "med": 5, "medium": 5, "loud": 7}
PC = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def parse_name(stem):
    """-> (note_token, midi, velocity, rr) from a VCSL file name; note is None if unpitched."""
    toks = re.split(r"[_ ]", stem)
    note = midi = vel = rr = None
    last_num = None
    for t in toks:
        m = NOTE_RE.fullmatch(t)
        if m and note is None:
            note = t
            midi = 12 * (int(m.group(3)) + 1) + PC[m.group(1)] + (1 if m.group(2) else 0)
            continue
        m = re.fullmatch(r"(?:v|vl)(\d+)", t)
        if m:
            vel = int(m.group(1))
            continue
        m = re.fullmatch(r"(ppp|pp|p|mp|mf|f|ff|fff|quiet|soft|med|medium|loud)(\d*)", t)
        if m:
            vel = DYN[m.group(1)]
            if m.group(2):
                rr = int(m.group(2))
            continue
        m = re.fullmatch(r"(?:rr|RR|r)(\d+)", t)
        if m:
            rr = int(m.group(1))
            continue
        if re.fullmatch(r"\d+", t):
            last_num = int(t)
    if rr is None:
        rr = last_num if last_num is not None else 1
    return note, midi, vel if vel is not None else 0, rr


def pick_notes(midis, k):
    notes = sorted(set(midis))
    if len(notes) <= k:
        return notes
    lo, hi = notes[0], notes[-1]
    seg = (hi - lo) / (k - 1)
    chosen = []
    for i in range(k):
        target = lo + seg * i
        cand = [n for n in notes if n not in chosen]
        chosen.append(min(cand, key=lambda n: (abs(n - target) / seg + 0.35 * min(n % 12, 12 - n % 12) / 6, n)))
    return sorted(chosen)


def P(cat, name, folder, include=None, n=6, octave=1):
    """A pitched instrument. Most VCSL sets name notes with C3 as middle C, so by default the output
    name is one octave up: scientific pitch of the sounding note, C4 = middle C = 261.6 Hz (as
    ilanaSynth displays notes). octave is the correction, checked by measuring each set's pitch."""
    return dict(kind="pitched", cat=cat, name=name, folder=folder, include=include, n=n, octave=octave)


NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def note_name(midi):
    return f"{NAMES[midi % 12]}{midi // 12 - 1}"


def X(cat, folder, picks):
    """Explicit picks: [(file stem, output name)]."""
    return dict(kind="explicit", cat=cat, folder=folder, picks=picks)


def T(cat, folder, stem, name, loop=None):
    """A texture: a long tail, or with loop=(start, end) fractions a seamless crossfaded loop."""
    return dict(kind="texture", cat=cat, folder=folder, stem=stem, name=name, loop=loop)


SI = "Idiophones/Struck Idiophones/"
FI = "Idiophones/Friction Idiophones/"
PI = "Idiophones/Plucked Idiophones/"
ZI = "Chordophones/Zithers/"
CC = "Chordophones/Composite Chordophones/"

PLAN = [
    # ---- Metal
    X("Metal", SI + "Anvil", [("Anvil_Hit1_v2_rr1_Mid", "Anvil Hit 1"), ("Anvil_Hit2_v2_rr1_Mid", "Anvil Hit 2"),
                              ("Anvil_Hit3_v2_rr1_Mid", "Anvil Hit 3")]),
    X("Metal", SI + "Brake Drum", [("BrakeDrum1_Hammer_v2_rr1_Mid", "Brake Drum 1 Hammer"),
                                   ("BrakeDrum1_YarnM_v2_rr1_Mid", "Brake Drum 1 Yarn Mallet"),
                                   ("BrakeDrum1_Susp_v3_rr1_Mid", "Brake Drum 1 Suspended"),
                                   ("BrakeDrum2_Hammer3_v2_rr1_Mid", "Brake Drum 2 Hammer"),
                                   ("BrakeDrum2_Susp_v2_rr1_Mid", "Brake Drum 2 Suspended")]),
    X("Metal", SI + "Clash Cymbals 1", [("cymbal_crash1_mf1", "Clash Cymbals 1 Crash"),
                                        ("cymbal_crash1_short1", "Clash Cymbals 1 Choked")]),
    X("Metal", SI + "Clash Cymbals 2", [("cymbal_crash2_f1", "Clash Cymbals 2 Crash")]),
    X("Metal", SI + "Suspended Cymbal 1", [("susCymb1_hit_f1", "Suspended Cymbal 1 Mallet"),
                                           ("susCymb1_hit_bell_mf1", "Suspended Cymbal 1 Bell"),
                                           ("susCymb1_hit_stick_f1", "Suspended Cymbal 1 Stick"),
                                           ("susCymb1_cresc_4s", "Suspended Cymbal 1 Swell")]),
    X("Metal", SI + "Suspended Cymbal 2", [("susCymb2_hit_mf1", "Suspended Cymbal 2 Mallet"),
                                           ("susCymb2_hit_bell_f1", "Suspended Cymbal 2 Bell"),
                                           ("susCymb2_hit_stick_mf1", "Suspended Cymbal 2 Stick"),
                                           ("susCymb2_cresc_4s", "Suspended Cymbal 2 Swell")]),
    X("Metal", SI + "Hi-Hat Cymbal", [("HiHat_HitC_v3_rr1_Mid", "Hi-Hat Closed"), ("HiHat_HitO_rr1_Mid", "Hi-Hat Open"),
                                      ("HiHat_HitLoose_rr1_Mid", "Hi-Hat Loose")]),
    X("Metal", SI + "Gong 1", [("gong_mf", "Gong mf"), ("gong_2_mp", "Gong Soft Strike")]),
    X("Metal", SI + "Flexatone", [("flexatone1", "Flexatone Shake"), ("flexatone_slap1", "Flexatone Slap"),
                                  ("flexatone_long", "Flexatone Long")]),
    X("Metal", SI + "Triangles", [("Triangle1_Hit_v2_rr1_Mid", "Triangle 1 Hit"),
                                  ("Triangle3_Hit_v2_rr1_Mid", "Triangle 3 Hit"),
                                  ("Triangle6_Hit_v2_rr1_Mid", "Triangle 6 Hit"),
                                  ("Triangle3_HitM_v2_rr1_Mid", "Triangle 3 Muted")]),
    X("Metal", SI + "Finger Cymbals", [("Fing_Cymb", "Finger Cymbals")]),
    # ---- Bells
    P("Bells", "Tubular Bells 1", SI + "Tubular Bells 1"),
    P("Bells", "Tubular Bells 2", SI + "Tubular Bells 2", octave=0),
    P("Bells", "Hand Chimes", SI + "Hand Chimes"),
    P("Bells", "Glockenspiel", SI + "Glockenspiel"),
    P("Bells", "Tubular Glockenspiel", SI + "Tubular Glockenspiel", octave=6),   # "non-standard pitch"
    P("Bells", "Bell Tree", SI + "Bell Tree/Individual"),
    X("Bells", SI + "Hand Bells, Nepalese", [("HB_1", "Nepalese Hand Bell 1"), ("HB_2", "Nepalese Hand Bell 2"),
                                             ("HB_3", "Nepalese Hand Bell 3")]),
    X("Bells", SI + "Agogo Bells", [("Agogo_High_v2_rr1_Mid", "Agogo High"), ("Agogo_Low_v2_rr1_Mid", "Agogo Low")]),
    X("Bells", SI + "Cowbells", [("Cowbell1_Normal_v3_rr1_Mid", "Cowbell 1"), ("Cowbell1_Muted_v3_rr1_Mid", "Cowbell 1 Muted"),
                                 ("Cowbell2_Normal_v3_rr1_Mid", "Cowbell 2"), ("Cowbell2_Double_v3_rr1_Mid", "Cowbell 2 Double")]),
    X("Bells", SI + "Sleigh Bells", [("Sleighbells_Hit_rr1_Mid", "Sleigh Bells Hit"), ("sleighbell2_shake1", "Sleigh Bells Shake")]),
    # ---- Glass
    P("Glass", "Wine Glass Rubbed Slow", FI + "Wine Glasses/Sustains/Slow"),
    P("Glass", "Wine Glass Rubbed Fast", FI + "Wine Glasses/Sustains/Fast"),
    # ---- Wood & Mallets
    P("Wood & Mallets", "Marimba", SI + "Marimba"),
    P("Wood & Mallets", "Xylophone Hard", SI + "Xylophone/Hard Mallets"),
    P("Wood & Mallets", "Xylophone Soft", SI + "Xylophone/Soft Mallets"),
    P("Wood & Mallets", "Vibraphone Hard", SI + "Vibraphone/Hard Mallets"),
    P("Wood & Mallets", "Vibraphone Soft", SI + "Vibraphone/Soft Mallets"),
    P("Wood & Mallets", "Balafon", SI + "Balafon/Traditional Mallet"),
    P("Wood & Mallets", "Balafon Hard", SI + "Balafon/Hard Mallet"),
    P("Wood & Mallets", "Balafon Soft", SI + "Balafon/Soft Mallet"),
    P("Wood & Mallets", "Xylophone Medium", SI + "Xylophone/Medium Mallets"),
    X("Wood & Mallets", SI + "Slit Drum", [("LogDrumHi_MedM_v2_rr1_Sum", "Slit Drum High"),
                                           ("LogDrumLo_MedM_v2_rr1_Sum", "Slit Drum Low")]),
    X("Wood & Mallets", SI + "Woodblock", [("wood_click_mp", "Woodblock 1"), ("wood_click2_mp", "Woodblock 2"),
                                           ("wood_click3_vl2", "Woodblock 3")]),
    X("Wood & Mallets", SI + "Claves", [("Claves1_Hit_v2_rr1_Mid", "Claves 1"), ("Claves2_Hit_v2_rr1_Mid", "Claves 2")]),
    X("Wood & Mallets", SI + "Cajon", [("Cajon_hit1_f_rr1", "Cajon Bass"), ("Cajon_hit2_f_rr1", "Cajon Slap"),
                                       ("Cajon_hit3_f_rr1", "Cajon Edge")]),
    X("Wood & Mallets", SI + "Slapstick", [("slapstick_rr1", "Slapstick")]),
    # ---- Friction & Bowed
    P("Friction & Bowed", "Vibraphone Bowed", SI + "Vibraphone/Bowed"),
    P("Friction & Bowed", "Bowed Psaltery", ZI + "Psaltery, Bowed and Plucked/LongBow"),
    P("Friction & Bowed", "Bowed Psaltery Spiccato", ZI + "Psaltery, Bowed and Plucked/Spiccato"),
    X("Friction & Bowed", SI + "Brake Drum", [("BrakeDrum1_Bowed_rr2_Mid", "Brake Drum 1 Bowed"),
                                              ("BrakeDrum2_Bowed_rr1_Mid", "Brake Drum 2 Bowed")]),
    X("Friction & Bowed", SI + "Suspended Cymbal 1", [("susCymb1_bow_7", "Suspended Cymbal 1 Bowed 1"),
                                                      ("susCymb1_bow_13", "Suspended Cymbal 1 Bowed 2"),
                                                      ("susCymb1_scrape_1", "Suspended Cymbal 1 Scrape")]),
    X("Friction & Bowed", SI + "Suspended Cymbal 2", [("susCymb2_bowed_2", "Suspended Cymbal 2 Bowed 1"),
                                                      ("susCymb2_bowed_4", "Suspended Cymbal 2 Bowed 2"),
                                                      ("susCymb2_scrape_2", "Suspended Cymbal 2 Scrape")]),
    X("Friction & Bowed", SI + "Bell Tree - Legacy", [("bell_tree_metalscrape1", "Bell Tree Metal Scrape"),
                                                      ("bell_tree_woodscrape1", "Bell Tree Wood Scrape")]),
    X("Friction & Bowed", SI + "Guiro", [("Guiro_Slow_rr2_Mid", "Guiro Slow"), ("Guiro_Med_rr1_Mid", "Guiro Medium"),
                                         ("Guiro_Fast_rr1_Mid", "Guiro Fast")]),
    X("Friction & Bowed", SI + "Cabasa", [("Cabasa1_Rub_v2_rr1_Mid", "Cabasa Rub")]),
    X("Friction & Bowed", SI + "Ratchet", [("Ratchet1_Slow_rr1_Mid", "Ratchet Slow"),
                                           ("Ratchet2_Crank_v1_rr1_Mid", "Ratchet Crank")]),
    # ---- Plucked
    P("Plucked", "Kalimba Kenya", PI + "Kalimba, Kenya"),
    P("Plucked", "Kalimba Tanzania", PI + "Kalimba, Tanzania"),
    P("Plucked", "Mbira Mavembe", PI + "Mbira Mavembe (Gandanga), Zimbabwe, Low G"),
    P("Plucked", "Mbira Nyamaropa", PI + "Mbira dzaVadzimu Nyamaropa, Zimbabwe, Low B"),
    P("Plucked", "Nyunga Nyunga", PI + "Nyunga Nyunga, Mozambique, Low F"),
    P("Plucked", "Psaltery Pluck", ZI + "Psaltery, Bowed and Plucked/Pluck"),
    P("Plucked", "Dan Tranh", ZI + "Dan Tranh/Normal"),
    P("Plucked", "Dan Tranh Tremolo", ZI + "Dan Tranh/Tremolo"),
    P("Plucked", "Dan Tranh Vibrato", ZI + "Dan Tranh/Vibrato"),
    P("Plucked", "Strumstick", CC + "Strumstick/Finger"),
    # ---- Strings
    P("Strings", "Concert Harp", CC + "Concert Harp", octave=0),
    P("Strings", "Folk Harp", CC + "Folk Harp"),
    P("Strings", "Grand Piano Kawai", ZI + "Grand Piano, Kawai/Sustains"),
    P("Strings", "Grand Piano Steinway", ZI + "Grand Piano, Steinway B/Sus", octave=0),
    P("Strings", "Grand Piano Steinway Dry", ZI + "Grand Piano, Steinway B/NoSus", octave=0),
    P("Strings", "Upright Piano Knight", ZI + "Upright Piano, Knight/Sustains"),
    P("Strings", "Upright Piano Yamaha", ZI + "Upright Piano, Yamaha/Sustains"),
    P("Strings", "Harpsichord English", ZI + "Harpsichord, English/Sustains/Normal"),
    P("Strings", "Harpsichord English Lute", ZI + "Harpsichord, English/Sustains/Lute"),
    P("Strings", "Harpsichord Flemish", ZI + "Harpsichord, Flemish/Sustains/Low"),
    P("Strings", "Harpsichord Flemish Upper", ZI + "Harpsichord, Flemish/Sustains/High"),
    P("Strings", "Harpsichord Kit", ZI + "Harpsichord, Unk/Sustains"),
    P("Strings", "Harpsichord French", ZI + "Harpsichord, French/Sustains"),
    P("Strings", "Harpsichord Italian", ZI + "Harpsichord, Italian/Sustains/stop1"),
    # ---- Textures: long tails
    T("Metal", SI + "Gong 1", "gong_fff", "Gong fff Tail"),
    T("Metal", SI + "Gong 1", "gong_f", "Gong f Tail"),
    T("Metal", SI + "Gong 1", "gong_p", "Gong p Tail"),
    T("Metal", SI + "Gong 1", "gong_2_f", "Gong Second Strike Tail"),
    T("Metal", SI + "Clash Cymbals 2", "cymbal_crash2_fff1", "Clash Cymbals fff Tail"),
    T("Metal", SI + "Suspended Cymbal 1", "susCymb1_roll_f1_rel", "Suspended Cymbal 1 Roll"),
    T("Metal", SI + "Suspended Cymbal 2", "susCymb2_roll_pp1_rel", "Suspended Cymbal 2 Soft Roll"),
    T("Metal", SI + "Suspended Cymbal 1", "susCymb1_cresc_7.5s", "Suspended Cymbal 1 Long Swell"),
    T("Metal", SI + "Flexatone", "flexatone_extralong", "Flexatone Extra Long"),
    T("Metal", SI + "Triangles", "Triangle1_Roll_rr1_Mid", "Triangle Roll"),
    T("Metal", SI + "Mark Trees/Legacy", "windchimes_random", "Mark Trees Random"),
    T("Metal", SI + "Mark Trees/Legacy", "windchimes_slowAsc1", "Mark Trees Slow Rise"),
    T("Metal", SI + "Mark Trees/Legacy", "windchimes_slowDesc1", "Mark Trees Slow Fall"),
    T("Bells", SI + "Bell Tree/Stroke", "BellTree_Stroke_1_Mid", "Bell Tree Stroke 1"),
    T("Bells", SI + "Bell Tree/Stroke", "BellTree_Stroke_10_Mid", "Bell Tree Stroke 2"),
    T("Friction & Bowed", SI + "Suspended Cymbal 1", "susCymb1_bow_17", "Suspended Cymbal 1 Bowed Tail"),
    T("Friction & Bowed", SI + "Suspended Cymbal 1", "susCymb1_bow_20", "Suspended Cymbal 1 Bowed Long"),
    T("Friction & Bowed", SI + "Suspended Cymbal 2", "susCymb2_bowed_6", "Suspended Cymbal 2 Bowed Tail"),
    T("Friction & Bowed", SI + "Gong 1", "gong_scrape_mf", "Gong Scrape"),
    T("Friction & Bowed", SI + "Bell Tree - Legacy", "bell_tree_scrape1", "Bell Tree Scrape 1"),
    T("Friction & Bowed", SI + "Bell Tree - Legacy", "bell_tree_scrape3", "Bell Tree Scrape 2"),
    T("Plucked", ZI + "Dan Tranh/Gliss", "Gliss_Up_Slw_ff_2", "Dan Tranh Slow Gliss Up"),
    T("Plucked", ZI + "Dan Tranh/Gliss", "Gliss_Dwn_Slw_ff_1", "Dan Tranh Slow Gliss Down"),
    T("Plucked", ZI + "Dan Tranh/FX", "FX_01", "Dan Tranh FX 1"),
    T("Plucked", ZI + "Dan Tranh/FX", "FX_30", "Dan Tranh FX 2"),
    T("Strings", ZI + "Grand Piano, Kawai/Sustains", "GPiano_sus_A#-1_v3_rr1_Player", "Grand Piano Low Resonance"),
    # ---- Textures: seamless loops (crossfaded) of sustained sounds
    T("Glass", FI + "Wine Glasses/Sustains/Slow", "glass1_D#4_Slow_1_Main", "Wine Glass D#5 Loop", loop=(0.25, 0.85)),
    T("Glass", FI + "Wine Glasses/Sustains/Slow", "glass3_A#4_Slow_1_Main", "Wine Glass A#5 Loop", loop=(0.25, 0.85)),
    T("Glass", FI + "Wine Glasses/Sustains/Fast", "glass2_F#4_Fast_1_Main", "Wine Glass F#5 Loop", loop=(0.25, 0.85)),
    T("Glass", FI + "Wine Glasses/Sustains/Fast", "glass4_D5_Fast_1_Main", "Wine Glass D6 Loop", loop=(0.25, 0.85)),
    T("Metal", SI + "Suspended Cymbal 1", "susCymb1_roll_mp1_nloop", "Suspended Cymbal Roll Loop", loop=(0.3, 0.9)),
    T("Metal", SI + "Suspended Cymbal 2", "susCymb2_roll_fff1_nloop", "Suspended Cymbal Loud Roll Loop", loop=(0.3, 0.9)),
    T("Metal", SI + "Triangles", "Triangle6_Roll_v2_rr1_Mid", "Triangle Roll Loop", loop=(0.25, 0.8)),
    T("Friction & Bowed", ZI + "Psaltery, Bowed and Plucked/LongBow", "BowedPsaltery_C4_Main_LongBow_rr1",
      "Bowed Psaltery C5 Loop", loop=(0.25, 0.8)),
    T("Plucked", ZI + "Dan Tranh/Tremolo", "B2_Trem_1", "Dan Tranh Tremolo Loop", loop=(0.2, 0.8)),
]


def resolve_plan():
    """-> list of jobs (src, dst, loop, meta) and a list of problems."""
    jobs, problems = [], []
    for spec in PLAN:
        folder = SRC_VCSL / spec["folder"]
        files = sorted(folder.glob("*.wav"))
        if not files:
            problems.append(f"no files in {spec['folder']}")
            continue
        by_stem = {f.stem: f for f in files}
        if spec["kind"] == "pitched":
            parsed = []
            for f in files:
                note, midi, vel, rr = parse_name(f.stem)
                if note is not None and (spec["include"] is None or spec["include"] in f.stem):
                    parsed.append((midi, note, vel, rr, f))
            if not parsed:
                problems.append(f"no pitched files in {spec['folder']}")
                continue
            levels = sorted({p[2] for p in parsed})
            target = levels[int(round(0.65 * (len(levels) - 1)))]   # a medium-loud layer
            for midi in pick_notes([p[0] for p in parsed], spec["n"]):
                cands = [p for p in parsed if p[0] == midi]
                best = min(cands, key=lambda p: (abs(p[2] - target), -p[2], p[3], p[4].name))
                dst = SMP_OUT / spec["cat"] / f"{spec['name']} {note_name(midi + 12 * spec['octave'])}.wav"
                jobs.append((str(best[4]), str(dst), None, spec["cat"], False))
        elif spec["kind"] == "explicit":
            for stem, name in spec["picks"]:
                if stem not in by_stem:
                    problems.append(f"missing {spec['folder']}/{stem}.wav")
                    continue
                jobs.append((str(by_stem[stem]), str(SMP_OUT / spec["cat"] / f"{name}.wav"), None, spec["cat"], False))
        else:
            if spec["stem"] not in by_stem:
                problems.append(f"missing {spec['folder']}/{spec['stem']}.wav")
                continue
            dst = SMP_OUT / spec["cat"] / "Textures" / f"{spec['name']}.wav"
            jobs.append((str(by_stem[spec["stem"]]), str(dst), spec["loop"], spec["cat"], True))
    seen = {}
    for j in jobs:
        if j[1] in seen:
            problems.append(f"two sources for {j[1]}: {seen[j[1]]} and {j[0]}")
        seen[j[1]] = j[0]
    srcs = [j[0] for j in jobs if j[2] is None]      # a loop may reuse a note's source
    for s in sorted({s for s in srcs if srcs.count(s) > 1}):
        problems.append(f"source used twice: {s}")
    return jobs, problems


_KERNELS = {}


def resample(x, sr_in, sr_out, half=48, beta=9.0):
    """Polyphase Kaiser-windowed-sinc resampler. x: (N, C) float64."""
    if sr_in == sr_out:
        return x
    g = math.gcd(sr_in, sr_out)
    up, down = sr_out // g, sr_in // g
    key = (up, down, half, beta)
    if key not in _KERNELS:
        fc = 0.93 * min(1.0, up / down)            # cutoff relative to the input Nyquist
        j = np.arange(-half + 1, half + 1)
        phase = (np.arange(up) * down) % up
        t = j[None, :] - phase[:, None] / up
        win = np.i0(beta * np.sqrt(np.clip(1 - (t / half) ** 2, 0, 1))) / np.i0(beta)
        H = fc * np.sinc(fc * t) * win
        H /= H.sum(axis=1, keepdims=True)
        _KERNELS[key] = (H, (np.arange(up) * down) // up, j)
    H, base, j = _KERNELS[key]
    n, nch = x.shape
    m = int(math.ceil(n * up / down))
    q = int(math.ceil(m / up))
    pad_front = half
    xp = np.zeros((pad_front + n + down * q + 2 * half + up, nch))
    xp[pad_front:pad_front + n] = x
    out = np.zeros((q * up, nch))
    s = xp.strides
    for r in range(up):
        start = pad_front + base[r] + j[0]
        view = np.lib.stride_tricks.as_strided(xp[start:], shape=(q, len(j), nch),
                                               strides=(down * s[0], s[0], s[1]))
        out[r::up] = np.einsum("qjc,j->qc", view, H[r])
    return out[:m]


def block_peaks(x, block):
    a = np.abs(x).max(axis=1)
    nb = int(math.ceil(len(a) / block))
    a = np.pad(a, (0, nb * block - len(a)))
    return a.reshape(nb, block).max(axis=1)


def onset_index(x, sr):
    a = np.abs(x).max(axis=1)
    head = a[:int(0.005 * sr)].max(initial=0.0)
    if head >= 10 ** (-30 / 20):                   # starts on the sound itself
        return 0
    thr = max(ONSET, 4 * head)
    idx = np.flatnonzero(a > thr)
    return int(idx[0]) if len(idx) else 0


def tail_index(x, sr):
    block = int(0.01 * sr)
    env = block_peaks(x, block)
    above = np.flatnonzero(env > TAIL_FLOOR)
    return len(x) if not len(above) else min(len(x), (int(above[-1]) + 1) * block)


def normalise(x):
    return x * (SMP_PEAK / np.abs(x).max())


def process_sample(job):
    src, dst, loop, cat, texture = job
    sr, x = read_pcm_wav(src)
    x = x - x.mean(axis=0)
    x = normalise(x)
    # Coarse trim at the source rate (keeps margins for the resampler), then resample.
    a, b = onset_index(x, sr), tail_index(x, sr)
    x = x[max(0, a - int(0.03 * sr)):min(len(x), b + int(0.05 * sr))]
    x = resample(x, sr, SMP_RATE)
    x = x - x.mean(axis=0)
    x = normalise(x)
    # Fine trim at 48 kHz: 2 ms pre-roll (faded in) before the onset, cut where the tail falls below -70 dBFS.
    a = onset_index(x, SMP_RATE)
    pre = int(0.002 * SMP_RATE)
    s0 = max(0, a - pre)
    x = x[s0:tail_index(x, SMP_RATE)].copy()
    fin = max(16, a - s0)
    x[:fin] *= (0.5 - 0.5 * np.cos(np.pi * np.arange(fin) / fin))[:, None]
    if loop is None:
        fout = int(min(0.25 * SMP_RATE, 0.1 * len(x)))
        x[len(x) - fout:] *= (0.5 + 0.5 * np.cos(np.pi * np.arange(fout) / fout))[:, None]
    else:
        # Seamless loop: the region [p, q) with its first L samples crossfaded (equal power) with what follows q.
        n = len(x)
        p, q = int(loop[0] * n), int(loop[1] * n)
        cross = int(min(1.0 * SMP_RATE, 0.3 * (q - p), n - q))
        seg = x[p:q].copy()
        t = np.arange(cross) / cross
        seg[:cross] = x[p:p + cross] * np.sin(0.5 * np.pi * t)[:, None] + x[q:q + cross] * np.cos(0.5 * np.pi * t)[:, None]
        x = seg - seg.mean(axis=0)
    x = normalise(x)
    Path(dst).parent.mkdir(parents=True, exist_ok=True)
    write_wav24(dst, x, SMP_RATE)
    return dict(src=src, dst=dst, cat=cat, texture=texture, loop=loop is not None,
                channels=x.shape[1], seconds=len(x) / SMP_RATE, size=Path(dst).stat().st_size)


def build_samples(log, jobs_n):
    t0 = time.time()
    jobs, problems = resolve_plan()
    for p in problems:
        log("plan problem: " + p)
    if problems:
        sys.exit("fix the plan before building samples")
    SMP_OUT.mkdir(parents=True, exist_ok=True)
    clear_owned_children(SMP_OUT)
    for cat in sorted({j[3] for j in jobs}):
        claim_folder(SMP_OUT / cat)
    # Longest files first so the pool stays busy; results are written to fixed paths, so order does not matter.
    jobs.sort(key=lambda j: -os.path.getsize(j[0]))
    with ProcessPoolExecutor(max_workers=jobs_n) as pool:
        results = list(pool.map(process_sample, jobs, chunksize=1))
    results.sort(key=lambda r: r["dst"])
    log(f"samples: {len(results)} files in {time.time() - t0:.1f} s")
    return results


# --------------------------------------------------------------------------
# Verification and manifest
# --------------------------------------------------------------------------

def verify(tables, samples, log):
    report, errors = [], []
    # Wavetables: every file, with the RIFF parser (the wave module has no IEEE-float support).
    frames_seen, wt_peak, wt_bytes = [], 0.0, 0
    for t in tables:
        c = read_riff_chunks(t["path"])
        fmt = struct.unpack("<HHIIHH", c[b"fmt "][:16])
        if fmt != (3, 1, 44100, 176400, 4, 32):
            errors.append(f"{t['path']}: fmt {fmt}")
        if c.get(b"clm ") != CLM_TEXT:
            errors.append(f"{t['path']}: bad clm chunk")
        d = np.frombuffer(c[b"data"], "<f4")
        if len(d) % FRAME or not MIN_FRAMES <= len(d) // FRAME <= MAX_FRAMES:
            errors.append(f"{t['path']}: {len(d)} samples")
        if not np.all(np.isfinite(d)):
            errors.append(f"{t['path']}: NaN/Inf")
        pk = float(np.abs(d).max())
        if pk > WT_PEAK + 1e-6:
            errors.append(f"{t['path']}: peak {pk}")
        means = np.abs(d.reshape(-1, FRAME).mean(axis=1)).max()
        if means > 1e-5:
            errors.append(f"{t['path']}: DC {means}")
        frames_seen.append(len(d) // FRAME)
        wt_peak = max(wt_peak, pk)
        wt_bytes += Path(t["path"]).stat().st_size
    if tables:
        report.append(f"Wavetables: {len(tables)} files re-read; frames per table {min(frames_seen)}-{max(frames_seen)} "
                      f"(all multiples of 2048 samples); max peak {wt_peak:.6f} (target 0.95); no NaN/Inf; "
                      f"'clm ' chunk present in all.")
    # Samples: every file, with the wave module.
    lsb = 1 / 8388607
    sm_peak_lo, sm_peak_hi, sm_bytes = 1.0, 0.0, 0
    for s in samples:
        with wave.open(s["dst"], "rb") as w:
            if (w.getframerate(), w.getsampwidth()) != (SMP_RATE, 3) or w.getnchannels() not in (1, 2):
                errors.append(f"{s['dst']}: format {w.getparams()}")
        _, x = read_pcm_wav(s["dst"])
        if not np.all(np.isfinite(x)):
            errors.append(f"{s['dst']}: NaN")
        pk = float(np.abs(x).max())
        if pk > SMP_PEAK + 2 * lsb:
            errors.append(f"{s['dst']}: peak {pk}")
        sm_peak_lo, sm_peak_hi = min(sm_peak_lo, pk), max(sm_peak_hi, pk)
        sm_bytes += Path(s["dst"]).stat().st_size
    if samples:
        report.append(f"Samples: {len(samples)} files re-opened with the wave module (48 kHz, 24-bit); "
                      f"peaks {20 * math.log10(sm_peak_lo):.3f} to {20 * math.log10(sm_peak_hi):.3f} dBFS "
                      f"(target -1 dBFS); no NaN.")
        report.append(f"Total sample size {sm_bytes / 1024 ** 3:.3f} GiB "
                      f"({'within' if sm_bytes <= SIZE_LIMIT else 'OVER'} the 1.5 GiB cap).")
        if sm_bytes > SIZE_LIMIT:
            errors.append("samples exceed 1.5 GiB")
    for e in errors:
        log("VERIFY ERROR: " + e)
    log("\n".join(report))
    return report, errors


def fmt_mb(n):
    return f"{n / 1024 ** 2:.1f} MiB"


def write_manifest(tables, notes, wt_skipped, samples, report):
    L = ["# ilanaSynth content manifest", "",
         "Generated by `tools/build_content.py`. Re-running it clears and rewrites every output folder listed here.", ""]
    if tables:
        total = sum(t["size"] for t in tables)
        L += ["## Wavetables", "",
              f"`{WT_OUT}`: {len(tables)} tables, {sum(t['frames'] for t in tables)} frames, {fmt_mb(total)}.", "",
              "Each table is 16-64 frames of 2048 samples, mono 32-bit float at 44.1 kHz, with Serum's `clm ` chunk. "
              "Every source cycle was resampled by FFT (harmonics 1-1023 kept, DC and the source's Nyquist bin dropped), "
              "then each table had its DC removed and was normalised to a peak of 0.95.", "",
              "| Family | Source waves kept | Tables | Order | Size |", "|---|---:|---:|---|---:|"]
        for fam, n, how, parts, ratio in notes:
            size = sum(t["size"] for t in tables if t["family"] == fam)
            L.append(f"| {fam} | {n} | {parts} | {how} | {fmt_mb(size)} |")
        L += ["", "Ordering: a family whose own file numbering is already a smooth sweep (its adjacent-frame spectral "
              "distance within 1.25x of the best path found) keeps that order; otherwise frames follow a greedy "
              "nearest-neighbour path through log-harmonic-spectrum space, starting from the darkest frame and "
              "refined by 2-opt. Families longer than 64 frames are cut into 16-64-frame tables at the largest "
              "timbral jumps. Table names end with the tone at the start and end of the table (by spectral centroid).",
              "", "Skipped AKWF waves:", ""]
        for why, items in wt_skipped.items():
            if items:
                shown = ", ".join(items[:12]) + (" ..." if len(items) > 12 else "")
                L.append(f"- {why}: {len(items)} ({shown})")
        L += ["- The `AKWF--*` folders (Surge, Teensy, Elektron, ...) are format conversions of the same waves and "
              "were not used; `AKWF-c`, `AKWF-js` and `AKWF-png` are code and images.", ""]
    if samples:
        L += ["## Samples", "", f"`{SMP_OUT}`: 48 kHz 24-bit WAV, stereo or mono as the source.", "",
              "| Category | Files | Textures | Size | Audio |", "|---|---:|---:|---:|---:|"]
        cats = sorted({s["cat"] for s in samples})
        for c in cats:
            ss = [s for s in samples if s["cat"] == c]
            tx = [s for s in ss if s["texture"]]
            L.append(f"| {c} | {len(ss) - len(tx)} | {len(tx)} | {fmt_mb(sum(s['size'] for s in ss))} | "
                     f"{sum(s['seconds'] for s in ss) / 60:.1f} min |")
        tot = sum(s["size"] for s in samples)
        L.append(f"| **Total** | {sum(not s['texture'] for s in samples)} | {sum(s['texture'] for s in samples)} | "
                 f"{fmt_mb(tot)} | {sum(s['seconds'] for s in samples) / 60:.1f} min |")
        L += ["", "Processing: DC removed; leading silence trimmed to a 2 ms faded pre-roll; the tail cut where it "
              "falls below -70 dBFS and faded out over its last 0.25 s (or 10%); resampled 44.1 to 48 kHz with a "
              "96-tap Kaiser-windowed-sinc polyphase filter; normalised to -1 dBFS peak. Pitched instruments: one "
              "medium-loud velocity layer and the first round robin, up to six notes spread over the range with a "
              "preference for Cs. Note names are the sounding pitch in scientific notation (C4 = middle C = 261.6 Hz, as "
              "ilanaSynth displays notes): most VCSL sets name notes with C3 as middle C and were moved up an octave, "
              "checked by measuring each set's pitch; Concert Harp, the Steinway and Tubular Bells 2 already used C4, "
              "and the Tubular Glockenspiel (marked non-standard pitch in VCSL) sounds six octaves above its labels. "
              "Tubular bells and the bell tree are inharmonic, so their names follow the library's labels. "
              "Textures are long tails or seamless loops (equal-power crossfade, up to 1 s) of "
              "sustained sounds, for the granular engine.", "", "VCSL folders used:", ""]
        used = sorted({str(Path(s["src"]).parent.relative_to(SRC_VCSL)).replace("\\", "/") for s in samples})
        L += [f"- {u}" for u in used]
        L += ["", "Not used: pianos' and harpsichords' release-trigger samples (not useful alone), the "
              "Kawai Legacy piano and legacy triangles/tambourines/claves/agogo (older duplicates of newer sets), "
              "claps, shakers and tambourines (off-theme), and `Gong 2` / `Tubular Bells 3 - Legacy` (MP3/OGG only, "
              "which the standard library cannot decode). This VCSL clone has no crotales, waterphone or singing bowls.",
              ""]
    L += ["## Verification", ""] + [f"- {r}" for r in report] + [""]
    L += ["## Licence", "",
          "Both sources are dedicated to the public domain under CC0 1.0 Universal "
          "(https://creativecommons.org/publicdomain/zero/1.0/). No attribution is required; credit is given anyway.", "",
          "- Adventure Kid Waveforms (AKWF-FREE) by Kristoffer Ekstrand: https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE",
          "- Versilian Community Sample Library (VCSL) by Versilian Studios / Samuel Gossner: https://github.com/sgossner/VCSL",
          ""]
    MANIFEST.write_text("\n".join(L), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--only", choices=["wavetables", "samples"])
    ap.add_argument("--jobs", type=int, default=max(1, min(12, (os.cpu_count() or 2) - 2)))
    args = ap.parse_args()
    t0 = time.time()
    log = lambda s: print(s, flush=True)
    for src in (SRC_AKWF, SRC_VCSL):
        if not src.is_dir():
            sys.exit(f"missing source folder {src}")
    tables, notes, wt_skipped, samples = [], [], {}, []
    if args.only in (None, "wavetables"):
        tables, notes, wt_skipped = build_wavetables(log)
    if args.only in (None, "samples"):
        samples = build_samples(log, args.jobs)
    report, errors = verify(tables, samples, log)
    if args.only is None:
        write_manifest(tables, notes, wt_skipped, samples, report)
        log(f"manifest: {MANIFEST}")
    log(f"done in {time.time() - t0:.1f} s")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
