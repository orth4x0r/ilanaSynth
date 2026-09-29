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
    inline const juce::Colour hover   { 0xff2a2e34 };
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
    static const juce::uint32 colours[] { 0xfff5c542, 0xff5b8cff, 0xff4fd1a5, 0xffb28aff, 0xffff7fb0, 0xff48d4e8 };
    return juce::Colour (colours[(size_t) juce::jlimit (0, 5, index)]);
}

// The type scale (component units; the editor zooms them). Every text in the
// UI uses one of these, so sizes stay consistent from page to page.
namespace TextSize
{
    inline constexpr float tiny    = 10.5f; // axis ticks, badges, hints
    inline constexpr float label   = 11.5f; // knob and field labels
    inline constexpr float body    = 13.0f; // values, menus, buttons
    inline constexpr float title   = 14.5f; // tabs, section titles
    inline constexpr float large   = 16.5f; // preset name
    inline constexpr float display = 21.0f;
    inline constexpr float hero    = 26.0f;
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
// Digits are tabular, so a value doesn't shift sideways while it changes.
inline juce::FontOptions font (float height, bool bold = false)
{
    const auto scale = juce::jmax (0.25f, uiScaleRef());
    const auto deviceHeight = height * scale;
    const auto snapped = std::round (deviceHeight) / scale;
    const auto typeface = bold ? boldTypefaceRef()
                               : (deviceHeight < 15.0f ? mediumTypefaceRef() : regularTypefaceRef());

    return juce::FontOptions().withHeight (snapped)
                              .withStyle (bold ? "Bold" : "Regular")
                              .withTypeface (typeface)
                              .withFeatureEnabled ("tnum");
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

// An on/off switch: a pill with a sliding knob. `amount` runs 0 (off) to 1
// (on) so callers can animate it; the on state glows in `colour`.
inline void paintSwitch (juce::Graphics& g, juce::Rectangle<float> area, float amount, juce::Colour colour, float hover = 0.0f)
{
    const auto h = juce::jmin (area.getHeight(), 18.0f);
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

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::Font (IlanaTheme::font (juce::jmin (IlanaTheme::TextSize::body, (float) box.getHeight() * 0.72f)));
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
        return juce::Font (IlanaTheme::font (current.getHeight(), current.isBold()));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (IlanaTheme::font (juce::jmin (IlanaTheme::TextSize::body, (float) buttonHeight * 0.72f)));
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
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
        const auto flare = valueFlare (&slider, sliderPos);

        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (Ui::track.interpolatedWith (juce::Colours::white, 0.04f * hover));
        g.strokePath (backgroundArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto accent = enabled ? slider.findColour (juce::Slider::rotarySliderFillColourId)
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

        if (button.getProperties().contains ("switch"))
        {
            const auto amount = (float) button.getProperties()["switchAmount"];
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
        if (button.getProperties().contains ("switch"))
            return;

        if (button.getToggleState() && button.isEnabled())
        {
            const auto onColour = button.findColour (juce::TextButton::buttonOnColourId).withAlpha (1.0f);
            g.setFont (getTextButtonFont (button, button.getHeight()));
            g.setColour (onColour.interpolatedWith (juce::Colours::white, 0.55f));
            g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 2),
                              juce::Justification::centred, 1, 0.8f);
            return;
        }

        LookAndFeel_V4::drawButtonText (g, button, highlighted, down);
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

    // Labels (slider values included) never draw a box: a value reads as
    // text under its knob on every page.
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll (label.findColour (juce::Label::backgroundColourId));

        if (! label.isBeingEdited())
        {
            const auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            const auto font = getLabelFont (label);
            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
            g.setFont (font);
            const auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
            g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                              juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                              label.getMinimumHorizontalScale());
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
        if (states.size() > 2048)
            for (auto it = states.begin(); it != states.end();)
                it = now - it->second.lastTime > 60000.0 ? states.erase (it) : std::next (it);

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

    // 1 just after a knob's value moves, easing back to 0 over about half a
    // second; keyed like animatedHover.
    static float valueFlare (const void* key, float position)
    {
        struct Flare { float last = -1.0f; float value = 0.0f; double time = 0.0; };
        static std::unordered_map<const void*, Flare> flares;

        if (flares.size() > 2048)
            flares.clear();

        auto& flare = flares[key];
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto dt = flare.time > 0.0 ? juce::jlimit (0.0, 0.1, (now - flare.time) * 0.001) : 0.0;
        flare.time = now;

        if (flare.last >= 0.0f && std::abs (position - flare.last) > 0.0005f)
            flare.value = 1.0f;
        else
            flare.value = juce::jmax (0.0f, flare.value - (float) dt * 2.2f);

        flare.last = position;
        return flare.value;
    }

    juce::Typeface::Ptr regularTypeface;
    juce::Typeface::Ptr boldTypeface;
};
