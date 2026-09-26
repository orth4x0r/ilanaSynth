# Current handoff

Updated 2026-09-25. The source tree is the source of truth. Work on `main`; Claude and Codex take turns and should not edit concurrently.

## State

M1–M3 were implemented before this task and pushed to `origin/main`. M3b phase A was committed separately as `4a58064`, `f8f32b7`, and `89d99fb`. It changed the three hard-wired voice oscillators, processor parameter fills, and editor controls to indexed data without changing sound. The phase A checkpoint built all targets, passed `ilanaTableTest` and `ilanaSnapshot --uitest`, and changed **0 of 235** seeded factory preset fingerprints.

M3b phase B expands the engine to six full oscillators. OSC 4–6 have Wavetable, Physical, Sample and Granular modes, unison, chord, spectral and time warps, and filter routes; they default off. The existing `osc1_*`, `osc2_*`, and `sub_*` IDs and choice indices are retained. The separate sub oscillator and noise remain separate. The FM matrix is 6×6 with the nine original cells mapped to their existing IDs. Route **Both** is appended after Direct and feeds each filter in parallel.

The envelope pool has 16 TensionAdsr envelopes. The first five keep their IDs (`amp_*`, `fe_*`, `f2e_*`, `me_*`, `e4_*`). ENV 6–16 have attack, decay, sustain, release, curve and velocity parameters and are appended mod sources. Each oscillator can select ENV 1–16 as its amp envelope; default ENV 1 retains the legacy render path. Voice lifetime follows all active oscillator amp envelopes. The ENV page has a patch-saved reveal count and always shows assigned envelopes.

The OSC page has six compact rows and one expanded card; MAIN has six oscillator rows; FM shows six operators and the full matrix. New OSC 4–6 modulation destinations are appended after the existing indices. The global QUALITY control is beside oversampling.

## Verification

- Pre-edit baseline: `build/ilanaFingerprint_artefacts/Release/ilanaFingerprint.exe build/m3b-before.csv` wrote 235 seeded fingerprints.
- Phase A: `cmake --build build --config Release` succeeded; `ilanaTableTest.exe` and `ilanaSnapshot.exe --uitest` passed with 0 failures; `python tools/compare_fingerprints.py build/m3b-before.csv build/m3b-phase-a-final.csv` reported **0 of 235 changed**.
- Phase B: `cmake --build build --config Release` succeeded for all targets, including VST3 and standalone. `python tools/compare_fingerprints.py build/m3b-before.csv build/m3b-phase-b-verified.csv` reported **0 of 235 changed**.
- `ilanaSnapshot.exe --uitest` passed with 0 failures. `ilanaSnapshot.exe build/m3b-phase-b-snapshots` captured default MAIN, OSC, FM, ENV/LFO, revealed ENV and full ENV pool. The snapshots were inspected: six rows and the full pool fit, and no labels or controls are clipped.
- The targeted `ILANA_M3B_TEST=1` run passed: all four modes on OSC 4–6, off-oscillator sample equality, Both route, all 36 FM cells, ENV 6 as a mod source, long amp release and eventual voice cleanup, old-state migration, patch-saved ENV reveal, Normal sample equality, High wavetable smoothing, and six-oscillator CPU. Six active oscillators with six unison voices per oscillator and a six-note chord measured **225.9 ms/s Normal** and **172.0 ms/s Eco**, below the 400 ms/s heavy-preset budget in the final full run.
- The final full `ilanaTableTest.exe` passed with **0 failures** (`build/m3b-tabletest-verified-final.log`). The heavy factory preset CPU cases passed the same 400 ms/s budget. One prior run had a variable Grain Choir tuning estimate; the final run passed.

## Decisions to review

- Route Both uses separate auxiliary filter states for its two parallel branches. This preserves the exact old serial and parallel filter path when no source selects Both. Both branch outputs are summed at 0.7071 gain each.
- Eco caps each oscillator at four unison voices; Normal uses the original limits and rendering; High averages two half-step wavetable reads for smoother high-note output. Physical, Sample and Granular processing are unchanged in High.
- When all active oscillators select ENV 1, the old post-filter AMP multiplication is preserved exactly. Selecting another amp envelope applies each oscillator envelope before the filter buses; this lets different oscillators have independent level shapes, while the dedicated sub/noise continue using AMP.
- New physical string banks have explicit seed values so their construction cannot advance the default seed sequence used by the first three oscillators. This is required for identical old preset fingerprints.
- ENV 1–5 start revealed for familiarity; subsequent cards are revealed with `+`. The count lives in the patch state property `envRevealCount` and defaults to five when absent.
- Existing modulation destination indices stay fixed. The 30 new OSC 4–6 targets occupy appended explicit destination indices after the pre-existing parameter destination segment; the total stays under 256.

## Known gaps

No known M3b sound or UI gaps. The factory tuning test has occasionally reported Grain Choir around 39 cents out on one run; it passed in the final run with the same audio implementation. Nothing from M3b has been pushed. The plugin was built locally but not installed.
