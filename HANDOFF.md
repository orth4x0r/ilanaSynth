# Current handoff

Updated 2026-09-24. Keep this note to the current state; edit facts in place instead of adding a minor-fix history.

## State

ROADMAP v1.2 M1 is implemented: oscillator mode index 1 is now **Physical**, with stiff-string dispersion, pickup and excitation positions, pick hardness and position, and slap. Existing String parameter IDs and mode index remain; missing M1 values in old states load as neutral defaults. M2 onward is still planned. The user builds and installs the plugin; do not run `build-and-install.cmd`.

## Verification

- `cmake --build build --config Release` succeeded for all targets.
- `build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe` passed, including M1 DSP, migration, tuning, extremes and heavy-preset CPU checks (see `build/m1-tests-final.log`, heavy CPU 40.1%). The timing check needs an otherwise idle machine.
- `build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe --uitest` passed. `snapshots/m1/osc-physical.png` was inspected; the OSC page scrolls when Physical needs taller controls.
- Fingerprint comparison: `python tools/compare_fingerprints.py build/m1-before-repeatable.csv build/m1-after-repeatable-final.csv` reported **0 of 235 changed**. Both files used the same fingerprint-only RNG seeds and synchronous spectral rendering; two M1 captures also compared as 0 changed. The original unseeded `build/m1-before.csv` remains for reference but is unsuitable for strict comparison because random and background spectral presets vary between runs.

No open M1 design questions. Local commits for M1 have not been pushed. Use the code and Git diff as the source of truth; `README.md` covers features, and `ROADMAP.md` covers future work.
