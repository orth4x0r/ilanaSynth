// Copy, Paste and Init on a module's right-click menu (ilana 2026-10-28,
// the RC 1.0 workflow list): an oscillator card's header, a filter card's,
// an envelope's or an LFO's (on PLAY and in the EDIT pop-ups' tiles). One
// clipboard per kind of module, kept for the session, so a module copied in
// one patch pastes into another (or another instance in the same host).
// Each item is one undo step.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

#include "../PluginProcessor.h"

namespace ModuleMenu
{
using Kind = IlanaSynthAudioProcessor::ModuleKind;

inline std::optional<IlanaSynthAudioProcessor::ModuleSnapshot>& clipboard (Kind kind)
{
    static std::array<std::optional<IlanaSynthAudioProcessor::ModuleSnapshot>, 4> held;
    return held[(size_t) kind];
}

// Adds Copy, Paste and Init (with a separator before them when the menu
// already has items). `changed` runs after a paste or an init (a card that
// caches what it shows).
inline void addItems (juce::PopupMenu& menu, IlanaSynthAudioProcessor& processor, Kind kind, int index,
                      std::function<void()> changed = {})
{
    if (menu.getNumItems() > 0)
        menu.addSeparator();

    const auto name = IlanaSynthAudioProcessor::moduleName (kind, index);
    auto* p = &processor;

    menu.addItem ("Copy " + name, true, false, [p, kind, index] { clipboard (kind) = p->copyModule (kind, index); });

    const auto& held = clipboard (kind);
    const auto pasteText = held.has_value() ? "Paste " + held->name + " here" : juce::String ("Paste");
    menu.addItem (pasteText, held.has_value(), false, [p, kind, index, name, changed]
    {
        if (const auto& from = clipboard (kind); from.has_value())
        {
            const auto snapshot = *from;
            p->performEdit ("Paste " + snapshot.name + " onto " + name, [p, &snapshot, index] { p->pasteModule (snapshot, index); });
            if (changed)
                changed();
        }
    });

    menu.addItem ("Init " + name, true, false, [p, kind, index, name, changed]
    {
        p->performEdit ("Init " + name, [p, kind, index] { p->initModule (kind, index); });
        if (changed)
            changed();
    });
}
} // namespace ModuleMenu
