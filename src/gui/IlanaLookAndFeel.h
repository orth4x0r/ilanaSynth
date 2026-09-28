#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <BinaryData.h>

#include <cmath>
#include <unordered_map>

namespace IlanaTheme
{
inline juce::Colour& accentRef()
{
    static juce::Colour accent (0xffff5b2b);
    return accent;
}

inline juce::Colour accent() { return accentRef(); }

inline const juce::uint32 palette[]
{
    0xffff5b2b, 0xff35c8ff, 0xff8fff3b, 0xffff4fd8
};

inline constexpr int numPalettes = 4;

// The fonts and textures, released when JUCE shuts down (after the last
// editor closes) rather than as statics when the plugin is unloaded: by then
// JUCE's font engine is gone, and freeing a FreeType face after it crashed
// hosts on exit.
struct ThemeResources : private juce::DeletedAtShutdown
{
    // Book (400) rather than Medium: lighter strokes stay crisp at UI sizes.
    juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (BinaryData::Jost400Book_ttf, (size_t) BinaryData::Jost400Book_ttfSize);
    // Used for small text: unhinted outlines thin out at tiny sizes, so the
    // medium weight keeps stems readable.
    juce::Typeface::Ptr medium = juce::Typeface::createSystemTypefaceFor (BinaryData::Jost500Medium_ttf, (size_t) BinaryData::Jost500Medium_ttfSize);
    juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor (BinaryData::Jost700Bold_ttf, (size_t) BinaryData::Jost700Bold_ttfSize);
    juce::Image leather, metal; // built on first use

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
inline juce::FontOptions font (float height, bool bold = false)
{
    const auto scale = juce::jmax (0.25f, uiScaleRef());
    const auto deviceHeight = height * scale;
    const auto snapped = std::round (deviceHeight) / scale;
    const auto typeface = bold ? boldTypefaceRef()
                               : (deviceHeight < 12.0f ? mediumTypefaceRef() : regularTypefaceRef());

    return juce::FontOptions().withHeight (snapped)
                              .withStyle (bold ? "Bold" : "Regular")
                              .withTypeface (typeface);
}

inline juce::Image& leatherTexture()
{
    auto& image = ThemeResources::getInstance()->leather;
    if (image.isNull()) image = []
    {
        constexpr int size = 512;
        juce::Image img (juce::Image::ARGB, size, size, true);
        juce::Graphics g (img);
        juce::Random random (0x1ea7be2);

        for (int i = 0; i < 130000; ++i)
        {
            const auto x = random.nextFloat() * (float) size;
            const auto y = random.nextFloat() * (float) size;
            const auto w = 0.8f + random.nextFloat() * 1.6f;
            const auto h = 0.8f + random.nextFloat() * 1.6f;
            const auto alpha = 0.015f + random.nextFloat() * 0.045f;
            const auto colour = (random.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (alpha);

            g.setColour (colour);
            g.fillRect (x, y, w, h);

            // Wrap the dots across the edges so the tile has no visible seam.
            const auto wrapX = x + w > (float) size;
            const auto wrapY = y + h > (float) size;

            if (wrapX)
                g.fillRect (x - (float) size, y, w, h);

            if (wrapY)
                g.fillRect (x, y - (float) size, w, h);

            if (wrapX && wrapY)
                g.fillRect (x - (float) size, y - (float) size, w, h);
        }

        return img;
    }();

    return image;
}

inline juce::Image& metalTexture()
{
    auto& image = ThemeResources::getInstance()->metal;
    if (image.isNull()) image = []
    {
        constexpr int w = 1024;
        constexpr int h = 256;
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);
        juce::Random random (0x9e7a13);

        for (int y = 0; y < h; ++y)
        {
            for (int i = 0; i < 280; ++i)
            {
                const auto x = random.nextFloat() * (float) w;
                const auto length = 8.0f + random.nextFloat() * 180.0f;
                const auto alpha = 0.012f + random.nextFloat() * 0.028f;
                const auto colour = (random.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (alpha);

                g.setColour (colour);
                g.fillRect (x, (float) y, length, 1.0f);

                if (x + length > (float) w)
                    g.fillRect (x - (float) w, (float) y, length, 1.0f);
            }
        }

        return img;
    }();

    return image;
}

inline void paintPageBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::ColourGradient base (juce::Colour (0xff1c1c21), (float) bounds.getX(), (float) bounds.getY(),
                               juce::Colour (0xff111114), (float) bounds.getX(), (float) bounds.getBottom(), false);
    g.setGradientFill (base);
    g.fillAll();

    g.setTiledImageFill (leatherTexture(), bounds.getX(), bounds.getY(), 0.9f);
    g.fillAll();

    juce::ColourGradient vignette (juce::Colours::transparentBlack,
                                   (float) bounds.getCentreX(), (float) bounds.getCentreY(),
                                   juce::Colours::black.withAlpha (0.3f),
                                   (float) bounds.getCentreX() + (float) bounds.getWidth() * 0.75f,
                                   (float) bounds.getCentreY(), true);
    g.setGradientFill (vignette);
    g.fillAll();

    // Stitched seam around the leather panel.
    juce::Path stitch;
    stitch.addRoundedRectangle (bounds.toFloat().reduced (9.0f), 10.0f);

    juce::Path dashed;
    const float dashes[] { 6.0f, 7.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, stitch, dashes, 2);
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.fillPath (dashed);
}

inline void paintCard (juce::Graphics& g, juce::Rectangle<float> bounds, float radius, juce::Colour tint)
{
    // Matte phenolic panel: soft sheen, no brushed streaks.
    juce::ColourGradient panel (juce::Colour (0xff2b2b33), bounds.getX(), bounds.getY(),
                                juce::Colour (0xff1a1a1f), bounds.getX(), bounds.getBottom(), false);
    panel.addColour (0.06, juce::Colour (0xff30303a));
    panel.addColour (0.5, juce::Colour (0xff232329));
    g.setGradientFill (panel);
    g.fillRoundedRectangle (bounds, radius);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, radius);
        g.reduceClipRegion (clip);

        const auto sheen = bounds.withHeight (bounds.getHeight() * 0.4f);
        juce::ColourGradient sheenGradient (juce::Colours::white.withAlpha (0.05f), sheen.getX(), sheen.getY(),
                                            juce::Colours::white.withAlpha (0.0f), sheen.getX(), sheen.getBottom(), false);
        g.setGradientFill (sheenGradient);
        g.fillRect (sheen);

        const auto shade = bounds.withTop (bounds.getBottom() - bounds.getHeight() * 0.25f);
        juce::ColourGradient shadeGradient (juce::Colours::transparentBlack, shade.getX(), shade.getY(),
                                            juce::Colours::black.withAlpha (0.16f), shade.getX(), shade.getBottom(), false);
        g.setGradientFill (shadeGradient);
        g.fillRect (shade);
    }

    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawLine (bounds.getX() + radius, bounds.getY() + 1.0f,
                bounds.getRight() - radius, bounds.getY() + 1.0f, 1.0f);

    g.setColour (tint.withAlpha (0.28f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
}

inline void paintRecessedPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    // Recessed bay the controls are mounted in.
    juce::ColourGradient recess (juce::Colour (0xff141418), bounds.getX(), bounds.getY(),
                                 juce::Colour (0xff1f1f26), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (recess);
    g.fillRoundedRectangle (bounds, radius);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine (bounds.getX() + radius, bounds.getY() + 0.8f,
                bounds.getRight() - radius, bounds.getY() + 0.8f, 1.4f);

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawLine (bounds.getX() + radius, bounds.getBottom() - 0.8f,
                bounds.getRight() - radius, bounds.getBottom() - 0.8f, 1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
}

inline void paintWell (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    juce::ColourGradient glass (juce::Colour (0xff0b0b0f), bounds.getX(), bounds.getY(),
                                juce::Colour (0xff16161c), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (bounds, radius);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, radius);
        g.reduceClipRegion (clip);

        // Top gloss.
        const auto gloss = bounds.withHeight (bounds.getHeight() * 0.45f);
        juce::ColourGradient glossGradient (juce::Colours::white.withAlpha (0.085f), gloss.getX(), gloss.getY(),
                                            juce::Colours::white.withAlpha (0.0f), gloss.getX(), gloss.getBottom(), false);
        g.setGradientFill (glossGradient);
        g.fillRect (gloss);

        // Diagonal specular streak.
        juce::Path streak;
        streak.addQuadrilateral (bounds.getX() - bounds.getWidth() * 0.25f, bounds.getBottom(),
                                 bounds.getX() + bounds.getWidth() * 0.2f, bounds.getBottom(),
                                 bounds.getX() + bounds.getWidth() * 0.5f, bounds.getY(),
                                 bounds.getX() + bounds.getWidth() * 0.05f, bounds.getY());
        // Big panels get a fainter streak, so it reads as glass, not a stripe.
        g.setColour (juce::Colours::white.withAlpha (0.035f * juce::jlimit (0.3f, 1.0f, 240.0f / bounds.getHeight())));
        g.fillPath (streak);

        // Inner shadow from the top edge.
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.45f), bounds.getX(), bounds.getY(),
                                    juce::Colours::transparentBlack, bounds.getX(),
                                    bounds.getY() + bounds.getHeight() * 0.3f, false);
        g.setGradientFill (inner);
        g.fillRect (bounds.withHeight (bounds.getHeight() * 0.3f));
    }

    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
}

inline void paintGlassOverlay (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addRoundedRectangle (bounds, radius);
    g.reduceClipRegion (clip);

    const auto gloss = bounds.withHeight (bounds.getHeight() * 0.35f);
    juce::ColourGradient glossGradient (juce::Colours::white.withAlpha (0.05f), gloss.getX(), gloss.getY(),
                                        juce::Colours::white.withAlpha (0.0f), gloss.getX(), gloss.getBottom(), false);
    g.setGradientFill (glossGradient);
    g.fillRect (gloss);

    juce::Path streak;
    streak.addQuadrilateral (bounds.getX() - bounds.getWidth() * 0.25f, bounds.getBottom(),
                             bounds.getX() + bounds.getWidth() * 0.2f, bounds.getBottom(),
                             bounds.getX() + bounds.getWidth() * 0.5f, bounds.getY(),
                             bounds.getX() + bounds.getWidth() * 0.05f, bounds.getY());
    g.setColour (juce::Colours::white.withAlpha (0.025f * juce::jlimit (0.3f, 1.0f, 240.0f / bounds.getHeight())));
    g.fillPath (streak);
}

inline void paintScrew (juce::Graphics& g, juce::Point<float> centre, float size)
{
    const auto bounds = juce::Rectangle<float> (size, size).withCentre (centre);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (bounds);

    juce::ColourGradient metal (juce::Colour (0xff565663), centre.x - size * 0.25f, centre.y - size * 0.3f,
                                juce::Colour (0xff23232b), centre.x + size * 0.3f, centre.y + size * 0.3f, true);
    g.setGradientFill (metal);
    g.fillEllipse (bounds.reduced (0.8f));

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawLine (centre.x - size * 0.24f, centre.y + size * 0.06f,
                centre.x + size * 0.24f, centre.y - size * 0.06f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (bounds.reduced (0.8f), 0.8f);
}
} // namespace IlanaTheme

class IlanaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IlanaLookAndFeel()
    {
        regularTypeface = IlanaTheme::regularTypefaceRef();
        boldTypeface = IlanaTheme::boldTypefaceRef();

        if (regularTypeface != nullptr)
            setDefaultSansSerifTypeface (regularTypeface);

        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff141416));

        setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::thumbColourId, juce::Colours::white);

        setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));

        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1d1d22));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff33333a));
        setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.85f));

        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d1d22));
        setColour (juce::PopupMenu::textColourId, juce::Colours::white.withAlpha (0.9f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff232329));
        setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.7f));
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);

        setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.7f));

        setColour (juce::TabbedComponent::backgroundColourId, juce::Colour (0xff141416));
        setColour (juce::TabbedComponent::outlineColourId, juce::Colour (0xff26262b));

        setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colour (0xff26262b));
        setColour (juce::TabbedButtonBar::tabTextColourId, juce::Colours::white.withAlpha (0.45f));
        setColour (juce::TabbedButtonBar::frontTextColourId, juce::Colours::white);

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
        return juce::Font (IlanaTheme::font (juce::jmin (16.0f, (float) box.getHeight() * 0.72f)));
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
        return juce::Font (IlanaTheme::font (juce::jmin (16.0f, (float) buttonHeight * 0.72f)));
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (IlanaTheme::font (13.5f));
        return label;
    }

    void applyAccent()
    {
        const auto accent = IlanaTheme::accent();

        setColour (juce::Slider::rotarySliderFillColourId, accent);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.35f));
        setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.85f));
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::TabbedButtonBar::frontOutlineColourId, accent);
    }

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (2.0f, 3.0f);
        const auto active = button.getToggleState();

        if (active)
        {
            juce::ColourGradient tabGradient (IlanaTheme::accent().withAlpha (0.3f), 0.0f, bounds.getY(),
                                              IlanaTheme::accent().withAlpha (0.1f), 0.0f, bounds.getBottom(), false);
            g.setGradientFill (tabGradient);
        }
        else if (isMouseOver || isMouseDown)
        {
            juce::ColourGradient tabGradient (juce::Colour (0xff272730), 0.0f, bounds.getY(),
                                              juce::Colour (0xff1b1b21), 0.0f, bounds.getBottom(), false);
            g.setGradientFill (tabGradient);
        }
        else
        {
            juce::ColourGradient tabGradient (juce::Colour (0xff202028), 0.0f, bounds.getY(),
                                              juce::Colour (0xff17171b), 0.0f, bounds.getBottom(), false);
            g.setGradientFill (tabGradient);
        }

        g.fillRoundedRectangle (bounds, 5.0f);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (active ? 0.1f : 0.05f));
        g.drawLine (bounds.getX() + 4.0f, bounds.getY() + 1.4f, bounds.getRight() - 4.0f, bounds.getY() + 1.4f, 1.0f);

        if (active)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.16f));
            g.fillRoundedRectangle (bounds.reduced (2.0f, 4.0f), 6.0f);
        }

        g.setColour (active ? IlanaTheme::accent() : juce::Colours::white.withAlpha (0.45f));
        g.setFont (active ? IlanaTheme::font (14.0f, true)
                          : IlanaTheme::font (14.0f));
        g.drawText (button.getButtonText(), bounds, juce::Justification::centred);

        if (active)
        {
            g.setColour (IlanaTheme::accent());
            g.fillRoundedRectangle (bounds.withHeight (2.0f).withY (bounds.getBottom() - 2.0f), 1.0f);
        }
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto hover = animatedHover (&slider, slider.isMouseOver(), 12.0f);
        const auto radius = juce::jlimit (14.0f, 30.0f,
                                          juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f)
                            * (1.0f + 0.07f * hover);
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto lineWidth = juce::jmax (2.0f, radius * 0.13f);
        const auto arcRadius = radius - lineWidth * 0.5f;

        if (hover > 0.01f)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.07f * hover));
            g.fillEllipse (juce::Rectangle<float> (radius * 3.1f, radius * 3.1f).withCentre (centre));
        }

        const auto arcStart = rotaryStartAngle;
        const auto arcEnd = rotaryEndAngle;

        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                     arcStart, arcEnd, true);
        g.setColour (juce::Colour (0xff26262e));
        g.strokePath (backgroundArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));

        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

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

            // A soft glow under the arc, then the arc and its bright core.
            g.setColour (accent.withAlpha (0.10f + 0.08f * hover));
            g.strokePath (valueArc, juce::PathStrokeType (lineWidth * 2.6f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

            g.setColour (accent.withAlpha (0.85f));
            g.strokePath (valueArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

            g.setColour (accent.brighter (0.5f).withAlpha (0.9f));
            g.strokePath (valueArc, juce::PathStrokeType (lineWidth * 0.38f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
        }

        // The value's tip, drawn even at the origin so a knob at zero still
        // reads as live rather than switched off.
        {
            const auto atOrigin = std::abs (sliderPos - originPos) <= 0.001f;
            const auto tipX = centre.x + std::cos (angle - juce::MathConstants<float>::halfPi) * arcRadius;
            const auto tipY = centre.y + std::sin (angle - juce::MathConstants<float>::halfPi) * arcRadius;
            const juce::Point<float> tip (tipX, tipY);

            g.setColour (accent.withAlpha (atOrigin ? 0.14f : 0.25f));
            g.fillEllipse (juce::Rectangle<float> (lineWidth * 4.0f, lineWidth * 4.0f).withCentre (tip));
            g.setColour (atOrigin ? accent.withAlpha (0.75f) : accent.brighter (0.7f));
            g.fillEllipse (juce::Rectangle<float> (lineWidth * 1.4f, lineWidth * 1.4f).withCentre (tip));
        }

        const auto bodyRadius = radius * 0.62f;
        const auto bodyBounds = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (bodyBounds.translated (0.0f, 1.6f).expanded (0.5f));

        juce::ColourGradient body (juce::Colour (0xff4b4b58),
                                   centre.x - bodyRadius * 0.45f, centre.y - bodyRadius * 0.55f,
                                   juce::Colour (0xff18181e),
                                   centre.x + bodyRadius * 0.5f, centre.y + bodyRadius * 0.6f, true);
        body.addColour (0.55, juce::Colour (0xff2f2f3a));
        g.setGradientFill (body);
        g.fillEllipse (bodyBounds);

        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addEllipse (bodyBounds);
            g.reduceClipRegion (clip);

            // Knurling (density scales with size so small knobs stay clean).
            const auto ticks = juce::jlimit (24, 56, (int) (bodyRadius * 2.6f));

            for (int i = 0; i < ticks; ++i)
            {
                const auto a = (float) i / (float) ticks * juce::MathConstants<float>::twoPi;
                const auto inner = bodyRadius * 0.74f;
                const juce::Point<float> p1 (centre.x + std::cos (a) * inner, centre.y + std::sin (a) * inner);
                const juce::Point<float> p2 (centre.x + std::cos (a) * bodyRadius, centre.y + std::sin (a) * bodyRadius);

                g.setColour ((i % 2 == 0 ? juce::Colours::white : juce::Colours::black)
                                 .withAlpha (i % 2 == 0 ? 0.05f : 0.1f));
                g.drawLine ({ p1, p2 }, 1.0f);
            }

            // Specular highlight arc.
            juce::Path spec;
            spec.addCentredArc (centre.x, centre.y, bodyRadius * 0.84f, bodyRadius * 0.84f, 0.0f,
                                juce::MathConstants<float>::pi * 1.05f, juce::MathConstants<float>::pi * 1.55f, true);
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.strokePath (spec, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (bodyBounds.reduced (0.5f), 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawEllipse (bodyBounds.reduced (1.4f), 0.8f);

        juce::Path pointer;
        const auto pointerThickness = juce::jmax (1.6f, radius * 0.09f);
        pointer.addRoundedRectangle (-pointerThickness * 0.5f, -radius * 0.66f,
                                     pointerThickness, radius * 0.42f, pointerThickness * 0.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillPath (pointer, juce::AffineTransform::translation (0.0f, 1.0f));

        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.fillPath (pointer);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const auto hover = animatedHover (&button, shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown, 14.0f);

        // Soft rubber key: matte, sitting flush on the panel.
        const auto base = backgroundColour.interpolatedWith (juce::Colours::white, hover * 0.07f);

        juce::ColourGradient rubber (base.brighter (shouldDrawButtonAsDown ? 0.02f : 0.13f), bounds.getX(), bounds.getY(),
                                     base.darker (shouldDrawButtonAsDown ? 0.0f : 0.15f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (rubber);
        g.fillRoundedRectangle (bounds, 5.0f);

        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.05f + 0.06f * hover));
        g.drawLine (bounds.getX() + 4.0f, bounds.getY() + 1.4f, bounds.getRight() - 4.0f, bounds.getY() + 1.4f, 1.0f);

        if (shouldDrawButtonAsDown)
        {
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillRoundedRectangle (bounds.reduced (1.0f), 5.0f);
        }

        if (hover > 0.01f)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.3f * hover));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
        }
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override
    {
        LookAndFeel_V4::drawComboBox (g, width, height, isButtonDown, buttonX, buttonY, buttonW, buttonH, box);

        const auto hover = animatedHover (&box, box.isMouseOver(), 14.0f);

        if (hover > 0.01f)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.45f * hover));
            g.drawRoundedRectangle (juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f), 4.0f, 1.2f);
        }
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

        g.setColour (juce::Colour (0xff2c2c33));
        g.fillRoundedRectangle (track, trackHeight * 0.5f);

        // Bipolar ranges fill outwards from zero, so a centred value reads as "none".
        const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const auto originX = bipolar ? (float) x + (float) width * (float) slider.valueToProportionOfLength (0.0)
                                     : (float) x;
        const auto thumbPos = juce::jlimit ((float) x, (float) (x + width), sliderPos);
        const auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (originX, thumbPos), track.getY(),
                                                                     juce::jmax (originX, thumbPos), track.getBottom());

        if (fill.getWidth() > 0.5f)
        {
            g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
            g.fillRoundedRectangle (fill, trackHeight * 0.5f);
        }

        const auto thumbX = juce::jlimit ((float) x, (float) (x + width), sliderPos);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ thumbX, centreY }));
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

    juce::Typeface::Ptr regularTypeface;
    juce::Typeface::Ptr boldTypeface;
};
