#pragma once

#include <typeinfo>

#include <juce_gui_basics/juce_gui_basics.h>

namespace SubmenuPlacement
{
// Called by IlanaLookAndFeel::preparePopupMenuWindow for every menu window.
// A submenu lines up with the row that opened it and drops down from there,
// as in Vital. JUCE opens one from a row in the lower half upwards, ending
// at the row, so inside the plugin window the BASIC filters floated up over
// OSC 2 (ilana 2026-11-24). Only for menus inside the editor (a parent
// component); it moves the new window to the row's top, then up just
// enough to fit. Works for a submenu opened by the mouse or the keyboard.
inline void place (juce::Component& window, int menuBorder)
{
    auto* host = window.getParentComponent();
    if (host == nullptr)
        return;

    const auto area = host->getLocalBounds().reduced (menuBorder);
    const auto rowTopIn = [host] (juce::Component& row) { return host->getLocalPoint (&row, juce::Point<int>()).y; };
    const auto moveTo = [&] (juce::Component& row)
    {
        const auto y = juce::jmax (area.getY() + 1, juce::jmin (rowTopIn (row) - menuBorder, area.getBottom() - window.getHeight() - 6));
        window.setTopLeftPosition (window.getX(), y);
    };

    for (auto* opener : host->getChildren())
    {
        if (opener == &window || typeid (*opener) != typeid (window) || ! opener->isVisible())
            continue;

        // The row under the mouse, when the mouse opened it.
        const auto mouse = opener->getMouseXYRelative();
        if (opener->getLocalBounds().contains (mouse))
        {
            auto* row = opener->getComponentAt (mouse);
            while (row != nullptr && row->getParentComponent() != opener)
                row = row->getParentComponent();
            if (row != nullptr)
            {
                moveTo (*row);
                return;
            }
        }

        // Opened from the keyboard: the row JUCE hung it from (its bottom
        // edge, plus the border, where JUCE put the window's top).
        for (auto* row : opener->getChildren())
        {
            const auto rowBottom = rowTopIn (*row) + row->getHeight();
            if (std::abs (window.getY() - (rowBottom - window.getHeight() + menuBorder)) <= 1)
            {
                moveTo (*row);
                return;
            }
        }
    }
}
} // namespace SubmenuPlacement
