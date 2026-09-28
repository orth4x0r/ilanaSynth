#!/usr/bin/env python3
"""Compare two preset fingerprint CSVs from ilanaFingerprint.

Flags presets whose loudness, brightness or stereo width moved more than the
run-to-run noise (random unison phases, drift, S&H) can explain.

    python3 tools/compare_fingerprints.py before.csv after.csv [--fail]

Exits 1 when something changed (--fail is accepted for readability in CI).
Presets below -50 dB in both runs (the ilanaSynth FX input presets, which the
instrument renders without input: silence or the previous preset's tail)
are skipped.
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
         "Bitwise Arp", "Throat Drone",
         # v1.3 (M10) presets built on random sequencers, spray, chaos and drift
         "Harp Cascade", "Lorenz Wash", "Double Pendulum Chaos", "Henon Glitch", "Rossler Tide", "Prob Seq Bells",
         "Ratchet Arp Machine", "Scale Spray Keys", "Prob Seq Drone", "Buchla Bongo Melody", "Evolve Macro Pad",
         "Vector Path Drift", "Evolving Vector Drone", "Baked Chaos Loop", "Six-Voice Drift"}


def load(path):
    with open(path, newline="") as handle:
        return {row["name"]: row for row in csv.DictReader(handle)}


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    paths = [a for a in sys.argv[1:] if not a.startswith("--")]
    before, after = load(paths[0]), load(paths[1])
    changed = 0

    for name, old in before.items():
        new = after.get(name)
        if new is None:
            print(f"MISSING  {name}")
            changed += 1
            continue

        # Silent, or only the previous preset's tail (the tool plays them
        # back to back), in both runs.
        if float(new["rms_db"]) <= -50.0 and float(old["rms_db"]) <= -50.0:
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
