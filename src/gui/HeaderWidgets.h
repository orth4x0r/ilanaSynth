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

inline juce::Path make (Icon icon, juce::Rectangle<float> r)
{
    juce::Path p;
    const auto cx = r.getCentreX();
    const auto cy = r.getCentreY();
    const auto s = juce::jmin (r.getWidth(), r.getHeight());

    switch (icon)
    {
        case Icon::Undo:
        case Icon::Redo:
        {
            // A hooked arrow: arc over the top, arrowhead on the left (undo).
            const auto radius = s * 0.3f;
            p.addCentredArc (cx, cy + s * 0.06f, radius, radius, 0.0f,
                             -juce::MathConstants<float>::halfPi * 1.25f, juce::MathConstants<float>::halfPi * 0.95f, true);
            const juce::Point<float> tip (cx - radius * 0.96f, cy - radius * 0.2f);
            juce::Path head;
            head.addTriangle (tip.x - s * 0.13f, tip.y - s * 0.02f, tip.x + s * 0.1f, tip.y - s * 0.02f,
                              tip.x - s * 0.02f, tip.y + s * 0.15f);

            juce::Path stroked;
            juce::PathStrokeType (s * 0.09f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createStrokedPath (stroked, p);
            stroked.addPath (head);

            if (icon == Icon::Redo)
                stroked.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f, cx, cy));

            return stroked;
        }

        case Icon::History:
        {
            const auto radius = s * 0.34f;
            juce::Path outline;
            outline.addEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre ({ cx, cy }));
            outline.startNewSubPath (cx, cy - radius * 0.62f);
            outline.lineTo (cx, cy);
            outline.lineTo (cx + radius * 0.48f, cy + radius * 0.3f);

            juce::PathStrokeType (s * 0.085f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createStrokedPath (p, outline);
            return p;
        }

        case Icon::Gear:
        {
            const auto outer = s * 0.4f;
            const auto inner = s * 0.29f;
            constexpr int teeth = 8;

            for (int i = 0; i < teeth * 2; ++i)
            {
                const auto a0 = (float) i / (float) (teeth * 2) * juce::MathConstants<float>::twoPi;
                const auto a1 = (float) (i + 1) / (float) (teeth * 2) * juce::MathConstants<float>::twoPi;
                const auto radius = i % 2 == 0 ? outer : inner;
                const juce::Point<float> pa (cx + std::cos (a0) * radius, cy + std::sin (a0) * radius);
                const juce::Point<float> pb (cx + std::cos (a1) * radius, cy + std::sin (a1) * radius);

                if (i == 0)
                    p.startNewSubPath (pa);
                else
                    p.lineTo (pa);

                p.lineTo (pb);
            }

            p.closeSubPath();
            p.addEllipse (juce::Rectangle<float> (s * 0.26f, s * 0.26f).withCentre ({ cx, cy }));
            p.setUsingNonZeroWinding (false);
            return p;
        }

        case Icon::Dice:
        {
            const auto box = juce::Rectangle<float> (s * 0.66f, s * 0.66f).withCentre ({ cx, cy });
            p.addRoundedRectangle (box, s * 0.12f);

            const auto pip = s * 0.12f;

            for (const auto& offset : { juce::Point<float> (-0.18f, -0.18f), juce::Point<float> (0.18f, 0.18f),
                                        juce::Point<float> (0.0f, 0.0f), juce::Point<float> (0.18f, -0.18f),
                                        juce::Point<float> (-0.18f, 0.18f) })
                p.addEllipse (juce::Rectangle<float> (pip, pip).withCentre ({ cx + offset.x * s, cy + offset.y * s }));

            p.setUsingNonZeroWinding (false);
            return p;
        }

        case Icon::ChevronLeft:
        case Icon::ChevronRight:
        {
            juce::Path line;
            const auto dir = icon == Icon::ChevronLeft ? 1.0f : -1.0f;
            line.startNewSubPath (cx + dir * s * 0.1f, cy - s * 0.22f);
            line.lineTo (cx - dir * s * 0.1f, cy);
            line.lineTo (cx + dir * s * 0.1f, cy + s * 0.22f);
            juce::PathStrokeType (s * 0.1f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded)
                .createStrokedPath (p, line);
            return p;
        }

        case Icon::Star:
            p.addStar ({ cx, cy + s * 0.02f }, 5, s * 0.17f, s * 0.38f, 0.0f);
            return p;

        case Icon::More:
            for (int i = -1; i <= 1; ++i)
                p.addEllipse (juce::Rectangle<float> (s * 0.13f, s * 0.13f).withCentre ({ cx + (float) i * s * 0.22f, cy }));
            return p;

        case Icon::Save:
        {
            // Down arrow into a tray.
            juce::Path line;
            line.startNewSubPath (cx, cy - s * 0.3f);
            line.lineTo (cx, cy + s * 0.08f);
            line.startNewSubPath (cx - s * 0.15f, cy - s * 0.06f);
            line.lineTo (cx, cy + s * 0.1f);
            line.lineTo (cx + s * 0.15f, cy - s * 0.06f);
            line.startNewSubPath (cx - s * 0.3f, cy + s * 0.08f);
            line.lineTo (cx - s * 0.3f, cy + s * 0.3f);
            line.lineTo (cx + s * 0.3f, cy + s * 0.3f);
            line.lineTo (cx + s * 0.3f, cy + s * 0.08f);
            juce::PathStrokeType (s * 0.085f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createStrokedPath (p, line);
            return p;
        }
    }

    return p;
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

    void setIconColour (std::optional<juce::Colour> colour)
    {
        iconColour = colour;
        repaint();
    }

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        const auto on = getToggleState();
        auto background = findColour (on ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId);

        getLookAndFeel().drawButtonBackground (g, *this, background, isHighlighted, isDown);

        auto area = getLocalBounds().toFloat().reduced (3.0f);
        const auto enabled = isEnabled();
        auto colour = iconColour.value_or (on ? juce::Colours::white : juce::Colours::white.withAlpha (0.72f));

        if (! enabled)
            colour = colour.withAlpha (0.22f);
        else if (isHighlighted)
            colour = colour.brighter (0.3f);

        g.setColour (colour);

        if (text.isNotEmpty())
        {
            const auto iconArea = area.removeFromLeft (area.getHeight());
            g.fillPath (IlanaIcons::make (icon, iconArea));
            g.setFont (IlanaTheme::font (12.5f, true));
            g.drawText (text, area.withTrimmedRight (2.0f), juce::Justification::centred);
        }
        else
        {
            const auto side = juce::jmin (area.getWidth(), area.getHeight());
            g.fillPath (IlanaIcons::make (icon, area.withSizeKeepingCentre (side, side)));
        }
    }

private:
    IlanaIcons::Icon icon;
    juce::String text;
    std::optional<juce::Colour> iconColour;
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

    void setFlash (float amount)
    {
        if (std::abs (amount - flash) > 0.005f)
        {
            flash = amount;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

        IlanaTheme::paintWell (g, bounds, 6.0f);

        if (hover)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.35f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
        }

        if (flash > 0.01f)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.22f * flash));
            g.fillRoundedRectangle (bounds, 6.0f);
            g.setColour (IlanaTheme::accent().withAlpha (0.6f * flash));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.4f);
        }

        auto text = getLocalBounds().reduced (12, 3);

        // Browse chevron on the right.
        const auto chevronArea = text.removeFromRight (14).toFloat();
        juce::Path chevron;
        chevron.startNewSubPath (chevronArea.getCentreX() - 4.0f, chevronArea.getCentreY() - 2.0f);
        chevron.lineTo (chevronArea.getCentreX(), chevronArea.getCentreY() + 2.0f);
        chevron.lineTo (chevronArea.getCentreX() + 4.0f, chevronArea.getCentreY() - 2.0f);
        g.setColour (IlanaTheme::accent().withAlpha (hover ? 1.0f : 0.7f));
        g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto categoryText = category.isNotEmpty() ? category.toUpperCase() : juce::String ("PRESET");
        g.setColour (IlanaTheme::accent().withAlpha (0.8f));
        g.setFont (IlanaTheme::font (9.5f, true));
        g.drawText (categoryText + (isModified ? "   -   EDITED" : ""), text.removeFromTop (11),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.setFont (IlanaTheme::font (16.5f, true));
        const auto nameText = name.isNotEmpty() ? name : juce::String ("Init");
        g.drawText (nameText, text, juce::Justification::centredLeft, true);

        if (isFavourite)
        {
            const auto nameWidth = juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (16.5f, true)), nameText);
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
    juce::String name, category;
    bool isFavourite = false;
    bool isModified = false;
    bool hover = false;
    float flash = 0.0f;
};
