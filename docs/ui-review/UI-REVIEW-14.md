# UI review 14 (pass 9): ilanaSynth v1.3 against Vital, Serum 2, and itself

One reviewer, three angles, one file, a fresh adversarial pass. Build: branch `claude/project-thread-smvgfk`, `/home/user/ilanaSynth/build`, head `46b45a9`. Evidence: `/home/user/shots14/{neuro,init,dx7,keys,small,dx7-small,extras,remap,confirm}/` (cited as `set/file`) and the real Vital and Serum 2 screenshots in `/home/user/refshots/`. I read about 70 shots at full size and the source in `src/gui/` and `src/PluginEditor.cpp` (file:line where I have it). I could not click or drag, so live behaviour is marked *(inferred)*. The snapshot tool turns OSC 1 physical and back on the OSC page, so "EDITED" after 03-OSC and the "OSC 1 plays a wavetable" text on `keys/04-PHYSICAL` are tool artefacts, not findings.

`ilanaSnapshot --uitest`: **`UI TESTS PASSED (0 failures)`**. The test is green while `dx7-small/01-MAIN` shows hard-clipped text ("MODULATES 1" / "MODULATES 3" lose their last character under the wave picture, and "6 OSCILLATORS PLAY THE OPERATOR E..." is cut), `dx7/06-ENV-LFO` shows "AMP E..." cut in the compact unused pool card, and `dx7/08-FM` shows the new "OSC 1 . OPERATOR" title running into the picker pill. So the cut-text test still misses text clipped by a sibling widget and text on state-dependent cards (V14-17).

## Verdict and scores

Review 13's fix round landed well where it was aimed. The FX rack is now a real two-column grid in chain order with no ragged edges (`neuro/fx-01`, `fx-04`), the duplicate-effect card says its whole sentence, PLAY holds six controls on every strip and spells out Sine / Square / Saw (`init/01-MAIN`, `neuro/added-osc-MAIN`), the OUT meter is a 190 px stereo instrument and the three header pills are equal, the DX7 operators are "OSC n" everywhere and the picker's tag is always the engine, PHYSICAL has full BODY and SOUNDBOARD boxes, SEQ's ARP lanes carry a "DRAW A STEP" hint, the matrix has source group headings and the DX7 envelope card shows six level bars. PLAY is now the densest and best page in the product, and by my block measure (below) it is as dense as Vital's VOICE page.

But the owner's first question this time was dead space, and against the real screenshots the build is two products. **PLAY, the 6-operator DX7 FM page, SEQ with the clip roll open and the 12-route matrix are as tight as the benchmarks.** About half of the other pages are not: VECTOR when off is 60 % bare floor, PHYSICAL on a non-physical oscillator 80 %, the matrix with a few routes has a 400 px text-only REMAP panel, the FM page on a one-oscillator patch is a 3 x 3 grid of black tiles with eight empty, the FX rack with an odd number of cards ends in a 490 x 205 px dashed tile, and the OSC page's control panel is half empty between rows. Vital and Serum never show a panel that exists only to say it is empty; every panel carries a display or controls even in its default state. That, plus three new small regressions from the last round (V14-5, V14-17, I14-1), is what holds the scores where they are.

| Angle | Review 11 | Review 12 | Review 13 | Review 14 | Target |
|---|---|---|---|---|---|
| Against Vital | 8.7 | 8.9 | 9.0 | **8.9** | 9.5 |
| Against Serum 2 | 8.6 | 8.8 | 8.8 | **8.7** | 9.5 |
| Integration | 8.9 | 9.1 | 9.1 | **9.2** | 9.5 |

Why Vital and Serum dipped 0.1: last round's items were fixed (+0.2 net), but measuring against the real screenshots turned up real-estate problems earlier reviews, working from a smaller reference set, scored too softly (empty-state pages, the single-oscillator FM page, the matrix REMAP placeholder), plus the three regressions. Why integration rose 0.1: the operator vocabulary, the engine tag, BODY / SOUNDBOARD parity and the HEARD switch are real gains; Airwindows units and the PLAY filter on a DX7 voice are still the visible seams.

## What stands between this build and 9.5

The shortest list I can defend. Each is small; together they are the gap.

1. **Make every page earn its area in its default state.** No panel whose only content is a sentence: VECTOR when off (show the pad as a live mini-scope of the four corner waves, or fold the page to a one-row switch plus the corners), PHYSICAL on a wavetable oscillator (offer SWITCH TO PHYSICAL inside a half-height card and fold the rest, or put the physical controls into the OSC page), the matrix's REMAP dock (show the selected row's curve, or the first route's, never a text box), the FM page with one oscillator (the matrix at 3 x 3 is 40 % of the page and eight tiles are black), and the FX rack's odd-card tile (V14-1 to V14-5).
2. **Tighten the OSC and FILTER pages to the density of PLAY.** The OSC panel's rows float on 117 px pitches with one control in a 700 px row; FILTER's flow and response panels are 470 px tall for four to seven nodes; the VOICE drawer spends 40 % of its width on a help paragraph. Cap rows, pack the unison row and the voice row, let the response graph take the flow's slack (V14-6 to V14-9).
3. **Finish the two new regressions and the cut-text test.** The FM operator title overlaps the picker; the DX7 strip's role line and the unused pool card cut text; make the UI test fail on text clipped by a sibling and run it on the DX7 patch at 75 % (V14-5, V14-17, I14-1).
4. **Give the Airwindows cards the built-in cards' units and displays**, and make a DX7 voice's PLAY filter say it is off (I14-2, I14-3).
5. **Serum's depth in two places:** an audition button in the browser (and a detail panel that is not 60 % empty) and a VIA column that earns its 80 px (S14-1, S14-2).
6. **Polish residue:** F2 has no switch, COLOUR still reads disabled, SUB pills are mixed case, the table browser's HEAR is far from the tile, physics LFOs speak their own language (V14-12 to V14-16, S14-5, S14-6).

Do 1 to 4 and I would score 9.3 to 9.4 on all three angles; with 5 and 6 I would give 9.5 on integration and 9.4 to 9.5 on the other two. Without 1 and 2 the build stays at 9.0 at best, because a user opens FILTER, OSC and FX in the first ten minutes and sees half a page of nothing.

## Review 13's items: fixed since review 13 / still open

### Vital (V13-1 to V13-21)

| # | Review 13 item | Status |
|---|---|---|
| V13-1 | FX rack holes and ragged rows | **Fixed in form, residue open**: two-column grid, no ragged edges; but the hole beside a lone last card is now a 490 x 205 px dashed ADD EFFECT tile (`neuro/10-FX`, `fx-03`) and FREEZE is a paragraph in a black box (V14-1) |
| V13-2 | Duplicate-effect card cuts its sentence | **Fixed** (`neuro/fx-duplicate`: whole sentence, REMOVE below). Test now covers it |
| V13-3 | One open strip leaves knobs floating | **Fixed** (`init/01-MAIN`: 6 controls, bigger dials, fills 290 px) |
| V13-4 | Five or six strips lose WARP / DETUNE; "Sin / Sqr / Saw" | **Fixed** (`neuro/added-osc-MAIN`: six controls on every strip; "Sine / Square / Saw") |
| V13-5 | OUT meter smallest instrument; PLAY lost output view | **Mostly fixed**: 190 px stereo meter (`OutputMeter.h:36-60`), PATCH + OUTPUT tiles show again when strips leave room (`keys/01-MAIN`), but cramped (V14-10) |
| V13-6 | Header pills unequal, grey | **Fixed**: three equal pills (`PluginEditor.cpp:1282-1285`) |
| V13-7 | SEQ control column pairs unlike controls | **Fixed**: menus in one row, knobs in a 2 x 2 below (`neuro/09-ARP-SEQ`); the knob block floats with 100+ px gaps (V14-11) |
| V13-8 | PLAY cannot show what oscillators do together | **Still open**: only appears as the OUTPUT tile when strips are short (`keys/01-MAIN`) |
| V13-9 | PHYSICAL empty state leaves floor | **Half fixed**: live page now fills (`neuro/physical-page`: string, EXCITER, BODY and SOUNDBOARD boxes); the empty state is still a ghost string, 80 % bare (V14-3) |
| V13-10 | VECTOR switch breaks rule; 720 px pad with one dot | **Half fixed**: switch is at the header's right, controls in two filled boxes; the pad is still a 480 x 480 square with "VECTOR OFF" and one dot (V14-2) |
| V13-11 | Filter 2 has no switch | **Still open** (`neuro/05-FILTER`: only a dot in the title) (V14-12) |
| V13-12 | FILTER's two top panels 470 px and mostly empty | **Still open** (`neuro/05-FILTER` unchanged) (V14-7) |
| V13-13 | FM diagram labels cram beside nodes | **Still open, minor** (`neuro/08-FM`: "0 st / 80 %" beside the node, "30 %" on the arrow) (V14-24) |
| V13-14 | Header caption that is a hint | **Half fixed**: now "drag the graph or the knobs" (`neuro/06-ENV-LFO`), still a sentence in a caption slot |
| V13-15 | COLOUR reads disabled | **Still open** (`init/01-MAIN`: grey ring at 100 %) (V14-14) |
| V13-16 | Three overflow vocabularies | **Half fixed**: pool tabs now say "2 MORE", "1 MORE", "9 MORE v"; the chip row still says "+13 v" |
| V13-17 | AIRWINDOWS pills 95 to 115 px | **Fixed** (`neuro/fx-empty`: one width inside a column; TAPE and CONSOLE pills are a shade narrower inside their row, cosmetic) |
| V13-18 | "+ ADD OSC n" two forms | **Still open, low** (pill on OSC, row on PLAY) |
| V13-19 | Remap tiles cut "EASE O..." | **Fixed** (`remap/remap-editor`: "EASE OUT" fits) |
| V13-20 | UI test does not enforce conventions | **Half fixed**: new FX cut-text checks; misses clipped-by-sibling text and 75 % DX7 (V14-17) |
| V13-21 | Casing of chips and pills | **Mostly fixed** (chips upper case: AMP ENV); SUB pills "Sine / Square / Saw / -1 Oct" are still mixed case (V14-15) |

### Serum 2 (S13-1 to S13-14)

| # | Review 13 item | Status |
|---|---|---|
| S13-1 | Sample page: black box, no key/velocity map | **Half fixed**: dashed drop zone with LOAD (`neuro/osc-sample-empty`); key-zone strip C1 to C5 on a loaded SFZ (`osc-sample-sfz`); no velocity map, no waveform ruler or loop markers (S14-4) |
| S13-2 | OSC panel two-thirds blank, 75 px floor | **Half fixed**: amounts dim rather than hide, the floor is gone (15 px); rows still float (V14-6) |
| S13-3 | Matrix roomy rather than powerful | **Half fixed**: source group headings (`neuro/07-MATRIX`), pitch 28 logical px (Vital's ratio); VIA is still 12 of 12 empty dashed "+" (S14-2) |
| S13-4 | No audition in the browser | **Still open** (S14-1) |
| S13-5 | FM algorithm tiles are numbers | **Still open** (`neuro/08-FM`) (S14-8) |
| S13-6 | Physics LFOs speak their own language | **Still open** (`neuro/lfo-sim-lorenz`: TRIGGER, OUT 1 AXIS, FIRE, SIGMA, RHO, BETA, SEED; no DRIVES list) (S14-5) |
| S13-7 | Table browser HEAR far from the tile | **Still open** (`dx7/table-browser`: HEAR top right) (S14-6) |
| S13-8 | IMPORT / EXPORT MIDI touch | **Fixed** (`neuro/gen-clip-expanded`: 8 px gap) |
| S13-9 | TAGS field and chips at once | **Still open** (`extras/extra-save-as`) (S14-9) |
| S13-10 | Bare "OFF" in macro strip | **Still open** ("TALK -> 2 . OFF", `neuro/01-MAIN`; `MacroStrip.h:106-108`) (I14-8) |
| S13-11 | ARP opens as three empty lanes | **Fixed** (`neuro/09-ARP-SEQ`: DRAW A STEP hint); the lanes are still 1100 x 500 px of dark (see real estate) |
| S13-12 | Hover line crowds the chips | **Not re-checked** (`neuro/hover-line` not read) |
| S13-13 | "S" solo is a 22 px circle | **Accepted** |
| S13-14 | Macro strip gap | **Fixed** (`+ MACRO` sits between the last macro and MASTER) |

### Integration (I13-1 to I13-16)

| # | Review 13 item | Status |
|---|---|---|
| I13-1 | Operator is OSC n / OPERATOR n / operator | **Fixed in vocabulary, new layout bug**: "OSC 1 . OPERATOR" runs into the picker pills (`dx7/08-FM`, `FmInputPages.h:197`) (I14-1) |
| I13-2 | OSC picker tag changes meaning | **Fixed** (`neuro/03-OSC`: WAVETABLE; `keys/03-OSC`: PHYSICAL; `dx7/03-OSC`: OPERATOR) |
| I13-3 | Airwindows unit-less knobs, blank displays | **Still open** (`neuro/fx-35`: TIME 100 %, FEEDBACK 0 %, ECHOES one line) (I14-2) |
| I13-4 | BODY has two homes of unequal completeness | **Fixed**: PHYSICAL's BODY box has every control (`neuro/physical-page`) |
| I13-5 | Three engine sub-box layouts | **Half fixed**: BODY and SOUNDBOARD match; WEST is still its own full card (`neuro/filter-west`) |
| I13-6 | Duplicate-effect card is a failure state | **Open by design** |
| I13-7 | SEQ chain mixes widgets | **Still open** (`neuro/gen-clip-expanded`) |
| I13-8 | DX7 PLAY filter looks normal | **Still open** (`dx7/01-MAIN`: flat response, full knob row) (I14-3) |
| I13-9 | "OUT" three meanings | **Half fixed**: FM says HEARD; the meter's OUT and the LFO "OUT 2" tag remain |
| I13-10 | OP LFO / OP PITCH chips look normal | **Still open** (`dx7/01-MAIN`: OP PITCH chip is wider) |
| I13-11 | Unused pool cards at full size | **Fixed, regressed**: "AMP E... unused" is compact now but cuts its own name (`dx7/06-ENV-LFO`) (V14-17) |
| I13-12 | "PRESET" / "BASS" / "KEYS . DX7 ROM1A" caption | **Still open** ("PRESET" over Init) |
| I13-13 | Resample wording | **Fixed** (convention written) |
| I13-14 | patch / preset | **Half fixed** (convention written; menus not re-read) |
| I13-15 | Two identical dice | **Still open** (`neuro/10-FX`: dice + "FX") |
| I13-16 | SAVE AND LOAD for a factory preset | **Still open in the snapshot** (`confirm/00-confirm`: "SAVE AND LOAD" over Neuro Wobble) |

---

# Screen real estate, page by page

Method. I measured every shot the same way for ilanaSynth, Vital and Serum 2: split the page body into blocks of about 1 % of the window width (14 px here), call a block "empty" when nothing is drawn in it (no text, line, knob or fill; flat background only), and count (a) all empty blocks and (b) empty blocks inside a bare region at least four blocks square (about 55 px across, the size where the eye reads "nothing here"). Dark display wells with no signal count as empty, which is how a user sees them. Body area is the window between the tab bar and the chip row (ilanaSynth) or the whole window above the keyboard (Vital) or below the header (Serum). This is a proxy, not an area of "wasted" pixels: a card's background between knobs is normal, but the benchmark's fraction of it is the yardstick. My estimate of dead space per page combines the measure with what is in the empty regions.

**The benchmark's numbers by the same measure:** Vital VOICE 34 % empty / 18 % in bare regions; EFFECTS 33 / 14; MATRIX 40 / 27; ADVANCED 39 / 21 (`refshots/vital-*.png`). Serum 2's main OSC page: 15 % / 0 % (`serum2-main-osc-page.png`). Neither has a region 55 px square with nothing in it on its main page, apart from dark displays and the matrix's remap graph (which carries a curve).

| Page / state | Empty (all) | In bare regions | What the empty area is | Benchmark's equivalent |
|---|---|---|---|---|
| **PLAY, 2 oscillators** (`neuro/01-MAIN`) | 41 % | 12 % | Between knob rows in the SUB + NOISE card (130 x 30 px gaps), the OSC 1 strip's dark picture, the left of OSC 3's folded strip (a 280 px stretch between "Wavetable . Basic" and its switch) | Vital VOICE 34 / 18; **at benchmark** |
| PLAY, 1 oscillator (`init/01-MAIN`) | 48 % | 16 % | OSC 1 strip's picture (a 330 x 245 px black box showing one saw), SUB + NOISE's picture (330 x 250 px) | as above |
| PLAY, 6 operators DX7 (`dx7/01-MAIN`) | 40 % | 14 % | Right of each strip's OUTPUT knob, the "6 OSCILLATORS" card's level bars leave 40 px gutters | Serum main 15 / 0 (denser), Vital 34 / 18 |
| PLAY, physical (`keys/01-MAIN`) | 40 % | 4 % | Nothing bare; the PATCH and OUTPUT tiles fill the spare (cramped, V14-10) | at benchmark |
| **VECTOR, off** (`neuro/02-VECTOR`) | **74 %** | **60 %** | The pad: 480 x 480 logical px of dark with a grid and one dot; the POSITION AND MOTION box: five knobs spaced 150 px apart, 40 % of the box bare | No equivalent page; Vital has no vector view; Serum puts it in the OSC panel as one display |
| **OSC, wavetable** (`neuro/03-OSC`, `init/03-OSC`) | 52 % | 40 % | SHAPE row: five controls in a 700 px row, two of them dimmed, with a 440 x 90 px gap to the right of FRAME; row pitch 117 px; UNISON row's gap between UNISON and DETUNE; the panel's right 60 px of every row; VOICE drawer: a 330 x 80 px help paragraph where a fifth of the width is text | Serum OSC panel: 12 controls plus display in 450 x 540 px with no row gap (`serum2-wavetable_osc.png`) |
| OSC, DX7 operator (`dx7/03-OSC`) | 62 % | 50 % | OP ENV graph is 1330 x 230 px (content); WAVE row has 3 controls in 1330 px: SINE, FEEDBACK, FB TYPE with a 370 px combo; PITCH row 5 controls | Vital's operator-less; DX7-like hardware has 12 knobs per operator |
| OSC, sample (`neuro/osc-sample-empty`, `osc-sample-sfz`) | 63 to 65 % | 49 to 52 % | Two-thirds of the SAMPLE row and UNISON row to the right; the picture area is a dashed box in empty state | Serum sample panel: the whole panel is the waveform and 8 controls (`serum2-sample_osc.png`) |
| **PHYSICAL, non-physical oscillator** (`neuro/04-PHYSICAL`, `keys/04-PHYSICAL` *tool artefact state but a real state*) | **87 %** | **80 %** | A 1450 x 500 px ghost string, a ghost soundboard, one sentence, a button; 270 px of floor under | Serum: switching the oscillator type swaps the panel in place |
| PHYSICAL, live (`neuro/physical-page`) | 57 % | 38 % | Gaps in the STRING row (2 knobs apart), BODY box left half | Vital none |
| **FILTER** (`neuro/05-FILTER`) | 56 % | 31 % | SIGNAL FLOW: a 600 x 470 px box with 4 to 7 nodes in a 120 px column and a 700 px blank to the right; BALANCE floating at the foot; RESPONSE graph 890 x 470 px with one curve; FILTER 2 knobs dimmed | Vital VOICE: filter graph 350 x 180 px with six controls; the page's filter is 1/5 of the window |
| FILTER with WEST on (`neuro/filter-west`) | 52 % | 21 % | The right column under BODY: a 750 x 220 px hole (BODY is a collapsed row, WEST is tall) | none |
| **MOD ENV/LFO** (`neuro/06-ENV-LFO`) | 57 % | 30 % | Graph interiors (a triangle in 700 x 280 px; the AMP ENV graph 700 x 180 px), LFO card's DRIVES list floor | Vital: LFO graph 550 x 200 px; at benchmark for displays |
| MOD LFO physics (`neuro/lfo-sim-lorenz`) | 55 % | 26 % | Phase plot 280 px wide in a 700 px well | Serum's LFO 8 is one display |
| **MATRIX, 12 routes** (`neuro/07-MATRIX`) | 38 % | 7 % | VIA column (80 px x 12 rows of empty "+") | Vital MATRIX 40 / 27; **ilana is denser** |
| **MATRIX, 5 routes** (`keys/07-MATRIX`) | 63 % | 46 % | The REMAP dock: a 1500 x 380 px box holding two sentences | Vital's remap dock shows the curve at the same size |
| MATRIX, empty (`init/07-MATRIX`) | 86 % | 76 % | Empty-state illustration (accepted as an empty state) | Vital shows 12 blank rows (also empty) |
| **FM, 2 oscillators** (`neuro/08-FM`) | 60 % | 42 % | FM diagram 880 x 300 px with two 70 px nodes at the left, a 400 px blank; matrix 3 x 3 with 7 empty tiles of 100 x 115 px | none (Serum's FM is a warp mode) |
| FM, 1 oscillator (`init/08-FM`, `keys/08-FM`) | 61 % | 46 % | Same: diagram shows one node and a hint; matrix is 9 tiles with one dot | none |
| FM, 6-operator DX7 (`dx7/08-FM`) | 43 % | 14 % | None to speak of: diagram, 6 x 6 matrix and operator card all busy | at benchmark |
| **SEQ, ARP off** (`neuro/09-ARP-SEQ`) | 54 % | 21 % | Three empty lanes (1100 x 500 px) with a hint in the middle; the GENERATE card has three boxes each 40 % bare | Serum's ARP is a keyboard strip, no empty lanes |
| SEQ, CLIP open (`neuro/gen-clip-expanded`) | 31 % | 6 % | None | Serum CLIP: same density |
| **FX, 3 cards + ADD tile** (`neuro/10-FX`, `fx-03`) | 65 % | 48 % | 490 x 205 px dashed tile; displays hold one trace; dimmed knobs | Vital EFFECTS 33 / 14: modules stacked edge to edge |
| FX, 2 cards (`keys/10-FX`) | 62 % | 40 % | The ADD EFFECT row (700 x 40) and each card's right 25 % | Vital |
| FX, empty rack (`init/10-FX`, `neuro/fx-empty`) | 63 % | 35 % | The library grid (content) and a 100 px floor above OUTPUT | Vital shows all modules always; no empty state |
| Preset browser, docked (`extras/extra-browser-docked`) | 64 % | 45 % | The detail panel: 370 x 700 px with a name, three tags, four macro names | Serum: browser plus ARTIST / DESC |
| Window chrome | header 105 px (10 %), bottom two rows ~130 px (12 %) | | Header: logo + preset + pills in 105 px; the status line (OUT scope, BPM, VOICES, CPU) adds a second row | Vital header 50 px (6 %); Serum 110 px but with preset and macros inside it |

**Summary.** The mean of the "bare region" measure across the ten main pages on the Neuro patch is 37 %, against Vital's 20 % (four pages) and Serum's 0 %. Take out the three empty-state pages and it is 24 %, which is near Vital. The offenders are listed worst first below (V14-1 to V14-9). **The worst four pages are PHYSICAL on a non-physical oscillator (80 % bare), the matrix with a REMAP placeholder (46 %), VECTOR off (60 %) and the FM page on a one-oscillator patch (46 %).** All four are pages a new user visits within minutes, none is an "error" state, and each ends the page with "this is where the thing would be".

---

# Part 1: Against Vital

### V14-1. High: the FX rack ends in a 490 x 205 px dashed hole whenever the card count is odd, and FREEZE is a paragraph in a black box
`neuro/10-FX`, `fx-03`, `fx-06`, `fx-12`: three cards in a two-column grid leave the right half of row two as the dashed ADD EFFECT tile (`FxPage.h:1228-1241`: the tile takes `lastRowHole`). It is a real button, but at 490 x 205 logical px it is 15 % of the page for one click target, and it is the same tile whether the rack has one effect or ten. FREEZE's picture slot holds "WHAT IT DOES" and two lines of help text in a 280 x 160 px black box (`FxPage.h:1594`); AIRWINDOWS DELAY's "ECHOES" display is one vertical line (`neuro/fx-35`). *Benchmark:* Vital's EFFECTS page stacks all modules edge to edge with a picture each, and adding is a column of tiles at the left (`vital-effects.png`); no hole, no paragraph. *Fix:* keep the add tile at 40 px height on its own row under the grid (as already done for an even count, `FxPage.h:1237`), and let a lone last card take the full row, with its picture growing. Replace FREEZE's text with a live spectrum snapshot (frozen or not) and AIRWINDOWS cards with at least the same picture as their built-in twin.

### V14-2. High: VECTOR is 74 % empty when off and 54 % when on the pad's own area
`neuro/02-VECTOR`, `neuro/vector-page`: a 480 x 480 logical px pad shows the grid, "VECTOR OFF . switch on to mix the corners" and one dot; the controls box beside it holds five knobs at 150 px spacing with 40 % floor (`FilterVectorPhysicalPages.h:401-409`: the pad "fills the card's height"). The whole page exists for one switch. *Benchmark:* neither Vital nor Serum has a page for a feature that is off; Serum shows each OSC's own mini-display. *Fix:* when off, draw the pad as a faint live mix of the four corner oscillators (the waves are already drawn on PLAY) so it is a picture first, or collapse the page to a header with the switch and the CORNERS box (a 140 px band) and let the remaining 600 px carry the PLAY strips' oscillator pictures at the corners; and pack POSITION AND MOTION into one row of five knobs.

### V14-3. High: PHYSICAL on a non-physical oscillator is a full page of ghost drawing
`neuro/04-PHYSICAL`: a 1450 x 500 px dark panel with a grey string and a grey "NO BODY", a sentence, a SWITCH TO PHYSICAL button, 270 px of floor. 80 % bare. *Benchmark:* Serum's panel changes in place when the oscillator type changes; nothing is ever drawn that cannot be used. *Fix:* when the selected oscillator is not physical, show the picker, one line "OSC 1 plays a wavetable" and the button in a 120 px card at the top, and give the rest to the OSC picker's other physical-capable oscillators or fold the page away; or open the page on the first physical oscillator if any.

### V14-4. High: the matrix REMAP dock is a text box that eats 37 % of the page when no curve is open
`keys/07-MATRIX`: five rows (200 px), then a 1500 x 380 px recessed panel with "REMAP / Click a row's CURVE ... / To add a route, drag a source ..." (`MatrixPage.h:182-195`). With 12 rows it is not shown (the rows fill the page, `neuro/07-MATRIX`), so the page's density depends on route count. *Benchmark:* Vital's MOD REMAP panel is always a curve (`vital-matrix.png`: "MOD REMAP 8" with a graph at the foot), here replaced by a two-line hint. *Fix:* draw the first route's curve (or the last-touched one) in the dock by default, selected row highlighted; the hint becomes a 12 px caption in the dock's header. With one to four routes let the rows take 56 px each (more room for the amount slider) rather than leave a text box.

### V14-5. Medium-high: the FM page on one or two oscillators is a 3 x 3 tile grid, eight tiles black, and a diagram with one bubble
`init/08-FM`, `keys/08-FM`, `neuro/08-FM`: 880 x 300 px diagram showing one 70 px node and "Drag from one oscillator to another..."; the FM MATRIX card is 570 x 800 px with nine 100 x 115 px tiles, one lit. 46 % in bare regions. On the DX7 voice the same page is perfect (`dx7/08-FM`). *Benchmark:* none (FM is a warp in Serum and absent in Vital), but both fit a page's size to its content. *Fix:* size the matrix to the oscillators in use (rows and columns for `n` oscillators, tiles 56 px, centred) and put the freed width into the operator card's envelope picture; with one oscillator show a one-line state and the algorithm tiles only. And in the one-oscillator case the operator card's "OSC 1 . OPERATOR" overlap (I14-1) does not exist but "Edit AMP ENV on MOD or PLAY..." (a 12 px grey hint at the card's foot) is the only content under the graph.

### V14-6. Medium: the OSC page's controls panel floats; 40 % of it is in bare regions
`neuro/03-OSC`, `init/03-OSC`: three rows (SHAPE, PITCH & LEVEL, UNISON) on a 117 px pitch (`OscPage.h:1305-1325`: `unitHeight = jmin(maxRowHeight, content / units)`), the SHAPE row has FRAME, WARP menu, WARP AMT, SPECTRAL menu, SPEC AMT in a six-column grid with the last column and a 90 px band below empty. Because the panel is 330 px tall and the rows are capped, row gaps are 60 px of nothing. *Benchmark:* Serum packs WT POS, UNISON, DETUNE, BLEND, two warps, PAN, LEVEL into 450 x 200 px (`serum2-wavetable_osc.png`). *Fix:* a 4 x 3 grid with the knobs at the PLAY strip's size (48 px) and the menus (WARP, SPECTRAL, UNI MODE, CHORD, TUNING, ENVELOPE) on a single row above; use the freed 150 px for a visible WARP CHAIN row or the frame-by-frame scope.

### V14-7. Medium: FILTER's response and flow panels are 470 px tall for four to seven nodes
`neuro/05-FILTER`, `init/05-FILTER`, `filter-six-osc`: the flow box is 600 x 470 px (`FilterVectorPhysicalPages.h`), a 120 px column of nodes at the left, BALANCE floating at the foot, 700 px of floor. Same as V13-12. *Benchmark:* Vital's filter cards give the graph the size and the controls sit under it (`vital-voice-main.png`). *Fix:* cap the flow at 340 px; response graph and flow share 340 px; give the extra 130 px to a spectrum/phase view or to the two filter cards (taller knobs), and fold BODY (collapsed rows are fine) alongside WEST instead of leaving a 750 x 220 px hole (`neuro/filter-west`).

### V14-8. Medium: the SEQ ARP card is 500 px of empty lanes until a step is drawn
`neuro/09-ARP-SEQ`: three lane graphs (VEL, STEP LEN, PITCH) on a 1100 x 500 px field, a hint button in the middle; the right column's four knobs are spread over a 280 x 500 px column. GENERATE below has three boxes with a 40 % bare right edge each. *Benchmark:* Serum's ARP lane is a strip. *Fix:* when ARP is off, show a 160 px strip of 16 empty steps with the hint and open the GENERATE boxes, which are the more useful content; or fill the lanes with a default 8-step up pattern when first switched on so the field is never empty.

### V14-9. Medium: VOICE drawer spends a third of its width on a help paragraph
`neuro/03-OSC`, `dx7/03-OSC`: MODE, VOICES, BEND RANGE, GLIDE, LEGATO ONLY, then a 4-line paragraph of explanation (330 x 90 px) that never changes. *Benchmark:* Vital's voice block (VOICES, BEND, VEL TRK, SPREAD, GLIDE, SLOPE) is 330 x 150 px with no help text; the knobs' tooltips carry the explanation. *Fix:* delete the paragraph (the info line and tooltips already carry it), widen the knobs, or move SPREAD & DRIFT's controls into the freed width so the drawer's three tabs need not be tabs.

### V14-10. Medium: the PLAY PATCH and OUTPUT tiles are cramped when they do appear
`keys/01-MAIN` (crop): PATCH shows nodes as 14 px pills in a 90 px band with the OUT line crossing at 8 px spacing; the OUTPUT tile is 70 px tall, with its spectrum line drawn through the "OUTPUT" label and the title "OUTPUT" appearing twice. They appear only when the strips are short (one oscillator), so PLAY changes character between patches (`MainPage.h:1595`: `patchMinHeight = 112`, `outputMinHeight = 70`). *Fix:* raise `outputMinHeight` to 100 and draw nodes at 22 px (the FILTER page's size) or show the OUTPUT card only; drop the in-graph "OUTPUT" label.

### V14-11. Medium-low: SEQ's control column floats and ARP's knobs are 100 px apart
`neuro/09-ARP-SEQ`: RATE and MODE menus on one row at the top, then a 2 x 2 of knobs (STEPS, GATE, OCTAVES, CHANCE) spaced 170 px vertically, with 90 px of floor between the menus and the first knob row. *Fix:* knob rows at 100 px pitch, menus directly above; or two columns of three.

### V14-12. Medium-low: Filter 2 has no switch (V13-11 again)
`neuro/05-FILTER`: F2 is a dot in the title; its cutoff at 20 kHz dims the card. The convention says a card's own module has its switch in the header at the right (`UI-CONVENTIONS.md`, "On/off switches"). WEST and BODY have one. *Fix:* a switch in FILTER 2's header, the type menu reading what it will bring back.

### V14-13. Medium-low: the header is 10 % of the window and a second row
`PluginEditor.cpp:1415-1445`: logo, preset name and arrows, SAVE, undo/redo, history, A/B, dice, gear on the first row (50 px); the status line holds the OUT waveform strip, BPM, VOICES and CPU on a second row (30 px) and the tab bar a third (35 px). Vital's is one 50 px strip with the tabs inside it. *Fix:* put BPM, VOICES and CPU on the tab row's right (the tab bar's SCOPE / KEYBOARD / ? sit there already; there is 300 px free at 1060) and gain 30 px for the pages.

### V14-14. Low: COLOUR on SUB + NOISE still reads disabled at 100 %
`init/01-MAIN`: grey ring and label while SUB is orange (V13-15). A noise tint did not change the knob's grey. *Fix:* the SUB colour at 50 % strength, or a lit ring.

### V14-15. Low: SUB pills are mixed case
`neuro/01-MAIN`: "Sine / Square / Saw / -1 Oct / -2 Oct" against the convention's "module and source names upper case; values keep their units' case". They are values (waveforms), so mixed case is defensible, but the menus on the strip above them ("Wavetable", "Neuro", "Off") are mixed case too and the tabs above those are upper case. *Fix:* leave as is and say in the convention that value pills are sentence case, or upper-case all pills.

### V14-16. Low: the unused SUB + NOISE and OSC rows in the DX7 PLAY keep a 280 px gap
`dx7/01-MAIN`, `neuro/01-MAIN`: folded rows read "Wavetable . Basic" at the left and a switch at the right with 400 px between. Fine; but folded rows show no waveform, so a user cannot see what turning them on does. *Fix:* a 28 px thumbnail in the fold.

### V14-17. Medium: text still cut (the UI test is green)
`dx7-small/01-MAIN` (crop at 75 %): "MODULATES 1" and "MODULATES 3" lose the last digit, clipped hard by the wave picture beside them (OscPage's role line is `OscPage.h:123-139`, drawn without a width limit in the strip). "6 OSCILLATORS PLAY THE OPERATOR E..." ellipsis-cuts (`MainPage.h:449`). `dx7/06-ENV-LFO`: the compact unused card says "AMP E... unused". `dx7/08-FM`: "OSC 1 . OPERATOR" overlaps the OSC 1 pill (`FmInputPages.h:197`, `reserve` is `operatorCard.getRight() - tabsLeft + 8`, and the title is 130 px). *Benchmark:* neither benchmark cuts a label. *Fix:* test on the DX7 patch at 100 % and 75 %, treat an overlap of a label's rectangle with a sibling's as a failure (not only an ellipsis), and shorten these labels ("-> 1" instead of "MODULATES 1" on a 75 % strip; "6 OSCILLATORS PLAY THE OP ENV").

### V14-18. Low: the hover line and the macro row share a strip
*(inferred; `neuro/hover-line` not read.)* The bottom two rows (chips 24 px, macros 40 px) take 12 % of the window. Vital's equivalent is the keyboard. If the keyboard is on, the window loses another 36 px. *Fix:* none needed beyond V14-13.

### V14-19. Low: dashed-tile buttons everywhere (ADD OSC, ADD EFFECT, ADD MODULATION, + MACRO)
Four different sizes and two label forms (`neuro/01-MAIN`, `neuro/10-FX`, `neuro/07-MATRIX`). They are fine; they could share one height (36 px). Same as V13-18.

---

# Part 2: Against Serum 2

### S14-1. Medium-high: still no way to hear a preset before loading it
`extras/extra-browser-docked`: 641 presets, search, categories, tags, a detail panel; the selected row loads on click and Up/Down. The detail panel is 370 x 700 px with a name, a category, three tags and four macro names, 60 % bare. Same as S13-4. *Benchmark:* Serum 2's browser loads on click (the same); Vital's has no audition either. Not a gap against them; it is a gap against the "powerhouse" brief. *Fix:* an audition button (a held key / one fixed note) in the detail panel, a "similar" list under the tags, and the tags as filter chips (click to filter).

### S14-2. Medium: the matrix's VIA column is empty on 12 of 12 rows
`neuro/07-MATRIX`: 80 px of dashed "+" boxes on every row. *Benchmark:* Vital's matrix has BIPOLAR, STEREO and MORPH in that width, each with a state; Serum's matrix has a "via" field populated for the active routes. *Fix:* show VIA only for rows that have one; give the width to AMOUNT, which is the control people drag.

### S14-3. Medium: sample page, empty state, repeats the 440 x 440 px dashed box
`neuro/osc-sample-empty`: 65 % empty; the picture is a dashed rectangle with the sentence and a LOAD button, and TUNED, LOOP, REVERSE, START, END, FADE IN, FADE OUT are drawn for a sample that does not exist. *Benchmark:* Serum 2's sample panel always shows a waveform with a loop bar (`serum2-sample_osc.png`). *Fix:* draw a ghost waveform in the box, dim the controls (as the OSC page does for an amount whose stage is off), keep the LOAD button.

### S14-4. Medium: the loaded sample has a key map but no velocity map, no loop markers, no waveform
`neuro/osc-sample-sfz`: zones C1 to C5 as a keyboard strip (a good addition); the picture is two curves (an envelope-like shape), not the waveform; no loop start / end markers on it. *Benchmark:* Serum 2's sample panel shows the waveform with draggable loop markers and LS / LE (`serum2-sample_osc.png`). *Fix:* waveform with START / END / loop handles drawn on it; velocity layers as rows in the zone strip.

### S14-5. Medium-low: physics LFOs speak their own language and lose the DRIVES list
`neuro/lfo-sim-lorenz`: SIGMA, RHO, BETA, SEED, TRIGGER, OUT 1 AXIS, FIRE; no DRIVES list; the tile says "OUT2" twice. Same as S13-6. *Fix:* DRIVES list under the controls for every LFO type; "TRIGGER" -> "RETRIG" where it is the same thing.

### S14-6. Medium-low: the table browser's HEAR is far from the tile
`dx7/table-browser`: HEAR is at the top right of a 1100 px panel; a tile's click selects. Serum's wavetable browser plays on click. *Fix:* hover-ear on the tile or HEAR on selection.

### S14-7. Low: the macro strip shows four macros of eight, and "+ MACRO" is a tile
`neuro/01-MAIN`: four macros, a 100 px "+ MACRO" tile, then MASTER. Fine. *Benchmark:* Serum shows all eight. *Fix:* none; the tile is honest.

### S14-8. Low: algorithm tiles are numbers
`neuro/08-FM`: "1" to "9" with a small icon and "+1 OSC". The diagram's title names the one chosen ("2-OP STACK, EDITED"). *Fix:* a hover name in the card header.

### S14-9. Low: SAVE's TAGS are a field and chips at once
`extras/extra-save-as`: "Gritty, Evolving, FM, Bright, Sustained, Wide" in the text field and the same words lit as chips; the field is editable and a typed word not in the vocabulary lights no chip. Also an 80 px gap between NAME and CATEGORY. *Fix:* chips only plus a small "+ tag" field.

### S14-10. Low: the matrix with the REMAP dock open cuts a row at its foot
`remap/remap-editor`: the list ends in half of row 7 (y = 560, a 40 px slice of the 12th row with its switch cut). V12-17 said the matrix now ends on a row; with the dock open it does not. *Fix:* snap the list height to a whole number of rows when the dock is open.

### S14-12. Low: "OUT 2" in an LFO tile, "OUT" in the meter, "OUT" in FM (now HEARD)
Remaining two of three (I13-9).

### S14-13. Low: no keyboard inside the window by default
Serum has the keyboard on the main window with the pitch and mod wheels; ilanaSynth has a KEYBOARD toggle (`neuro/01-MAIN`) and the keyboard comes at the cost of the pages' last 36 px. Fine; call out.

---

# Part 3: Integration (DX7/FM, physical, sample, Airwindows and the rest)

### I14-1. Medium-high: last round's "OSC 1 . OPERATOR" title now overlaps the picker pill
`dx7/08-FM`, `dx7-small/08-FM`: the card title "OSC 1 . OPERATOR" is 130 px and the first pill (OSC 1) starts at x = 212, on top of the "R" (`FmInputPages.h:197`; `reserve` from `tabsLeft`). It is the one place the review 13 vocabulary fix introduced a layout fault. At 75 % it is clear of the pill (`dx7-small/08-FM`: pills begin at x = 170, the title ends at 165), so it is a 100 % bug. *Fix:* shorten to "OSC 1" with the engine in the card caption ("operator, ratio"), as every other card does.

### I14-2. Medium: Airwindows cards still show unit-less 0 to 100 % knobs and one-line displays
`neuro/fx-35`: DELAY on AIRWINDOWS reads TIME 100 %, FEEDBACK 0 %, FREQ 50 %, FLUTTER 0 %, MIX 100 % with an ECHOES display of one vertical line; the built-in DELAY shows ms or note lengths. Same as I13-3 (and I12-7). The AIRWINDOWS control is a segment inside the card header, which is a good integration, but the card below it is a different product. *Fix:* each Airwindows algorithm's parameter text function (the 0 to 1 value mapped to the plug-in's own unit string; `airwindowsKnobLabel` in `FxPage.h` already holds names) and a picture per category (delay: tap graph, reverb: decay, filter: response, dynamics: in/out).

### I14-3. Medium: a DX7 voice keeps a normal-looking filter on PLAY
`dx7/01-MAIN`: a flat response and a full knob row for a voice that has no filter. Same as I13-8. The FILTER page (`dx7/05-FILTER`) shows the same flat response with F1 and F2 at 20 kHz and their dimmed cards; the signal-flow reads "SERIAL ... STRINGS". *Fix:* when the filter is fully open and the voice is an operator voice, PLAY's FILTER card says "filter off (a DX7 has none)" and shows the type menu only.

### I14-4. Medium: the page count depends on what the oscillators are
Switching oscillator 1 to physical makes PHYSICAL a real page and the filter flow gain STRINGS / SOUNDBOARD; with a wavetable it is a ghost page (V14-3); the FM page is a different page on DX7. Vital and Serum show the same panels whatever the patch. This is the cost of the integration (one oscillator picker for every engine); the fix is to make the non-active views fold (V14-3, V14-5) rather than draw themselves.

### I14-5. Medium-low: WEST is still its own full card; BODY a collapsed row when off and a full card when on
`neuro/05-FILTER`, `neuro/filter-west`: WEST and BODY sit on the same row with one expanded and one collapsed, a 750 x 220 px hole (V14-7). They are two things of the same shape (an engine sub-box with a switch) that the PHYSICAL page now draws consistently; on FILTER they are different. *Fix:* both as half-width boxes with the same rows, collapsed together or open together.

### I14-6. Medium-low: the DX7 pool chips look like the other chips
`dx7/01-MAIN`: OP LFO and OP PITCH in the chip row with an OP PITCH chip 145 px wide against LFO 1's 90 px. Same as I13-10. *Fix:* a thin separator before them (group "OPERATOR") and equal widths.

### I14-7. Low: the preset caption alternates: "PRESET" over Init, "BASS", "KEYS . DX7 ROM1A"
`HeaderWidgets.h:320`. Same as I13-12. *Fix:* "INIT" patches show "NEW" or nothing.

### I14-8. Low: bare "OFF" in the macro strip
`neuro/01-MAIN`, `neuro/fx-01`: "TALK -> 2 . OFF" in amber (`MacroStrip.h:106-108`). What is off is the target module. Same as S13-10. *Fix:* "TALK -> 2 . 1 off" or the module name.

### I14-9. Low: the two dice
`neuro/10-FX`: the header dice and the FX page's dice-with-"FX" are the same glyph. *Fix:* the FX one a text button "RANDOMISE FX".

### I14-10. Low: SAVE AND LOAD for a factory preset
`confirm/00-confirm`: "Replace your edits? 'Neuro Wobble' has changes that aren't saved." with SAVE AND LOAD as the primary. The commit message says "SAVE AS... AND LOAD" for a factory preset; the snapshot may not set the factory flag *(inferred)*. *Fix:* check the dialog with a factory preset in a live session.

### I14-11. Low: the SEQ chain "KEYS > STRUM > EUCLID > CLIP > VOICES" is text; the tabs beside it are toggles
`neuro/gen-clip-expanded`. Fixed in order, not in widget. Same as I13-7.

### I14-12. Low: the empty state of each engine is drawn differently
Matrix: an illustration and six starter chips; FX: the library; VECTOR: a ghost pad; PHYSICAL: a ghost string; SAMPLE: a dashed box; FM: black tiles and a hint. Six empty-state grammars. *Fix:* one: a dim ghost of the live view, one line, one button (V14-2 to V14-5).

### I14-13. Low: the operator envelope card's level bars are the best new widget, and PLAY's AMP ENV does not have one
`dx7/01-MAIN`: "6 OSCILLATORS PLAY THE OPERATOR ENV" with six bars; the wavetable patch's card shows only the one envelope. Fine, as designed.

---

## Workflow speed (the same five jobs)

1. **Load a sound:** click the preset name; docked browser; up/down; same as Serum, no audition (S14-1).
2. **Shape a wavetable oscillator:** PLAY has FRAME, WARP, UNISON, DETUNE in the strip (3 s); OSC adds the rest at a 117 px row pitch (V14-6).
3. **Route a modulator:** drag a chip onto a knob, drag the ring; same as Vital; 12-route matrix is readable (`neuro/07-MATRIX`).
4. **Build an FX chain:** + ADD EFFECT then the library; the grid is stable; a hole tile (V14-1).
5. **Make a DX7 patch:** FM page with 6 x 6 matrix, operator card and pitch EG; best of its kind (`dx7/08-FM`) apart from the title overlap (I14-1).

## Settled decisions and conventions check

- Settled decisions in `REVIEW-PLAN.md` are not argued, and the PLAY layout is not argued; V14-10 and V14-16 only ask that its fallback tiles and folded strips are as tidy as the rest.
- UI-CONVENTIONS: on/off switches: F2 has none (V14-12), VECTOR now complies. Captions: "drag the graph or the knobs" in the envelope header is a hint in a caption slot (V13-14, half-fixed). Casing: chips comply; SUB pills are values (V14-15). Separators: " . " used throughout. Text that may not fit: the rule is broken by V14-17, with the test green. Vocabulary: OSC n and the engine tag comply; "OUT" has two remaining meanings.

## Top ten, one line each

1. V14-4: the matrix's REMAP dock is a 1500 x 380 px text box (37 % of the page) whenever fewer than about eight routes exist; Vital shows the curve there.
2. V14-3: PHYSICAL on a non-physical oscillator is a full page, 80 % bare, of ghost drawing and a button.
3. V14-2: VECTOR off is a 480 x 480 px pad showing one dot; the page is 60 % bare floor.
4. V14-5: the FM page on one or two oscillators is a 3 x 3 black tile grid and a one-bubble diagram, 46 % bare; only the DX7 case is dense.
5. V14-1: the FX rack ends in a 490 x 205 px dashed ADD EFFECT hole at any odd card count; FREEZE is a paragraph in a black box.
6. V14-6 / V14-7: the OSC page's controls float on a 117 px row pitch, and FILTER's flow and response panels are 470 px tall for a handful of nodes (40 % and 31 % in bare regions against Vital's 18 %).
7. V14-17 / I14-1: text is still cut with the test green: "MODULATES 1" clipped at 75 %, "AMP E..." in the unused pool card, and the new "OSC 1 . OPERATOR" title overlapping the picker pill.
8. I14-2: Airwindows cards still show unit-less 0 to 100 % knobs and one-line displays beside the built-in effects' real units.
9. S14-1 / S14-2: no audition in the 641-preset browser (60 % empty detail panel), and the matrix's VIA column is 12 of 12 empty.
10. I14-3 / V14-12: a DX7 voice keeps a normal-looking filter on PLAY, and Filter 2 still has no switch.

## Score

**Against Vital: 8.9 / 10. Against Serum 2: 8.7 / 10. Integration: 9.2 / 10.**

PLAY, the DX7 FM page, SEQ with the clip roll and the matrix with a dozen routes are at or past the benchmark's density. The rest of the product has to catch up to them before it reads as one commercial product; the single biggest lever is that no page in its default state may be a sentence on a dark panel.
