#!/usr/bin/env python3
"""Compare two preset fingerprint CSVs from ilanaFingerprint.

Flags presets whose loudness, brightness or stereo width moved more than the
run-to-run noise (random unison phases, drift, S&H) can explain.

    python3 tools/compare_fingerprints.py before.csv after.csv
"""
import csv
import sys


# Presets driven by random sources (S&H, chaotic feedback) vary run to run by
# more than the default tolerance; compare them loosely.
NOISY = {"Clock Weirdo", "S&H Techno", "Self Osc Drone", "Chord Pad", "Wide Chord Pad", "Minor Chord Stab",
         # v1.0 library presets built on random phases, S&H, drift or random arps
         "Ladder Sub", "MS-20 Growl", "Octave Hyper Bass", "Dark Matter", "Per-Voice Drift Pad",
         "Random Glass Arp", "Diode Rumble", "Deep Space Drone", "Radio Tuning", "Granular Cloud", "Pluck Sub Bass",
         "Warp Wobble", "Glitch Gate", "Pluck Walker",
         # v1.1 presets built on chaos/random LFOs, grains, spray and scale-random arps
         "Metal Hat", "Granular Clap", "Drunk Tom", "Feedback Choir", "Harmonic Cut Pad", "Grain Choir", "Swarm",
         "Vinyl Dust Keys", "Frozen Grain Lead", "Lorenz Bass", "Drunk Tape Keys", "Chaos Filter Pad",
         "Pentatonic Rain", "Scale Walker", "Hirajoshi Spray", "Blues Machine", "Granular Riser", "Lorenz Radio",
         "Bitwise Arp", "Throat Drone"}


def load(path):
    with open(path, newline="") as handle:
        return {row["name"]: row for row in csv.DictReader(handle)}


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    before, after = load(sys.argv[1]), load(sys.argv[2])
    changed = 0

    for name, old in before.items():
        new = after.get(name)
        if new is None:
            print(f"MISSING  {name}")
            changed += 1
            continue

        rms_delta = float(new["rms_db"]) - float(old["rms_db"])
        old_centroid = max(1.0, float(old["centroid_hz"]))
        centroid_ratio = float(new["centroid_hz"]) / old_centroid
        side_delta = float(new["side_ratio"]) - float(old["side_ratio"])

        scale = 8.0 if name in NOISY else 1.0
        problems = []
        if abs(rms_delta) > 1.0 * scale:
            problems.append(f"level {rms_delta:+.2f} dB")
        if abs(centroid_ratio - 1.0) > 0.08 * scale:
            problems.append(f"brightness x{centroid_ratio:.2f}")
        if abs(side_delta) > 0.08 * scale:
            problems.append(f"width {side_delta:+.2f}")

        if problems:
            changed += 1
            print(f"CHANGED  {name}: " + ", ".join(problems))

    print(f"{changed} of {len(before)} presets changed")
    return 1 if changed else 0


if __name__ == "__main__":
    sys.exit(main())
