// Duplicating, reordering and removing oscillators on PLAY (ilana
// 2026-10-26): a card dragged by its header lands where it is dropped (one
// undo step), the header's x removes it, the header menu offers Duplicate.
// Included by Snapshot.cpp; runs from runUiTests (ILANA_UITEST_ONLY=OS for
// just these). ILANA_SNAPSHOT_OSCDRAG=<dir> also saves pictures mid-drag.
#pragma once

// (The pages live in PluginEditor.cpp's anonymous namespace: the test finds
// the cards through the editor's getOscCardBounds and sends the mouse to
// whatever component is under it, as a real drag would.)
namespace OscSlotUi
{
inline juce::MouseEvent mouse (juce::Component& component, juce::Point<float> position, bool down, juce::Point<float> downPosition)
{
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position,
                             down ? juce::ModifierKeys::leftButtonModifier : juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &component, &component, juce::Time::getCurrentTime(), downPosition, juce::Time::getCurrentTime(), 1, false);
}

inline float semi (IlanaSynthAudioProcessor& processor, const char* id)
{
    auto* parameter = processor.apvts.getParameter (id);
    return parameter->convertFrom0to1 (parameter->getValue());
}

inline void setSemis (IlanaSynthAudioProcessor& processor, float a, float b, float c)
{
    for (const auto& [id, value] : { std::pair<const char*, float> { "osc1_semi", a }, { "osc2_semi", b }, { "sub_semi", c } })
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// The component a click at p (editor coordinates) reaches: the deepest
// visible child under it that takes the mouse. (getComponentAt needs the
// editor itself on screen.)
inline juce::Component* componentUnder (juce::Component& parent, juce::Point<int> p, juce::Component& editor)
{
    auto& children = parent.getChildren();
    for (int i = children.size(); --i >= 0;)
    {
        auto* child = children[i];
        const auto local = child->getLocalPoint (&editor, p);
        if (! child->isVisible() || ! child->getLocalBounds().contains (local))
            continue;
        if (auto* deeper = componentUnder (*child, p, editor))
            return deeper;
        bool self = true, kids = true;
        child->getInterceptsMouseClicks (self, kids);
        if (self && child->hitTest (local.x, local.y))
            return child;
    }
    return nullptr;
}

inline juce::Component* componentUnder (juce::Component& editor, juce::Point<int> p)
{
    return componentUnder (editor, p, editor);
}

// A press at from, a drag through to and a release, in the editor's
// coordinates, all sent to the component under from.
inline juce::Component* drag (juce::Component& editor, juce::Point<float> from, juce::Point<float> to,
                              const std::function<void()>& midway = {})
{
    auto* target = componentUnder (editor, from.roundToInt());
    if (target == nullptr)
        return nullptr;
    const auto local = [&] (juce::Point<float> p) { return target->getLocalPoint (&editor, p); };
    target->mouseDown (mouse (*target, local (from), true, local (from)));
    for (int step = 1; step <= 8; ++step)
        target->mouseDrag (mouse (*target, local (from + (to - from) * ((float) step / 8.0f)), true, local (from)));
    if (midway)
        midway();
    target->mouseUp (mouse (*target, local (to), false, local (from)));
    return target;
}
} // namespace OscSlotUi

inline void runOscSlotUiTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    using namespace OscSlotUi;

    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    settle (300);
    // (The pictures at ilana's 175 %.)
    const auto dir = juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_OSCDRAG", {});
    const auto oldSize = editor.getBounds();
    if (dir.isNotEmpty())
        editor.setSize (1855, 1260);
    editor.showPage ("MAIN");
    settle (400);
    processor.addOscillator (2);
    processor.performEdit ("Set SEMI", [&processor] { setSemis (processor, 1.0f, 2.0f, 3.0f); });
    settle (300);
    expect (semi (processor, "osc1_semi") == 1.0f && semi (processor, "sub_semi") == 3.0f,
            "OSC 1-3 set apart by SEMI (" + juce::String (semi (processor, "osc1_semi")) + ", " + juce::String (semi (processor, "sub_semi")) + ")");

    // The cards' headers, found by their x buttons (top to bottom).
    const auto removeButtons = [&editor]
    {
        std::vector<juce::TextButton*> buttons, found;
        findAll<juce::TextButton> (editor, buttons);
        for (auto* button : buttons)
            if (visibleInTree (button) && button->getTooltip().startsWith ("Remove this oscillator"))
                found.push_back (button);
        std::sort (found.begin(), found.end(), [&editor] (auto* x, auto* y)
                   { return editor.getLocalArea (x, x->getLocalBounds()).getY() < editor.getLocalArea (y, y->getLocalBounds()).getY(); });
        return found;
    };
    const auto headers = removeButtons();
    expect (headers.size() == 3, "PLAY shows three cards, each with an x (" + juce::String ((int) headers.size()) + ")");
    if (headers.size() != 3)
        return;

    // OSC 1 dragged by its name to the lower half of OSC 3.
    auto* page = headers[0]->getParentComponent();
    const auto headerY = [&] (int card) { return (float) editor.getLocalArea (headers[(size_t) card], headers[(size_t) card]->getLocalBounds()).getCentreY(); };
    const auto nameX = (float) editor.getLocalPoint (page, juce::Point<int> (32, 0)).x;
    const juce::Point<float> grab (nameX, headerY (0));
    const juce::Point<float> drop (nameX + 300.0f, headerY (2) + (headerY (2) - headerY (1)) * 0.75f);
    drag (editor, grab, drop);
    settle (300);
    expect (semi (processor, "osc1_semi") == 2.0f && semi (processor, "osc2_semi") == 3.0f && semi (processor, "sub_semi") == 1.0f,
            "OSC 1 dragged below OSC 3 lands last (the others move up)");
    processor.getUndoManager().undo();
    settle (200);
    expect (semi (processor, "osc1_semi") == 1.0f && semi (processor, "osc2_semi") == 2.0f && semi (processor, "sub_semi") == 3.0f,
            "one undo puts the cards back");

    // A click on a header without a drag moves nothing.
    drag (editor, grab, grab);
    expect (semi (processor, "osc1_semi") == 1.0f, "a click on the header moves nothing");

    // Duplicate OSC 1: the copy is OSC 2, the old OSC 2 and 3 move down.
    processor.performEdit ("Duplicate OSC 1", [&processor] { processor.duplicateOscillator (0); });
    settle (300);
    expect (processor.isOscillatorShown (3) && semi (processor, "osc2_semi") == 1.0f && semi (processor, "sub_semi") == 2.0f
                && semi (processor, "osc4_semi") == 3.0f,
            "Duplicate puts a copy of OSC 1 right under it");
    if (dir.isNotEmpty())
        save (editor, juce::File (dir).getChildFile ("osc-duplicated.png"));
    processor.getUndoManager().undo();
    settle (300);
    expect (! processor.isOscillatorShown (3) && semi (processor, "osc2_semi") == 2.0f, "one undo takes the copy away");

    if (dir.isNotEmpty())
    {
        // The picture: OSC 1 held over OSC 2's lower half, the line between OSC 2 and 3.
        if (auto* viewport = page->findParentComponentOfClass<juce::Viewport>())
            viewport->setViewPosition (0, 0);
        settle (100);
        const auto now = removeButtons();
        const auto y = [&] (int card) { return (float) editor.getLocalArea (now[(size_t) card], now[(size_t) card]->getLocalBounds()).getCentreY(); };
        drag (editor, { nameX, y (0) }, { nameX + 300.0f, y (1) + (y (2) - y (1)) * 0.75f }, [&]
        {
            settle (100);
            juce::File (dir).createDirectory();
            save (editor, juce::File (dir).getChildFile ("osc-drag.png"));
        });
        settle (300);
        processor.getUndoManager().undo();
        settle (300);
    }

    // The header's x removes the oscillator.
    auto* remove = removeButtons()[2];
    remove->triggerClick();
    settle (300);
    expect (! processor.isOscillatorShown (2), "OSC 3's x removes it");
    if (dir.isNotEmpty())
        editor.setBounds (oldSize);
}
