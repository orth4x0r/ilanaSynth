# Filter overhaul: status (models done 2026-09-30, cloud, branch `claude/project-thread-xhtug0`)

REVIEW-PLAN step 4. Every model is rebuilt and the four new ones are in; old presets keep
their level. Still open: modulatable spectral warps (the other half of step 4, below).

## What changed
All in `src/dsp/FilterCore.h` (shared cores), `src/dsp/FilterUnit.h` and `src/dsp/FilterModels2.h`.
Type indices are unchanged; 25..28 are appended.

| Model | Now | Self-oscillation (tests) |
|---|---|---|
| Low / Band / High Pass, Notch, Morph | linear range bit-identical; above 0.98 the state clipping is replaced by nonlinear damping (Svf.h) | at the cutoff within 0.25 %, level even across the range |
| Ladder LP / HP | `StageCascade` (linear stages, loop sum saturating, solved exactly), 2x | 0.00 % |
| Diode LP | `DiodeLadderCore` (equal capacitors); slides up as resonance falls (4x at 0) to keep the old brightness; make-up gain part in, part out; soft output ceiling | 0.07 % (it never oscillated before) |
| MS-20 LP / HP | new `Korg35Core`: the loop's diode limiter solved per sample (one Newton step), 2x; K reaches 2 at resonance 0.97 | 0.02 % (HP was up to 16 % flat) |
| Ladder BP / Drive | `StageCascade` with saturating stages, 2x | 0.21 % (stopped oscillating above ~500 Hz before) |
| OTA LP / BP | `StageCascade` with OTA stages; BP uses positive feedback | 0.17 / 0.26 % (LP was up to 31 % flat, BP never) |
| SEM | integrators' gains sag with level (OTA secant gains) | never, as designed |
| Steiner-Parker | diode-limited nonlinear damping, 2x | 0.19 % (never before) |
| Phaser Notch | the feedback loop solved without the unit delay | n/a |
| Combs (+, -, Damped, Morph) | cubic Lagrange read; the damping filter's phase delay taken off the delay | ring at the pitch within 0.14 % (were up to 3 % flat) |
| **303 Acid** (25) | `DiodeLadderCore` with the top capacitor halved and a 150 Hz high-pass in the feedback; the core's frequency and the threshold solved from its response so it still oscillates at the cutoff; resonance keeps the bass; drive inside the loop | 0.08 % |
| **Moog Drive** (26) | `StageCascade` with saturating stages, drive into the loop's input, partial bass make-up (thins like a Minimoog), 2x | 0.21 % |
| **Vowel Morph** (27) | Klatt cascade of five formant resonators, A-E-I-O-U on MORPH, male (1 kHz) to female (2 kHz) with the cutoff, resonance narrows the bands | n/a |
| **Comb Body** (28) | the comb at the cutoff ringing five modes at a free-free tube's ratios (1, 2.756, 5.404, 8.933, 13.34); MORPH blends comb and body | n/a |

Formant, Vowel, Talking and Twin Peak were already linear TPT banks and are unchanged.
DRIVE goes inside for 303 Acid and Moog Drive only (`FilterType::drivesInside`, `FilterUnit::setDrive`,
`Voice.cpp`); the others keep the tanh in front, so their levels hold.

## How it was checked
- **Presets (the level rule):** `tools/check_level_rule.py tests/fingerprints-linux.csv after.csv`: every
  preset's rms within 1 dB and peak at most 1 dB above the baseline. All 396 rows pass. Calibration constants
  (make-up gains, limiter levels, output ceilings) were fitted with the fingerprints of the presets that use each
  model (`ILANA_FINGERPRINT_ONLY=a,b` renders only those; `ILANA_FINGERPRINT_DUMP=dir` writes the signals).
- **Level table:** `tests/FilterTests.inc` embeds the Linux table made before any change
  (`levels-before-linux.txt`); every cell within 1.25 dB up to resonance 0.5 and 2 dB at 0.9 (worst: Steiner
  at 300 Hz and 0.9, 1.84 dB; the old model clipped its states). `tools/compare_filter_levels.py` diffs two tables.
- **Tests** (`ILANA_FILTER_TEST=1`, also in the full suite): level table, self-oscillation pitch of 16 models,
  comb pitch, stability under loud noise and sweeps for all 29, DC and bass behaviour, the 303's bass against the
  Moog's, roll-off slopes, drive inside, Vowel Morph's vowels and voices, Comb Body's modes, the response display
  against the models, CPU.
- **CPU:** the rebuilt 25 average about 62 ns/sample on the Linux container against 59 before (1.05x; the
  budget is 1.5x). `ILANA_FILTER_CPU=1` prints the per-model report.
- Existing tests: `M84Tests.inc` MS-20 HP's passband holds at unity (the level match is a soft output ceiling,
  not a gain); nothing else needed changing.

## Airwindows
Checked for reusable filters: Capacitor2, Isolator2, Baxandall2, Air/Air3 and Pop2 are stereo mixing tools
(non-resonant shelves and one-poles with their own dither and state per channel), already reachable as the
Airwindows FX. None fits inside a per-voice resonant filter; nothing was reused.

## Left
- **Modulatable spectral warps.** Spectral warps are built per table on a worker thread (`SpectralCache`,
  64 amount steps), so SPECTRAL AMOUNT can't follow a per-voice modulator as it is. Options: a few precomputed
  amount levels crossfaded per voice (memory heavy for 256-frame tables), or a block-rate global amount. Not
  started.

## How to gate
```
cmake --build build --target ilanaTableTest ilanaSnapshot ilanaFingerprint
ILANA_FILTER_TEST=1 build/ilanaTableTest_artefacts/Release/ilanaTableTest    # filter tests only
ILANA_FILTER_TEST=1 ILANA_FILTER_LEVELS=1 ...                                  # print the level table
ILANA_FILTER_TEST=1 ILANA_FILTER_OSC=1 ...                                     # self-oscillation report
build/ilanaFingerprint_artefacts/Release/ilanaFingerprint after.csv
python3 tools/check_level_rule.py tests/fingerprints-linux.csv after.csv       # before updating the baseline
tools/verify.sh                                                                # the whole gate
```
Level rule (owner's decision): tone may change, level may not: every preset's rms_db within +-1 dB, peak not
above old + 1 dB. Compensate inside the model, never in presets. Indices are never renumbered; new types are
appended.
