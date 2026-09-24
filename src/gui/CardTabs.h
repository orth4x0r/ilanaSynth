#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "IlanaLookAndFeel.h"

// A row of small pill tabs for a card header (F1 / F2, AMP / MOD ...), with an
// optional "open" button that jumps to the full page for that section.
class CardTabs : public juce::Component,
                 public juce::SettableTooltipClient
{
public:
    CardTabs (juce::StringArray namesIn, std::vector<juce::Colour> coloursIn, bool withOpenButton)
        : names (std::move (namesIn)), colours (std::move (coloursIn)), hasOpen (withOpenButton)
    {
        setRepaintsOnMouseActivity (true);
    }

    std::function<void (int)> onSelect;
    std::function<void()> onOpen;

    int getSelected() const { return selected; }

    void setSelected (int index, bool notify)
    {
        // With no pills (open button only) the index is just remembered.
        index = names.isEmpty() ? index : juce::jlimit (0, names.size() - 1, index);

        if (index == selected)
            return;

        selected = index;
        repaint();

        if (notify && onSelect != nullptr)
            onSelect (selected);
    }

    // Width the pills need, so a card can right-align them.
    int getIdealWidth() const
    {
        auto width = 0;

        for (int i = 0; i < names.size(); ++i)
            width += pillWidth (i) + gap;

        return width + (hasOpen ? openWidth : 0);
    }

    void paint (juce::Graphics& g) override
    {
        const auto mouse = getMouseXYRelative().toFloat();

        for (int i = 0; i < names.size(); ++i)
        {
            const auto pill = pillBounds (i);
            const auto active = i == selected;
            const auto colour = colourFor (i);
            const auto hovered = isMouseOver() && pill.contains (mouse);

            g.setColour (active ? colour.withAlpha (0.22f) : juce::Colours::white.withAlpha (hovered ? 0.08f : 0.03f));
            g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);

            if (active)
            {
                g.setColour (colour.withAlpha (0.8f));
                g.drawRoundedRectangle (pill.reduced (0.5f), pill.getHeight() * 0.5f, 1.0f);
            }

            g.setColour (active ? colour : juce::Colours::white.withAlpha (hovered ? 0.8f : 0.5f));
            g.setFont (IlanaTheme::font (10.5f, true));
            g.drawText (names[i], pill, juce::Justification::centred);
        }

        if (hasOpen)
        {
            const auto open = openBounds();
            const auto hovered = isMouseOver() && open.contains (mouse);
            g.setColour (juce::Colours::white.withAlpha (hovered ? 0.9f : 0.45f));

            // Two-arrow "expand" glyph.
            const auto box = open.withSizeKeepingCentre (11.0f, 11.0f);
            juce::Path glyph;
            glyph.startNewSubPath (box.getX() + 4.0f, box.getY());
            glyph.lineTo (box.getRight(), box.getY());
            glyph.lineTo (box.getRight(), box.getY() + 7.0f);
            glyph.startNewSubPath (box.getRight(), box.getY());
            glyph.lineTo (box.getX() + 3.0f, box.getBottom() - 3.0f);
            glyph.startNewSubPath (box.getX(), box.getY() + 4.0f);
            glyph.lineTo (box.getX(), box.getBottom());
            glyph.lineTo (box.getX() + 7.0f, box.getBottom());
            g.strokePath (glyph, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        for (int i = 0; i < names.size(); ++i)
        {
            if (pillBounds (i).contains (event.position))
            {
                setSelected (i, true);
                return;
            }
        }

        if (hasOpen && openBounds().contains (event.position) && onOpen != nullptr)
            onOpen();
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        setTooltip (hasOpen && openBounds().contains (event.position) ? "Open the full page for this section" : juce::String());
    }

private:
    static constexpr int gap = 4;
    static constexpr int openWidth = 24;

    int pillWidth (int index) const
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (10.5f, true)), names[index]) + 16;
    }

    juce::Rectangle<float> pillBounds (int index) const
    {
        auto x = (float) (getWidth() - getIdealWidth());

        for (int i = 0; i < index; ++i)
            x += (float) (pillWidth (i) + gap);

        return { x, 1.0f, (float) pillWidth (index), (float) getHeight() - 2.0f };
    }

    juce::Rectangle<float> openBounds() const
    {
        return { (float) (getWidth() - openWidth), 0.0f, (float) openWidth, (float) getHeight() };
    }

    juce::Colour colourFor (int index) const
    {
        return juce::isPositiveAndBelow (index, (int) colours.size()) ? colours[(size_t) index] : IlanaTheme::accent();
    }

    juce::StringArray names;
    std::vector<juce::Colour> colours;
    bool hasOpen = false;
    int selected = 0;
};
