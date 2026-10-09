#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

// PLAY's OP ENV tab: the Operator Env is no modulation source (nothing to
// drag), but a DX7 voice's envelope card showed it first, so its picture
// and what plays it stay on PLAY.
inline constexpr int opEnvSourceId = -1;

// What PLAY's MODULATION card (gui/pages/ModSourcePanel.h) shows the rest of
// the code, outside the editor's pages: its sources in tab order, the
// selected one, and its tabs. The UI tests find the card by this.
class ModulationCardView
{
public:
    virtual ~ModulationCardView() = default;

    virtual std::vector<int> getSourceList() const = 0;
    virtual int getSelectedSource() const = 0;
    virtual void selectSource (int source) = 0;
    // The tabs shown, in order (each a click selects, a drag assigns).
    virtual std::vector<juce::Component*> getSourceTabs() const = 0;
    virtual int sourceOfTab (const juce::Component& tab) const = 0;
    // EDIT ›: the selected source's full editor.
    virtual void openSelectedInMod() = 0;
};
