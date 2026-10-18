// The PATCH tab's row of effects under the canvas (src/gui/FxCarousel.h, ilana
// 2026-10-15: "small carousel of effects under the patcher"): every library
// effect has a tile, a click adds the effect before OUT, a drag up onto a
// cable drops it into that cable, a sweep along the row scrolls it. Included
// by Snapshot.cpp after FxLfoUiTests.h; runs from runUiTests
// (ILANA_UITEST_ONLY=FC for just these). ILANA_SNAPSHOT_CAROUSEL=1 writes
// pictures instead.
#pragma once

inline void runFxCarouselUiTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    constexpr int reverb = 13, drive = 2, delay = 9;

    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    settle (200);
    processor.addFxEffect (drive);
    processor.addFxEffect (delay);
    processor.setFxPatcherOn (true);
    editor.showPage ("PATCH");
    settle (400);

    auto* view = OscRoutingUi::findRoutingView (editor);
    auto* carousel = findChild<FxCarousel> (editor);
    expect (view != nullptr && carousel != nullptr, "CAROUSEL: PATCH shows the canvas and the row of effects");
    if (view == nullptr || carousel == nullptr)
        return;

    auto expected = 0;
    for (const auto& group : fxLibraryGroups())
        for (const auto& entry : group.entries)
            expected += entry.twin > 0 ? 2 : 1;
    expect (carousel->numTiles() == expected && expected > 40, "CAROUSEL: one tile for every effect in the library (" + juce::String (carousel->numTiles()) + ")");
    expect (carousel->isVisible() && carousel->getBottom() <= view->getParentComponent()->getHeight() && carousel->getY() >= view->getBottom(),
            "CAROUSEL: the row sits under the canvas");

    const auto effectBoxes = [view]
    {
        std::vector<const PatchGraph::Node*> found;
        for (const auto& node : view->getGraph().nodes())
            if (! node.isFixed() && node.id < (1u << 20))
                found.push_back (&node);
        std::sort (found.begin(), found.end(), [] (const auto* a, const auto* b) { return a->x < b->x; });
        return found;
    };
    const auto tileOf = [carousel] (int type)
    {
        for (int i = 0; i < carousel->numTiles(); ++i)
            if (carousel->tileAt (i).type == type)
                return i;
        return -1;
    };
    const auto centreOf = [carousel] (int index) { return carousel->tileBounds (index).toFloat().getCentre(); };

    // A tile that is on screen is clicked: Reverb is added, wired before OUT.
    const auto reverbTile = tileOf (reverb);
    expect (reverbTile >= 0 && carousel->viewport().contains (centreOf (reverbTile).toInt()), "CAROUSEL: Reverb's tile is in view");
    const auto before = effectBoxes().size();
    carousel->mouseDown (PatchCableUi::mouse (*carousel, centreOf (reverbTile), true));
    carousel->mouseUp (PatchCableUi::mouse (*carousel, centreOf (reverbTile), false, false, centreOf (reverbTile)));
    settle (300);
    expect (effectBoxes().size() == before + 1, "CAROUSEL: a click adds the effect");
    auto hasReverb = false;
    for (const auto* node : effectBoxes())
        hasReverb = hasReverb || node->kind == reverb;
    expect (hasReverb, "CAROUSEL: the new box is a Reverb");

    // A drag up onto the cable between the first two effects drops it in.
    const auto boxes = effectBoxes();
    expect (boxes.size() >= 3, "CAROUSEL: three effects to drop between");
    if (boxes.size() < 3)
        return;
    const auto firstId = boxes[0]->id, secondId = boxes[1]->id;
    const auto target = (view->portPosition ({ firstId, 0 }, false) + view->portPosition ({ secondId, 0 }, true)) * 0.5f;
    const auto to = carousel->getLocalPoint (view, target * view->getZoom());
    const auto dropTile = tileOf (delay);
    expect (dropTile >= 0 && carousel->viewport().contains (centreOf (dropTile).toInt()), "CAROUSEL: Delay's tile is in view");
    const auto from = centreOf (dropTile);
    carousel->mouseDown (PatchCableUi::mouse (*carousel, from, true));
    for (int step = 1; step <= 10; ++step)
        carousel->mouseDrag (PatchCableUi::mouse (*carousel, from + (to - from) * ((float) step / 10.0f), true, false, from));
    carousel->mouseUp (PatchCableUi::mouse (*carousel, to, false, false, from));
    settle (300);
    expect (effectBoxes().size() == before + 2, "CAROUSEL: a drag adds the effect");
    auto joined = false;
    for (const auto& node : view->getGraph().nodes())
        if (! node.isFixed() && node.id < (1u << 20) && node.id != firstId && node.id != secondId && node.kind == delay)
        {
            auto in = false, out = false;
            for (const auto& cable : view->getGraph().cables())
            {
                in = in || (cable.from.node == firstId && cable.to.node == node.id);
                out = out || (cable.from.node == node.id && cable.to.node == secondId);
            }
            joined = joined || (in && out);
        }
    expect (joined, "CAROUSEL: a tile dropped on a cable lands in that cable");

    // A sweep along the row scrolls it; the wheel does too; it stays in range.
    carousel->scrollTo (0.0f);
    const auto start = centreOf (0);
    carousel->mouseDown (PatchCableUi::mouse (*carousel, start, true));
    carousel->mouseDrag (PatchCableUi::mouse (*carousel, start - juce::Point<float> (120.0f, 0.0f), true, false, start));
    carousel->mouseUp (PatchCableUi::mouse (*carousel, start - juce::Point<float> (120.0f, 0.0f), false, false, start));
    expect (carousel->getScroll() > 100.0f, "CAROUSEL: a sweep along the row scrolls it");
    carousel->scrollTo (1.0e6f);
    const auto last = carousel->tileBounds (carousel->numTiles() - 1);
    expect (last.getRight() <= carousel->viewport().getRight() + 1 && last.getRight() >= carousel->viewport().getRight() - 1,
            "CAROUSEL: the row stops at its last tile");
    carousel->scrollTo (-50.0f);
    expect (carousel->getScroll() == 0.0f, "CAROUSEL: the row does not scroll before its first tile");
}
