# UI review 8: ilanaSynth v1.3 (after the review 7 fixes) against Serum 2

Reviewer: a fresh, adversarial pass on branch `claude/project-thread-smvgfk` (HEAD `096ff08`). The benchmark is Serum 2's UI as I
know it (I did not run Serum 2). Workflow speed gets extra weight.

Inputs: the shots in `/home/user/shots8/` (1060x720 logical at 1.5x; `small/` and `dx7-small/` are at 75 %), plus my renders in
`/home/user/shots8/serum-extra/`:
- `p1/` is the P1 modulation set on Neuro Wobble.
- `fm-init/` and `fm-brass/` are the FM set. "Brass 1" did not resolve, so both are Init.
- `fx-contact*.png` is a contact sheet of all 41 FX shots.
- `crop-*.png` are zoomed crops.

I also read the source in `src/gui/` and `src/PluginEditor.cpp`. Shot paths below are relative to `shots8/`. **(inferred)** marks a
claim I could not confirm in a shot or in the code.

Artefacts I did not count:
- EDITED after 03-OSC.
- "ENV +12/+14" and the revealed ENV 5-16.
- STRINGS in SIGNAL FLOW on Neuro (the tool turns on `sym_on`).
- The "⚠" on TALK, ROOM, HAMMER and DECAY in the later shots. These macros point at effects or modules that the snapshot tool
  changes or swaps out. They do not show on the first PLAY shot.

## Verdict

Review 7's cycle fixed most of what it set out to fix, and it fixed it well:
- SIGNAL FLOW keeps F1/F2 (`keys/05-FILTER.png` shows "STR+BRD").
- PLAY's oscillator column has no empty slots, and the knob columns are fixed.
- A DX7 operator reads RATIO / LEVEL dB / TRIM % / FINE everywhere.
- The 6-operator stack is spaced out.
- DX7 voices get macros by sound (BARK / DARKEN / WOBBLE / ROOM).
- The piano roll has a DRAW mode and "▲1" off-range markers.
- WEST and BODY dim properly when off.
- OP ENV / OP PITCH / OP LFO are real pool cards on MOD.
- The envelope pool ends in "11 MORE" instead of scrolling sideways.

The build now looks like one product on most pages.

What still separates it from Serum 2 is mostly **DX7 machinery that leaks into every patch, and a handful of half-finished
merges**:
- Every patch, even a plain wavetable bass, shows three dead DX7 cards on MOD: OP LFO, OP ENV and OP PITCH, all marked "unused".
- On the DX7 voices themselves, the chip bar folds away OP ENV, the one envelope that sounds, and keeps three unused envelopes
  visible.
- The DX7 pitch EG and LFO are called "OP PITCH / OP LFO" on MOD but "PITCH & LFO" and "VOICE PITCH & LFO" on FM. The review 7
  work plan banned exactly that.
- The MSEG is still both a pool module and an LFO shape.
- Dry/wet is still drawn two ways on FX cards (a MIX knob, or a BLEND header slider).

There are also small layout faults: a truncated "6 opera…" pool card, mod-ring dots over the FRAME value, a squeezed EXCITE combo,
and the hover line covering card bottoms.

**Score: 7.8 / 10 against Serum 2** (review 7: 7.0).

---

## Workflow speed: the four common jobs

Clicks are counted from a freshly opened instance on PLAY.

| Job | ilanaSynth now | Serum 2 | Verdict |
|---|---|---|---|
| Route an LFO to cutoff and set the depth | Drag the LFO 1 chip onto CUTOFF (1 drag), drag the ring (1 drag). **2 gestures**. On DX7 voices LFO 1-3 stay visible (good), but OP ENV folds away (S8-2) | Drag the LFO's handle onto the knob, drag for depth: 2 gestures | **Par** |
| Browse and audition 5 presets | Click the name (1), Down x5 (each one auditions), Enter. **1 click + keys**. Or use the < > arrows: 1 click per preset | Arrows or the browser with filters: about the same | **Par**. The DX7 chip's count changes with the bank filter (S8-27) |
| Add an effect and move it first | + ADD EFFECT (1), pick the type (1), drag its header to slot 1 (1 drag). **2 clicks + 1 drag** | Click an empty slot, pick, drag: the same | **Par**. A second instance of a type is still refused (settled) |
| Draw an 8-note clip | SEQ (1), CLIP tab (1), its switch (1), DRAW (1, or the D key), then one drag paints a run of notes. **4 clicks + 1 drag**. Without DRAW: a double-click per note | Serum 2's clip editor draws on a plain click **(inferred)**: about 2 clicks + drags | **Slightly slower**: DRAW is off by default (`ClipEditor.h:1608`, `drawMode = false`) and its tab needs its own switch |

Overall the common jobs are at Serum 2's speed. The one real gap is the clip editor starting in select mode.

---

## Fixed since review 7 / still open (my area: S7-1 … S7-42)

| S7 | Finding | Now |
|---|---|---|
| 1 | SIGNAL FLOW loses F1/F2 with strings or a soundboard | **Fixed** (`keys/05-FILTER.png`: F1 → F2 → STR+BRD → OUT) |
| 2 | PLAY's empty fixed slots | **Fixed** ("+ ADD OSC 4" dashed row). New: rows shrink as oscillators are added (S8-24) |
| 3 | PLAY knob order changes with mode / FM role | **Fixed** (SEMI/LEVEL/FRAME/UNISON; operators RATIO/LEVEL/TRIM/FINE) |
| 4 | Two LEVELs on a DX7 operator | **Fixed** (LEVEL dB + TRIM % on PLAY, OSC and FM) |
| 5 | Five names for the operator envelope | **Mostly fixed** (OP ENV in labels, "Operator Env" in prose). PLAY and MOD word "unused" differently (S8-17) |
| 6 | DX7 pitch EG / LFO a parallel system | **Partly fixed.** They are pool cards on MOD now, but FM still names them "PITCH & LFO" / "VOICE PITCH & LFO" (S8-3), and they sit on every patch (S8-1) |
| 7 | Three drawn-shape systems | **Partly fixed.** MSEG is now an LFO SHAPE, but the old MSEG module remains as a pool card and chip (S8-5) |
| 8 | Chip bar hides core LFOs first | **Fixed for LFOs**. New: it folds OP ENV first on DX7 voices (S8-2) |
| 9 | Same four macros on every DX7 voice | **Fixed** (BARK / DARKEN / WOBBLE / ROOM on E.Piano 1; no "!" on the first shot) |
| 10 | DX7 duplicates in the browser | **Mostly fixed** (repeats hidden, Title Case). The counts disagree (S8-27) |
| 11 | MOD label on the filter curve | **Fixed** (open ring marker, no label: `neuro/01-MAIN.png`) |
| 12 | DX7 operators wear wavetable controls | **Partly fixed.** The OSC page is clean (OP ENV graph, no WARP/SPECTRAL). PLAY still shows "Wavetable / Sine" combos per operator, and OSC keeps RESAMPLE / EDIT / LOAD .WAV and UNISON (S8-9) |
| 13 | Cramped 6-op stack | **Fixed** (`serum-extra/fm-init/fm-dx7-algorithm-1.png`) |
| 14 | FM diagram shows off oscillators as live | **Fixed** (dashed "OSC 3 OFF") |
| 15 | Two wet controls per FX card | **Partly fixed.** One per card, but two different widgets across cards (S8-6) |
| 16 | FX leftovers: two + ADD EFFECT, card widths, RACK A/B | **Partly fixed.** One + ADD EFFECT. Widths still mix full and half (S8-14) |
| 17 | Notes outside the roll hidden | **Fixed** ("▲1" marker) |
| 18 | No paint mode | **Fixed** (DRAW, D key), but off by default (speed table) |
| 19 | ARP lanes without readouts | **Partly fixed.** PITCH has numbers, VELOCITY and GATE don't, and the pitch numbers sit on the zero line across bar edges (S8-15) |
| 20 | Hover help covers the chip bar; third naming scheme | **Partly fixed.** It sits above the bar now but covers the card bottoms, and says "OSC 1 › Pitch" over a knob labelled SEMI (S8-11) |
| 21 | VEC X/Y chips in the card, not the bar | **Partly fixed.** They are in the bar but folded under MORE, and repeated in the card as VECTOR X / VECTOR Y (S8-20) |
| 22 | Body in three places | **Mostly fixed** (one BODY card on FILTER, a summary row on PHYSICAL). The summary breaks the switch rule (S8-12) |
| 23 | Off cards look live | **Fixed** |
| 24 | BODY controls 60 px lower than WEST | **Fixed** (aligned rows) |
| 25 | OSC rows mostly air, floating labels | **Mostly fixed**. The bottom SUB + NOISE panel is still half empty (S8-29) |
| 26 | LFO and MSEG panels mostly empty | **Open** (S8-7) |
| 27 | Op cards as pills, not pool cards | **Fixed** (but see S8-1) |
| 28 | Envelope time axis mixes scales | **Partly fixed**. The log axis and "KEY UP +100 ms" are still two scales in one plot (S8-31) |
| 29 | "Off" as a value for levels | **Fixed** (−inf dB, 0 ms) |
| 30 | Tutorial points at the wrong button | **Fixed** |
| 31 | Docked and floating browsers behave differently | **Fixed** (same keys, same hint) |
| 32 | Confirm dialog interrupts auditioning | **Fixed** ("Don't ask again") |
| 33 | Thin depth rings | **Not verified** (no drag shot). The rings now crowd value text in compact rows (S8-8) |
| 34 | FM matrix footer hides a limit | **Fixed** ("OSC 1 × OSC 2 only"), but it shows on DX7 voices where it means nothing (S8-21) |
| 35 | Matrix noise | **Partly fixed**. Rows are still in creation order (S8-25) |
| 36 | FX library Airwindows layout | **Not verified** (no shot) |
| 37 | SEQ header says one thing while two engines run | **Fixed** (the "KEYS › STRUM › EUCLID › CLIP › VOICES" chain) |
| 38 | Clip corner label repeats GRID | **Fixed** |
| 39 | PHYSICAL soundboard box too big | **Fixed** |
| 40 | Envelope pool scrolls sideways | **Fixed** ("11 MORE ▾") |
| 41 | FM matrix hint jargon | **Fixed** ("Depth = the modulating operator's LEVEL (on its card) × this cell.") |
| 42 | Header spacing | **Fixed** ("VOICES 1/32") |

Tally: 25 fixed, 6 mostly fixed, 9 partly fixed, 1 open (S7-26), 2 not verified (S7-33, S7-36).

---

## Findings, worst first

### S8-1. High (bolted on): three dead DX7 cards on MOD on every patch
- **What's wrong:** Neuro Wobble is a wavetable bass, but its MOD page shows OP LFO "unused" as the 4th LFO card and OP ENV and OP
  PITCH "unused" as two of the five envelope cards. That is 3 of 9 pool slots for a module the patch does not use. The full-width
  OP ENV editor (ten knobs) is editable while it does nothing (`neuro/06-ENV-LFO.png`, `neuro/lfo-mseg.png`). The code makes this
  a rule: "They are always in the pool; while no oscillator plays the Operator Env they are greyed" (`src/gui/OperatorPoolCards.h:18-24`).
  This is the clearest "own corner" left: a DX7 subsystem that every user sees on every patch.
- **Serum 2's way:** modules you have not added do not show. You add an LFO or envelope with "+".
- **Fix:** show the OP cards only when an oscillator's ENVELOPE is OP ENV (the same test `FmOperatorInfo::anyOperatorEnv` the chip
  bar already uses). Otherwise offer "OP ENV (DX7)" in the pool's "+" menu, where picking it sets OSC 1's ENVELOPE.
- **Evidence:** `neuro/06-ENV-LFO.png`, `neuro/lfo-mseg.png`, `init/06-ENV-LFO.png`.

### S8-2. High (workflow): on DX7 voices the chip bar folds OP ENV and keeps three unused envelopes
- **What's wrong:** On E.Piano 1, AMP ENV is "unused", and FILT ENV and FILT 2 ENV do nothing. Yet the bar shows AMP ENV,
  FILT ENV and FILT 2 ENV as chips, and OP ENV, the envelope every operator plays, is hidden under "ENV +1 ▾". The cause is in
  `foldRank` (`src/PluginEditor.cpp:919-935`): only matrix routing counts as "routed". OP ENV (chip kind −2) is not a pool chip,
  so it ranks with the plain unrouted chips, and being later in the group it folds first.
- **Serum 2's way:** ENV 1 (the amp envelope) is always first and visible.
- **Fix:** treat "played by an oscillator" as routed in `foldRank`, or rank OP ENV 0 when `anyOperatorEnv`. Fold an envelope
  marked "unused" first (AMP ENV on an all-OP-ENV patch).
- **Evidence:** `dx7/01-MAIN.png`, `dx7/03-OSC.png`, `dx7/06-ENV-LFO.png` (bar: AMP ENV, FILT ENV, FILT 2 ENV, "ENV +1").

### S8-3. High (inconsistency, work plan not followed): the DX7 pitch EG and LFO have three names
- **What's wrong:** MOD's pool cards and the chips say **OP PITCH** and **OP LFO**. The FM page's card header says **"PITCH & LFO"**
  (`src/gui/pages/FmInputPages.h:471`) and its button says **"VOICE PITCH & LFO"** (`FmInputPages.h:1343`). The review 7 work plan
  set "The DX7 pitch envelope is OP PITCH, the DX7 LFO is OP LFO" as the shared vocabulary. Q1 did not apply it to its own page.
  "VOICE" is also misleading: every LFO in the synth is "voice" something.
- **Serum 2's way:** one name per module, everywhere.
- **Fix:** button "OP PITCH · OP LFO", card header "OP PITCH · OP LFO". Better still, drop the FM-page copy and link to the MOD
  cards with "MOD ›", as MOD's OP ENV links back with "FM ›". The same editor in two places is a parallel system in itself.
- **Evidence:** `dx7/08-FM-pitch-lfo.png` against `dx7/06-ENV-LFO.png`.

### S8-4. High (duplicate editors): OP ENV, OP PITCH and OP LFO are fully editable on two pages
- **What's wrong:** FM's operator card has the OP ENV graph and stage knobs, the OSC page has another OP ENV graph, and MOD has the
  third copy with OSC 1..6 pills. The pitch EG and LFO are likewise on both FM and MOD. Three editors for one envelope means three
  layouts to learn: FM shows STAGES / KEYS & VELOCITY tabs, MOD shows KEY RATE and LEVEL in one row, and OSC shows LEVEL plus a
  graph.
- **Serum 2's way:** an envelope is edited in its one place, and other pages show a mini view that links there.
- **Fix:** keep the full editor on MOD (with the other envelopes). FM and OSC show a small read-only thumbnail plus "EDIT OP ENV ›"
  (PLAY's ENVELOPE card already does this).
- **Evidence:** `dx7/08-FM.png`, `dx7/03-OSC.png`, `dx7/06-ENV-LFO.png`.

### S8-5. Medium-High: the MSEG is still two systems
- **What's wrong:** There is an MSEG pool card ("the patch's four-point MSEG; drag its card onto a knob", with LOOP and RATE) and an
  MSEG chip. Its own hint then says "Any LFO can draw its own: SHAPE › MSEG." So the user meets two MSEGs. The caption says
  "four-point", but the graph shows five points (`neuro/lfo-mseg.png`).
- **Serum 2's way:** an LFO is the MSEG. There is no separate module.
- **Fix:** hide the legacy MSEG card and chip unless a patch routes the MSEG source (as OP cards should, S8-1). New patches use
  LFO SHAPE › MSEG. Fix the "four-point" caption.
- **Evidence:** `neuro/lfo-mseg.png`, `dx7/lfo-pool-main.png`.

### S8-6. Medium: dry/wet is drawn two ways on FX cards
- **What's wrong:** Effects with their own MIX get a knob. Effects without one (AMP, TILT, UTILITY, LIMITER, TREMOLO) and
  duplicate cards get a thin "BLEND 100%" slider in the card header (`FxPage.h:984-992`, `FxLibrary.h:551-587`). It is the same
  control in a different widget and position, with a different name. The duplicate card, whose body says it does nothing of its
  own, still shows BLEND (`neuro/fx-duplicate.png`).
- **Serum 2's way:** every effect has a MIX knob in the same place.
- **Fix:** render the slot blend as a MIX knob in the body where the effect has no MIX, and hide it on duplicate cards.
- **Evidence:** `serum-extra/fx-contact-a.png` (AMP, TILT, UTILITY, LIMITER, TREMOLO), `neuro/fx-duplicate.png`.

### S8-7. Medium (dead space): the LFO and MSEG panels are mostly empty
- **What's wrong:** LFO 1's panel is 790x310 px holding SHAPE, three switches and three knobs. About 60 % is air. The MSEG panel holds
  one switch and one knob in the same space (`neuro/lfo-mseg.png`). This is unchanged since review 7 (S7-26).
- **Serum 2's way:** the LFO editor is mostly graph; the few controls sit in one strip under it.
- **Fix:** widen the graph to ~65 % and stack the controls in one column, or put RATE/START/SMOOTH in the SHAPE row.
- **Evidence:** `neuro/06-ENV-LFO.png`, `dx7/06-ENV-LFO.png`, `neuro/lfo-mseg.png`.

### S8-8. Medium (layout): mod-ring dots sit on the value text in compact PLAY rows
- **What's wrong:** With 5 oscillators the rows shrink to ~120 px. FRAME's three rings and their end dots then reach the "50%"
  value, the cyan dot overlaps the "%", and the label touches the ring (`serum-extra/crop-frame-ring.png`).
- **Fix:** in compact rows, drop the value 3-4 px or draw fewer rings (the outer one plus a "+2" tick).
- **Evidence:** `neuro/added-osc-MAIN.png`.

### S8-9. Medium: DX7 operators still show wavetable furniture
- **What's wrong:** On PLAY each operator shows a "Wavetable" mode combo and a "Sine" table combo. The OSC page keeps
  RESAMPLE / EDIT / LOAD .WAV, the WAVE/3D/SPEC tabs and UNISON on an FM operator.
- **Serum 2's way:** an FM source shows only what it uses.
- **Fix:** on PLAY show "Operator" in the mode combo's place and the table only when it is not Sine. On OSC, hide RESAMPLE/EDIT/LOAD
  for operators, or move them under the "…" menu.
- **Evidence:** `dx7/01-MAIN.png`, `dx7/03-OSC.png`.

### S8-10. Medium (squeezed text): "6 opera…" on the OP ENV pool card
- **What's wrong:** With 8 envelope cards the OP ENV card truncates its badge to "6 opera…". That is the one thing the card tells you.
- **Fix:** use "6 OPS" or "×6" when it is narrow.
- **Evidence:** `dx7/env-pool-revealed.png`, `serum-extra/crop-op-env-card.png`.

### S8-11. Medium: the hover line covers card bottoms and uses a third name
- **What's wrong:** Hovering SEMI shows "OSC 1 › Pitch" (from `ModNames.h:294`) in a bar that overlays the bottom 20 px of the page.
  It cuts off SUB + NOISE's and LFO's lower borders (`neuro/hover-line.png`). The knob says SEMI, the matrix says "OSC 1 › Pitch",
  and the help says "Pitch offset in semitones".
- **Fix:** reserve the line's height (or float it over the macro row's labels). Name it from the knob's label: "OSC 1 › SEMI (pitch)".
- **Evidence:** `neuro/hover-line.png`.

### S8-12. Medium (breaks UI-CONVENTIONS): state shown as text with no switch
- **What's wrong:** PHYSICAL's footer reads "BODY off" and "SOUNDBOARD on, Dense", with link buttons but no switch
  (`FilterVectorPhysicalPages.h:699-702`). SIGNAL FLOW shows "OFF WEST BODY". PLAY's off oscillator says "OFF" under its name,
  700 px from its switch. The convention reads "Never state 'on' or 'off' in text without a switch beside it."
- **Fix:** put a small switch before BODY and SOUNDBOARD on PHYSICAL. Drop the word "OFF" on PLAY (dimming already says it).
- **Evidence:** `keys/physical-page.png`, `neuro/05-FILTER.png`, `neuro/01-MAIN.png`.

### S8-13. Medium: the FM page on a non-FM patch is a big empty diagram
- **What's wrong:** On Neuro Wobble (two oscillators, one FM route) the diagram is 830x400 px with two nodes and a grey dashed one.
  The ALGORITHMS row offers nine BASIC tiles and the "CUSTOM: NEAR B1 2-OP STACK" label.
- **Serum 2's way:** FM is an oscillator warp mode in the oscillator, with no page to visit.
- **Fix:** when no oscillator is an operator and the matrix has ≤ 2 routes, shrink the diagram and lead with the matrix.
- **Evidence:** `neuro/08-FM.png`.

### S8-14. Medium (layout): FX card widths mix full and half; half the page is empty
- **What's wrong:** AMP, PHASER, DELAY, REVERB and TRANCE GATE take the full width, while the rest are half width. On a 3-effect
  chain the lower 40 % of the page is empty (`neuro/10-FX.png`). In a split group, DRIVE takes half the width and leaves the other
  half empty, then REVERB takes the full width (`neuro/fx-split.png`). The split group's last card is cut by the OUTPUT bar with no
  fade.
- **Serum 2's way:** one column of equal-width racks.
- **Fix:** a single rule. Either everything is full width with a height that fits its controls, or two columns that pack.
  Add a fade or shadow at the scroll edge.
- **Evidence:** `serum-extra/fx-contact-a.png`, `neuro/fx-split.png`.

### S8-15. Medium-Low: ARP lane readouts only on PITCH, drawn across bar edges
- **What's wrong:** PITCH shows "+7", "+12" and "-5" on the zero line, half over the bars. VELOCITY and GATE have no numbers at all.
  "PITCH st" mixes case with VELOCITY and GATE.
- **Fix:** put a value under each step for all three lanes, in one style (or show it on hover). Name the lane "PITCH (st)".
- **Evidence:** `init/gen-arp-lanes.png` (in `neuro/` too).

### S8-16. Medium-Low: "B" on LFO chips and tiles is unexplained
- **What's wrong:** "LFO 2 [B]" means "this simulated shape has a second output you can drag". Nothing on screen says so.
  It looks like an A/B state.
- **Fix:** label it "B out" in the tooltip and draw it as a second small chip "LFO 2·B" that drags.
- **Evidence:** `neuro/01-MAIN.png`, `neuro/06-ENV-LFO.png`.

### S8-17. Low-Medium: "unused" said two ways
- **What's wrong:** PLAY says "Not used: the OSCs play their OP ENV" (`MainPage.h:849`). MOD says "unused: no oscillator plays the
  Operator Env (an oscillator's ENVELOPE picks it)" (`OperatorPoolCards.h:502`) and has an "unused" chip on cards. PLAY uses
  "the OSCs", MOD uses "oscillator".
- **Fix:** one string: "unused: the oscillators play OP ENV" / "unused: no oscillator plays OP ENV".

### S8-18. Low-Medium: the LFO tile's "+2" reads as a depth
- **What's wrong:** "Filter 1 › Cutoff +2" on LFO 1's tile means "and 2 more targets". The matrix next door writes depths as
  "+70%", so "+2" reads as a value.
- **Fix:** "Filter 1 › Cutoff · 3 targets" or "+2 more".
- **Evidence:** `neuro/01-MAIN.png`.

### S8-19. Low-Medium: the TRANSPOSE stepper is a one-off widget
- **What's wrong:** In the FM PITCH & LFO card, TRANSPOSE is a "− +" pair with an "all SEMI" caption. It is the only stepper in the
  synth; everything else is a knob or a combo.
- **Fix:** a knob in semitones, like SCALE SHIFT next to it.
- **Evidence:** `dx7/08-FM-pitch-lfo.png`.

### S8-20. Low-Medium: VECTOR X/Y in two places with two names
- **What's wrong:** The vector card's header has "VECTOR X" and "VECTOR Y" chips. The chip bar has them only inside "MORE +2". The
  work plan called them VEC X / VEC Y.
- **Fix:** show them in the bar unfolded while VECTOR is on (they are the module's whole point), drop the card copies, and use one
  name.
- **Evidence:** `neuro/vector-page.png`.

### S8-21. Low: the FM matrix footer and NOISE FM show on DX7 voices
- **What's wrong:** RING MOD, SYNC 2 TO 1 ("OSC 1 × OSC 2 only") and NOISE FM COLOUR take the matrix's bottom quarter on E.Piano 1,
  where they mean nothing.
- **Fix:** fold them into a "MORE" row while a DX7 algorithm is chosen.
- **Evidence:** `dx7/08-FM.png`.

### S8-22. Low: the EXCITE combo uses a smaller font
- **What's wrong:** "Piano Hammer" in PHYSICAL's EXCITE combo is visibly smaller than every other combo's text.
- **Evidence:** `serum-extra/crop-excite-combo.png`.

### S8-23. Low: dialog button casing differs from the rest of the UI
- **What's wrong:** The confirm and Save As dialogs use "Cancel", "Save and load", "Load anyway" and "Save". The browser, the tour and
  the cards use SAVE AS, DELETE, GOT IT and REMOVE. The tour says "Don't show this again" and the confirm says "Don't ask again".
- **Fix:** upper-case buttons everywhere (UI-CONVENTIONS doesn't cover buttons: add a rule).
- **Evidence:** `confirm/00-confirm.png`, `extras/extra-save-as.png`, `neuro/00-tutorial.png`.

### S8-24. Low: PLAY rows change height as oscillators are added
- **What's wrong:** Rows are ~175 px with 3 oscillators, ~123 px with 5, and ~115 px with 6. Everything jumps when you press
  + ADD OSC.
- **Fix:** keep one row height and scroll the column past 4 (Serum 2 keeps fixed oscillator panels).
- **Evidence:** `neuro/01-MAIN.png` against `neuro/added-osc-MAIN.png`.

### S8-25. Low: matrix rows in creation order
- **What's wrong:** LFO 1 is on rows 1, 10 and 12, and LFO 2 on rows 9 and 11.
- **Fix:** default sort by source (the header already sorts), or group same-source rows.
- **Evidence:** `neuro/07-MATRIX.png`.

### S8-26. Low: the matrix says "Macro 1 (TONE)", the strip says "TONE"
- **Fix:** "TONE (M1)", so the name you see on the knob comes first.

### S8-27. Low: the DX7 chip count changes
- **What's wrong:** The floating browser says "DX7 270" (repeats hidden). The docked one with ROM1A picked says "DX7 288".
  `isHiddenRepeat` depends on the bank filter (`PresetPanel.h:1726-1742`). Meanwhile "All 641" stays lit while a bank filter is on.
- **Fix:** count the same way in both states, and highlight the DX7 chip instead of "All" while it filters.
- **Evidence:** `neuro/preset-browser.png`, `extras/extra-browser-docked-dx7.png`.

### S8-28. Low: DEXED01 wraps onto its own chip row
- **Fix:** shrink the bank chips or put the banks in one scrolling row.
- **Evidence:** `extras/extra-browser-docked-dx7.png`.

### S8-29. Low (dead space): the OSC page's lower panel is half empty
- **What's wrong:** SUB + NOISE fills the left half; the right ~700 px is empty. The tab strip (VOICE, SYMPATHETIC STRINGS,
  ACOUSTIC KEYS) is also shown on DX7 voices, where none of them applies.
- **Evidence:** `neuro/03-OSC.png`, `dx7/03-OSC.png`.

### S8-30. Low: OSC 1's FM caption cased two ways
- **What's wrong:** "FM FROM 2" on PLAY and in the OSC tab, but "FM from OSC 2" in the OSC card header.
- **Evidence:** `neuro/03-OSC.png`.

### S8-31. Low: envelope plots mix a log time axis with "+100 ms" after KEY UP
- **What's wrong:** Ticks read 10 ms, 100 ms, then KEY UP, then "+100 ms". That is two scales in one plot.
- **Fix:** a linear axis with the release on the same scale, or label the log axis as log.
- **Evidence:** `neuro/env-pool-full.png`, `dx7/06-ENV-LFO.png`.

### S8-32. Low: the SEQ ARP tab shows bright lanes while ARP is off
- **What's wrong:** The header says "ARP is off: switch it on in its tab", but the lanes are drawn at full colour.
- **Fix:** dim the lanes at 60 % (the data stays readable) while the engine is off.
- **Evidence:** `neuro/09-ARP-SEQ.png`.

### S8-33. Low (alignment): the ARP MODE/RATE combos sit 18 px below the knob labels in the same row
- **Evidence:** `neuro/09-ARP-SEQ.png` (MODE/RATE labels at y≈605, OCTAVES/GATE at y≈587).

### S8-34. Low: the PLAY spectrum thumbnail is one bar
- **What's wrong:** In SPEC view, PLAY's 120 px thumbnail shows a single yellow line for OSC 1. It reads as a broken display.
- **Fix:** keep the wave thumbnail on PLAY, or draw the spectrum log-spaced.
- **Evidence:** `extras/extra-spec-and-card.png`.

### S8-35. Low: the source card covers the filter TYPE combo
- **What's wrong:** Hovering FRAME opens the card over FILTER's TYPE. It is fine while hovering, but it is wider than its content.
- **Evidence:** `extras/extra-spec-and-card.png`.

### S8-36. Low: OSC 3 "0 st 50%" captions beside a dashed OFF node
- **What's wrong:** On the FM diagram, an off oscillator still shows tuning and level captions.
- **Evidence:** `neuro/08-FM.png`.

### S8-37. Low: the output meter and resize grip crowd the bottom-right corner
- **What's wrong:** "OUT" sits 4 px from the window edge, and the grip's hatching runs into the meter.
- **Evidence:** `serum-extra/crop-out-meter.png`, `small/01-MAIN.png`.

### S8-38. Low: VECTOR and EVOLVE pages leave the bottom ~40 % of their right columns empty
- **Evidence:** `neuro/vector-page.png`.

### S8-39. Low: the clip editor's FIT button is disabled with no hint why
- **Evidence:** `neuro/gen-clip.png` (FIT grey between − and +).

### S8-40. Low: the FX header's "FX" dice and "SAVE / LOAD" sit far from CHAIN 1/2
- **What's wrong:** It is a three-item toolbar split across 1100 px.
- **Evidence:** `neuro/10-FX.png`.

### S8-41. Low (polish): the header waveform squiggle under the preset name changes on every shot
- **What's wrong:** It is decoration that moves constantly next to the most-read text. Serum 2 keeps its header still.
- **Fix:** a setting to turn it off, or draw it dimmer.

---

## Overall

**7.8 / 10 against Serum 2.** The review 7 round landed nearly everything it promised, and the core workflows (modulation by drag,
browsing, FX, the piano roll) are at Serum 2's speed. The page structure (PLAY / OSC / FILTER / MOD / FM / SEQ / FX) is clear.

What stands between this and 9.5 is the shortest list of changes below. Items 1-4 are the ones a Serum 2 user would notice in the
first five minutes:

1. **DX7 only where DX7 is used:** OP LFO / OP ENV / OP PITCH cards on MOD only when an oscillator plays OP ENV (S8-1), and the
   FM matrix footer and the OSC panels hidden on DX7 voices (S8-21, S8-29).
2. **One editor, one name:** OP PITCH / OP LFO everywhere (S8-3), and the full OP editors in one place, with thumbnails and links
   elsewhere (S8-4).
3. **Chip bar rank by "sounds", not by "routed"** (S8-2).
4. **One MSEG** (S8-5) **and one dry/wet widget** (S8-6).
5. **Layout pass:** ring dots vs values (S8-8), "6 opera…" (S8-10), hover line overlap (S8-11), the EXCITE font (S8-22), FX widths
   and empty space (S8-14), the LFO panel (S8-7), fixed PLAY row height (S8-24).
6. **Conventions pass:** switches beside every on/off text (S8-12), one "unused" string (S8-17), button casing (S8-23), lane
   readouts (S8-15).
7. **DRAW on by default** in an empty clip (speed table).

With 1-4 done, I would score it about 8.8. With 5-7 as well, it reaches about 9.3-9.5. What is left beyond that is visual
refinement (motion, the header, empty space on VECTOR / FX), which is about taste more than faults.
