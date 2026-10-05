#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

#include "IlanaLookAndFeel.h"

// A row of tabs that say what each holds: a colour tag (lit while that part
// sounds), a name and a quiet state after it ("WAVETABLE", "OFF",
// "CARRIER"). OSC's oscillators and its shared sections use it.
class StateTabs : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    struct Item
    {
        juce::String name, state;
        juce::Colour colour;
        bool lit = true;
        juce::String tooltip;
        // The on dot is a switch's state only (UI-CONVENTIONS; review 8,
        // I8-21): a tab for something with no switch of its own (VOICE)
        // has none, rather than a dot lit by "some value is above 0".
        bool dot = true;

        bool operator== (const Item& other) const
        {
            return name == other.name && state == other.state && colour == other.colour && lit == other.lit && tooltip == other.tooltip
                && dot == other.dot;
        }
    };

    std::function<void (int)> onSelect, onMenu;

    StateTabs() { setRepaintsOnMouseActivity (true); }

    void setItems (std::vector<Item> newItems)
    {
        if (newItems == items)
            return;

        items = std::move (newItems);
        selected = juce::jlimit (0, juce::jmax (0, (int) items.size() - 1), selected);
        repaint();
    }

    void setSelected (int index)
    {
        if (index != selected)
        {
            selected = index;
            repaint();
        }
    }

    int getSelected() const { return selected; }
    int getNumItems() const { return (int) items.size(); }
    const Item& getItem (int index) const { return items[(size_t) juce::jlimit (0, (int) items.size() - 1, index)]; }

    int getIdealWidth() const { return widthAt (0); }
    // The width with every state quiet, and with the names cut to their
    // last word (the narrowest it draws).
    int getQuietWidth() const { return widthAt (2); }
    int getShortWidth() const { return widthAt (3); }

    juce::Rectangle<int> getTabBounds (int index) const
    {
        const auto level = stateLevel();
        auto x = 0;
        for (int i = 0; i < index; ++i)
            x += tabWidth (i, level) + gap;
        return { x, 0, tabWidth (index, level), getHeight() };
    }

    void paint (juce::Graphics& g) override
    {
        const auto mouse = getMouseXYRelative();

        for (int i = 0; i < (int) items.size(); ++i)
        {
            const auto& item = items[(size_t) i];
            const auto tab = getTabBounds (i).toFloat().reduced (0.5f, 1.5f);
            const auto active = i == selected;
            const auto hover = isMouseOver() && tab.contains (mouse.toFloat());
            const auto radius = tab.getHeight() * 0.5f;

            g.setColour (active ? IlanaTheme::Ui::raised.interpolatedWith (item.colour, 0.2f)
                                : IlanaTheme::Ui::panel.interpolatedWith (juce::Colours::white, hover ? 0.05f : 0.0f));
            g.fillRoundedRectangle (tab, radius);
            g.setColour (active ? item.colour.withAlpha (0.85f) : IlanaTheme::Ui::line.interpolatedWith (item.colour, hover ? 0.4f : 0.0f));
            g.drawRoundedRectangle (tab.reduced (0.5f), radius, 1.0f);

            auto area = tab.reduced (stateLevel() >= 3 ? 8.0f : 12.0f, 0.0f);
            // The same on indicator as every switchable tab (review 7).
            if (item.dot)
            {
                IlanaTheme::paintOnDot (g, { area.getX() + 3.5f, area.getCentreY() }, item.colour, item.lit);
                area.removeFromLeft (13.0f);
            }

            g.setColour (active ? IlanaTheme::Ui::text : (item.lit ? IlanaTheme::Ui::text2 : IlanaTheme::Ui::text3));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true));
            g.drawText (shownName (i, stateLevel()), area, juce::Justification::centredLeft);

            if (item.state.isNotEmpty() && showsState (i, stateLevel()))
            {
                area.removeFromLeft ((float) nameWidth (i, stateLevel()) + 7.0f);
                g.setColour (active ? item.colour.interpolatedWith (IlanaTheme::Ui::text2, 0.4f) : IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText (item.state, area, juce::Justification::centredLeft);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        for (int i = 0; i < (int) items.size(); ++i)
            if (getTabBounds (i).contains (event.getPosition()))
            {
                if (event.mods.isPopupMenu())
                {
                    if (onMenu != nullptr)
                        onMenu (i);
                    return;
                }

                setSelected (i);

                if (onSelect != nullptr)
                    onSelect (i);

                return;
            }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        juce::String tip;
        for (int i = 0; i < (int) items.size(); ++i)
            if (getTabBounds (i).contains (event.getPosition()))
                tip = items[(size_t) i].tooltip;
        setTooltip (tip);
    }

private:
    static constexpr int gap = 6;

    // Shortest of all, a name keeps its last word ("OSC 3" as "3", where an
    // oscillator picker sits in a tight header: UI review 8, I8-10).
    juce::String shownName (int index, int level) const
    {
        const auto& name = items[(size_t) index].name;
        return level >= 3 ? name.fromLastOccurrenceOf (" ", false, false) : name;
    }

    int nameWidth (int index, int level = 0) const
    {
        // Rounded up, with a pixel spare, so the text never ellipsises.
        return 2 + (int) std::ceil (juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true)),
                                                                            shownName (index, level)));
    }

    // Short of room, the states go quiet: first on the tabs not chosen,
    // then on all of them (the tooltips still say them).
    bool showsState (int index, int level) const { return level == 0 || (level == 1 && index == selected); }

    int tabWidth (int index, int level) const
    {
        const auto& state = items[(size_t) index].state;
        const auto stateWidth = state.isEmpty() || ! showsState (index, level)
                                    ? 0
                                    : 9 + (int) std::ceil (juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true)), state));
        return (level >= 3 ? 16 : 24) + (items[(size_t) index].dot ? 13 : 0) + nameWidth (index, level) + stateWidth;
    }

    int widthAt (int level) const
    {
        auto width = 0;
        for (int i = 0; i < (int) items.size(); ++i)
            width += tabWidth (i, level) + gap;
        return juce::jmax (0, width - gap);
    }

    int stateLevel() const
    {
        for (int level = 0; level < 3; ++level)
            if (widthAt (level) <= getWidth())
                return level;

        return 3;
    }

    std::vector<Item> items;
    int selected = 0;
};
