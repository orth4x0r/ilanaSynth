// Review 16, A16-1: nothing in an OSC card overlaps anything else. The right
// cluster (SCALE, ROOT, the OUT bar, the FM chip) once drew over each other.
// Included by Snapshot.cpp after its helpers; runOscCardOverlapTests runs
// from runUiTests.
#pragma once

void runOscCardOverlapTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    editor.showPage ("OSC");
    settle (300);
    juce::StringArray clashes;
    const auto nameOf = [] (juce::Component* component)
    {
        juce::String name = typeid (*component).name();
        if (auto* client = dynamic_cast<juce::TooltipClient*> (component))
            name = client->getTooltip().upToFirstOccurrenceOf ("\n", false, false);
        return name;
    };
    auto cards = 0;
    size_t checked = 0;

    for (const auto* preset : { "Neuro Wobble", "Init", "E.PIANO 1 (ROM1A)", "Bright Concert Grand" })
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (preset)));
        settle (500);

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const auto card = editor.getOscCardBounds (osc);
            if (card.isEmpty())
                continue;

            ++cards;
            // The card's visible controls, labels and pills: every shown
            // child of the page whose centre lies in the card.
            std::vector<juce::Component*> all;
            findAll<juce::Component> (editor, all);
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (editor, knobs);
            juce::Component* pageOf = nullptr;
            for (auto* knob : knobs)
                if (visibleInTree (knob) && card.contains (editor.getLocalArea (knob, knob->getLocalBounds()).getCentre()))
                    pageOf = knob->getParentComponent();
            std::vector<std::pair<juce::Component*, juce::Rectangle<int>>> items;
            for (auto* component : all)
            {
                if (! visibleInTree (component) || component->getWidth() <= 0 || component->getHeight() <= 0
                    || dynamic_cast<juce::Viewport*> (component) != nullptr)
                    continue;
                // (Everything the OSC page itself holds: controls, labels, pills and pictures.)
                if (component->getParentComponent() != pageOf)
                    continue;
                const auto area = editor.getLocalArea (component, component->getLocalBounds());
                if (card.contains (area.getCentre()))
                    items.push_back ({ component, area });
            }

            checked += items.size();
            for (size_t a = 0; a < items.size(); ++a)
                for (size_t b = a + 1; b < items.size(); ++b)
                {
                    // (A knob's box has a few px of room for its modulation ring: the overlap must be real.)
                    const auto both = items[a].second.getIntersection (items[b].second);
                    if (both.getWidth() > 4 && both.getHeight() > 4)
                        clashes.add (juce::String (preset) + " OSC " + juce::String (osc + 1) + ": " + nameOf (items[a].first)
                                     + " / " + nameOf (items[b].first));
                }
        }
    }

    expect (cards >= 8 && checked > 150 && clashes.isEmpty(),
            "OSC: no two controls, labels or pills in an OSC card overlap, on four patches (A16-1)"
                + (clashes.isEmpty() ? juce::String() : ": " + clashes.joinIntoString ("; ")));
}
