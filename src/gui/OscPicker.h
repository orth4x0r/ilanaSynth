#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <functional>
#include <vector>

#include "IlanaLookAndFeel.h"
#include "StateTabs.h"

// The one way to pick an oscillator (UI review 8, I8-10): OSC's tabs
// (StateTabs), each oscillator in its colour, its tag lit while it plays.
// FM's operator card, MOD's OP ENV editor and PHYSICAL use it; OSC uses the
// same tabs with each oscillator's role after its name. Short of room the
// names shrink to their numbers.
class OscPicker : public StateTabs
{
public:
    OscPicker()
    {
        onSelect = [this] (int tab)
        {
            if (juce::isPositiveAndBelow (tab, (int) oscs.size()) && onPick != nullptr)
                onPick (oscs[(size_t) tab]);
        };
    }

    // Called with the oscillator (0-5) picked.
    std::function<void (int)> onPick;

    // The oscillators offered, in order; lit while they play; each one's
    // tooltip.
    void setOscillators (const std::vector<int>& list, const std::function<bool (int)>& isLit,
                         const std::function<juce::String (int)>& tooltipFor = {})
    {
        std::vector<Item> items;
        for (const auto osc : list)
        {
            Item item;
            item.name = "OSC " + juce::String (osc + 1);
            item.colour = IlanaTheme::oscColour (osc);
            item.lit = isLit == nullptr || isLit (osc);
            item.tooltip = tooltipFor != nullptr ? tooltipFor (osc) : "Pick OSC " + juce::String (osc + 1);
            items.push_back (item);
        }
        oscs = list;
        setItems (items);
        setSelectedOsc (chosen);
    }

    void setSelectedOsc (int osc)
    {
        chosen = osc;
        const auto found = std::find (oscs.begin(), oscs.end(), osc);
        setSelected (found != oscs.end() ? (int) (found - oscs.begin()) : -1);
    }

    int getSelectedOsc() const { return chosen; }
    const std::vector<int>& getOscillators() const { return oscs; }

    // The tab bounds of an oscillator (the UI test clicks them), or empty.
    juce::Rectangle<int> getBoundsOf (int osc) const
    {
        const auto found = std::find (oscs.begin(), oscs.end(), osc);
        return found != oscs.end() ? getTabBounds ((int) (found - oscs.begin())) : juce::Rectangle<int>();
    }

    // Picks an oscillator as a click would.
    void pick (int osc)
    {
        setSelectedOsc (osc);
        if (onPick != nullptr && std::find (oscs.begin(), oscs.end(), osc) != oscs.end())
            onPick (osc);
    }

private:
    std::vector<int> oscs;
    int chosen = 0;
};
