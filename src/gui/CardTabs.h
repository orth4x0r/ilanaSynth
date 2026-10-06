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
    // Set for tabs that are engines with their own power (SEQ's PATTERN,
    // review 7, V7-11): each tab's state is drawn as a small switch inside
    // the pill, and a click on it calls this instead of selecting the tab.
    // One place for "on" (the switch) and one for "shown" (the fill).
    std::function<void (int)> onToggle;

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

            // The choice and the hover fade as every pill's do (the shared animator).
            const auto chosen = IlanaTheme::fade (*this, i, active ? 1.0f : 0.0f);
            const auto hover = IlanaTheme::fade (*this, 1000 + i, hovered ? 1.0f : 0.0f, IlanaTheme::FadeRate::hover);

            if (! hasDots())
            {
                IlanaTheme::paintPill (g, pill, names[i], colour, chosen, hover);
                continue;
            }

            // The name shifts right of its dot: lit while on, a quiet ring
            // while off.
            IlanaTheme::paintPill (g, pill, {}, colour, chosen, hover);
            g.setColour (IlanaTheme::Ui::text2.interpolatedWith (IlanaTheme::Ui::text, juce::jmax (chosen, 0.5f * hover)));
            g.setFont (IlanaTheme::pillFont());
            g.drawText (names[i], pill.withTrimmedLeft ((float) dotSpace()), juce::Justification::centred);

            if (onToggle != nullptr)
            {
                // A switch, not a light: it is the engine's power.
                const auto overSwitch = isMouseOver() && switchBounds (i).expanded (2.0f).contains (mouse);
                IlanaTheme::paintSwitch (g, switchBounds (i), IlanaTheme::fade (*this, 2000 + i, isTabOn (i) ? 1.0f : 0.0f), colour,
                                         IlanaTheme::fade (*this, 3000 + i, overSwitch ? 1.0f : 0.0f, IlanaTheme::FadeRate::hover));
                continue;
            }

            IlanaTheme::paintOnDot (g, { pill.getX() + (float) padding() * 0.5f + 3.0f, pill.getCentreY() }, colour, isTabOn (i));
        }

        if (hasOpen)
        {
            const auto open = openBounds();
            const auto hovered = isMouseOver() && open.contains (mouse);
            // The one "go to its full editor" idiom, "EDIT ›" (review 10,
            // I10-2): the same words as the header links, not a glyph. Set
            // apart from the pills by a gap and drawn as a link, text only in
            // the accent colour, so it does not read as one more tab (V11-19).
            const auto linkColour = names.isEmpty() ? IlanaTheme::accent() : colourFor (selected); // (the card's colour: the sheet's .lk)
            g.setColour (hovered ? linkColour.brighter (0.25f) : linkColour);
            g.setFont (IlanaTheme::linkFont());
            g.drawText (juce::String ("EDIT ") + juce::String::fromUTF8 ("\xe2\x80\xba"), open.withTrimmedLeft (12.0f), juce::Justification::centredRight);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        for (int i = 0; i < names.size(); ++i)
        {
            if (onToggle != nullptr && hasDots() && switchBounds (i).expanded (3.0f, 4.0f).contains (event.position))
            {
                onToggle (i);
                return;
            }

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
        juce::String tip;

        for (int i = 0; i < names.size() && onToggle != nullptr && hasDots(); ++i)
            if (switchBounds (i).expanded (3.0f, 4.0f).contains (event.position))
                tip = names[i] + " is " + (isTabOn (i) ? "on" : "off") + "\nClick the switch to turn " + names[i]
                    + (isTabOn (i) ? " off." : " on.");
            else if (pillBounds (i).contains (event.position))
                tip = "Show " + names[i] + "'s settings (the switch in the tab turns it on or off)";

        setTooltip (hasOpen && openBounds().contains (event.position) ? juce::String ("Edit\nOpen the full page for this section") : tip);
        repaint(); // the switch under the pointer
    }

    // Where tab `index`'s switch sits (the UI test clicks it).
    juce::Rectangle<float> switchBounds (int index) const
    {
        const auto pill = pillBounds (index);
        const auto height = juce::jlimit (6.0f, 10.0f, pill.getHeight() - 8.0f);
        return juce::Rectangle<float> (height * 1.8f, height).withPosition (pill.getX() + (float) padding() * 0.5f, pill.getCentreY() - height * 0.5f);
    }

private:
    static constexpr int gap = 4;
    static constexpr int openWidth = 72;

    static constexpr int idealPadding = 16;

    // The dot, or the wider switch, left of the name.
    int dotSpace() const { return onToggle != nullptr ? 22 : 10; }

    bool hasDots() const { return ! tabOn.empty(); }

    int textWidth (int index) const
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::pillFont()), names[index])
             + (hasDots() ? dotSpace() : 0);
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
