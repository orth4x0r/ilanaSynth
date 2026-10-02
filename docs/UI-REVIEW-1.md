# UI/UX review 1: ilanaSynth against Vital and Serum 2

2026-09-30. Basis: the three latest screenshots (PLAY overview, VECTOR, PHYSICAL, Init preset, v1.3 UI), `src/gui/`, HANDOFF's UI notes. Not run live: no Vital or Serum 2 install here, so the competitor side is from knowledge of their current UIs. Findings only; no source was edited (the roadmap thread is touching `PluginEditor.cpp`). Decisions in REVIEW-PLAN are not reopened.

## Where we already match or beat them
- Consistent knob language, per-source mod colours, drag-a-chip-onto-a-knob modulation, right-click quick modulation, undo/redo and A/B in the header, UI zoom 75-200 %, preset browser with search, tags and favourites, hover help on every parameter. Idle CPU is low (one vblank clock).
- Unique: physical/PHYSICAL page, VECTOR pad with path and Evolve, PATCH routing card. Serum 2 and Vital have nothing like the animated string.

## Gaps, ranked by how much a Vital/Serum user would feel them
1. **PLAY is an inventory, not a live view (high).** Vital's main page is the sound: three oscillator scopes, the filter response and the mod sources all visibly moving. Ours, with Init, is one lit oscillator, two "OFF" rows, a PATCH diagram and an empty SUB + NOISE card (dimmed knobs still take a third of the left column). Recommend: collapse SUB + NOISE to one line like the off oscillators; show a combined live output scope or spectrum in the space; let PATCH double as the routing editor. (Already REVIEW-PLAN step 6.)
2. **Mod visibility on the knobs (high).** Vital draws a live ring showing each knob's modulated position and a value bar per assigned source; Serum 2 does the same and lists sources in the tooltip. Ours has grey chips and glowing dots, but the ring only shows on some knobs. Recommend: a modulation ring plus live modulated-value marker on every knob, and a click on a chip highlighting all knobs it drives (Vital does this).
3. **Direct manipulation of displays (medium-high).** In Vital the filter response, envelope and LFO shapes are edited by dragging on the graphs, and wavetable frames are edited in a full-window editor with a spectral view. Ours has the filter display and MSEG points (envelope has points), but the oscillator display is "3D" view only and the filter graph drags less than Vital's. Recommend: filter-graph drag for cutoff/reso, oscillator display drag for frame/warp, and a spectrum view toggle beside 3D.
4. **Preset browser (medium).** Serum 2 and Vital show a persistent side browser with author, style tags, preview/audition and previous/next by category. Ours is a popup with search/tags/favourites. Recommend: audition on hover or arrow keys, filter chips for the category, and show the preset's macro names in the list (helps with the "macros barely do anything" complaint). With the preset-diversity work landing, this matters more.
5. **Header density (medium).** Undo, redo, history, A/B, dice, settings, star, save and the "..." menu are all top-level icons with similar weight. Vital keeps a quiet header. Recommend: group the actions (file, edit, tools) and give save and the preset name more weight.
6. **Empty and dimmed states (medium).** PHYSICAL with no string shows a dead string plus a dead body box and a paragraph, and VECTOR leaves space above and below the pad (known). Recommend: show the PHYSICAL preview only when an oscillator is physical, otherwise a single card with "switch to physical" and a small demo; scale the VECTOR pad to the card.
7. **Bottom bar (low-medium).** 16 source chips, 4 macros (5-8 behind a button), glide, legato, bend, voice mode, voices, master and the tips line all share one strip. Serum 2 puts macros in a smaller side area. Recommend: move VOICES/BEND/VOICE MODE into the settings or a MAIN voice card, keep macros and master; that also frees width for macro names (they read MACRO 1 even when a preset names them).
8. **Text and labels (low).** Small caps labels are ~9 px at 100 % zoom in the knob grid; check contrast on dimmed controls (accessibility). Serum 2's bigger type reads better at low zoom.
9. **Discovery (low).** Vital's first-run and Serum 2's tooltips carry usage. We have a tour and a status-bar line; keep them, they are competitive.

## Suggested order
Live PLAY view (1) and mod rings (2) first (users see them in the first minute), then graph drag (3), then browser audition (4), then the rest. Items 1, 6 and 7 are already partly in REVIEW-PLAN step 6.

## Next review
After the filter overhaul UI lands (the new type list and any spectral-warp knobs need a look against Vital's filter and Serum 2's filter pages), and after the preset redesign, so the browser gets judged with real content. Do it live in the built plugin then.
