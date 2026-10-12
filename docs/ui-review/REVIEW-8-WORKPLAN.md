# Review 8 (pass 3): work plan

ilana (2026-10-04): "once the tasks are done. run 3 review and debug / polish / ui-ux passes, till the review score is at least a 9.5 on one of the competitors". Review 8 scored the build 7.5 vs Vital, 7.8 vs Serum 2, 7.5 integration. Each review ends with "the shortest list to 9.5": read it, those items come first in every package.

Sources (every finding is in scope; IDs are the finding numbers): `UI-REVIEW-8-VITAL.md` (V8-1..40), `UI-REVIEW-8-SERUM2.md` (S8-1..41), `UI-REVIEW-8-INTEGRATION.md` (I8-1..40). Also the review 7 rows those files mark partly fixed / still open in your area. Renders: /home/user/shots8/.

Rules: as before (REVIEW-7-WORKPLAN.md, docs/UI-CONVENTIONS.md). Parameter IDs and choice indices append only; old presets render the same (fingerprints); settled decisions in docs/REVIEW-PLAN.md stay (header A/B and history, 8 macros, Init sound, one FX instance per type); string literals under 16 KB; add `--uitest` checks for everything you fix, and for every layout fix a geometric check (no overlap / no clipping / no squeezed text) so it stays fixed.

Shared decisions for this pass (every package follows them):
- **DX7 parts only where used.** OP ENV / OP PITCH / OP LFO cards, chips and panels appear only when at least one oscillator plays the Operator Env (or the patch is a DX7 voice), and then they come first in their pools and in the chip bar. On other patches they are absent (not greyed).
- **One operator vocabulary.** TRIM is the oscillator level, always in % (never dB), and the matrix calls it "OSC n › Trim". LEVEL is the operator output level in dB, matrix "OSC n › OP ENV Level". The DX7 pitch envelope is OP PITCH and the DX7 LFO is OP LFO on every page, including the FM page's panel and its button (no "PITCH & LFO").
- **One editor per thing.** The Operator Env, OP PITCH and OP LFO each have one editor component, used wherever they appear, with one knob order that follows the graph left to right. Other pages link to it rather than repeating it.
- **One MSEG.** MSEG means one drawn-shape LFO mode. The separate patch-level MSEG module stays in the engine (append-only params) but is reached as an LFO shape, not a separate pool card / chip / panel; old patches that use it keep sounding the same.
- **Stable chip bar.** Fixed order from the pools' order; folding depends only on width (never on routing or preset), and sources the patch actually plays (e.g. OP ENV on DX7 voices) never fold before unused ones.
- **Casing.** Per docs/UI-CONVENTIONS.md; destination names via one "MODULE › Control" formatter.

## Packages

**R1 Operator editors and names** (FmInputPages.h operator card, KEYS & VELOCITY and the OP PITCH / OP LFO panel; OperatorPoolCards.h; OperatorEnvDisplay.h; operator names in ModNames.h, ParamInfo.h and host parameter names; the operator parts of MainPage.h / OscPage.h):
I8-1, 2, 3, 6, 7, 8, 10, 15, 27, 28, 30, 38, 39, 40; S8-3, 4, 9, 10, 19; V8-4, 5, 8, 11, 16, 21, 32.

**R2 FM page layout and diagram** (FmDiagram.h, the FM page layout and FM matrix in FmInputPages.h, noise/sub controls):
I8-11, 19; S8-13, 21, 36; V8-6 (FM part), 7, 14, 15, 35.

**R3 MOD pools, LFO, MSEG** (EnvLfoPages.h, ModulePool.h, LfoDisplay.h, EnvelopeDisplay.h, EnvThumbs.h, LfoThumbs.h, PLAY's ENVELOPE card selection):
I8-4, 5, 16, 17, 18, 26, 36; S8-1, 5, 7, 16, 17, 18, 31; V8-1, 2, 6 (LFO / MSEG panes), 18, 28, 29, 40.

**R4 Modulation** (chip bar in PluginEditor.cpp, ParamControls.h drag feedback, MatrixPage.h, MatrixWidgets.h, ModNames.h formatter, InfoStrip.h, MacroStrip.h incl. EVOLVE):
I8-9, 12, 13, 14; S8-2, 11, 20, 25, 26, 27, 35, 38 (EVOLVE part); V8-3, 10, 13, 20, 26 (EVOLVE part), 31, 37.
EVOLVE moves onto the macro card (per-macro evolve inside the macro's hover card / panel), and the VECTOR page loses its EVOLVE pane.

**R5 PLAY / OSC / PHYSICAL / VECTOR / FILTER / FX layout** (MainPage.h and OscPage.h non-operator layout, FilterVectorPhysicalPages.h, FilterWidgets.h, FilterDisplay.h, FxPage.h, FxDisplays.h, FxWidgets.h, FxLibrary.h):
I8-22, 33, 34, 37; S8-6, 8, 14, 24, 29, 34, 38 (VECTOR part), 40; V8-6 (FX, VECTOR, PHYSICAL parts), 9, 17, 19, 23, 24, 26 (VECTOR band), 27, 33, 34, 38, 39.

**R6 Global consistency, header, browser, SEQ** (IlanaLookAndFeel.h text fitting, shared widgets CardTabs.h / StateTabs.h / section headers, header and output meter, PresetPanel.h, dialogs, SeqPage.h, ClipEditor.h, GenerativeWidgets.h):
I8-20, 21, 23, 24, 25, 29, 31, 32, 35; S8-12, 15, 22, 23, 28, 30, 32, 33, 37, 39, 41; V8-12, 22, 25, 30, 36.
Squeezed text: fix centrally (shrink-to-fit with a floor, then ellipsis, never horizontal squeeze) and add a uitest that walks every page and fails on any squeezed label. DRAW on by default in the clip editor (S8 workflow).

Ownership overlaps: R1 and R2 both edit FmInputPages.h (R1: operator card, KEYS & VELOCITY, OP PITCH / OP LFO panel; R2: page layout, diagram area, FM matrix). R1 and R5 both edit MainPage.h / OscPage.h (R1: what an operator strip / card contains; R5: layout, heights, off strips, dead space). Keep edits to the other's regions minimal.

**After R1-R6 (by the thread):** merge R1..R6 plus the FX teardown fix, full gate, fresh renders, review 9.
