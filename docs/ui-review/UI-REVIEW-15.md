# UI review 15 (pass 10): ilanaSynth v1.3 against Vital, Serum 2, and itself

One reviewer, three angles, one file. Build: branch `claude/project-thread-smvgfk`, `/home/user/ilanaSynth/build`, head `921c6a8`. Evidence: `/home/user/shots15/{neuro,init,dx7,keys,small,dx7-small,extras,remap,confirm}/` (cited `set/file`), the real Vital and Serum 2 screenshots in `/home/user/refshots/`, and the source in `src/gui/` (file:line where I have it). I read about 45 shots at full size. I could not click or drag, so live behaviour is marked *(inferred)*. The snapshot tool turns OSC 1 physical and back on the OSC page, so "EDITED" after 03-OSC and the "OSC 1 plays a wavetable" state on `keys/04-PHYSICAL` are tool artefacts.

`ilanaSnapshot --uitest`: **`UI TESTS PASSED (0 failures)`**. This time the green is believable for the review-14 cases: `dx7-small/01-MAIN` and `small/01-MAIN` show no clipped text, the FM title no longer overlaps the picker, and the pool cards read whole.

## Verdict and scores

Review 14's fix round landed well. Almost every item aimed at is visibly better: VECTOR shows four live corner pictures and one knob row, PHYSICAL on a non-physical oscillator is a half-height preview with a clear SWITCH TO PHYSICAL, the matrix REMAP dock is always a curve (with quick shapes), the FX rack's lone card goes full width and FREEZE shows a spectrum, the OSC card is four tidy rows, the sample page has a waveform, ruler, key map and START/END flags, a DX7 voice's PLAY filter says FILTER OFF, Filter 2 has a switch and WEST and BODY sit side by side. The cut-text test now covers sibling overlap. Nothing I looked at regressed into something worse.

But measured the same way as review 14, the build is still not at the benchmark on the pages a user opens in the first ten minutes. The mean "bare region" share across the ten main pages on the Neuro patch fell from **37.8 % to 29.9 %** (Vital's four pages: 18.5 %; Serum 2's main page: 6 %). PLAY, the 6-operator FM page and the matrix are at or better than Vital; FILTER, MOD ENV/LFO and SEQ ARP did not change at all (31, 30, 41 %); two of the "fixed" pages are still around 40 % bare (VECTOR off 42 %, PHYSICAL preview 46 %); and the FM page on a one- or two-oscillator patch got worse in a new way (two 170 x 240 px slabs holding one dot each, V15-3). New dead space showed up where the last round added things: a lone FX card leaves a 270 px floor (V15-4), no-display FX cards are a black box with one sentence (V15-5), and WEST on makes the whole FILTER page jump (V15-8).

| Angle | Review 12 | Review 13 | Review 14 | Review 15 | Target |
|---|---|---|---|---|---|
| Against Vital | 8.9 | 9.0 | 8.9 | **9.1** | 9.5 |
| Against Serum 2 | 8.8 | 8.8 | 8.7 | **8.8** | 9.5 |
| Integration | 9.1 | 9.1 | 9.2 | **9.3** | 9.5 |

Why only +0.2 / +0.1 / +0.1 for a big fix round: the fixes were real, but the owner's top question (empty space) is still answered "half-yes". Serum 2 stays lowest because its strengths (a sound you can hear before loading, every panel a display, a wavetable you can see on every oscillator, depth in the browser) are the things this round did not touch.

## What stands between this build and 9.5

The shortest list I can defend. All are layout or small-widget jobs; none is a new engine.

1. **Finish the real-estate job on the four pages that did not move** (V15-1, V15-2, V15-6, V15-7): SEQ ARP lanes (41 % bare, 1100 x 500 px of dark), FILTER's twin 470 px panels (31 %), MOD ENV/LFO's two graphs (30 %), VECTOR off and the PHYSICAL preview (42 / 46 %). Each needs one move: fold the ARP lanes to one row of bars with a hint until a step exists, cap the FILTER response and flow at about 330 px and give the slack to the filter knob row, give the LFO and ENV graphs the same height as the editor beside them, draw VECTOR's pad dimmed with the corner waves faded into it.
2. **Fix the three pages that the last round made odd**: FM with one or two oscillators (tall slabs, V15-3), the FX rack with one card (floor, V15-4), the no-display FX cards (V15-5).
3. **Stop pages jumping**: WEST on re-flows FILTER (V15-8); macro tiles swap their text for + ASSIGN depending on which are idle (V15-17).
4. **Serum's depth, once**: an audition in the browser and a detail panel that holds more than a tag list (S15-1), and a VIA column that earns its 60 px (S15-2).
5. **Close the vocabulary seams**: DX7 modulator "DEPTH" against the written rule "OUTPUT" (I15-1), physics LFOs' own words and no DRIVES list (I15-3), string controls split over OSC and PHYSICAL (I15-4), a DX7 FILTER page that does not say the filter is off (I15-2).

Do 1 to 3 and I would give 9.3 / 9.1 / 9.4; with 4 and 5, 9.4 to 9.5 on each. Without 1 the build stays at 9.1 at best, because ARP, FILTER and MOD are three of the seven tabs.

---

# Review 14's items: fixed since review 14 / still open

### Real-estate and Vital items (V14-n)

| # | Review 14 item | Status |
|---|---|---|
| V14-1 | FX rack ends in a 490 x 205 px dashed hole at odd card counts; FREEZE is a paragraph | **Fixed**: the lone card takes the full row, the ADD EFFECT bar is one full-width row (`neuro/10-FX`), FREEZE shows a live spectrum with a HOLD switch (`neuro/fx-12`). New side effect: V15-4, V15-5 |
| V14-2 | VECTOR is 74 % empty when off, 54 % when on | **Mostly fixed**: four live corner pictures, one knob row (`neuro/02-VECTOR`, `init/vector-page`). 62 % empty / 42 % bare when off (was 73 / 65 by my re-measure), 57 / 36 when on. The pad is still a 720 x 720 px dark square with one dot (V15-6) |
| V14-3 | PHYSICAL on a non-physical oscillator is a full page of ghost drawing | **Mostly fixed**: the string is a 1450 x 110 px preview, SWITCH TO PHYSICAL is at the top right, the controls below are dimmed (`neuro/04-PHYSICAL`). Bare 80 % to 46 %. The page is still a screen of dimmed knobs with a 500 x 220 px empty right third (V15-7) |
| V14-4 | Matrix REMAP dock is a text box that eats 37 % of the page | **Fixed**: always a curve with quick shapes and a text column (`keys/07-MATRIX`, bare 45 % to 33 %). The text column is still prose (V15-18) |
| V14-5 | FM page on one or two oscillators is 3 x 3 black tiles | **Half fixed, new bug**: only playing oscillators get rows now, but the rows are stretched to 240 px tall (`init/08-FM`, `keys/08-FM`); bare still 46 %. See V15-3 |
| V14-6 | OSC panel floats: 117 px row pitch, 40 % bare | **Fixed in form**: four full rows on an 88 px pitch (`neuro/03-OSC`). Bare 34 % to 31 %: the rows now fill but each is half empty to the right (V15-9) |
| V14-7 | FILTER's response and flow are 470 px tall for 4 to 7 nodes | **Half fixed**: flow blocks are bigger and the BALANCE knob is inside it, the response is unchanged (`neuro/05-FILTER`, bare 33 % to 31 %). See V15-1 |
| V14-8 | SEQ ARP card is 500 px of empty lanes | **Still open** (`neuro/09-ARP-SEQ`: same lanes, hint button added in review 13). The control column to the right is tighter, which makes the 1100 px lane block look emptier |
| V14-9 | VOICE drawer wastes a third on a help paragraph | **Half fixed**: SPREAD & DRIFT merged in; the paragraph is now a one-line note in the tab band (`neuro/03-OSC`: still 830 px of a 60 px band). Fine, but see V15-12 |
| V14-10 | PLAY PATCH / OUTPUT tiles cramped | **Fixed** (`keys/01-MAIN`: PATCH tile is a full 330 px graph) |
| V14-11 | SEQ control column floats, ARP knobs 100 px apart | **Fixed in form**: RATE and MODE on one row, a 2 x 2 knob block below (`neuro/09-ARP-SEQ`). The block still sits in a 430 x 480 px column with 40 % bare |
| V14-12 | Filter 2 has no switch | **Fixed** (`neuro/05-FILTER`: switch at the card's right) |
| V14-13 | Header is 10 % of the window and a second row | **Still open, accepted** (105 px: logo, preset, pills; status row below). Not argued again |
| V14-14 | COLOUR on SUB + NOISE reads disabled at 100 % | **Still open** (`neuro/01-MAIN`: grey ring, grey label at 100 %; see V15-14) |
| V14-15 | SUB pills are mixed case | **Fixed by decision**: now documented as values (UI-CONVENTIONS "Values keep their own case") |
| V14-16 | Unused SUB + NOISE / OSC rows keep a 280 px gap in DX7 PLAY | **Fixed**: the unused SUB row is a single 56 px line (`dx7/01-MAIN`) |
| V14-17 | Text still cut, UI test green | **Fixed**: `small/01-MAIN`, `dx7-small/01-MAIN`, `dx7/06-ENV-LFO` all whole; test adds covered-text check (commit `921c6a8`) |
| V14-18 | Hover line and macro row share a strip | **Not re-checked** |
| V14-19 | Dashed-tile buttons everywhere | **Half fixed**: the FX add bar is one row, ADD MODULATION is one row, ADD OSC 4 is a row; + MACRO is still a tile (`neuro/01-MAIN`) |

### Serum 2 items (S14-n)

| # | Review 14 item | Status |
|---|---|---|
| S14-1 | No audition in the 641-preset browser | **Still open** (`extras/extra-browser-docked`: no HEAR / play button anywhere in the row or the detail panel) |
| S14-2 | VIA column is empty on 12 of 12 rows | **Half fixed**: 60 px, a small dashed "+" (`neuro/07-MATRIX`); still 12 of 12 empty |
| S14-3 | Sample page empty state repeats the 440 x 440 px dashed box | **Fixed**: a full-width ghost waveform with LOAD in it (`neuro/osc-sample-empty`) |
| S14-4 | Loaded sample: no velocity map, loop markers, waveform | **Mostly fixed**: waveform, ruler, START / END flags, key zones C1 to C5 (`neuro/osc-sample-sfz`). No velocity map; loop markers appear only with LOOP on *(inferred, not shot)* |
| S14-5 | Physics LFOs speak their own language and lose the DRIVES list | **Still open** (`neuro/lfo-sim-lorenz`: SIGMA, RHO, BETA, SEED, TRIGGER, OUT 1 AXIS, FIRE; no DRIVES list) |
| S14-6 | Table browser's HEAR far from the tile | **Not re-checked** (`init/table-browser` not read) |
| S14-7 | Macro strip shows four of eight | **Still open, accepted** |
| S14-8 | Algorithm tiles are numbers | **Fixed**: DX7 tiles are operator-graph pictures, basic ones are 2-op pictures (`dx7/08-FM`) |
| S14-9 | SAVE's TAGS field and chips at once | **Fixed in form**: the field mirrors the chips as typed text (`extras/extra-save-as`); still two inputs for one thing (S15-9) |
| S14-10 | Matrix with REMAP dock cuts a row | **Fixed** (`keys/07-MATRIX`: five rows, dock under them) |
| S14-12 | "OUT 2" / "OUT" / "OUT" | **Still open** (LFO tile "OUT 2", meter "OUT", FM "OUT" in the node; low) |
| S14-13 | No keyboard in the window by default | **Accepted** |

### Integration items (I14-n)

| # | Review 14 item | Status |
|---|---|---|
| I14-1 | "OSC 1 . OPERATOR" overlaps the picker | **Fixed** (`dx7/08-FM`: "OSC 1" then the pills; `FmInputPages.h`) |
| I14-2 | Airwindows cards show unit-less knobs and one-line displays | **Mostly fixed**: Airwindows cards show their own picture or, where none exists, a captioned note (`neuro/fx-16`, `fx-08`). Knob units were fixed (`HAAS DELAY 14 ms`, `TILT LEVEL 0.0 dB`). The note boxes are the new dead space (V15-5) |
| I14-3 | A DX7 voice keeps a normal-looking filter on PLAY | **Fixed on PLAY** (`dx7/01-MAIN` "FILTER OFF · a DX7 voice has none"); **not on the FILTER page** (I15-2) |
| I14-4 | Page count depends on what the oscillators are | **Improved**: PHYSICAL is a dimmed preview, not a ghost page. Still a page that exists for one engine |
| I14-5 | WEST its own full card; BODY a collapsed row when off | **Fixed**: WEST and BODY are two halves of one row (`neuro/05-FILTER`, `filter-west`). Side effect: V15-8 |
| I14-6 | DX7 pool chips look like the other chips | **Still open, low** (`dx7/01-MAIN`: OP LFO, OP PITCH chips are wider and have a different dot) |
| I14-7 | The preset caption alternates "PRESET" / "BASS" / "KEYS . DX7 ROM1A" | **Fixed**: "NEW PATCH" over Init (`init/01-MAIN`), category over presets |
| I14-8 | Bare "OFF" in the macro strip | **Fixed**: "· 1 OFF" after the target count (`keys/04-PHYSICAL`); see I15-9 |
| I14-9 | The two dice | **Fixed**: the FX dice is a RANDOMISE FX text button (`neuro/10-FX`) |
| I14-10 | SAVE AND LOAD for a factory preset | **Not re-checked** (`confirm/` not read in full) |
| I14-11 | SEQ chain is text; tabs beside it are toggles | **Half fixed**: STRUM / EUCLID / CLIP in the chain are coloured, current ones lit (`neuro/gen-clip-expanded`); chain words are still text, the switches are the tab pills |
| I14-12 | Empty state of each engine drawn differently | **Improved**: sample, vector and physical empty states now all show a ghost of the real picture. FM's and ARP's differ (V15-3, V15-1) |
| I14-13 | PLAY's AMP ENV lacks the operator card's level bars | **Still open, accepted** |

---

# Screen real estate, page by page

**Method, same as review 14.** I measured the page body (tab bar to chip row, 1546 x 834 px at 1.5x) in blocks of about 1 % of the window width (16 px physical here, 14 px logical in review 14). A block is empty when its colour range is 14 or less per channel (flat background only; no text, line, knob or fill). "Bare" is the share of blocks inside an empty region at least four blocks square (about 55 px). I scripted it so the numbers are repeatable (`/tmp/claude-0/s/m.py` in the review session). Check on the benchmark: my script gives Vital VOICE 36 / 18, EFFECTS 33 / 12, MATRIX 40 / 26, ADVANCED 40 / 18 (review 14: 34 / 18, 33 / 14, 40 / 27, 39 / 21), Serum 2 main 22 / 6 (review 14: 15 / 0). I also re-ran the script on review 14's own screenshots so before and after use the same code: where the two "before" columns differ, the re-measured one is the fair one. Dark display wells with no signal count as empty, which is how a user sees them. This is a proxy; what the empty area is matters, and I name it.

**Benchmark.** Vital: 33 to 40 % empty, 12 to 26 % bare. Serum 2 main OSC page: 22 % empty, 6 % bare. Vital's whole window has no region 55 px square with nothing in it except display wells and the matrix's remap graph (which carries a curve). Mean over four Vital pages: 18.5 % bare.

**Summary.** Mean bare share over the ten main pages on the Neuro patch: **37.8 % (review 14, re-measured) to 29.9 % (now)**, against 18.5 % for Vital. Pages at the benchmark now: PLAY 2 osc (11), PLAY physical (8), matrix with 12 routes (4), FM with 6 operators (10), SEQ with the clip roll (19), FILTER with WEST on (13). Pages still 30 % or worse: ARP (41), VECTOR off (42), PHYSICAL preview (46), FM with 1 to 2 oscillators (35 to 46), OSC (31 to 45), FILTER (31), MOD ENV/LFO (30), FX with one card (52), the empty matrix (74, accepted), the preset browser (45).

| Page / state | R14 published (empty / bare) | R14 re-measured | **R15 (empty / bare)** | What the empty area is now | Benchmark's equivalent |
|---|---|---|---|---|---|
| **PLAY, 2 oscillators** (`neuro/01-MAIN`) | 41 / 12 | 37 / 11 | **37 / 11** | OSC 1 strip's picture (a 290 x 160 px sine), the right of SUB + NOISE's COLOUR knob; gap between rows | Vital VOICE 36 / 18; **better than benchmark** |
| PLAY, 1 oscillator (`init/01-MAIN`) | 48 / 16 | 43 / 15 | **43 / 15** | OSC 1's picture and SUB + NOISE's picture (330 x 250 px each, one saw / one square) | Vital VOICE 36 / 18; at benchmark |
| PLAY, 6 operators DX7 (`dx7/01-MAIN`) | 40 / 14 | 35 / 11 | **36 / 12** | The 160 px between each strip's FINE and OUTPUT knobs; the OP ENV card's bars | Serum main 22 / 6 (denser) |
| PLAY, physical (`keys/01-MAIN`) | 40 / 4 | 37 / 5 | **40 / 8** | The PATCH tile's graph (700 x 190 px, mostly flow lines) | at benchmark |
| PLAY, six strips (`neuro/added-osc-MAIN`) | n/a | 38 / 6 | **38 / 6** | none | at benchmark |
| **VECTOR, off** (`neuro/02-VECTOR`) | 74 / 60 | 73 / 65 | **62 / 42** | The pad: a 720 x 720 px square, one dot, a faded wave; the POSITION AND MOTION box's right third | no Vital equivalent; Serum's is a display in the OSC panel |
| VECTOR, on (`init/vector-page`) | n/a | 67 / 56 | **57 / 36** | The pad's four grid cells around the path | same |
| **OSC, wavetable** (`neuro/03-OSC`) | 52 / 40 | 51 / 34 | **48 / 31** | Right half of every row: SHAPE row has controls only to 80 %, the VOICE drawer's STEREO column; row gaps are gone | Serum OSC panel (`serum2-wavetable_osc.png`) has no row with a gap |
| OSC, DX7 operator (`dx7/03-OSC`) | 62 / 50 | 62 / 43 | **60 / 45** | The OP ENV graph is 1330 x 300 px (content, but a plain 4-segment line); the PITCH & WAVE row is 10 controls in 1330 px with 150 px under each | none |
| OSC, physical (`keys/03-OSC`) | n/a | 60 / 43 | **59 / 44** | The picture is 440 x 530 px with the string a thin line in the middle; the STRING row has three controls in 900 px | Serum swaps panel in place |
| OSC, sample empty (`neuro/osc-sample-empty`) | 63 / 49 | 63 / 45 | **58 / 41** | The ghost waveform is drawn but the SAMPLE row below is 8 dimmed controls on 1500 px | Serum sample panel (`serum2-sample_osc.png`): waveform plus 8 controls |
| OSC, sample loaded (`osc-sample-sfz`) | 65 / 52 | 61 / 41 | **55 / 31** | Between the waveform and the key map (30 px); the controls row | same |
| **PHYSICAL page, non-physical OSC** (`neuro/04-PHYSICAL`) | 87 / 80 | 84 / 80 | **62 / 46** | A 1450 x 110 px preview string, then a screen of dimmed knobs; right 500 x 220 px of the STRING box | Serum: no page |
| PHYSICAL page, live (`neuro/physical-page`, `keys/04-PHYSICAL` states) | 57 / 38 | 53 / 37 | **54 / 36** | EXCITER row ends at 62 % of the width; the STRING row at 56 % | none |
| **FILTER** (`neuro/05-FILTER`) | 56 / 31 | 55 / 33 | **54 / 31** | RESPONSE: 890 x 470 px, one curve; SIGNAL FLOW: 600 x 470 px, nodes in a 120 px column, a 400 px blank to the right of F1 / F2; F2's six dimmed knobs | Vital VOICE's filter: 350 x 180 px with six controls |
| FILTER with WEST on (`neuro/filter-west`) | 52 / 21 | 49 / 21 | **44 / 13** | The response graph shrinks to 250 px; flow keeps a blank band | none |
| FILTER, DX7 voice (`dx7/05-FILTER`) | n/a | 56 / 34 | **54 / 33** | A flat line in a 890 x 470 px graph for a filter that is off | none |
| **MOD ENV/LFO** (`neuro/06-ENV-LFO`) | 57 / 30 | 54 / 30 | **54 / 30** | The LFO graph (700 x 280 px, a triangle) and the ENV graph (700 x 180 px, one trapezoid) and the first-card chips' floor | Vital: LFO graph 550 x 200 px, ENV graph 550 x 150 px, same ratio; at benchmark for displays |
| MOD LFO physics (`neuro/lfo-sim-lorenz`) | 55 / 26 | 51 / 24 | **51 / 24** | The phase plot is a 280 px square in a 700 px well | Serum's LFO 8 is one 600 px display |
| **MATRIX, 12 routes** (`neuro/07-MATRIX`, `remap/remap-matrix`) | 38 / 7 | 34 / 5 | **31 / 4** | The 60 px VIA column (12 dashed "+"), the left 40 px of every row | Vital MATRIX 40 / 26: **ilana is denser** |
| **MATRIX, 5 routes** (`keys/07-MATRIX`) | 63 / 46 | 61 / 45 | **52 / 33** | The 650 x 390 px text column to the right of the curve (prose, five quick-shape tiles under it) | Vital's dock holds the curve at full width |
| MATRIX, empty (`init/07-MATRIX`) | 86 / 76 | 81 / 74 | **81 / 74** | The empty-state illustration (accepted) | Vital shows 12 blank rows |
| **FM, 1 oscillator** (`init/08-FM`, `keys/08-FM`) | 61 / 46 | 62 / 48 | **60 / 46** | Two 170 x 240 px cells with a dot each; a one-node diagram with two "OFF" dashed circles; 880 x 300 px diagram well | no equivalent (Serum's FM is a warp) |
| **FM, 2 oscillators** (`neuro/08-FM`) | 60 / 42 | 58 / 42 | **56 / 35** | Diagram: the third node dashed and OFF, the arrow 200 px long; matrix cells 170 x 150 px with one dot each (3 of 6 empty) | none |
| FM, 6-operator DX7 (`dx7/08-FM`) | 43 / 14 | 35 / 10 | **35 / 10** | none to speak of | at benchmark |
| **SEQ, ARP off** (`neuro/09-ARP-SEQ`) | 54 / 21 | 60 / 41 | **60 / 41** | Three lanes 1100 x 500 px of empty bars under a one-line hint; the GENERATE card's three boxes are 40 % bare (a 130 x 70 px floor under each) | Serum's ARP is a 350 px keyboard strip |
| SEQ, CLIP open (`neuro/gen-clip-expanded`) | 31 / 6 | 46 / 19 | **46 / 19** | The roll's blank grid beyond the notes (content) | Serum CLIP: same |
| **FX, 3 cards** (`neuro/10-FX`) | 65 / 48 | 60 / 37 | **56 / 28** | The OTT card's 1170 x 210 px in/out graph, one straight line in it; the sentence wells (V15-5) | Vital EFFECTS 33 / 12 |
| FX, 2 cards (`keys/10-FX`) | 62 / 40 | 58 / 32 | **57 / 31** | LIMITER's 1040 x 210 px graph with a straight line; REVERB's right third | Vital |
| **FX, 1 card** (`dx7/10-FX`) | n/a | 69 / 52 | **68 / 52** | A **1546 x 270 px blank floor under OUTPUT**; the REVERB graph is 540 x 260 px with two diagonal lines | Vital shows every module always |
| FX, empty rack (`init/10-FX`) | 63 / 35 | 57 / 30 | **57 / 30** | The library grid (content) | Vital shows all modules |
| Preset browser, docked (`extras/extra-browser-docked`) | 64 / 45 | 60 / 45 | **60 / 45** | The detail panel: 370 x 700 px, a name, tags, four macro names; ~330 px of floor under | Serum: browser plus ARTIST / DESC and audition |
| Window chrome | 105 px header (10 %), ~130 px bottom rows (12 %) | | **same** | Header: logo, preset, three pills; status row; macro strip 90 px | Vital header 50 px; Serum 110 px with macros inside |

**Where the first screen is spent, in one line each.** Vital VOICE: 3 oscillators + 2 filters + 4 mod displays + keys in 100 % of the window. ilanaSynth PLAY: 3 strips + SUB + filter + env + LFO in 100 % of the window: the same density. Everything else is a single-purpose page; the three that sit at 30 %+ are the three where a graph is sized by the page, not by the data (FILTER, MOD ENV/LFO, SEQ ARP). The cure is the same each time: the display's size follows what is in it.

---

# Part 1: Against Vital (V15-n)

### V15-1. High: FILTER, MOD ENV/LFO and SEQ ARP did not move (31, 30, 41 % bare)
**Wrong.** Review 14 listed all three (V14-7, V14-8 and the ENV/LFO row). Round 9 changed FILTER's flow blocks and added a switch, but the two top panels are still 890 x 470 and 600 x 470 px (`neuro/05-FILTER`): a single low-pass curve and seven nodes in a 120 px column with a 400 px blank to their right. On MOD, the LFO graph (700 x 280) and the ENV graph (700 x 180) are the same size they were. The ARP lanes are still three 1100 x 500 px empty bars (`neuro/09-ARP-SEQ`).
**Benchmark.** Vital's filter display is 350 x 180 px with six controls under it, and its LFO and ENV displays take the width they need (550 px) and share it with the controls (`vital-voice-main.png`).
**Fix.** FILTER: cap RESPONSE and FLOW at about 340 px of height, put FILTER 1 and FILTER 2's knob rows directly under them and let the knob cards grow (the knobs are 55 px; they can be 75). ARP: show one lane (VEL) with the other two folded to a 22 px header each (PITCH, STEP LEN) until a step is drawn; the control column then gets the freed 330 px for a larger ARP keyboard-style pattern. MOD: make the LFO graph and the LFO editor equal in height (280 px each, as now) but let the AMP ENV graph take 260 px, with the DELAY...KEY RATE knobs on the same row as Vital's ENV (`vital-voice-main.png` ENV 1). Source: `FilterVectorPhysicalPages.h` layout (`resized`, around 78 and 716), `GenerativeWidgets.h:813` for the ARP hint.
**Evidence.** `neuro/05-FILTER`, `neuro/06-ENV-LFO`, `neuro/09-ARP-SEQ`; refshots Vital voice.

### V15-2. High: the FX rack's cards size their graph by the page, not by the effect
**Wrong.** OTT's IN / OUT graph is 1170 x 210 px with one straight diagonal in it (`neuro/10-FX`); LIMITER's is 1040 x 210 px (`keys/10-FX`); REVERB's decay plot is 540 x 260 px with two straight lines (`dx7/10-FX`). The graphs are real, but the line carries about as much information as a label.
**Benchmark.** Vital's compressor display is 380 px with three live band bars; its distortion display is 135 px; chorus 180 px (`vital-effects.png`). Each display is sized for its data and the rest of the card is knobs.
**Fix.** Cap graph width at about 520 px, give the rest of a wide card to a second row of knobs or to the effect's secondary controls (OTT's time, upward / downward ratio; LIMITER's lookahead). On a lone card, the spare is better spent making the card taller than wider.
**Evidence.** `neuro/10-FX`, `keys/10-FX`, `dx7/10-FX`; `FxPage.h` card layout (`resized` around 540 to 600).

### V15-3. High: the FM page on a patch with one or two oscillators has two 170 x 240 px cells
**Wrong.** Round 9 dropped silent oscillators from the matrix, which is right, but the row height for a one- or two-row matrix is raised to 168 px (`FmInputPages.h:707`: `count <= 2 ? 168 : 130`) and the columns share the whole 600 px width, so the cells are 170 x 240 px slabs each holding a 12 px dot (`init/08-FM`, `keys/08-FM`). The NOISE FM row gets the same size. Bare share is 46 %, the same as before the fix. On `neuro/08-FM` (two oscillators) the three cells with no route are 170 x 150 px of dark.
**Benchmark.** Vital's modulation matrix rows are 37 px and Serum's FM is a knob. Neither draws a 170 px cell for "nothing".
**Fix.** Cell size is a square of at most 100 px; centre the matrix and show the algorithm diagram above at the width left. With one oscillator, replace the whole matrix with a line "FM needs a second oscillator: ADD OSC 2" plus the NOISE FM and extras row, rather than an empty 1x1 grid.
**Evidence.** `init/08-FM`, `keys/08-FM`, `neuro/08-FM`; `FmInputPages.h:707`.

### V15-4. Medium-high: a lone FX card leaves a 270 px blank floor
**Wrong.** With one effect (`dx7/10-FX`, a one-reverb preset) the rack is a 330 px card, the ADD EFFECT bar, the OUTPUT strip and then 270 px of nothing down to the chip row (68 % empty, 52 % bare, the same as before). `FxPage.h:551-560` has the intent ("the rack's spare height goes to its rows, so the page doesn't end in a blank band") but it only stretches when the stack is shorter than its area; with a single card it caps.
**Benchmark.** Vital's rack is always full (every module is shown, off or on).
**Fix.** When the rack has 1 to 2 cards, show the next suggested effects as a row of dimmed compact cards (as Vital does with its module list) below ADD EFFECT, or let the lone card's graph and knobs grow to 520 px (but see V15-2). A short "TRY: DELAY · CHORUS · EQ" row of buttons is a 60 px compromise.
**Evidence.** `dx7/10-FX`; `FxPage.h:548-560`.

### V15-5. Medium-high: the no-display FX cards are a black box with one sentence
**Wrong.** HAAS, TILT, TRANCE GATE (partly) and other effects without a graph show a 290 x 210 px well titled "WHAT IT DOES" with two lines of text at the top and 150 px of black below (`neuro/fx-08`, `fx-18`, `fx-16`). That is the very pattern review 14 said no panel may have ("a panel whose only content is a sentence"). The help text is useful, but it belongs in a tooltip or a one-line caption.
**Benchmark.** Vital draws a display for every effect (even Reverb's is a live decay) and Serum puts a graph or a spectrum on each module.
**Fix.** Draw a small picture for the simple effects: Haas as two offset pulses (L, R), Tilt as a seesawing line, Width as two arcs, Trance Gate as its step row (already present below). Drop the text well; put the sentence in the card's tooltip. If a picture is not feasible, shrink the well to a 60 px caption strip and widen the knobs.
**Evidence.** `neuro/fx-08`, `neuro/fx-18`, `neuro/fx-16`; `FxPage.h:1568-1582`.

### V15-6. Medium: VECTOR off is still a 720 px dark square with a dot
**Wrong.** The corners and knob row are good now. The pad itself (`neuro/02-VECTOR`) is a 720 x 720 px square with a grid, a faded wave behind a dot and "VECTOR OFF · switch on to mix the corners" in the lower middle. It is 42 % bare against Vital's 18.
**Benchmark.** Serum's vector-style display (the OSC panel) always shows the live blend.
**Fix.** Draw the pad off as the real thing, dimmed to 40 %: the four corner waves fading across it so the state "what would I get" is visible; keep the dot. Or make the pad 520 px and give the extra 200 px to the corner picks.
**Evidence.** `neuro/02-VECTOR`, `init/vector-page`; `VectorPad.h:81`.

### V15-7. Medium: the PHYSICAL preview is 46 % bare, mostly dimmed knobs
**Wrong.** The preview strip and the SWITCH TO PHYSICAL button are good. Below them the STRING & BODY card is a full screen of dimmed controls (`neuro/04-PHYSICAL`: a 1250 x 300 px region of grey knobs and a 500 x 220 px empty right third of the STRING box). Dimmed knobs read as empty.
**Benchmark.** Serum swaps the OSC panel in place when the type changes; there is no page for a type the oscillator isn't.
**Fix.** When the selected oscillator is not physical, show only the preview strip and BODY / SOUNDBOARD (the two boxes that act on any oscillator through the keys body) and fold STRING & EXCITER to a one-line "STRING & EXCITER: switch OSC 1 to Physical to edit" row. That ends the page at about 55 % of its height.
**Evidence.** `neuro/04-PHYSICAL`, `keys/04-PHYSICAL` (tool-artefact state, same page).

### V15-8. Medium: switching WEST on re-flows the whole FILTER page
**Wrong.** Off: RESPONSE and FLOW are 470 px tall and WEST is a 60 px strip (`neuro/05-FILTER`). On: they shrink to 250 px and WEST becomes a 280 px card (`neuro/filter-west`). The graph, the flow nodes and the filter cards all jump 220 px (a "thing that moves around" in the owner's list). BODY's switch does the same.
**Benchmark.** Vital's filter model dropdown changes controls in place; the display stays.
**Fix.** Keep the page's geometry fixed: reserve a 280 px lower band (WEST and BODY open or collapsed) and size RESPONSE and FLOW at 250 to 300 px in both states. It also cures V15-1's panel size.
**Evidence.** `neuro/05-FILTER`, `neuro/filter-west`.

### V15-9. Medium: the OSC card's four rows each end at 60 to 80 % of the width
**Wrong.** `neuro/03-OSC`: SHAPE has FRAME, WARP, WARP AMT, SPECTRAL, SPEC AMT (5 controls) and the last 200 px empty; WARP CHAIN has the same five with a blank right; PITCH & LEVEL and UNISON fill. The group labels take a 150 px gutter on the left. Rows are even but not equally full.
**Benchmark.** Serum's panel has 12 controls in 450 x 540 with no row more than a control short (`serum2-wavetable_osc.png`).
**Fix.** Merge WARP CHAIN into SHAPE as a second row of the same five columns (it already shares the column grid), or give FRAME a 2-column wide knob. Keep the left labels but at 110 px.
**Evidence.** `neuro/03-OSC`, `small/03-OSC`.

### V15-10. Medium: the DX7 OSC page's OP ENV is a 1330 x 300 px four-segment line
**Wrong.** `dx7/03-OSC`: the op envelope graph is as wide as the window with a thin line and one handle; below it ten controls on 1330 px.
**Benchmark.** Vital's ENV graph is 550 px and the controls sit beside it.
**Fix.** Graph 700 px, with the DX7's six envelope numbers (R1 to R4, L1 to L4) as a column to its right; PITCH & WAVE then fits under them without a 150 px stretch. The FM page already has this arrangement (`dx7/08-FM`).
**Evidence.** `dx7/03-OSC`, `dx7/08-FM`.

### V15-11. Low-medium: the OSC page's physical state keeps the 440 x 530 px picture with a thin string
**Wrong.** `keys/03-OSC`: the picture is a tall 440 x 530 px box in which a 4 px string sits mid-height and a hammer drops from the top. It is a real, animated picture but 80 % of it is dark. The STRING row to its right has three controls in 900 px.
**Fix.** Make the picture 440 x 330 px and add the EXCITER row (EXCITE POS, HAMMER, BOW PRESS) under the STRING row, so the controls live on the OSC page and PHYSICAL's page is the same widget at full width (see I15-4).
**Evidence.** `keys/03-OSC`, `neuro/osc-sympathetic`.

### V15-12. Low-medium: the VOICE drawer's help sentence is still a sentence
**Wrong.** `neuro/03-OSC`: "MODE sets how notes share voices: POLY plays chords, MONO and LEGATO one note at a time. GLIDE slides the pitch from the last note." sits in the tab band at about 12 px, 830 px wide. It is in its own corner, not under the controls it describes.
**Fix.** Tooltips on MODE and GLIDE carry it; delete the band text (`OscPage.h:1417`).
**Evidence.** `neuro/03-OSC`, `OscPage.h:1417`.

### V15-13. Low: the matrix remap dock's right column is prose
**Wrong.** `keys/07-MATRIX`: next to the curve, a 310 x 390 px column says "ROW 1 / HAMMER / → OSC 1 > Hammer / Amount +70 %, unipolar / Left to right... / Click to add a point...". The row is already highlighted and named in the title strip of the dock ("REMAP · HAMMER → OSC 1 > Hammer"), so half of it repeats.
**Fix.** Drop the big restated source / destination, keep the two hint lines at small size, and move the nine quick shapes up under the curve picker so the curve gets the width.
**Evidence.** `keys/07-MATRIX`.

### V15-14. Low: COLOUR on SUB + NOISE still reads disabled at 100 %
**Wrong.** `neuro/01-MAIN`: the grey ring and dim label at 100 % look the same as the dimmed F2 knobs (V14-14 not fixed). NOISE is 0 %, so the knob is dimmed because noise is off; its value text stays bright.
**Fix.** Dim COLOUR's ring and label and its value (`100 %` greyed), or hide the number until NOISE is above 0.
**Evidence.** `neuro/01-MAIN`, `init/01-MAIN`.

### V15-15. Low: the ARP's control column ends in empty floor
**Wrong.** `neuro/09-ARP-SEQ`: RATE, MODE, STEPS, GATE, OCTAVES, CHANCE take 430 x 360 px of a 480 px column; the last 120 px are empty. After V15-1's lane fold this column should grow, not float.
**Evidence.** `neuro/09-ARP-SEQ`.

### V15-16. Low: the ENV/LFO page's two pool rows have different heights
**Wrong.** `neuro/06-ENV-LFO`: LFO tiles are 80 px tall, ENV tiles 70 px; the "+" tile at the end is a 52 x 80 px tile in one and 52 x 70 in the other. Cosmetic.
**Evidence.** `neuro/06-ENV-LFO`.

### V15-17. Medium: macro tiles change shape with state
**Wrong.** On Init, MACRO 1 shows "+ ASSIGN" and MACRO 2 to 4 show "0 %" (`init/01-MAIN`); on the vector page MACROs 2 and 4 show + ASSIGN and 1 and 3 show a wave icon (`neuro/vector-page` / `init/vector-page`); on Neuro they show "TONE → 2 · 1 OFF" (`keys/04-PHYSICAL`). The comment at `MacroStrip.h:204` says only the first idle macro keeps its button, but the vector shot shows two, so the rule is "whichever is idle", and a tile's width and text move with its state.
**Benchmark.** Vital's macros are four fixed tiles; Serum's are eight fixed tiles with a count badge.
**Fix.** Every macro tile has the same two lines: the name and its value, with a 20 px "+" glyph at the right when it has no route, and "→ 2" when it has. No tile swaps its text for a button.
**Evidence.** `init/01-MAIN`, `neuro/vector-page`; `MacroStrip.h:181-204`.

### V15-18. Low: the chip bar says "+13 v" and the pools say "2 MORE"
**Wrong.** `neuro/07-MATRIX` chip bar: "+13 v" with coloured dots; `dx7/01-MAIN` PLAY ENVELOPE: "2 MORE". Review 13's item (V13-16) is still half-done.
**Fix.** One wording: "+13 MORE v".
**Evidence.** `neuro/07-MATRIX`, `dx7/01-MAIN`.

### V15-19. Low: the PLAY OSC strip's TO OUTPUT subtitle and the picture's frame marker clash on OSC 2
**Wrong.** `neuro/01-MAIN`: OSC 2's subtitle "TO OUTPUT, MODULATES 1" is 9 px grey capital text; at 75 % (`small/01-MAIN`) it is readable but small and its colour is the dimmest on the card. Not clipped; low priority.
**Evidence.** `neuro/01-MAIN`, `small/01-MAIN`.

---

# Part 2: Against Serum 2 (S15-n)

### S15-1. Medium-high: no audition in the 641-preset browser, and a detail panel 60 % empty
**Wrong.** Unchanged from review 14 (`extras/extra-browser-docked`): a list row has a star, a name, tags and a category chip; the detail panel (370 x 700 px) holds the name, "BASS", "by ilanaSynth", six tags and four macro names, then 330 px of floor. There is no way to hear a preset before loading it, and no preview picture.
**Benchmark.** Serum 2's browser has ARTIST / DESC and a play-note audition; Vital's shows a preset's description.
**Fix.** A PLAY C3 button in the detail panel header (and Space on a selected row) that auditions the selected preset on a throwaway voice; a small 2-second waveform thumbnail rendered once per preset on first view (cached); the comment and macro knobs (read-only) under the tags.
**Evidence.** `extras/extra-browser-docked`, `extras/extra-browser-dx7`.

### S15-2. Medium: the VIA column is a 60 px column of 12 empty dashed "+"
**Wrong.** `neuro/07-MATRIX`: narrower, but still 12 of 12 empty with a tiny "+" in the cell. A column that is empty on every row teaches nothing.
**Benchmark.** Vital's matrix has BIPOLAR, STEREO and MORPH icon columns, all of them used icons; Serum's matrix has a "via" only when set.
**Fix.** Hide the column until any row has a VIA or the user presses a "VIA" header toggle; or merge VIA into the AMOUNT cell as a small "via MOD WHEEL" chip that appears when set and as a hover affordance when not.
**Evidence.** `neuro/07-MATRIX`.

### S15-3. Medium: the sample page's controls are a dimmed 1500 px row of eight
**Wrong.** `neuro/osc-sample-empty`: the ghost waveform is good, but under it TUNED, LOOP, REVERSE, START, END, FADE IN, FADE OUT are dimmed and spaced 190 px apart with a left group label, then PITCH & LEVEL on a second row, then UNISON and CHORD. The page is 41 % bare.
**Benchmark.** Serum's sample panel (`serum2-sample_osc.png`) has all eight controls in one 100 px strip under the waveform.
**Fix.** The SAMPLE row in a 100 px strip right under the wave (START / END are on the wave itself already, so drop those two knobs when a sample is loaded or keep them as readouts).
**Evidence.** `neuro/osc-sample-empty`, `neuro/osc-sample-sfz`, refshot Serum sample.

### S15-4. Medium: no loop markers or velocity map on the sample page
**Wrong.** The key map is there; the velocity layers of an SFZ are not drawn; loop points appear only when LOOP is on *(inferred)*. A Serum user expects the loop region on the wave as soon as the sample is loaded.
**Fix.** Show loop start / end flags at all times (dimmed when LOOP is off), draggable.
**Evidence.** `neuro/osc-sample-sfz`.

### S15-5. Medium-low: the wavetable oscillator has no way to see the other oscillators' tables at once
**Wrong.** Serum's main window shows OSC A, B, C panels together (`serum2-main-osc-page.png`); here PLAY shows 2D pictures of each, but OSC page shows one at a time and the 3D view is only in OSC 1's slot. Accepted by design (PLAY is the owner's layout); mentioning because it is where Serum earns its "I see everything" feel.
**Fix.** None required; consider a 3D toggle on the PLAY strip's picture.
**Evidence.** `neuro/01-MAIN`, refshot Serum main.

### S15-6. Medium-low: physics LFOs still speak their own language
**Wrong.** `neuro/lfo-sim-lorenz`: TRIGGER, OUT 1 AXIS, FIRE, SIGMA, RHO, BETA, SEED; no DRIVES list in the card (the ordinary LFO has one, `neuro/06-ENV-LFO`).
**Benchmark.** Serum 2's Lorenz LFO (`serum2-s2_lorenz.png`) is one display with RATE, DELAY, SMOOTH, PHASE: the same row as every other LFO.
**Fix.** Give the physics card the same bottom DRIVES list; keep SIGMA / RHO / BETA as a second knob row labelled "LORENZ"; rename OUT 1 AXIS to AXIS 1.
**Evidence.** `neuro/lfo-sim-lorenz`.

### S15-7. Low: LFO and ENV pool cards are two types of thumbnail
**Wrong.** In `neuro/06-ENV-LFO` the LFO tile shows a coloured label with a route chip ("FILTER 1 > Cutoff +2 more") and a thumbnail; the ENV tile shows a label with a target badge ("AMP", "FILTER 1") and a thumbnail. The chips differ in style (outlined pill versus filled), meaning "what it drives" is shown two ways.
**Fix.** Same pill for both.
**Evidence.** `neuro/06-ENV-LFO`.

### S15-8. Low: ENV tile names are "AMP ENV", "FILT ENV", "FILT 2 ENV"; Serum names envelopes ENV 1 to 4
**Wrong.** Not a bug; the names say what they drive, which beginners like. But the matrix source list, chip bar and pools use all the same names, and when a user routes ENV 2 elsewhere the name lies *(inferred)*. Accepted in review 6.
**Fix.** None.

### S15-9. Low: SAVE AS has a TAGS text field and a chip grid for the same thing
**Wrong.** `extras/extra-save-as`: "Gritty, Evolving, FM, Bright, Sustained, Wide" typed in a field, and the same tags as lit chips under it.
**Benchmark.** Serum's save dialog is name, author, description and tag pills.
**Fix.** Chips only, with a "+ NEW TAG" chip that opens a one-word field.
**Evidence.** `extras/extra-save-as`.

### S15-10. Low: the preset browser's three bottom buttons and the top-right FLOAT / x are different heights
**Wrong.** `extras/extra-browser-docked`: RANDOM PRESET, SAVE AS, DELETE, FOLDER, IMPORT .SYX are 40 px tall and equal; FLOAT and x at the top right are 28 px. The SAVE AS button is orange-filled but DELETE is dimmed with no explanation of why *(inferred: no preset selected)*.
**Fix.** Same height for both rows; DELETE tooltip "Only user presets can be deleted".
**Evidence.** `extras/extra-browser-docked`.

### S15-11. Low: the tag chip row "+37" and the DX7 270 chip are two kinds of filter
**Wrong.** `extras/extra-browser-docked`: "DX7 270" (a bank filter) sits first in the row, then ten tag chips, then "+37". The first is not a tag.
**Fix.** DX7 moves to the left rail's list (with a count), as a bank.
**Evidence.** `extras/extra-browser-docked`.

---

# Part 3: Integration (I15-n)

### I15-1. Medium: DX7 modulators say DEPTH, the rule says OUTPUT
**Wrong.** UI-CONVENTIONS ("A DX7 operator shows one level, OUTPUT"). `dx7/01-MAIN` shows OUTPUT on carriers (OSC 1, 3, 5) and DEPTH on modulators (OSC 2, 4, 6) (`OscPage.h:111-119`); the FM page card says OUTPUT for every operator and the matrix header says "depth = the modulating operator's OUTPUT". So one knob has two names depending on the page and the role.
**Benchmark.** Dexed uses OUTPUT LEVEL everywhere.
**Fix.** Either the convention is amended ("DEPTH on a modulator's PLAY strip and OSC card only") and the FM card follows, or every page says OUTPUT. I prefer OUTPUT everywhere with the unit (dB) carrying the meaning, as the FM matrix header already says.
**Evidence.** `dx7/01-MAIN`, `dx7/03-OSC`, `dx7/08-FM`; `OscPage.h:111`.

### I15-2. Medium: the DX7 FILTER page does not say the filter is off
**Wrong.** PLAY says "FILTER OFF · a DX7 voice has none" (`dx7/01-MAIN`); the FILTER page for the same patch (`dx7/05-FILTER`) shows a flat 890 x 470 px response graph, CUTOFF 20.00 kHz bright and F2 dimmed. The same patch is described two ways.
**Fix.** The same banner across the response graph; flow nodes F1 / F2 dimmed.
**Evidence.** `dx7/01-MAIN`, `dx7/05-FILTER`.

### I15-3. Medium: physics LFOs and the matrix both list routes, only one is a list
**Wrong.** Same finding as S15-6 from the integration side: a source with its own widgets ("OUT 1 / OUT 2 AXIS") that the matrix, chip bar and DRIVES list don't reflect (the chip says "OUT 2" in two places).
**Fix.** See S15-6.
**Evidence.** `neuro/lfo-sim-lorenz`, `neuro/01-MAIN` (chip "OUT 2").

### I15-4. Medium: a physical oscillator's controls live in two places
**Wrong.** The OSC page (`keys/03-OSC`) shows EXCITE, DECAY, DAMP; PHYSICAL (`keys/04-PHYSICAL`, tool-artefact state shows the full set) shows SUSTAIN, STIFF, REGISTER, DAMPER, COUPLING, EXCITE POS, BOW PRESS... A user editing DECAY on one page cannot find REGISTER without a page change. Wavetable, sample and operator oscillators have all their controls on the OSC page.
**Benchmark.** Serum: one panel per oscillator type; everything on it.
**Fix.** Put the full STRING and EXCITER rows on the OSC page for a physical oscillator (there is a 200 px band free, V15-11) and make PHYSICAL the animation-and-body page. Then "physical" matches every other engine.
**Evidence.** `keys/03-OSC`, `neuro/physical-page`.

### I15-5. Medium-low: SAMPLE's and PHYSICAL's empty states are dimmed ghosts; FM's and ARP's are different
**Wrong.** Sample: ghost wave with LOAD; PHYSICAL: dimmed controls with SWITCH TO PHYSICAL; VECTOR: pictures faded; FM with one oscillator: a hint line and two dashed circles; ARP: a button on three lanes; FX: a library grid; matrix: an illustration. Five "nothing here yet" styles.
**Fix.** One rule: the page draws the thing at 40 % opacity and puts a single action button on it. FM: a dimmed 2-operator stack with ADD OSC 2; ARP: dimmed bars; FX: dimmed three-card skeleton.
**Evidence.** `init/08-FM`, `neuro/09-ARP-SEQ`, `init/10-FX`, `neuro/osc-sample-empty`.

### I15-6. Medium-low: three kinds of on / off for a module on one page
**Wrong.** `neuro/05-FILTER`: FILTER 2 has a switch in its header (right), WEST and BODY have switches at the right of a collapsed row, FILTER 1 has none (always on). The convention is respected, but WEST and BODY switch on a row that looks like a card header without a body; the on / off is the same switch as a setting. OK by convention; the visual weight (a 60 px strip) differs from F2's 190 px card, which jumps (V15-8).
**Evidence.** `neuro/05-FILTER`.

### I15-7. Low: Airwindows cards have a BUILT-IN | AIRWINDOWS pill; built-in cards show it too
**Wrong.** `neuro/10-FX`: DRIVE has "BUILT-IN | AIRWINDOWS" in the title; VOWEL and OTT do not (they have no Airwindows twin). A user may wonder where it went. Reasonable; mention only that the pill's position (after the name) moves the cards' title by 190 px.
**Fix.** Always show the pill at the same x for all cards that have it; a missing twin is fine.
**Evidence.** `neuro/10-FX`.

### I15-8. Low: "SOUNDBOARD" and "STRINGS" in the OSC drawer are tabs; BODY is a card elsewhere
**Wrong.** `neuro/03-OSC`: VOICE / SUB + NOISE / STRINGS / SOUNDBOARD in one drawer; BODY lives on FILTER and PHYSICAL, SOUNDBOARD is also on PHYSICAL. The same module has three homes (OSC drawer tab, PHYSICAL box, and the signal flow's STRINGS / SOUNDBOARD node).
**Fix.** Keep, but name one editor: the drawer tab and the PHYSICAL box should be the same widget (they appear to be, `keys/04-PHYSICAL`).
**Evidence.** `neuro/03-OSC`, `keys/04-PHYSICAL`.

### I15-9. Low: the macro strip's "· 1 OFF" has no way to reach the off route
**Wrong.** `keys/04-PHYSICAL`: "HAMMER → 2 · 2 OFF" is orange text, not a button; a click goes to the matrix *(inferred)*. A discoverability gap, small.
**Fix.** Tooltip "2 of 4 routes are switched off in the matrix; click to see them".
**Evidence.** `keys/04-PHYSICAL`; `MacroStrip.h:106`.

### I15-10. Low: "NOT ADDED" and "OSC 4: none" are two words for the same state
**Wrong.** `neuro/02-VECTOR`: the bottom-right corner picture says "NOT ADDED"; its menu says "OSC 4: none"; the pad says "OSC 4: none". The conventions say "OSC 4: none". The picture word is the odd one.
**Fix.** "NONE" in the picture.
**Evidence.** `neuro/02-VECTOR`; `VectorPad.h:81`.

### I15-11. Low: the FM page's ENVELOPE menu says AMP ENV on a Semitones oscillator and the hint says "pick OP ENV for a DX7 envelope"
**Wrong.** `neuro/08-FM`: the footer "Edit AMP ENV on MOD or PLAY, or pick OP ENV for a DX7 envelope here." is a full sentence in a 30 px strip on every page of the card.
**Fix.** Make it a tooltip on ENVELOPE.
**Evidence.** `neuro/08-FM`, `init/08-FM`.

### I15-12. Low: the OSC page's right-hand "FM FROM OSC 2 30 % >" pill is the only place the wavetable page mentions FM
**Wrong.** `neuro/03-OSC`: a pill in the card header. It is a good link. But PLAY says "TO OUTPUT, MODULATES 1" on OSC 2 and nothing on OSC 1. Two phrasings for one route ("MODULATES 1", "FM FROM OSC 2 30 %"), and the PLAY one has no amount.
**Fix.** PLAY: "FM from OSC 2". Matches.
**Evidence.** `neuro/03-OSC`, `neuro/01-MAIN`.

### I15-13. Low: help text appears as a card caption, as a note, as a "WHAT IT DOES" well and as a footer
**Wrong.** Hints are sentences by convention; seen in four positions: tab band (`neuro/03-OSC`), under a graph (`neuro/08-FM` footer), in a well (`neuro/fx-08`), in a header caption (`neuro/06-ENV-LFO` "drag the graph or the knobs"). The caption rule (lower-case fragments) says header captions are fragments, which "drag the graph or the knobs" is; the others are sentences. All follow the rules; the positions differ.
**Fix.** One position: a line under the card's graph, 12 px, text3.

### I15-14. Low: the UI test is good; add the checks that would have caught this round's findings
**Wrong.** The test (`ilanaSnapshot --uitest`, PASSED) checks text. It does not check geometry. V15-3 (cell aspect), V15-4 (spare height under OUTPUT), V15-8 (page re-flow), V15-5 (a well with one sentence) are all checkable.
**Fix.** Tests: no FM cell taller than 1.4 x its width; spare height under the last card of the FX rack under 80 px on 1, 2, 3 cards; FILTER's graph bounds identical with WEST on and off; no `paintWell` whose only child is text.
**Evidence.** `--uitest` output.

---

## Workflow speed (the same five jobs)

| Job | Clicks / gestures | Verdict |
|---|---|---|
| Make a wobble bass | Preset, drag a macro: 2 | as Vital |
| Add an LFO to cutoff | MOD, drag the chip onto the knob (or "+" in matrix): 1 drag | as Vital |
| Add a reverb | FX, ADD EFFECT, pick: 3 | one more than Vital (always visible there) |
| Audition 10 presets | arrow keys, load each: no audition | slower than Serum |
| Edit a DX7 operator envelope | OSC tab, pick osc, drag the graph: 3 | at benchmark |
| Draw a remap curve | MATRIX, click CURVE, drag: 2 | better than Vital's (always shown) |

## Settled decisions and conventions check

- Settled decisions (docs/REVIEW-PLAN.md) respected; I did not argue against the PLAY strip layout.
- UI-CONVENTIONS: on / off switch positions: respected (FILTER 2, WEST, BODY). The on dot: respected. Casing: "Values keep their own case": respected. Separators: " · " used in subtitles. Vocabulary: **one breach** (I15-1 DEPTH against OUTPUT). Text that may not fit: UI test passes; no cut text on 100 % or 75 % sets. Page switches: respected.

## Top ten, one line each

1. V15-1: FILTER, MOD ENV/LFO and SEQ ARP did not move (31, 30, 41 % bare); three of seven tabs are still half dark.
2. V15-3: the FM page on one or two oscillators has 170 x 240 px cells with a dot in each (46 % bare, row height at `FmInputPages.h:707`).
3. V15-4: a lone FX card leaves a 270 px blank floor under OUTPUT (52 % bare).
4. V15-5: HAAS, TILT and other no-display FX cards are black boxes with a sentence (the review 14 anti-pattern).
5. V15-8: WEST on re-flows the whole FILTER page by 220 px; the graphs shrink and jump.
6. S15-1: still no audition in the 641-preset browser; the detail panel is 60 % floor.
7. V15-2: FX graphs are 1000+ px wide for one straight line (OTT, LIMITER, REVERB).
8. I15-4: a physical oscillator's controls live on two pages, unlike every other engine.
9. V15-17: macro tiles change text and shape with state ("+ ASSIGN" appears on whichever macros are idle).
10. I15-1 / I15-2: DX7 modulators say DEPTH against the written rule OUTPUT, and the DX7 FILTER page shows a normal graph for a filter that is off.

## Score

**Against Vital: 9.1 / 10. Against Serum 2: 8.8 / 10. Integration: 9.3 / 10.**

PLAY, the 6-operator FM page, the matrix, the sample page and PHYSICAL's new preview are at the benchmark's density; the bare-region mean fell from 37.8 % to 29.9 % (Vital 18.5 %). To reach 9.5 the build needs the three pages that did not move (FILTER, ARP, MOD ENV/LFO) to be sized by their content, the three odd pages (FM small, FX lone card, no-display FX cards) to stop being slabs, and an audition in the browser.
