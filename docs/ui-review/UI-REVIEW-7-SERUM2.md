# UI review 7: ilanaSynth v1.3 (after the review 6 fixes) against Serum 2

Reviewer: a fresh, adversarial pass on branch `claude/project-thread-smvgfk`. The benchmark is Serum 2's UI, compared from what I
know of it (I did not run Serum 2). This review gives extra weight to workflow speed: how many clicks the common jobs take.
Inputs: the shots in `/home/user/shots7/` (1060x720 logical at 1.5x; `small/` and `dx7-small/` are 75 %), new renders in
`/home/user/shots7/serum-extra/` (`p1/` = P1 modulation set on Neuro Wobble, `fm-init/` = FM set on Init, `fm-brass/` = FM set on
the DX7 voice "BRASS 1 (ROM1A)", plus two crops), and the source in `src/gui/` and `src/PluginEditor.cpp`. Shot paths are
relative to `shots7/`. **(inferred)** marks claims I could not confirm in a shot or in the code.

Artefacts I did not count: EDITED after 03-OSC; "ENV +13/+14" and the 16 revealed envelopes; `sym_on` is switched on by the
snapshot tool on the OSC page (`tools/Snapshot.cpp:5998`), so the STRINGS block on Neuro/DX7 FILTER shots is a tool state. The
layout bug it exposes (finding 1) is real, though: Felt Hammer Board shows it with its own soundboard.

## Verdict

This is a much better build. Most of review 6's big items landed properly: a real piano roll, depth rings on knobs, all 32 DX7
algorithms with their numbers, an Operator Env with ms and dB, a tagged browser that files DX7 voices by sound, one FX chain
view, all 8 macros, a floating scope, a 0 dB master. The common jobs are now about as fast as in Serum 2.

What is left is mostly **half-merged systems and layout leftovers**. The DX7 operator is still described in two vocabularies
(PLAY's LEVEL 50 % is FM's TRIM 0 dB, and the envelope has five names). The DX7 pitch EG and LFO still live on the FM page with
their own words, and they stay editable on patches where they do nothing. There are three separate drawn-shape systems: a
drawable LFO, a Steps shape and a single MSEG module. The new fixed oscillator slots leave 40 % of PLAY's left column empty on
most patches. The SIGNAL FLOW diagram loses both filters whenever strings or a soundboard are on, which includes the flagship piano.
On the content side, every one of the 288 DX7 voices opens with the same four macros, and SPACE is flagged "!" as dead.

**Score: 7.0 / 10 against Serum 2** (review 6: 5.5).

---

## Workflow speed: the four common jobs

| Job | ilanaSynth now | Serum 2 | Verdict |
|---|---|---|---|
| Route an LFO to cutoff | Drag the LFO 1 chip onto CUTOFF (1 drag, lands at 35 %), then drag the ring for depth (1 drag). On a DX7 voice, LFO 2/3 sit behind "LFO +2 ▾": you hover or click first (+1) | Drag the LFO's handle onto the knob, then drag for depth | **Par**, except on DX7 voices (finding 8). The rings are thin targets (finding 33) |
| Browse and audition presets | Click the name, then Up/Down auditions, Enter keeps, Esc reverts. Or use the < > arrows. Tag chips and categories are one click each | Arrows beside the name, then the browser with filters | **Par**. The 288 DX7 rows with duplicates slow the scroll (finding 10). Docked and floating browsers use different keys (finding 31) |
| Add and reorder FX | + ADD EFFECT, then the type (2 clicks). Drag a card's header to reorder (1 drag) | Click an empty slot, pick, drag to reorder | **Par**. A second instance of a type is still refused (settled) |
| Draw a clip | SEQ, then the CLIP pill, then **double-click** each note and drag its edge for length. A drag on empty space draws a selection box, not a note | Draw notes directly, drag to set length **(inferred: from memory of Serum 2's clip editor)** | **Slower**: two clicks per note, and no way to paint a run (finding 18) |

---

## Fixed since review 6 / still open (my area: S6-1 … S6-46)

| S6 | Finding | Now |
|---|---|---|
| 1 | Op EG is its own envelope system | **Mostly fixed.** "Operator Env" uses ms and dB, the graph can be dragged, AMP ENV says "unused (Op Env)", and the MOD page shows an OP ENV pill. Still open: it is not a mod source, it is edited only on FM, and it has five names (#5) |
| 2 | FM diagram collapses | **Fixed** for E.Piano 1. A 6-operator stack (DX7 algorithm 1) is cramped, and the glows and captions overlap (#13) |
| 3 | PLAY/OSC hide the FM | **Partly fixed.** CARRIER / MOD › 1 / OP ENV captions and RATIO are shown. Still shown on operators: FRAME, WARP, SPECTRAL, UNISON, 3D view (#12) |
| 4 | DX7 own category, shouty names | **Mostly fixed.** Voices are filed by sound ("KEYS · DX7 ROM1A"), names are in Title Case, and there is a DX7 288 chip. Still open: identical macros (#9) and duplicate voices (#10) |
| 5 | Two LEVELs, two envelope names | **Partly fixed.** FM now says LEVEL (output) + TRIM, but PLAY/OSC still call the trim "LEVEL 50 %" (#4). The envelope names multiplied (#5) |
| 6 | DX7 PITCH/LFO hidden second LFO | **Partly fixed.** OP LFO / OP PITCH are chips and MOD pills. Still edited on FM with its own vocabulary, and live on non-DX7 patches (#6) |
| 7 | Only 7 of 32 algorithms | **Fixed** (DX7 1-16 / 17-32, numbered) |
| 8 | Folded card leaks labels | **Fixed** (no fold-to-fit) |
| 9 | FX one per type | **Open by decision**. The duplicate card's wording is now plain, with REMOVE |
| 10 | Clip editor strip | **Fixed** (key column, ruler, selection, velocity lane, EXPAND). New problems: #17, #18 |
| 11 | 10 px depth badges | **Fixed** (rings). The rings are thin targets (#33) |
| 12 | Chaos LFO overlap | **Fixed** (`neuro/lfo-sim-lorenz.png`) |
| 13 | Two physical editors | **Fixed** (PHYSICAL shows "the controls of its OSC card") |
| 14 | BODY in two places | **Open**, now three places (#22) |
| 15 | Macro into an off effect | **Fixed in the UI** ("!" and a macro card). **Open in content**: the factory presets ship with it (#9) |
| 16 | SIGNAL FLOW wires modulators to filter | **Fixed** ("OSC 2 FM" dashed). New bug: F1/F2 vanish (#1) |
| 17 | FM matrix wall | **Fixed** (dots, FB glyph) |
| 18 | Tutorial "DX7 MODE", import buried | **Fixed** (IMPORT .SYX in the browser). New: a wrong hint, old name (#30, #5) |
| 19 | Chip codes | **Fixed** (no codes). New: the folding order is wrong (#8) |
| 20 | Filter grids | **Fixed** (dropdown + arrows) |
| 21 | Response markers | **Mostly fixed** (MOD label). The label collides on PLAY (#11) |
| 22 | STEPS | **Partly fixed.** Steps is now an LFO shape, but the MSEG is still a separate module (#7). No step count (on purpose) |
| 23 | ARP fake staircase | **Fixed** (editable lanes). The lanes have no readouts (#19) |
| 24 | GENERATE layout | **Fixed** (three boxed sub-cards) |
| 25 | FX chain twice, two wet controls, `S` | **Partly fixed.** One chain view and SOLO are done. BLEND + MIX and two "+ ADD EFFECT" remain (#15, #16) |
| 26 | FX cards waste width | **Mostly fixed** (two columns). The widths are inconsistent (#16) |
| 27 | AW names | **Fixed** |
| 28 | No factory tags | **Fixed** |
| 29 | Duplicate routes, hidden sort | **Fixed** |
| 30 | Matrix naming | **Fixed** in the matrix. The status line still says "Osc1 Frame" (#20) |
| 31 | Dock widens window | **Fixed** |
| 32 | REED PICKUP collision | **Fixed** (not seen in any shot) |
| 33 | Two ways to add an oscillator | **Partly fixed.** It is still two buttons, and PLAY now has empty slots (#2) |
| 34 | 1-16 rulers, stray scroll bar | **Mostly fixed.** A many-envelope pool still scrolls sideways under the cards (#40) |
| 35 | Remove LFO/env hidden | **Not verified** (no hover shot) |
| 36 | EVOLVE zeros, VEC X/Y chips | **Partly fixed.** Rows plus "+ MACRO" are done, but the VEC chips sit in the card, not the bar (#21) |
| 37 | PHYSICAL dead end | **Fixed** (greyed tab) |
| 38 | 5-8 macro toggle | **Fixed** |
| 39 | Status line always on | **Fixed**, but it now covers the chip bar (#20) |
| 40 | Header readouts | **Fixed** ("1/32", A B) |
| 41 | Scope covers PLAY | **Fixed** (floating panel) |
| 42 | Thin Save As | **Fixed** (author, tag chips, comment, clear wording) |
| 43 | Master levels differ | **Fixed** (MASTER 0.0 dB everywhere) |
| 44 | Op EG caption | **Fixed** ("C3, velocity 100", time axis) |
| 45 | Algorithm tiles that add oscillators | **Fixed** ("+1" corner tags) |
| 46 | 75 % text | **Fixed** (9 px floor) |

Tally: 35 fixed or mostly fixed, 8 partly fixed, 2 open (S6-9 by decision; S6-14 grew to three places), 1 not verified.

---

## Findings, worst first

### 1. High (layout bug, cause found): SIGNAL FLOW loses both filters when strings or a soundboard are on
- **What's wrong:** On Felt Hammer Board (a soundboard patch) and on any patch with sympathetic strings, F1 and F2 are not drawn. All the oscillator wires meet in a thin purple stroke before WEST. The diagram that should explain the filter routing shows no filters. The orange SERIAL badge also sits on OSC 1's wire.
- **Cause:** `SignalFlow::computeLayout` (`src/gui/FilterWidgets.h:574-646`) reserves the blocks after the filters first: OUT, the post block (×1.45), BODY, WEST and their gaps. The filters get what is left, `filterWidth = jmin (blockWidth*1.15, area.getWidth()*0.42)`. At this card's width (about 302 logical px), the right-hand blocks need about 203 px, but only about 197 px is left after the oscillator column and the 34 px fan-in. The area goes negative and the filters get zero width.
- **Serum 2:** Its routing view never drops a stage.
- **Fix:** Lay out the filters first with a minimum width (about 40 px each), then shrink the after-blocks, or wrap the post block onto a second row. Abbreviate "STR+BRD" before shrinking anything. Add a `--uitest` check that F1 and F2 are at least 30 px wide with `sb_on` and `sym_on` set. Move the SERIAL badge between F1 and F2 so it doesn't cross a wire.
- **Evidence:** `keys/05-FILTER.png`, `serum-extra/crop-flow.png` (from `neuro/05-FILTER.png`).

### 2. High: PLAY's fixed oscillator slots leave 40 % of the column empty on most patches
- **What's wrong:** Every 1-3 oscillator patch (most of the library) shows "+ ADD OSC 4" in a dashed box, then "OSC 5 not added" and "OSC 6 not added" as two more empty dashed boxes. That is about 330 of 790 px of the left column, on the main page, on almost every preset. The fixed slots replaced fold-to-fit (good: nothing moves now), but they overcorrected.
- **Serum 2:** Its main page fills the space with the oscillators that exist, with large displays.
- **Fix:** One slim "+ ADD OSC 4" row (about 36 px) after the last oscillator. Give the space back by making the existing rows taller, with a bigger wave display, or show a mini SIGNAL FLOW / FM tile there. Slots after the next addable one show nothing.
- **Evidence:** `neuro/01-MAIN.png`, `keys/01-MAIN.png`, `init/01-MAIN.png`; `src/gui/pages/MainPage.h:881` ("+ ADD OSC n"), `:959` ("not added").

### 3. High: PLAY's oscillator knobs change order with the mode and the FM role
- **What's wrong:** The four knobs in an oscillator row come from different lists. A plain wavetable has `FRAME LEVEL SEMI UNISON` (`MainPage.h:69`). An FM carrier or modulator has `SEMI LEVEL FINE FRAME` (`:78`). Physical has `DECAY DAMP LEVEL SEMI` (`:70`). On Neuro Wobble, OSC 3 (off) shows FRAME / LEVEL / SEMI / UNISON directly under OSC 1-2's SEMI / LEVEL / FINE / FRAME, so FRAME jumps from the last column to the first. Routing FM into an oscillator reorders its knobs, and LEVEL moves between columns 2 and 3 across modes.
- **Serum 2:** Every oscillator panel has the same layout.
- **Fix:** Fixed columns: 1 = pitch (SEMI / RATIO / FIXED), 2 = LEVEL, 3 = the mode's main knob (FRAME / DECAY / START / POSITION), 4 = the mode's second knob (UNISON / DAMP / END / SIZE). FINE goes to the OSC page.
- **Evidence:** `neuro/01-MAIN.png` (rows 1-3), `keys/01-MAIN.png`, `init/01-MAIN.png`.

### 4. High: "LEVEL" on a DX7 operator means two different parameters on two pages
- **What's wrong:** On PLAY and OSC, an operator's LEVEL reads 50 % (`oscN_level`). On FM, the same row's LEVEL reads 0.0 dB, but that is the Operator Env's output level (`eg_out`), and `oscN_level` is relabelled TRIM and shown in dB, with 0 dB at 50 % (`src/gui/pages/FmInputPages.h:698-713, 765-775`). A user who turns PLAY's LEVEL to 100 % gets +6 dB on FM's TRIM, which they never saw. The patch's real operator levels (−30.9 dB, −7.5 dB …) appear only in the FM diagram's captions.
- **Serum 2:** One level per oscillator, the same on every view.
- **Fix:** Use the FM page's story everywhere. On PLAY/OSC, an Operator Env oscillator's LEVEL knob shows the output level (`eg_out`, dB), and `oscN_level` becomes TRIM on the OSC page only.
- **Evidence:** `dx7/01-MAIN.png` (LEVEL 50 %) vs `dx7/08-FM.png` (LEVEL 0.0 dB) vs `dx7/03-OSC.png` (LEVEL 50 %).

### 5. High: the operator envelope has five names
- **What's wrong:**
  - "Operator Env": the ENVELOPE combo, FM header, tooltips.
  - "Op EG": PLAY's envelope card, "Not used: the OSCs play their Op EG" (`MainPage.h:818`).
  - "Operator EG": the OSC header "plays its Operator EG (FM page)" (`OscPage.h:110`), the EDIT OP ENV tooltips (`MainPage.h:193`, `OscPage.h:353`), the tutorial "(ENVELOPE: Operator EG)" and its chip "DX7 BANKS + OPERATOR EG" (`TutorialOverlay.h:30`), and the DX7 preset descriptions (`processor/Presets.cpp:246`).
  - "OP ENV": buttons, PLAY row captions, the MOD pill.
  - "unused (Op Env)": the AMP ENV pool card.
  Its pitch envelope is "OP PITCH" on the chip and "Op Pitch Env" in tooltips (`OperatorPoolCards.h:27`).
- **Serum 2:** ENV 1-4, one name each, everywhere.
- **Fix:** One display name, "Op Env" (long form "Operator Env"), from one table (`ModNames.h` already does this for sources). Grep and replace "EG" in all user-facing strings. "Op Pitch" everywhere.
- **Evidence:** `dx7/01-MAIN.png`, `dx7/03-OSC.png`, `dx7/08-FM.png`, `neuro/00-tutorial.png`.

### 6. High: the DX7 pitch EG and LFO are still a parallel system, and they stay live on patches where they do nothing
- **What's wrong:** FM > PITCH & LFO has its own LFO (SHAPE, RETRIG, RATE, DELAY, PITCH DEPTH, PITCH SENS, AMP DEPTH, SCALE SHIFT) and a pitch envelope (PITCH 1 / PITCH 2 …). It is reached through a pill in the same row as operators 1-6. The MOD page's OP LFO / OP PITCH pills only jump to FM. On Init (no oscillator on the Operator Env) the panel is fully lit and editable (RETRIG on, RATE 5.6 Hz). It does nothing there: `Voice::getOpLfoValue` returns 0 unless a DX7 voice plays (`src/dsp/Voice.h:458`), and the pill is only drawn "quiet" (`FmInputPages.h:746`). The caption "SCALE SHIFT moves only the keys the operators' scaling follows…" is DX7 lore.
- **Serum 2:** LFOs are LFOs: one editor, one set of words.
- **Fix:** Edit the Op LFO in the MOD page's LFO editor (as a card in the LFO row, with RATE / DELAY / SHAPE in the same places as LFO 1). Keep the DX7-only depths (PITCH SENS, AMP DEPTH) in a small "Operator" section. Put the Op Pitch envelope in the envelope pool with the same editor. When no oscillator uses the Operator Env, disable the panel with "Only oscillators on the Operator Env follow this".
- **Evidence:** `dx7/08-FM-pitch-lfo.png`, `init/added-osc-FM.png`, `serum-extra/fm-init/fm-pitch-lfo.png`.

### 7. High: three separate drawn-shape systems (drawable LFO, Steps shape, MSEG module)
- **What's wrong:** LFO shapes can be drawn (points, GRID 8). Steps is an LFO shape with a bar editor. The MSEG is a single separate module: one card placed **after** the LFO pool's "+", its own "MSEG" chip, and only LOOP and RATE. It has no SYNC, RETRIG or SMOOTH, and it can't be duplicated. The MSEG card's caption ("drag its card onto a knob, or pick it as an oscillator's ENVELOPE") explains that it is also an envelope, which a user would not guess.
- **Serum 2:** Every LFO is a drawable multi-segment shape with modes (trigger / envelope / off). There is one system.
- **Fix:** Make "MSEG" a shape (or mode) of any LFO slot, like Steps, and keep the old MSEG source as "LFO n · MSEG" in old patches (append only). Until then, move its card before "+" and give it the LFO panel's SYNC / RETRIG / SMOOTH row.
- **Evidence:** `neuro/lfo-mseg.png`, `neuro/lfo-steps.png`, `init/env-dahdsr.png`.

### 8. High (workflow): the chip bar hides the core LFOs first
- **What's wrong:** When the bar is crowded it folds the LFO and ENV groups before the "performance" group (`PluginEditor.cpp:856-866`: "the performance sources only once both are folded"). On every DX7 voice, OP LFO and OP PITCH join the performance group, so LFO 2 and LFO 3 go behind "LFO +2 ▾" while PRESSURE, RANDOM and MSEG stay out. Routing LFO 2 now takes a hover or click plus the tray. The bar also changes with zoom: "LFO +2" at 100 % and "LFO 1, LFO 2, LFO +1" at 75 % (`dx7-small/08-FM.png`). OP LFO is an LFO but sits in the performance group.
- **Serum 2:** LFO 1-4 are always in reach.
- **Fix:** Fold in this order: unused performance sources (PRESSURE, RANDOM, MSEG) into "MORE ▾" first, then unused LFOs/ENVs past the third. Never fold a source that is already routed. Put OP LFO in the LFO group and OP PITCH in the ENV group.
- **Evidence:** `dx7/01-MAIN.png`, `serum-extra/fm-brass/fm-1.png`, `dx7-small/08-FM.png`.

### 9. High (content): every DX7 voice ships with the same four macros, one of them flagged dead
- **What's wrong:** E.Piano 1 and Brass 1 both show `BRIGHT · TONE · DRIFT · SPACE`, and SPACE has the orange "!" because it drives the reverb mix while the reverb slot is off. This is very likely the same on all 288 voices **(inferred from two voices and the shared macro set)**. Felt Hammer Board, the flagship piano, also opens with "HAMMER !" and "DECAY !". The new warning is honest, but factory content should never trigger it.
- **Serum 2:** Factory macros are hand-set per preset.
- **Fix:** For the DX7 set, turn the reverb slot on at 0 % mix so SPACE works, or map SPACE to something always present. Fix Felt Hammer Board's two targets. Add a library test that fails a factory preset with a macro target whose module is off (the UI already computes this for the "!").
- **Evidence:** `dx7/01-MAIN.png`, `serum-extra/fm-brass/fm-dx7-algorithm-1.png`, `keys/05-FILTER.png`.

### 10. Medium-High: DX7 duplicates and machine names crowd the browser
- **What's wrong:** "Oboe ROM2A" and "Oboe ROM4A", "Octave War ROM2B" and "ROM4B", "Koto ROM1A" and "ROM3B" sit next to each other (the ROM cartridges repeat voices). Names like "Jazz Guit1", "OB Genviv" and "KS Bell" are still cartridge abbreviations. DX7 voices are 288 of the 659 rows in All, so they slow down Up/Down auditioning.
- **Serum 2:** The factory library is curated, with packs as a filter.
- **Fix:** Hide exact duplicates (same parameter hash) and list the other banks in the info panel ("also in ROM4A"). Expand common abbreviations (Guit → Guitar). Consider sorting DX7 rows after native presets within a category.
- **Evidence:** `extras/extra-browser-dropdown.png`, `neuro/preset-browser.png`, `extras/extra-browser-docked.png`.

### 11. Medium (layout bug, cause found): the filter display's "MOD" label sits on the curve and the marker
- **What's wrong:** On PLAY's short filter display, "MOD" is drawn at `live.y - 14`, clamped to the plot top (`src/gui/FilterDisplay.h:518`). It lands on the response line, and in `p1-macro-card.png` it lands on marker 1. There is no collision check against the curve or the set marker.
- **Fix:** Put the label below the ring when the ring is in the top third. Skip it when the plot is under about 60 px tall (show it in the tooltip instead). Check its rectangle against the set marker.
- **Evidence:** `neuro/01-MAIN.png`, `serum-extra/p1/p1-main.png`, `serum-extra/p1/p1-macro-card.png`, `small/01-MAIN.png`.

### 12. Medium: DX7 operators still wear wavetable controls
- **What's wrong:** A sine operator on OSC shows FRAME (0 %, one frame), WARP / WARP AMT, SPECTRAL / SPEC AMT, UNI MODE / UNISON / DETUNE / BLEND / SPREAD / CHORD, BOUNCE, EDIT, LOAD .WAV and a 3D view of 64 identical sines. On PLAY each of the six operator rows has a FRAME knob at 0 %. The things that matter (output level, key scaling, rate scaling) are on another page.
- **Serum 2:** Not applicable, but its oscillator panel shows only what the oscillator type uses.
- **Fix:** When an oscillator is on the Operator Env, PLAY's fourth knob becomes OUTPUT (or KEY RATE), and OSC's SHAPE row becomes "Waveform ▾ · Feedback · FB type". Unison stays but folds. Default the display to WAVE for single-frame tables.
- **Evidence:** `dx7/03-OSC.png`, `dx7/01-MAIN.png`.

### 13. Medium (layout): the 6-operator stack is cramped
- **What's wrong:** On DX7 algorithm 1 (Brass 1) the stack 6 → 5 → 4 → 3 uses nodes of about 22 px radius. Each node's glow overlaps the next node. The "MOD" caption crosses the ring's stroke. The feedback loop ring overlaps OSC 6's node. The "100 %" arrow labels sit on the glows. The left 40 % of the canvas holds only the 2 → 1 pair.
- **Fix:** Scale the node size to the tallest stack (not to the canvas), put MOD/OUT under the number inside the ring, draw the feedback loop to the right of the node, and spread the columns across the width.
- **Evidence:** `serum-extra/fm-brass/fm-dx7-algorithm-1.png`, `serum-extra/crop-alg1.png`.

### 14. Medium: the FM diagram on a plain patch shows switched-off oscillators as live, in random places
- **What's wrong:** On Init, OSC 2 and OSC 3 are off (the matrix says "OSC 2: OFF"), but the diagram draws them as bright "OUT" nodes, scattered (1 top left, 2 top right, 3 bottom centre), with about 70 % of the canvas empty.
- **Fix:** Draw off oscillators dimmed and dashed. Lay unconnected nodes in one row along the bottom (the carrier line), as the algorithm tiles do.
- **Evidence:** `serum-extra/fm-init/fm-1.png`, `init/added-osc-FM.png`.

### 15. Medium: every FX card has two wet controls and a band menu
- **What's wrong:** Each card header has a BLEND 100 % pill, and the card body has a MIX knob (Vowel 70 %, Drive 50 %, OTT 50 %, Reverb 25 %). Both mean "how much effect". Every card also has a FULL BAND dropdown even when no splitter exists. Inside a SPLIT BANDS group, each card repeats its band ("LOW BAND", "HIGH BAND") under the group's own LOW / MID / HIGH bar, and nothing shows that MID is empty.
- **Serum 2:** One MIX per effect; no per-card band menu.
- **Fix:** Show BLEND only on effects without their own MIX, or drive the card's MIX from BLEND. Show the band menu only inside a split group, as the group's column headings (LOW | MID | HIGH lanes), with "(empty)" placeholders.
- **Evidence:** `neuro/10-FX.png`, `init/fx-split.png`; `src/gui/pages/FxPage.h:368-395`, `:892`.

### 16. Medium: FX page leftovers: two "+ ADD EFFECT", inconsistent card widths, and RACK A/B beside the header's A/B
- **What's wrong:**
  - "+ ADD EFFECT" appears in the toolbar and again as a dashed card in the rack.
  - Card width follows the neighbours: Chorus (three knobs) is full width in `fx-split.png`, while Vowel and Drive are half width in `neuro/10-FX.png`.
  - RACK A / RACK B / COPY TO B is a second A/B system, a second chain stored per patch (tooltip, `FxPage.h:260-262`), next to the header's A|B patch compare. Same letters, different meaning.
  - Below the cards, about 250 px is empty on a three-effect patch.
- **Fix:** Keep the dashed slot and drop the toolbar button, or the reverse. Make a card's width depend on its own content only. Rename the racks "CHAIN 1 / CHAIN 2" (or move them under SAVE / LOAD).
- **Evidence:** `neuro/10-FX.png`, `init/fx-split.png`, `neuro/fx-duplicate.png`.

### 17. Medium: the piano roll hides notes outside its pitch range with no sign of them
- **What's wrong:** Init's clip has notes at A#3, A3 and C4. In the normal view, the roll shows G2-G3, and those notes are simply not there. There is no scroll bar or edge marker, and only the EXPAND view shows them.
- **Serum 2:** The roll scrolls, with a scroll bar, and fits the clip on open.
- **Fix:** Centre and fit the pitch range to the clip's notes on open (the code already centres a *new* clip, `ClipEditor.h:596`). Draw "▲ 3" / "▼ 2" markers at the top and bottom edges for notes out of view. Show a thin vertical scroll bar.
- **Evidence:** `neuro/gen-clip-selected.png` vs `neuro/gen-clip-expanded.png`.

### 18. Medium (workflow): drawing notes takes a double-click each, and there is no paint mode
- **What's wrong:** A note is added by double-clicking empty space. Its length comes from the GRID, and you then drag its edge. A drag on empty space draws a selection box (`ClipEditor.h:14-17, 377-403`). Drawing a 16-note bass line takes 32 clicks plus length drags.
- **Serum 2:** A draw (pencil) mode where a click adds a note and a drag sets its length **(inferred)**.
- **Fix:** A DRAW toggle (or hold Alt or Ctrl): a single click adds a note, dragging right while adding sets its length, and dragging across empty cells paints a run at the grid. Make the last length used the default.

### 19. Medium: the ARP lanes have no readouts, and their off overlay covers the data
- **What's wrong:** The VELOCITY, GATE and PITCH lanes are flat brown blocks with no value scale or numbers. PITCH at 0 st is an empty row with no centre line. "ARP OFF: the lanes play once it is on" is a pill drawn over steps 7-11 of the GATE lane. All three lanes use the same colour.
- **Serum 2:** Lanes show a value on hover, with a centre line on bipolar lanes.
- **Fix:** A colour per lane, a 0 st centre line plus "+12 / −12" ticks, the value under the pointer, and the lanes dimmed when ARP is off, with the hint moved to the header.
- **Evidence:** `neuro/09-ARP-SEQ.png`.

### 20. Medium: the hover help covers the chip bar and uses a third naming scheme
- **What's wrong:** While the pointer is over a knob, the status line replaces the source chips (`neuro/hover-line.png`). The sources vanish exactly while you are looking at the knob you want to modulate. The line says "Osc1 Frame", the knob's source card says "OSC 1 › FRAME", and the matrix says "OSC 1 › Frame".
- **Fix:** Show the help line in the gap above the macro row, or in a tooltip, never over the chips. Take the parameter name from the same table as the matrix ("OSC 1 › Frame").
- **Evidence:** `neuro/hover-line.png`, `extras/extra-spec-and-card.png`, `neuro/07-MATRIX.png`.

### 21. Medium: VECTOR X / Y chips live in the vector card, not in the chip bar
- **What's wrong:** The fix for "VEC X / Y have no chip" put two chips in the VECTOR card's header. The bar at the bottom, where every other source lives, does not list them. The header also reads "four oscillators at the corners; drag", which looks cut off (it means "drag the chips", `FilterVectorPhysicalPages.h:342`).
- **Fix:** Add VEC X / VEC Y to the bar's performance group while the vector is on (and keep the header chips if wanted). Change the text to "drag X or Y onto a knob".
- **Evidence:** `neuro/vector-page.png`.

### 22. Medium: "body" now lives in three places with five labels
- **What's wrong:**
  - FILTER > BODY card: "BODY" switch, a BODY type ("Classic"), a COUPLING menu and a COUPLE knob.
  - OSC > ACOUSTIC KEYS tab: the soundboard.
  - OSC > PHYSICAL > BODY: a summary with "RESONATOR BODY off" and "SOUNDBOARD on, Dense", plus links to FILTER and ACOUSTIC KEYS.
  - SIGNAL FLOW: "BODY" and "STR+BRD".
  - The physical oscillator also has its own COUPLING knob (a different parameter).
- **Fix:** One "BODY" card (resonator + soundboard together) on FILTER, mirrored read-only on PHYSICAL. One word each: "BODY", "SOUNDBOARD", "BODY COUPLING" (menu), "STRING COUPLING" (oscillator knob).
- **Evidence:** `keys/physical-page.png`, `keys/05-FILTER.png`, `keys/03-OSC.png`.

### 23. Medium: switched-off cards look live on FILTER
- **What's wrong:** WEST is off, but all seven of its knobs, its three menus and its GATE display are drawn at full brightness. BODY (off) dims only MATERIAL and COUPLE. VECTOR dims when off, and PLAY's off oscillators are dimmed, so the off state is drawn three different ways.
- **Fix:** One rule: a module switched off draws its controls at the disabled alpha (they stay editable), as VECTOR and the oscillator rows do.
- **Evidence:** `neuro/05-FILTER.png`, `dx7/05-FILTER.png`.

### 24. Medium (layout): BODY card's controls start 60 px lower than WEST's
- **What's wrong:** In the FILTER page's bottom row, WEST's first row of controls starts just under its title, but BODY leaves about 60 px (at 1.5x) of empty band under "oscillator mix excites the body" before BODY / AMOUNT. The two cards side by side are misaligned.
- **Fix:** Top-align both cards' control grids at the same y.
- **Evidence:** `neuro/05-FILTER.png`.

### 25. Medium (layout): OSC page rows are mostly air, and their labels float below their controls
- **What's wrong:** SHAPE, PITCH & LEVEL and UNISON each take about 175 px of height for one row of knobs. The section label is vertically centred in the row, while the knobs sit at the top: "SHAPE" is level with the FRAME value, not the FRAME label, and "UNISON" sits below the UNI MODE menu. About half the controls panel is empty.
- **Serum 2:** Dense oscillator panels around a large display.
- **Fix:** Top-align the labels with the control labels, and pack the rows (about 90 px each). Give the space to the wavetable display, or move the SUB + NOISE / VOICE card up so it doesn't need scrolling.
- **Evidence:** `neuro/03-OSC.png`, `dx7/03-OSC.png`, `keys/03-OSC.png`.

### 26. Medium: the LFO and MSEG panels are mostly empty
- **What's wrong:** In MOD > LFO, the right-hand panel has SHAPE, three switches and three knobs in its top half, and about 150 px empty below. The MSEG panel has LOOP and RATE in a panel as large as the graph. Meanwhile the graph has no time ruler on LFOs (the MSEG has one: 0 ms … 2 s).
- **Serum 2:** The controls sit around a large graph (mode, rate, rise, delay, smooth, grid).
- **Fix:** Put the controls in one row under the graph and widen the graph to the full card. Give LFOs the same time ruler as MSEG (beats when synced).
- **Evidence:** `neuro/06-ENV-LFO.png`, `neuro/lfo-mseg.png`.

### 27. Medium: Op Env / Op Pitch / Op LFO on MOD are pills in the section header, not pool cards
- **What's wrong:** The fix put "OP LFO 5.4 Hz Sine → FM", "OP ENV 6 operators → FM" and "OP PITCH no pitch movement → FM" as small pills beside the LFO and ENVELOPES headings. They are a different widget from the pool cards next to them, and clicking one leaves the page.
- **Fix:** Show them as normal pool cards (same thumbnail and size) at the end of each pool, editable in place (see #6), with "→ FM" as a small link.
- **Evidence:** `dx7/06-ENV-LFO.png`.

### 28. Low-Medium: the envelope time axis mixes two scales
- **What's wrong:** The ticks read "10 ms, 100 ms … +10 ms, +100 ms" (Neuro), and "1 s … +10 ms, +1 s" on Felt Hammer Board. The attack/decay part is absolute and log-scaled, and the release part is relative, on one unlabelled axis.
- **Fix:** Label the split ("key up") and keep one unit style, or mark the release ticks "R +100 ms".
- **Evidence:** `neuro/06-ENV-LFO.png`, `keys/01-MAIN.png`.

### 29. Low-Medium: "Off" used as a value for levels and times
- **What's wrong:** Operator Env SUSTAIN "Off" and END "Off" (levels), envelope DELAY "Off" and HOLD "Off" (0 ms), Op LFO DELAY "Off". "Off" reads like a switch. TRIM on the same page already says "-inf dB".
- **Fix:** Levels read "-inf dB", times read "0 ms".
- **Evidence:** `dx7/08-FM.png`, `neuro/06-ENV-LFO.png`.

### 30. Low: the tutorial points to the wrong button
- **What's wrong:** "Reopen it any time with the ? beside SCOPE" (`TutorialOverlay.h:188`), but the ? sits beside KEYS. There is also a 100 px gap between the three START HERE bullets and NEW IN 1.3.
- **Fix:** "with the ? at the top right". Close the gap.
- **Evidence:** `neuro/00-tutorial.png`.

### 31. Low: the docked and floating browsers behave differently
- **What's wrong:** Floating: "Up/Down browse · Enter keep · Esc close" (audition, then keep or revert). Docked: "click loads · a tab closes it". The info panel lists macros 1-4 only, now that all 8 are always shown. The "+40" / "+39" tag overflow is plain text, not a chip.
- **Fix:** The same keys in both modes. List all named macros. Draw the overflow as a chip that opens the full tag list.
- **Evidence:** `extras/extra-browser-dropdown.png`, `extras/extra-browser-docked.png`.

### 32. Low: the confirm dialog interrupts auditioning **(inferred)**
- **What's wrong:** After an edit, every preset step (arrows or Up/Down) brings "Replace your edits?", with Save and load / Load anyway. That's good, but the dialog has an empty band between its text and the checkbox. While browsing, a single "keep editing in A, audition in B" path would be faster.
- **Fix:** After the first "Load anyway" in a browsing session, don't ask again until the browser closes. Tighten the dialog.
- **Evidence:** `confirm/00-confirm.png`.

### 33. Low: depth rings are thin targets
- **What's wrong:** Rings sit 2.5-3.5 px apart, and the hit band runs from the first ring −2.5 px to the last +3 px (`src/gui/ParamControls.h:1082-1125`). Grabbing the second of three rings means hitting a band about 3 px wide, which is about 2 px at 75 %.
- **Serum 2:** The selected source's depth handle is large.
- **Fix:** While a chip is pinned or hovered, give that source's ring the whole band (an 8 px target). Alt-drag anywhere on the knob adjusts the pinned source's depth.

### 34. Low: the FM matrix footer hides a limit
- **What's wrong:** RING MOD and SYNC 2 TO 1 apply only to OSC 1 × OSC 2. They sit in the matrix footer, with no way to pick the pair.
- **Fix:** Say "OSC 1 × OSC 2 only" in the label, or offer ring mod as a per-cell mode in the matrix.
- **Evidence:** `dx7/08-FM.png`.

### 35. Low: matrix noise
- **What's wrong:** VIA says "Aux: none" on all 12 rows. An unlabelled ▶ sits between POLARITY and DESTINATION on every row. The lower half of the page is empty on a 12-route patch, and "+ ADD MODULATION" is far top right.
- **Fix:** Leave VIA blank with "+ via" on hover. Drop the ▶, or label it in its tooltip. Put an "+ add" row after the last route.
- **Evidence:** `neuro/07-MATRIX.png`.

### 36. Low: the FX library's Airwindows layout
- **What's wrong:** The "AIRWINDOWS" sub-heading appears five times, and "AIRWINDOWS (ALL)" sits under the RHYTHM column, where nobody looks for it.
- **Fix:** One "AIRWINDOWS" column (or the existing per-category entries with a small badge, plus "(ALL)" at the end of TONE & LEVEL).
- **Evidence:** `init/fx-empty.png`.

### 37. Low: SEQ header says one thing while two engines run
- **What's wrong:** The chain reads "KEYS › STRUM › EUCLID › CLIP › VOICES", but the description says "EUCLID plays the held chord on each hit", and the editor below shows the clip. How EUCLID and CLIP combine is not said.
- **Fix:** The description follows the chain: "EUCLID triggers, CLIP supplies the notes", or whatever the engine does **(inferred)**.
- **Evidence:** `neuro/gen-clip-selected.png`.

### 38. Low: the clip roll's corner label repeats GRID
- **What's wrong:** "1/16" sits in the key column's top-left corner. It is the grid, already shown in the GRID menu below.
- **Fix:** Drop it, or make it the GRID control itself.

### 39. Low: PHYSICAL's soundboard box takes a third of the string view
- **What's wrong:** On Felt Hammer Board, "SOUNDBOARD (DENSE)" is a large gold box under the string with no controls **(inferred: not interactive)**, which squeezes the string animation.
- **Fix:** A slim band with the model name, or let it toggle to show its modes.
- **Evidence:** `keys/physical-page.png`.

### 40. Low: the envelope pool still scrolls sideways when it is full
- **What's wrong:** With many envelopes, the strip shows ENV 5 … ENV 9 cut at the edge, with a scroll bar under the cards. The PLAY envelope card gets a "+12" tab.
- **Fix:** Wrap to a second row, or fold extra envelopes into a "+12 ▾" card, as the chip bar does.
- **Evidence:** `neuro/lfo-steps.png`, `neuro/hover-line.png` (snapshot state, but reachable by users).

### 41. Low: the FM matrix hint is jargon
- **What's wrong:** "On the Operator Env a modulator's depth is its LEVEL; a cell scales it." It is true, but it uses a third meaning of "LEVEL" (see #4).
- **Fix:** "Depth = the modulator's OUTPUT × this cell".

### 42. Low: header spacing
- **What's wrong:** "VOICES" and "1/32" are two separate labels about 60 px apart, so they read as two items. "120.0 BPM" floats on its own.
- **Fix:** "VOICES 1/32" as one group, and BPM next to it with the same spacing.
- **Evidence:** every shot.

---

## Overall

**7.0 / 10 against Serum 2.** The review 6 round did what it said on most items. The modulation workflow (rings, grouped chips,
source cards, macro cards with dead-target warnings, a docked remap editor) is now close to Serum 2's. The browser, the piano
roll and the FX chain are real tools now, and the DX7 voices sound and look like part of the synth rather than a separate
mode. To reach 8+, the second cycle should do the following:
- Finish the merges it started: one operator LEVEL and one envelope name (#4, #5); the Op LFO and pitch EG edited where LFOs and envelopes are edited (#6, #27); one drawn-shape system (#7).
- Fix the two real layout bugs (#1 SIGNAL FLOW, #11 MOD label) and give PLAY's empty slots back (#2), with fixed knob columns (#3).
- Fold the chip bar's rare sources before LFO 2/3 (#8).
- Clean the factory content: DX7 macros that do something, no dead "!" on the flagships, no duplicate ROM voices (#9, #10).
