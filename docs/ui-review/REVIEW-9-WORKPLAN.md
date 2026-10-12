# Review 9 (pass 4): work plan

ilana's goal: keep running review and fix passes until a review scores 9.5 against Vital or Serum 2. Review 9 (`UI-REVIEW-9.md`, one reviewer, three angles) scored 8.0 vs Vital, 8.0 vs Serum 2, 8.3 integration, with 83 findings (V9-1..31, S9-1..26, I9-1..26). Each section ends with "the shortest list to 9.5": those items come first. ilana asked for fewer agents (three packages this pass).

Rules: as before (REVIEW-8-WORKPLAN.md, docs/UI-CONVENTIONS.md, the drawFitted text rule and its 60-page uitest). Parameter IDs and choice indices append only; old presets render the same (fingerprints, 0 changed); settled decisions in docs/REVIEW-PLAN.md stay (header A/B and history, 8 macros, Init sound, one FX instance per type, CHAIN 1/2 separate from A/B); string literals under 16 KB; a uitest for every fix, a geometric one for every layout fix.

## Packages

**T1 Layout, space and type** (MainPage.h, OscPage.h layout, FilterVectorPhysicalPages.h layout, FilterWidgets.h, FilterDisplay.h, FxPage.h, FxWidgets.h, FM page layout in FmInputPages.h, IlanaLookAndFeel.h value formatting, dialogs, output meter):
V9-1..15, 17, 18, 19, 21..26, 28, 29; S9-12, 14, 25, 26; I9-17, 18.
Notes: no page may end in a large dead area at 100 % or 75 % (add a uitest that measures the largest empty rectangle per page and fails over a threshold); all six DX7 operators visible on PLAY without scrolling (compact operator strips when more than four oscillators play); a value never loses its unit (shrink the font to the floor first, then abbreviate the unit, never drop it); one font size per row for labels and combos; off modules collapse to their header (WEST, BODY, FX cards, SUB/NOISE halves) with the switch to bring them back; one "add" component used everywhere.

**T2 Workflow: voice, SEQ, matrix, browser, macros** (header and voice menu in PluginEditor.cpp, a VOICE section on PLAY or the header, SeqPage.h, ClipEditor.h, GenerativeWidgets.h, MatrixPage.h, MatrixWidgets.h, RemapEditor.h, PresetPanel.h, SavePresetOverlay.h, MacroStrip.h, TutorialOverlay.h):
V9-27, 30, 31; S9-1..4, 6..11, 13, 15, 16, 19..24; I9-8, 12, 13, 20, 21, 24.
Notes: voice mode, voices, bend range and glide visible in one place without a hidden menu (S9-1), and the OSC page's unison tab renamed so "VOICE" means one thing (S9-2); selecting a SEQ tab whose engine is off and drawing in it turns it on (S9-3); compact matrix rows and no empty REMAP placeholder; MIDI export from the clip editor (drag-out if feasible, else EXPORT MIDI to a file); "+ ASSIGN" a real button.

**T3 Integration: one way of doing things** (EnvLfoPages.h, LfoDisplay.h, ModulePool.h, the MSEG module and its loader, physical oscillator UI (OscPage.h physical card, PHYSICAL page parts of FilterVectorPhysicalPages.h, PhysicalView.h), OscPicker, operator naming in OscPage.h, FxLibrary.h / Airwindows labels, ModSourceChip.h, ParamInfo.h names):
V9-16, 20; S9-5, 17, 18; I9-1..7, 9, 10, 11, 14, 15, 16, 19, 22, 23, 25, 26.
Notes: one LFO trigger model (one control, caption from it, same for plain and simulation LFOs); legacy MSEG: migrate old patches into an LFO's MSEG shape on load only if the render is bit-identical (fingerprints 0 changed), otherwise keep the module but show it only as an LFO shape entry, never a separate card / white chip; physical modelling edited in one place (one component, the other page links to it); one oscillator picker everywhere; Airwindows knobs in the built-in vocabulary with units where derivable; the "not verified" items (I9-22, I9-23) checked by driving the UI in the snapshot tool (sample/SF2 pages rendered, Op Env flat-segment drag, KEYS & VELOCITY) and fixed if wrong.

Ownership overlaps: T1 and T3 both touch OscPage.h and FilterVectorPhysicalPages.h (T1: layout, empty space, collapsing off modules; T3: the physical editor's single home, operator wording, picker). T1 and T2 both touch PluginEditor.cpp (T1: meter; T2: header voice controls). Keep edits to the other's regions minimal.

**After T1-T3 (by the thread):** merge, full gate, fresh renders, review 10 (one reviewer), product report.
