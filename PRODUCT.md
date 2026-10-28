# Product

<!-- impeccable:product-schema 1 -->

Read by the Impeccable design skill (`.claude/skills/impeccable`). Written 2026-10-28 from ilana's recorded decisions (docs/REVIEW-PLAN.md, docs/UI-CONVENTIONS.md, HANDOFF.md), not from a fresh interview: anything marked *(inferred)* still needs her ok.

## Platform

desktop

Impeccable only knows web, ios, android and adaptive, so it warns and falls back to web; ignore that. ilanaSynth is a JUCE C++ audio plugin (VST3, CLAP, AU, standalone), drawn entirely by `juce::LookAndFeel` and custom `paint()` code. There is no HTML, CSS or DOM: skip `impeccable detect`, the live browser mode and web-only checks (ARIA, breakpoints, touch). Treat audits like the native path (design review only) and check the UI with `ilanaSnapshot` PNGs at `ILANA_SNAPSHOT_SCALE=1.75` (HANDOFF.md, Verify).

## Users

One person: ilana, a producer in Argentina, making music in Ableton Live 12 on Windows. She designs sounds and plays them in a session; the plugin window sits beside the DAW at **175 % host scale**. Personal use only; there are no other users, no store and no distribution.

## Product Purpose

A complete sound design machine on one page: a "physical hybrid" synth (strings, bows, hammers, electric pianos and resonant bodies wired into wavetable, FM / DX7, granular and sample engines, two routable filters and a 41-type FX rack). Success is that she reaches the sound she hears in her head quickly, by ear, without hunting for controls.

## Positioning

Physical modelling as a first-class oscillator and as a BODY that the whole oscillator mix excites, so hybrids (supersaw-rung sitars, granular-bowed plates) are one click away. Judged against Vital and Serum 2 for workflow and density.

## Operating Context

- In a DAW, mid-session, often while a loop plays: tweaks are made by ear, with the mouse, and judged by the live graphs.
- Her targets: supersaw leads, pads, brass leads, reese, 808s, acid, weird granular and stringed keys and textures (Arca, Crystal Castles, Jane Remover, underscores, trance, dnb).
- Presets come from automated pipelines and blind listening votes; she never hand-tunes hundreds of patches.

## Capabilities and Constraints

- Feature freeze since 2026-10-13: interface work only until RC 1.0, apart from the roadmap items she approved.
- No full rewrite. Parameter IDs, mod destinations and appended choices are fixed (HANDOFF.md, Rules).
- CPU matters: idle repaints and physical-model cost are watched (docs/ROUTING-CPU.md).
- Every UI change: restate the scope in one line and get her ok first; mockups need her "go"; every merge needs her typed "go".
- Vocabulary is fixed by docs/UI-CONVENTIONS.md (RANDOMISE vs RANDOM PRESET, VOICE vs UNISON, Patch vs preset...).

## Brand Commitments

- Name **ilanaSynth**, tagline "El Sinte de Big Ilana Da GOAT".
- The approved PLAY page (mockup fit8) and the review-23 card language are the visual authority: polish them, never undo them.

## Evidence on Hand

- Approved pictures and reviews: docs/ui-review/, docs/img/play.png, the review 23 to 26 artifacts linked in HANDOFF.md.
- Real presets in `content/`, her own presets in her Documents folder (never touched).
- No testimonials, users or benchmarks exist; never invent them.

## Product Principles

1. Every control is easy to hear: keep or reduce visible controls; a knob that changes little is a bug.
2. One page, no hunting: scroll instead of folding cards; same card sizes everywhere.
3. Purpose built: one grid, shared edges and baselines, no dead space, Vital-like density.
4. Big readable text at her real scale; nothing smaller than the approved mockup.
5. Colour only where it means something (identity, state, the active value).

## Accessibility & Inclusion

ilana has ADHD: unfinished things should be visible and short to resume, and the interface should not ask her to remember state across pages *(inferred: from project notes, not a stated UI requirement)*. Text never drops below the passive floor in `IlanaTheme::TextSize` and never gets condensed sideways.
