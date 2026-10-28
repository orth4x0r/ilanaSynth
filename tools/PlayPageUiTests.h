// The approved PLAY page (UI review 24): its cards' ink margins, the filter graphs' width and the
// two-line knob names. Included by Snapshot.cpp after OscTypeUiTests.h; runPlayPageTests runs from
// runUiTests, and alone with ILANA_UITEST_ONLY=PLAY.
#pragma once

// Where a card's content starts and ends, left and right, in pixels: the first and last column under the header
// that differs from the card's own fill (the most common colour inside it).
struct InkMargins { int left = -1, right = -1; };

inline InkMargins scanInkMargins (const juce::Image& image, juce::Rectangle<int> card, int header)
{
    const auto body = card.withTrimmedTop (header + 2).withTrimmedBottom (4).reduced (4, 0).getIntersection (image.getBounds());
    std::map<juce::uint32, int> counts;
    for (int y = body.getY(); y < body.getBottom(); y += 2)
        for (int x = body.getX(); x < body.getRight(); x += 2)
            ++counts[image.getPixelAt (x, y).getARGB()];
    juce::uint32 fill = 0;
    auto best = 0;
    for (const auto& [colour, count] : counts)
        if (count > best)
        {
            best = count;
            fill = colour;
        }
    const juce::Colour background (fill);
    const auto differs = [&] (int x)
    {
        for (int y = body.getY(); y < body.getBottom(); ++y)
        {
            const auto c = image.getPixelAt (x, y);
            if (std::abs ((int) c.getRed() - (int) background.getRed()) + std::abs ((int) c.getGreen() - (int) background.getGreen())
                    + std::abs ((int) c.getBlue() - (int) background.getBlue()) > 36)
                return true;
        }
        return false;
    };
    InkMargins margins;
    for (int x = body.getX(); x < body.getRight(); ++x)
        if (differs (x))
        {
            margins.left = x - card.getX();
            break;
        }
    for (int x = body.getRight() - 1; x >= body.getX(); --x)
        if (differs (x))
        {
            margins.right = card.getRight() - 1 - x;
            break;
        }
    return margins;
}

void runPlayPageTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    processor.loadFactoryPreset (0);
    settle (400);
    editor.setSize (1060, 720);
    editor.showPage ("MAIN");
    settle (500);

    std::vector<KnobControl*> knobs;
    findAll<KnobControl> (editor, knobs);
    KnobControl* anchor = nullptr;
    for (auto* knob : knobs)
        if (knob->getParameterId() == "west_fold")
            anchor = knob;
    expect (anchor != nullptr && anchor->getParentComponent() != nullptr, "PLAY: the page is found by WEST's FOLD knob");
    if (anchor == nullptr || anchor->getParentComponent() == nullptr)
        return;

    auto* page = anchor->getParentComponent();
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
    juce::String report, uneven;
    for (const auto& entry : juce::StringArray::fromTokens (page->getProperties()["playCards"].toString(), ";", ""))
    {
        const auto parts = juce::StringArray::fromTokens (entry, ",", "");
        if (parts.size() != 5)
            continue;
        auto card = juce::Rectangle<int> (parts[1].getIntValue(), parts[2].getIntValue(), parts[3].getIntValue(), parts[4].getIntValue());
        card = editor.getLocalArea (page, card);
        const auto margins = scanInkMargins (image, card, 20);
        report << parts[0] << " " << margins.left << "/" << margins.right << "  ";
        if (margins.left < 0 || margins.right < 0 || std::abs (margins.left - margins.right) > 2)
            uneven << parts[0] << " " << margins.left << "/" << margins.right << ", ";
    }
    std::cout << "  (PLAY ink margins left/right: " << report << ")" << std::endl;
    // Where the first filter's graph and dial block sit, for the vertical centring check.
    {
        std::vector<FilterDisplay*> graphs;
        findAll<FilterDisplay> (editor, graphs);
        KnobControl* cutoff = nullptr;
        for (auto* knob : knobs)
            if (knob->getParameterId() == "f1_cutoff")
                cutoff = knob;
        for (auto* graph : graphs)
            if (cutoff != nullptr && visibleInTree (graph))
            {
                const auto g = editor.getLocalArea (graph->getParentComponent(), graph->getBounds());
                const auto d = editor.getLocalArea (cutoff, cutoff->packedCell());
                std::cout << "  (PLAY F1 graph y " << g.getY() << ".." << g.getBottom() << ", cutoff cell y " << d.getY() << ".." << d.getBottom() << ")" << std::endl;
                break;
            }
    }
    // The number inside a dial keeps its share of the dial when the window zooms (it was a fixed size, so Live's
    // 200 % window, a second after it opened at design size, shrank it to a quarter of the dial).
    {
        const auto shareAt = [&] (int width, const char* id)
        {
            editor.setSize (width, width * 720 / 1060);
            settle (300);
            for (auto* knob : knobs)
                if (knob->getParameterId() == id && knob->isPacked())
                    return knob->packedNumberDevicePx() / knob->packedDialDevicePx();
            return -1.0f;
        };
        for (const auto* id : { "f1_cutoff", "west_fold" })
        {
            const auto small = shareAt (1060, id), big = shareAt (2120, id);
            expect (small > 0.2f && big > 0.2f && std::abs (big - small) < 0.06f,
                    juce::String ("PLAY: ") + id + "'s number keeps its share of the dial from 100 % to 200 % (" + juce::String (small, 3) + " vs " + juce::String (big, 3) + ")");
        }
        editor.setSize (1060, 720);
        settle (300);
    }
    expect (uneven.isEmpty(), "PLAY: every card's ink has equal left and right margins, within 2 px (" + uneven + ")");

    // UI review 26: a preset that loads with three oscillators squashed the module bands (band 2 at 89 px, not
    // 104), so ENVELOPE, LFO and BODY's second rows sat on their first rows' names. The bands are one height on
    // every patch, and no two of PLAY's knobs overlap.
    {
        const auto cardHeights = [page]
        {
            std::map<juce::String, int> heights;
            for (const auto& entry : juce::StringArray::fromTokens (page->getProperties()["playCards"].toString(), ";", ""))
                if (const auto parts = juce::StringArray::fromTokens (entry, ",", ""); parts.size() == 5)
                    heights[parts[0]] = parts[4].getIntValue();
            return heights;
        };
        const auto twoOscs = cardHeights();
        const auto threeIndex = processor.getFactoryPresetNames().indexOf ("Neuro Wobble");
        expect (threeIndex >= 0, "PLAY: Neuro Wobble (three oscillators) is a factory preset");
        if (threeIndex >= 0)
        {
            processor.loadFactoryPreset (threeIndex);
            settle (400);
            editor.setSize (1061, 721);
            settle (200);
            editor.setSize (1060, 720);
            settle (400);
            const auto threeOscs = cardHeights();
            juce::String changed;
            for (const auto& [name, height] : twoOscs)
                if (threeOscs.count (name) == 0 || threeOscs.at (name) != height)
                    changed << name << " " << height << " -> " << (threeOscs.count (name) != 0 ? threeOscs.at (name) : -1) << ", ";
            expect (changed.isEmpty(), "PLAY: the module bands keep their height with three oscillators (" + changed + ")");

            std::vector<KnobControl*> shown;
            for (auto* knob : knobs)
                if (knob->getParentComponent() == page && knob->isPacked() && visibleInTree (knob))
                    shown.push_back (knob);
            juce::String overlaps;
            for (size_t a = 0; a < shown.size(); ++a)
                for (size_t b = a + 1; b < shown.size(); ++b)
                    if (shown[a]->getBounds().reduced (KnobControl::packedPadX, 0).withTrimmedTop (KnobControl::packedPadTop)
                            .intersects (shown[b]->getBounds().reduced (KnobControl::packedPadX, 0).withTrimmedTop (KnobControl::packedPadTop)))
                        overlaps << shown[a]->getParameterId() << " / " << shown[b]->getParameterId() << ", ";
            expect (overlaps.isEmpty(), "PLAY: no two knobs overlap on a three-oscillator preset (" + overlaps + ")");
            processor.loadFactoryPreset (0);
            settle (400);
        }
    }

    // ilana on beta 1.4: with more envelopes than pills fit (AMP and FILT showed), the graph showed ENV 4 but no
    // pill did, so it couldn't be picked. The pill row now scrolls so the selected source is always among its pills.
    {
        for (int env = 1; env < 5; ++env)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, env, true);
        settle (600);
        juce::Component* card = nullptr;
        juce::Button* next = nullptr;
        juce::Button* previous = nullptr;
        std::vector<juce::Component*> all;
        findAll<juce::Component> (editor, all);
        for (auto* c : all)
            if (visibleInTree (c))
            {
                if (c->getName() == "PLAY envelope card")
                    card = c;
                else if (c->getName() == "PLAY envelope card next")
                    next = dynamic_cast<juce::Button*> (c);
                else if (c->getName() == "PLAY envelope card previous")
                    previous = dynamic_cast<juce::Button*> (c);
            }
        expect (card != nullptr && next != nullptr && previous != nullptr, "PLAY: the ENVELOPE card and its arrows are found");
        if (card != nullptr && next != nullptr && previous != nullptr)
        {
            const auto shown = [card] { return juce::StringArray::fromTokens (card->getProperties()["shownSources"].toString(), ",", ""); };
            const auto selected = [card] { return card->getProperties()["selectedSource"].toString(); };
            expect (shown().size() < 5, "PLAY: the ENVELOPE card has more envelopes than pills (the case ilana hit; " + juce::String (shown().size()) + " pills)");
            juce::String missing;
            for (int i = 0; i < 10; ++i)
            {
                (i < 5 ? next : previous)->triggerClick();
                settle (30);
                if (! shown().contains (selected()))
                    missing << ModNames::source (selected().getIntValue()) << ", ";
            }
            expect (missing.isEmpty(), "PLAY: stepping through the 5 envelopes both ways, the selected one always has a pill on screen (" + missing + ")");
        }
        for (int env = 1; env < 5; ++env)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, env, false);
        settle (400);
    }
}
