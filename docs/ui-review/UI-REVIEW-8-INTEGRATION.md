# UI review 8: integration audit (pass 3)

Date: 2026-10-04. Branch `claude/project-thread-smvgfk` at 096ff08 (review 7's packages Q1 to Q7 merged). No code was
changed.

**Question asked:** do the special systems (DX7 / FM operators and the Operator Env, physical modelling, vector, Airwindows,
chaos and physics LFOs, MSEG and Steps, the sequencing engines, sample / SF2) feel like one synth, with one vocabulary,
one widget set and one way of doing things? Do the seven review-7 packages look like one product and follow
`docs/UI-CONVENTIONS.md`? And are there bugs a user would hit: wrong values, dead controls, things that don't respond?

**Evidence.**
- The standard renders in `/home/user/shots8/{init,neuro,dx7,keys,small,dx7-small,extras,remap,confirm}/`.
- New renders in `/home/user/shots8/integration-extra/`:
  - `epiano-fm/`: the FM page on "E.PIANO 1 (ROM1A)" (`ILANA_SNAPSHOT_FM=1`): each operator, KEYS & VELOCITY, PITCH &
    LFO, DX7 algorithm 1, WARP FM.
  - `epiano-p1/`: the modulation set on E.Piano 1 (`ILANA_SNAPSHOT_P1=1`): chip tray, matrix, knob card, macro card.
  - `keys-filter/05-FILTER.png`: Felt Hammer Board's FILTER page before the tool touches the patch (I7-1 check).
  - Crops: `crop-dx7-fm-card.png`, `crop-dx7-pitch-graph.png`, `crop-dx7-mod-openv-graph.png`,
    `crop-dx7-mod-filtenv-graph.png`, `crop-keys-osc-subtabs.png`.

Code references are `file:line` on this branch. **(inferred)** marks a claim read from code but not seen on screen.

**Tool artefacts, not reported:** EDITED after 03-OSC; Felt Hammer Board's PHYSICAL shot saying OSC 1 "plays a
wavetable"; STRINGS in later SIGNAL FLOW shots; "ENV +14" / "LFO +15" after the pools are revealed; the P1 set's extra
routings (matrix rows 9 and 10, the "!" repeats); the ROOM warning sign in `dx7/lfo-pool-full.png` (taken after the FX
pass changed the rack).

**Settled, not re-argued:** one FX instance per type; no level-neutral switch to the Operator Env; crossovers and
vocoder knobs not modulatable; no MSEG grid/snap and no STEPS count.

---

## Verdict

**Review 7's packages fixed nearly every bug the last audit found, and the build now looks like one product on the
surface.**

These now work:
- SIGNAL FLOW keeps F1 and F2 on Felt Hammer Board.
- The Operator Env time knobs turn the normal way.
- ROOM (was SPACE) moves the reverb on DX7 voices.
- A DX7 algorithm click keeps the feedback ("FB 6 · 25%").
- Knobs that can't take a source have a dotted track and refuse a drop with a note.
- OP ENV, OP PITCH and OP LFO are real cards in the MOD pools.
- BOUNCE is now RESAMPLE.
- The exciters read "Piano Hammer" / "Bright Hammer".
- Reverb and the other FX families have a BUILT-IN | AIRWINDOWS switch on the card.
- The scope docks like a page.
- The ratio is `×1.00` everywhere.

The type scale, card headers, knob, chip and source colours are shared everywhere.

**What still feels like two synths is the DX7 operator, again in its second layer.** The vocabulary rule in
REVIEW-7-WORKPLAN ("LEVEL is the operator's output in dB; TRIM is the oscillator's trim in %") is followed on PLAY and
OSC, but:
- the FM page shows TRIM in dB;
- the matrix calls TRIM "Level" and calls LEVEL "OP ENV Level";
- the FM page calls OP PITCH + OP LFO "PITCH & LFO";
- three OP ENV editors (FM, MOD, OSC) put the knobs in different orders and draw different time axes;
- the Operator Env's time knobs go dead on any flat segment. They read "1 ms" wherever you turn them, which hits every
  OP PITCH stage on most of the 288 voices.

The other big seam has moved. It used to be "DX7 things live only on FM". Now it is "DX7 things live everywhere":
- OP LFO, OP ENV and OP PITCH take three of the nine visible MOD pool slots on every patch, Init included;
- on a DX7 voice, where they matter, the chip bar folds them away;
- they drop behind "11 MORE" once the pools grow.

The drawn-shape story is still three systems under one name:
- the MSEG pool card;
- the LFO shape "MSEG" (the old Curve);
- ENVELOPE › MSEG.

The legacy physics shapes share their names with the new ones.

None of this needs new engines. Most of it is labels, ordering and one shared editor.

**Score: 7.5 / 10** (review 7: 6.5).

---

## Fixed since review 7 / still open (I7 findings)

| I7 | Item | Status | Note |
|---|---|---|---|
| 1 | SIGNAL FLOW drops F1/F2 | **Fixed** | `keys-filter/05-FILTER.png`: OSC › F1 › F2 › BOARD › OUT; WEST / BODY parked under "OFF". |
| 2 | LEVEL is two parameters | **Partly** | PLAY / OSC say LEVEL (dB) + TRIM (%). The FM KEYS tab shows TRIM in dB, and the matrix calls TRIM "Level" (I8-1, I8-2). |
| 3 | Op Env time knobs reversed | **Fixed / new issue** | Longer is clockwise now. A flat segment reads "1 ms" at any knob position (I8-3). |
| 4 | SPACE dead on DX7 voices | **Fixed** | ROOM → FX Reverb › Mix +35 %, reverb on at 0 % mix (`dx7/10-FX.png`, `epiano-p1/p1-matrix.png`). |
| 5 | Silent non-modulatable knobs | **Fixed** | Dotted track and "can't be modulated" note (`ParamControls.h:1112-1150`, `:1816-1827`). Seen on UNISON, OCTAVES, NOTES, SEED. |
| 6 | Three names for the Operator Env | **Fixed** | No "Operator EG" / "Op EG" / "Op Env" in visible strings. |
| 7 | OP pills were links | **Fixed / new issue** | Real pool cards with editors. New: they sit on every patch (I8-5), and three editors disagree (I8-6). |
| 8 | AMP ENV opens on DX7 voices | **Partly** | MOD has the "unused" badge. PLAY's ENVELOPE card still opens AMP ENV and has no OP ENV tab (I8-18). |
| 9 | Algorithm click drops feedback | **Fixed** | `epiano-fm/fm-dx7-algorithm-1.png` keeps "FB 6 · 25%". |
| 10 | Operator card jumps | **Partly** | Stable between operators. The diagram still moves between the AMP ENV card, the OP ENV card and PITCH & LFO (I8-11). |
| 11 | Tiny nodes in deep stacks | **Fixed** | Algorithm 1 draws full-size nodes. |
| 12 | Raw DX7 numbers | **Mostly** | Notes, dB and %. The curve names "-Lin / +Exp" remain (I8-28). |
| 13 | Two sets of stage names | **Fixed** | ATTACK / DECAY 1 / DECAY 2 / RELEASE, PEAK / MID / SUSTAIN / END everywhere. |
| 14 | KEYS & VELOCITY layout | **Fixed** | `epiano-fm/fm-1-keys.png` is clean. |
| 15 | Hint names a missing control | **Fixed** | SCALE SHIFT and TRANSPOSE both exist. |
| 16 | Source menu grouping | **Fixed (inferred)** | `ModNames.h:79-163`. |
| 17 | "Module › Control" names | **Fixed** | "OP LFO › Pitch Depth", "OP PITCH › Attack". The casing is mixed (I8-12). |
| 18 | OP LFO shape list | **Mostly** | Same names, different order (I8-39). |
| 19 | PLAY strip shows FRAME | **Fixed** | RATIO / LEVEL / TRIM / FINE and a mini OP ENV. |
| 20 | OSC operator card irrelevant rows | **Partly** | The UNISON knob, TABLE, WAVE/3D/SPEC, RESAMPLE / EDIT / LOAD .WAV remain; SEMI is missing (I8-15). |
| 21 | EVOLVE on VECTOR | **Open** | I8-13. |
| 22 | VECTOR X / Y chips in the card | **Partly** | Now in the bar's MORE fold *and* still in the card header (I8-14). |
| 23 | Hidden preset trim | Not re-checked | `PluginEditor.cpp:1963` handles it; the SAVE overlay carries it. |
| 24 | SF2: no LOAD, no zones | **Fixed (inferred)** | LOAD in Sample / Multisample mode (`OscPage.h:504-508`, `:1290`), zone strip (`WaveDisplay.h:774-930`). Not rendered. |
| 25 | BOUNCE three meanings | **Fixed** | RESAMPLE (`OscPage.h:401`, tour). |
| 26 | BODY four labels | **Mostly** | One BODY card. The flow says BOARD; OSC / PHYSICAL say SOUNDBOARD (I8-32). |
| 27 | Exciter list mixed | **Fixed** | `PhysicalView.h:23-30`. The host name still says "Hammer (classic)" (I8-41). |
| 28 | Airwindows a shelf | **Mostly** | BUILT-IN / AIRWINDOWS per family card. "AIRWINDOWS (ALL)" is still a separate type (I8-35). |
| 29 | Step editors' names / rulers | **Partly** | STEPS everywhere. Rulers differ; VELOCITY vs VEL (I8-24). |
| 30 | MSEG own editor | **Open, in a new form** | Three "MSEG"s (I8-4). |
| 31 | Scope floats over PLAY | **Fixed** | Docked page with FLOAT / ×. |
| 32 | Flow contradictions with WEST | **Fixed** | "serial: F1 into WEST", Filter 2 overlay. Small wording pair left (I8-32). |
| 33 | Two ratio glyphs | **Fixed** | `×1.00`. |
| 34 | PITCH graph text over KEY UP | **Mostly** | The text has a backing; the graph still lacks a time axis and a KEY UP label (I8-29). |
| 35 | Two caption grammars | **Mostly** | `captionFragment`. One instruction is still written two ways (I8-30). |
| 36 | Switches in four places | **Mostly** | Card header right. Text "on/off" without a switch remains (I8-21). |
| 37 | Three tab "on" styles | **Mostly** | One dot. It now also means "in use" on VOICE / ACOUSTIC KEYS (I8-22). |
| 38 | BLEND pill | **Fixed** | A knob. |
| 39 | FX knobs ignore family colour | **Partly** | FX cards follow the family; WEST's knobs are orange, not WEST's lime (I8-23). |
| 40 | PLAY vs FILTER type names | **Fixed** | |
| 41 | DX7 ROM truncations | **Fixed** | "E.Piano 1". |
| 42 | PITCH & LFO in the pill row | **Open** | "VOICE PITCH & LFO" at the end of the operator pills (I8-8). |
| 43 | WEST replace combo squeezed | **Fixed** | "Replaces F2" at normal size. |
| 44 | S&H hidden as Legacy | **Fixed** | Random › S&H. Legacy now clashes by name (I8-36). |
| 45 | VIA says "Aux: none" | **Fixed** | An empty "+" cell. |

Tally: 25 fixed (2 inferred), 9 mostly, 8 partly, 3 open or reopened, 1 not re-checked.

---

## Findings, worst first

Severity:
- **High:** wrong information, a dead control, or one thing named as another.
- **Medium:** friction, duplication, a parallel system, inconsistency a user meets.
- **Low:** polish.

### I8-1. High: TRIM reads 0.0 dB on the FM page and 50 % on PLAY and OSC; it is the same knob
- **What's wrong.**
  - On a DX7 voice, `oscN_level` shows as **TRIM 50 %** on PLAY (`dx7/01-MAIN.png`) and on OSC (`dx7/03-OSC.png`).
  - On the FM page's KEYS & VELOCITY tab it shows as **TRIM 0.0 dB** (`epiano-fm/fm-1-keys.png`).
  - `FmInputPages.h:805-822` (`setUpLevelText`) rewrites the text to dB whenever the oscillator plays the Operator Env.
    `OscPage.h:335` and `MainPage.h:78` don't.
  - The workplan's rule is "TRIM (%) on every page".
- **Benchmark.** Vital and Serum show one unit per parameter everywhere, matrix included.
- **Fix.** Pick one unit. Keep % (the rule) and drop the dB override, or move the dB text into the shared `ParamKnob`
  formatter so all three pages agree. Add a uitest that reads the TRIM text on PLAY, OSC and FM for E.Piano 1.

### I8-2. High: the matrix names the TRIM knob "Level" and the LEVEL knob "OP ENV Level"
- **What's wrong.**
  - E.Piano 1's macros BARK route to **"OSC 2 › Level"** (`epiano-p1/p1-matrix.png`). On every page that knob is labelled
    **TRIM**.
  - The knob the pages call **LEVEL** (`_eg_out`) is "OSC 2 › OP ENV Level" in the matrix and the host
    (`ModNames.h:294`, `:405`; `OperatorEgParams.h:53`).
  - A user who reads "Level" in the matrix will look for the LEVEL knob, the wrong one.
  - The same knob position on PLAY changes meaning with the ENVELOPE choice. The second knob, LEVEL, is `_level` (%)
    under AMP ENV and `_eg_out` (dB) under OP ENV (`MainPage.h:78`).
- **Benchmark.** Serum 2's matrix destination text is the knob's label.
- **Fix.** Name `oscN_level` "OSC n › Trim" in `ModNames` while the oscillator plays the Operator Env. Simpler: always
  call it "Level / Trim" in the matrix, and call `_eg_out` "OSC n › Level (OP ENV)". The knob card's title should match.

### I8-3. High: the Operator Env time knobs go dead on any flat segment
- **What's wrong.**
  - Each time knob shows the segment's *duration*. On a DX7 the duration depends on the level gap
    (`OperatorEnvDisplay.h:140-205`).
  - When the levels at both ends are equal, every rate gives "1 ms" (or 0). Examples:
    - E.Piano 1's RELEASE: SUSTAIN and END are both −inf dB. It reads 1 ms with its pointer at about 11 o'clock, while
      ATTACK reads 1 ms at about 7 o'clock (`crop-dx7-fm-card.png`).
    - All four OP PITCH times on any voice without pitch movement. ATTACK / DECAY 1 / DECAY 2 / RELEASE all read "1 ms"
      at four different angles (`dx7/08-FM-pitch-lfo.png`, `neuro/08-FM-pitch-lfo.png`).
  - Turning the knob changes the stored rate (it matters as soon as a level moves) but the readout never changes.
    Typing "2 s" binary-searches to an arbitrary rate (`rateForSeconds`, `:201-216`).
  - The user sees a dead control.
- **Benchmark.** Dexed shows the rate, so the knob always reads what it holds. Vital's envelope times are times of a
  full sweep.
- **Fix.** For a flat segment, show the time a full-scale sweep at that rate would take, greyed, with "(flat)" in the
  tooltip. Or show "flat · rate 63". Never show a value that ignores the knob.

### I8-4. Medium-High: "MSEG" names three different things
- **What's wrong.** One word, three systems:
  1. The **MSEG** pool card: the patch's four-point MSEG, its own white editor with only LOOP and RATE
     (`init/lfo-mseg.png`; `EnvLfoPages.h:984-997`).
  2. **SHAPE › MSEG**, which every LFO has: the old Curve shape, renamed (`LfoShapeMenu.h:40-41`). The MSEG card's own
     hint points there ("Any LFO can draw its own: SHAPE › MSEG").
  3. **ENVELOPE › MSEG**, an oscillator's amp envelope played by the pool MSEG (`ParameterLayout.cpp:749`).

  A CLOCKED S&H card shares the same panel. Review 7's plan asked for "one drawn-shape system: drawable, Steps, MSEG all
  as LFO shape modes". The shape mode exists, but the old module stayed beside it under the same name, with a different
  editor.
- **Benchmark.** Vital: every LFO *is* an MSEG; there is one editor. Serum 2: LFOs are MSEGs; there are no extra MSEG
  modules.
- **Fix.**
  - Rename the pool card to what it is ("ENV MSEG" or "4-POINT"), or
  - convert it on load to an LFO with SHAPE › MSEG plus LOOP off, and hide the card for new patches.
  - Give LOOP / ONE-SHOT to the LFO SHAPE › MSEG mode, so one editor covers both.

### I8-5. Medium-High: DX7-only cards take MOD pool space on every patch, and hide where they matter
- **What's wrong.**
  - On Init and Neuro Wobble, the LFO row shows LFO 1-3 + **OP LFO (unused)**. The envelope row shows AMP / FILT / FILT 2
    + **OP ENV (unused)** + **OP PITCH (unused)** (`init/06-ENV-LFO.png`).
  - That is three of nine visible slots spent on an engine the patch doesn't use. The MSEG card is pushed out of view.
  - The unused OP ENV editor stays fully editable with no oscillator named (I8-40).
  - On a DX7 voice, where these cards matter, they drop behind "11 MORE" once the pools grow (`dx7/lfo-pool-full.png`,
    `dx7/env-pool-full.png`).
- **Benchmark.** Vital and Serum show a module only when the patch has it (Vital's LFO list grows from use).
- **Fix.**
  - Show the OP cards only while an oscillator plays the Operator Env. Otherwise offer them under "+".
  - Pin them first in their rows on DX7 voices, as AMP ENV is pinned.

### I8-6. Medium-High: three Operator Env editors, three layouts
- **What's wrong.**
  - **FM page** (`dx7/08-FM.png`): LEVEL in the top row; stages ATTACK … END, then KEY RATE last; scaling on a KEYS &
    VELOCITY tab; operator picked with coloured "● 1"… pills.
  - **MOD › OP ENV** (`dx7/06-ENV-LFO.png`): ATTACK, DECAY 1, DECAY 2, RELEASE, **KEY RATE**, PEAK … END, **LEVEL** last;
    no scaling; operator picked with "OSC 1"… text pills (`OperatorPoolCards.h:256` against `FmInputPages.h:212-214`).
  - **OSC page** (`dx7/03-OSC.png`): LEVEL and the graph only, and a caption saying it plays "(FM page)".
  - Each graph draws its own time axis (I8-7).
- **Benchmark.** Vital: one envelope editor component, the same wherever it appears.
- **Fix.** One `OperatorEnvEditor` component (graph, knobs in one fixed order, scaling as a tab) used by FM and MOD. OSC
  links to it. Fix the order to the FM one; KEY RATE belongs with the scaling.

### I8-7. Medium: the OP ENV graph's time axis is unlike every other envelope graph
- **What's wrong.**
  - AMP / FILT / ENV n graphs put ticks *below* the plot, with KEY UP as an axis label and "+100 ms" after it
    (`crop-dx7-mod-filtenv-graph.png`).
  - The OP ENV graph puts "100 ms / 1.0 s / 10 s" *inside* the plot, KEY UP inside at the top, and no time after key up
    (`crop-dx7-mod-openv-graph.png`).
  - The OP PITCH graph has no time axis and no KEY UP label at all (`crop-dx7-pitch-graph.png`).
  - Review 7's Q3 note was "time ticks outside the plot; one time scale".
- **Fix.** Draw the OP graphs with the shared envelope ruler (`EnvelopeDisplay`'s axis code).

### I8-8. Medium: the FM page calls OP PITCH + OP LFO "PITCH & LFO"
- **What's wrong.**
  - The vocabulary rule is **OP PITCH** and **OP LFO**. The MOD pool, chips and matrix follow it.
  - The FM page's button is "VOICE PITCH & LFO", and the panel title is "PITCH & LFO" (`FmInputPages.h:471`, `:1343`).
  - The panel mixes both modules under one header with no sub-heads: SHAPE … TRANSPOSE (LFO and tuning), then ATTACK …
    END (the pitch envelope).
  - The button still sits in the operator pill row (I7-42).
- **Fix.** Two sub-headed sections, OP LFO and OP PITCH, in the pool colours. Call the button "OP PITCH · OP LFO" and
  move it to the card header's right, apart from the operator pills.

### I8-9. Medium: on DX7 voices the chip bar folds the sources the voice uses
- **What's wrong.**
  - On E.Piano 1 the bar shows LFO 1, LFO 2, LFO 3 (unrouted). It folds OP LFO into "LFO +2" and OP PITCH into "ENV +1"
    (`dx7/01-MAIN.png`).
  - The fold order puts "Op LFO, Op Pitch" first to go (`PluginEditor.cpp:858-866`).
  - On this voice, the OP LFO is the vibrato the wheel drives.
- **Benchmark.** Vital's bar shows the modulators the patch has.
- **Fix.** While an oscillator plays the Operator Env, rank OP LFO / OP PITCH with the routed chips. Fold unrouted
  LFO 2/3 and FILT 2 ENV first.

### I8-10. Medium: "pick an oscillator" is drawn four ways
- **What's wrong.**
  - FM: "● 1 … ● 6" coloured pills.
  - MOD › OP ENV: "OSC 1 … OSC 6" outline pills.
  - OSC: `StateTabs` "● OSC 1 CARRIER".
  - PHYSICAL: centred pill buttons "OSC 1 / OSC 2 / OSC 3" (`init/physical-page.png`).

  All four select the same thing.
- **Fix.** One widget (`StateTabs`, compact variant) for all four.

### I8-11. Medium: the FM diagram jumps when the operator card changes kind
- **What's wrong.**
  - On Neuro Wobble the AMP ENV operator card starts at y≈680 and the diagram is tall (`neuro/08-FM.png`).
  - Opening PITCH & LFO makes the card start at y≈585 and the diagram shrinks; the nodes move up 48 px
    (`neuro/08-FM-pitch-lfo.png`).
  - An OP ENV operator uses the taller card too. Switching ENVELOPE or tabs moves everything above.
- **Fix.** Size the card for the tallest content and keep it fixed (the I7-10 rule, applied across kinds, not only
  between operators).

### I8-12. Medium: the matrix breaks the casing rule, in two casings at once
- **What's wrong.**
  - `UI-CONVENTIONS.md`: module and source names are upper case "in labels, chips, tabs and combos".
  - The matrix source combo reads "Mod Wheel", "Pressure", "Macro 1 (BARK)", while the chips say MOD WHEEL and
    PRESSURE.
  - The destinations mix "Filter 1 › Cutoff", "FX Reverb › Mix" and "Voice › Analog Drift" with "OSC 2 › Level" and
    "OP LFO › Pitch Depth" (`epiano-p1/p1-matrix.png`).
- **Fix.** Upper-case the module part everywhere ("FILTER 1 › Cutoff", "MOD WHEEL"), from `ModNames.h`.

### I8-13. Medium: EVOLVE is still a separate machine on PLAY › VECTOR (I7-21)
- **What's wrong.**
  - EVOLVE has its own card on the VECTOR tab with a one-off mini-slider per macro, an "EVOLVE" knob that repeats the
    card title, and FREEZE where every other card has its switch (`init/vector-page.png`).
  - It moves macros, which live in the bottom strip, not the vector.
- **Benchmark.** Serum 2 / Vital put per-macro behaviour on the macro itself.
- **Fix.** Put EVOLVE on the macro card (the knob card the strip already opens): an EVOLVE amount + RATE per macro, and a
  global FREEZE in the macro strip's menu.

### I8-14. Medium: VECTOR X / VECTOR Y chips exist twice
- **What's wrong.** They are draggable chips in the VECTOR card header (`init/vector-page.png`). They are also in the
  bar's MORE fold ("MORE +2"). A source should have one home.
- **Fix.** Drop the header chips. Show "drag X / Y from the bar" in the caption, or a small "→ bar" link.

### I8-15. Medium: an FM operator still wears wavetable clothes
- **What's wrong.**
  - On PLAY, each DX7 operator row shows two combos, "Wavetable" and "Sine" (`dx7/01-MAIN.png`).
  - On OSC, the operator card shows the "Wavetable" mode combo, RESAMPLE / EDIT / LOAD .WAV, WAVE / 3D / SPEC, a TABLE
    combo and a UNISON knob (`dx7/03-OSC.png`).
  - OSC's PITCH & LEVEL row has RATIO and FINE but no SEMI, which the FM card has.
- **Benchmark.** Vital: an oscillator in FM mode still shows its wave, but nothing claims it is something else.
- **Fix.**
  - For an operator, replace the PLAY combos with "Sine · ×14.00", or the TABLE combo alone.
  - On OSC, hide EDIT / LOAD .WAV / 3D / SPEC and UNISON while the oscillator is an operator.
  - Add SEMI.

### I8-16. Medium: the physics / chaos LFO says "free-running" while it restarts on each note
- **What's wrong.**
  - The LFO header caption depends only on RETRIG and KEY (`EnvLfoPages.h:1007-1011`). A Bounce or Lorenz LFO set to
    TRIGGER = Note reads "free-running, shared by all voices" (`init/lfo-sim-bounce.png`, `init/lfo-sim-lorenz.png`).
  - The panel has two restart controls, TRIGGER (Note / Free / Beat / Generative) and RETRIG, with no word on which wins.
- **Fix.** Build the caption from TRIGGER for sim shapes ("drops on each note", "runs free", "on the beat"). Hide RETRIG
  where TRIGGER covers it.

### I8-17. Medium: the LFO panel moves its switches when the shape changes, and leaves dead space
- **What's wrong.**
  - Basic shapes: SYNC / RETRIG / KEY top right, RATE / START / SMOOTH floating in the middle of an empty panel
    (`init/06-ENV-LFO.png`).
  - Sims: SYNC / RETRIG / KEY under TRIGGER on the left, RATE top right (`init/lfo-sim-lorenz.png`).
  - MSEG: one switch and one knob in a full-height panel (`init/lfo-mseg.png`).
  - Flipping SHAPE moves every control.
- **Fix.** One grid: the left column always SHAPE / TRIGGER / switches; the right always RATE then the shape's own knobs.

### I8-18. Medium: PLAY's ENVELOPE card on a DX7 voice still opens AMP ENV
- **What's wrong.**
  - The card's tabs are AMP ENV / FILT ENV / FILT 2 ENV. AMP ENV is selected, its graph bright, with a small "Not used:
    the OSCs play their OP ENV" and EDIT OP ENV, which jumps to FM (`dx7/01-MAIN.png`; `MainPage.h:849`).
  - The envelope that shapes the sound has no tab on PLAY.
- **Fix.** On Operator Env voices, show an OP ENV tab first (the selected operator's or a 6-operator overlay) and dim
  AMP ENV's tab.

### I8-19. Low-Medium: the same FM routing is described in two vocabularies
- **What's wrong.**
  - A wavetable oscillator in an FM route is "FM FROM 2 / FM INTO 1" (`neuro/01-MAIN.png`).
  - Switch its TUNING to Ratio and the same routing reads "CARRIER", "MOD › 1" or "OUT, MOD › 2" (`OscPage.h:77-101`).
  - The FM diagram calls both OUT / MOD.
- **Fix.** One phrasing for both: "OUT", "MOD › 1", "OUT, MOD › 1", whether or not the oscillator is an operator.

### I8-20. Low-Medium: on/off said in text without a switch (UI-CONVENTIONS, rule 4)
- **What's wrong.**
  - PHYSICAL: "BODY off" and "SOUNDBOARD on, Dense" (`init/physical-page.png`).
  - SIGNAL FLOW: "OFF  WEST  BODY".
  - OSC tabs: "OSC 2 OFF" (switched by right-click only).
  - The GENERATE folded summary: "SNAP TO KEY off · STRUM Up · SPRAY off" (`neuro/gen-clip-expanded.png`).
  - FM matrix: "OSC 3: OFF".
- **Fix.** Either put the switch beside the word (the PHYSICAL summary can carry the real toggles), or drop the word and
  dim the item.

### I8-21. Low-Medium: the on dot means "in use" on some tabs
- **What's wrong.**
  - VOICE and ACOUSTIC KEYS light their dot from a heuristic (any value above 0), not from a switch
    (`OscPage.h:855-866`).
  - SUB + NOISE lights from `subosc_on || noise_level > 0`, so its dot can be lit while its switch is off
    (inferred).
  - The convention defines the dot as the switch state.
- **Fix.** Light only from a switch. VOICE has none, so drop its dot.

### I8-22. Low-Medium: WEST's knobs ignore WEST's colour
- **What's wrong.** WEST is lime in its header dot, flow block and response line. Its knobs draw in the accent orange
  (`init/filter-west.png`). FX cards and the filters use their family colour.
- **Fix.** Pass WEST's colour to its knobs.

### I8-23. Low: the four SEQ engines lay out the same controls differently
- **What's wrong.**
  - Step rulers: ARP and PROB SEQ number every step above the grid; EUCLID shows 1 / 5 / 9 / 13 below; CLIP shows
    bar.beat.
  - Velocity lane: "VELOCITY" (ARP) vs "VEL" (CLIP).
  - GATE and STEPS sit in different columns per engine (`neuro/09-ARP-SEQ.png`, `init/gen-euclid.png`,
    `init/gen-probseq.png`).
- **Fix.** One lane header style and ruler; a fixed control row (RATE, STEPS, GATE, then the engine's own).

### I8-24. Low: ARP's lanes look live while ARP is off
- **What's wrong.** Full-colour lanes with only a header caption "ARP is off" (`neuro/09-ARP-SEQ.png`). Q7 asked for an
  off overlay that doesn't hide the data.
- **Fix.** Draw the lanes at 50 % alpha with a small "OFF" badge, as off oscillators are dimmed.

### I8-25. Low: squeezed text
- **What's wrong.**
  - "STRING COUPLING" is horizontally compressed on OSC and PHYSICAL (`keys/03-OSC.png`, `init/physical-page.png`).
  - "KEY RATE" is compressed on MOD's OP ENV (`init/06-ENV-LFO.png`).
  - The EXCITE combo uses a smaller font on PHYSICAL ("Piano Hammer") than on OSC.
- **Fix.** "COUPLING" (the section already says STRING); give the OP ENV knobs the env pool's column width.

### I8-26. Low: physics units
- **What's wrong.** "9.81 m/s2" and "0.00 /s" (`LfoSim.h:174-177`, `init/lfo-sim-bounce.png`).
- **Fix.** "m/s²" and a named unit, e.g. "0.00 drag" or "per s".

### I8-27. Low: KEYS & VELOCITY curve names are DX7 jargon
- **What's wrong.** LOW / HIGH CURVE read "-Lin", "+Exp" (`epiano-fm/fm-1-keys.png`). Q1 asked for curve names that fit.
- **Fix.** "Down, linear", "Up, curved", with a small curve icon in the combo.

### I8-28. Low: the OP PITCH graph has no ruler
- **What's wrong.** No time axis, no KEY UP label, ±st labels inside the plot. The empty-state text sits on a box over
  the dashed key-up line. A stray tick floats at the bottom (`crop-dx7-pitch-graph.png`).
- **Fix.** The shared ruler (I8-7).

### I8-29. Low: one instruction, two grammars
- **What's wrong.**
  - The MOD envelope header caption says "drag the graph or the knobs; the dot on a segment sets its curve".
  - The FM card's hint for the same gesture is a sentence: "Drag the graph's points: across for time, up or down for
    level."
  - PLAY's "Not used: the OSCs play their OP ENV" is a hint without a full stop.
- **Fix.** Use one hint sentence per editor, in the same place.

### I8-30. Low: the OSC operator card says "(FM page)" while it edits the envelope itself
- **What's wrong.** The caption reads "plays its Operator Env (FM page)" above an editable OP ENV graph and LEVEL
  (`dx7/03-OSC.png`; `OscPage.h:127-128`). It is also editable on MOD.
- **Fix.** "plays its OP ENV".

### I8-31. Low: small name pairs
- **What's wrong.**
  - The flow says "SUB+N" and "BOARD"; elsewhere "SUB + NOISE" and "SOUNDBOARD".
  - The WEST combo says "Replaces F2"; the overlay says "Replace Filter 2".
  - PLAY says "+ ADD OSC 4"; OSC says "+ ADD OSC".
- **Fix.** One spelling each.

### I8-32. Low: grey header dots read as "off"
- **What's wrong.** OUTPUT, SIGNAL FLOW and MSEG have grey dots in their headers (`dx7/10-FX.png`,
  `keys-filter/05-FILTER.png`). Next to coloured family dots and the on-dot convention, grey looks like "switched off".
- **Fix.** No dot for cards without a family colour, or the accent.

### I8-33. Low: "AIRWINDOWS (ALL)" stays beside the family switches
- **What's wrong.**
  - Every family card has BUILT-IN | AIRWINDOWS. The all-in-one type (`init/fx-30.png`) is a second road to the same
    algorithms.
  - Switching a reverb card between models moves the ALGORITHM column (x≈417 built-in, x≈380 Airwindows;
    `dx7/10-FX.png`, `init/fx-34.png`).
- **Fix.** Hide type 30 from ADD EFFECT for new patches (keep it for old ones). Fix the ALGORITHM column position.

### I8-34. Low: Airwindows knobs in raw %
- **What's wrong.** INPUT / SOFTEN / HEAD BUMP … all "50 %" (`init/fx-30.png`), BIGNESS "100 %". Known since review 6.
  It is the last place where knobs carry no unit.
- **Fix.** Per-algorithm text where the port knows it (dB, ms); else keep %, but say so in the tooltip.

### I8-35. Low: dash and parenthetical style
- **What's wrong.** "VECTOR OFF - switch on …" and "PREVIEW - Physical oscillators only" use a spaced hyphen. Corner
  labels mix "(off)" and "(none)" (`init/02-VECTOR.png`).
- **Fix.** "·" or an en dash; "OSC 4: none".

### I8-36. Low: legacy LFO shapes share names with the new ones
- **What's wrong.** The Legacy submenu holds Chaos, Bounce, Pendulum, Spring and Friction. They print the same names as
  the Physics / Chaos shapes, because only S&H, Smooth Random and Drunk get a suffix (`LfoShapeMenu.h:40-67`). On an old
  patch the menu shows two "Bounce" entries, and the SHAPE combo can't tell them apart (inferred).
- **Fix.** Add "(classic)" to every legacy name.

### I8-37. Low: PLAY's oscillator thumbnail means two things
- **What's wrong.** For an operator it draws the OP ENV shape; for any other oscillator, the waveform
  (`dx7/01-MAIN.png` vs `neuro/01-MAIN.png`). There is no label, and a decaying envelope looks like a saw.
- **Fix.** A tiny "OP ENV" tag in the thumbnail's corner, or draw the sine with the envelope as a fill behind it.

### I8-38. Low: OP LFO shape order differs from the LFO list
- **What's wrong.** OP LFO is Triangle, Saw Down, Saw Up, Square, Sine, S&H (DX7 order, `OperatorEgParams.h:31`); the
  LFO list is Sine, Triangle, Saw Up, Saw Down, Square, S&H.
- **Fix.** Display in the LFO order (display-only remap; the stored index stays).

### I8-39. Low: on a patch without operators, the OP ENV editor edits an unnamed oscillator
- **What's wrong.** `init/06-ENV-LFO.png` shows a live OP ENV editor with no OSC pills, under "unused: no oscillator
  plays the Operator Env". It is not clear whose envelope the knobs change (OSC 1, inferred).
- **Fix.** Grey the knobs and offer "Use on OSC 1" (sets ENVELOPE to OP ENV), or hide the editor (see I8-5).

### I8-40. Low: host parameter names lag the UI
- **What's wrong.** The host and automation lanes still read "Hammer (classic)" (`ParameterLayout.cpp:230`, `:371`,
  `:699`), and "Osc1 OP ENV Attack" against the UI's "OSC 1". The UI says "Bright Hammer".
- **Fix.** Parameter names can change without renumbering IDs; align them.

---

## Do the seven packages look like one product?

**Yes, at a glance, and more than in review 7.**

What the packages share:
- the card header and the caption fragment rule;
- the dotted "not modulatable" track;
- `×1.00`;
- the docked scope;
- the BUILT-IN | AIRWINDOWS segmented switch;
- the SEQ tabs with their switches;
- the "unused" badge.

Q6 put the switch and caption rules into shared widgets, so most pages follow them for free.

**Where the parallel build still shows:**
- **Q1 vs Q2 vs Q3 on the operator:** TRIM in dB (Q1), "Level" in the matrix (Q2), a second OP ENV editor with its own
  order and axis (Q3). Findings I8-1, 2, 6 and 7. Each package was right by its own file; nobody owned "the operator".
- **Q3 vs Q4:** four oscillator pickers (I8-10).
- **Q3 vs Q7:** step lanes and rulers drawn per engine (I8-23).
- **Q4 vs Q5:** WEST's knob colour (I8-22).
- **Q2 vs Q6 rule:** matrix casing (I8-12).

A fourth cycle should give one owner to each cross-cutting object (the operator, the envelope editor, the oscillator
picker, the step lane) rather than to each page.

## What feels native now (keep it)

- The dotted track and refused-drop note for non-modulatable knobs.
- OP ENV / OP PITCH / OP LFO as pool cards, with "6 operators" and "unused" badges.
- AMP ENV / FILT ENV badges naming their target ("● Amp", "● Filter 1").
- BUILT-IN | AIRWINDOWS on each FX family card.
- RESAMPLE, "Piano Hammer", the PHYSICAL summary links to FILTER and ACOUSTIC KEYS.
- SIGNAL FLOW with WEST in Filter 2's place and the overlay explaining it.
- The physics LFO previews (the ball, A / B pills).
- The SEQ chain readout with one sentence on how the engines combine.

## The shortest path to 9.5

1. **One operator vocabulary end to end** (I8-1, 2, 8, 12). Unit, matrix name and host name match the label on every
   page.
2. **One Operator Env editor** (I8-6, 7, 28), and **no dead time knobs** (I8-3).
3. **DX7 cards only where an operator uses them, pinned first there**, in the pools and the chip bar (I8-5, 9, 18, 39).
4. **One drawn-shape system** (I8-4, 36): the pool MSEG folded into LFO SHAPE › MSEG.
5. **One oscillator picker and one step-lane component** (I8-10, 23), a fixed FM card height (I8-11), and a fixed LFO
   panel grid (I8-17).
6. **EVOLVE onto the macro card** (I8-13) and **operator cards without wavetable controls** (I8-15).

Then the low items as one polish batch, each with a uitest (text read-back on PLAY / OSC / FM / matrix for one DX7 voice
covers most of 1 and 2).

## Score

**7.5 / 10 for integration** (review 7: 6.5).
- **Up** because every confirmed bug from review 7 is fixed (flow, ROOM, feedback, reversed knobs). Modulatability is
  visible, and the special systems now share the widget set.
- **Not higher** because:
  - the DX7 operator, the synth's most visible special system, still shows one knob in two units and two names;
  - its envelope has three editors;
  - a common edit (a flat segment) leaves a knob that reads the same at every angle;
  - MSEG is three things.
- **Against Vital or Serum 2 as a commercial product**, these are the seams a reviewer finds in the first ten minutes
  with a DX7 preset. Fixing points 1 to 4 above would put integration at about 9; points 5 and 6 and the polish batch
  would reach 9.5.
