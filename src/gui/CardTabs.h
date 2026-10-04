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
    const juce::StringArray& getNames() const { return names; }

    // Replaces the pills (a pool that grows and shrinks: PLAY's envelope
    // tabs); the selection is kept in range, without notifying.
    void setNames (juce::StringArray newNames, std::vector<juce::Colour> newColours)
    {
        if (newNames == names && newColours == colours)
            return;

        names = std::move (newNames);
        colours = std::move (newColours);
        selected = names.isEmpty() ? selected : juce::jlimit (0, names.size() - 1, selected);
        repaint();
    }

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

    // A lit dot in a tab whose part is switched on (review 6, I6-15): cards
    // whose tabs are separate engines (SEQ's PATTERN) show which are on, not
    // only which is shown. Tabs that never get a state draw no dot.
    void setTabOn (int index, bool on)
    {
        if (! juce::isPositiveAndBelow (index, names.size()))
            return;

        if (tabOn.empty())
            tabOn.assign ((size_t) names.size(), -1);

        const auto state = on ? 1 : 0;

        if (tabOn[(size_t) index] == state)
            return;

        tabOn[(size_t) index] = state;
        repaint();
    }

    bool isTabOn (int index) const
    {
        return juce::isPositiveAndBelow (index, (int) tabOn.size()) && tabOn[(size_t) index] == 1;
    }

    // Width the pills need, so a card can right-align them.
    int getIdealWidth() const
    {
        return widthWith (idealPadding);
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

            if (! hasDots())
            {
                IlanaTheme::paintPill (g, pill, names[i], colour, active, hovered ? 1.0f : 0.0f);
                continue;
            }

            // The name shifts right of its dot: lit while on, a quiet ring
            // while off.
            const auto hover = hovered ? 1.0f : 0.0f;
            IlanaTheme::paintPill (g, pill, {}, colour, active, hover);
            g.setColour (active ? colour.interpolatedWith (juce::Colours::white, 0.2f) : juce::Colours::white.withAlpha (0.55f + 0.3f * hover));
            g.setFont (IlanaTheme::pillFont());
            g.drawText (names[i], pill.withTrimmedLeft ((float) dotSpace), juce::Justification::centred);
            const auto dot = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ pill.getX() + (float) padding() * 0.5f + 3.0f, pill.getCentreY() });

            if (isTabOn (i))
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

    static constexpr int idealPadding = 16;

    static constexpr int dotSpace = 10;

    bool hasDots() const { return ! tabOn.empty(); }

    int textWidth (int index) const
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::pillFont()), names[index])
             + (hasDots() ? dotSpace : 0);
    }

    int widthWith (int padding) const
    {
        auto width = 0;

        for (int i = 0; i < names.size(); ++i)
            width += textWidth (i) + padding + gap;

        return width + (hasOpen ? openWidth : 0);
    }

    // A row given less than its ideal width tightens the pills' padding
    // (down to 6 px) rather than running off the left.
    int padding() const
    {
        if (names.isEmpty() || getWidth() >= widthWith (idealPadding))
            return idealPadding;

        return juce::jlimit (6, idealPadding, (getWidth() - widthWith (0)) / names.size());
    }

    int pillWidth (int index) const { return textWidth (index) + padding(); }

    juce::Rectangle<float> pillBounds (int index) const
    {
        auto x = (float) juce::jmax (0, getWidth() - widthWith (padding()));

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
    std::vector<int> tabOn; // per tab: -1 no dot, 0 off, 1 on
};
