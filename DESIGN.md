---
name: ilanaSynth
colors:
  page: "#0f1014"
  header: "#0a0b0e"
  panel: "#15171c"
  raised: "#1c1f26"
  hover: "#313642"
  well: "#0a0b0e"
  line: "#2a2e37"
  line-inner: "#20232a"
  track: "#262a33"
  text-primary: "#eef0f4"
  text-label: "#aab0bb"
  text-hint: "#7a8190"
  accent-ember: "#ff6a3d"
  accent-ice: "#35c8ff"
  accent-acid: "#8fff3b"
  accent-magenta: "#ff4fd8"
  accent-text-on-tint: "#ff8a63"
  osc-1: "#f2b83d"
  osc-2: "#5b8cff"
  osc-3: "#3ecf9a"
  osc-4: "#a47bff"
  osc-5: "#e39a6b"
  osc-6: "#3fc7e8"
  env-amp: "#ff5a4a"
  env-filt: "#b46bff"
  env-filt2: "#7d86ff"
  env-mod: "#8fff3b"
  env-4: "#5b8cff"
typography:
  tiny: { fontFamily: Manrope, fontSize: 12.1px, fontWeight: 500 }
  label: { fontFamily: Manrope, fontSize: 12.65px, fontWeight: 500 }
  body: { fontFamily: Manrope, fontSize: 14.3px, fontWeight: 500 }
  title: { fontFamily: Manrope, fontSize: 15.95px, fontWeight: 700 }
  large: { fontFamily: Manrope, fontSize: 18.15px, fontWeight: 700 }
  display: { fontFamily: Manrope, fontSize: 23.1px, fontWeight: 700 }
  hero: { fontFamily: Manrope, fontSize: 28.6px, fontWeight: 700 }
rounded:
  card: 10px
  button: 3px to 8px (26 % of the height)
  pill: full
  menu: 10px
---

# Design System: ilanaSynth

The tokens above are copied from `src/gui/IlanaLookAndFeel.h` (`IlanaTheme::Ui`, `palette`, `oscColour`, `envIdentityColour`, `TextSize`). **The code is the source of truth**; if the two disagree, fix this file. Sizes are logical pixels at 100 % zoom; ilana runs the plugin at 175 %.

## Overview

A dark, flat graphite instrument panel with one accent. Operate mode: the tool disappears into the sound. Density like Vital, every control on one grid, big readable text at real scale. Authority, in order: the approved PLAY page (mockup fit8, `src/gui/pages/PlayPage.h`), the review-23 card language (headed sections on one grid), then `docs/UI-CONVENTIONS.md`.

## Colors

### Primary
The skin accent (`IlanaTheme::accent()`, one of four: ember, ice, acid, magenta) marks only what is active or carries a value: a knob's arc, a lit switch, the selected tab. Never decoration.

### Identity colours
Oscillators (osc-1..6) and envelopes (env-*) have fixed identity colours, used as a small tag, a card edge, a waveform, a mod ring or a cable; never as the accent. Since 2026-10-28 a section may also carry a colour the user picks (right-click), the colour pass's "colour only where it means something".

### Neutral
Surfaces step from header (darkest) to page, panel, raised and hover. Text has three steps: primary for values and titles, label for labels, hint for hints and disabled.

### Named Rules
- A switched-off section dims to 60 % alpha (`dimmedAlpha`), still about 4:1 for its labels.
- A card without a family colour has no tag in its title (a grey dot read as "off").

## Typography

One family, Manrope (Regular, Medium, Bold), embedded. Scale ratio about 1.1. Floors: passive text never below `tiny` (12.1), interactive text never below 13.75; anything that does not fit shrinks to the floor then ellipsises through `IlanaTheme::drawFitted`, never condenses sideways. Upper case for names of modules and sources (AMP ENV, OSC 1), sentence case for prose. Values keep their own case and units; a unit goes on a second line under the value.

## Layout

One spacing grid per card: controls in rows and columns with shared edges and baselines, labels on one left edge, toggles in one column, even gaps, equal margins, no dead space and no half-empty rows. Cards keep the same size whatever their content; when a page runs out of room it **scrolls**, it never folds a card. Window 1060 x 720 logical.

## Elevation & Depth

Flat. Cards are a panel fill with a 1 px line; displays are recessed wells (darker, inner line). A soft glow (`paintGlow`) marks a live or lit state only.

## Shapes

Rounded cards (10), rounded buttons scaled with their height, full pills for switches and choices. Knobs: three sizes by role (main 48, compact 40, mini 36, minimum 28), the ring 8 px inside the cell, the value never touching the ring (about 3 px clear).

## Components

### Knobs
`KnobControl` / packed knob: name under, value inside or under, one value size, no shrink-to-fit. Right-click on a mod ring offers Unipolar, Bipolar, Stereo.

### Cards / Containers
`paintCard` with a header (`paintCardHeader`): title in upper case, lower-case caption fragment, the module's on/off switch at the right of the header.

### Tabs and pickers
`CardTabs` / `StateTabs` with the on dot for things that switch; page switches sit right after the main tabs. Oscillator pickers are always `OscPicker`.

### Menus
`PopupCombo`, 26 px rows, 10 px radius, a menu beside a switch lists only what is on.

## Do's and Don'ts

### Do:
- Restate the scope of any UI change in one line and get ilana's ok before building.
- Check every change in `ilanaSnapshot` PNGs at 175 % (`ILANA_SNAPSHOT_SCALE=1.75`) against the approved pictures, measured, not eyeballed.
- Keep motion short (120 to 180 ms) and honour the "Full motion" setting.

### Don't:
- Relayout when the ask was a visual fix; redesign the PLAY page; fold cards for room.
- Use colour as decoration, or the accent for an inactive state.
- Use JUCE `drawFittedText` with a horizontal squash, or text smaller than the mockup.
