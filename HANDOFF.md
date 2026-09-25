# Current handoff

Updated 2026-09-24. Keep this note to the current state; edit facts in place instead of adding a minor-fix history.

## State

ROADMAP v1.2 M1 is implemented: oscillator mode index 1 is now **Physical**, with stiff-string dispersion, pickup and excitation positions, pick hardness and position, and slap. Existing String parameter IDs and mode index remain; missing M1 values in old states load as neutral defaults. M2 onward is still planned. The user builds and installs the plugin; do not run `build-and-install.cmd`.

## Verification

- `cmake --build build --config Release` succeeded for all targets.
- `build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe` passed, including M1 DSP, migration, tuning, extremes and heavy-preset CPU checks (see `build/m1-tests-final.log`, heavy CPU 40.1%). The timing check needs an otherwise idle machine.
- `build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe --uitest` passed. `snapshots/m1/osc-physical.png` was inspected; the OSC page scrolls when Physical needs taller controls.
- Fingerprint comparison: `python tools/compare_fingerprints.py build/m1-before-repeatable.csv build/m1-after-repeatable-final.csv` reported **0 of 235 changed**. Both files used the same fingerprint-only RNG seeds and synchronous spectral rendering; two M1 captures also compared as 0 changed. The original unseeded `build/m1-before.csv` remains for reference but is unsuitable for strict comparison because random and background spectral presets vary between runs.

Claude reviewed M1 and committed polish on top: pick hardness now defaults to 1.0 (the raw burst, i.e. the legacy sound) and softens monotonically below it (previously 0 was raw but 0.01 was softest); old states get each missing M1 parameter's default, not 0; slap scales with velocity; Physical controls show percentages; a Physical OSC card gets 70 px extra height and the page scrolls only when cards cannot fit at their minimum. Fingerprints still 0 of 235 changed (`build/m1-review.csv`), UI tests pass, all M1 tests pass.

Known: the heavy-preset CPU timing test currently fails on this machine for both Codex's M1 commits and the polish (~50-60 % vs a ~40 % idle run), so treat it as load-sensitive, not an M1 regression. The OSC wave display still draws the wavetable in Physical mode (pre-existing from String mode).

No open M1 design questions. The M1 commits and Claude's polish commit are not pushed yet. Use the code and Git diff as the source of truth; `README.md` covers features, and `ROADMAP.md` covers future work.
