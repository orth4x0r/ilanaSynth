# UI conventions

The rules every page follows, so a control looks and reads the same wherever it is (UI review 7, I7-35 to I7-37, V7-36). Most are built into the shared widgets in `src/gui/` (named below), so a page gets them by using those widgets.

## On/off switches
- **A card's own module** (WEST, BODY, VECTOR, SUB + NOISE, an FX card, an oscillator): the switch sits in the card header, at the right (`IlanaTheme::cardSwitchBounds`).
- **A tabbed card's engines** (SEQ's ARP / PROB SEQ / CLIP, the FM operators): the tab says whether its engine is on with the on dot (below); the engine's switch is in its tab, not in the body.
- **A part inside a card** (SNAP TO KEY, STRUM, SPRAY): a switch in its sub-box header, with the state word after it.
- Never state "on" or "off" in text without a switch beside it.

## The on dot
- Every tab that names something that can be switched on or off shows one indicator: a lit dot with a halo while on, a quiet ring while off (`IlanaTheme::paintOnDot`, used by `CardTabs`, `StateTabs` and the FM operator pills).
- Tabs that name things that are always on (AMP ENV / FILT ENV, F1 / F2) have no dot.

## Captions and hints
- **Header captions** (a card's or a section's subtitle): lower-case fragments, no full stop ("rows modulate columns", "same controls as its OSC card"). `paintCardHeader` and `paintSectionTitle` apply this (`IlanaTheme::captionFragment`).
- **Hints under or beside a control**: full sentences with a full stop ("Drag the graph's points: across for time, up or down for level.").
- Tooltips: the control's name on the first line, then sentences.

## Casing
- Names of modules and sources are upper case in labels, chips, tabs and combos: AMP ENV, FILT ENV, OSC 1, LFO 2, OP ENV.
- Sentence case only for prose: hints, tooltips, dialog text, menu items that are actions ("Reset to default").
- Values keep their units' own case (ms, dB, Hz, ×1.00).
