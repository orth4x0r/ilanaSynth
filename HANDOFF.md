# Current handoff

Updated 2026-09-25. Keep this note to the current state rather than appending a changelog.

## State

ROADMAP v1.2 milestones M1, M2 and M3 are implemented and pushed to `origin/main` (M2 `531a48f`, M3 `da0d6c3`, Claude's polish `6a1fcd1`). Existing oscillator modes, exciter choices and LFO shape indices retain their positions.

M2 adds Bounce, Pendulum, Spring and Friction after the existing LFO shapes. Both global and per-voice render paths share the stateful motion code. RETRIG resets motion; Pendulum's optional velocity-scaled kick works on note-on even without RETRIG. Two per-LFO controls change labels and meaning with the selected physics shape. Missing parameters in old states are filled from their declared defaults before `replaceState`.

M3 adds Bow as the last Physical exciter choice, with bow pressure and speed, aftertouch and MPE pressure response. Bridge buzz and velocity-sensitive fret rattle default off. A processor-level bank of up to six sympathetic strings listens to the summed voice signal before the FX rack, then rings after input stops. It follows the GENERATE scale and root or six manual MIDI notes. All delay storage is allocated in `prepareToPlay`.

M3b phase A is **complete** on `main`; phase B has **not started**. Oscillator parameters, per-voice storage and rendering, processor fills, OSC page controls, and MAIN oscillator strip setup use indexed data. The historical `osc1`/`osc2`/`sub` parameter IDs remain unchanged, OSC 3's special sub controls still work, and the count remains 3 until phase B. Phase A makes no sound or behaviour change. Nothing from M3b has been pushed.

## Verification

- M3b baseline was captured **before edits** with `build/ilanaFingerprint_artefacts/Release/ilanaFingerprint.exe build/m3b-before.csv` (235 presets). The existing target built successfully with `cmake --build build --config Release --target ilanaFingerprint` using sandbox escalation for MSBuild's file tracker.
- After the intermediate voice commit, `cmake --build build --config Release` built all targets, `ilanaTableTest.exe` passed (0 failures; `build/m3b-phase-a-progress-tests.log`), and `ilanaSnapshot.exe --uitest` passed (0 failures). `compare_fingerprints.py build/m3b-before.csv build/m3b-phase-a-progress.csv` reported **0 of 235 changed**.
- Phase A checkpoint: `cmake --build build --config Release` built all targets; `build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe` passed with 0 failures (`build/m3b-phase-a-final-tests.log`); `build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe --uitest` passed with 0 failures, including indexed granular checks for OSC 1–3. `python tools/compare_fingerprints.py build/m3b-before.csv build/m3b-phase-a-final.csv` reported **0 of 235 changed**. `ilanaSnapshot.exe build/m3b-phase-a-snapshots` generated the UI snapshots; default MAIN, OSC and Physical OSC were inspected with no clipping or overlap.

- Before edits: `cmake --build build --config Release --target ilanaFingerprint`; `build/ilanaFingerprint_artefacts/Release/ilanaFingerprint.exe build/m2m3-before.csv` wrote 235 repeatable fingerprints.
- M2: `cmake --build build --config Release` succeeded. `build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe` passed with 0 failures (`build/m2-tests-final.log`); `python tools/compare_fingerprints.py build/m2m3-before.csv build/m2-after.csv` reported 0 of 235 changed.
- M3: `cmake --build build --config Release` succeeded for all targets. `build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe` passed with 0 failures (`build/m2m3-tests-final.log`). Six sympathetic strings on Swarm took 120.0 ms/s against the 400 ms/s heavy-preset budget. The legacy extreme-unison timing check passed at 45.8%; an earlier busy run measured 54% and its idle rerun passed at 44.6%.
- `python tools/compare_fingerprints.py build/m2m3-before.csv build/m2m3-after-final.csv` reported **0 of 235 changed**.
- `build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe --uitest` passed. `build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe snapshots/m2m3` generated physics LFO and OSC bow/buzz/sympathetic snapshots. The Pendulum, Bounce, bow/buzz and manual sympathetic layouts were inspected with no overlaps.

## Claude's M2/M3 review and polish

- Physics LFOs no longer go flat when free-running: once Bounce, Pendulum or Spring settles it is re-excited smoothly (ball thrown back up from the floor, pendulum/spring pushed from rest). Pendulum no longer jumps on retrigger (`resetPhysics` now outputs the displayed swing, not the raw angle).
- Sympathetic strings: with no GENERATE scale they use an open tuning on the root (root, 5th, octave, 3rd, 5th, octave) instead of a C..F semitone cluster; the loop filter delay is compensated so they ring in tune (measured 0.0 / 0.5 cents at C2 / C5, was flat); `sym_amount` defaults to 50 % so switching ON is audible (still neutral while `sym_on` is off).
- Bridge buzz now acts inside the string loop (a jawari-style, magnitude-reducing contact curve) instead of an output waveshaper; it cannot sustain or grow the string.
- OSC page: sympathetic strings are a one-line header with the ON switch; their settings open below only when ON (notes only in MANUAL). The default page fits without scrolling again (outer margin 12 -> 6 px, minimum card 124 px).
- Integer parameters now use `describeValue` like the floats: drone notes show note names (C3 = MIDI 60) and accept typed names, STRINGS shows "3 strings", SEMI shows "st".
- State loading collects saved parameter IDs in a set instead of an O(n^2) scan.
- Tests added: physics motion never goes flat and never jumps, bounces get lower, retriggers start without a jump, sympathetic tuning and open-tuning fallback, buzz stability, integer value text.
- Verified: all tests pass except the heavy-preset CPU timing check, which failed only because the laptop was on battery (Sol's unmodified M2/M3 also failed then: 73.7 %); fingerprints 0 of 235 changed (`build/m2m3-review.csv`); `--uitest` passes; snapshots inspected. Run the CPU test plugged in and idle.

## Decisions to review

- During phase A, the old OSC 3 string-mode transition ordering is preserved explicitly because the no-sound-change checkpoint takes precedence. This can be simplified in phase B after the phase A fingerprint checkpoint.

- Physics motion speed follows the existing free or tempo synced LFO rate. The generic controls mean Height/Bounce, Swing/Damp, Stiff/Damp or Drive/Stick. Kick strength follows note velocity in both global and per-voice paths.
- The shared sympathetic controls live on the OSC page below the voice strip because they act on the combined oscillator output. The card has its own height so the Physical controls keep their knob size and the page scrolls.
- With scale tuning, the six strings occupy ascending notes from MIDI 48 plus the GENERATE root. Manual mode uses six MIDI note controls. The drone bank is mono in the stereo field to keep one shared resonator.

## Known gaps

No known M2 or M3 gaps. Phase B remains: six oscillators, Both routing, 6×6 FM, 16 envelopes, per-oscillator amp envelopes, QUALITY, UI, migration tests, CPU test, README, and final verification. Do not mark ROADMAP M3b done until those pass. M3b has not been pushed.
