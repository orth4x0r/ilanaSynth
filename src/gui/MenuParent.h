#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Where pop-up menus and call-out boxes open: inside the plugin editor (moved
// out of IlanaLookAndFeel.h, which includes it; code health before RC 1.0).
namespace IlanaTheme
{
// The one place that builds the options for a pop-up menu anchored to a component. Every menu
// launch site goes through here, so a change to how menus open (parent, scale, renderer) is one edit.
// Returns the options so a site can still chain .withMousePosition(), .withMinimumWidth() and so on.
// Menus open inside the editor's zoomed content, as Vital's do (its
// SinglePopupSelector is a child of the interface, not a window): they take
// exactly the page's scale whatever the host does. As windows of their own
// they came out at 1x, much smaller than the page, while Live's Auto-Scale
// Plug-In Window bitmap-stretched the editor (ilana 2026-11-24: "i really
// really want that fixed", "why can't we do it and vital can"). The cost, as
// in Vital: a menu can't hang outside the plugin window.
inline juce::PopupMenu::Options menuOptions (juce::Component* target)
{
    auto options = juce::PopupMenu::Options().withTargetComponent (target);
    if (target != nullptr)
        for (auto* c = target; c != nullptr; c = c->getParentComponent())
            if (c->getProperties().contains ("ilanaCalloutHost"))
                return options.withParentComponent (c);
    return options;
}
inline juce::PopupMenu::Options menuOptions (juce::Component& target) { return menuOptions (&target); }

// The component a call-out box (the table browser, the colour picker, the
// remap editor) goes into: the plugin editor itself, never the top-level
// component. In a plugin the top level is the host wrapper around the
// editor (JUCE's VST3 ContentWrapperComponent), so a call-out added there
// is the editor's sibling: the software renderer paints it, but with Vulkan
// on, the editor is drawn into a child window that covers the wrapper, and
// the call-out opened invisibly under it (ilana 2026-11-17, "they never
// open"). Falls back to the top level outside an editor.
inline juce::Component* calloutParent (juce::Component& anchor)
{
    // (The editor marks itself with "ilanaCalloutHost": this header doesn't
    // see juce_audio_processors.)
    for (auto* c = &anchor; c != nullptr; c = c->getParentComponent())
        if (c->getProperties().contains ("ilanaCalloutHost"))
            return c;
    return anchor.getTopLevelComponent();
}
} // namespace IlanaTheme
