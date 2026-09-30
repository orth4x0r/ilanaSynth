# Filter overhaul: status (models done 2026-09-30, cloud, branch `claude/project-thread-xhtug0`)

REVIEW-PLAN step 4, done. Every model is rebuilt and the four new ones are in; old presets keep
their level; SPECTRAL AMOUNT is a modulation destination (below).

## What changed
All in `src/dsp/FilterCore.h` (shared cores), `src/dsp/FilterUnit.h` and `src/dsp/FilterModels2.h`.
Type indices are unchanged; 25..28 are appended.

| Model | Now | Self-oscillation (tests) |
|---|---|---|
| Morph (and, before the Airwindows swap, Low / Band / High Pass and Notch) | linear range bit-identical; above 0.98 the state clipping is replaced by nonlinear damping (Svf.h) | at the cutoff within 0.25 %, level even across the range |
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
2026-09-30, the owner's call: **Low Pass, Band Pass, High Pass and Notch are now Airwindows' Y filters**
(YLowpass / YBandpass / YHighpass / YNotch, MIT; `src/dsp/AirwindowsFilters.h`). Each is the plugins' chain: a fixed
20 kHz Butterworth in, the encode curve (RESEDGE fixed at 0.25, p = 1.15^4 ~ 1.75, a table instead of `pow`), the
resonant two-pole, the decode curve, a fixed 20 kHz Butterworth out; the voice signal meets the curves at half level
(`headroom`). Two changes from the plugins: the resonant stage is computed as a zero-delay SVF with the biquad's
response (a direct-form biquad overshot up to +3.7 dB when a filter envelope moved the cutoff every 16 samples), and
the band-pass peaks at Q like the old one (YBandpass holds its peak at unity, which lost up to 29 dB). Resonance keeps
the old damping (Q = 1 / (2 - 2 r)), topped at Q 40: they ring at the cutoff (tests) but no longer self-oscillate.
24 dB puts both stages inside one curve pair. Presets: 389 changed tone, every one within the level rule; four needed
a trim change of 0.1-0.4 dB (Clock Weirdo, Rust Bass, Resonant Drop, Self-Osc Choir). Morph, Formant and the other
models are unchanged. Airwindows character filters are appended types 29-37 (`src/dsp/AirwindowsCharacter.h`, tests
`ILANA_AWFILTER_TEST`): AW Z LP / HP / BP (MORPH = poles, RESO = drive into the op-amp stage), AW Acid (ZAcidLowpass,
MORPH = meltdown), AW X LP (RESO = nuke), AW YNot LP (MORPH = resedge), AW Holt (MORPH = poles), AW Angle (MORPH =
hard), AW Pear (MORPH = nonlin, RESO = poles). Type 38 is the **Disperser**: up to 64 zero-delay all-pass sections at
the cutoff (KEY TRK tunes it to the note), RESO = sharpness (Q 0.5-8), MORPH = stages. Their display draws a plain
two-pole shape (flat for the Disperser); their level still moves with cutoff (up to ~9 dB for Z and X).

Earlier (step 4): Capacitor2, Isolator2, Baxandall2, Air/Air3 and Pop2 are mixing tools, already in the FX rack.

## Spectral warps, modulatable
Each oscillator's SPECTRAL AMOUNT is now a modulation destination ("OscN Spectral Amount", appended to
`Mod::getParamDestinations`, so no index moved). The offset is applied at block rate like the other parameter
destinations and picks one of `SpectralCache`'s 64 prebuilt amount steps, so it follows macros, LFOs and
envelopes per block (not per voice; per-voice would need the tables crossfaded per voice). Test: a macro on
Osc1 Spectral Amount with Stretch moves the spectral centroid (2375 to 2198 Hz); unrouted, it stays.

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
