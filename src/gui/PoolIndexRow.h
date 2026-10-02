#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

#include "IlanaLookAndFeel.h"

// A compact numbered row (1-16) above a pool's cards (UI review 4, V29 and
// S7): every slot is one click away without scrolling the cards sideways.
// A number is lit when its slot is in the patch, carries a dot in the slot's
// colour when it is in use (routed or playing), and the selected one is a
// pill. Clicking a number the patch doesn't have yet adds it and opens it.
class PoolIndexRow : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
{
public:
    struct State
    {
        bool shown = false;  // a card in the pool (added or in use)
        bool inUse = false;  // routed, or playing a part of the sound
    };

    PoolIndexRow (int countIn, juce::String kindIn) : count (countIn), kind (std::move (kindIn))
    {
        startTimerHz (4);
    }

    std::function<State (int)> stateOf;
    std::function<juce::Colour (int)> colourOf;
    std::function<juce::String (int)> nameOf;
    std::function<void (int)> onPick;

    void setSelected (int index)
    {
        if (selected != index)
        {
            selected = index;
            repaint();
        }
    }

    int getSelected() const { return selected; }
    int getCount() const { return count; }
    State getState (int index) const { return stateOf != nullptr ? stateOf (index) : State(); }

    // What a click on number `index` (0-based) does.
    void pick (int index)
    {
        if (juce::isPositiveAndBelow (index, count) && onPick != nullptr)
            onPick (index);

        repaint();
    }

    juce::Rectangle<float> cellBounds (int index) const
    {
        const auto width = (float) getWidth() / (float) juce::jmax (1, count);
        return { (float) index * width, 0.0f, width, (float) getHeight() };
    }

    void paint (juce::Graphics& g) override
    {
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));

        for (int index = 0; index < count; ++index)
        {
            const auto state = getState (index);
            const auto colour = colourOf != nullptr ? colourOf (index) : IlanaTheme::accent();
            const auto cell = cellBounds (index).reduced (1.5f, 1.0f);
            const auto pill = cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 30.0f), cell.getHeight());
            const auto hovered = index == hoverIndex;

            if (index == selected)
            {
                g.setColour (colour.withAlpha (0.22f));
                g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
                g.setColour (colour.withAlpha (0.8f));
                g.drawRoundedRectangle (pill.reduced (0.5f), pill.getHeight() * 0.5f, 1.0f);
            }
            else if (hovered)
            {
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
            }

            g.setColour (index == selected ? colour.interpolatedWith (juce::Colours::white, 0.25f)
                         : state.shown     ? IlanaTheme::Ui::text.withAlpha (hovered ? 1.0f : 0.8f)
                                           : IlanaTheme::Ui::text3.withAlpha (hovered ? 0.9f : 0.55f));
            const auto text = pill.withTrimmedRight (state.inUse ? 6.0f : 0.0f);
            g.drawText (juce::String (index + 1), text, juce::Justification::centred);

            if (state.inUse)
            {
                g.setColour (colour);
                g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ text.getRight() - 1.0f, pill.getCentreY() }));
            }
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.position);

        if (index != hoverIndex)
        {
            hoverIndex = index;

            if (index >= 0)
            {
                const auto state = getState (index);
                const auto name = nameOf != nullptr ? nameOf (index) : kind + " " + juce::String (index + 1);
                setTooltip (name + (state.inUse ? ": in use" : state.shown ? ": added, not routed yet" : ": not in the patch yet (click adds it)")
                            + "\nClick to open it below.");
            }

            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverIndex = -1;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mouseWasDraggedSinceMouseDown())
            return;

        pick (indexAt (event.position));
    }

private:
    int indexAt (juce::Point<float> position) const
    {
        for (int index = 0; index < count; ++index)
            if (cellBounds (index).contains (position))
                return index;

        return -1;
    }

    // States come from parameters and the matrix; re-read four times a second.
    void timerCallback() override
    {
        if (! isShowing())
            return;

        juce::uint64 signature = 0;

        for (int index = 0; index < count; ++index)
        {
            const auto state = getState (index);
            signature = signature * 4u + (state.shown ? 1u : 0u) + (state.inUse ? 2u : 0u);
        }

        if (signature != lastSignature)
        {
            lastSignature = signature;
            repaint();
        }
    }

    int count = 16;
    juce::String kind;
    int selected = 0;
    int hoverIndex = -1;
    juce::uint64 lastSignature = 0;
};
