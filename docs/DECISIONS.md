# Decision log

One row per decision. Sources: RP = [REVIEW-PLAN.md](REVIEW-PLAN.md) "The user's decisions"; HO = [HANDOFF.md](../HANDOFF.md); RM = [ROADMAP.md](../ROADMAP.md) ("Decisions made", "When a milestone is done"); HH = [HANDOFF-HISTORY.md](HANDOFF-HISTORY.md). To add one, append a row; mark replaced rows "superseded" rather than deleting them.

## Product
| Date | Decision | Source |
|---|---|---|
| 2026-09-30 | Personal use only; commercial work (licensing, store, installer polish) is parked | RP |
| 2026-09-30 | Positioning: "physical hybrid" synth | RP |
| 2026-09-30 | Downloads of samples, wavetables and reference presets are allowed | RP |
| 2026-09-30 | Presets come from automated pipelines only; the user A/B-tests by ear | RP |
| 2026-09-30 | Old presets may change in sound but must never break level-wise (no silent, clipping or much quieter ones) | RP |
| 2026-09-30 | Filters: replace the old ones, improve massively, make warps and spectral parameters modulatable; reuse Airwindows filters where they fit | RP |
| 2026-09-30 | Scope: everything from the review except scripting, plus SoundFont (SF2/SFZ) | RP |
| 2026-09-30 | 32 voices, 8 macros | RP |
| 2026-09-30 | Keep Grand Piano but rename it ("not a piano") | RP |
| 2026-09-30 | Choice parameters are non-automatable (the user has no host automation in sets; 460 params) | RP |
| 2026-09-30 | At most 2 agents in parallel (weekly usage); work moves to cloud sessions | RP |
| 2026-09-29 | UI style: "calm, with some glow and flashy animation", font Manrope; seven tabs; one oscillator palette (OSC 1 gold) | HO |
| 2026-09-27 | Audio input via two plugins (instrument and FX build) | RM |
| 2026-09-26 | Physical engine used twice: an oscillator mode and the BODY section | RM |
| 2026-09-26 | 6 full oscillators that are also the FM operators; pools of 16 envelopes and 16 LFOs (fixed counts because hosts need a fixed parameter list) | RM |
| 2026-09-26 | Sympathetic strings follow the GENERATE scale (manual override); Evolve is per macro; feedback guitar is an exciter type | RM |
| 2026-09-26 | Edited wavetables are stored in the patch (recipe, embedded data, file reference), 16 patch-table slots | RM |
| 2026-09-26 | Windows first; macOS, Linux and CI waited for M9 | RM |

## Engineering rules
| Date | Decision | Source |
|---|---|---|
| standing | Old presets must not change: run `ilanaFingerprint` before and after and compare with `tools/compare_fingerprints.py`; intended changes are explained in ROADMAP. Compare only against a baseline from the same platform (Linux: `tests/fingerprints-linux.csv`) | HO |
| standing | Parameter IDs, choice indices, mod destinations and sources are never renumbered; new ones are appended | HO |
| standing | New features get their own random generators and explicit seeds so old random sequences do not shift | HO |
| standing | New features default to old behaviour and skip their code while unused | HO |
| standing | Per-block parameter reads use a `ParamRef` member, not a string built per block | HO |
| standing | Superseded models stay as "classic" for old patches (M2 LFOs, M4 hammer, Classic body) | RM |
| 2026-09-26 | A sound is done only when fitted to a reference and its listening round has passed; an unreached target gets a written gap | RM |
| 2026-09-26 | References: internet sources without login first; installed plugins only for their init sound; the user should not have to do anything | RM |
| standing | Each milestone ends with all targets building, `ilanaTableTest` and `ilanaSnapshot --uitest` at 0 failures, the fingerprint check, a commit and an `mN-done` tag | HO |
| 2026-09-28 | GitHub Actions is manual-only (no minutes left): run `tools/verify.sh` before pushing | HO |
| 2026-09-26 | Route Both keeps separate aux filter state so old serial/parallel paths stay exact; Eco caps unison at 4 voices | HH "Decisions to review" |
| 2026-09-26 | When all oscillators use ENV 1, the old post-filter AMP multiply is preserved exactly | HH "Decisions to review" |
| 2026-09-26 | New physical string banks have explicit seeds so old fingerprints hold | HH "Decisions to review" |
