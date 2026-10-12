# UI review 8: ilanaSynth against Vital (pass 3, fresh and adversarial)

Reviewer: a fresh agent. The benchmark is Vital, judged from knowledge of its UI (I did not run it). I looked at layout clarity, the modulation drag-and-drop workflow, visual feedback, consistency and typography.

**Evidence**
- The shots in `/home/user/shots8/` (`neuro`, `init`, `dx7`, `keys`, `small`, `dx7-small`, `extras`, `remap`, `confirm`).
- New renders in `/home/user/shots8/vital-extra/`:
  - `p1-neuro/`: `ILANA_SNAPSHOT_P1=1` on Neuro Wobble (knob card, macro card, chip tray, matrix, remap).
  - `fm-piano2/`: `ILANA_SNAPSHOT_FM=1` on "PIANO 2 (ROM1A)", DX7 algorithm 18, then algorithm 1.
- To confirm causes, I read `src/gui/` and `src/PluginEditor.cpp`.

**What I did not count**
- Claims marked **(inferred)** are not confirmed in a shot or in the code.
- Some states are snapshot-tool artefacts, so I left them out:
  - EDITED after `03-OSC`.
  - The large envelope and LFO pools after the pool-reveal steps (ENV +12 / +14 chips).
  - Felt Hammer Board's HAMMER / DECAY "⚠" marks and its "OSC 1 plays a wavetable" PHYSICAL page, which come from the tool flipping OSC 1.
  - Neuro's TONE → OSC 3 route in the P1 set.

## Verdict

This is the best build so far, and the change since review 7 is real. On the vocabulary:
- The operator envelope has one name: OP ENV / Operator Env. A grep finds no "EG" in visible text.
- Ratios read `×1.00` everywhere.
- PLAY shows an operator's real LEVEL in dB.
- The FM node style is unified.
- BASIC tiles are named B1-B9.
- A wavetable patch says "FM FROM 2" instead of "FM operator".
- The SEQ pills carry their own switch.

On layout:
- PLAY's dead dashed slots are gone.
- Pools fold into "N MORE" instead of scrolling.
- FX has one dry/wet control and CHAIN 1 / 2.
- WEST, BODY and VECTOR dim as a whole when they are off.
- The remap header names the route.
- Clip selection is clear.
- The chip bar folds the same way at 75 % and at 100 %.

As a polished product next to Vital, though, three kinds of problem remain:

1. **The DX7 layer still shows on patches that don't use it.** Every patch's MOD page has three DX-only cards: OP LFO, OP ENV and OP PITCH. Init has them, and so does Neuro Wobble, a wavetable bass. All three read "unused". One of them, OP ENV, sits in a source pool but cannot be dragged as a source. Vital's MOD area only ever shows modulators.
2. **One thing is still shown two ways, at the next level down:**
   - There are two MSEG systems: the patch MSEG card and SHAPE › MSEG.
   - TRIM is a % on PLAY and OSC, and dB on FM.
   - An operator's controls come in three orders on PLAY, OSC and FM, and the OP ENV knobs come in two orders on FM and MOD.
   - Destination names come in three casings.
   - The chip bar's contents differ from the pools'.
3. **Space and placement.**
   - The LFO / MSEG editor pane, the FX page, the FM page on a non-DX patch, EVOLVE and the PHYSICAL page are still largely empty.
   - The source chip bar still moves its chips between presets: AMP ENV sits 45 px further right on a DX7 voice than on Neuro.
   - Several old layout bugs remain:
     - the F1 and F2 nodes on top of each other (third review running);
     - handles on the OP ENV ruler labels;
     - squeezed value text;
     - halos touching on deep algorithms.

None of this is structural any more. It is the polish layer Vital is known for, and that is the gap between 7.5 and 9.5.

## Fixed since review 7 / still open (review 7 Vital items)

| # | Review 7 finding | Status in this build |
|---|---|---|
| 1 | Operator envelope has four names | **Fixed**: OP ENV short, "Operator Env" long; no "EG" in visible strings |
| 2 | LEVEL two parameters on PLAY and FM | **Mostly fixed**: PLAY shows LEVEL in dB plus TRIM. New: TRIM reads % on PLAY / OSC and dB on FM (V8-4) |
| 3 | Deep DX7 algorithms crowd the diagram | **Partly fixed**: nodes stay full size. On algorithm 18 the 6→5→4 chain snakes up and back, halos touch and depth labels sit on arrowheads (V8-7) |
| 4 | PLAY fixed slots waste a third | **Fixed** (one "+ ADD OSC 4" row). Off oscillators still take a full 170 px strip (V8-17) |
| 5 | OP pitch EG / LFO a parallel system | **Partly fixed**: OP LFO / OP ENV / OP PITCH are pool cards edited on MOD. But they show on every patch (V8-1), OP ENV isn't a source (V8-11), and VOICE PITCH & LFO is still in the operator pill row (V8-21) |
| 6 | Chip bar reflows by preset and zoom | **Partly fixed**: the same at 75 % and 100 %. Still differs by preset and routing, with three overflow chips (V8-3) |
| 7 | Two dry/wet controls on FX cards | **Fixed** (BLEND only where an effect has no MIX). Reverb still has KEEP DRY beside MIX (V8-33) |
| 8 | Dead SPACE macro on DX7 voices | **Fixed** (ROOM drives a reverb that is on, at MIX 0) |
| 9 | Op Env graph unusable for short segments | **Partly fixed**: log time axis. Handles still sit on the "10 s" label, and level 0 reads "-inf dB" (V8-8) |
| 10 | KEYS & VELOCITY raw DX7 | **Mostly fixed** (dB, %, note name). AMP MOD 0 reads "Off", curves read "-Lin" (V8-32) |
| 11 | SEQ pills are tabs and lights | **Fixed** (switch inside each pill) |
| 12 | Ratio written two ways | **Fixed** (×1.00) |
| 13 | Two FM node styles | **Fixed** ("OSC n / role", caption to the right) |
| 14 | BASIC tiles unnamed, "+1" | **Fixed** (B1-B9, "+1 OSC", "CUSTOM: NEAR B1 2-OP STACK") |
| 15 | Wavetable bass called an FM operator | **Fixed** ("FM FROM 2", "FM INTO 1") |
| 16 | DX7 operators on OSC show wavetable / unison | **Mostly fixed** (OP ENV graph, LEVEL, RATIO). Still "Wavetable" combo, RESAMPLE / EDIT / LOAD .WAV, UNISON (V8-16) |
| 17 | Pools scroll sideways | **Fixed** ("11 MORE" card) |
| 18 | Filter nodes overlap, "MOD" collides | **Half fixed**: the MOD text is gone (ring only). F1 / F2 still overlap at 20 kHz (V8-9) |
| 19 | MSEG one extra card | **Changed into a new problem**: SHAPE › MSEG was added, but the patch MSEG card stayed, so there are now two MSEG systems (V8-2) |
| 20 | Matrix VIA column | **Fixed** ("+" placeholder). The lower third is still empty (V8-20) |
| 21 | VECTOR knobs live while off | **Fixed**. The empty band under the header remains (V8-26) |
| 22 | Two A/B systems | **Fixed** (CHAIN 1 / CHAIN 2 / COPY TO 2) |
| 23 | Envelope ticks inside the plot | **Fixed** on AMP / FILT ENV. Still inside on the OP ENV graph (V8-8) |
| 24 | AMP ENV graph at full colour on DX7 | **Fixed** (dimmed, "Not used: the OSCs play their OP ENV", EDIT OP ENV) |
| 25 | DX7 rows repeat bank and FM tag | **Fixed** (dim bank column, no FM tag). New: DX7 count 288 vs 270 (V8-25) |
| 26 | PLAY knob order changes in an FM route | **Fixed** (SEMI LEVEL FRAME UNISON on every wavetable strip) |
| 27 | Rings and floating badges | **Fixed** (rings only, no floating badges in the shots) |
| 28 | Bare amber "!" | **Fixed** (⚠ plus a line in the macro card: "OSC 3 is off: no effect now") |
| 29 | Dead areas on MOD, FX, PHYSICAL | **Still open** (V8-6) |
| 30 | Noise colour on FM, level on PLAY | **Still open** (V8-15) |
| 31 | 3D default for one-frame tables | **Fixed** (WAVE for Sine) |
| 32 | EXCITER row two knobs wide | **Still open** for Piano Hammer. Fine for Bow (V8-23) |
| 33 | PHYSICAL duplicates OSC, BODY as links | **Partly fixed** (the page is now a large live string with the controls under it). BODY is still two status lines with jump buttons (V8-23) |
| 34 | WEST / BODY live while off | **Fixed** |
| 35 | Floating BYPASS label | **Fixed** (label on the line) |
| 36 | Casing drifts | **Partly fixed**: combos are upper case. Destination names still come in three casings (V8-13) |
| 37 | "F1 = F2" caption | **Fixed** ("Even") |
| 38 | Clip selection hard to see | **Fixed** (white outline, others dim) |
| 39 | 75 % chip bar folds differently | **Fixed** |
| 40 | Remap "ROW 1 (slot 1)" | **Fixed** ("REMAP · Macro 1 (TONE) → Filter 1 › Cutoff") |
| 41 | KEY UP marker collides | **Fixed** for the caption. The OP ENV handles still sit on ruler labels (V8-8) |
| 42 | Duplicate FX offered | **Fixed** (types in the rack are dimmed in the library). The old-patch duplicate card still shows a BLEND pill (V8-38) |
| 43 | "DICE FX" | **Fixed** (dice icon + FX) |

Summary: 27 fixed or mostly fixed, 11 partly fixed, 4 still open, 1 turned into a new problem (#19).

## Findings, worst first

### V8-1. High: three DX-only cards on every patch's MOD page, including Init
**What's wrong.**
- On Init, Neuro Wobble and every other non-DX patch, the LFO pool ends with **OP LFO "unused"** and the ENVELOPES pool with **OP ENV "unused"** and **OP PITCH "unused"**.
- That is three of the eight visible cards on a wavetable patch, a fifth of the MOD page, for a feature the patch doesn't use.
- Selecting OP ENV shows the line "unused: no oscillator plays the Operator Env (an oscillator's ENVELOPE picks it)" and ten dimmed knobs.
- This is exactly the "own corner, own vocabulary" problem the owner asked about. The DX7 engine announces itself on every patch.

The cards are added unconditionally (`EnvLfoPages.h:347-360`, `opEnv.isActive = anyOperator` only greys them; the OP LFO card is at `:739-745`).

**Vital's way.** The modulator area shows only the modulators the patch has. A feature appears when you add it.

**Fix.**
- Show OP ENV / OP PITCH / OP LFO only while some oscillator's ENVELOPE is OP ENV, the same rule the MSEG card already uses ("while it is used").
- Otherwise offer them in the "+" menu as "Operator Env (DX7)".
- Add a uitest: on Init, no pool card reads "unused".

**Evidence:** `init/env-dahdsr.png`, `neuro/06-ENV-LFO.png`, `small/06-ENV-LFO.png`, `neuro/lfo-steps.png`.

### V8-2. High: two MSEG systems
**What's wrong.** Review 7 asked for MSEG to become an LFO shape. It did (SHAPE › MSEG), but the old one stayed:
- The patch-level **MSEG card** is still in the LFO pool (`neuro/lfo-mseg.png`).
- It has its own panel: LOOP and RATE, about 70 % empty.
- Its subtitle reads "the patch's four-point MSEG; drag its card onto a knob". Its footer reads "Any LFO can draw its own: SHAPE › MSEG" (`EnvLfoPages.h:763-774, 984-997`).
- The chip bar has an **MSEG** chip even when the pool doesn't show the card (`neuro/01-MAIN.png` chip bar vs `neuro/06-ENV-LFO.png` pool).

So a user can draw an MSEG in two places, with two editors, two RATE knobs and two grids. Only one of them can be an oscillator's ENVELOPE.

**Vital's way.** One drawn-shape system: every LFO is an MSEG.

**Fix.**
- Migrate the patch MSEG into a hidden LFO slot on load, and show it as "LFO n (MSEG)" in the pool, so one editor serves both.
- Keep the `mseg_*` parameters for old patches.
- Let ENVELOPE pick "LFO n, once per note".
- If that is too far: hide the MSEG card and chip unless the patch routes or plays it, as the card's own comment says it should.

### V8-3. High: the source chip bar still moves under the user's hand
**What's wrong.** Folding is now stable across zoom, but not across presets or routing:
- **Neuro** (`neuro/01-MAIN.png`): LFO 1 | LFO 2 B | LFO 3 | MSEG ‖ AMP ENV starts at x≈455 ‖ … PRESSURE | RANDOM.
- **E.Piano 1** (`dx7/01-MAIN.png`): LFO 1 | LFO 2 | LFO 3 | **LFO +2 ▾** ‖ AMP ENV starts at x≈410 ‖ … **ENV +1 ▾** ‖ … **MORE +1 ▾** (RANDOM is hidden).
- Adding a fourth LFO (`p1-neuro/p1-chip-tray.png`) pushes AMP ENV to x≈555, hides MSEG and folds PRESSURE and RANDOM into MORE +2.

There are up to three overflow menus with different names. Routed chips never fold (`PluginEditor.cpp:924`, `if (chipRouted[index]) return 0`), so the bar also changes when you add or remove a routing. The bar and the pools disagree too: Neuro's bar has MSEG but no OP LFO, while its pool has OP LFO but no MSEG.

**Vital's way.** Sources sit in fixed places (LFO 1-8 and ENV 1-6 tabs, fixed macro and keyboard sources).

**Fix.**
- Give the bar fixed slots: LFO 1-4, ENV 1-4 (AMP, FILT, FILT 2, MOD) and the four performance sources.
- Put a single "MORE ▾" at a fixed x for everything else: LFO 5+, ENV 5+, MSEG, OP *, RANDOM, VEC X / Y.
- Don't let routing state change positions; show routing with a lit dot instead.
- Build the bar from the same list the pools use, so the two always agree.

### V8-4. High: TRIM is a % on PLAY and OSC and dB on FM
**What's wrong.** On an OP ENV oscillator the same parameter (`oscN_level`) reads in two units:
- **"TRIM 50%"** on PLAY (`MainPage.h:78`) and on OSC (`OscPage.h:335`).
- **"TRIM 0.0 dB"** on the FM page's KEYS & VELOCITY tab (`FmInputPages.h:808-821`, `setUpLevelText`).

This is the same class of bug as review 7's #2 (LEVEL % vs dB), moved one knob over. The workplan's vocabulary says "the oscillator's trim (%) is TRIM on every page".

**Vital's way.** One knob, one unit, everywhere.

**Fix.** Pick one; dB is clearer for a trim ("0.0 dB" at 50 %). Install the text function on the shared knob factory instead of in `FmPage`, so PLAY, OSC and FM all use it.

**Evidence:** `dx7/01-MAIN.png` and `dx7/03-OSC.png` vs `vital-extra/fm-piano2/fm-1-keys.png`.

### V8-5. High: an operator's controls come in three layouts, and its envelope in two orders
**What's wrong.**
- **PLAY** strip: RATIO, LEVEL, TRIM, FINE.
- **OSC** card: OP ENV (LEVEL), then PITCH & LEVEL (TUNING, RATIO, FINE, TRIM, PAN, ENVELOPE), then WAVE (TABLE, FEEDBACK, FB TYPE, UNISON). SEMI is missing.
- **FM** card: TUNING, SNAP, FB TYPE, ENVELOPE / RATIO, **SEMI**, FINE, LEVEL; TRIM sits on another tab.
- **OP ENV knobs**:
  - FM, STAGES tab: ATTACK, DECAY 1, DECAY 2, RELEASE, PEAK, MID, SUSTAIN, END, **KEY RATE** (`FmInputPages.h:212`).
  - MOD: ATTACK, DECAY 1, DECAY 2, RELEASE, **KEY RATE**, PEAK, MID, SUSTAIN, END, **LEVEL** (`OperatorPoolCards.h:256`).
  - So KEY RATE moves from last to fifth, and LEVEL appears in one place only.

Muscle memory breaks every time the user changes page.

**Vital's way.** An oscillator module is the same widget with the same order on every view.

**Fix.**
- One operator control order: RATIO | SEMI | FINE | LEVEL | TRIM.
- One OP ENV row: ATTACK, DECAY 1, DECAY 2, RELEASE | PEAK, MID, SUSTAIN, END | KEY RATE, defined once and used by both the FM card and the MOD pool card.

**Evidence:** `dx7/01-MAIN.png`, `dx7/03-OSC.png`, `dx7/08-FM.png`, `dx7/06-ENV-LFO.png`.

### V8-6. High: large dead areas remain on five pages
**What's wrong.**
- **LFO editor** (`neuro/06-ENV-LFO.png`): the right pane is about 780×310 px with SHAPE, three toggles and three knobs spread across it. The knob row floats about 80 px under the toggles.
- **MSEG pane** (`neuro/lfo-mseg.png`): two controls in about 780×310 px.
- **FX** (`neuro/10-FX.png`, `dx7/10-FX.png`): about 260 px empty under OUTPUT with three effects, and about 400 px with one.
- **FM on a non-DX patch** (`neuro/08-FM.png`):
  - The diagram is a 830×400 box holding two nodes.
  - The operator card spreads four knobs across 800 px.
  - The 3×4 FM matrix has 160×120 px cells, each holding one 6 px dot.
- **VECTOR** (`neuro/02-VECTOR.png`): a 60 px empty band under the header. EVOLVE is a 400×780 pane holding one sentence and a button.
- **PHYSICAL** (`keys/04-PHYSICAL.png`): the card is inset 80 px from the page edges, unlike every other page.

**Vital's way.** Panels are sized to their content, and editors fill their frames (the LFO graph is wide, the controls sit in a strip).

**Fix.**
- LFO / MSEG: graph full width, controls in one row under it.
- FX: OUTPUT directly under the last card. Let the rack grow into the page, or center it.
- FM: cap the matrix cell size at about 64 px and centre the grid. Size the diagram to its nodes and give the space to the operator card.
- PHYSICAL: use the standard page margins.

### V8-7. Medium-high: deep algorithms still read poorly (algorithm 18)
**What's wrong.** On PIANO 2 (`vital-extra/fm-piano2/fm-1.png`) the nodes keep their size, but:
- The chain 6→5→4→1 is drawn as a diagonal that climbs to the top centre (OSC 6), drops right (OSC 5), goes further right (OSC 4) and comes back left into OSC 1. A DX7 chart draws it as a vertical stack.
- OSC 3's halo overlaps OSC 1's halo.
- OSC 6's halo touches OSC 5's.
- The three "100%" depth labels sit on arrowheads, beside OSC 3 and under OSC 6.
- OSC 2's arrow runs a long curve under OSC 3.
- The top-left quarter of the box is empty.

**Vital's way.** Not applicable (no operator view). Dexed and the DX7 manual draw each algorithm as stacks on a grid.

**Fix.**
- Lay out every DX7 algorithm on a column grid: carriers on the bottom row, each modulator stack straight above its carrier, side branches in the next column.
- The 32 layouts are fixed, so precompute them as a table.
- Extend the uitest to check that halos don't intersect and that labels don't intersect arrows.

### V8-8. Medium-high: OP ENV graph: ruler inside the plot, handles on labels, "-inf dB"
**What's wrong.**
- On FM, OSC and MOD, the OP ENV graph draws its ruler ("100 ms", "1.0 s", "10 s", "+10 ms") inside the plot.
- The END and release handles sit on the "10 s" label: `dx7/08-FM.png` (handle at x≈790 on "10 s"), `dx7/03-OSC.png`, `fm-piano2/fm-1.png` ("+10 ms" under the last two handles).
- AMP ENV moved its ruler under the frame (review 7 #23); OP ENV didn't, so the two envelope graphs now look different.
- Level 0 reads **"-inf dB"**: lower-case ASCII, where a typographer would write "−∞ dB" (`OperatorEnvDisplay.h:237`, `FmOperatorInfo.h:95`, `ParamInfo.h:218, 227`, `FmInputPages.h:817`).

**Fix.** Share `EnvelopeDisplay`'s below-frame ruler with `OperatorEnvDisplay`. Pad the plot so the last handle sits clear of the right edge. Use "−∞ dB" from one helper.

### V8-9. Medium-high: Filter 1 and Filter 2 nodes still overlap (third review running)
**What's wrong.** With both filters open at 20 kHz (every DX7 voice, Felt Hammer Board, Init), the "1" and "2" nodes touch or overlap at the right edge. See `dx7/01-MAIN.png`, `keys/05-FILTER.png`, `init/mod-pinned-chip.png`. On PLAY's 640 px graph the two are about 30 px apart and read as one blob.

**Vital's way.** Each filter has its own graph, or the nodes are separated.

**Fix.** When the nodes are within 1.5 radii, fan them vertically (one above the curve, one below) and keep each one's leader line to its true cutoff. Add a uitest: node circles must not intersect.

### V8-10. Medium: no target feedback while dragging a source
**What's wrong.** While you drag a chip, the only feedback is on the knob under the mouse (`ParamControls.h:1105-1135`: `itemDragEnter` sets `dragHover` on that knob alone). Nothing on the page shows which controls take the source until the mouse reaches them. Knobs that can't be modulated show a dashed track, but only in their resting state. **(inferred** from the code; no shot of a drag in progress**)**.

**Vital's way.** While a modulation source is dragged, every modulatable control lights up with a target circle, and the one under the cursor highlights. You can see the drop targets before you move.

**Fix.**
- While a `modsource:` drag is active (`DragAndDropContainer::isDragAndDropActive`), have every `ParamKnob` paint a faint ring in the source's colour.
- Dim the knobs that can't take it.
- Highlight the one under the cursor.

### V8-11. Medium: the OP ENV card sits in a source pool but is not a source
**What's wrong.**
- Every other card in the MOD pools can be dragged onto a knob: the MSEG subtitle says so, and so does the matrix header ("Drag a source onto any knob").
- OP ENV's tooltip says "It shapes its operators only, so it isn't a modulation source" (`EnvLfoPages.h:357-358`).
- It looks exactly like FILT ENV beside it.
- OP PITCH and OP LFO, its neighbours, *are* sources (chips).

So the pool's one rule ("a card is something you can drag") has an exception that looks identical.

**Fix.** Either make OP ENV a source (per-voice operator 1's envelope is useful), or draw it as an editor tab, not a pool card: a dashed outline and no drag affordance, with the cursor showing "not a source" on drag.

### V8-12. Medium: squeezed value and label text
**What's wrong.** JUCE's fitted text shrinks text sideways when it doesn't fit (`IlanaLookAndFeel.h:818`, `getMinimumHorizontalScale`). Visibly condensed:
- **"-30.9 dB"** and **"-15.1 dB"** under PLAY's LEVEL knobs (`dx7/01-MAIN.png`, `dx7-small/01-MAIN.png`);
- **"STRING COUPLING"** (`keys/03-OSC.png`, `keys/physical-page.png`);
- **"KEY RATE"** at 75 % (`small/06-ENV-LFO.png`);
- the **"Piano Hammer"** combo (`keys/physical-page.png`, a smaller font than the combos beside it).

Next to normal text, squeezed glyphs look broken.

**Vital's way.** Fixed label widths and short names. Text is never condensed.

**Fix.**
- Set the minimum horizontal scale to 1.0 for knob values and labels.
- Widen PLAY's knob columns by about 6 px, or write "−30.9" with "dB" in the label.
- Rename STRING COUPLING to COUPLING (its section already says STRING).
- Add a uitest that fails when a label needs a scale below 1.0 at 100 % or 75 %.

### V8-13. Medium: destination names in three casings, and SEMI is also "Pitch"
**What's wrong.**
- Knob hover card: **"OSC 1 › FRAME"** (`p1-neuro/p1-knob-card.png`).
- Matrix, macro card and remap: **"Filter 1 › Cutoff"**, **"OSC 1 › Frame"**, **"Sub › Level"** (`neuro/07-MATRIX.png`, `p1-macro-card.png`).
- Hover info bar: **"OSC 1 › Pitch"** for the knob labelled **SEMI** (`neuro/hover-line.png`).

`docs/UI-CONVENTIONS.md` says module and source names are upper case in labels, chips and combos. The matrix combos break that. The SEMI / Pitch pair is one control under two names.

**Fix.** One formatter (`ModNames`) for "MODULE › CONTROL", with the control named exactly as its knob label: "OSC 1 › SEMI", "FILTER 1 › CUTOFF". Use it in the knob card, the matrix, the macro card, the remap header and the info bar.

### V8-14. Medium: the SUB + NOISE switch only switches the sub
**What's wrong.**
- The card is titled SUB + NOISE and has one header switch, but the switch is `subosc_on`: NOISE stays live with its own level (`MainPage.h:794-801`, "noise has its own level").
- With the switch off, SHAPE / OCTAVE / SUB dim while NOISE doesn't (`dx7/01-MAIN.png`).
- On OSC, the "SUB + NOISE" tab dot reads "on" when either is active (`OscPage.h:860`).
- The same knob is **SUB** on PLAY and **SUB LEVEL** on OSC.

**Vital's way.** SUB and NOISE are separate modules, each with its own switch.

**Fix.** Either put the switch on the SUB half only, with "SUB" written beside it, or split the card into SUB | NOISE halves with a switch each. Use one label: SUB LEVEL or SUB.

### V8-15. Medium: noise COLOUR still lives only on FM
**What's wrong.** Unchanged since reviews 6 and 7. NOISE level is in SUB + NOISE on PLAY and OSC, while COLOUR is a knob in the FM matrix's NOISE FM row (`neuro/08-FM.png`). A user shaping noise on PLAY or OSC can't find its colour.

**Fix.** Show the same COLOUR knob beside NOISE in SUB + NOISE (one parameter, two views), as review 7 asked.

### V8-16. Medium: DX7 operators still carry wavetable furniture
**What's wrong.**
- PLAY: every operator strip still has a "Wavetable" / "Sine" combo pair (`dx7/01-MAIN.png`).
- OSC header: RESAMPLE, EDIT, LOAD .WAV and the "Wavetable" mode combo.
- OSC WAVE row: TABLE and UNISON 1 (`dx7/03-OSC.png`).

On a DX7 voice these are about 30 controls with no musical role. The combos take the most valuable column on PLAY.

**Fix.** For an OP ENV oscillator:
- On PLAY, replace the combo pair with a WAVE pill ("SINE") and the OP ENV mini graph.
- On OSC, fold RESAMPLE / EDIT / LOAD .WAV into the "⋯" menu and hide UNISON behind a "+ UNISON" row.

### V8-17. Medium: off oscillators keep full-height strips on PLAY
**What's wrong.** Neuro's OSC 3 (off) and Felt Hammer Board's OSC 2 and OSC 3 (off) each take a 170 px strip with a dimmed display, two combos and four knobs (`neuro/01-MAIN.png`, `keys/01-MAIN.png`). On Felt Hammer Board, 340 px of PLAY is two oscillators that make no sound. Meanwhile SUB + NOISE, also in the column, keeps 170 px for two knobs.

**Vital's way.** Vital has only three oscillator panels, always shown and usually in use, so an off oscillator costs little space. This build has up to six slots, so the cost of an off strip is much larger.

**Fix.** Draw an off oscillator as a 40 px header strip ("OSC 3 · OFF · Basic saw" plus its switch) that expands when switched on. That keeps the order fixed, which was the reason for fixed slots, and gives the space to the strips in use.

### V8-18. Medium: envelope knob order doesn't follow the graph, and there are two curve controls
**What's wrong.**
- **DAHDSR order.** The AMP ENV editor shows ATTACK, DECAY, SUSTAIN, RELEASE, VEL, CURVE, DELAY, HOLD, KEY RATE (`init/env-dahdsr.png`). The graph labels DELAY and HOLD on the left, before the attack, but their knobs come seventh and eighth, after RELEASE.
- **Two curve controls.** The header says "the dot on a segment sets its curve", yet a CURVE knob sits in the row too. Two controls for curve, and nothing says how they relate.

**Vital's way.** DELAY, ATTACK, HOLD, DECAY, SUSTAIN, RELEASE in time order, with curve set only by dragging the segment.

**Fix.**
- Order the knobs DELAY, ATTACK, HOLD, DECAY, SUSTAIN, RELEASE | VEL, KEY RATE.
- Rename the CURVE knob to what it does: probably the global velocity curve, or "ALL CURVES". If the segment dots cover it, drop it from this row.

### V8-19. Medium: FX split group: mixed card widths, a half-empty Drive, a clipped Reverb
**What's wrong.** In `neuro/fx-split.png`:
- Inside SPLIT BANDS, DRIVE is a half-width card with its right half empty, while REVERB below it is full width.
- The rack scrolls, and REVERB's knob values are cut by the fixed OUTPUT card.
- Above the split, CHORUS is half width with nothing beside it.

**Fix.** One rule for card width: full width inside a split group; two-up only for top-level cards, and only in pairs. Make OUTPUT scroll with the rack, or reserve its height before laying out the cards.

### V8-20. Medium: matrix lower third empty; the repeat count disagrees with itself
**What's wrong.**
- **Empty space.** With 12 rows, about 200 px under "+ ADD MODULATION" is empty (`neuro/07-MATRIX.png`).
- **Counts disagree.** With repeats, the header says "2 repeat a routing (click ! to merge)" while the button says "MERGE REPEATS (1)" (`p1-neuro/p1-remap.png`). The first counts rows and the second counts pairs; a user reads them as a contradiction.
- **Remap pane.** The pane right of the remap curve is mostly empty under its three lines.

**Fix.** "1 routing is repeated (rows 1 and 10)" and "MERGE REPEATS". Let the rows grow to fill the height, up to 44 px, when there are few.

### V8-21. Medium: VOICE PITCH & LFO still sits in the operator pill row
**What's wrong.** On FM, the pill row reads "1 2 3 4 5 6 | VOICE PITCH & LFO" (`dx7/08-FM.png`). The voice-wide pitch EG and LFO look like a seventh operator tab. On Neuro the button is greyed but still there (`neuro/08-FM.png`). Now that OP PITCH and OP LFO are MOD pool cards, this tab is a second editor for them: the parallel-system problem again.

**Fix.** Remove the tab and make the button jump to MOD with OP PITCH / OP LFO selected, as FM ›, the reverse link, already does. Or show it as a small link ("OP PITCH / OP LFO on MOD ›") beside ALGORITHMS.

### V8-22. Medium-low: the ARP control row is misaligned, and "GATE" means two things
**What's wrong.**
- **Misaligned row.** On SEQ, the MODE and RATE combos sit with their labels at y≈605, while OCTAVES, GATE, CHANCE and STEPS have their labels at y≈587 (`neuro/09-ARP-SEQ.png`). The row has two baselines.
- **Two GATEs.** "GATE" is both a lane (per-step length) and a knob (global length), side by side.

**Fix.** One baseline per control row: combos top-aligned with knob labels. Rename the knob GATE ALL or LENGTH, or the lane GATE ×.

### V8-23. Medium-low: PHYSICAL: the Piano Hammer EXCITER row is half empty, and BODY is still status lines
**What's wrong.**
- **EXCITER row.** For Piano Hammer, it holds EXCITE, EXCITE POS and HAMMER, then about 500 px of nothing (`keys/03-OSC.png`). Bow fills the row (`neuro/osc-sympathetic.png`), so the layout depends on the exciter.
- **BODY.** On the PHYSICAL page, BODY is "BODY off [FILTER] · SOUNDBOARD on, Dense [ACOUSTIC KEYS]": two status lines with jump buttons, so body is still edited in three places (`keys/physical-page.png`).

**Fix.** Lay the EXCITER row on a fixed grid, the same width as the STRING row. Give BODY a compact control group on the PHYSICAL page (TYPE, AMOUNT, DECAY). Keep the jump buttons as "more ›".

### V8-24. Medium-low: PHYSICAL preview animates a string the page says doesn't exist (inferred)
**What's wrong.** In `keys/04-PHYSICAL.png` the text reads "OSC 1 plays a wavetable, so it has no string. Switch it to Physical…", yet above it a live string, a bow line, "BOW" and a "SOUNDBOARD (DENSE)" box are drawn at full strength. The state comes from the snapshot tool, but the contradiction would show for any user who switches an oscillator away from Physical **(inferred)**.

**Fix.** In the empty state, draw the preview dimmed with no string, or hide it.

### V8-25. Medium-low: browser: DX7 count changes, and duplicate names
**What's wrong.**
- The DX7 chip reads **"DX7 288"** in the drop-down (`extras/extra-browser-dx7.png`) and **"DX7 270"** in the docked view (`extras/extra-browser-docked.png`). Hidden duplicates are counted in one view and not in the other.
- The All list shows "Oboe ROM2A" and "Oboe ROM4A" as adjacent rows with the same name, tags and category.

**Fix.** Count what the list shows. Disambiguate same-name rows ("Oboe", "Oboe (ROM4A)"), or merge identical voices.

### V8-26. Low: VECTOR keeps an empty band, and EVOLVE is a big empty pane
`neuro/02-VECTOR.png`:
- About 60 px between the VECTOR header and the pad.
- EVOLVE is a full-height pane holding one centred sentence and a "+ MACRO" button.

**Fix.** Start the pad under the header. Size EVOLVE to its rows, or put it under the pad as a strip.

### V8-27. Low: signal-flow abbreviations
"SUB+N", "STR+BRD" (`FilterWidgets.h:760`) and "OSC 1" all appear in one diagram. Nowhere else does the synth abbreviate SUB + NOISE, or SYMPATHETIC STRINGS + SOUNDBOARD.
**Fix.** "SUB/NOISE" and "STRINGS · BOARD" on two lines, or the full names in a wider node.

### V8-28. Low: the "B" sub-chip is unexplained
"LFO 2 [B]" on the chip and the card (`ModSourceChip.h:76-131`, `LfoThumbs.h:712-848`) is the shape's second output. The only place this is said is a tooltip. Vital has no equivalent.
**Fix.** Label it "OUT B" on hover, and show "2 outputs" in the LFO editor header when the shape has a B output.

### V8-29. Low: LFO cards print routing text over the waveform
On PLAY and MOD, "Filter 1 › Cutoff +2" and "OSC 1 › Frame +1" are drawn on top of the thumbnail curve (`neuro/01-MAIN.png`). On PLAY's 170 px cards the label covers half the shape.
**Fix.** Move the route line to the card header (it fits on MOD's wide cards), or show it on hover only for PLAY's small cards.

### V8-30. Low: the OUT meter label collides with the resize grip
At the bottom-right corner, "OUT" sits on the diagonal resize grip at 100 %, and is cut at the window edge at 75 % (`dx7-small/01-MAIN.png`).
**Fix.** Move the label above the meter, or inset the meter 14 px from the corner.

### V8-31. Low: the hover info bar covers content
The info line ("OSC 1 › Pitch  Pitch offset in semitones…") is drawn over the bottom 30 px of the SUB + NOISE card (`neuro/hover-line.png`).
**Fix.** Draw it in the chip-bar row or the header, or reserve its strip.

### V8-32. Low: "Off" for a depth, and raw curve names
On KEYS & VELOCITY:
- AMP MOD at 0 reads **"Off"**, but the conventions keep "Off" for switches.
- LOW and HIGH CURVE read **"-Lin"** (`fm-piano2/fm-1-keys.png`).
- SCALE KEY reads "A-1" (correct DX7, but the synth elsewhere writes C3 for middle C).

**Fix.** "0 %", and "−LIN / −EXP / +EXP / +LIN" (upper case, true minus). Keep the note names.

### V8-33. Low: the reverb has KEEP DRY and MIX
The built-in reverb card has a KEEP DRY toggle beside its MIX knob (`dx7/10-FX.png`). Review 7 #7 removed the double dry/wet from the card header; this pair is the same question inside the body.
**Fix.** Describe it in the label ("DRY ON / OFF at 100 % MIX"), or fold it into MIX's right-click menu.

### V8-34. Low: per-card CPU readouts
Every FX card header shows "CPU 0.1%" to "CPU 0.7%" (`neuro/10-FX.png`). It is useful in a debug build. For users, it is noise beside the effect name, and the header already shows CPU.
**Fix.** Show it on hover, or only above a threshold (> 2 %).

### V8-35. Low: oversized FM matrix cells on small patches
See V8-6. With three oscillators, each cell is 160×120 px for a 6 px dot. With six, the cells are 70 px. The grid's look changes with the patch.
**Fix.** Use a fixed cell size of about 70 px, centred.

### V8-36. Low: sub-tabs are hard to find
VECTOR is a sub-tab of PLAY, and PHYSICAL a sub-tab of OSC (top right, beside SCOPE). FILTER, FM, SEQ and FX have none, so the same slot holds different things or nothing. New features (VECTOR + EVOLVE, PHYSICAL PAGE in the tour) are one click deeper than their names suggest.
**Fix.** Show the sub-tabs left-aligned under the main tab they belong to, or as the page's first card-tab row, as on OSC (OSC 1 … OSC 3).

### V8-37. Low: four "MACRO 5-8 0%" knobs on every factory preset
The 8 macros are settled. But on every factory preset, four of them are generic "MACRO n 0%" knobs at full contrast beside four named ones (`neuro/01-MAIN.png`, `dx7/01-MAIN.png`).
**Fix.** Draw unassigned macros dimmed, with a "+" hint ("drag to assign"), so the named four read as the preset's controls.

### V8-38. Low: the duplicate FX card shows a BLEND pill
The old-patch duplicate card ("Duplicate OTT (not supported)") shows "BLEND 100%" in its header, the control review 7 removed from cards with a MIX (`neuro/fx-duplicate.png`).
**Fix.** Hide BLEND on the duplicate card.

### V8-39. Low: no filter on/off; a filter that does nothing is drawn at full strength
F1 and F2 have no switch. Filter 2 at 20 kHz Low Pass (Neuro, Init, every DX7 voice) is drawn and coloured like an active filter. Vital filters have a power button, and an off filter greys out.
**Fix.** Dim a filter card whose settings are a pass-through ("open at 20 kHz"). A real switch would be a new parameter, so append it (default on) if it is wanted.

### V8-40. Low: what does the dot after an LFO or ENV name mean?
"LFO 1 ●", "LFO 2 ●", "AMP ENV ●", "FILT ENV ●" carry a coloured dot after the name when the module is routed (`neuro/06-ENV-LFO.png`). The conventions reserve the on dot for things that can be switched. Here it means "routed", with no legend.
**Fix.** Drop the dot (the route pill beside it already says it is routed), or use a link glyph.

## Score against Vital

**7.5 / 10** (review 7: 6.5; review 6: 4.5).

**Why it rose.** Review 7's headline problems are fixed: one name for the operator envelope, ratio and level units, the node style, dead slots, scrolling pools, the double dry/wet, the two A/B systems, the dead SPACE macro, SEQ tabs, the remap header and module dimming. The workflow holds together:
- drag a chip onto a knob;
- a ring appears;
- drag the ring;
- rest on the knob for a card with live bars;
- the matrix lists the route with its source and destination.

That is close to Vital's.

**Why not higher.** Vital's score comes from three things this build still lacks in places:
- **Nothing on screen that the patch doesn't use.** V8-1 and V8-2 break this: DX7 cards on every patch, and two MSEGs.
- **Every control has one look, one name, one unit and one place.** V8-3 to V8-5 and V8-13 break this.
- **Panels that fill their frames without collisions.** V8-6 to V8-9 and V8-12 break this.

### The shortest list to 9.5
1. **Hide OP ENV / OP PITCH / OP LFO unless an oscillator uses the Operator Env (V8-1). Fold the patch MSEG into the LFO system, or hide it unless used (V8-2).** Removes the last bolted-on feel.
2. **Give the chip bar fixed positions** (LFO 1-4, ENV 1-4, performance sources, one MORE at a fixed x), built from the pools' list (V8-3).
3. **One operator widget**: the same control order on PLAY, OSC and FM, one OP ENV knob row shared by FM and MOD, and TRIM in one unit (V8-4, V8-5, V8-21).
4. **Fill or shrink the empty panes**: LFO/MSEG editor, FX, FM on small patches, VECTOR/EVOLVE, collapsed off-oscillator strips (V8-6, V8-17).
5. **Fix the collisions in one pass, with uitests**:
   - F1/F2 node fan-out (V8-9);
   - OP ENV ruler under the frame (V8-8);
   - grid layouts for DX7 algorithms (V8-7);
   - no squeezed text at 100 % or 75 % (V8-12).
6. **One "MODULE › CONTROL" formatter** that uses the knob's own label (V8-13). Add drop-target highlighting during a drag (V8-10).

With 1-3 done this is an 8.5. With 4-6 as well, and the low items swept, it reaches 9.5 against Vital.
