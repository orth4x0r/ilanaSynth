# Review plan: state and what's next

Updated 2026-09-30, at the move from the Windows PC to cloud sessions.
Review: claude.ai/artifact/LAbrHT1KnLVEiv8Yd3SjHS. Projected "potential" review
(what the synth should be once this plan ships): claude.ai/artifact/XecerytfKfBKdepvZLxKZt.

## The user's decisions (standing)
- Personal use only. Commercial work (licensing, store, installer polish) is parked and forgotten.
- Downloads of samples, wavetables, reference presets: allowed.
- Presets are made by automated pipelines only (no hired people). The user A/B-tests by ear.
- Positioning: "physical hybrid" synth.
- Old presets may change in sound, but must never break level-wise (no silent, clipping or wildly quieter presets).
- Filters: replace the old ones, improve them massively, make warps and the other spectral stuff modulatable.
  The user also suggests reusing Airwindows filters where they fit (there are good ones: e.g. Air, Baxandall,
  Capacitor, Isolator, Pop, Coils, and the uncoded Airwindows filter set).
- Everything from the review except scripting, plus SoundFont (SF2/SFZ) support.
- AI agents maintain the synth for years: keep docs short, current and machine-readable.
- 32 voices, 8 macros. Keep Grand Piano but rename it (it's "not a piano").
- The user has no host automation in sets (choice params made non-automatable).
- At most 2 agents in parallel (weekly usage).

## Done (all on main)
| Commit | What |
|---|---|
| addda8c | FX DRY delayed by the oversampling latency |
| f1fc370 | Live grains and string seeds repeatable; the input rings the Classic body; Init opens the filter; Filtered FM feedback by default; `ilanaPresetRender`, `tools/preset_critic.py`, `ILANA_PLAN_TEST` |
| f71b1e9 | 460 choice params non-automatable |
| 1885678 | Useless tests purged, loose ones tightened |
| b8200b4 | CPU: SSE2 Sync warp, one tanh for mono sources, 32 voices, DC blockers, `src/PresetTrims.h` |
| a9006ba, 3fb7c0d | Scala microtuning; a loaded scale survives factory presets |
| 39a2f21 | 8 macros; MTS-ESP client; level + macro trims for 289 presets; A/B clip tool; `tools/build_content.py` |
| 1cc7c25 | Airwindows FX (39 algorithms); Lead glide divided by 3 at load; `tools/preset_diversity.py` |
| c911267 | Filter overhaul groundwork (see below) |

CPU: the heavy benchmark went from 42-84 % to 19-25 %. Library loudness spread (10-90 %) 17.5 dB to 8 dB.

## Gate state at the move (not clean)
The last Windows run: FX tests and `--uitest` pass; 9 of 396 fingerprints moved (lead glide, expected).
Two table-test failures, both believed fixed or environmental but NOT re-verified:
- Vocal Chop -14.4 dB under the median (limit 14): its trim is now 0 dB (`LEVEL_OVERRIDES` in `tools/tune_presets.py`).
- The 64-slot matrix CPU test (499.9 ms/s): a parallel build was loading the machine.
First job in the cloud: build on Linux, run the gate, make a Linux fingerprint baseline.

## Content in the repo
- `content/samples/`: 365 CC0 samples from VCSL (0.76 GB), by category, 48 kHz 24-bit. The synth looks in
  `~/Documents/ilanaSynth Samples`: copy or symlink `content/samples` there.
- `content/wavetables/`: 147 AKWF tables (CC0). Link to `~/Documents/ilanaSynth Wavetables`.
- `tools/build_content.py` rebuilds both from their sources.
- `content/analysis/`: the critic's `report.csv`/`index.csv`, CLAP embeddings (`clap_embeddings.npy`)
  and reports for our library (`critic-v4`) and the references (`critic-refs` = 57 Vital presets,
  `critic-refs-surge` = 200 Surge XT patches). The WAV renders (5 GB) are not committed: re-render ours with
  `ilanaPresetRender`; `preset_diversity.py` works from the committed embeddings alone for the references.
  To re-render references, install Vital / Surge XT on Linux and use `ilanaRefHost --presets`.
- `content/analysis/ab-page/`: the A/B page's clips and manifest (artifact claude.ai/artifact/ESuBEENUzTpvuemWi3rXTM,
  votes in its db `votes` collection; 54+ votes so far).
- CLAP scoring: `pip install torch transformers`, model `laion/clap-htsat-unfused` (downloads itself).

## Next, in order
1. **Gate on Linux** (above). Fix what fails.
2. **Re-render the Lead A/B clips** (`tools/ab_clips.py`) with the reduced glide; republish the A/B page.
3. **Preset diversity (the user's top complaint after levels):** presets sound alike. Measured spread against
   the references: Bass 57 %, Pad 53 %, Lead 66 %, Keys 76 %, Pluck 68 %; 50 of 57 pads form one crowd.
   Causes: reverb on nearly everything (Hall 111x), FX chains nearly all Reverb / Delay+Reverb / Chorus+Reverb,
   macro 4 = SPACE/HALL/ROOM/ECHO on ~240 presets, 1 ms amp attack almost everywhere.
   Redesign each category (Pads first) around distinct archetypes; vary FX (dry, Airwindows, distortion,
   no reverb), macros, attack, register and motion; merge duplicates; fill the gaps listed in
   `content/analysis/critic-v4/diversity.md`; use the sample library and the physical engine.
   Target: spread at least ~80 % of the references', no crowd bigger than ~4, levels at the category target.
   Loop: `ilanaPresetRender` -> `preset_critic.py` -> `clap_score.py` -> `preset_diversity.py`, plus the user's A/B votes.
4. **Filter overhaul:** groundwork on main, status and design in `docs/filter-overhaul-status.md`
   (baselines in `docs/filter-overhaul/`, cores in `src/dsp/FilterCore.h`, tests `ILANA_FILTER_TEST=1`).
   Remaining: rebuild the 25 models, add 303 Acid, Moog Drive, Vowel Morph, Comb Body; modulatable spectral warps;
   levels within +-1 dB. Make a Linux level baseline from c911267 before changing any model.
   Check Airwindows' filters for pieces worth reusing.
5. **Weak macros:** ~80 presets whose macros barely do anything (`tools/tune_presets.py` lists them): fix by hand.
6. **Interface:** a live patch view instead of PLAY's empty ADD OSCILLATOR box; the VECTOR pad fills its card
   and the evolve rows fit 8; a PHYSICAL page preview when no oscillator is physical; verify mod rings; identity pass.
7. **Features:** SoundFont (SF2/SFZ) in the Sample oscillator; FX splitters (multiband/LR/MS); vocoder; clip sequencer.
8. **Maintenance for agents:** rename Grand Piano; split PluginEditor.cpp and PluginProcessor.cpp;
   an architecture map and decision log; trim slow tests (M81 coverage, arp release); README typo "buto" -> "bruto".
9. Lowest: a self-hosted CI runner; a GPU renderer for macOS and Linux.
