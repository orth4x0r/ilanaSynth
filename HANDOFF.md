# Current handoff

Updated 2026-09-25. Keep this note to the current state rather than appending a changelog.

## State

ROADMAP v1.2 M1 and M2 are implemented. M2 adds Bounce, Pendulum, Spring and Friction after all existing LFO choices. Both shared and per-voice LFOs run the new stateful shapes. RETRIG restarts the motion; Pendulum has an optional note kick. Two shape-dependent controls appear on the ENV/LFO page. Missing parameters in old states now load their declared defaults, including the M1 controls. M3 remains planned.

## Verification

- Baseline before edits: `cmake --build build --config Release --target ilanaFingerprint`; `build/ilanaFingerprint_artefacts/Release/ilanaFingerprint.exe build/m2m3-before.csv` wrote 235 fingerprints.
- `cmake --build build --config Release` succeeded for all targets. `build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe` passed (0 failures, `build/m2-tests-final.log`), including LFO range, motion, retrigger, extremes, and missing parameter migration. Heavy preset CPU timings were 55–96 ms/s on an idle machine, under the 400 ms/s budget.
- `build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe --uitest` passed. `build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe snapshots/m2m3` generated shape snapshots; the Pendulum controls and display were inspected.
- `python tools/compare_fingerprints.py build/m2m3-before.csv build/m2-after.csv` reported **0 of 235 changed**.

## Decisions to review

- Physics motion speed follows the existing free or tempo synced LFO rate. The two generic controls mean Height/Bounce, Swing/Damp, Stiff/Damp, or Drive/Stick for the four shapes.
- Pendulum kick uses note velocity for per-voice LFOs. The shared retrigger path applies a fixed moderate kick because its note velocity is not retained in that path.

## Known gaps

- M3 is not implemented yet. No changes have been pushed.
