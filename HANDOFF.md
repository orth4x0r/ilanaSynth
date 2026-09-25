# Current handoff

Updated 2026-09-25. Keep this note to the current state rather than appending a changelog.

## State

ROADMAP v1.2 milestones M1, M2 and M3 are implemented on local `main` in separate milestone commits. M2 is commit `531a48f`. Existing oscillator modes, exciter choices and LFO shape indices retain their positions. No changes have been pushed.

M2 adds Bounce, Pendulum, Spring and Friction after the existing LFO shapes. Both global and per-voice render paths share the stateful motion code. RETRIG resets motion; Pendulum's optional velocity-scaled kick works on note-on even without RETRIG. Two per-LFO controls change labels and meaning with the selected physics shape. Missing parameters in old states are filled from their declared defaults before `replaceState`.

M3 adds Bow as the last Physical exciter choice, with bow pressure and speed, aftertouch and MPE pressure response. Bridge buzz and velocity-sensitive fret rattle default off. A processor-level bank of up to six sympathetic strings listens to the summed voice signal before the FX rack, then rings after input stops. It follows the GENERATE scale and root or six manual MIDI notes. All delay storage is allocated in `prepareToPlay`.

## Verification

- Before edits: `cmake --build build --config Release --target ilanaFingerprint`; `build/ilanaFingerprint_artefacts/Release/ilanaFingerprint.exe build/m2m3-before.csv` wrote 235 repeatable fingerprints.
- M2: `cmake --build build --config Release` succeeded. `build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe` passed with 0 failures (`build/m2-tests-final.log`); `python tools/compare_fingerprints.py build/m2m3-before.csv build/m2-after.csv` reported 0 of 235 changed.
- M3: `cmake --build build --config Release` succeeded for all targets. `build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe` passed with 0 failures (`build/m2m3-tests-final.log`). Six sympathetic strings on Swarm took 120.0 ms/s against the 400 ms/s heavy-preset budget. The legacy extreme-unison timing check passed at 45.8%; an earlier busy run measured 54% and its idle rerun passed at 44.6%.
- `python tools/compare_fingerprints.py build/m2m3-before.csv build/m2m3-after-final.csv` reported **0 of 235 changed**.
- `build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe --uitest` passed. `build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe snapshots/m2m3` generated physics LFO and OSC bow/buzz/sympathetic snapshots. The Pendulum, Bounce, bow/buzz and manual sympathetic layouts were inspected with no overlaps.

## Decisions to review

- Physics motion speed follows the existing free or tempo synced LFO rate. The generic controls mean Height/Bounce, Swing/Damp, Stiff/Damp or Drive/Stick. Kick strength follows note velocity in both global and per-voice paths.
- The shared sympathetic controls live on the OSC page below the voice strip because they act on the combined oscillator output. The card has its own height so the Physical controls keep their knob size and the page scrolls.
- With scale tuning, the six strings occupy ascending notes from MIDI 48 plus the GENERATE root. Manual mode uses six MIDI note controls. The drone bank is mono in the stereo field to keep one shared resonator.

## Known gaps

No known M2 or M3 gaps. M4 and later roadmap work remains planned. Nothing has been pushed.
