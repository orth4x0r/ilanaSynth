#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "IlanaLookAndFeel.h"

// Small vector icons for the header, drawn as paths so they stay crisp at
// every zoom level (the Jost font has no arrow or gear glyphs).
namespace IlanaIcons
{
enum class Icon
{
    Undo,
    Redo,
    History,
    Gear,
    Dice,
    ChevronLeft,
    ChevronRight,
    Star,
    More,
    Save
};

// The mockup's icons (lib.js ICON), as its SVG paths on a 14 px grid (15
// for the dice and the gear): strokes 1.6 px (the save arrow 1.8), round
// ends, scaled to the box they are drawn in.
inline juce::Path make (Icon icon, juce::Rectangle<float> r)
{
    juce::Path stroked, filled;
    auto grid = 14.0f;

    const auto stroke = [&stroked] (const juce::Path& outline, float width)
    {
        juce::Path p;
        juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (p, outline);
        stroked.addPath (p);
    };
    const auto svg = [] (const char* d) { return juce::Drawable::parseSVGPath (d); };
    const auto circle = [] (float cx, float cy, float radius)
    {
        juce::Path p;
        p.addEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        return p;
    };

    switch (icon)
    {
        case Icon::Undo:
            stroke (svg ("M4 5H9a3.5 3.5 0 010 7H4M4 5l2.5-2.5M4 5l2.5 2.5"), 1.6f);
            break;

        case Icon::Redo:
            stroke (svg ("M10 5H5a3.5 3.5 0 000 7h5M10 5L7.5 2.5M10 5L7.5 7.5"), 1.6f);
            break;

        case Icon::History:
            stroke (circle (7.0f, 7.0f, 5.5f), 1.6f);
            stroke (svg ("M7 4v3.4l2.2 1.3"), 1.6f);
            break;

        case Icon::Gear:
            grid = 15.0f;
            stroke (circle (7.5f, 7.5f, 2.2f), 1.6f);
            stroke (svg ("M7.5 1v2M7.5 12v2M1 7.5h2M12 7.5h2M3 3l1.4 1.4M10.6 10.6L12 12M3 12l1.4-1.4M10.6 4.4L12 3"), 1.6f);
            break;

        case Icon::Dice:
        {
            grid = 15.0f;
            juce::Path box;
            box.addRoundedRectangle (1.5f, 1.5f, 12.0f, 12.0f, 3.0f);
            stroke (box, 1.6f);
            for (const auto pip : { juce::Point<float> (5.0f, 5.0f), juce::Point<float> (10.0f, 10.0f), juce::Point<float> (7.5f, 7.5f) })
                filled.addPath (circle (pip.x, pip.y, 1.1f));
            break;
        }

        case Icon::ChevronLeft:
            stroke (svg ("M8 4.5L5.5 7 8 9.5"), 1.3f); // (the sheet draws a small 13 px ‹)
            break;

        case Icon::ChevronRight:
            stroke (svg ("M6 4.5L8.5 7 6 9.5"), 1.3f);
            break;

        case Icon::Star:
            filled = svg ("M7 1l1.8 3.9 4.2.5-3.1 2.9.8 4.2L7 10.4 3.3 12.5l.8-4.2L1 5.4l4.2-.5z");
            break;

        case Icon::More:
            for (const auto x : { 2.5f, 7.0f, 11.5f })
                filled.addPath (circle (x, 7.0f, 1.3f));
            break;

        case Icon::Save:
            stroke (svg ("M7 1v8M3.5 6L7 9.5 10.5 6M1.5 12.5h11"), 1.8f);
            break;
    }

    stroked.addPath (filled);
    const auto side = juce::jmin (r.getWidth(), r.getHeight());
    const auto scale = side / grid;
    stroked.applyTransform (juce::AffineTransform::scale (scale).translated (r.getCentreX() - side * 0.5f, r.getCentreY() - side * 0.5f));
    return stroked;
}
} // namespace IlanaIcons

// A header key with an icon (and optional short text) instead of a word.
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, IlanaIcons::Icon iconIn, const juce::String& tooltip)
        : juce::Button (name), icon (iconIn)
    {
        setTooltip (tooltip);
    }

    void setText (const juce::String& newText)
    {
        text = newText;
        repaint();
    }

    // The one key that should read as the header's main action (SAVE): a
    // tinted accent fill and edge, white icon and text.
    void setEmphasis (bool shouldEmphasise)
    {
        emphasis = shouldEmphasise;
        repaint();
    }

    // No key background: the button sits inside another display (the
    // preset display's prev / next segments).
    void setFlat (bool shouldBeFlat)
    {
        flat = shouldBeFlat;
        repaint();
    }

    void setIconColour (std::optional<juce::Colour> colour)
    {
        iconColour = colour;
        repaint();
    }

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        using namespace IlanaTheme;

        const auto on = getToggleState();
        const auto enabled = isEnabled();
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

        // A disabled key fades as a whole, as the sheet's redo (.ib at 45 %).
        const auto fade = enabled ? 1.0f : 0.45f;

        if (emphasis && enabled)
        {
            // SAVE, the header's one main action (.btn.pri): the accent solid
            // with dark text, 8 px corners.
            g.setColour (accent().brighter (isHighlighted && ! isDown ? 0.12f : 0.0f).withMultipliedBrightness (isDown ? 0.9f : 1.0f));
            g.fillRoundedRectangle (bounds, 8.0f);
        }
        else if (! flat)
        {
            // The sheet's header key (.ib): 30 px, ink 2, a hairline, 8 px
            // corners; lit (.ib.on) the accent at 15 % with a half-accent rim.
            auto fill = on ? Ui::panel.interpolatedWith (accent(), 0.15f) : Ui::panel;
            fill = fill.interpolatedWith (Ui::hover, (isDown ? 0.8f : isHighlighted ? 0.5f : 0.0f));
            g.setColour (fill.withMultipliedAlpha (fade));
            g.fillRoundedRectangle (bounds, 8.0f);
            g.setColour ((on ? accent().withAlpha (0.5f) : Ui::line.interpolatedWith (Ui::text3, isHighlighted ? 0.4f : 0.0f)).withMultipliedAlpha (fade));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 7.5f, 1.0f);
        }
        else if (isHighlighted)
        {
            g.setColour (juce::Colours::white.withAlpha (isDown ? 0.1f : 0.05f));
            g.fillRect (getLocalBounds());
        }

        auto colour = iconColour.value_or (emphasis ? juce::Colour (0xff1a0b06) : on ? Ui::accent2 : Ui::text2);

        if (! enabled)
            colour = colour.withMultipliedAlpha (fade);
        else if (isHighlighted && ! emphasis)
            colour = colour.interpolatedWith (Ui::text, 0.7f);

        g.setColour (colour);

        if (text.isNotEmpty())
        {
            // Icon, 6 px, then the word, centred as one group (.btn: gap 6).
            const auto font = juce::Font (IlanaTheme::font (15.0f, true).withKerningFactor (0.04f));
            const auto textWidth = juce::GlyphArrangement::getStringWidth (font, text);
            const auto group = juce::jmin (bounds.getWidth() - 8.0f, 14.0f + 6.0f + textWidth);
            const auto x = bounds.getCentreX() - group * 0.5f;
            g.fillPath (IlanaIcons::make (icon, { x, bounds.getCentreY() - 7.0f, 14.0f, 14.0f }));
            g.setFont (font);
            g.drawText (text, juce::Rectangle<float> (x + 20.0f, bounds.getY(), group - 20.0f + 1.0f, bounds.getHeight()),
                        juce::Justification::centredLeft, false);
        }
        else
        {
            // The sheet's 14 px icon (15 for the dice and gear) in a 30 px key;
            // the flat prev / next arrows take 14 in their 28 px segments.
            const auto side = juce::jmin (bounds.getWidth(), bounds.getHeight(), (icon == IlanaIcons::Icon::Dice || icon == IlanaIcons::Icon::Gear) ? 15.0f : 14.0f);
            g.fillPath (IlanaIcons::make (icon, bounds.withSizeKeepingCentre (side, side)));
        }
    }

private:
    IlanaIcons::Icon icon;
    juce::String text;
    bool emphasis = false, flat = false;
    std::optional<juce::Colour> iconColour;
};

// SCOPE, KEYBOARD and ? in the tab bar: a small ghost button (clear fill,
// a thin rim, 6 px corners); lit with the accent while it is switched on.
class GhostButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto on = getToggleState();

        if (on)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.16f));
            g.fillRoundedRectangle (bounds, 6.0f);
        }
        else if (isHighlighted || isDown)
        {
            g.setColour (juce::Colours::white.withAlpha (isDown ? 0.1f : 0.05f));
            g.fillRoundedRectangle (bounds, 6.0f);
        }

        g.setColour (on ? IlanaTheme::accent().withAlpha (0.8f) : IlanaTheme::Ui::line.brighter (isHighlighted ? 0.3f : 0.0f));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true).withKerningFactor (0.04f));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }

    // The width its text needs.
    int getIdealWidth() const
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true).withKerningFactor (0.04f)), getButtonText()) + 20;
    }
};

// The preset name as the centrepiece of the header: an inset display with
// the category above the name. Clicking it opens the browser.
class PresetDisplay : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    PresetDisplay() { setTooltip ("Preset\nClick to browse presets.  Use the arrows to step through them."); }

    std::function<void()> onClick;

    void setPreset (const juce::String& newName, const juce::String& newCategory, bool favourite, bool modified)
    {
        if (newName == name && newCategory == category && favourite == isFavourite && modified == isModified)
            return;

        name = newName;
        category = newCategory;
        isFavourite = favourite;
        isModified = modified;
        repaint();
    }

    // A word in place of the category for a moment ("SAVED").
    void showNotice (const juce::String& text)
    {
        notice = text;
        repaint();
        const auto shown = ++noticeCount;
        juce::Timer::callAfterDelay (1800, [safeThis = juce::Component::SafePointer<PresetDisplay> (this), shown]
        {
            if (safeThis != nullptr && safeThis->noticeCount == shown)
            {
                safeThis->notice.clear();
                safeThis->repaint();
            }
        });
    }

    juce::String getNotice() const { return notice; }
    bool isShowingModified() const { return isModified; }

    void setFlash (float amount)
    {
        if (std::abs (amount - flash) > 0.005f)
        {
            flash = amount;
            repaint();
        }
    }

    // The two 28 px segments at the ends hold the prev / next buttons (the
    // editor lays them over this display); the live scope sits right of the
    // name, faint.
    static constexpr int arrowWidth = 28;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

        // (The field itself is painted by the editor, under the live wave.)
        g.setColour (hover ? IlanaTheme::accent().withAlpha (0.6f) : IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

        // A preset load flashes the field with a glow that fades out.
        if (flash > 0.01f)
        {
            IlanaTheme::paintGlow (g, bounds.reduced (1.0f), 10.0f, IlanaTheme::accent(), 2.0f * flash);
            g.setColour (IlanaTheme::accent().withAlpha (0.12f * flash));
            g.fillRoundedRectangle (bounds, 10.0f);
            g.setColour (IlanaTheme::accent().withAlpha (0.7f * flash));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.2f);
        }

        // The rules between the arrows and the name.
        g.setColour (IlanaTheme::Ui::line2);
        g.fillRect (juce::Rectangle<float> ((float) arrowWidth, 1.0f, 1.0f, bounds.getHeight() - 1.0f));
        g.fillRect (juce::Rectangle<float> ((float) (getWidth() - arrowWidth - 1), 1.0f, 1.0f, bounds.getHeight() - 1.0f));

        auto text = getLocalBounds().withTrimmedLeft (arrowWidth + 12).withTrimmedRight (arrowWidth + 8).reduced (0, 3);

        // Init's category is its own name: show it as the start-up label does.
        const auto categoryText = category.isNotEmpty() && ! category.equalsIgnoreCase ("Init") ? category.toUpperCase()
                                                                                                : name.isEmpty() || name.equalsIgnoreCase ("Init") ? juce::String ("NEW PATCH") // (I14-7: not "PRESET" over Init)
                                                                                                                                                      : juce::String ("PRESET");
        // "BASS · EDITED": the category and the edited mark in the accent, one line.
        auto categoryRow = text.removeFromTop (14);
        // (The sheet's .nm i: 9 px bold, tracked 0.1 em, always in the accent.)
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true).withKerningFactor (0.1f));
        g.setColour (IlanaTheme::accent());
        g.drawText (notice.isNotEmpty() ? notice : categoryText + (isModified ? juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 EDITED")) : juce::String()),
                    categoryRow, juce::Justification::centredLeft);

        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (nameSize, true));
        const auto nameText = name.isNotEmpty() ? name : juce::String ("Init");
        g.drawText (nameText, text, juce::Justification::centredLeft, true);

        if (isFavourite)
        {
            const auto nameWidth = juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (nameSize, true)), nameText);
            const auto starX = juce::jmin ((float) text.getRight() - 10.0f, (float) text.getX() + nameWidth + 12.0f);
            g.setColour (juce::Colour (0xffffd447));
            g.fillPath (IlanaIcons::make (IlanaIcons::Icon::Star,
                                          juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ starX, (float) text.getCentreY() })));
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mouseWasClicked() && onClick != nullptr)
            onClick();
    }

private:
    // The preset name is the header's centrepiece: a size up from the type
    // scale's "large".
    static constexpr float nameSize = 20.0f;
    juce::String name, category;
    bool isFavourite = false;
    bool isModified = false;
    juce::String notice;
    int noticeCount = 0;
    bool hover = false;
    float flash = 0.0f;
};

// The header's compare switch (A/B stays in the header, a settled
// decision), drawn compact as "A | B" with the version playing lit. The
// button's text is the side shown ("A" or "B"); a click flips it.
class ABButton : public juce::TextButton
{
public:
    ABButton() : juce::TextButton ("A") {}

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        // The sheet's A / B (.ab): a hairline box of 8 px corners on the
        // header's ink, the side playing filled with the accent edge to edge
        // (dark letter), the other a t3 letter.
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto radius = 8.0f;
        const auto onB = getButtonText() == "B";
        auto left = bounds;
        const auto right = left.removeFromRight (bounds.getWidth() * 0.5f);

        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (bounds, radius);
            g.reduceClipRegion (clip);

            if (isHighlighted || isDown)
            {
                g.setColour (juce::Colours::white.withAlpha (isDown ? 0.08f : 0.04f));
                g.fillRect (onB ? left : right);
            }

            g.setColour (IlanaTheme::accent());
            g.fillRect (onB ? right : left);
        }

        g.setColour (IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius - 0.5f, 1.0f);

        g.setFont (IlanaTheme::font (15.0f, true));
        const auto dark = juce::Colour (0xff1a0b06);
        g.setColour (onB ? IlanaTheme::Ui::text3.interpolatedWith (IlanaTheme::Ui::text, isHighlighted ? 0.5f : 0.0f) : dark);
        g.drawText ("A", left.toNearestInt(), juce::Justification::centred);
        g.setColour (onB ? dark : IlanaTheme::Ui::text3.interpolatedWith (IlanaTheme::Ui::text, isHighlighted ? 0.5f : 0.0f));
        g.drawText ("B", right.toNearestInt(), juce::Justification::centred);
    }
};

