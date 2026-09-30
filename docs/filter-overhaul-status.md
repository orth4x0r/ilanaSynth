# Filter overhaul: status (WIP, stopped 2026-09-30 for the move to the cloud)

Branch `worktree-agent-a0aff2c6ac2a294a0`, based on `a734563` (after `39a2f21`).
The product's filters are **not changed yet**: no product source file is
modified, so fingerprints and the existing tests are unaffected. ilanaTableTest,
ilanaSnapshot and ilanaFingerprint build (Windows, Release); `ILANA_FILTER_TEST=1`
passes (it only prints reports so far). The full suite was not rerun: its only
change is the CPU report appended at its end.

## Done
- **Baselines, made before any model change** (Windows/MSVC, 48 kHz), in `docs/filter-overhaul/`:
  - `fingerprints-before-windows.csv`: `ilanaFingerprint` on all 371 presets plus the 25 "Filter model:" rows. Two runs were identical (0 of 396 changed). Linux baselines differ; make a Linux one from this commit before changing models in the cloud.
  - `levels-before.txt`: `LEVEL { type, slope24, 9 x rms dB }`, a 110 Hz band-limited saw at 0.7 through every type at cutoffs 300 / 1000 / 3000 Hz and resonance 0 / 0.5 / 0.9. Meant to be pasted into `tests/FilterTests.inc` as the level reference (each rebuilt model should stay within about 1 dB of its old row).
  - `cpu-before.txt`: ns per sample per model (coefficients refreshed every 16 samples, sweeping cutoff). Average 46.8 ns in that run; a later run gave 30.9 ns on the same code (the "Silent" power plan swings about 1.5x), so compare only alternating runs of old and new builds.
  - `selfosc-before.txt`: self-oscillation pitch error and level at resonance 1 for 50 Hz..10 kHz. The old SVF, Ladder LP/HP and MS-20 LP track within 0.1 %; OTA LP is 1-31 % flat (unit-delay loop), MS-20 HP 0.2-16 % flat, Ladder BP/Drive stop oscillating above ~500 Hz, Diode LP, SEM, Steiner never oscillate. SVF at reso 0.99 does not sustain; at 0.995 rings at rms 0.05.
  - `preset-usage.txt`: which factory presets use which type (with resonance, drive, slope). Low Pass 199, Ladder LP 41, Band Pass 14, Diode LP 7, High Pass 6, Formant 5, Vowel 5, Twin Peak 5, MS-20 LP 4, OTA LP 4, SEM 4, Comb + 3, Morph 3, the rest 2 each. Only two presets run the SVF above 0.98 resonance (a Low Pass at 0.995, Self-Osc Choir's Band Pass at 0.99).
- **`tests/FilterTests.inc`** (new), included from `tests/TableTest.cpp`, run by `ILANA_FILTER_TEST=1` (wired like `ILANA_POLISH_TEST`) and at the end of the full suite. For now it only prints the per-model CPU report; `ILANA_FILTER_LEVELS=1`, `ILANA_FILTER_USAGE=1` and `ILANA_FILTER_OSC=1` print the three reports above. The checks (stability, cutoff, self-oscillation pitch, DC, new models' behaviour) are still to write.
- **`src/dsp/FilterCore.h`** (new, compiled only by the tests so far): the shared pieces for the rebuild:
  - `tanhApprox` / `tanhOverX` (Pade 7/6 tanh and its secant gain),
  - `Halfband2x`: 2x polyphase IIR half-band up/down sampler (4 coefficients, de Soras design, transition 0.08: flat to 16 kHz at 48 kHz, >63 dB rejection of anything that folds below 18 kHz; coefficients computed and checked in Python),
  - `solveLoop`: exact solve of u = L tanh((v - a u) / L) (linear start + 2 Newton steps),
  - `StageCascade`: four TPT one-poles with a solved ZDF loop, per-stage secant-gain saturation (the transistor/OTA pair), optional high-pass stages 3-4 (OTA band-pass) and positive feedback,
  - `DiodeLadderCore`: the diode ladder as its tridiagonal node model solved per sample (Thomas) with ZDF feedback, per-diode-pair secant saturation, optional feedback high-pass. Checked in Python: equal capacitors give Stinchcombe's D(s) = s^4 + 7s^3 + 15s^2 + 10s + 1; halving the top node's capacitor gives his TB-303 D(s) = s^4 + 6.727s^3 + 14.142s^2 + 9.514s + 1 (after the frequency scaling). `coreResponse()` gives the same linear response for the display.
  None of these is unit-tested yet.

## Left (in order)
1. Rebuild the models on FilterCore (plan below), keeping FilterType indices and the `FilterUnit` API (`prepare`, `setType`, `makeCoefficients`, `setCoefficients`, `process`).
2. Append the four new types after Twin Peak (25..28): **303 Acid**, **Moog Drive**, **Vowel Morph**, **Comb Body**; names in `FilterType::getNames()`, `usesMorph`, `response()`, the second grid page in `src/gui/FilterWidgets.h` (ANALOG: + 303 Acid, Moog Drive; VOICE: + Vowel Morph; SHAPES or VOICE: + Comb Body), `PluginEditor.cpp` hasSlope (line ~1527).
3. Drive inside the model for 303 Acid and Moog Drive only: add `FilterUnit::setDrive` and a `FilterType::drivesInside(type)`; in `Voice.cpp` (~line 1489) skip the front `tanh (x * drive)` for those types and pass the drive in. Old types keep the front drive, so their levels hold.
4. Tests in FilterTests.inc; level calibration against `levels-before.txt`; fingerprints against the baseline (rms within 1 dB, peak not above old + 1 dB); CPU (rebuilt average within ~1.5x of old, alternating runs).
5. Existing tests that encode old quirks: `runFilterModelTests` (SVF 24 dB must match the legacy cascade exactly below 0.98: keep that true), `runExtraFilterTests`, `M84Tests.inc` (e.g. "Ladder Drive self-oscillates" only checks ringing). Update with comments only if the model is now more correct; never loosen stability checks.
6. README filters list, one line in HANDOFF.md, `ilanaSnapshot --uitest`, FILTER page snapshot showing the new types.

## Design decisions (the plan for step 1)
- **Cutoff = self-oscillation frequency** for every resonant model, so full resonance plays in tune with key tracking. Everything in a loop is discretised with the same bilinear prewarp (g = tan(pi fc / fs) at the rate it runs), so the linear loop oscillates exactly at fc; loop saturation is memoryless (no phase), so it limits the amplitude without moving the pitch. k is set slightly above the oscillation threshold at resonance 1 so the tone sustains at a low, pitch-stable amplitude.
- **SVF types (Low/Band/High Pass, Notch, Morph)**: already TPT (Simper form). Keep the linear range bit-identical (199 presets use Low Pass; a test requires it). Above 0.98 replace the state tanh clipping with nonlinear (cubic-like) damping: kappa = k + c (1 - tanhOverX (bp_prev)), solved per sample, so the self-oscillation is clean and at pitch. Pick c so the level at 0.99-0.995 stays near the old one (see selfosc-before.txt).
- **Ladder LP/HP**: StageCascade with linear stages and the loop sum saturated (as before, but solved exactly), k = 4.15 r and the same (1 + 0.5k) compensation and taps as before (41 presets: keep the level), 2x oversampled.
- **Ladder BP / Ladder Drive**: same cascade with stage saturation (Huovilainen-style character, now ZDF so it oscillates at all cutoffs); old taps and output gains, then calibrate.
- **Moog Drive (new)**: drive into the loop's input pair, saturating stages, only partial bass compensation (the Moog's thinning), 24 dB.
- **OTA LP / BP**: StageCascade with OTA (tanh(x - y)) stages; LP negative feedback, BP (two LP + two HP stages) positive feedback (it oscillates at k = 4 at fc).
- **Diode LP**: DiodeLadderCore, equal capacitors (Stinchcombe), no feedback HP; osc at w0 = sqrt(10/7) in stage units, threshold k = 18.39; 12 dB tap = node 3. Note it is much darker than the old Moog-shaped model at the same cutoff (its -3 dB point at resonance 0 is 0.1 x the oscillation frequency): 7 presets use it, so compensate level in the model and check those 7 fingerprints.
- **303 Acid (new)**: DiodeLadderCore with the top capacitor halved (threshold k = 17.0 at w0 = 1.1892) and a one-pole high-pass in the feedback at 150 Hz (Open303's value; a1k0n's analysis names the feedback high-pass as the 303's one departure from the ideal ladder). The HP adds phase lead, so a table of (HP corner / cutoff) -> (core scale, threshold k) keeps the oscillation on pitch (solve arg Hcore(jw) = -pi - atan(rho)). Slope test: about -18 dB/octave in the octave above cutoff.
- **MS-20 LP / HP (Korg-35)**: ZDF Sallen-Key, saturation in the feedback path solved with Newton; K = 0.01 + 2.05 r (oscillates at K = 2, exactly at fc); 2x oversampled.
- **SEM**: nonlinear TPT SVF (integrator-input secant gains, the OTAs), never self-oscillates, 1x rate.
- **Steiner-Parker**: SVF with the diode-limited nonlinear damping, self-oscillates at resonance 1; 2x oversampled.
- **Phaser Notch**: solve its feedback through the four allpasses without the unit delay (Newton on x = in + fb tanh(A x + S)).
- **Combs (+, -, Damped, Morph)**: 4-point interpolation instead of linear, and the loop damping filter's phase delay at the pitch subtracted from the delay, so combs ring exactly at the cutoff (Comb -: at fc / 2).
- **Formant, Vowel, Talking, Twin Peak**: linear TPT band-pass banks already; keep.
- **Vowel Morph (new)**: Klatt-style cascade of five resonators (formant amplitudes come out of the cascade), A-E-I-O-U on MORPH, male (Csound tenor table) to female (Csound soprano table) as cutoff goes 1 -> 2 kHz, scaled beyond; resonance narrows the bandwidths.
- **Comb Body (new)**: comb at the cutoff feeding five modal resonators at free-free tube ratios 1, 2.756, 5.404, 8.933, 13.34 (a struck tube/chime), decays falling with mode number; MORPH blends comb and body.
- CPU: the fast tanh and cheaper ladder make room for 2x oversampling; the old Diode LP (114 ns) and Ladder BP/Drive (143 ns) are the expensive ones now.

## How to gate
```
cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=<juce-src> -DILANA_CLAP=OFF
cmake --build build --config Release --target ilanaTableTest ilanaSnapshot ilanaFingerprint
# after changing a class layout in a shared header (FilterUnit.h, Svf.h):
#   delete build/<target>.dir/Release/*.obj first
ILANA_FILTER_TEST=1 ilanaTableTest            # filter tests + CPU report
ILANA_FILTER_TEST=1 ILANA_FILTER_OSC=1 ilanaTableTest   # self-oscillation report
ILANA_FILTER_TEST=1 ILANA_FILTER_LEVELS=1 ilanaTableTest  # level table (compare with levels-before.txt)
ilanaTableTest                                # full suite, 0 failures
ilanaSnapshot --uitest                        # 0 failures
ilanaFingerprint after.csv
python tools/compare_fingerprints.py base.csv after.csv   # base made on the same platform before the change
```
Level rule (owner's decision): tone may change, level may not: every preset's rms_db within +-1 dB, peak not above old + 1 dB. Compensate inside the model, never in presets. Old filter models are replaced outright (no classic copies); indices are never renumbered, new types are appended.
