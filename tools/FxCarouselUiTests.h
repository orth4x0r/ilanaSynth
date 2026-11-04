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

    // The category picker: ALL then one chip per category; a click shows that category's tiles only.
    expect (carousel->numChips() == 1 + (int) fxLibraryGroups().size() && carousel->getShownGroup() == -1, "CAROUSEL: a picker with ALL and every category");
    {
        const auto chip = carousel->chipBounds (2).toFloat().getCentre(); // (DRIVE)
        carousel->mouseDown (PatchCableUi::mouse (*carousel, chip, true));
        carousel->mouseUp (PatchCableUi::mouse (*carousel, chip, false, false, chip));
        auto driveCount = 0;
        for (const auto& entry : fxLibraryGroups()[1].entries)
            driveCount += entry.twin > 0 ? 2 : 1;
        auto allDrive = true;
        for (int i = 0; i < carousel->numTiles(); ++i)
            allDrive = allDrive && carousel->tileAt (i).group == 1;
        expect (carousel->getShownGroup() == 1 && carousel->numTiles() == driveCount && allDrive, "CAROUSEL: a category chip shows only that category's effects");
        const auto all = carousel->chipBounds (0).toFloat().getCentre();
        carousel->mouseDown (PatchCableUi::mouse (*carousel, all, true));
        carousel->mouseUp (PatchCableUi::mouse (*carousel, all, false, false, all));
        expect (carousel->getShownGroup() == -1 && carousel->numTiles() == expected, "CAROUSEL: ALL brings every effect back");
    }

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

    // ILANA_UITEST_SHOTS=<dir>: pictures of the drop, the picking box and the paste, for review.
    const auto shotsDir = juce::SystemStats::getEnvironmentVariable ("ILANA_UITEST_SHOTS", "");
    const auto shot = [&] (const char* name)
    {
        if (shotsDir.isNotEmpty())
        {
            settle (150);
            save (editor, juce::File (shotsDir).getChildFile (name));
        }
    };

    // A tile dropped on empty canvas lands there on its own, wired to nothing (ilana 2026-11-04).
    {
        const auto cablesBefore = processor.getFxPatchGraph().cables().size();
        const auto countBefore = effectBoxes().size();
        carousel->scrollTo (0.0f);
        const auto tile = tileOf (reverb) >= 0 && carousel->viewport().contains (centreOf (tileOf (reverb)).toInt()) ? tileOf (reverb) : 0;
        const auto from = centreOf (tile);
        // (The empty space under the boxes, inside the canvas.)
        // (Straight up from the tile: a sideways sweep scrolls the row instead.)
        const auto above = view->getLocalPoint (carousel, from);
        const auto empty = carousel->getLocalPoint (view, juce::Point<float> (above.x, (float) view->getHeight() - 30.0f));
        carousel->mouseDown (PatchCableUi::mouse (*carousel, from, true));
        for (int step = 1; step <= 10; ++step)
            carousel->mouseDrag (PatchCableUi::mouse (*carousel, from + (empty - from) * ((float) step / 10.0f), true, false, from));
        carousel->mouseUp (PatchCableUi::mouse (*carousel, empty, false, false, from));
        settle (300);
        const PatchGraph::Node* dropped = nullptr;
        for (const auto* node : effectBoxes())
        {
            auto wired = false;
            for (const auto& cable : view->getGraph().cables())
                wired = wired || cable.from.node == node->id || cable.to.node == node->id;
            if (! wired)
                dropped = node;
        }
        expect (effectBoxes().size() == countBefore + 1 && dropped != nullptr && processor.getFxPatchGraph().cables().size() == cablesBefore,
                "CAROUSEL: a tile dropped on empty canvas lands there, wired to nothing (" + juce::String ((int) countBefore) + " -> "
                    + juce::String ((int) effectBoxes().size()) + " boxes, " + juce::String ((int) cablesBefore) + " -> "
                    + juce::String ((int) processor.getFxPatchGraph().cables().size()) + " cables, " + (dropped != nullptr ? "one unwired" : "none unwired") + ")");
        shot ("patch-drop-unwired.png");
    }

    // Copy and paste: two wired effects picked, Ctrl+C, Ctrl+V. The copies keep the cable between them and nothing
    // else (no route from the end of one chain into the other), with their settings.
    {
        auto boxesNow = effectBoxes();
        const PatchGraph::Node* first = nullptr;
        const PatchGraph::Node* second = nullptr;
        for (const auto& cable : view->getGraph().cables())
            for (const auto* a : boxesNow)
                for (const auto* b : boxesNow)
                    if (first == nullptr && cable.from.node == a->id && cable.to.node == b->id)
                    {
                        first = a;
                        second = b;
                    }
        expect (first != nullptr, "PASTE: two wired effects to copy");
        if (first == nullptr)
            return;
        const auto firstKind = first->kind, secondKind = second->kind;
        const auto oldIds = std::set<PatchGraph::NodeId> { first->id, second->id };
        const auto cablesBefore = processor.getFxPatchGraph().cables().size();
        {
            // (The picking box drawn round the two, pictured mid-drag.)
            const auto zoom = view->getZoom();
            const auto box = view->nodeBounds (*first).getUnion (view->nodeBounds (*second));
            const auto from = (box.getTopLeft() - juce::Point<float> (12.0f, 12.0f)) * zoom;
            const auto to = (box.getBottomRight() + juce::Point<float> (8.0f, 8.0f)) * zoom;
            view->mouseDown (PatchCableUi::mouse (*view, from, true));
            for (int step = 1; step <= 6; ++step)
                view->mouseDrag (PatchCableUi::mouse (*view, from + (to - from) * ((float) step / 6.0f), true, false, from));
            shot ("patch-select-box.png");
            view->mouseUp (PatchCableUi::mouse (*view, to, false, false, from));
        }
        view->setSelection ({ first->id, second->id });
        view->keyPressed (juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0));
        view->keyPressed (juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0));
        settle (300);
        shot ("patch-paste.png");
        const auto picked = view->getSelection();
        auto kinds = std::multiset<int>();
        auto fresh = true;
        for (const auto id : picked)
        {
            fresh = fresh && oldIds.count (id) == 0;
            if (const auto* node = view->getGraph().find (id))
                kinds.insert (node->kind);
        }
        auto inside = 0, outside = 0;
        for (const auto& cable : view->getGraph().cables())
        {
            const auto a = std::find (picked.begin(), picked.end(), cable.from.node) != picked.end();
            const auto b = std::find (picked.begin(), picked.end(), cable.to.node) != picked.end();
            inside += a && b ? 1 : 0;
            outside += a != b ? 1 : 0;
        }
        expect (picked.size() == 2 && fresh && kinds == std::multiset<int> { firstKind, secondKind } && inside == 1 && outside == 0
                    && processor.getFxPatchGraph().cables().size() == cablesBefore + 1,
                "PASTE: the copies come in picked, with the cable between them and nothing else wired ("
                    + juce::String ((int) picked.size()) + " boxes, " + juce::String (inside) + " inside, " + juce::String (outside) + " outside)");
        processor.getUndoManager().undo();
        settle (300);
        expect (processor.getFxPatchGraph().cables().size() == cablesBefore, "PASTE: one undo takes the paste back");
    }
}
