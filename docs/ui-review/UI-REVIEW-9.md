# UI review 9 (pass 4): ilanaSynth v1.3 against Vital, Serum 2, and itself

One reviewer, three angles, one file. Build: branch `claude/project-thread-smvgfk`, `/home/user/ilanaSynth/build`. Evidence: `/home/user/shots9/{neuro,init,dx7,keys,small,dx7-small,extras,remap,confirm}/` (a shot is cited as `set/file`), plus my own renders in `/home/user/shots9/review-extra/` (`mseg/` = "MSEG Sweep", `chaos/` = "Double Pendulum Chaos", `fm/` = DX7 voice at algorithms 1, 18 and 32). I read the source in `src/gui/` and `src/PluginEditor.cpp` to confirm causes. I could not click or drag, so anything about live behaviour (drag feedback, hover cards, audio) is marked *(inferred)*. The snapshot tool edits the patch on the OSC page, so "EDITED" and the physical/wavetable flip after 03-OSC are tool artefacts, and so is the "+13" envelope chip in the bar (the tool reveals all 16 envelopes); I did not count those as findings.

## Verdict and scores

This is a different product from review 8. The three worst structural problems of that review are gone: the DX7-only cards no longer sit on every patch (neuro and Init MOD pages are clean), the operator envelope / OP PITCH / OP LFO now have one name and one editor, and the FM page, the SEQ page and the FX header read as parts of one instrument. The type fitting no longer squeezes text sideways, the drag-feedback code for knobs exists (`ParamControls.h:989-1030`), and WEST's knobs finally wear WEST's colour. What keeps it from 9.5 is no longer confusion but *finish*: large dead areas on five pages (PLAY's left column, FX, VECTOR, the OSC bottom panel, the small-patch FM matrix), full-size controls for modules that are off, a DX7 voice that hides two of its six operators behind a scroll on PLAY, units that silently disappear from knob values, labels in one row shrunk to different sizes, a still-visible legacy MSEG, a still-unexplained "B", and a handful of hidden features (voice mode and glide live behind grey header text). None of it is hard; all of it is what separates "very good" from "commercial".

| Angle | Review 8 | Review 9 | Target |
|---|---|---|---|
| Against Vital | 7.5 | **8.0** | 9.5 |
| Against Serum 2 | 7.8 | **8.0** | 9.5 |
| Integration | 7.5 | **8.3** | 9.5 |

Cross-references: findings are filed once, in the section they fit best; "(see X)" points to the home of a related one.

---

# Part 1: Against Vital

Vital's strengths I measure against: every page full and balanced, a modulation drag that always shows where it will land, one control style, and type that is the same size everywhere.

## Fixed since review 8 / still open (V8-1 to V8-40)

| # | Review 8 item | Status |
|---|---|---|
| 1 | DX-only cards on every patch's MOD page | **Fixed** (`neuro/06-ENV-LFO`, `init/06-ENV-LFO`: no OP cards; they appear on `dx7/06-ENV-LFO`) |
| 2 | Two MSEG systems | **Mostly fixed**: SHAPE > MSEG exists; old patches still show a separate MSEG card and chip (see I9-2) |
| 3 | Chip bar moves | **Fixed**: same regions at 75 % and 100 %, folds by width only |
| 4 | TRIM % on PLAY/OSC, dB on FM | **Fixed** (`dx7/08-FM`: TRIM 50 %) |
| 5 | Operator controls in three layouts | **Mostly fixed**: same order RATIO / (SEMI) / FINE / LEVEL / TRIM; PLAY omits SEMI |
| 6 | Dead areas on five pages | **Still open** (V9-1, V9-3, V9-7, V9-8, V9-9) |
| 7 | Deep algorithms read poorly | **Partly fixed**: nodes full size, but algorithm 18 still crowds (V9-10) |
| 8 | OP ENV ruler/handles/"-inf dB" | **Fixed** (ruler outside plot, "-∞ dB") |
| 9 | F1/F2 nodes overlap | **Mostly fixed**: fan apart on FILTER (`FilterDisplay.h:259-296`); on PLAY at 20 kHz they still touch (V9-24) |
| 10 | No target feedback while dragging | **Fixed in code** (`ParamControls.h:989-1030`) *(inferred: no drag shot)* |
| 11 | OP ENV card is not a source | **Partly fixed**: dashed outline now, unexplained (V9-22) |
| 12 | Squeezed text | **Mostly fixed**: nothing squeezed; new problem, units dropped (V9-5) |
| 13 | Destination names in three casings | **Fixed** ("MODULE › Control" everywhere in the matrix) |
| 14 | SUB + NOISE switch only switches sub | **Partly fixed**: now labelled SUB, but see V9-17 |
| 15 | Noise colour only on FM | **Fixed** (COLOUR on PLAY and OSC) |
| 16 | DX7 operators wear wavetable furniture | **Partly fixed**: PLAY clean; OSC still "Wavetable" combo, TABLE, FEEDBACK (I9-6) |
| 17 | Off oscillators keep full strips | **Fixed** (one-row strips) |
| 18 | Envelope knob order / two curve controls | **Fixed** (DELAY ATTACK HOLD DECAY SUSTAIN RELEASE VEL CURVE KEY RATE) |
| 19 | FX split group widths | **Partly fixed**: Reverb no longer clipped; widths still mix (S9-14) |
| 20 | Matrix lower third empty | **Partly fixed**: a REMAP placeholder fills it (S9-6) |
| 21 | VOICE PITCH & LFO in the pill row | **Fixed** ("OP PITCH · OP LFO ›") |
| 22 | ARP row misaligned, GATE twice | **Partly fixed**: aligned; two GATEs remain (I9-13) |
| 23 | PHYSICAL exciter row, BODY as links | **Partly fixed**: BODY has a switch; still a "FILTER ›" jump (I9-3) |
| 24 | PHYSICAL preview animates a missing string | **Fixed** ("Physical oscillators only", dimmed) |
| 25 | Browser DX7 count | **Fixed** ("DX7 270", "32 of 641" under ROM1A) |
| 26 | VECTOR empty band and EVOLVE pane | **Partly fixed**: EVOLVE moved to the macro; the page is still sparse (V9-7) |
| 27 | Signal-flow abbreviations | **Partly fixed** ("SUB/NOISE" vs "SUB + NOISE", I9-10) |
| 28 | "B" sub-chip unexplained | **Still open** (V9-20) |
| 29 | LFO cards print routing over the waveform | **Still open** (V9-16) |
| 30 | OUT label collides with grip | **Still open** (V9-19) |
| 31 | Hover bar covers content | **Fixed** (`neuro/hover-line`: sits between the page and the chip bar) |
| 32 | "Off" for a depth, raw curve names | **Not verified** (no KEYS & VELOCITY shot this round) |
| 33 | Reverb KEEP DRY | **Fixed** (WET ON TOP) |
| 34 | Per-card CPU readouts | **Still open** (V9-21) |
| 35 | Oversized FM matrix cells | **Partly fixed** (V9-8) |
| 36 | Sub-tabs hard to find | **Fixed** (page switch beside the tabs) |
| 37 | Four "MACRO 5-8 0 %" knobs | **Fixed** ("+ ASSIGN", dimmed) |
| 38 | Duplicate FX card BLEND pill | **Fixed** (`neuro/fx-duplicate`) |
| 39 | No filter on/off | **Still open** (V9-13) |
| 40 | Dot after LFO/ENV names | **Fixed** |

## Findings (Vital), worst first

**V9-1. High: PLAY's left column ends in dead space.**
- What: on Init, 4 oscillators off, SUB + NOISE ends at y 707 of 955 (about 165 logical px empty, `init/01-MAIN`); on Neuro about 90 px (`neuro/01-MAIN`). The right column fills to the bottom, so the page looks unfinished and lopsided.
- Vital: pages are always full; sections stretch.
- Fix: let the oscillator rows share the free height (rows grow, up to a cap), or put a thin scope or the keyboard thumbnail there. Evidence: `init/01-MAIN`, `neuro/01-MAIN`, `MainPage.h` layout.

**V9-2. High: a DX7 voice on PLAY shows four of six operators; the rest are behind a scroll with a 3 px bar.**
- What: `dx7/01-MAIN` and `dx7-small/01-MAIN` show OSC 1-4 and a thin scrollbar at the column edge; OSC 5 and 6 (the carriers of algorithm 5's third pair) are hidden. The overview page's job is to show the whole patch.
- Vital: the main page never scrolls.
- Fix: at five or six oscillators switch the strips to a compact row (about 100 px: thumbnail 60 px, four knobs) so six fit; keep the tall strip for 1-4.

**V9-3. High: the FX page leaves its bottom third empty, and the empty "+ ADD EFFECT" slot is as big as a real card.**
- What: with three effects, 250 px below OUTPUT is blank (`neuro/10-FX`), and the add slot is a 200 px dashed card. With a full-width slot free it is a slim row (`neuro/fx-33`). Two sizes for one idea.
- Fix: the add slot is always one slim row; below the rack show a rack summary or let cards grow taller with a larger graph.

**V9-4. Medium-high: off modules keep their whole body.**
- What: WEST and BODY, switched off, are two 280 px cards of fourteen dimmed controls (`neuro/05-FILTER`, `keys/05-FILTER`): half the FILTER page showing nothing. The OSC strips already collapse when off (V8-17 fix).
- Fix: collapse an off card to its header plus its one-line caption; expand on switch-on (animate it).

**V9-5. Medium-high: units silently disappear from knob values.**
- What: on a DX7 voice's PLAY, OSC 2's LEVEL reads "-30.9" while OSC 4's reads "-7.5 dB" in the same column (`dx7/01-MAIN`). Cause: when the value does not fit, `IlanaLookAndFeel.h:272-285` drops the unit ("the number alone").
- Vital: a readout always carries its unit or never does.
- Fix: never drop a unit. Fit the whole column to the smallest size, or widen the knob label cell to 72 px, or show "LEVEL dB" once in the label and numbers only.

**V9-6. Medium-high: labels in one row are shrunk to different sizes.**
- What: UNISON is visibly smaller than SEMI and LEVEL (`neuro/01-MAIN`); SUSTAIN, RELEASE and KEY RATE are smaller than ATTACK and DELAY (`neuro/06-ENV-LFO`); the 75 % shot makes KEY RATE nearly unreadable (`small/06-ENV-LFO`). Each label fits itself (`drawFitted`) instead of the row fitting as one.
- Fix: a row-level fit: compute the smallest size any label in a card row needs and apply it to all, or shorten the name (KEY RATE to KEY RT, UNISON to UNI) so no label shrinks.

**V9-7. Medium: the VECTOR page spreads six groups across the height with nothing to bind them.**
- What: PATH is a lone toggle with 80 px of nothing around it; corner combos float; X / Y knobs are 150 px lower; WANDER 200 px lower again (`neuro/02-VECTOR`, `init/vector-page`).
- Fix: three labelled boxes like SEQ's GENERATE (the stable pattern in this build): CORNERS (2 × 2), POSITION (X, Y), MOTION (PATH + rate, WANDER + rate); the pad stays left and gets taller.

**V9-8. Medium: the FM matrix on a small patch floats in an empty card.**
- What: three oscillators fill about 130 px of a 700 px card; the hint, headers, cells and footer are separated by 100 px gaps (`neuro/08-FM`, `neuro/fm-1`).
- Fix: top-align hint, headers, cells and footer; for 3 oscillators draw cells larger, or show a "ring mod / sync / noise" panel beside it.

**V9-9. Medium: the OSC page's lower half is air.**
- What: SUB + NOISE spreads two knobs and three combos over 1500 px with 200 px gaps (`neuro/03-OSC`); the shape panel has 70 px empty under UNISON; WARP AMT and SPEC AMT are drawn dim at full size while the combos say Off. (Same as S8-29, still open.)
- Fix: width-limit the SUB + NOISE panel to its content and put VOICE content beside it; hide the AMT knobs until a warp is chosen.

**V9-10. Medium: algorithm 18 still crowds and its labels drift.**
- What: OSC 6 sits flush against the diagram's top edge with its halo clipped; the long curved links from OSC 2 and OSC 4 carry their "35 %" labels 150 px away from the node they belong to (`review-extra/fm/fm-dx7-algorithm-18.png`).
- Fix: shrink the diagram's layout margin so the top row has 12 px of air; place the depth label on the link's midpoint nearest the destination; keep the label off the arrowhead.

**V9-11. Medium: combos in one row use different font sizes.**
- What: the physical page's EXCITE combo "Piano Hammer" is visibly smaller than the neighbouring labels (`keys/physical-page`, same as S8-22); on the FM card "Harmonic" and "AMP ENV" are smaller than "Ratio" and "Semitones" in the same row (`neuro/fm-dx-keys`).
- Fix: widen the combos in the row (the row has the room: 170 px free) or fit the row, as V9-6.

**V9-12. Medium: five different "add" affordances.**
- What: "+ ADD OSC 4" is a filled pill inside a dashed row on PLAY (`MainPage.h:487`), a pill in the tab row on OSC (`OscPage.h:571`), "+ ADD EFFECT" is its own dashed card class (`FxLibrary.h:515`), "+ ADD MODULATION" is a `DashedAddButton` (`MatrixPage.h:26`), and LFO/envelope pools use a square "+" tile; the chip bar has a "+" too.
- Fix: one component (dashed outline, "+ ADD X", same height) and a "+" tile only for pools.

**V9-13. Medium: a filter that does nothing is drawn at full strength (V8-39).**
- What: Filter 2 at 20 kHz, 0 % reso, a full-colour card, a lit "FILTER 2" dot (`neuro/05-FILTER`). There is no switch, so a user cannot tell "off" from "open".
- Fix: a card-header switch like WEST's (parameter append-only: off = bypass), or grey the card when its cutoff is at the open end and the mix is serial.

**V9-14. Medium: SCOPE replaces PLAY but PLAY and OVERVIEW stay lit.**
- What: `neuro/scope-panel`: the content is the scope, the PLAY tab and its OVERVIEW pill are still the lit ones, and SCOPE is lit too. Which "page" am I on?
- Fix: dim the page tab and the pill while a tool page covers them, or make SCOPE a sub-page pill beside OVERVIEW / VECTOR.

**V9-15. Medium: the "Replace your edits?" dialog makes the destructive button the primary.**
- What: LOAD ANYWAY is the filled orange button; SAVE AND LOAD and CANCEL are grey (`confirm/00-confirm`).
- Fix: filled = the safe choice (SAVE AND LOAD); LOAD ANYWAY outlined.

**V9-16. Medium: routing text and badges sit on LFO waveforms (V8-29, still open).**
- What: "FILTER 1 › Cutoff +2 more" is a pill over LFO 1's waveform, "OUT B" over the lower right of LFO 2's (`neuro/06-ENV-LFO`); on a 2 px curve the overlays win.
- Fix: a 16 px caption strip under the curve for routing, badge in the header row.

**V9-17. Medium: two "SUB"s and a noise with no switch.**
- What: on PLAY's SUB + NOISE strip the knob is "SUB 50 %" and the switch is labelled "SUB" a few pixels right of it (`neuro/01-MAIN`). The switch only switches the sub (`OscPage.h` sub tab), so noise cannot be turned off but 0 %.
- Fix: label the switch with the card ("SUB + NOISE") or drop its label; the 0 % noise knob is its own off.

**V9-18. Medium-low: modulation rings touch labels.**
- What: OSC 1's FRAME ring on PLAY reaches the "FRAME" label (`neuro/01-MAIN`, `neuro/mod-pinned-chip`: the pinned ring crosses the CUTOFF label too). The 1 px gap loses to the 2.5 px ring.
- Fix: 4 px between ring and label, or clip rings at the label's top (`ParamControls.h:994` `reduceClipRegion`).

**V9-19. Medium-low: the output meter is the smallest thing on the screen.**
- What: bottom right, about 15 × 20 px with "OUT" under it, a few px from the resize grip (`neuro/01-MAIN`). Vital's meters are readable at a glance; the scope's meter is large (`neuro/scope-panel`) but hidden.
- Fix: a 40 px wide stereo meter beside MASTER; move the grip to the corner.

**V9-20. Medium-low: "B" is still unexplained (V8-28).**
- What: a "B" chip on the LFO 2 source chip and "OUT B" on its card (`neuro/01-MAIN`). The tooltip explains it; the screen does not.
- Fix: spell it "LFO 2 · 2nd out" or "OUT 2"; the chip name already says which LFO.

**V9-21. Low-medium: per-card CPU shows on some FX cards only (V8-34).**
- What: "FREEZE CPU 2.1 %" beside the title (`neuro/fx-12`), nowhere else. A number that appears from time to time reads as an error.
- Fix: hide cards' CPU (the header total exists) or show it on all in a hover.

**V9-22. Low-medium: the OP ENV pool card's dashed outline has no meaning on screen.**
- What: `dx7/08-FM-pitch-lfo`: OP ENV is dashed; the others are solid. *(Inferred: "not a source".)*
- Fix: a "per operator" caption, or the same outline plus a tooltip line.

**V9-23. Low-medium: scrolling pages cut content hard.**
- What: the FX page scrolls under a fixed header row with a hard clip at y 195, no fade, a 3 px bar (`neuro/fx-duplicate`, `fx-midside`).
- Fix: an 12 px top fade under the CHAIN row, and a wider hover scrollbar.

**V9-24. Low-medium: filter markers at 20 kHz.**
- What: on FILTER they are pinned to the plot edge with a vertical line cut by the border; on PLAY's 70 px graph they sit side by side touching halos (`init/01-MAIN`).
- Fix: inset by the halo radius; on PLAY draw one marker with a "1 · 2" badge.

**V9-25. Low-medium: disabled text may be below readable contrast** *(inferred from shots, not measured)*. Dim labels in WEST / BODY when off and "WARP AMT" look near 2:1 (`neuro/05-FILTER`). Fix: floor disabled text at 3:1.

**V9-26. Low: 75 % zoom puts the smallest text at about 9 px.** "KEY RATE", DRIVES lines and axis ticks (`small/06-ENV-LFO`). A 75 % plugin window is a normal choice on laptops. Fix: floor tiny text at 10 px screen size by dropping the element instead (as the code already does for hints) or by growing the window minimum.

**V9-27. Low: the header's BPM / VOICES / CPU are three grey fragments.** "VOICES 1 / 32" is actually a button for voice mode and glide (see S9-1); nothing says so.

**V9-28. Low: matrix column headers: "TO OSC 1" then bare "OSC 2" and "OSC 3".** On Init the prefix is dropped on the off columns (`neuro/fm-1`). Fix: "TO OSC n" on all, dim for off.

**V9-29. Low: log time axes label two ticks.** The AMP ENV plot shows "10 ms", "100 ms" at the left, then nothing until KEY UP (`neuro/06-ENV-LFO`). Add "1 s".

**V9-30. Low: the Save dialog has two ways to enter tags.** A free-text field and a chip row that edit the same list (`extras/extra-save-as`). Fine for power users; it reads as a duplicate. Fix: hide the text field under a "type your own" link.

**V9-31. Low: the LFO DRIVES list is plain text** *(inferred, I did not click)*. Vital's routing is clickable. Fix: each row jumps to its matrix row.

## Score against Vital: 8.0 / 10

Layout clarity 8, drag-drop workflow 8.5, visual feedback 8, consistency 8, typography 7.5. Review 8's headline faults are fixed; what remains is dead area and fit-and-finish.

### The shortest list to 9.5 against Vital
1. Fill or collapse every dead area: V9-1, V9-3, V9-4, V9-7, V9-8, V9-9.
2. All six DX7 operators visible on PLAY (V9-2).
3. Type: never drop a unit, one font size per row (V9-5, V9-6, V9-11).
4. One "add" component (V9-12) and a filter on/off (V9-13).
5. The remaining small marks: V9-14, V9-15, V9-16, V9-19, V9-20.

---

# Part 2: Against Serum 2

Serum 2's strengths I measure against: a clear page per job, a fast oscillator and warp workflow, drag-and-drop everywhere, an effects rack you can reorder, a good clip/arp sequencer and a browser that never leaves you.

## Workflow speed (the same four jobs as review 8)

| Job | ilanaSynth now | Serum 2 | Verdict |
|---|---|---|---|
| Route an LFO to cutoff and set the depth | Drag the LFO 1 chip onto CUTOFF (1 drag), drag its ring (1 drag): **2 gestures**, from any page | 2 gestures | **Par** |
| Browse and audition 5 presets | Click the name (1), Down × 5 (each loads), Enter; or 5 clicks on the arrows: **1 click + keys** | about the same | **Par** (DX7 chip count now steady) |
| Add an effect and move it first | + ADD EFFECT (1), pick the type (1), drag its header to slot 1 (1 drag): **2 clicks + 1 drag** | same | **Par** |
| Draw an 8-note clip | SEQ (1), CLIP tab (1), its tiny switch (1), notes by drag; DRAW is now on (`neuro/gen-clip`: lit). **3 clicks + 1 drag or 8 clicks** | clip editor draws on a click *(inferred)*, about 2 clicks | **One click slower**: the tab does not turn the engine on (S9-3) |

New since review 8: a fifth job, "set voice mode to mono and add glide", is **3 clicks and a hidden menu**, because the controls live behind the grey "VOICES 1 / 32" text (S9-1); Serum has them on a global page.

## Fixed since review 8 / still open (S8-1 to S8-41)

| # | Review 8 item | Status |
|---|---|---|
| 1 | Three dead DX7 cards on MOD | **Fixed** |
| 2 | Chip bar folds OP ENV on DX7 voices | **Mostly fixed**: OP LFO and OP PITCH lead; AMP ENV (unused) still has a chip (`dx7/06`) |
| 3 | Pitch EG / LFO three names | **Fixed** (OP PITCH, OP LFO) |
| 4 | Duplicate editors | **Mostly fixed**: one component, still on MOD, FM and a graph on OSC |
| 5 | MSEG two systems | **Mostly fixed**: old patches show an MSEG card with MOVE TO LFO (`review-extra/mseg/06`, `EnvLfoPages.h:896-964`) |
| 6 | Dry/wet drawn two ways | **Fixed** (a MIX knob on each card) |
| 7 | LFO and MSEG panels empty | **Partly fixed**: panel has DRIVES; still 150 px empty under it (`neuro/06`) |
| 8 | Mod-ring dots on value text | **Partly fixed**: rings now touch labels (V9-18) |
| 9 | DX7 operators wavetable furniture | **Partly fixed** (I9-6) |
| 10 | "6 opera…" truncation | **Fixed** (wraps, `dx7/01-MAIN`) |
| 11 | Hover line covers cards / third name | **Fixed** ("OSC 1 › Semi" under the page, `neuro/hover-line`) |
| 12 | State as text without a switch | **Fixed** (BODY and SOUNDBOARD have switches) |
| 13 | FM page empty on non-FM patch | **Partly fixed** (V9-8) |
| 14 | FX card widths | **Partly fixed** (S9-14) |
| 15 | ARP lane readouts | **Fixed** (numbers on all three lanes, `neuro/gen-arp-lanes`); placement inconsistent (S9-13) |
| 16 | "B" unexplained | **Still open** (V9-20) |
| 17 | "unused" said two ways | **Fixed** |
| 18 | "+2" reads as depth | **Fixed** ("+2 more") |
| 19 | TRANSPOSE stepper one-off | **Fixed** (knob, `OperatorPoolCards.h:288`) |
| 20 | VECTOR X/Y two places | **Fixed** |
| 21 | FM footer on DX7 voices | **Fixed** |
| 22 | EXCITE combo smaller font | **Still open** (V9-11) |
| 23 | Dialog button casing | **Fixed** |
| 24 | PLAY rows change height | **Fixed** (`neuro/added-osc-MAIN`) |
| 25 | Matrix rows in creation order | **Partly fixed**: SOURCE / AMOUNT / DESTINATION sort handles on the header |
| 26 | Macro named three ways | **Partly fixed**: "TONE (M1)" vs "TONE" (I9-8) |
| 27 | DX7 chip count changes | **Fixed** |
| 28 | DEXED01 wraps onto its own row | **Partly fixed**: still a second row (`extras/extra-browser-dx7`), now tidy |
| 29 | OSC lower panel half empty | **Still open** (V9-9) |
| 30 | FM caption cased two ways | **Fixed** |
| 31 | Envelope plots mix scales | **Partly fixed** (V9-29) |
| 32 | ARP lanes bright while off | **Fixed** (dimmed) |
| 33 | ARP combos 18 px low | **Fixed** |
| 34 | PLAY spectrum thumbnail one bar | **Fixed** *(inferred: gone from the shots)* |
| 35 | Source card covers TYPE combo | **Fixed** |
| 36 | OSC 3 captions beside OFF node | **Fixed** |
| 37 | Meter and grip crowd | **Still open** (V9-19) |
| 38 | VECTOR / EVOLVE empty | **Partly fixed** (V9-7) |
| 39 | FIT disabled without a hint | **Fixed** (`ClipEditor.h:1782-1788` tooltip) |
| 40 | FX header dice and SAVE/LOAD far from CHAIN | **Fixed** (one grouped row) |
| 41 | Header squiggle changes per shot | **Open by design**: it is a live scope; it still reads as an underline (S9-26) |

## Findings (Serum 2), worst first

**S9-1. High: voice mode, voice count, pitch-bend range and glide are behind grey header text.**
- What: they open from a menu when you click "VOICES 1 / 32" (`PluginEditor.cpp:392`, `:1085`, `:2139-2188`, glide is a custom menu item `:2151`). Nothing on any page says so; the text looks like a status.
- Serum 2: a Global tab with these as visible controls.
- Fix: a VOICE card on the OSC page's shared strip with POLY / MONO / LEGATO as a three-way switch, VOICES, BEND, GLIDE as knobs; keep the header text as a shortcut.

**S9-2. High: "VOICE" names two different things.**
- What: the OSC page's shared tab "VOICE" is about unison spread, start and drift (`OscPage.h:895`: "How the unison voices spread, start and drift"), while "VOICES" in the header is the polyphony/mode menu. A user hunting for mono will open the VOICE tab and not find it.
- Fix: rename the tab UNISON (it holds that) and let S9-1's card be VOICE.

**S9-3. Medium-high: the SEQ tabs select and enable separately.**
- What: clicking a tab shows the engine; a tiny switch inside the pill turns it on (`neuro/09-ARP-SEQ`: ARP shown, "ARP is off: switch it on in its tab"). The first job in the speed table pays for it.
- Fix: clicking the tab of an engine that is off shows it and asks nothing, but the editor has a full-width "SWITCH ON" row across the lanes until it is on (so one click, one obvious target). Or: selecting a tab switches it on when no engine is on.

**S9-4. Medium-high: modulation and parameters have three homes.**
- What: oscillator controls on PLAY, OSC and (for operators) FM; filter on PLAY and FILTER; envelopes on PLAY and MOD; LFO on PLAY and MOD. They are the same parameters in different layouts (PLAY has no SEMI for an operator, FM has no PAN). That is fine for speed, but nothing marks PLAY's as the "quick" copy.
- Fix: a small "all controls ›" on each PLAY card header that jumps to the full page (the ↗ icon exists on some cards but not on the oscillators; see I9-14).

**S9-5. Medium: the Airwindows flip swaps every control.**
- What: on DRIVE, BUILT-IN shows AMOUNT ×5.0 / MIX / FOLD, AIRWINDOWS shows ALGORITHM / DENSITY / HIGHPASS / OUTPUT / MIX in raw % (`neuro/fx-33`, `fx-split`). The knob positions do not carry. (Same as I8-34.)
- Fix: write units (dB, Hz) where the Airwindows effect has them; put the "algorithm" combo first on both.

**S9-6. Medium: the matrix's REMAP placeholder is 300 px of nothing.**
- What: with 8 routes the area below holds only "Click a row's CURVE to draw ..." (`dx7/07-MATRIX`); with the editor open the list shrinks to 8 of 12 rows (`remap/remap-editor`).
- Fix: show it only after a CURVE click, and give the list that room otherwise; or show a small spectrum of "which parameters are modulated".

**S9-7. Medium: matrix rows are 56 px tall, 12 fit.** Serum's matrix rows are about half that; with 20+ routes you scroll constantly (`neuro/07-MATRIX`). Fix: 36 px rows, with the polarity and curve visible on hover.

**S9-8. Medium: the browser's chrome eats the list.**
- What: search, a DX7 chip row, one or three rows of bank chips, a tag row and then 11 list rows (`extras/extra-browser-dx7`: four chip rows).
- Fix: collapse bank chips under one "BANK ▾"; the DX7 count chip stays.

**S9-9. Medium: STRUM and SNAP TO KEY are gated twice.**
- What: STRUM has a switch and a DIRECTION combo that includes "Off" (`neuro/09-ARP-SEQ`: switch off and DIRECTION Off); SNAP TO KEY has a switch, a KEY combo that includes Off, and SNAP PLAYED.
- Fix: one gate: remove "Off" from the combos, or drop the switch.

**S9-10. Medium: no MIDI drag-out or export from the clip editor** *(inferred: only IMPORT MIDI exists, `ClipEditor.h`)*. A generated clip cannot leave the synth. Fix: EXPORT MIDI, ideally drag from the editor.

**S9-11. Medium-low: the VEL lane in the clip editor has no scale.** The ARP lanes got numbers (`gen-arp-lanes`); the clip's VEL strip is bare stems (`neuro/gen-clip`). Fix: the same 1-127 ticks and a value on hover.

**S9-12. Medium-low: the frame readout and the FRAME knob disagree on screen.** "FRAME 6 / 64" under the 3D view next to a FRAME knob reading 30 % (`neuro/03-OSC`). The caption is the live, modulated frame (`WaveDisplay.h:1004`), the knob is the set value; nothing says "now". Fix: "NOW FRAME 6 / 64" or show both as one ring.

**S9-13. Medium-low: ARP lane numbers sit in different places.** PITCH values are above the bar for negative, below for positive, on the zero line across bar edges (`neuro/gen-arp-lanes`: "-5" over the bar, "+7" under the lane). Fix: one position per lane, a fixed readout row.

**S9-14. Medium-low: FX card widths mix full and half.** A three-knob CHORUS takes the full width with 400 px of air between its graph and knobs (`neuro/fx-split`); the same card is half width elsewhere. Fix: three-knob effects always half width; reserve full width for long ones.

**S9-15. Medium-low: PROB SEQ shows three controls and an empty half row** (`neuro/gen-probseq`). ARP and EUCLID fill six slots. Fix: add the engine's own useful parameters (swing, direction) or centre the three.

**S9-16. Low-medium: "STEP OFF" at the top left of the ARP lanes is unexplained.** *(inferred from `neuro/09-ARP-SEQ`)* Fix: tooltip and a one-word label.

**S9-17. Low-medium: the chip bar shows an unused AMP ENV on DX7 voices** while OP ENV, the envelope the voice plays, is not a source (`dx7/06-ENV-LFO`). Fix: hide AMP ENV there until it is used, as for OP cards.

**S9-18. Low-medium: the oscillator's 3D view and WAVE view have no shown "warp" handles** *(inferred)*. Dragging up and down on the plot changes the warp amount (`WaveDisplay.h:376-382`) but the WARP AMT knob sits dim next to "Off" and nothing hints it. Fix: a one-line hint in the plot ("drag: frame, warp").

**S9-19. Low-medium: the table browser has no audition and no favourites** *(inferred from `neuro/table-browser-editor`)*: a grid of names; picking loads it into the oscillator. Fix: hover plays; a star as in the preset browser.

**S9-20. Low: the welcome tour is a wall.** Three bullets plus twelve "NEW IN 1.3" pills (`neuro/00-tutorial`). A first-run user needs the first three lines. Fix: the pills behind "WHAT'S NEW".

**S9-21. Low: tab shortcuts reach only the seven tabs.** Ctrl+1-7 (`PluginEditor.cpp:2771`); the sub-pages (VECTOR, PHYSICAL, MATRIX) have none. Fix: Ctrl+Shift+1-3 or Tab inside a page.

**S9-22. Low: `+ ADD EFFECT` is refused for a second instance of a type** (settled: not reopened) but the duplicate card's text exposes the engine: "(older patches can do this)" (`neuro/fx-duplicate`). Fix: cut the parenthesis.

**S9-23. Low: preset browser rows show 3 tags and a category chip, no author or date.** Serum's browser carries the author. Fix: an author column on wide docks.

**S9-24. Low: bank chips such as "ROM1A" repeat inside DX7 preset rows** (`extras/extra-browser-docked-dx7`, dim suffix) as well as in the filter chips; fine, but the suffix is also the sort key: unclear. Fix: tooltip only.

**S9-25. Low: "ADD OSC 6" row on PLAY is inside the scroll** (`neuro/added-osc-MAIN`), so at 5 oscillators it can be off-screen. Fix: pin it under the list.

**S9-26. Low (polish): the header scope squiggle sits just below the preset box like an underline.** (`neuro/01-MAIN`) with BPM below it. Fix: give it a 24 px band with a faint baseline.

## Score against Serum 2: 8.0 / 10

Page organisation 8, oscillator 8, FX and modulation workflow 8.5, clip and arp 8, browser 8, visual polish 7.5. The workflow is at parity with Serum on three of four jobs and one click short on the fourth; what holds it back is hidden voice settings and the finish items from Part 1.

### The shortest list to 9.5 against Serum 2
1. Surface voice mode, voices, bend and glide (S9-1) and rename the unison tab (S9-2).
2. One-click engine enable in SEQ (S9-3).
3. Fill the dead areas (Part 1: V9-1, V9-3, V9-4, V9-9).
4. Tighten the matrix (S9-6, S9-7) and the browser chrome (S9-8).
5. Clip editor: EXPORT MIDI, velocity scale (S9-10, S9-11); PROB SEQ fill (S9-15).

---

# Part 3: Integration

Question: does the DX7 / FM operators and Operator Env, physical modelling, vector, Airwindows, chaos and physics LFOs, MSEG / Steps, the sequencing engines and sample / SF2 feel like one synth? Checked against `docs/UI-CONVENTIONS.md`.

## Fixed since review 8 / still open (I8-1 to I8-40)

| # | Review 8 item | Status |
|---|---|---|
| 1 | TRIM dB vs % | **Fixed** (`dx7/08-FM`: TRIM 50 %) |
| 2 | Matrix calls TRIM "Level" | **Fixed** ("OSC 2 › Trim", `dx7/07-MATRIX`) |
| 3 | Op Env time knobs dead on flat segments | **Not verified** (needs a drag) |
| 4 | "MSEG" names three things | **Mostly fixed**; see I9-2 |
| 5 | DX7-only cards on every patch | **Fixed** |
| 6 | Three Op Env editors | **Mostly fixed**: one component |
| 7 | OP ENV time axis unlike others | **Fixed** (log, `dx7/06`) |
| 8 | "PITCH & LFO" | **Fixed** |
| 9 | Chip bar folds sources the voice uses | **Fixed** |
| 10 | "Pick an oscillator" drawn four ways | **Still open** (I9-5) |
| 11 | FM diagram jumps | **Fixed** *(inferred: `dx7/08-FM` and `-pitch-lfo` keep it in place)* |
| 12 | Matrix casing | **Fixed** |
| 13 | EVOLVE a separate machine | **Fixed** (on the macro: the yellow ∿ on the strip, `neuro/vector-page`) |
| 14 | VECTOR X/Y twice | **Fixed** |
| 15 | FM operator wavetable clothes | **Partly fixed** (I9-6) |
| 16 | Physics LFO says "free-running" | **Changed** (I9-1) |
| 17 | LFO panel moves switches | **Fixed** (`chaos/06-ENV-LFO`: same SYNC / RETRIG / KEY row) |
| 18 | PLAY ENVELOPE opens AMP ENV on DX7 | **Fixed** (OP ENV tab first, `dx7/01-MAIN`) |
| 19 | FM routing two vocabularies | **Fixed** *(inferred)* |
| 20 | On/off said in text | **Fixed** (switches beside every state word) |
| 21 | On dot means "in use" | **Fixed** |
| 22 | WEST's knobs ignore WEST's colour | **Fixed** (`neuro/filter-west`: lime) |
| 23 | Four SEQ engines lay out differently | **Mostly fixed**: RATE / STEPS / GATE align; PROB SEQ short (S9-15) |
| 24 | ARP lanes look live while off | **Fixed** |
| 25 | Squeezed text | **Mostly fixed** (V9-5) |
| 26 | Physics units | **Fixed** ("1.74 m", "1.00 kg", "90°", `chaos/06`) |
| 27 | KEYS & VELOCITY curve names | **Not verified** |
| 28 | OP PITCH graph has no ruler | **Still open** (I9-11) |
| 29 | One instruction, two grammars | **Fixed** *(inferred)* |
| 30 | OSC card says "(FM page)" | **Fixed** ("EDIT OP ENV ›") |
| 31 | Small name pairs | **Partly fixed** (I9-10) |
| 32 | Grey header dots read as off | **Fixed** |
| 33 | "AIRWINDOWS (ALL)" | **Not verified** (no add-menu shot) |
| 34 | Airwindows knobs in raw % | **Still open** (S9-5) |
| 35 | Dash / parenthetical style | **Fixed** ("OSC 3: off") |
| 36 | Legacy LFO shapes share names | **Not verified** |
| 37 | PLAY thumbnail two meanings | **Fixed** (each strip's thumbnail is labelled by its content) |
| 38 | OP LFO shape order | **Not verified** |
| 39 | OP ENV editor on a patch without operators | **Fixed** (absent) |
| 40 | Host parameter names | **Not verified** |

## Does it follow `docs/UI-CONVENTIONS.md`?

| Rule | Verdict |
|---|---|
| On/off switches in the header, right | **Mostly**: BODY and SOUNDBOARD on PHYSICAL put the switch under the label at the left (`keys/physical-page`) while SEQ's GENERATE boxes put it right of the header (I9-4) |
| One on dot | **Yes** |
| Captions lower-case fragments | **No in one place**: the matrix title reads "12 of 64 slots in use.  Drag a source onto any knob, then ..." with capitals and full stops (`neuro/07-MATRIX`; I9-9) |
| Casing | **Mostly**: modules and sources upper; band and option names drift (I9-16) |
| Separators " · " | **Yes** (the code search for spaced hyphens finds none in user text) |
| Fitted text only through `drawFitted` | **Yes** (no `drawFittedText` outside the helper); the fallback is the unit-dropping of V9-5 |
| Tabs without a switch have no dot | **Yes** |
| Page switch beside the tabs | **Yes** |

## Findings (Integration), worst first

**I9-1. High: the simulation LFOs have three controls for one idea, and their caption contradicts itself.**
- What: a chaos or physics LFO shows RETRIG (off), KEY (off) and a TRIGGER combo ("Note"), under the line "shared by all voices, restarts on each note; 2 outputs (A, B)" (`chaos/06-ENV-LFO`, caption built at `LfoShapeMenu.h:188-196`). RETRIG means "one LFO per voice" on plain LFOs but on a simulation the TRIGGER combo decides when it restarts. And "shared by all voices" plus "restarts on each note" means a new note resets the held notes' modulation.
- Fix: for simulations hide RETRIG, show TRIGGER (Free / Note / Beat / Step) plus a PER VOICE switch; caption: "one shared run, restarts when a note starts (held notes jump)".

**I9-2. High: the legacy MSEG is still a separate pool card, chip and white source.**
- What: on an old patch the LFO row has a fourth card named MSEG, a white "MSEG" chip, and a matrix source "MSEG" with no family colour (`review-extra/mseg/06-ENV-LFO.png`, `07-MATRIX.png`). The card's editor and MOVE TO LFO are hidden until selected (`EnvLfoPages.h:896-964`).
- Fix: migrate on load: loading an old patch runs MOVE TO LFO (the audio is identical, per the code comment), keeping the old module only inside saved files; or colour it as LFO 4.

**I9-3. Medium-high: physical modelling has two homes with two layouts.**
- What: an oscillator's string, exciter and body controls are on the OSC card (`neuro/osc-sympathetic`) and again on the PHYSICAL page (`keys/physical-page`); BODY's own switch is on FILTER and PHYSICAL, with a "FILTER ›" jump; the soundboard is on PHYSICAL, OSC's ACOUSTIC KEYS and the flow graph.
- Fix: PHYSICAL is the only editor; the OSC card for a physical oscillator becomes the string preview, DECAY / DAMP, and "EDIT STRING ›".

**I9-4. Medium: switch placement differs between sub-boxes.**
- What: in SEQ's GENERATE the switch is right of each box's header (`neuro/09-ARP-SEQ`); on PHYSICAL's BODY and SOUNDBOARD rows it sits left under the label (`keys/physical-page`). `UI-CONVENTIONS.md` says "a switch in its sub-box header".
- Fix: move them into the group's header line.

**I9-5. Medium: "pick an oscillator" is still drawn four ways.**
- What: OSC page: large pills with a role ("OSC 2 OUT, MOD › 1"); PHYSICAL: pills with a ring dot; FM operator card: "OSC 1 / OSC 2 / OSC 3" on a 3-op patch but dots "1 2 3 4 5 6" on a 6-op one (`neuro/08-FM` vs `dx7/08-FM`); MOD's OP ENV card: "1 ... 6" plus "FM ›".
- Fix: one `OscPicker` with a compact variant (numbers) used wherever width is short.

**I9-6. Medium: an FM operator on OSC still says "Wavetable".**
- What: the OSC card's mode combo reads "Wavetable" for an operator whose caption says "FM operator, a carrier (heard), plays its OP ENV"; below it TABLE / FEEDBACK / FB TYPE and a sine preview (`dx7/03-OSC`). PLAY calls the same thing "OPERATOR".
- Fix: the combo shows "Operator" (the table is "WAVE: Sine"), the preview is the OP ENV only.

**I9-7. Medium: the operator's level has two names depending on mode.**
- What: the oscillator's amplitude is "LEVEL 100 %" on a normal patch (`neuro/fm-dx-keys`) and "TRIM 50 %" on a DX7 voice, where LEVEL is the operator output in dB (`dx7/08-FM`). KEY LVL (dB/oct) exists only on the non-DX7 card. The shared-decision rule says TRIM is the oscillator level, always.
- Fix: say TRIM on both (the PLAY strip of a normal patch would then show TRIM), or LEVEL (%) on both and call the DX7 one OUTPUT.

**I9-8. Medium: macros are named three ways.**
- What: "TONE" in the strip, "TONE (M1)" as a matrix source (`neuro/07-MATRIX`, `remap/remap-editor`); review 8 also found "Macro 1 (TONE)" in the remap header, which this build no longer shows.
- Fix: "TONE" everywhere; the matrix shows a macro dot in the colour.

**I9-9. Medium: caption convention broken on the matrix, and sentence captions on FM.**
- What: the matrix header is two full sentences with a capital and full stops; FM's footer "Edit AMP ENV on MOD or PLAY, or pick OP ENV ... here." sits where captions go (`neuro/08-FM`). The convention separates fragment captions from hint sentences.
- Fix: caption "12 of 64 routes"; the hint becomes a hint line below.

**I9-10. Medium: name pairs.** "SUB/NOISE" in the flow, "SUB + NOISE" in the strip; "STRINGS SOUNDBOARD" (two lines, `keys/05-FILTER`) vs SOUNDBOARD vs BOARD. Fix: one name each.

**I9-11. Medium: the OP PITCH graph has no time ruler (I8-28, still open).** Value axis +3 / 0 / -3 st and KEY UP only (`dx7/08-FM-pitch-lfo`); every other envelope has a time axis. Fix: the same log ruler.

**I9-12. Medium: macro warnings are bare triangles.** "HAMMER ⚠" and "DECAY ⚠" on a macro whose target is off (`keys/04-PHYSICAL`, `neuro/05-FILTER`: "TALK ⚠"). The explanation is a tooltip. Fix: put the line "OSC 3 is off: no effect now" in the macro's hover text *and* under the strip when hovered. *(inferred: tooltip content from review 8 notes.)*

**I9-13. Medium: ARP has GATE twice.** The lane "GATE ×" (a multiplier per step) and the knob GATE 50 % (`neuro/09-ARP-SEQ`). Fix: lane "STEP LEN", knob "GATE".

**I9-14. Medium-low: "go to the full page" has five idioms.** The ↗ icon on PLAY's FILTER / ENVELOPE / LFO cards, "EDIT OP ENV ›", "OP PITCH · OP LFO ›", "FM ›", "FILTER ›", "ACOUSTIC KEYS ›". Fix: a trailing "›" button style everywhere, and an icon only for pop-out.

**I9-15. Medium-low: the Airwindows effects use a second vocabulary** (S9-5): DENSITY, HIGHPASS, OUTPUT, raw %, while the built-in twin says AMOUNT, FOLD with units. Fix: friendly names in the library table.

**I9-16. Medium-low: casing of options drifts.** "Room", "Dense", "Classic", "Low Pass" (title case) beside "M/S SIDE", "LOW BAND", "HIGH BAND" (upper) in one rack (`neuro/fx-midside`). The convention covers modules and sources, not options. Fix: options title case, all.

**I9-17. Medium-low: the vector corner combos offer an oscillator that does not exist.** On Init, BOTTOM RIGHT is "OSC 4" while the pad's caption says "OSC 4: none" (`neuro/02-VECTOR`). Fix: list only existing oscillators plus "none".

**I9-18. Low-medium: the LFO RATE knob shows a yellow tick on its ring** (`neuro/06-ENV-LFO`, top of RATE) that is the same colour as the "modulated" dot on filters, but nothing is routed to it. *(inferred: tempo-sync marker?)* Fix: if it is the sync marker, draw it as one on every synced rate.

**I9-19. Low-medium: the per-voice versus shared question is invisible on plain LFOs too.** PLAY's LFO card shows SYNC and RETRIG but not "shared / per voice" (`neuro/01-MAIN`); MOD's caption does. Fix: the caption on PLAY's card.

**I9-20. Low-medium: macros 5-8 can be assigned only by dragging** *(inferred)*; "+ ASSIGN" is a label, not a button. Fix: click it to open the matrix with a new row.

**I9-21. Low: the SEQ chain caption can be 38 characters** ("EUCLID rests PROB SEQ's steps between its hits"); at 75 % it truncates *(inferred, I did not render it)*.

**I9-22. Low: Sample / SF2 / multisample pages were not in any render.** I cannot say whether LOAD, zones and mapping follow the same widget set; this is the one special system I could not judge.

**I9-23. Low: "Not verified" rows above need one interactive pass** (Op Env flat segment drag, KEYS & VELOCITY, legacy LFO names, add menu). Everything not listed as inferred was seen.

**I9-24. Low: A/B (header) and CHAIN 1 / 2 (FX) are two A/B-like systems.** Settled as separate; the letters / numbers differ. No change.

**I9-25. Low: the frame, tempo and rate readouts format differently.** "1/16" (sync), "4.0 Hz", "0.35 Hz", "30 ms": fine; but "Free" as a SEED value, "Auto" as EXCITE POS appear in the same slot as numbers. Fix: a ∅ or "Auto" in the dim colour.

**I9-26. Low: the FM page's "KEY LVL" label is cryptic** (`neuro/fm-dx-keys`): "KEY LVL 0.0 dB/oct". Fix: "KEY SCALE".

## Score integration: 8.3 / 10

One vocabulary 8.5, one widget set 8, convention compliance 8.5, functional soundness 8 (only static evidence). The special systems no longer look foreign; the leftovers are the legacy MSEG, physical modelling's double home, the simulation LFO triggers and name pairs.

### The shortest list to 9.5 on integration
1. Migrate legacy MSEG on load (I9-2); one LFO trigger model (I9-1).
2. One editor for physical modelling (I9-3), one oscillator picker (I9-5), one level name (I9-7).
3. Operator wording on OSC (I9-6), OP PITCH ruler (I9-11), the remaining conventions (I9-4, I9-9, I9-10, I9-16).
4. One interactive pass on the "not verified" items (I9-22, I9-23).

---

## Top 10 findings overall

1. V9-2: a DX7 voice on PLAY shows four of six operators behind a scroll.
2. S9-1: voice mode, glide, bend range and voice count hide behind the grey "VOICES 1 / 32" text.
3. V9-1 / V9-3: PLAY's left column and the FX page end in large dead areas; the empty add-effect slot is full card size.
4. I9-1: chaos and physics LFOs have RETRIG, KEY and TRIGGER plus a self-contradicting caption.
5. I9-2: the legacy MSEG is still its own pool card, chip and white source.
6. V9-5: knob values silently lose their unit ("-30.9" beside "-7.5 dB").
7. V9-4: off WEST and BODY cards keep 280 px of dimmed controls.
8. S9-2: "VOICE" names unison on the OSC page and polyphony in the header.
9. I9-3: physical modelling is edited on two pages in two layouts.
10. V9-6: labels in one row are shrunk to different sizes (UNISON, SUSTAIN, KEY RATE).
