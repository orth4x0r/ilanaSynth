# UI review 7: ilanaSynth against Vital (cycle 2, fresh and adversarial)

Reviewer: a fresh agent, benchmark Vital (from knowledge of its UI; I did not run it). Evidence: the shots in `/home/user/shots7/` (1060x720 logical at 1.5x; `small` and `dx7-small` are 75 %), plus new renders in `/home/user/shots7/vital-extra/`:
- `fm-epiano/` and `fm-piano2/`: `ILANA_SNAPSHOT_FM=1` on DX7 "E.PIANO 1 (ROM1A)" (algorithm 5) and "PIANO 2 (ROM1A)" (algorithm 18). The last two shots in each folder are after the tool applies DX7 algorithm 1.
- `p1-neuro/`: `ILANA_SNAPSHOT_P1=1` on Neuro Wobble (knob rings, knob and macro cards, chip tray, matrix, remap).
- `pages-piano2/`: PLAY and OSC for PIANO 2.

I read the source in `src/gui/` and `src/PluginEditor.cpp` to confirm causes. Claims marked **(inferred)** are not confirmed in a shot or in the code. As the brief says, EDITED after `03-OSC` is a snapshot-tool artefact, and so is the large envelope or LFO pool in shots taken after the pool-reveal steps. I do not count either.

## Verdict

This is a much better build than review 6's. The worst items are gone. Knobs have draggable depth rings. The FM diagram stacks operators DX-style, and E.PIANO 1 now reads like a DX7 algorithm chart. All 32 DX7 algorithms are there with their numbers. The Operator EG has become an "Operator Env" with ms, seconds, dB and a draggable graph. PLAY has fixed slots. Steps live inside the LFO editor. MASTER reads 0 dB on every preset. The browser files DX7 voices by sound. The confirm dialog, Save As, the scope and the tutorial are all fixed.

What remains is mostly the second layer of the same problem: **one thing shown two ways**. The FM operator is the clearest case:
- Its envelope has four names: Operator EG, Operator Env, OP ENV and Op EG.
- Its LEVEL knob is a different parameter on PLAY than on FM, so PLAY shows "50 %" for six operators whose real levels range from −30.9 dB to 0 dB.
- Its ratio reads "x1.000" on one knob and "×1.00" in the header beside it.
- Its pitch EG and LFO are still edited only on FM. On MOD they appear only as link pills.

Other parallel systems that remain:
- MSEG is a single extra card in the LFO row, not a shape any LFO can use.
- FX cards have two dry/wet controls (a BLEND pill and a MIX knob).
- There are two A/B systems: header A|B and FX RACK A/B.
- The SEQ engine pills are tabs and on/off lights at the same time.

New layout problems:
- PLAY's fixed slots leave about 330 px of empty dashed boxes on a 3-oscillator patch.
- On algorithm 18 the stacked diagram shrinks, and arrows run over captions.
- The source chip bar reflows by preset and by zoom.
- Every DX7 voice ships with a SPACE macro that drives a reverb that is switched off.

## Fixed since review 6 / still open (review 6 Vital items)

| # | Review 6 finding | Status in this build |
|---|---|---|
| 1 | FM diagram hides OSC 1 and OSC 2 | **Fixed** for algorithm 5 (clean three-stack chart, `dx7/08-FM.png`). **Still weak** on deep algorithms: on algorithm 18 the nodes shrink to r≈15, and arrows cross captions (#3) |
| 2 | Depth dots instead of rings | **Fixed**: up to three draggable rings per knob (`ParamControls.h:660-733`). Badges remain beside the rings (#27) |
| 3 | DX7 PLAY shows wavetables and an unused AMP ENV | **Partly fixed**: CARRIER / MOD › n / OP ENV tags, RATIO knobs, AMP ENV greyed with a note and EDIT OP ENV. Still "Wavetable / Sine" combos, a FRAME knob on a sine, and LEVEL showing the trim rather than the operator level (#1, #2) |
| 4 | Operator EG: second envelope system, DX vocabulary | **Mostly fixed**: ATTACK…END in ms / s / dB, a draggable graph with a ruler, chips. Left: four names (#1); KEYS & VELOCITY still raw DX values (A-1, -Lin, 0-99) (#10); short segments impossible to grab (#9) |
| 5 | DX pitch EG / LFO: a fourth LFO behind a tab | **Partly fixed**: Hz / ms units, OP LFO and OP PITCH chips, pills on MOD. Still edited only on FM, still in the operator pill row (#5) |
| 6 | Algorithm grid knows 7 of 32 | **Fixed**: BASIC / DX7 1-16 / DX7 17-32, numbered, header "DX7 ALGORITHM 5". BASIC tiles are still unnamed and carry an unexplained "+1" (#14) |
| 7 | Folded SUB + NOISE leaks labels | **Fixed** (fold-to-fit is gone) |
| 8 | 1-16 rulers don't line up | **Fixed**. New: full pools scroll sideways and clip the last card (#17) |
| 9 | Chip codes, chips for unused envelopes | **Fixed** (no codes). New: group chips reflow by preset and zoom (#6) |
| 10 | Four names for one envelope | **Fixed** for the filter and amp envelopes (except casing: "Amp Env" in combos, AMP ENV on chips). The problem has moved to the Operator Env (#1) |
| 11 | Lorenz values over knobs | **Fixed** (`neuro/lfo-sim-lorenz.png`: two clean rows) |
| 12 | DX7 voices as a separate library | **Fixed**: filed under Keys / Bass / Lead…, Title Case names, bank chips. Left: a red "ROM1A" after every name and an "FM" tag on all 288 (#25) |
| 13 | FM lights OUT for off oscillators | **Fixed** ("OSC 3: OFF" column and OUT dimmed, `neuro/08-FM.png`) |
| 14 | FM matrix header mixes controls | **Fixed** (pair row for ring mod and sync). Noise colour is a NOISE row on FM, still not in SUB + NOISE (#30) |
| 15 | FB labels clipped in the 6x6 matrix | **Fixed** (↻ glyph, "FB 6 · 25%") |
| 16 | REED PICKUP caption collision | **Fixed** |
| 17 | Envelope tick labels collide | **Partly fixed**: DAHDSR names now sit on their spans. Ticks are still inside the plot, with an unexplained "+" (#23) |
| 18 | Filter node 2 and BALANCE | **Partly fixed**: BALANCE greyed in serial. The F1 / F2 nodes still overlap at 20 kHz, and the "MOD" ghost label collides with nodes (#18) |
| 19 | PLAY layout changes by preset | **Fixed**, but the fixed slots create large dead space (#4) |
| 20 | STEPS bound to a missing LFO | **Fixed** (Steps is an LFO shape) |
| 21 | Flat wavetable, 3D hidden | **Fixed** (WAVE / 3D / SPEC segmented, 3D default, frame readout outside the plot). 3D of a one-frame sine is noise (#31) |
| 22 | Matrix duplicates, inline remap, names | **Fixed** (docked remap, "Filter 1 › Cutoff"). Left: a column of identical "Aux: none" pills and the empty lower third (#20) |
| 23 | PHYSICAL dead end | **Fixed** (tab greyed) |
| 24 | Physical card half empty | **Partly fixed** (live string in the display). The EXCITER row is two knobs in a 900 px row (#32) |
| 25 | FX displays, palette, SOLO / BAND | **Fixed** (displays, labels, "in rack" handling, splitter groups). New: BLEND pill plus MIX knob (#7) |
| 26 | Macros silent | **Fixed** (hover card, amber "!"). But every DX7 voice's SPACE macro shows "!" (#8) |
| 27 | Master level swing | **Fixed** (0.0 dB everywhere) |
| 28 | Tutorial | **Fixed** (unticked, Ctrl+1-7, three bullets). The pill says "OPERATOR EG" (#1) |
| 29 | VECTOR live while off | **Partly fixed**: pad dimmed, "OSC 4 (none)". The X / Y / PATH RATE / WANDER knobs still look live (#21) |
| 30 | Arp / generate layout | **Fixed** (boxed GENERATE, dimmed lanes). New: engine pills with two meanings (#11) |
| 31 | Clip editor | **Fixed** (key column, bar ruler, velocity lane). Selected notes are still hard to tell apart **(inferred** from `gen-clip-selected.png`**)** (#38) |
| 32 | Heading dots look like LEDs | **Fixed** on page headings (underline style) |
| 33 | FM copy and units | **Partly fixed**: header "OSC 1 ×1.00". Ratio is "x1.000" on the knob; captions differ between the two diagram layouts (#12, #13) |
| 34 | PLAY envelope tabs vs pool | **Fixed** (tabs follow the pool, "+12" overflow) |
| 35 | Browser basics | **Fixed** (All = 659, IMPORT .SYX, sort) |
| 36 | Save As | **Fixed** (author, tags, comment, clear wording) |
| 37 | Scope cryptic | **Fixed** (VIEW WAVE / SPEC / BOTH, FREEZE, PEAK HOLD, floating panel) |
| 38 | Bare digit keys | **Fixed** (Ctrl/Cmd + digit, `PluginEditor.cpp:2575-2608`) |
| 39 | Static hint bar | **Fixed** (gone) |
| 40 | OSC header buttons look like tabs | **Fixed** (real buttons) |
| 41 | Operator pills look disabled | **Fixed** (coloured dots) |
| 42 | Greyed DETUNE on DX7 PLAY | **Fixed** on PLAY. The OSC page still shows UNISON / DETUNE 15 ct / BLEND / SPREAD for each operator (#16) |

Summary: 30 fixed, 10 partly fixed, 2 changed into a new problem (#8 → pool scrolling; #19 → dead space). None is untouched.

## Findings, worst first

### 1. High: the operator envelope has four names
**What's wrong.** One feature is called:
- "Operator EG" in the OSC card header ("plays its Operator EG (FM page)", `pages/OscPage.h:110`), the EDIT OP ENV tooltip (`pages/MainPage.h:193`) and the tutorial pill "DX7 BANKS + OPERATOR EG" (`TutorialOverlay.h:29-30`);
- "Operator Env" in the ENVELOPE combo and on FM (`FmInputPages.h:600`, `ParamInfo.h:418`);
- "OP ENV" on the PLAY tag, the button and the MOD pill (`MainPage.h:192, 967`; `FmOperatorInfo.h:104`);
- "Op EG" in PLAY's note "Not used: the OSCs play their Op EG" (`MainPage.h:818`);
- "Op Env" on the AMP ENV card tag "unused (Op Env)" (`dx7/06-ENV-LFO.png`).

Review 6 fixed exactly this problem for the filter envelope. It has come back on the newest feature.
**Vital's way.** ENV 1-6: one name everywhere.
**Fix.** Pick "OP ENV" (short) / "Operator Env" (long) and grep out every "EG". Add a uitest that scans visible text and tooltips for "Op EG" and "Operator EG".
**Evidence:** `dx7/01-MAIN.png`, `dx7/03-OSC.png`, `dx7/06-ENV-LFO.png`, `dx7/00-tutorial.png`.

### 2. High: LEVEL is two different parameters on PLAY and on FM
**What's wrong.** On an Operator Env oscillator, the FM card's **LEVEL** knob is the operator's output level (`oscN_eg_out`, "0.0 dB", "-15.8 dB"). The oscillator's own `_level` is renamed **TRIM** and moved to the KEYS & VELOCITY tab (`FmInputPages.h:698-713, 764-775`). PLAY and OSC still show `_level` labelled **LEVEL**, and it reads "50 %" (`MainPage.h:79`).

So in E.PIANO 1, PLAY shows six identical "LEVEL 50%" knobs. The real levels, shown on the FM diagram, are −30.9, −7.5, −15.1 and 0 dB. A user who turns PLAY's OSC 2 LEVEL down to soften the tine is moving the trim, not the modulation index they can see on FM. The units differ too: PLAY says % and FM says dB for the same word.
**Vital's way.** A knob has one name and one unit wherever it appears.
**Fix.** On PLAY and OSC, put the operator level (`eg_out`) in the LEVEL slot for Operator Env oscillators, in dB as on FM, and label `_level` TRIM there too (or hide it). Use `FmOperatorInfo::levelText` everywhere.
**Evidence:** `dx7/01-MAIN.png` vs `dx7/08-FM.png`; `vital-extra/fm-piano2/fm-1-keys.png` (TRIM 0.0 dB = PLAY's 50 %).

### 3. High: deep DX7 algorithms crowd the stacked diagram
**What's wrong.** On PIANO 2 (algorithm 18: 6→5→4 into 1, plus 3 and 2), the stack has four rows, so `operatorRadius()` drops to its 15 px floor (`FmDiagram.h:491-503`). At that size:
- the node label becomes a bare digit with "MOD" squeezed under it (`:318-324`);
- OSC 4's arrow into OSC 1 runs through OSC 1's caption ("×1.00" is overwritten);
- OSC 2's arrow crosses its own "-9.8 dB" caption;
- the "100%" depth labels sit on the arrowheads;
- OSC 3's feedback ring touches the arrow from OSC 2.

On algorithm 1, OSC 6's feedback ring overlaps the node, and the 2→1 stack floats in empty space on the left. The diagram is 310 px tall while the operator card below takes 340 px.
**Vital's way.** Not applicable (no operator view). Dexed draws every algorithm at one fixed node size.
**Fix.** Let the diagram take the height it needs (move ENVELOPE's graph into the card's right half, as it already is, and shorten the knob rows). Keep captions out of arrow paths: route arrows to the node's top centre and put captions on the far side of the arrow. Extend the uitest from "node circles don't overlap" to "arrows don't cross captions or labels".
**Evidence:** `vital-extra/fm-piano2/fm-1.png`, `fm-dx7-algorithm-1.png`.

### 4. High: PLAY's fixed slots waste a third of the left column
**What's wrong.** Neuro Wobble (two oscillators) shows OSC 3 off, a 100 px dashed "+ ADD OSC 4" box, then two more 100 px dashed boxes reading "OSC 5 not added" and "OSC 6 not added". That is about 330 px of empty frames in the most valuable column on the main page. The "not added" boxes have no button, so the user cannot act on them. Only the first empty slot can be added. On Felt Hammer Board it is four of seven boxes.
**Vital's way.** Three oscillator slots that are always real oscillators. There are no placeholders, and no second tier of slots you can't use yet.
**Fix.** Collapse the empty slots after the first into one 32 px "+ ADD OSC 4" row. Give the space back to the oscillator strips (taller displays) or to SUB + NOISE. Keep the order fixed, so the layout still doesn't jump between presets.
**Evidence:** `neuro/01-MAIN.png`, `keys/01-MAIN.png`, `small/01-MAIN.png`.

### 5. High: the operator pitch EG and LFO are still a parallel system edited on FM only
**What's wrong.** OP LFO and OP PITCH are now chips, and MOD shows them as small pills ("OP LFO ~ 5.4 Hz Sine → FM") on the LFO and ENVELOPES heading lines (`EnvLfoPages.h:1225`). But they are links, not pool cards. Clicking one leaves MOD for FM. There, PITCH & LFO is still the seventh pill in the *operator* row (`FmInputPages.h:303-316`), after "1 … 6", and it holds voice-wide settings. The OP LFO has its own SHAPE / RETRIG / RATE / DELAY set and its own "PITCH DEPTH / AMP DEPTH / PITCH SENS" routing knobs, rather than matrix rows. The pitch EG's knobs are named PITCH 1, PITCH 2, SUSTAIN, END, while the operator's are PEAK, MID, SUSTAIN, END.
**Vital's way.** Every LFO and envelope lives in one pool and is edited in one editor. Its routings are matrix rows.
**Fix.** Make OP LFO and OP PITCH real cards in the MOD pools that open in the normal LFO and envelope editor pane (DX-specific knobs underneath). Show PMD / AMD as read-only matrix rows ("OP LFO → all OP ENV oscillators › Pitch"). On FM, move PITCH & LFO out of the operator pill row into its own "VOICE" button beside ALGORITHMS. Use the same level names for both envelopes (PEAK / MID / SUSTAIN / END).
**Evidence:** `dx7/06-ENV-LFO.png`, `dx7/08-FM-pitch-lfo.png`.

### 6. High: the source chip bar reflows by preset and by zoom
**What's wrong.** `layoutChips()` folds the group with more chips first (`PluginEditor.cpp:806-866`). Neuro Wobble shows LFO 1 | LFO 2 | LFO 3. A DX7 voice adds OP LFO and OP PITCH, so the bar becomes "LFO 1 | LFO +2 ▾" and "ENV +1 ▾": FILT 2 ENV and LFO 2/3, the sources people actually drag, disappear behind a menu to make room for two DX-only chips. At 75 % the same DX7 voice shows "LFO 1 | LFO 2 | LFO +1 ▾" instead (`dx7-small/08-FM.png` vs `dx7/08-FM.png`). The tray opens at the far left of the window, not under its chip (`p1-neuro/p1-chip-tray.png`). Drag targets that move between presets break muscle memory.
**Vital's way.** Sources sit in fixed places (the LFO 1-8 and ENV 1-6 tabs), whatever the patch.
**Fix.** Fold the DX-only chips (OP LFO, OP PITCH) and MSEG / RANDOM first, into one "MORE ▾" chip, before folding any LFO or envelope that is in use. Anchor the tray under its chip.

### 7. High: FX cards have two dry/wet controls
**What's wrong.** Every FX card header has a "BLEND 100%" pill (the slot's `_mix`, a sideways-drag slider drawn like a button, `FxLibrary.h:363-403`; `FxPage.h:393`). Most bodies also have a MIX knob (Vowel 70 %, Drive 50 %, OTT 50 %, Reverb 25 %). Two controls set how much effect you hear, and nothing says how they combine. The pill looks like a toggle, and nothing tells the user to drag it.
**Vital's way.** One MIX per effect, as a knob, in the module.
**Fix.** Drop BLEND from cards whose effect has its own MIX (or hide the effect's MIX and keep one). If both must stay, label them "SLOT BLEND" / "WET MIX", draw BLEND as a thin slider with a fill, and show its tooltip on hover.
**Evidence:** `neuro/10-FX.png`, `neuro/fx-split.png`.

### 8. High: every DX7 voice ships with a dead SPACE macro
**What's wrong.** E.PIANO 1's reverb is switched off, yet macro 4 SPACE drives its MIX (badge 4 on a greyed card). The macro shows the amber "!" in every DX7 shot (E.PIANO 1, PIANO 2, all pages). The warning works, but the factory content triggers it on 288 presets. The user's first turn of a macro does nothing.
**Vital's way.** Macros on factory presets are wired to things you can hear.
**Fix.** For DX7 voices, have SPACE also switch the reverb slot on (macro-driven enable), or wire SPACE to a reverb that is on at MIX 0. Add a library test: no factory macro may target only modules that are off.
**Evidence:** `dx7/10-FX.png`, `dx7/01-MAIN.png`, `vital-extra/fm-piano2/fm-1.png`.

### 9. Medium-high: Operator Env graphs can't be edited when segments are short
**What's wrong.** The graph uses one time scale for a 10 s envelope (rulers "1.0 s", "10 s"). On OSC 6 of PIANO 2 (DECAY 1 = 73 ms, the rest 1 ms), three handles pile up in a 10 px corner on top of the "10 ms" label (`fm-dx7-algorithm-1.png`). On E.PIANO 1 the release dot sits on the "10 s" label. Level 0 reads "Off" while the others read dB ("MID -18.1 dB", "SUSTAIN Off", "END Off").
**Vital's way.** Envelope graphs zoom to fit and can be zoomed by hand. Handles never stack.
**Fix.** Use a log-time or zoom-to-content axis, or spread overlapping handles apart, as the filter graph should do too. Write "−∞ dB" or "silent", not "Off", for a level.

### 10. Medium: KEYS & VELOCITY is still raw DX7
**What's wrong.** SCALE KEY "A-1", LOW / HIGH DEPTH "0", VEL "2", AMP MOD "0" are 0-99 or 0-7 DX values. LOW CURVE / HIGH CURVE are ~70 px combos showing "-Line…" (truncated) (`fm-piano2/fm-1-keys.png`). Their labels sit 3 px lower than the knob labels beside them.
**Fix.** Show depth as dB/oct (as KEY LVL already does on plain operators), VEL as "dB at velocity 1", AMP MOD as %. Widen the combos or use "−LIN / −EXP / +EXP / +LIN" glyph pills. Align the labels.

### 11. Medium: SEQ engine pills are both tabs and on/off lights
**What's wrong.** ARP / EUCLID / PROB SEQ / CLIP in the PATTERN header carry a dot that shows whether the engine is on, and a fill that shows which one is being edited. In `neuro/09-ARP-SEQ.png` ARP is filled (selected) while ARP is off. In `gen-clip-selected.png` EUCLID has a lit dot but no fill, and CLIP has both. The real on switch is a separate toggle at the bottom left (ARP, CLIP). So there are two places that look like "ARP on".
**Vital's way.** Not applicable (no sequencer). The usual convention is a tab for view and a switch in the tab for power.
**Fix.** Put the power switch *inside* each pill (a small toggle on the left) and drop the bottom-left toggle. Or keep pills as plain tabs and show power only with the switch.

### 12. Medium: ratio written two ways, side by side
**What's wrong.** RATIO knob "x1.000" / "x14.00" (Latin x, three decimals, `ParamInfo.h:199`). Operator header and diagram "×1.00" / "×14.00" (multiplication sign, two decimals, `FmOperatorInfo.h:78`). DRIVE "4.0x" puts the x after the number (`ParamInfo.h:52`). All three appear on one screen.
**Fix.** "×1.000" everywhere (one formatter, three significant decimals for ratios), and "4.0×" for drive.

### 13. Medium: the two FM diagram layouts draw nodes differently
**What's wrong.** Stacked layout (DX7): "OSC 2 / MOD" inside the node, caption "×14.00 / -30.9 dB" to the right. Triangle layout (Neuro): "1 / OUT" inside, caption "0 st 80%" underneath, with the depth "30%" written mid-line and two drag dots on the line. Small stacked nodes switch to a bare digit (`FmDiagram.h:318-320`). The same operator looks different depending on which algorithm is chosen.
**Fix.** One node style: "OSC n" always, role under it, caption always to the right in the same two-line format (tuning, level).
**Evidence:** `neuro/08-FM.png` vs `dx7/08-FM.png` vs `fm-piano2/fm-1.png`.

### 14. Medium: BASIC algorithm tiles have no names and an unexplained "+1"
**What's wrong.** On the BASIC page, the nine tiles have no number or name. Four carry a "+1" in the corner (needs one more oscillator, I assume), with no legend. Neuro's routing (OSC 2 → OSC 1, both OUT) reads "CUSTOM ROUTING" although it is one modulator into one carrier. The DX7 pages have numbers, so BASIC is now the odd one out.
**Fix.** Name the tiles (B1-B9 or "2-op stack", "3-op chain"…), and show the name under the lit tile and on hover. Replace "+1" with "adds OSC 4" in the tooltip and a small "+osc" glyph. Match one-modulator patterns to the nearest tile.
**Evidence:** `neuro/08-FM.png`.

### 15. Medium: a wavetable bass is called an "FM operator"
**What's wrong.** `OscRole::isOperator()` is true for any oscillator in an FM route (`OscPage.h:55-62`). Neuro Wobble's OSC 1 card header reads "FM operator, a carrier (heard)". PLAY tags it CARRIER, and OSC 2 "OUT, MOD › 1". For a wavetable patch with a bit of FM, DX vocabulary takes over the oscillator's identity.
**Vital's way.** FM is a warp mode on the oscillator: "FM ← OSC 2". The oscillator stays a wavetable oscillator.
**Fix.** Use operator wording only when the tuning is Ratio or Fixed, or the oscillator is on the Operator Env. Otherwise say "FM from OSC 2" / "FM into OSC 1" in the same place.
**Evidence:** `neuro/03-OSC.png`, `neuro/01-MAIN.png`.

### 16. Medium: DX7 operators on OSC show wavetable and unison controls that do nothing
**What's wrong.** OSC page, E.PIANO 1, OSC 1: a giant 3D view of a one-frame sine, FRAME 0 %, WARP, SPECTRAL, UNI MODE, UNISON 1, a greyed DETUNE "15 ct", BLEND 100 %, SPREAD 50 %, CHORD. That is a dozen controls with no effect on an operator, and the operator's own Operator Env controls are not on this page at all (only an EDIT OP ENV button).
**Fix.** When an oscillator is an Operator Env operator, collapse SHAPE and UNISON into one folded row and show its envelope graph and RATIO / LEVEL there, matching FM.
**Evidence:** `dx7/03-OSC.png`.

### 17. Medium: pools that fill up scroll sideways and clip the last card
**What's wrong.** With more envelopes or LFOs, the MOD pool becomes a single horizontal strip with a 3 px scrollbar, and ENV 9 is cut in half at the right edge (`neuro/lfo-steps.png`). The PLAY LFO card does the same: LFO 4 is clipped (`p1-chip-tray.png`). Nothing says "8 more".
**Vital's way.** Fixed tabs (LFO 1-8) that always fit.
**Fix.** Wrap to a second row, or show a "+N" overflow card that opens a grid. Don't clip a card at the edge.

### 18. Medium: filter graph nodes still overlap, and "MOD" collides
**What's wrong.** With both filters open (DX7 voices, Felt Hammer Board), nodes 1 and 2 sit on top of each other at the right edge ("1 2" touching). The ghost "MOD" ring and label for F1's modulated cutoff is drawn wherever the LFO happens to be. In the shots it lands on node 1 ("1○MOD", `scope-panel.png`), on node 2 (`05-FILTER.png`) or on the 100 Hz tick (`01-MAIN.png`).
**Fix.** Fan coincident nodes apart (as review 6 asked). Draw the modulated position as a range bar on the curve in the filter's colour, without text.

### 19. Medium: MSEG is one extra card, not an LFO shape
**What's wrong.** Steps became an LFO shape. MSEG did not: it is a single card *after* the "+" in the LFO row, with its own panel (LOOP, RATE) and ~70 % empty space. There is only one MSEG, with no grid or snap (MSEG grid / snap is on the "not done on purpose" list). It is also the ENVELOPE option "MSEG run once per note", so one shape serves two roles.
**Vital's way.** Every LFO is an MSEG: draw any LFO's shape, with grid and snap.
**Fix.** At minimum, move the MSEG card before "+" and give it the same editor layout as the LFOs (SYNC, RETRIG, SMOOTH). Better: offer "Drawn" as an LFO shape that uses the MSEG editor (the drawable LFO shapes from review 4 already exist).
**Evidence:** `neuro/lfo-mseg.png`, `neuro/06-ENV-LFO.png`.

### 20. Medium: the matrix VIA column is twelve identical "Aux: none" pills
**What's wrong.** Every row has a full-size "Aux: none" pill in the VIA column. It is the same text on every row, and it is louder than the AMOUNT value. The page's lower third is still empty with 12 rows.
**Fix.** Show an empty "＋" in VIA until an aux is set. Show "Aux: none" only on hover. Let the rows grow taller (bigger amount sliders) when there are few.
**Evidence:** `neuro/07-MATRIX.png`.

### 21. Medium: VECTOR knobs look live while VECTOR is off
**What's wrong.** The pad dims and says "VECTOR OFF", but X, Y, PATH RATE, WANDER and WANDER RATE are at full contrast, and so is the PATH switch. The vector card also has a ~60 px empty band under its header. The corner combos say "Osc 1" while the pad says "OSC 1".
**Fix.** Apply the `EffectRules` dimming to the whole control block. Use "OSC 1" in the combos. Remove the empty band.
**Evidence:** `neuro/02-VECTOR.png`.

### 22. Medium: two A/B systems
**What's wrong.** The header has A | B (snapshot compare), and the FX page has RACK A / RACK B / COPY TO B (two stored FX chains, `FxPage.h:260-261, 1726-1728`). Same letters, different meanings, and nothing on the FX page explains RACK B except a tooltip.
**Fix.** Rename the FX one "CHAIN 1 / CHAIN 2", or "ALT CHAIN" with a switch. Or fold it into the header A/B so there is one compare system.
**Evidence:** `neuro/10-FX.png`.

### 23. Medium: envelope time ticks are still inside the plot, with an unexplained "+"
**What's wrong.** The ticks "100 ms", "+10 ms", "+1 s" sit inside the graph frame, under the release segment. The "+" (time after key up) is never explained. On PLAY, "+10 ms" and "+1 s" sit 30 px apart under the release dot (`keys/01-MAIN.png`).
**Fix.** Put the ruler below the frame. Label the release part "after release: 10 ms, 1 s", or draw a "KEY UP" divider as the Operator Env graph already does.

### 24. Medium: the AMP ENV graph on a DX7 voice is still drawn at full colour
**What's wrong.** On PLAY the AMP ENV knobs are dimmed with "Not used: the OSCs play their Op EG", but the graph beside them is drawn at full orange strength with its handles. On MOD the card says "unused (Op Env)", yet the editor below shows AMP ENV at full strength with "drag the graph or the knobs".
**Fix.** Dim the graph too. Better, when every oscillator is on the Operator Env, show the selected operator's Op Env graph in PLAY's ENVELOPE card (review 6 #3 asked for this).
**Evidence:** `dx7/01-MAIN.png`, `dx7/06-ENV-LFO.png`.

### 25. Medium-low: DX7 browser rows repeat the bank and the "FM" tag
**What's wrong.** With ROM1A selected, every row still ends its name with a red "ROM1A". Every DX7 row carries an "FM" tag, so 288 voices share a tag that tells you nothing. Tag chips (Evolving, Bright, Metallic, +23) share a row with bank chips (ROM1A…DEXED01), separated only by a thin divider.
**Fix.** Hide the bank suffix when a bank chip is active (or always, and show it in a dim column). Drop "FM" from the DX7 tag set (the category or bank says it). Give tags their own row or a "TAGS ▾" chip.
**Evidence:** `extras/extra-browser-dx7.png`.

### 26. Medium-low: PLAY strip knobs change order when an oscillator joins an FM route
**What's wrong.** A plain wavetable strip shows FRAME, LEVEL, SEMI, UNISON (`MainPage.h:68`). As soon as the same oscillator is in an FM route, it shows SEMI, LEVEL, FINE, FRAME (`:78`). In Neuro Wobble, OSC 1/2 and OSC 3 on the same page use different orders, and FRAME jumps from the first to the last column.
**Fix.** One order for every wavetable strip: tuning, LEVEL, FINE, FRAME. Swap only the tuning knob (SEMI / RATIO / FIXED).
**Evidence:** `neuro/01-MAIN.png`.

### 27. Low-medium: rings and badges both, and the badges still float
**What's wrong.** Knobs now have rings, but the old badges are still drawn beside them: macro "1" next to LFO 1 RATE, "4" next to SUB, "2" and a blue half-pie next to the FX MORPH knob, "3" next to Drive AMOUNT. The badges float outside the knob, often over the next label. That is two legends for one routing.
**Fix.** Keep the badges only on hover, or as a tiny number inside the ring's end. Drop the pie badges.
**Evidence:** `neuro/10-FX.png`, `neuro/01-MAIN.png`.

### 28. Low-medium: the amber "!" on a macro is a bare glyph
**What's wrong.** "SPACE !" and "TONE !" carry a 10 px amber exclamation mark (`MacroStrip.h:107-110`). Its meaning appears only after you rest on the macro and read its card.
**Fix.** Add a tooltip on the mark itself ("1 target is off: Reverb Mix"), or draw the macro's ring dimmed with a strike-through instead.

### 29. Low-medium: big dead areas on MOD, FX and PHYSICAL
**What's wrong.**
- The LFO editor's right pane holds six controls in ~780x300 px, and the bottom half is empty (`neuro/06-ENV-LFO.png`).
- MSEG's pane holds two controls.
- FX with three effects leaves ~250 px empty above OUTPUT (`neuro/10-FX.png`). With one effect (DX7) it leaves ~400 px (`dx7/10-FX.png`).
- The PHYSICAL page's right pane has an empty lower half.
- The OSC page spreads its rows (SHAPE / PITCH & LEVEL / UNISON) ~170 px apart.

Vital fills its panes or sizes them to their content.
**Fix.** Let the LFO graph take the full width, with the controls in a row under it. Size FX cards to their content and let OUTPUT follow the last card. Tighten the OSC rows to about 90 px.

### 30. Low: noise colour lives on FM, noise level on OSC / PLAY
**What's wrong.** The NOISE row with COLOUR sits at the bottom of the FM matrix, while NOISE (level) is in SUB + NOISE on PLAY and OSC. Review 6 #14 asked for one home. The noise became an FM source row, which is clever, but the colour knob went with it.
**Fix.** Show COLOUR in SUB + NOISE too (same parameter), or move it there and leave the FM row as a pure modulation row.

### 31. Low: 3D view by default even for a single-frame table
**What's wrong.** The OSC page shows "3D" for "Sine" on DX7 voices: 64 identical sine slices stacked in perspective, plus "FRAME 1 / 64". It is meaningless and takes the largest area on the page.
**Fix.** Default to WAVE when every frame is the same (or the table has one frame). Hide the frame readout then.

### 32. Low: the physical oscillator's EXCITER row is two knobs in a wide row
**What's wrong.** EXCITE (combo), EXCITE POS, HAMMER, then ~500 px empty. Review 6 #24 is half done.
**Fix.** Put EXCITER on the STRING row's second line or beside the combo.
**Evidence:** `keys/03-OSC.png`, `keys/physical-page.png`.

### 33. Low: PHYSICAL page duplicates the OSC card, and its BODY is a pair of links
**What's wrong.** The right pane is "the controls of its OSC card": the same knobs as OSC, in a different grid. BODY shows "RESONATOR BODY off → FILTER" and "SOUNDBOARD on, Dense → ACOUSTIC KEYS". These are status lines with jump buttons, so BODY is now in three places (FILTER, OSC › ACOUSTIC KEYS, PHYSICAL).
**Fix.** Make PHYSICAL a view-only enlargement of the string (or drop it, since the OSC card now has the live string), and give BODY one home.

### 34. Low: WEST and BODY cards look live while off
**What's wrong.** On FILTER, WEST is switched off but its FOLD / SYMMETRY / STAGES / DECAY / RESO / STRIKE / OPEN knobs and three combos are at full contrast. BODY dims only some of its knobs (MATERIAL and COUPLE dim; AMOUNT, DECAY, SIZE, OFFSET, KEY TRK don't).
**Fix.** One rule: a module that is off dims every control in it.
**Evidence:** `neuro/05-FILTER.png`.

### 35. Low: SIGNAL FLOW still floats a "BYPASS" label with a stray curve
**What's wrong.** The bypass branch is a thin curve that drops below the chain to a free-floating "BYPASS" word (`neuro/05-FILTER.png`).
**Fix.** Label the branch on the line ("bypass ↷") and route it above the filters.

### 36. Low: casing drifts between widgets
**What's wrong.** "Amp Env" / "Operator Env" in combos vs "AMP ENV" / "OP ENV" on chips and tabs. "Osc 1" in VECTOR combos vs "OSC 1" everywhere else. "semitones · Amp Env · OUT" in the FM card header (lowercase first word).
**Fix.** Upper case for every name of a module or source in labels, chips and combos. Sentence case only for prose.

### 37. Low: the filter's "F1 = F2" caption under BALANCE
**What's wrong.** It is greyed in serial mode (good), but "F1 = F2" is an equation, not a label. Is it the value? A mode?
**Fix.** Show the value ("50 %: even") and a tooltip.

### 38. Low: clip editor selection is hard to see (inferred)
**What's wrong.** In `gen-clip-selected.png`, every note has the same fill and outline, so the selected notes can't be told from the others **(inferred**: I could not confirm from the shot which notes the tool selected**)**.
**Fix.** Draw selected notes with a white outline and a brighter fill, and dim the others.

### 39. Low: the 75 % chip bar folds differently from 100 %
See #6. Separately, at 75 % the macro strip's "SPACE !" mark crowds the value text.
**Evidence:** `dx7-small/08-FM.png`.

### 40. Low: the remap header says "ROW 1 (slot 1)"
**What's wrong.** Two numbering systems for the same row, side by side ("REMAP ROW 1 (slot 1)"). After a sort they will differ, which is worse.
**Fix.** Say "REMAP · LFO 1 → Filter 1 › Cutoff". The source and destination are what the user recognises.
**Evidence:** `remap/remap-editor.png`.

### 41. Low: the KEY UP marker collides with text
**What's wrong.** On the Operator Env graph, the dashed KEY UP line runs through "C3, velocity 100". On the pitch EG graph it runs through the sentence "No pitch movement: drag a point up or down".
**Fix.** Put the KEY UP label on its own line under the corner caption, and the hint text below the plot.
**Evidence:** `dx7/08-FM-pitch-lfo.png`, `dx7/08-FM.png`.

### 42. Low: the duplicate-FX path is still offered
**What's wrong.** One instance per type is a settled decision (and matches Vital, which has each effect once). But the palette still lets you add a second OTT, and the result is a full-size card explaining that it isn't supported (`neuro/fx-duplicate.png`).
**Fix.** Disable types already in the rack in the ADD EFFECT menu (with "already in slot 4" as the tooltip). Keep the explanatory card only for old patches that contain duplicates.

### 43. Low: "DICE FX" and the header dice
**What's wrong.** The header has a dice icon (new random patch), and the FX page has "DICE FX". The tutorial says "the dice rolls a fresh patch". The word "DICE" as a verb is opaque, and the two buttons don't look related.
**Fix.** Use the same dice icon with "FX" (the dice icon followed by "FX") and the tooltip "Randomise the FX chain".

## Score versus Vital

**6.5 / 10** (review 6: 4.5). The structural complaints are largely fixed: modulation rings, the stacked DX diagram, all 32 algorithms, the Operator Env in the synth's units, a fixed PLAY, Steps inside the LFO, one source table, master at 0 dB, DX7 voices in the library. It now feels like one synth with an FM engine rather than a synth with a DX7 bolted on. What keeps it from Vital's 8+ is consistency, not features:
- the operator is named and measured differently on each page (#1, #2, #12, #13);
- the operator pitch EG / LFO and MSEG are still parallel systems (#5, #19);
- there are double controls (#7, #22, #11);
- the chip bar moves under the user's hand (#6);
- the new fixed layouts leave large empty boxes (#4, #29).

Fix #1-#8 and this is a 7.5.
