# UI review 13 (pass 8): ilanaSynth v1.3 against Vital, Serum 2, and itself

One reviewer, three angles, one file. Build: branch `claude/project-thread-smvgfk`, `/home/user/ilanaSynth/build`, head `570e42d`. Evidence: `/home/user/shots13/{neuro,init,dx7,keys,small,dx7-small,extras,remap,confirm}/` (cited as `set/file`). I read about 60 of those shots at full size and the source in `src/gui/` and `src/PluginEditor.cpp` to confirm causes (file:line where I have it). I could not click or drag, so live behaviour is marked *(inferred)*. The snapshot tool edits the patch on the OSC page, so "EDITED" after 03-OSC and the odd patch state in the focused sets (`osc-*`, `fx-*`) are tool artefacts, not findings.

`ilanaSnapshot --uitest`: `UI TESTS PASSED (0 failures)`. The test is green while `neuro/fx-duplicate` shows cut text ("Remove it, or kee...", V13-2), so it does not cover state-dependent cards. That is itself a finding (V13-20).

## Verdict and scores

Review 12's fix round landed well, and the owner's PLAY redesign is a real step up: big waves, menus row, six knobs, SUB shape as pills and strips that fill the height make PLAY the best page in the product. It reads as a synth now, not a form. The FM matrix fills its card with a route readout and an EXTRAS bar, the SEQ controls moved into a column beside the lanes, the clip row has three groups, the DX7 operator levels show on PLAY, the OSC drawer hides STRINGS and SOUNDBOARD on a DX7 voice, VOICE LEVEL is gone from operator pages, and the SEQ chain order matches the tabs. Nothing the round did made a page worse that I could find.

What is left is smaller than last time, and it is of four kinds:

1. **The new PLAY layout is tuned for the two-oscillator case.** With one open strip the knobs hang in the middle of a 290 px strip (`init/01-MAIN`); with five or six they lose WARP, DETUNE and the third menu and SUB's shape pills become "Sin / Sqr / Saw" (`init/added-osc-MAIN`). The PATCH tile and the big output view that the old column ended with are still in the source but can no longer appear (`MainPage.h:1485`), so PLAY has no output instrument at all.
2. **The FX rack still has holes.** Natural card widths wrap into rows that do not fill: a lone CRUSH card leaves a 700 x 270 px hole beside it (`neuro/fx-03`); FREEZE is a 620 px card with an empty left half (`neuro/fx-12`); the duplicate-effect card cuts its own sentence (`neuro/fx-duplicate`).
3. **Chrome that has not been brought up to the rest.** The OUT meter is still the smallest instrument on the screen and the header status pills are 17 px tall grey text; the VECTOR switch breaks the written convention; casing differs between chips and pills; the remap tiles still say "EASE O...".
4. **One thing, two names.** DX7 operators are "OSC n" on PLAY, "OPERATOR n" on FM and "operators" in the envelope card; the OSC picker's tag says the oscillator's role on one patch and its engine on another; Airwindows cards show unit-less 0 to 100 % where the built-in twin shows ms.

| Angle | Review 11 | Review 12 | Review 13 | Target |
|---|---|---|---|---|
| Against Vital | 8.7 | 8.9 | **9.0** | 9.5 |
| Against Serum 2 | 8.6 | 8.8 | **8.8** | 9.5 |
| Integration | 8.9 | 9.1 | **9.1** | 9.5 |

Why the scores moved so little: the fixes were real (+0.1 to +0.2 each), but a fresh adversarial pass on a redesigned page turns up as many new items as the old ones it closes, and the Serum gap is in depth (sample editor, matrix density, sound-design browsing), which this round did not touch.

## What stands between this build and 9.5

The shortest list I can defend (each one is small; together they are the gap):

1. **Make the FX rack a real grid.** Two columns, every card half width or full width, display-less cards (FREEZE, AIRWINDOWS) fill their half with the algorithm picker large; no lone card, no hole; fix the duplicate card's sentence (V13-1, V13-2).
2. **Make the PLAY strips robust at both ends.** One open strip: centre or enlarge the knob row; five or six strips: keep the same six controls (drop the picture first, not WARP and DETUNE); spell out Sine / Square / Saw (V13-3, V13-4).
3. **Give the output a proper instrument and the header proper controls.** A 16 px stereo meter at 160 px or wider, away from the resize grip; BPM, VOICES and CPU as 24 px pills with real contrast (V13-5, V13-6).
4. **One vocabulary for the operator voice and one rule for the OSC picker's tag**, plus units on Airwindows knobs (I13-1 to I13-3).
5. **Close the dead-space panels that remain:** the FILTER page's two 470 px panels at three oscillators, PHYSICAL's 270 px floor and empty BODY box, the 720 px VECTOR pad when off, the OSC page's 75 px floor (V13-9 to V13-12, S13-2).
6. **Serum's depth in three places:** a denser matrix (36 px rows with group tints), a sample page with a velocity/key map and a drop zone that looks like one, and audition in the preset browser (S13-1, S13-3, S13-4).
7. **Turn UI-CONVENTIONS into tests.** The casing rule, the switch-at-right rule and "no cut text in a state-dependent card" are all broken today with the test green (V13-20).

Do those seven and I would score 9.4 to 9.5 on all three angles. Without 3 and 4 the build stays at 9.2 at best.

## Review 12's items: fixed since review 12 / still open

### Vital (V12-1 to V12-30)

| # | Review 12 item | Status |
|---|---|---|
| V12-1 | PATCH tile hidden at 5 to 6 operators or oscillators | **Fixed by the redesign**: all six strips fit without a scroll bar (`dx7/01-MAIN`, `init/added-osc-MAIN`); the PATCH tile itself can no longer appear (V13-5) |
| V12-2 | Strips 190 px with controls hung from the top | **Half fixed**: Neuro (two strips) is balanced; Init (one strip) is 290 px with a 60 px gap under the menus (V13-3) |
| V12-3 | Card-interior test passes on blanks | **Still open** (V13-20) |
| V12-4 | FX rack ragged edge and a hole | **Half fixed**: the hole beside a lone third card now holds a dashed ADD EFFECT card; with a wide first card the hole returns (V13-1) |
| V12-5 | SEQ rows left-heavy | **Fixed**: controls in a right column beside the lanes; the column's pairing is untidy (V13-7) |
| V12-6 | FM matrix dead bands | **Fixed** (`dx7/08-FM`: cells fill, readout line, full-width EXTRAS bar) |
| V12-7 | SUB + NOISE never folds | **Fixed** (`dx7/01-MAIN`: folded to one line, "Sine . sub off, no noise") |
| V12-8 | "OSC 4 (not added)" | **Fixed** ("OSC 4: none", `neuro/02-VECTOR`) |
| V12-9 | OUT meter smallest instrument | **Still open** (V13-5) |
| V12-10 | BPM / VOICES / CPU unequal, two baselines | **Half fixed**: one baseline, one pill style; widths and gaps still unequal (V13-6) |
| V12-11 | Scope squiggle reads as an underline | **Fixed**: it carries an "OUT" tag (`neuro/01-MAIN`) |
| V12-12 | Mod rings crowd FRAME value | **Open on compact strips** (`init/added-osc-MAIN`: "50%" under the rings); fine on roomy strips |
| V12-13 | DX7 ENVELOPE card a graph and a paragraph | **Fixed**: operator level bars (`dx7/01-MAIN`) |
| V12-14 | Strip content depends on strip count | **Half fixed**: a tooltip names the dropped warp, but the controls are still dropped (V13-4) |
| V12-15 | COLOUR reads disabled | **Still open** (V13-15) |
| V12-16 | 3D hint drops "up / down: warp" | **Fixed** ("drag: frame . warp", `neuro/03-OSC`) |
| V12-17 | Remap tiles truncate, matrix cut mid-row | **Half fixed**: matrix now ends on a row; "EASE O..." still cut (V13-19) |
| V12-18 | Informative text at the 9 px floor | **Still open (inferred)**: hints on the FM operator card and tags on strips are 12 units (9 px at 75 %) |
| V12-19 | VECTOR pad 720 px square with one dot | **Still open** (V13-10) |
| V12-20 | SAVE AS warns after typing; TAGS typed and clicked | **Half fixed**: the dialog is tidy (`extras/extra-save-as`); tags are still both a text field and chips (S13-9) |
| V12-21 | FX header buttons three styles; AIRWINDOWS button/tag | **Mostly fixed**: the card header is one BUILT-IN / AIRWINDOWS segment; the empty-rack picker's buttons vary 95 to 115 px (V13-17) |
| V12-22 | "+13" overflow shows dots, no names | **Fixed**: "+13 v" opens a list (`neuro/07-MATRIX`) |
| V12-23 | BYPASS label crosses a connector | **Still open** (`dx7/filter-flow-busy`: the label sits on the F1 line) |
| V12-24 | PHYSICAL empty state is a card and 270 px of floor | **Still open** (V13-9) |
| V12-25 | LFO chips mix three label styles | **Still open** (`neuro/06-ENV-LFO`: route pills, rate and "OUT 2" in one bar) |
| V12-26 | Filter 2 has no visible switch | **Still open** (V13-11) |
| V12-27 | Filter response caption neither sentence nor fragment | **Fixed** ("cutoff across, resonance up and down") |
| V12-28 | Two numbers for FRAME | **Not re-checked** (needs a live patch) |
| V12-29 | Tutorial key text | **Fixed** (`neuro/00-tutorial`: Ctrl 1-7 tabs, Ctrl Shift 1-3 pages, gear: settings) |
| V12-30 | "+ ADD OSC 4" two forms | **Still open** (128 px pill on OSC, full-width row on PLAY; V13-18) |

### Serum 2 (S12-1 to S12-15)

| # | Review 12 item | Status |
|---|---|---|
| S12-1 | OSC rows on one grid, 70 px floor | **Half fixed**: on the grid; the floor is still 75 px on a wavetable oscillator (`neuro/03-OSC`) (S13-2) |
| S12-2 | Left picture two jobs, hidden gesture | **Fixed**: frame scrubber under the wavetable, "drag: frame . warp" caption |
| S12-3 | Clip bar one row of ten | **Fixed**: three groups (`neuro/gen-clip-expanded`); IMPORT MIDI and EXPORT MIDI touch (S13-8) |
| S12-4 | SEQ chain order differs from tab order | **Fixed** (KEYS > STRUM > EUCLID > CLIP > VOICES, `gen-clip-expanded`) |
| S12-5 | "Auto" and "Random" in value slots | **Not re-checked** (EXCITE POS reads "Auto" in `dx7/physical-page`, still a value-slot word) |
| S12-6 | Browser: name sort, four pills per row | **Fixed**: one category pill per row, tags on the selected row only (`extras/extra-browser-docked`); SORT is a menu (inferred) |
| S12-7 | Sample page: no drop text, no velocity map | **Half fixed**: "Drop a .wav or .sfz here, or LOAD..." is there; no velocity or key map (S13-1) |
| S12-8 | "FM FROM OSC 2" a grey link | **Fixed**: a blue pill in the card title (`neuro/03-OSC`) |
| S12-9 | Matrix without grouping | **Still open** (S13-3); rows are tinted by source, which helps |
| S12-10 | Macro names are not what they do | **Half fixed**: route counts ("TONE -> 2"); the bare "OFF" tag is new (S13-10) |
| S12-11 | DRIVES list repeats the matrix | **Left as is** (it is a useful per-source view); absent on physics LFOs (S13-6) |
| S12-12 | Table browser HEAR far from the tile | **Still open** (`neuro/table-browser`: HEAR is top right of a 1,100 px panel) (S13-7) |
| S12-13 | DX7 PLAY card says "OPERATOR" over "Sine" | **Fixed** ("WAVE" over the combo) |
| S12-14 | SOLO a bordered grey box | **Fixed**: a circular S toggle (`neuro/10-FX`) |
| S12-15 | Empty FX help sentence 13 px, two lines | **Fixed** (`neuro/fx-empty`: a heading and one sentence) |

### Integration (I12-1 to I12-18)

| # | Review 12 item | Status |
|---|---|---|
| I12-1 | DX7 operator has two level controls | **Fixed** (`dx7/03-OSC`: OUTPUT only) |
| I12-2 | Two kinds of operator pill row | **Fixed** (FM operator card uses the one picker, `dx7/08-FM`) |
| I12-3 | PLAY role line a word meaning two things | **Fixed**: "TO OUTPUT" / "MODULATES 1" and DEPTH vs OUTPUT |
| I12-4 | Physical page not on the engine-switch grammar | **Half fixed** (BODY and SOUNDBOARD switches now both right; the BODY box is empty when off, I13-5) |
| I12-5 | SOUNDBOARD's EDIT link inside its sub-box | **Half fixed**: it is a bordered button beside the switch in the sub-box (`dx7/physical-page`), not a card-title link |
| I12-6 | Drawer offers STRINGS and SOUNDBOARD on a DX7 voice | **Fixed** (`dx7/03-OSC`: VOICE, SUB + NOISE, SPREAD & DRIFT) |
| I12-7 | Airwindows knob names | **Still open**, and worse in view of units (I13-3) |
| I12-8 | "OUT" three meanings | **Half fixed**: PLAY says "TO OUTPUT"; the meter and the LFO "OUT 2" tag keep "OUT" (I13-9) |
| I12-9 | Algorithm title and 1 to 9 tiles | **Half fixed**: "2-OP STACK, EDITED"; tiles still numbers (S13-5) |
| I12-10 | Physical strip combos | **Not re-checked** |
| I12-11 | DX7 shows SUB + NOISE at full height | **Fixed** (folded) |
| I12-12 | DX7 PLAY filter shows a flat response | **Still open** (`dx7/01-MAIN`) (I13-8) |
| I12-13 | F2 silent when off by value | **Still open** (V13-11) |
| I12-14 | OP LFO / OP PITCH chips look normal | **Still open** (I13-10) |
| I12-15 | patch / preset wording | **Half fixed**: browser and dialogs say preset; menus still say "Init patch", "Random patch" (I13-14) |
| I12-16 | Vector title vs "OSC 4: none" | **Fixed** |
| I12-17 | "6 oscillators" vs "6 operators" | **Still open** (I13-1) |
| I12-18 | OSCILLATORS / OSC / operators | **Still open** (I13-1) |

---

# Part 1: Against Vital

What I measure against: every panel full and balanced, one control style, one text size per role, no state that appears only sometimes, a master meter you can read at a glance.

**V13-1. Medium-high: the FX rack still leaves holes and ragged rows; a card without a display looks empty.**
Cards take a natural width and wrap. With CRUSH (770 px) first, DRIVE and OTT cannot join it, so row 1 is one card and a 700 x 270 px hole (`neuro/fx-03`). With a display-less card the left half is blank: FREEZE is a 620 px card with a lone HOLD switch floating at x = 200 and MIX at 500 (`neuro/fx-12`); the AIRWINDOWS WIDENER card keeps an empty display slot with ALGORITHM at its foot (`neuro/fx-41`). Right edges end at x = 1525, 1470 and 1548 depending on the row (`neuro/10-FX`, `fx-35`). Vital's FX are fixed-width tiles in a column; Serum 2's are a vertical strip. *Fix:* a two-column grid, each card half or full width by type (a display card full width only if it needs it), the card's extra width given to its display, not left blank; let the wide cards be a half and let their knobs wrap. At minimum fill the right of row 1 by letting the next card move up (a first-fit pack).

**V13-2. Medium-high: the duplicate-effect card cuts its own sentence.** "The rack keeps one set of settings per effect, so this slot runs slot 1's OTT again. Remove it, or kee..." (`neuro/fx-duplicate`; `FxPage.h:1537-1549`: two lines, 100 px trimmed from the right for REMOVE). It is a long message that appears in a state the UI test never paints. *Fix:* "Duplicate OTT: it shares slot 1's settings. REMOVE it, or keep it for the same sound." in one 12 px line plus the button, or let the message use the whole card width above the button.

**V13-3. Medium: the PLAY strip with one open oscillator leaves the knobs floating.** `init/01-MAIN`: the strip is 290 px; the menus row sits at the top, then a 60 px gap, then six 48 px knobs, then 40 px of floor. Neuro's two 215 px strips are balanced (`neuro/01-MAIN`). The SUB + NOISE card is the same 295 px (`MainPage.h:530` slot height shared by the open strips and SUB) with a 330 x 245 px picture of one square wave. *Fix:* when the strip is taller than about 220 px, give the knobs the larger size (`KnobSize::main` is 48; the picture could take the extra, not the controls), or centre the knob row in the leftover height; draw SUB's picture at the strip's usual width.

**V13-4. Medium: five or six strips are different strips (and "Sin / Sqr / Saw").** The compact strip has four knobs (SEMI, LEVEL, FRAME, UNISON) and two stacked menus; WARP, DETUNE and the warp menu are gone, with only a tooltip line "(set on OSC)" (`MainPage.h:1300-1330`, `init/added-osc-MAIN`). The same screen shows SUB's shape as "Sin / Sqr / Saw" while the roomy SUB card says "Sine / Square / Saw" (`MainPage.h:120`, `neuro/01-MAIN`). The mod rings crowd the "50%" under FRAME (`init/added-osc-MAIN`). Vital has one oscillator panel at one size. *Fix:* the compact row keeps six controls by dropping the picture to 70 px; use the full words (the pills have the width); put the FRAME ring colours inside the dial, not over the value.

**V13-5. Medium: the OUT meter is still the smallest instrument, and PLAY lost its output view.** The meter is a 110 x 38 px box with two bars about 8 px tall and "OUT -- dB" (`PluginEditor.cpp:1446-1451`, `OutputMeter.h:36-60`; `neuro/01-MAIN`), beside a 108 px master knob, with the resize grip against its right edge. Review 12 said the same (V12-9). The PATCH tile and OUTPUT view code in `MainPage.h:1485` and `:557-570` shows only when 112 px of spare height is left, which the new strips never leave *(inferred from the layout math and every PLAY shot)*, so it is dead code, and PLAY has no signal-flow or output picture. Vital's output meter is a tall stereo bar at the far right of the header. *Fix:* a 160 x 36 px meter with 14 px bars and the dB scale; move the grip clear; delete or revive the PATCH/OUTPUT code.

**V13-6. Medium: the header status pills are small, grey and unevenly placed.** BPM (112 px) sits under the preset name, VOICES (175 px) and CPU (100 px) at the far right with a 115 px gap between them (`neuro/01-MAIN`; `PluginEditor.cpp:1293, 1425`: the VOICES hit box is 17 px tall). The text is `text3` grey on a dark pill, the lowest contrast in the header, yet VOICES is a menu and BPM a control *(inferred)*. *Fix:* three equal 24 px pills at one gap, `text2` text; the preset's category and "EDITED" tag keep the caption line.

**V13-7. Medium: SEQ's control column pairs unlike controls.** The six ARP controls sit in three rows: RATE (a menu) beside STEPS (a knob), GATE (knob) beside MODE (menu), OCTAVES beside CHANCE (`SeqPage.h:481-507`, `neuro/09-ARP-SEQ`). The menu and the knob in one row sit on different baselines (RATE's box at y = 252, STEPS' dial at y = 270; MODE at 421, GATE at 445). PROB SEQ's single column is tidy (`neuro/gen-probseq`). The GENERATE boxes below have empty floors under SNAP TO KEY and STRUM. *Fix:* menus in one row at the top (RATE, MODE), knobs in a four-up grid below; one baseline per row.

**V13-8. Medium-low: the PLAY page cannot show what the oscillators do together.** Neuro's strips show each wave, but nothing shows the mix (the signal flow lives on FILTER only). Vital's oscillator panels share one mixed scope view; Serum's too. *Fix:* when SCOPE is closed, give the empty header scope strip (14 px tall) a hover to show the OUT waveform larger, or show the mixed wave as the third picture in a FILTER-like "PATCH" card when one oscillator is added *(this overlaps V13-5)*.

**V13-9. Medium-low: the PHYSICAL page's empty state still leaves 270 px of floor, and its off BODY box is empty.** `neuro/04-PHYSICAL`, `keys/04-PHYSICAL`: the card ends at y = 685 of 960. On a physical oscillator (`dx7/physical-page`) the BODY sub-box is a 160 px dark rectangle with only a switch while SOUNDBOARD beside it is full. *Fix:* show the string larger (it is a nice picture) and let the card grow; an off BODY box should show its type and mix greyed, as an off FX card does.

**V13-10. Medium-low: the VECTOR pad is a 720 px square that says "VECTOR OFF" with one dot, and the switch breaks the written rule.** `neuro/02-VECTOR`. The three boxes at the right are 270 px of controls. `FilterVectorPhysicalPages.h:378-392` puts the switch after the title (a V11-24 decision), against UI-CONVENTIONS ("a card's own module: the switch sits in the card header, at the right"). Either the convention or the page is wrong. *Fix:* follow the convention (switch at the right of the header, like every other card) and shrink the pad to 480 px with the boxes beside it wider, or update the convention with the exception.

**V13-11. Medium-low: Filter 2 has no visible switch and says nothing when it is off by value.** `neuro/05-FILTER`: FILTER 2's card is dimmed (cutoff 20 kHz, reso 0 %) with only a dot in the title, while WEST and BODY have switches at the right. Same as V12-26/I12-13. *Fix:* a switch in the F2 header, with the type menu reading "Low Pass (off)" when bypassed.

**V13-12. Medium-low: the FILTER page's two top panels are 470 px tall and mostly empty at three oscillators.** `neuro/05-FILTER`: SIGNAL FLOW has seven nodes in a 600 x 470 px box; BALANCE floats at the foot. *Fix:* cap both at 340 px and give the rest to the cards below (which would then not need to be so squat), or put the PATCH view's output meter in the empty lower half of SIGNAL FLOW.

**V13-13. Low-medium: the FM diagram labels cram beside the nodes.** `neuro/08-FM`: "0 st / 80 %" at x = 203 sit against the arrow head from OSC 2; "-12 st / 55 %" at 488. The diagram is 880 x 300 for two nodes. *Fix:* put the numbers under the nodes (the DX7 view does, `dx7/08-FM`).

**V13-14. Low-medium: a header caption that is a hint.** `EnvLfoPages.h:677`: "drag the graph or the knobs; a segment's dot bends it, CURVE bends all" in the card header is neither a fragment nor a sentence. UI-CONVENTIONS: captions are lower-case fragments, hints are sentences below or beside a control. *Fix:* "drag the graph or the knobs" in the caption; the rest as a tooltip on the graph.

**V13-15. Low: COLOUR on SUB + NOISE reads as disabled even when it is 100 %.** `neuro/01-MAIN`: grey ring, grey label (`MainPage.h:119-127`: NOISE and COLOUR use `text2`), while SUB is orange. Same as V12-15. *Fix:* the noise knobs take the SUB colour at half strength, or both stay grey and COLOUR shows its ring filled.

**V13-16. Low: overflow chips with no hint of what they hold.** The DX7 envelope tab bar ends in "+1" (`dx7/01-MAIN`), the MOD pool in "9 MORE v" (`neuro/env-pool-full`), the bottom chips in "+13 v". Three vocabularies for the same idea. *Fix:* "+1" becomes "FILT 2 ENV" if it fits (the bar has 70 px free at the left in `dx7/01-MAIN`) and one word for the others ("MORE").

**V13-17. Low: AIRWINDOWS buttons in the empty-rack picker vary from 95 to 115 px and start at x = 520 to 525.** `neuro/fx-empty` (TAPE, CONSOLE vs DRIVE, CRUSH). *Fix:* one fixed width, one left edge.

**V13-18. Low: "+ ADD OSC n" is a 128 px pill on OSC and a full-width row on PLAY.** `neuro/03-OSC` vs `neuro/01-MAIN`. Fine in itself; but the OSC page's pill is dim grey while PLAY's is the same dashed grey; neither says it is a button until hovered *(inferred)*.

**V13-19. Low: remap quick-shape tiles still cut "EASE O...".** `remap/remap-editor`: tiles are 55 px wide; "DEAD" and "STEPS 4" fit, "EASE OUT" does not. The old three-line description under the curve is also three sentences of 12 px grey. *Fix:* tiles 66 px or two-line labels.

**V13-20. Low-medium: the UI test is not an enforcement of the conventions.** It passes while: the VECTOR switch is on the wrong side (V13-10), a state-dependent card cuts text (V13-2), chips mix casing (V13-21), the SUB pills use two spellings (V13-4). *Fix:* add four checks: (a) a card's on switch is in `cardSwitchBounds`; (b) paint the duplicate card, off-state cards (BODY off, F2 off) and the AIRWINDOWS cards through the fitted-text probe; (c) all chip/pill labels that name a module are upper case; (d) one checked spelling per ChoicePills set between the roomy and compact variants.

**V13-21. Low: casing of chips and pills.** UI-CONVENTIONS says module names in labels, chips, tabs and combos are upper case. The env chips say "Amp" and "Filter 1" (`neuro/06-ENV-LFO`), the LFO chips "FILTER 1 > Cutoff +2 more" (a destination path in mixed case, deliberately), the SUB pills "Sine / Square / Saw / -1 Oct", the FX cards' ALGORITHM menus "Tape Delay 2". Everything else is upper case. *Fix:* upper-case the module word in the route chips ("AMP", "FILTER 1"), keep parameter names in sentence case; decide that engine/shape pills are values (sentence case) and write it down.

---

# Part 2: Against Serum 2

What I measure against: depth where the sound is made (oscillator, wavetable, sample, matrix, browser) and the ability to see and audition what you are editing.

## Workflow speed (the same five jobs)

| Job | Clicks here | Serum 2 / Vital | Note |
|---|---|---|---|
| Pick a wavetable for OSC 1 | 1 (the table menu on PLAY) or 2 (OSC, table browser) | 2 | **Faster** than either; the table menu on the PLAY strip is a good move |
| Route LFO 1 to cutoff | 1 drag (chip onto a knob) | 1 drag | Same; the hover popup lists existing routes (`extras/extra-spec-and-card`) |
| Change a route's curve | 2 (MATRIX row's curve, then the remap editor) | 2 | Same; the remap editor is excellent (`remap/remap-editor`) |
| Add a reverb | 2 (FX, + ADD EFFECT) | 2 | Same |
| Find a bass sound | 1 click, then filter chips and search | 1 | Same; no audition without loading |

## Findings (Serum 2), worst first

**S13-1. Medium-high: the sample page is a form with a black box.** `neuro/osc-sample-empty`: the picture area is a 440 x 440 px black rectangle with grey text "Drop a .wav or .sfz here, or LOAD..." that reads as a disabled panel; no waveform ruler, no start/end markers on the (empty) area; TUNED, LOOP and REVERSE are switches with no value line under them, so row 1 has two heights (switches at y = 285 and knobs at 310); no velocity or key range map (Serum 2's sample oscillator and Vital both show one). *Fix:* a dashed border and a "LOAD..." button in the middle of the box; once a sample is loaded, show start/end/loop markers on the wave; a key/velocity zone strip for .sfz.

**S13-2. Medium: the OSC page's right panel is two-thirds blank in its SHAPE row, and floors out.** `neuro/03-OSC`: SHAPE has one knob (FRAME) and two 280 px menus; the rest of the 1,000 px row is empty (440 px to the right of SPECTRAL); the inner panel ends at y = 703 while the card ends at 780 (the 75 px floor of V12-1/S12-1), and the left picture reaches y = 770, so the two columns end at different heights. *Fix:* the picture and the panel end on one line; WARP AMT and SPECTRAL AMT knobs are always shown (greyed when the menu is Off) so the row is full and does not change layout when a warp is chosen (`neuro/osc-pd-chain` shows the layout shifting by a 108 px row when a warp chain is added).

**S13-3. Medium: the matrix is roomy rather than powerful.** Rows are 51 px tall (`neuro/07-MATRIX`): 12 routes fill the page, 64 slots mean 3,200 px of scroll; the VIA column is a column of dashed "+" boxes (80 px) that is empty on 12 of 12 rows; no group tint or heading by source (S12-9), though the coloured source bar helps; no per-row mute without the ON switch at the far left. Serum 2's matrix shows about 14 routes at a time and Vital's about 8 large ones; the user here sees 12, so it is on par; but at 40 routes it is worse. *Fix:* a density setting (36 px rows), VIA collapsing to a "+" when unused, a sticky source group.

**S13-4. Medium: browsing sounds without hearing them.** `extras/extra-browser-docked`: 641 presets with search, tag chips and a detail panel, but no audition (a play-note button or "hover to hear"), no "similar", and the favourites and user rows read "0" before the user has any. Serum 2 has no audition either; Vital has none; but this product already has HEAR in the table browser (S13-7) so the machinery exists *(inferred)*. The docked SAVE AS is a dark brown that looks as disabled as DELETE beside it (`extra-browser-docked`). *Fix:* an audition on hover or a PLAY-A-NOTE button; make SAVE AS the accent fill.

**S13-5. Medium-low: FM algorithm tiles are numbers.** `neuro/08-FM`: tiles "1 to 9" with "+1 OSC" tags; DX7 pages 1 to 32. Names are in tooltips only *(inferred)*. The "+1 OSC" tag is a good addition. *Fix:* a one-line name under the diagram ("2-OP STACK" is already the title) and the tile's name on hover in the header.

**S13-6. Medium-low: physics LFOs have their own control vocabulary and drop the DRIVES list.** `neuro/lfo-sim-lorenz`: TRIGGER, OUT 1 AXIS, FIRE, SIGMA, RHO, BETA, SEED, PER VOICE; the standard LFO shows RETRIG, KEY and the DRIVES list. The chip bar says "LFO 1 OUT2" with a tag, in an LFO tile that also says "OUT 2". Two different ways to say "this LFO has a second output". *Fix:* show the DRIVES list for every shape (it is useful on the one with two outputs, split into OUT 1 and OUT 2 groups); make the header caption "shared . restarts on note . OUT 2" the one place that names the second output.

**S13-7. Low-medium: the table browser's HEAR is far from the tile you click.** `neuro/table-browser`: HEAR is top right of a 1,350 px panel; a tile's click only selects it. *Fix:* click a tile = select, HEAR on the tile (a small ear on hover), or a 1-key shortcut.

**S13-8. Low: IMPORT MIDI and EXPORT MIDI touch.** `neuro/gen-clip-expanded` (x = 1370: the right edge of one is the left edge of the other). Other toolbar buttons have 8 px gaps. *Fix:* 8 px gap.

**S13-9. Low: SAVE's TAGS are a text field and chips at once.** `extras/extra-save-as`: "Gritty, Evolving, FM, Bright, Sustained, Wide" in the field and the same words lit as chips below; the field is editable. Two controls for one value, and typed text that is not in the vocabulary cannot light a chip *(inferred)*. *Fix:* chips only, plus a "+" for a new tag.

**S13-10. Low: a bare "OFF" in the macro strip.** `MacroStrip.h:106-108`: "TALK -> 2 . OFF" in amber. What is off is the target module (a reverb mix whose reverb is switched off), which is only in its tooltip. *Fix:* "TALK -> 2 . 1 off" or the module's name ("REVERB OFF") when there is one, a count otherwise.

**S13-11. Low: the SEQ ARP opens as three empty lanes.** `neuro/09-ARP-SEQ`: 1,100 x 500 px of dark lanes, VEL, STEP LEN and PITCH (st) labels, and "ARP is off: draw to switch it on" in the card title (11 px grey). It is a hidden gesture on first run. *Fix:* a centred "DRAW A STEP TO TURN THE ARP ON" over the lanes when off, like the VECTOR pad's "VECTOR OFF . switch on to mix the corners".

**S13-12. Low: the info line (hover line) crowds the chip row.** `neuro/hover-line`: the text sits on the page card's foot, 4 px above the chips. It is readable; but it is the only place a knob's meaning shows, and 12 px grey. *Fix:* 20 px of its own strip, which the layout already reserves (`PluginEditor.cpp:1469`), drawn on the card colour.

**S13-13. Low: "S" solo is a 22 px circle.** `neuro/10-FX`. It is a toggle with a tooltip; a user will not know S means solo until they hover. Serum 2 does the same; no change needed beyond a tooltip on first show.

**S13-14. Low: the macro strip tops out at four visible macros plus "+ MACRO".** `neuro/01-MAIN`: macros 1 to 4 and a "+ MACRO" tile, then the master knob; there is a 190 px gap between "+ MACRO" and MASTER. *Fix:* centre the tile in the gap or let the fifth macro appear there.

---

# Part 3: Integration (DX7/FM, physical, sample, Airwindows and the rest)

How well the newer engines look and read like the original ones: one vocabulary, one widget set, no parallel systems.

**I13-1. Medium-high: a DX7 operator is "OSC n", "OPERATOR n" and "operator" in different places.** PLAY: six strips titled "OSC 1" to "OSC 6" with RATIO / FINE / OUTPUT (`dx7/01-MAIN`); the ENVELOPE card says "6 OPERATORS PLAY THE OPERATOR ENV" over rows "OSC 1" to "OSC 6"; FM: the card title is "OPERATOR 1" with pills "OSC 1", "2" ... (`dx7/08-FM`); OSC page: the combo says "Operator" and the pill "FM operator, a carrier (heard), plays its OP ENV" (`dx7/03-OSC`). Same as I12-17/I12-18; now more visible because PLAY is larger. *Fix:* the thing is "OSC n" everywhere and "operator" is the kind (a role word, like WAVETABLE or SAMPLE); FM's card title "OSC 1 . OPERATOR"; the envelope header "6 OSCILLATORS PLAY THE OPERATOR ENV".

**I13-2. Medium: the OSC picker's tag changes meaning from patch to patch.** `OscPage.h:1090-1091`: the selected tab's tag is the role ("OUT", "MOD -> 2") when it has one, else the engine ("WAVETABLE", "SAMPLE"). `neuro/03-OSC` shows "OSC 1 OUT"; `init/added-osc-OSC` "OSC 1 WAVETABLE"; `neuro/osc-sample-empty` "OSC 1 SAMPLE". Same slot, two kinds of word. *Fix:* the tag is always the engine on the OSC page (the role has the FM pill beside the table menu), and the role goes in the tab's tooltip.

**I13-3. Medium: Airwindows cards show raw unit-less knobs where the built-in twin shows real units.** `neuro/fx-35`: DELAY on AIRWINDOWS reads TIME 100 %, FEEDBACK 0 %, FREQ 50 %, FLUTTER 0 %, MIX 100 %, and an ECHOES graph with one line; built-in DELAY reads ms or note lengths. `neuro/fx-41`: WIDENER has no display at all and a 100 px ALGORITHM menu in a 300 px blank (I12-7, V11-6). The card is a different product inside the same frame. *Fix:* an algorithm line under the menu ("what this does", from the Airwindows readme, one sentence); show "a knob's 0 to 1" honestly as 0 to 100 only where that is right; hide the ECHOES graph (draw nothing) rather than a static line.

**I13-4. Medium: the same module has two homes with different completeness (BODY, STRINGS, SOUNDBOARD).** BODY has a full card on FILTER (`dx7/filter-flow-busy`: TYPE, COUPLING, AMOUNT, DECAY, MATERIAL, SIZE, OFFSET, KEY TRK, COUPLE) and a sub-box on PHYSICAL that is empty when off (`dx7/physical-page`). STRING DECAY and DAMP are on the OSC page's STRING row (`neuro/osc-physical`) and on the PHYSICAL page's STRING column. The OSC page's "EDIT STRING >" button jumps to the second. Two surfaces for one set of parameters is OK when one is a summary; here both are full. *Fix:* PHYSICAL becomes the one editor and the OSC page's STRING row shows two knobs (DECAY, DAMP) as a summary with the jump link, which is what it already does for DX7; FILTER's BODY card shows a one-line summary and a jump link.

**I13-5. Medium: BODY, SOUNDBOARD and WEST: three "engine sub-boxes", three layouts.** WEST is a full-width card with menus and a gate picture (`dx7/filter-flow-busy`); BODY a half card with menus and eight knobs; SOUNDBOARD a sub-box with a bordered EDIT SOUNDBOARD > button (`dx7/physical-page`). EDIT links elsewhere are in the card title as "EDIT >". *Fix:* the card-header "EDIT >" link on SOUNDBOARD; the same header order (title, caption, link, switch) on all three.

**I13-6. Medium-low: the duplicate-effect card is a parallel failure state.** "Duplicate OTT (not supported)" is on the not-done-on-purpose list and known; but the product lets you create it (the picker offers every type) then tells you it was wrong. Vital simply does not offer a type twice in the rack. *Fix:* grey the picker's tile for a type already in the rack, with "in rack, slot 1" as its tooltip; the card stays for old patches.

**I13-7. Medium-low: the SEQ chain mixes input, engines and a destination with different widgets.** `neuro/gen-clip-expanded`: "KEYS > STRUM > EUCLID > CLIP > VOICES" is text with coloured active names; the tabs at the right are toggles; GENERATE below has its own on dots and sub-box switches. Order and names now agree (good); but a click on "STRUM" in the chain does not obviously switch STRUM on *(inferred)*. *Fix:* chain names as buttons (a click scrolls to or opens the part), with a pressed look.

**I13-8. Medium-low: a DX7 voice keeps a normal-looking PLAY filter.** `dx7/01-MAIN`: a flat response with markers 1 and 2 at 10 kHz, a full cutoff row and F1/F2 tabs, for a voice whose filter is bypassed in effect. The card should say "filter off" (the DX7 has none). *Fix:* when the cutoff is fully open and the filter type is Low Pass at 12 dB with zero resonance, write "open" in the display; same for F2.

**I13-9. Low-medium: "OUT" still means a meter, a role and an LFO output.** The macro strip's meter (`OUT -- dB`), the FM matrix's row switches (`OUT` over a switch, `neuro/08-FM`) and the LFO tags ("OUT 2") plus "TO OUTPUT" on PLAY. *Fix:* "OUT" for the meter only; FM's row switch reads "HEARD", the LFO tag "2ND OUT" (or "OUT B").

**I13-10. Low-medium: the DX7 pool chips look like normal sources.** `dx7/01-MAIN`: OP LFO and OP PITCH sit in the same chip row as LFO 1 and FILT ENV; OP PITCH's chip is wider than the others (x = 395 to 540 against 130 for LFO 1). Both are voice-wide sources that an ordinary knob will not accept in the usual way *(inferred)*. *Fix:* an "OP" group divider, as the macros' divider.

**I13-11. Low-medium: the DX7 MOD page has an empty "OP PITCH" tile and an "AMP ENV unused" tile at the same size as live ones.** `dx7/06-ENV-LFO`: the tile for OP PITCH shows a flat line; AMP ENV carries a grey "unused" tag. Fine, but they take two of five tile slots and push FILT ENV off the first screen on narrower windows *(inferred)*. *Fix:* unused envelopes shrink to a 40 px chip.

**I13-12. Low: "KEYS . DX7 ROM1A" and "PRESET" and "BASS" are three kinds of caption above the preset name.** `dx7/01-MAIN`, `init/01-MAIN`, `neuro/01-MAIN`: the first is category . bank, the second a literal "PRESET" (for Init, no category), the third a category. Consistent in code; to a user "PRESET" above "Init" reads as a label for the box, and the others read as data. *Fix:* "INIT" shows no caption, or "PRESET" shows for all with the category in the browser.

**I13-13. Low: the Resample / Bounce wording.** The tutorial says RESAMPLE, the OSC header has RESAMPLE, the source and docs call it bounce, the menu says "Resample the patch into OSC n" (`OscPage.h:1870`). Fine in the UI; mention in HANDOFF's vocabulary list for the next editor.

**I13-14. Low: patch and preset still alternate.** UI: "Init patch", "Random patch", "Ask before replacing an edited patch" (`PluginEditor.cpp:2120, 2192, 2291`) against "Save preset as", "Replace your edits? 'Neuro Wobble' has changes..." and RANDOM PRESET. The convention (patch = the sound you are editing, preset = the saved file) is followed in dialogs and not in the settings menu. *Fix:* "Init preset" is wrong too: use "New patch" and "Random patch" for the editing actions, and keep "preset" for files, and say so in UI-CONVENTIONS.

**I13-15. Low: the FX header's dice ("FX") and the global dice.** Two dice with the same icon: the header's (patch randomise menu) and the FX page's `FX` (chain). The convention names the tooltip; the icon is identical *(inferred)*. *Fix:* the FX one is a small "RANDOMISE" text button.

**I13-16. Low: the confirm dialog says SAVE AND LOAD for a factory preset.** `confirm/00-confirm.png`: "Neuro Wobble has changes that aren't saved. Loading 'Init' replaces them." with SAVE AND LOAD as the primary. For a factory preset, SAVE means SAVE AS (a name dialog) *(inferred)*, so the primary opens a second dialog. *Fix:* "SAVE AS... AND LOAD" when the current preset is factory; "SAVE AND LOAD" when it is a user preset.

---

## Top ten, one line each

1. V13-1: the FX rack still leaves holes (700 x 270 px beside a lone CRUSH card, a half-empty FREEZE card, ragged right edges).
2. V13-2: the duplicate-effect card cuts its sentence ("Remove it, or kee..."), and the UI test passes anyway.
3. I13-1: DX7 operators are OSC n, OPERATOR n and "operator" on different pages.
4. V13-5: the OUT meter is still the smallest instrument, and PLAY lost its PATCH/output view (dead code at `MainPage.h:1485`).
5. V13-3 / V13-4: PLAY strips are tuned for two oscillators: one strip leaves knobs floating, five or six drop WARP and DETUNE and abbreviate "Sin / Sqr / Saw".
6. I13-3: Airwindows cards show unit-less 0 to 100 % knobs and blank displays beside the built-in twins' real units.
7. S13-1: the sample page is a black box with a form and no velocity or key map.
8. I13-2: the OSC picker's tag is the role on one patch and the engine on another.
9. V13-10: the VECTOR switch breaks the written convention, and the pad is a 720 px square with one dot when off.
10. V13-20: the UI test does not enforce the conventions (casing, switch position, state-dependent cards).
