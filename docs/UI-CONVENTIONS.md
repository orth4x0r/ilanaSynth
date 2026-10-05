# UI conventions

The rules every page follows, so a control looks and reads the same wherever it is (UI review 7, I7-35 to I7-37, V7-36). Most are built into the shared widgets in `src/gui/` (named below), so a page gets them by using those widgets.

## On/off switches
- **A card's own module** (WEST, BODY, VECTOR, SUB + NOISE, an FX card, an oscillator): the switch sits in the card header, at the right (`IlanaTheme::cardSwitchBounds`).
- **A tabbed card's engines** (SEQ's ARP / PROB SEQ / CLIP, the FM operators): the engine's switch is in its tab, not in the body. Where a click in the tab is the engine's power (`CardTabs::onToggle`, SEQ's PATTERN tabs), the tab draws a small switch left of its name, clicked apart from the tab itself; otherwise the tab shows the on dot (below).
- **A part inside a card** (SNAP TO KEY, STRUM, SPRAY): a switch in its sub-box header, with the state word after it.
- Never state "on" or "off" in text without a switch beside it.

## The on dot
- Every tab that names something that can be switched on or off shows one indicator: a lit dot with a halo while on, a quiet ring while off (`IlanaTheme::paintOnDot`, used by `CardTabs`, `StateTabs` and the FM operator pills), or, in a `CardTabs` with `onToggle`, the tab's switch in its place.
- Tabs that name things that are always on (AMP ENV / FILT ENV, F1 / F2) have no dot.

## Captions and hints
- **Header captions** (a card's or a section's subtitle): lower-case fragments, no full stop ("rows modulate columns", "same controls as its OSC card"). `paintCardHeader` and `paintSectionTitle` apply this (`IlanaTheme::captionFragment`).
- **Hints under or beside a control**: full sentences with a full stop ("Drag the graph's points: across for time, up or down for level.").
- Tooltips: the control's name on the first line, then sentences.

## Casing
- Names of modules and sources are upper case in labels, chips, tabs and combos: AMP ENV, FILT ENV, OSC 1, LFO 2, OP ENV.
- Sentence case only for prose: hints, tooltips, dialog text, menu items that are actions ("Reset to default").
- Values keep their units' own case (ms, dB, Hz, ×1.00).
- Buttons say what they do in upper case everywhere, dialogs included: SAVE, CANCEL, LOAD ANYWAY, SAVE AND LOAD, GOT IT (UI review 8, S8-23). Check boxes are sentences ("Don't show this again"), the same words wherever the same tick appears.

## Separators
- Parts of a status line or a summary are joined with a middle dot " · " ("VECTOR OFF · switch on to mix the corners", "3 VOICES · 12 ct · 50% WIDTH"), never a spaced hyphen (UI review 8, I8-35).
- A missing or switched-off part in a label reads "OSC 4: none", "OSC 2: off", not in parentheses.

## Text that may not fit
- All fitted text goes through `IlanaTheme::drawFitted` (labels, knob values and menus get it from the look and feel): a line that is too long first shrinks, down to the passive floor (the interactive floor for buttons, menus and values), then is cut with an ellipsis. It is never condensed sideways (JUCE's `drawFittedText` with a horizontal scale under 1), which read as a broken font (UI review 8, V8-12, I8-25).
- The UI test paints every page on three patches at 100 % and 75 % and fails on any cut text; it also fails if `drawFittedText` appears anywhere in `src/` but the helper.

## On dots and cards without a switch
- A tab for something with no switch of its own (VOICE, SPREAD & DRIFT, SOUNDBOARD) has no on dot (`StateTabs::Item::dot`), rather than a dot lit by "some value is above zero" (UI review 8, I8-21). An off oscillator's tab only puts its dot out; it doesn't also say OFF.
- A card without a family colour (OUTPUT, SIGNAL FLOW) has no tag in its title: a grey dot read as "switched off" (`IlanaTheme::hasFamilyColour`, I8-32).

## Page switches
- A top-level tab's own pages (PLAY's OVERVIEW / VECTOR, OSC's OSCILLATORS / PHYSICAL, MOD's ENV / LFO / MATRIX) are picked with the switch right after the tabs, not at the far right beside SCOPE (UI review 8, V8-36).

## Vocabulary: one name for one thing (UI review 10, I10-3, I10-13)
- **RANDOMISE** (the dice): scramble the parameters. The header's dice menu randomises the patch or a part of it; the FX page's dice (tooltip "Randomise FX") the chain. **RANDOM PRESET** (the browser) is a different action: it loads an existing preset from the list shown.
- **SAVE** is the patch (header SAVE, SAVE AS in the browser); **SAVE CHAIN / LOAD CHAIN** is the FX chain's file. A button that writes or reads a chain says CHAIN.
- **KEYBOARD** is the on-screen keyboard (the header button); **KEYS** is the SEQ chain's input node only (the preset category is a browser filter, not a control). The OSC strip's acoustic-keys tab is **SOUNDBOARD** and its drone strings **STRINGS**, the nouns the signal flow uses.
- **VOICE** is how notes are shared out (mode, voices, bend, glide) and lives in one place: OSC > VOICE. The header's VOICES count and the settings menu's "Voice settings" open that tab; there is no second menu. **UNISON** is the oscillator's stack of detuned copies only (its card's row); the strip's SPREAD & DRIFT tab is what every voice shares.
- A DX7 operator shows one level, **OUTPUT**. The oscillator's own level into the voice is **VOICE LEVEL** (OSC page, WAVE row).
- A button that opens a file chooser says **LOAD...**; one that jumps to another page is **EDIT <WHAT> ›** (`styleJumpLink`; the cards' header link `CardTabs` draws is the same words, "EDIT ›").
- Oscillator pickers are `OscPicker` everywhere (a coloured dot and "OSC n", the number alone when the header is tight); no page draws its own.
- A menu beside a switch lists only what is on: the "Off" choice stays in the parameter (indices never change) but the switch is the only off; while the switch is off the menu reads the choice it will bring back.
