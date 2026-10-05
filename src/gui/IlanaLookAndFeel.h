#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <BinaryData.h>

#include <cmath>
#include <unordered_map>

namespace IlanaTheme
{
inline juce::Colour& accentRef()
{
    static juce::Colour accent (0xffff6a3d);
    return accent;
}

inline juce::Colour accent() { return accentRef(); }

inline const juce::uint32 palette[]
{
    0xffff6a3d, 0xff35c8ff, 0xff8fff3b, 0xffff4fd8
};

inline constexpr int numPalettes = 4;

// The colour tokens. Surfaces are flat graphite with a faint cool bias; the
// accent is kept for what is active or carries a value, so it stays the one
// thing the eye goes to.
namespace Ui
{
    inline const juce::Colour bg      { 0xff111215 }; // page
    inline const juce::Colour header  { 0xff15171a }; // header strip
    inline const juce::Colour panel   { 0xff181a1e }; // cards
    inline const juce::Colour raised  { 0xff22252a }; // buttons, menus
    inline const juce::Colour hover   { 0xff31363e }; // lighter than line, so hovers show
    inline const juce::Colour well    { 0xff0e0f12 }; // displays
    inline const juce::Colour line    { 0xff2a2e34 }; // borders
    inline const juce::Colour track   { 0xff30343b }; // knob and slider tracks
    inline const juce::Colour text    { 0xffe9eaed }; // values, titles
    inline const juce::Colour text2   { 0xffa3a9b1 }; // labels
    inline const juce::Colour text3   { 0xff7d838c }; // hints, disabled
}

// The oscillators' identity colours: shown as a small tag, a card edge and a
// waveform, never as the accent.
inline juce::Colour oscColour (int index)
{
    static const juce::uint32 colours[] { 0xfff5c542, 0xff5b8cff, 0xff4fd1a5, 0xffb28aff, 0xffd9a070, 0xff48d4e8 };
    return juce::Colour (colours[(size_t) juce::jlimit (0, 5, index)]);
}

// How opaque a control is while it does nothing (its section is off): dimmed,
// but still readable against the card (about 4:1 for the label text).
inline constexpr float dimmedAlpha = 0.6f;

// Knob sizes by role (UI review 4, V22), the same on every page: the largest
// dial a knob takes (its cell; the ring drawn inside is 8 px smaller). A
// smaller cell shrinks the dial, down to `minimum`.
//   main:  the parameters a card is about (oscillator, filter, envelope...)
//   small: detail rows under them (a physical string's exciter, body...)
//   mini:  grids of amounts (the FM send matrix)
namespace KnobSize
{
    inline constexpr int main    = 48;
    inline constexpr int small   = 40;
    inline constexpr int mini    = 36;
    inline constexpr int minimum = 28;
}

// The type scale (component units; the editor zooms them). Every text in the
// UI uses one of these, so sizes stay consistent from page to page. UI review
// 6 (#46) raised the whole scale by 10 %, so that at 75 % zoom (0.75) tiny
// text still renders 9 px tall.
namespace TextSize
{
    inline constexpr float tiny    = 12.1f;  // axis ticks, badges, hints
    inline constexpr float label   = 12.65f; // knob and field labels
    inline constexpr float body    = 14.3f;  // values, menus, buttons
    inline constexpr float title   = 15.95f; // tabs, section titles
    inline constexpr float large   = 18.15f; // preset name
    inline constexpr float display = 23.1f;
    inline constexpr float hero    = 28.6f;

    // The floors (UI review 4, S25), in the same units (a font's full
    // height; Manrope's letters are about 0.73 of it): no text a user clicks
    // is smaller than minInteractive (buttons, tabs, pills, menus), no other
    // text smaller than minPassive. The UI test walks every page's labels
    // and buttons against them.
    inline constexpr float minInteractive = 13.75f;
    inline constexpr float minPassive     = tiny;

    // The smallest zoom the editor offers, and the floor at it in screen
    // pixels (UI review 5 #31, 6 #46): text that would draw smaller is left
    // out (a hint) rather than shrunk. 12 units at 75 % is 9 px, so
    // the smallest sizes grow to 10 px there (UI review 9, V9-26).
    inline constexpr float smallestZoom  = 0.75f;
    inline constexpr float screenFloorPx = 10.0f;
    inline constexpr float zoomFloor     = screenFloorPx / smallestZoom;
}

// The UI test's probe: while armed, the smallest text drawn (in device
// pixels, through font() below) and how often it went under a limit.
struct FontProbe
{
    bool armed = false;
    float smallest = 1.0e6f;
    float limit = 0.0f;
    int under = 0;
};

inline FontProbe& fontProbe()
{
    static FontProbe probe;
    return probe;
}

// The fonts, released when JUCE shuts down (after the last editor closes)
// rather than as statics when the plugin is unloaded: by then JUCE's font
// engine is gone, and freeing a FreeType face after it crashed hosts on exit.
struct ThemeResources : private juce::DeletedAtShutdown
{
    // Manrope (SIL Open Font License, resources/fonts/OFL-Manrope.txt).
    juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (BinaryData::Manrope400Regular_ttf, (size_t) BinaryData::Manrope400Regular_ttfSize);
    // Used for small text: light strokes thin out at tiny sizes, so the
    // medium weight keeps stems readable.
    juce::Typeface::Ptr medium = juce::Typeface::createSystemTypefaceFor (BinaryData::Manrope500Medium_ttf, (size_t) BinaryData::Manrope500Medium_ttfSize);
    juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor (BinaryData::Manrope700Bold_ttf, (size_t) BinaryData::Manrope700Bold_ttfSize);

    ~ThemeResources() override { clearSingletonInstance(); }

    JUCE_DECLARE_SINGLETON_INLINE (ThemeResources, false)
};

inline juce::Typeface::Ptr& regularTypefaceRef() { return ThemeResources::getInstance()->regular; }
inline juce::Typeface::Ptr& mediumTypefaceRef() { return ThemeResources::getInstance()->medium; }
inline juce::Typeface::Ptr& boldTypefaceRef() { return ThemeResources::getInstance()->bold; }

// The editor renders its design space through a scale transform; this mirrors
// that zoom so fonts can be snapped to whole device pixels (fractional
// rasterisation is what makes text look soft).
inline float& uiScaleRef()
{
    static float scale = 1.0f;
    return scale;
}

// All UI text should be created through this: a Font constructed from plain
// FontOptions resolves its typeface through the process default LookAndFeel,
// not the one set on our editor, which is why it fell back to Segoe UI.
// Pass tabular for numbers that change while you watch (knob values, BPM,
// CPU), so they don't shift sideways; everywhere else digits are
// proportional, or a "1" sits in a wide gap ("LFO 1", "1/8").
inline juce::FontOptions font (float height, bool bold = false, bool tabular = false)
{
    const auto scale = juce::jmax (0.25f, uiScaleRef());
    // No text under the screen floor, whatever the zoom (V9-26).
    const auto deviceHeight = juce::jmax (height * scale, TextSize::screenFloorPx);
    if (auto& probe = fontProbe(); probe.armed)
    {
        probe.smallest = juce::jmin (probe.smallest, deviceHeight);
        probe.under += deviceHeight < probe.limit - 0.01f ? 1 : 0;
    }
    const auto snapped = juce::jmax (1.0f, std::round (deviceHeight)) / scale;
    const auto typeface = bold ? boldTypefaceRef()
                               : (deviceHeight < 16.5f ? mediumTypefaceRef() : regularTypefaceRef());

    const auto options = juce::FontOptions().withHeight (snapped)
                                            .withStyle (bold ? "Bold" : "Regular")
                                            .withTypeface (typeface);
    return tabular ? options.withFeatureEnabled ("tnum") : options;
}

// The UI test's other probe (UI review 8, V8-12 / I8-25): while armed, every
// text drawFitted() below had to shrink, and every one it still had to cut.
struct TextFitProbe
{
    bool armed = false;
    juce::StringArray shrunk, cut;
    juce::StringArray cutDetails; // "text: needs N px of M at H"
    juce::StringArray respelled;  // "text -> line": a value drawn without its space or with a shorter unit
    juce::StringArray garbled;    // text holding double-encoded UTF-8 (V10-1)
};

// Whether a string holds the marks of UTF-8 read as Latin-1 and encoded again
// ("Â·" for a middle dot, "â€" for a dash or quote): never real text here.
inline bool isMojibake (const juce::String& text)
{
    return text.contains (juce::String::fromUTF8 ("\xc3\x82")) || text.contains (juce::String::fromUTF8 ("\xc3\x83"))
           || text.contains (juce::String::fromUTF8 ("\xc3\xa2\xe2\x82\xac")) || text.contains (juce::String::fromUTF8 ("\xef\xbf\xbd"));
}

inline TextFitProbe& textFitProbe()
{
    static TextFitProbe probe;
    return probe;
}

// Text in a box, the one way the UI fits text that may be too long (UI review
// 8, V8-12): never squeezed sideways (JUCE's fitted text condenses glyphs,
// which reads as a broken font next to normal text). A line that doesn't fit
// tries, in turn, until it does:
//   1. a smaller size, down to `floorHeight` (the floor for its kind: the
//      interactive one for values, menus and buttons);
//   2. the same, with its letters set a little closer (tracking, up to
//      0.04 of the height between letters: spacing, not narrower glyphs,
//      and never enough to close a gap between words);
//   3. for a value with a unit ("-30.9 dB"), the space goes and a long
//      unit is shortened ("kHz" to "k"): a value never loses its unit;
//   4. down to the passive floor (still 10 px at 75 %), tracked;
//   5. wrapped onto its other lines where it may take more, else cut with
//      an ellipsis.
// The probe records every line that had to change and every one cut.
inline juce::Font fittedFont (const juce::Font& font, const juce::String& line, float room, float floor)
{
    const auto scale = juce::jmax (0.25f, uiScaleRef());
    const auto width = juce::GlyphArrangement::getStringWidth (font, line);
    // (Nor shrunk under the screen floor at a small zoom: V9-26.)
    floor = juce::jmin (font.getHeight(), juce::jmax (floor, TextSize::screenFloorPx / scale));

    // Width follows height closely, so one step lands near the fit; snap
    // down to whole device pixels, as font() does, so the glyphs stay crisp.
    auto height = juce::jmax (floor, std::floor (font.getHeight() * room / juce::jmax (1.0f, width) * scale) / scale);
    auto smaller = font.withHeight (height);

    while (height > floor && juce::GlyphArrangement::getStringWidth (smaller, line) > room + 0.01f)
    {
        height = juce::jmax (floor, height - 1.0f / scale);
        smaller = font.withHeight (height);
    }

    // (Tracking is a touch only, never more than 0.04 of the height: more
    // closes the gap between words, "AMPENV", and reads as condensed type.)
    for (auto tracking = -0.01f; tracking >= -0.0401f && juce::GlyphArrangement::getStringWidth (smaller, line) > room + 0.01f;
         tracking -= 0.01f)
        smaller = font.withHeight (height).withExtraKerningFactor (tracking);

    return smaller;
}

inline bool fitsIn (const juce::Font& font, const juce::String& line, float room)
{
    return juce::GlyphArrangement::getStringWidth (font, line) <= room + 0.01f;
}

inline void drawFitted (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                        juce::Justification justification, int maximumLines = 1,
                        float floorHeight = TextSize::minPassive)
{
    if (area.isEmpty() || text.isEmpty())
        return;

    if (auto& probe = textFitProbe(); probe.armed && isMojibake (text))
        probe.garbled.addIfNotAlreadyThere (text);

    if (text.containsAnyOf ("\r\n"))
    {
        g.drawFittedText (text, area, justification, juce::jmax (1, maximumLines), 1.0f);
        return;
    }

    auto line = text.trim();
    const auto font = g.getCurrentFont();
    const auto room = (float) area.getWidth();

    if (fitsIn (font, line, room))
    {
        g.drawText (line, area, justification, false);
        return;
    }

    // Text given more than one line wraps at its own size first, when the
    // lines fit the box; it only shrinks when they don't.
    if (maximumLines > 1)
    {
        juce::GlyphArrangement wrapped;
        wrapped.addJustifiedText (font, line, 0.0f, font.getAscent(), room, juce::Justification::left);
        const auto lines = wrapped.getNumGlyphs() > 0
                               ? juce::roundToInt (wrapped.getBoundingBox (0, -1, true).getHeight() / font.getHeight())
                               : 1;

        if (lines <= maximumLines && (float) lines * font.getHeight() <= (float) area.getHeight() + 0.5f)
        {
            g.drawFittedText (line, area, justification, maximumLines, 1.0f);
            return;
        }
    }

    auto fitted = fittedFont (font, line, room, floorHeight);

    if (! fitsIn (fitted, line, room))
    {
        // A number with a unit after a space never loses its unit (V9-5):
        // the space goes first ("-30.9dB"), then a long unit is shortened
        // ("kHz" to "k"), each at the floor size; the passive floor last.
        const auto number = line.upToLastOccurrenceOf (" ", false, false);
        const auto unit = line.fromLastOccurrenceOf (" ", false, false);
        const auto isValue = number.isNotEmpty() && unit.length() <= 3 && ! unit.containsAnyOf ("0123456789")
                             && number.retainCharacters ("0123456789").isNotEmpty()
                             && number.removeCharacters ("0123456789.,+-:/").removeCharacters (juce::String::fromUTF8 ("\xe2\x88\x92\xc3\x97")).isEmpty();
        auto settled = false;

        if (isValue)
        {
            const auto shortUnit = unit == "kHz" ? juce::String ("k") : unit == "oct" ? juce::String ("o") : unit;
            const juce::String candidates[] { number + unit, number + shortUnit };

            for (const auto& candidate : candidates)
            {
                const auto attempt = fittedFont (font, candidate, room, floorHeight);

                if (fitsIn (attempt, candidate, room))
                {
                    line = candidate;
                    fitted = attempt;
                    settled = true;
                    break;
                }
            }

            if (! settled)
            {
                line = number + shortUnit;
                fitted = fittedFont (font, line, room, TextSize::minPassive);
                settled = true;
            }
        }

        if (! settled)
            fitted = fittedFont (font, line, room, TextSize::minPassive);
    }

    const auto fits = fitsIn (fitted, line, room);

    if (! fits && maximumLines > 1)
    {
        g.drawFittedText (line, area, justification, maximumLines, 1.0f);
        return;
    }

    if (auto& probe = textFitProbe(); probe.armed)
    {
        if (line != text.trim())
            probe.respelled.addIfNotAlreadyThere (text.trim() + " -> " + line);
        (fits ? probe.shrunk : probe.cut).addIfNotAlreadyThere (text.trim());
        if (! fits)
            probe.cutDetails.add (line + ": needs " + juce::String (juce::GlyphArrangement::getStringWidth (fitted, line), 1) + " of "
                                  + juce::String (room, 1) + " px at " + juce::String (fitted.getHeight(), 2));
    }

    g.setFont (fitted);
    g.drawText (line, area, justification, ! fits);
    g.setFont (font);
}

inline void drawFitted (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                        juce::Justification justification, int maximumLines = 1,
                        float floorHeight = TextSize::minPassive)
{
    drawFitted (g, text, area.toNearestInt(), justification, maximumLines, floorHeight);
}

inline void paintPageBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour (Ui::bg);
    g.fillRect (bounds);

    // A faint accent haze from the top, so the page isn't dead flat.
    juce::ColourGradient haze (accent().withAlpha (0.035f), (float) bounds.getCentreX(), (float) bounds.getY() - 80.0f,
                               accent().withAlpha (0.0f), (float) bounds.getCentreX(), (float) bounds.getY() + (float) bounds.getHeight() * 0.7f, true);
    g.setGradientFill (haze);
    g.fillRect (bounds);
}

// A soft glow around a rounded rectangle, built from a few widening strokes.
inline void paintGlow (juce::Graphics& g, juce::Rectangle<float> bounds, float radius, juce::Colour colour, float strength)
{
    if (strength <= 0.01f)
        return;

    for (int i = 3; i >= 1; --i)
    {
        const auto spread = (float) i * 2.2f;
        g.setColour (colour.withAlpha (0.07f * strength / (float) i));
        g.drawRoundedRectangle (bounds.expanded (spread * 0.5f), radius + spread * 0.5f, spread);
    }
}

inline void paintCard (juce::Graphics& g, juce::Rectangle<float> bounds, float radius, juce::Colour tint)
{
    g.setColour (Ui::panel);
    g.fillRoundedRectangle (bounds, radius);

    g.setColour (Ui::line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

    // The tint only as a faint wash along the top edge.
    juce::ColourGradient wash (tint.withAlpha (0.05f), bounds.getX(), bounds.getY(),
                               tint.withAlpha (0.0f), bounds.getX(), bounds.getY() + juce::jmin (60.0f, bounds.getHeight()), false);
    g.setGradientFill (wash);
    g.fillRoundedRectangle (bounds.reduced (1.0f), radius);
}

inline void paintRecessedPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setColour (Ui::bg.interpolatedWith (Ui::panel, 0.5f));
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (Ui::line.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
}

inline void paintWell (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setColour (Ui::well);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (Ui::line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
}

// Kept for callers that layered it over a well; the flat theme has no gloss.
inline void paintGlassOverlay (juce::Graphics&, juce::Rectangle<float>, float) {}

// A section's identity tag: a small glowing dot in its colour.
inline void paintTag (juce::Graphics& g, juce::Point<float> centre, juce::Colour colour)
{
    g.setColour (colour.withAlpha (0.22f));
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre));
    g.setColour (colour);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre));
}

// Whether a tab's switchable part is on (review 7, I7-37: one indicator on
// every tab that names a switch, CardTabs and the FM operator pills): a lit
// dot with a halo while on, a quiet ring while off.
inline void paintOnDot (juce::Graphics& g, juce::Point<float> centre, juce::Colour colour, bool on)
{
    const auto dot = juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre);

    if (on)
    {
        g.setColour (colour.withAlpha (0.3f));
        g.fillEllipse (dot.expanded (2.5f));
        g.setColour (colour.interpolatedWith (juce::Colours::white, 0.15f));
        g.fillEllipse (dot);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawEllipse (dot.reduced (0.5f), 1.0f);
    }
}

// A header caption (a card's or a section's subtitle) as the one grammar
// headers use: a lower-case fragment without a full stop (review 7, I7-35;
// hints under a control are sentences). See docs/UI-CONVENTIONS.md.
inline juce::String captionFragment (const juce::String& text)
{
    auto fragment = text.trim();

    if (fragment.endsWithChar ('.') && ! fragment.endsWith (".."))
        fragment = fragment.dropLastCharacters (1);

    if (fragment.length() > 1 && juce::CharacterFunctions::isUpperCase (fragment[0])
        && juce::CharacterFunctions::isLowerCase (fragment[1]))
        fragment = fragment.substring (0, 1).toLowerCase() + fragment.substring (1);

    return fragment;
}

// Where a card title's subtitle can start: past the tag and the name.
inline int cardTitleWidth (const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidthInt (juce::Font (font (TextSize::body, true)), text) + 30;
}

// A card's title: its tag, then the name in the text colour. A card with no
// family colour (a grey or white one: OUTPUT, SIGNAL FLOW) has no tag, so
// no grey dot reads as "switched off" beside the on dots (review 8, I8-32).
inline bool hasFamilyColour (juce::Colour colour) { return colour.getSaturation() > 0.15f; }

inline void paintCardTitle (juce::Graphics& g, juce::Rectangle<int> header, const juce::String& text, juce::Colour colour)
{
    if (hasFamilyColour (colour))
        paintTag (g, { (float) header.getX() + 3.0f, (float) header.getCentreY() }, colour);
    g.setColour (Ui::text);
    g.setFont (font (TextSize::body, true));
    g.drawText (text, header.withTrimmedLeft (14), juce::Justification::centredLeft);
}

// A card's header, the same everywhere: tag, title, then a quiet subtitle
// right after the title (never pushed to the right edge, which belongs to the
// card's tabs and its on switch).
// A hint cut to the room it has: whole " · " parts are dropped from the end,
// and a hint that still doesn't fit is left out rather than cut mid-word
// ("Operator E...") or shrunk.
inline juce::String fittedHint (const juce::String& hint, const juce::Font& f, float width)
{
    const auto dot = juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "));
    auto text = hint;

    while (text.isNotEmpty() && juce::GlyphArrangement::getStringWidth (f, text) > width)
        text = text.contains (dot) ? text.upToLastOccurrenceOf (dot, false, false) : juce::String();

    return text;
}

inline void paintCardHeader (juce::Graphics& g, juce::Rectangle<int> header, const juce::String& title,
                             const juce::String& subtitle, juce::Colour colour, int rightReserve = 100)
{
    paintCardTitle (g, header, title, colour);

    if (subtitle.isEmpty())
        return;

    auto area = header.withTrimmedLeft (cardTitleWidth (title)).withTrimmedRight (rightReserve); // 16 px after the title
    g.setColour (Ui::text3);
    g.setFont (font (TextSize::label));
    g.drawText (fittedHint (captionFragment (subtitle), juce::Font (font (TextSize::label)), (float) area.getWidth()), area,
                juce::Justification::centredLeft, false);
}

// Where a card's on switch goes (a ToggleControl with its 13 px label
// space), centred on the title line: at the card's right edge (its pill
// level with the right of the card's content), or left of the remove
// button on cards that have one.
inline juce::Rectangle<int> cardSwitchBounds (juce::Rectangle<int> card, int titleCentreY, bool besideRemoveButton = false)
{
    // Level with the title's letters, which sit a touch below the line's
    // centre.
    titleCentreY += 1;

    if (besideRemoveButton)
    {
        constexpr int width = 56, rightInset = 8 + 28;
        return { card.getRight() - rightInset - width, titleCentreY - 13 - 10, width, 13 + 20 };
    }

    // A 32 px pill centred in 40, its right edge 12 px in from the card's:
    // the same margin as the title's tag on the left.
    constexpr int width = 40, rightInset = 12 - 4;
    return { card.getRight() - rightInset - width, titleCentreY - 13 - 10, width, 13 + 20 };
}

// An on/off switch: a pill with a sliding knob. `amount` runs 0 (off) to 1
// (on) so callers can animate it; the on state glows in `colour`.
inline void paintSwitch (juce::Graphics& g, juce::Rectangle<float> area, float amount, juce::Colour colour, float hover = 0.0f)
{
    const auto h = juce::jmin (area.getHeight(), 18.0f);

    if (h < 6.0f)
        return;

    const auto w = h * 1.8f;
    const auto pill = juce::Rectangle<float> (w, h).withCentre (area.getCentre());

    if (amount > 0.01f)
        paintGlow (g, pill, h * 0.5f, colour, amount * (0.8f + 0.4f * hover));

    g.setColour (Ui::track.interpolatedWith (colour, amount).interpolatedWith (juce::Colours::white, 0.05f * hover));
    g.fillRoundedRectangle (pill, h * 0.5f);

    const auto knob = h - 4.0f;
    const auto x = pill.getX() + 2.0f + (w - 4.0f - knob) * amount;
    g.setColour (juce::Colour (0xffc9ccd1).interpolatedWith (juce::Colours::white, amount));
    g.fillEllipse (x, pill.getY() + 2.0f, knob, knob);
}

// A pill's text: clickable, so at the interactive floor.
inline juce::FontOptions pillFont() { return font (TextSize::minInteractive, true); }

// A choice pill: the one style for every small selector (F1 / F2, 12 / 24
// dB, OSC 1 / 2 / 3, ARP / EUCLID, WAVE / SPEC ...). The chosen one is tinted
// in its colour with a bright edge; the others are quiet.
inline void paintPill (juce::Graphics& g, juce::Rectangle<float> pill, const juce::String& text, juce::Colour colour,
                       bool active, float hover = 0.0f, bool enabled = true)
{
    const auto radius = pill.getHeight() * 0.5f;
    g.setColour (active ? colour.withAlpha (0.22f) : juce::Colours::white.withAlpha (0.03f + 0.05f * hover));
    g.fillRoundedRectangle (pill, radius);

    g.setColour (active ? colour.withAlpha (0.8f) : juce::Colours::white.withAlpha (0.06f + 0.08f * hover));
    g.drawRoundedRectangle (pill.reduced (0.5f), radius, 1.0f);

    g.setColour (active ? colour.interpolatedWith (juce::Colours::white, 0.2f)
                        : juce::Colours::white.withAlpha ((enabled ? 0.55f : 0.3f) + 0.3f * hover));
    g.setFont (pillFont());
    g.drawText (text, pill, juce::Justification::centred);
}
// Draws a TextButton as a choice pill in `colour` when chosen.
inline void makePill (juce::Button& button, juce::Colour colour)
{
    button.getProperties().set ("pill", true);
    button.setColour (juce::TextButton::buttonOnColourId, colour);
    button.repaint();
}

} // namespace IlanaTheme

class IlanaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IlanaLookAndFeel()
    {
        using namespace IlanaTheme;

        regularTypeface = regularTypefaceRef();
        boldTypeface = boldTypefaceRef();

        if (regularTypeface != nullptr)
            setDefaultSansSerifTypeface (regularTypeface);

        setColour (juce::ResizableWindow::backgroundColourId, Ui::bg);

        setColour (juce::Slider::textBoxTextColourId, Ui::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::thumbColourId, juce::Colours::white);

        setColour (juce::Label::textColourId, Ui::text2);

        setColour (juce::ComboBox::backgroundColourId, Ui::raised);
        setColour (juce::ComboBox::outlineColourId, Ui::line);
        setColour (juce::ComboBox::textColourId, Ui::text);
        setColour (juce::ComboBox::arrowColourId, Ui::text3);

        setColour (juce::PopupMenu::backgroundColourId, Ui::raised);
        setColour (juce::PopupMenu::textColourId, Ui::text);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, Ui::text2);

        setColour (juce::TextButton::buttonColourId, Ui::raised);
        setColour (juce::TextButton::textColourOffId, Ui::text2);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);

        setColour (juce::ToggleButton::textColourId, Ui::text2);

        setColour (juce::TabbedComponent::backgroundColourId, Ui::bg);
        setColour (juce::TabbedComponent::outlineColourId, Ui::line);

        setColour (juce::TabbedButtonBar::tabOutlineColourId, Ui::line);
        setColour (juce::TabbedButtonBar::tabTextColourId, Ui::text2);
        setColour (juce::TabbedButtonBar::frontTextColourId, Ui::text);

        setColour (juce::ScrollBar::thumbColourId, Ui::track);
        setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);

        setColour (juce::TooltipWindow::backgroundColourId, Ui::raised);
        setColour (juce::TooltipWindow::textColourId, Ui::text);
        setColour (juce::TooltipWindow::outlineColourId, Ui::line);

        setColour (juce::TextEditor::backgroundColourId, Ui::well);
        setColour (juce::TextEditor::outlineColourId, Ui::line);
        setColour (juce::TextEditor::textColourId, Ui::text);

        applyAccent();
    }

    void setAccent (juce::Colour colour)
    {
        IlanaTheme::accentRef() = colour;
        applyAccent();
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override
    {
        if (regularTypeface == nullptr)
            return LookAndFeel_V4::getTypefaceForFont (font);

        const auto style = font.getTypefaceStyle();

        if (boldTypeface != nullptr
            && (style.containsIgnoreCase ("Bold") || style.containsIgnoreCase ("Semi")
                || style.containsIgnoreCase ("Black")))
            return boldTypeface;

        return regularTypeface;
    }

    // Every menu's text the same size from 18 px boxes up (review 8, S8-22:
    // a 19 px EXCITE box drew "Piano Hammer" a size down from its
    // neighbours); only shorter boxes go to the interactive floor.
    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::Font (IlanaTheme::font (box.getHeight() >= 18 ? IlanaTheme::TextSize::body : IlanaTheme::TextSize::minInteractive));
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body));
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        // Labels are built once, but painted at every zoom level, so re-run the
        // size through IlanaTheme::font to keep device pixels whole.
        const auto& current = label.getFont();
        return juce::Font (IlanaTheme::font (current.getHeight(), current.isBold(),
                                             label.getProperties().contains ("tabular")));
    }

    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override
    {
        // A lone symbol ("×", "+") is a glyph a third the font's height:
        // draw it larger, so a remove or add button reads as one.
        if (const auto text = button.getButtonText(); text.length() == 1 && ! juce::CharacterFunctions::isLetterOrDigit (text[0]))
            return juce::Font (IlanaTheme::font (juce::jlimit (IlanaTheme::TextSize::minInteractive, 22.0f, (float) buttonHeight * 0.95f)));


        // Never under the interactive floor, however short the button.
        return juce::Font (IlanaTheme::font (juce::jlimit (IlanaTheme::TextSize::minInteractive, IlanaTheme::TextSize::body,
                                                           (float) buttonHeight * 0.72f)));
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->getProperties().set ("tabular", true); // a live value
        label->setBorderSize ({ 1, 1, 1, 1 });          // (the value keeps its room, and its unit's space: V10-8)
        label->setFont (IlanaTheme::font (IlanaTheme::TextSize::body, false, true));
        return label;
    }

    void applyAccent()
    {
        const auto accent = IlanaTheme::accent();

        setColour (juce::Slider::rotarySliderFillColourId, accent);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.28f));
        setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.85f));
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::TabbedButtonBar::frontOutlineColourId, accent);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
        setColour (juce::CaretComponent::caretColourId, accent);
    }

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (2.0f, 3.0f);
        const auto active = button.getToggleState();
        const auto hover = animatedHover (&button, isMouseOver || isMouseDown, 14.0f);
        const auto lit = animatedHover (reinterpret_cast<const char*> (&button) + 1, active, 10.0f);

        if (hover > 0.01f && ! active)
        {
            g.setColour (IlanaTheme::Ui::hover.withAlpha (0.6f * hover));
            g.fillRoundedRectangle (bounds, 5.0f);
        }

        g.setColour (IlanaTheme::Ui::text2.interpolatedWith (IlanaTheme::Ui::text, juce::jmax (lit, hover * 0.6f)));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, active));
        g.drawText (button.getButtonText(), bounds, juce::Justification::centred);

        // The underline grows out from the centre and glows.
        if (lit > 0.01f)
        {
            const auto width = (bounds.getWidth() - 16.0f) * lit;
            const auto bar = juce::Rectangle<float> (width, 2.0f).withCentre ({ bounds.getCentreX(), bounds.getBottom() - 1.0f });
            IlanaTheme::paintGlow (g, bar, 1.0f, IlanaTheme::accent(), lit * 1.4f);
            g.setColour (IlanaTheme::accent());
            g.fillRoundedRectangle (bar, 1.0f);
        }

        if ((hover > 0.01f && hover < 0.99f) || (lit > 0.01f && lit < 0.99f))
            button.repaint();
    }

    // Check boxes (the tour's "Don't show this again") in the theme's font
    // and colours; the stock one drew its text in the default typeface.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool) override
    {
        const auto box = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ 13.0f, (float) button.getHeight() * 0.5f });
        g.setColour (highlighted ? IlanaTheme::Ui::hover : IlanaTheme::Ui::raised);
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (button.getToggleState() ? IlanaTheme::accent() : IlanaTheme::Ui::text3);
        g.drawRoundedRectangle (box, 4.0f, 1.4f);

        if (button.getToggleState())
        {
            juce::Path tick;
            tick.startNewSubPath (box.getX() + 4.5f, box.getCentreY());
            tick.lineTo (box.getX() + 7.8f, box.getBottom() - 5.0f);
            tick.lineTo (box.getRight() - 4.0f, box.getY() + 5.0f);
            g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour (button.findColour (juce::ToggleButton::textColourId));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText (button.getButtonText(), button.getLocalBounds().withTrimmedLeft (30), juce::Justification::centredLeft);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        using namespace IlanaTheme;

        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto hover = animatedHover (&slider, slider.isMouseOver() || slider.isMouseButtonDown(), 12.0f);
        const auto radius = juce::jlimit (14.0f, 30.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f);
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto lineWidth = juce::jmax (2.5f, radius * 0.13f);
        const auto arcRadius = radius - lineWidth * 0.5f;
        const auto enabled = slider.isEnabled();

        // Recent movement makes the arc flare, then settle.
        const auto flare = valueFlare (&slider, sliderPos, slider.isMouseOverOrDragging());

        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (Ui::track.interpolatedWith (juce::Colours::white, 0.04f * hover));
        if (slider.getProperties().getWithDefault ("notModulatable", false))
        {
            // A knob no source can drive: a dotted track (UI review 7, I7-5).
            juce::Path dotted;
            const float dashes[] { 1.0f, lineWidth * 1.15f };
            juce::PathStrokeType (lineWidth * 0.85f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createDashedStroke (dotted, backgroundArc, dashes, 2);
            g.fillPath (dotted);
        }
        else
        {
            g.strokePath (backgroundArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Pages dim controls that do nothing right now (a section switched
        // off) by fading their control; those arcs go grey too, so an idle
        // knob never looks live. Only the knob's own wrappers count, not the
        // page fading in.
        // (Wrappers are small; a page, card or panel is wide, and its alpha
        // is a transition, so the walk stops before reading one.)
        auto dimmed = false;
        for (auto* c = slider.getParentComponent(); c != nullptr && ! dimmed; c = c->getParentComponent())
        {
            if (c->getWidth() > 200 || dynamic_cast<juce::Viewport*> (c) != nullptr)
                break;
            dimmed = c->getAlpha() < 0.99f;
        }

        const auto accent = enabled && ! dimmed ? slider.findColour (juce::Slider::rotarySliderFillColourId)
                                                : Ui::text3;

        // Bipolar parameters (pan, fine, bend...) fill from the centre.
        auto originPos = 0.0f;

        if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
            originPos = (float) slider.valueToProportionOfLength (0.0);

        const auto originAngle = rotaryStartAngle + originPos * (rotaryEndAngle - rotaryStartAngle);

        if (std::abs (sliderPos - originPos) > 0.001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                    juce::jmin (originAngle, angle), juce::jmax (originAngle, angle), true);

            // Glow under the arc: brighter on hover and while it moves.
            const auto glow = 0.10f + 0.10f * hover + 0.22f * flare;
            g.setColour (accent.withAlpha (glow * 0.5f));
            g.strokePath (valueArc, juce::PathStrokeType (lineWidth * 3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent.withAlpha (glow));
            g.strokePath (valueArc, juce::PathStrokeType (lineWidth * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour (accent.interpolatedWith (juce::Colours::white, 0.25f * flare));
            g.strokePath (valueArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Flat cap with a pointer line; a soft ring lights on hover.
        const auto bodyRadius = arcRadius - lineWidth * 1.4f;
        const auto bodyBounds = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);

        g.setColour (Ui::raised.interpolatedWith (juce::Colours::white, 0.04f * hover));
        g.fillEllipse (bodyBounds);
        g.setColour (Ui::line.interpolatedWith (accent, 0.45f * hover));
        g.drawEllipse (bodyBounds.reduced (0.5f), 1.0f);

        const auto dir = juce::Point<float> (std::cos (angle - juce::MathConstants<float>::halfPi),
                                             std::sin (angle - juce::MathConstants<float>::halfPi));
        const auto p1 = centre + dir * (bodyRadius * 0.28f);
        const auto p2 = centre + dir * (bodyRadius * 0.86f);
        g.setColour (enabled ? Ui::text : Ui::text3);
        g.drawLine ({ p1, p2 }, juce::jmax (1.8f, radius * 0.085f));

        // The value's tip on the arc, drawn even at the origin so a knob at
        // zero still reads as live.
        {
            const auto tip = centre + dir * arcRadius;
            const auto atOrigin = std::abs (sliderPos - originPos) <= 0.001f;
            g.setColour (accent.withAlpha ((atOrigin ? 0.12f : 0.22f) + 0.25f * flare));
            g.fillEllipse (juce::Rectangle<float> (lineWidth * 3.6f, lineWidth * 3.6f).withCentre (tip));
            g.setColour (atOrigin ? accent.withAlpha (0.7f) : accent.interpolatedWith (juce::Colours::white, 0.55f));
            g.fillEllipse (juce::Rectangle<float> (lineWidth * 1.3f, lineWidth * 1.3f).withCentre (tip));
        }

        if (flare > 0.01f || (hover > 0.01f && hover < 0.99f))
            slider.repaint(); // keep the flare and hover easing
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        using namespace IlanaTheme;

        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const auto hover = animatedHover (&button, shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown, 14.0f);

        if (hover > 0.01f && hover < 0.99f)
            button.repaint();

        if (button.getProperties().contains ("pill"))
        {
            paintPill (g, bounds, button.getButtonText(), button.findColour (juce::TextButton::buttonOnColourId).withAlpha (1.0f),
                       button.getToggleState(), hover, button.isEnabled());
            return;
        }

        if (button.getProperties().contains ("switch"))
        {
            // Switches without an animation driver just show their state.
            const auto amount = button.getProperties().contains ("switchAmount")
                                    ? (float) button.getProperties()["switchAmount"]
                                    : (button.getToggleState() ? 1.0f : 0.0f);
            // Centred in its space, under its name when it has one.
            paintSwitch (g, bounds, amount, accent(), hover);
            return;
        }

        const auto on = button.getToggleState();
        const auto onColour = button.findColour (juce::TextButton::buttonOnColourId).withAlpha (1.0f);
        const auto radius = juce::jmin (5.0f, bounds.getHeight() * 0.3f);

        if (on && button.isEnabled())
            paintGlow (g, bounds, radius, onColour, 0.7f + 0.5f * hover);

        // Lit buttons are a tinted fill with a bright edge; others are flat.
        auto fill = on ? Ui::raised.interpolatedWith (onColour, 0.28f) : backgroundColour;
        fill = fill.interpolatedWith (juce::Colours::white, hover * 0.06f);

        if (shouldDrawButtonAsDown)
            fill = fill.darker (0.2f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, radius);

        g.setColour (on ? onColour.withAlpha (0.75f)
                        : Ui::line.interpolatedWith (accent(), 0.5f * hover));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool down) override
    {
        if (button.getProperties().contains ("switch") || button.getProperties().contains ("pill"))
            return;

        juce::ignoreUnused (highlighted, down);
        g.setFont (getTextButtonFont (button, button.getHeight()));

        if (button.getToggleState() && button.isEnabled())
        {
            const auto onColour = button.findColour (juce::TextButton::buttonOnColourId).withAlpha (1.0f);
            g.setColour (onColour.interpolatedWith (juce::Colours::white, 0.55f));
            IlanaTheme::drawFitted (g, button.getButtonText(), button.getLocalBounds().reduced (4, 2),
                                    juce::Justification::centred, 1, IlanaTheme::TextSize::minInteractive);
            return;
        }

        // The stock text's insets (LookAndFeel_V2), drawn through the one
        // fitting rule instead of its condensed two-line fit.
        const auto yIndent = juce::jmin (4, button.proportionOfHeight (0.3f));
        const auto cornerSize = juce::jmin (button.getHeight(), button.getWidth()) / 2;
        const auto fontHeight = juce::roundToInt (getTextButtonFont (button, button.getHeight()).getHeight() * 0.6f);
        const auto leftIndent = juce::jmin (fontHeight, 2 + cornerSize / (button.isConnectedOnLeft() ? 4 : 2));
        const auto rightIndent = juce::jmin (fontHeight, 2 + cornerSize / (button.isConnectedOnRight() ? 4 : 2));
        g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                         .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
        IlanaTheme::drawFitted (g, button.getButtonText(),
                                juce::Rectangle<int> (leftIndent, yIndent, button.getWidth() - leftIndent - rightIndent, button.getHeight() - yIndent * 2),
                                juce::Justification::centred, 1, IlanaTheme::TextSize::minInteractive);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int, int, int, int, juce::ComboBox& box) override
    {
        using namespace IlanaTheme;

        const auto bounds = juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f);
        const auto hover = animatedHover (&box, box.isMouseOver() || isButtonDown, 14.0f);

        if (hover > 0.01f && hover < 0.99f)
            box.repaint();
        const auto radius = juce::jmin (5.0f, bounds.getHeight() * 0.3f);

        g.setColour (box.findColour (juce::ComboBox::backgroundColourId).interpolatedWith (juce::Colours::white, 0.05f * hover));
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId).interpolatedWith (accent(), 0.6f * hover));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

        // A small chevron, turning accent on hover.
        const auto size = juce::jmin (7.0f, (float) height * 0.28f);
        const auto c = juce::Point<float> ((float) width - 11.0f, (float) height * 0.5f);
        juce::Path chevron;
        chevron.startNewSubPath (c.x - size * 0.6f, c.y - size * 0.3f);
        chevron.lineTo (c.x, c.y + size * 0.3f);
        chevron.lineTo (c.x + size * 0.6f, c.y - size * 0.3f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId).interpolatedWith (accent(), hover)
                        .withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.5f));
        g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (1, 1, box.getWidth() - 20, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    // The value words that mean "no setting of its own" (the knob's
    // automatic or free state), drawn dim under a knob.
    static bool isPlaceholderValue (const juce::String& text)
    {
        // ("Auto" and "Random" are settings and read in the value colour: review 11, S11-17.)
        return text == "Free" || text == "Off" || text == "None";
    }

    // Labels (slider values included) never draw a box: a value reads as
    // text under its knob on every page.
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll (label.findColour (juce::Label::backgroundColourId));

        if (! label.isBeingEdited())
        {
            // A disabled name is never fainter than 3:1 against its card
            // (V9-25): its own fade and its card's dimming are counted
            // together, and the label gives back what they take.
            auto shown = 1.0f;
            for (const juce::Component* c = &label; c != nullptr; c = c->getParentComponent())
                shown *= c->getAlpha();
            const auto alpha = label.isEnabled() ? 1.0f : juce::jlimit (0.5f, 1.0f, 0.55f / juce::jmax (0.01f, shown));
            auto font = getLabelFont (label);
            // A row of names shares one size (layoutRow sets the cap).
            if (label.getProperties().contains ("fitCap"))
                font = font.withHeight (juce::jmin (font.getHeight(), (float) label.getProperties()["fitCap"]));
            // A word standing in for a number ("Auto", "Free", "Off") reads
            // dim, as a default rather than a setting (UI review 9, I9-25).
            const auto placeholder = dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr
                                     && isPlaceholderValue (label.getText());
            g.setColour ((placeholder ? IlanaTheme::Ui::text3 : label.findColour (juce::Label::textColourId)).withMultipliedAlpha (alpha)); // (dimmer than a number: I10-15)
            g.setFont (font);
            const auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
            // Never condensed (V8-12): shrunk to the floor, then cut. A value
            // or a combo's text is clicked, so it keeps the interactive floor.
            const auto interactive = label.isEditable() || dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr
                                     || dynamic_cast<juce::ComboBox*> (label.getParentComponent()) != nullptr;
            IlanaTheme::drawFitted (g, label.getText(), textArea, label.getJustificationType(),
                                    juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                                    interactive ? IlanaTheme::TextSize::minInteractive : IlanaTheme::TextSize::minPassive);
        }
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.fillAll (IlanaTheme::Ui::raised);
        g.setColour (IlanaTheme::Ui::line);
        g.drawRect (0, 0, width, height, 1);
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override
    {
        auto thumb = isScrollbarVertical ? juce::Rectangle<int> (x, thumbStartPosition, width, thumbSize)
                                         : juce::Rectangle<int> (thumbStartPosition, y, thumbSize, height);
        thumb = isScrollbarVertical ? thumb.reduced (juce::jmax (1, width / 2 - 2), 2) : thumb.reduced (2, juce::jmax (1, height / 2 - 2));

        g.setColour ((isMouseOver || isMouseDown) ? IlanaTheme::Ui::text3 : IlanaTheme::Ui::track);
        g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
            return;
        }

        const auto trackHeight = 4.0f;
        const auto centreY = (float) y + (float) height * 0.5f;
        const juce::Rectangle<float> track ((float) x, centreY - trackHeight * 0.5f, (float) width, trackHeight);

        g.setColour (IlanaTheme::Ui::track);
        g.fillRoundedRectangle (track, trackHeight * 0.5f);

        // Bipolar ranges fill outwards from zero, so a centred value reads as "none".
        const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const auto originX = bipolar ? (float) x + (float) width * (float) slider.valueToProportionOfLength (0.0)
                                     : (float) x;
        const auto thumbPos = juce::jlimit ((float) x, (float) (x + width), sliderPos);
        const auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (originX, thumbPos), track.getY(),
                                                                     juce::jmax (originX, thumbPos), track.getBottom());

        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const auto hover = animatedHover (&slider, slider.isMouseOver() || slider.isMouseButtonDown(), 12.0f);

        if (fill.getWidth() > 0.5f)
        {
            IlanaTheme::paintGlow (g, fill, trackHeight * 0.5f, accent, 0.8f + 0.6f * hover);
            g.setColour (accent);
            g.fillRoundedRectangle (fill, trackHeight * 0.5f);
        }

        const auto thumbX = juce::jlimit ((float) x, (float) (x + width), sliderPos);
        const auto thumb = juce::Rectangle<float> (11.0f + 2.0f * hover, 11.0f + 2.0f * hover).withCentre ({ thumbX, centreY });
        g.setColour (accent.withAlpha (0.25f * hover));
        g.fillEllipse (thumb.expanded (4.0f));
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.fillEllipse (thumb);

        if (hover > 0.01f && hover < 0.99f)
            slider.repaint();
    }

private:
    struct HoverState
    {
        float value = 0.0f;
        double lastTime = 0.0;
    };

    static std::unordered_map<const void*, HoverState>& hoverStates()
    {
        static std::unordered_map<const void*, HoverState> states;
        return states;
    }

    float animatedHover (const void* key, bool isOver, float rate)
    {
        auto& states = hoverStates();
        const auto now = juce::Time::getMillisecondCounterHiRes();

        // Keyed by component address and never told when one is deleted:
        // drop entries not painted for a minute once there are many, so the
        // map doesn't grow with every editor opened.
        static double lastSweep = 0.0;

        if (states.size() > 2048 && now - lastSweep > 1000.0)
        {
            lastSweep = now;
            for (auto it = states.begin(); it != states.end();)
                it = now - it->second.lastTime > 60000.0 ? states.erase (it) : std::next (it);
        }

        auto& state = states[key];
        // Not painted for seconds (or a new component at a reused address):
        // start from where it should be rather than from a stale glow.
        if (state.lastTime > 0.0 && now - state.lastTime > 2000.0)
            state.value = isOver ? 1.0f : 0.0f;
        const auto dt = state.lastTime > 0.0 ? juce::jlimit (0.0, 0.1, (now - state.lastTime) * 0.001) : 0.016;
        state.lastTime = now;

        const auto target = isOver ? 1.0f : 0.0f;
        state.value += (target - state.value) * juce::jlimit (0.0f, 1.0f, (float) (dt * (double) rate));

        return state.value;
    }

    // 1 just after the user moves a knob, easing back to 0 over about half a
    // second; keyed like animatedHover. Preset loads, undo and automation
    // move knobs too but don't flare.
    static float valueFlare (const void* key, float position, bool byUser)
    {
        struct Flare { float last = -1.0f; float value = 0.0f; double time = 0.0; };
        static std::unordered_map<const void*, Flare> flares;
        const auto now = juce::Time::getMillisecondCounterHiRes();

        // Drop entries not painted for a minute once there are many.
        static double lastSweep = 0.0;

        if (flares.size() > 2048 && now - lastSweep > 1000.0)
        {
            lastSweep = now;
            for (auto it = flares.begin(); it != flares.end();)
                it = now - it->second.time > 60000.0 ? flares.erase (it) : std::next (it);
        }

        auto& flare = flares[key];
        const auto dt = flare.time > 0.0 ? juce::jlimit (0.0, 0.1, (now - flare.time) * 0.001) : 0.0;
        flare.time = now;

        if (byUser && flare.last >= 0.0f && std::abs (position - flare.last) > 0.0005f)
            flare.value = 1.0f;
        else
            flare.value = juce::jmax (0.0f, flare.value - (float) dt * 2.2f);

        flare.last = position;
        return flare.value;
    }

    juce::Typeface::Ptr regularTypeface;
    juce::Typeface::Ptr boldTypeface;
};
