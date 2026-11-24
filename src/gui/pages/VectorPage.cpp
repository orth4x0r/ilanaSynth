#include "VectorPage.h"

#include "PageHelpers.h"

VectorPage::VectorPage (IlanaSynthAudioProcessor& p)
    : processorRef (p),
      pad (p),
      on (p.apvts, "vec_on", "ON"),
      path (p.apvts, "vec_path", "PATH"),
      sync (p.apvts, "vec_sync", "SYNC"),
      shape (p.apvts, "vec_shape", "SHAPE"),
      x (p.apvts, "vec_x", "X", colour(), true),
      y (p.apvts, "vec_y", "Y", colour(), true),
      // (The motion's own colour, not the accent again: UI review 31, item 40.)
      rate (p.apvts, "vec_rate", "PATH RATE", motionColour(), false),
      // PATH RATE as a note value while SYNC is on, in the same place (as an LFO's RATE: UI review 31, item 36).
      division (p.apvts, "vec_div", "PATH RATE", motionColour(), false),
      // The pad's own wander, apart from the oscillators' analog drift
      // (UI review 6, I6-25).
      drift (p.apvts, "vec_drift", "WANDER", motionColour(), false),
      driftRate (p.apvts, "vec_drift_rate", "WANDER RATE", motionColour(), false)
{
    const char* ids[] { "vec_a", "vec_b", "vec_c", "vec_d", "vec_e", "vec_f" };
    for (int c = 0; c < VectorShape::maxCorners; ++c)
    {
        cornerMenus[(size_t) c] = std::make_unique<ComboControl> (p.apvts, ids[c], cornerName (VectorShape::square, c));
        cornerWaves[(size_t) c] = std::make_unique<VectorCornerWave> (p, c);
        addAndMakeVisible (*cornerMenus[(size_t) c]);
        addAndMakeVisible (*cornerWaves[(size_t) c]);
    }
    addAll (*this, pad, on, path, shape, x, y, rate, drift, driftRate, sync, pathShapes);
    addChildComponent (division);
    path.showAsSwitch();
    sync.showAsSwitch();
    path.setSwitchColour (motionColour());
    sync.setSwitchColour (motionColour());
    division.setTooltip ("Path rate (synced)\nSYNC is on, so PATH RATE is a note value at the host tempo: one trip round the path "
                         "per note. Turn SYNC off for Hz.");

    // PATH's shapes (UI review 31, item 36): a pick writes the path's eight points in one undo step; the
    // menu reads the shape the points make, or Custom after a drag on the pad.
    pathShapes.addItemList (pathShapeNames(), 1);
    pathShapes.setTextWhenNothingSelected ("Custom");
    pathShapes.getProperties().set ("packedMenu", true);
    pathShapes.getProperties().set ("textPrefix", "PATH");
    pathShapes.setTooltip ("Path shape\nWrites the path's eight points as a Circle, a Figure 8, a Square (every corner), "
                           "or a line across (X) or up and down (Y), in one undo step. Drag the points on the pad for your own.");
    pathShapes.onChange = [this]
    {
        if (const auto id = pathShapes.getSelectedId(); id > 0 && id != matchedPathShape())
            writePathShape (id - 1);
    };
    // (One box with its name inside, on the CORNERS box's title line.)
    shape.setPackedMenu (true);

    // The corners name the oscillators as the pad does ("OSC 1").
    for (auto& corner : cornerMenus)
    {
        auto& box = corner->getComboBox();
        // (Read first: it only matches while the shown text is the item's.)
        const auto selectedId = box.getSelectedId();
        for (int item = 0; item < box.getNumItems(); ++item)
            box.changeItemText (box.getItemId (item), box.getItemText (item).toUpperCase());
        box.setSelectedId (0, juce::dontSendNotification);
        box.setSelectedId (selectedId, juce::dontSendNotification);
    }

    // Switched on with the pad still as it comes (a square on OSC 1-4), the shape follows the patch: one corner
    // for each oscillator it has, those oscillators at the corners (ilana's idea, UI review 29).
    on.getButton().onClick = [this] { if (on.getButton().getToggleState()) fitShapeToPatch(); };
    pad.onSwitchedOn = [this] { fitShapeToPatch(); };

    // Off, the whole block dims and says why, as every card's controls
    // do (one rule, EffectRules: UI review 7, V7-21).
    for (juce::Component* control : { (juce::Component*) &path, (juce::Component*) &shape, (juce::Component*) &x,
                                      (juce::Component*) &drift, (juce::Component*) &driftRate })
        effectRules.add (*control, effectRules.isOn ("vec_on"), "VECTOR is off", [] { return FilterColours::offAlpha; });
    // Y too, and on a slider (X only, Y held at the centre) it takes the lighter dim of an idle control: it
    // looked live there and did nothing (UI review 30).
    effectRules.add (y, [this] { return readParam ("vec_on") > 0.5f && processorRef.getVectorShape() != VectorShape::slider; },
                     "The Slider shape uses X only",
                     [this] { return readParam ("vec_on") > 0.5f ? IlanaTheme::dimmedAlpha : FilterColours::offAlpha; });
    for (auto& corner : cornerMenus)
        effectRules.add (*corner, effectRules.isOn ("vec_on"), "VECTOR is off", [] { return FilterColours::offAlpha; });
    // (The corners' pictures too, as the pad: off, nothing in the card stays lit, UI review 26.)
    for (auto& wave : cornerWaves)
        effectRules.add (*wave, effectRules.isOn ("vec_on"), "VECTOR is off", [] { return FilterColours::offAlpha; });
    // At the module-off alpha while VECTOR is off, the lighter dim of one
    // idle control while only its PATH is (V7-34 with V7-21).
    for (juce::Component* control : { (juce::Component*) &rate, (juce::Component*) &division, (juce::Component*) &sync,
                                      (juce::Component*) &pathShapes })
        effectRules.add (*control, [this] { return readParam ("vec_on") > 0.5f && readParam ("vec_path") > 0.5f; },
                         "VECTOR or its PATH is off",
                         [this] { return readParam ("vec_on") > 0.5f ? IlanaTheme::dimmedAlpha : FilterColours::offAlpha; });

    updateCornerControls();

    startTimerHz (20);
}

std::array<juce::Point<float>, 8> VectorPage::pathShapePoints (int shapeIndex)
{
    std::array<juce::Point<float>, 8> points;
    for (int i = 0; i < 8; ++i)
    {
        const auto angle = juce::MathConstants<double>::twoPi * i / 8.0;
        const auto s = (float) std::sin (angle), c = (float) std::cos (angle), s2 = (float) std::sin (2.0 * angle);
        const float square[8][2] { { 0, 0 }, { 0.5f, 0 }, { 1, 0 }, { 1, 0.5f }, { 1, 1 }, { 0.5f, 1 }, { 0, 1 }, { 0, 0.5f } };
        const float line[8] { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.75f, 0.5f, 0.25f };
        switch (shapeIndex)
        {
            // (The circle is the path every patch starts with: ParameterLayout's vec_px / vec_py defaults.)
            case 0:  points[(size_t) i] = { 0.5f + 0.35f * c, 0.5f + 0.35f * s }; break;
            case 1:  points[(size_t) i] = { 0.5f + 0.4f * s, 0.5f + 0.3f * s2 }; break;
            case 2:  points[(size_t) i] = { square[i][0], square[i][1] }; break;
            case 3:  points[(size_t) i] = { line[i], 0.5f }; break;
            default: points[(size_t) i] = { 0.5f, line[i] }; break;
        }
    }
    return points;
}

int VectorPage::matchedPathShape () const
{
    for (int shapeIndex = 0; shapeIndex < pathShapeNames().size(); ++shapeIndex)
    {
        const auto points = pathShapePoints (shapeIndex);
        auto same = true;
        for (int i = 0; i < 8 && same; ++i)
            same = std::abs (readParam ("vec_px" + juce::String (i + 1)) - points[(size_t) i].x) < 0.004f
                && std::abs (readParam ("vec_py" + juce::String (i + 1)) - points[(size_t) i].y) < 0.004f;
        if (same)
            return shapeIndex + 1;
    }
    return 0;
}

void VectorPage::writePathShape (int shapeIndex)
{
    const auto index = juce::jlimit (0, pathShapeNames().size() - 1, shapeIndex);
    const auto points = pathShapePoints (index);
    processorRef.performEdit ("Vector path: " + pathShapeNames()[index], [this, &points]
    {
        for (int i = 0; i < 8; ++i)
            for (const auto& [id, value] : { std::pair<juce::String, float> { "vec_px" + juce::String (i + 1), points[(size_t) i].x },
                                             { "vec_py" + juce::String (i + 1), points[(size_t) i].y } })
                if (auto* parameter = processorRef.apvts.getParameter (id))
                {
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                    parameter->endChangeGesture();
                }
    });
    pathShapes.setSelectedId (matchedPathShape(), juce::dontSendNotification);
}

juce::String VectorPage::cornerName (int shapeIndex, int corner)
{
    switch (shapeIndex)
    {
        case VectorShape::slider:   { const char* n[] { "LEFT", "RIGHT" }; return n[juce::jlimit (0, 1, corner)]; }
        case VectorShape::triangle: { const char* n[] { "TOP", "BOTTOM RIGHT", "BOTTOM LEFT" }; return n[juce::jlimit (0, 2, corner)]; }
        case VectorShape::pentagon: { const char* n[] { "TOP", "RIGHT", "BOTTOM RIGHT", "BOTTOM LEFT", "LEFT" }; return n[juce::jlimit (0, 4, corner)]; }
        case VectorShape::hexagon:  { const char* n[] { "TOP", "UPPER RIGHT", "LOWER RIGHT", "BOTTOM", "LOWER LEFT", "UPPER LEFT" }; return n[juce::jlimit (0, 5, corner)]; }
        default:                    { const char* n[] { "TOP LEFT", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM RIGHT" }; return n[juce::jlimit (0, 3, corner)]; }
    }
}

void VectorPage::paint (juce::Graphics& g)
{
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        IlanaTheme::paintCard (g, vectorCard.toFloat(), 7.0f, colour().withAlpha (0.35f));

        // The switch is at the header's right like every card's own module
        // (UI-CONVENTIONS; V13-10), the caption after the title.
        auto header = vectorCard.reduced (12, 0).removeFromTop (28);
        IlanaTheme::paintCardHeader (g, header, "VECTOR", readParam ("vec_on") > 0.5f ? "one oscillator at each corner; drag VECTOR X or Y from the source bar onto a knob"
                                                                                       : "one oscillator at each corner", colour());

        // The controls in two boxes, as SEQ's GENERATE has them (UI review
        // 9, V9-7): where the oscillators sit, and where the point is and how it moves.
        const struct { juce::Rectangle<int> box; const char* title; } boxes[] {
            { cornersBox, "CORNERS" }, { motionBox, "POSITION AND MOTION" } };
        for (const auto& part : boxes)
        {
            IlanaTheme::paintRecessedPanel (g, part.box.toFloat(), 5.0f);
            paintSubBoxTitle (g, part.box.reduced (10, 0).withHeight (boxHeaderHeight), part.title, {}, false, 0, colour());
        }
    }

void VectorPage::resized ()
{
    auto area = getLocalBounds().reduced (12);
    // The pad gets most of the page, square at the card's full height
    // (no band above it: V7-21); the controls take the width left.
    vectorCard = area;

    auto inner = vectorCard.reduced (12, 0);
    inner.removeFromTop (30);
    inner.removeFromBottom (12);
    // (The pad is a square: it takes the card's height and the controls
    // the rest of the width, so no bare strip stands beside it, V12-3.)
    const auto padWidth = juce::jlimit (200, juce::jmax (200, inner.getWidth() - 330 - 12), inner.getHeight());
    pad.setBounds (inner.removeFromLeft (padWidth));
    inner.removeFromLeft (12);
    // The vector's on switch in its header, like every card's.
    on.setBounds (IlanaTheme::cardSwitchBounds (vectorCard, vectorCard.getY() + 14));
    // Two boxes fill the controls column's height (no empty foot): CORNERS
    // (the menus, each over a picture of its oscillator, V14-2) over
    // POSITION AND MOTION in one row (X, Y, PATH, its rate, WANDER and its rate).
    constexpr int gap = 8, padding = 12;
    const auto motionHeight = boxHeaderHeight + 2 + 72 + 8; // (a row of PLAY's rings, UI review 26)
    cornersBox = inner.removeFromTop (juce::jmax (boxHeaderHeight + 2 * 90, inner.getHeight() - gap - motionHeight));
    inner.removeFromTop (gap);
    motionBox = inner;

    // SHAPE closes the CORNERS box's title line, the PATH shapes POSITION AND MOTION's.
    shape.setBounds (juce::Rectangle<int> (200, 26).withPosition (cornersBox.getRight() - padding - 200, cornersBox.getY() + 6));
    pathShapes.setBounds (juce::Rectangle<int> (200, 26).withPosition (motionBox.getRight() - padding - 200, motionBox.getY() + 6));

    // The corners in rows: two for a slider, three for a triangle, two by two for the square, three over two
    // for a pentagon, three by two for a hexagon. Each row spans the box, so no cell stands empty.
    const auto shapeIndex = processorRef.getVectorShape();
    const auto count = VectorShape::cornerCount (shapeIndex);
    std::vector<int> rows;
    switch (count)
    {
        case 2:  rows = { 2 }; break;
        case 3:  rows = { 3 }; break;
        case 5:  rows = { 3, 2 }; break;
        case 6:  rows = { 3, 3 }; break;
        default: rows = { 2, 2 }; break;
    }

    auto corners = cornersBox.reduced (padding, 0).withTrimmedTop (boxHeaderHeight + 10).withTrimmedBottom (8);
    // Every corner has the same cell (menu, picture, LEVEL and on switch), so the rows stay even. A corner with
    // no oscillator ("OSC 4: none") shows a dimmed empty picture over the whole of its cell with "+ add OSC n".
    const auto missing = [this] (int corner)
    {
        const auto osc = processorRef.getVectorCorner (corner);
        return ! (osc >= 0 && osc < OscillatorIds::count && processorRef.isOscillatorShown (osc));
    };
    // A row of empty corners is one short "+ add OSC n" row (the menu and the add button side by side); the rows with
    // an oscillator share the height that leaves.
    constexpr int addRowHeight = 46;
    std::vector<bool> emptyRow;
    auto fullRows = 0, shortHeight = 0, firstOfRow = 0;
    for (const auto inRow : rows)
    {
        auto allMissing = true;
        for (int i = 0; i < inRow; ++i)
            allMissing = allMissing && missing (firstOfRow + i);
        firstOfRow += inRow;
        emptyRow.push_back (allMissing);
        fullRows += allMissing ? 0 : 1;
        shortHeight += allMissing ? addRowHeight + Space::s4 : 0;
    }
    const auto fullHeight = fullRows > 0 ? (corners.getHeight() - shortHeight) / fullRows : 0;

    auto first = 0;
    for (size_t r = 0; r < rows.size(); ++r)
    {
        auto row = corners.removeFromTop (r + 1 == rows.size() ? corners.getHeight() : emptyRow[r] ? addRowHeight + Space::s4 : fullHeight);
        const auto inRow = rows[r];
        const auto cellWidth = row.getWidth() / inRow;
        for (int i = 0; i < inRow; ++i)
        {
            auto cell = (i + 1 == inRow ? row : row.removeFromLeft (cellWidth)).reduced (3, 2);
            auto& menu = *cornerMenus[(size_t) (first + i)];
            auto& wave = *cornerWaves[(size_t) (first + i)];
            auto* level = cornerLevels[(size_t) (first + i)].get();
            auto* power = cornerSwitches[(size_t) (first + i)].get();
            const auto controlled = level != nullptr && power != nullptr && ! missing (first + i);
            if (level != nullptr && power != nullptr)
            {
                level->setVisible (controlled);
                power->setVisible (controlled);
            }
            if (! controlled)
            {
                // Empty corner: the menu, and beside it the "+ add OSC n" button, one row.
                auto top = cell.removeFromTop (addRowHeight);
                menu.setBounds (top.removeFromLeft (top.getWidth() / 2));
                top.removeFromLeft (Space::s8);
                wave.setBounds (top.withSizeKeepingCentre (top.getWidth(), 40));
                wave.setVisible (true);
                continue;
            }
            menu.setBounds (cell.removeFromTop (addRowHeight));
            cell.removeFromTop (Space::s2);
            // The corner oscillator's LEVEL ring and on switch side by side on one row (the switch level with the
            // ring's middle), the picture the card's full width beneath (the pad already shows the wave; balancing
            // the corners needs their levels).
            constexpr int ringColumn = 66, ringRow = 64;
            auto ringCell = cell.removeFromTop (ringRow);
            level->placeAsRing (ringCell.removeFromLeft (ringColumn), 40);
            power->setBounds ({});
            placeSwitchBeside (*power, *level, ringCell.removeFromLeft (ringColumn));
            cell.removeFromTop (Space::s4);
            wave.setBounds (cell);
            wave.setVisible (cell.getHeight() > 20);
        }
        first += inRow;
    }
    for (int c = 0; c < VectorShape::maxCorners; ++c)
    {
        cornerMenus[(size_t) c]->setVisible (c < count);
        cornerMenus[(size_t) c]->setLabelText (cornerName (shapeIndex, c));
        if (c >= count)
        {
            cornerWaves[(size_t) c]->setVisible (false);
            for (auto* control : { (juce::Component*) cornerLevels[(size_t) c].get(), (juce::Component*) cornerSwitches[(size_t) c].get() })
                if (control != nullptr)
                    control->setVisible (false);
        }
    }

    auto knobs = motionBox.reduced (padding, 0).withTrimmedTop (boxHeaderHeight + 2).withTrimmedBottom (6);
    // (Every name on one line, the PATH switch under its own: the row no longer drops it, S1-14.)
    // SYNC beside PATH RATE, the note value in RATE's place while it is on (UI review 31, item 36).
    layoutRow (knobs, { &x, &y, &path, &rate, &sync, &drift, &driftRate }, true);
    // PLAY's ring knobs (one knob style, UI review 26), at SEQ's and FM's dial, so the values are one size
    // with theirs (14 px, not 16: UI review 31, item 39); the switches level with their middles.
    for (auto* knob : { &x, &y, &rate, &drift, &driftRate })
        knob->placeAsRing (knobs.withX (knob->getX()).withWidth (knob->getWidth()), 40);
    division.placeAsRing (knobs.withX (rate.getX()).withWidth (rate.getWidth()), 40);
    for (auto* toggle : { &path, &sync })
        placeSwitchBeside (*toggle, x, toggle->getBounds().withY (x.getY()));
}

void VectorPage::placeSwitchBeside (ToggleControl& toggle, KnobControl& knob, juce::Rectangle<int> cell)
{
    const auto middle = knob.getY() + juce::roundToInt (knob.getDialCentre().y);
    const auto nameTop = knob.getY() + knob.getPackedNameArea().getY();
    toggle.setPackedName (ToggleControl::NamePlace::below);
    toggle.setBelowNameSize (knob.packedNameUnits());
    toggle.setBounds (juce::Rectangle<int> (cell.getCentreX() - knob.getWidth() / 2, middle - 7, knob.getWidth(),
                                            knob.getBottom() - (middle - 7)));
    toggle.setNameDrop (juce::jmax (0, nameTop - (middle - 7 + 15 + 1)));
}

void VectorPage::updateCornerControls ()
{
    for (int c = 0; c < VectorShape::maxCorners; ++c)
    {
        const auto osc = juce::jlimit (0, OscillatorIds::count - 1, processorRef.getVectorCorner (c));
        if (cornerOsc[(size_t) c] == osc && cornerLevels[(size_t) c] != nullptr)
            continue;

        cornerOsc[(size_t) c] = osc;
        const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) osc]);
        const auto oscColour = IlanaTheme::oscColour (osc);
        cornerLevels[(size_t) c] = std::make_unique<KnobControl> (processorRef.apvts, prefix + "_level", "LEVEL", oscColour, false);
        cornerSwitches[(size_t) c] = std::make_unique<ToggleControl> (processorRef.apvts, prefix + "_on", "ON");
        cornerSwitches[(size_t) c]->showAsSwitch();
        cornerSwitches[(size_t) c]->setSwitchColour (oscColour);
        const auto tip = "OSC " + juce::String (osc + 1) + " level and on switch\nThe same controls as its card on PLAY.";
        cornerLevels[(size_t) c]->setTooltip (tip);
        addChildComponent (*cornerLevels[(size_t) c]);
        addChildComponent (*cornerSwitches[(size_t) c]);
    }
}

void VectorPage::fitShapeToPatch ()
{
    if (processorRef.getVectorShape() != VectorShape::square)
        return;
    for (int c = 0; c < 4; ++c)
        if (processorRef.getVectorCorner (c) != c)
            return;

    std::vector<int> shown;
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        if (processorRef.isOscillatorShown (osc))
            shown.push_back (osc);
    if (shown.size() < 2 || shown.size() == 4)
        return;

    const auto fitted = shown.size() == 2 ? VectorShape::slider : shown.size() == 3 ? VectorShape::triangle
                      : shown.size() == 5 ? VectorShape::pentagon : VectorShape::hexagon;
    processorRef.performEdit ("Vector shape", [this, fitted, shown]
    {
        const auto write = [this] (const juce::String& id, float value)
        {
            if (auto* parameter = processorRef.apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                parameter->endChangeGesture();
            }
        };
        write ("vec_shape", (float) fitted);
        const char* ids[] { "vec_a", "vec_b", "vec_c", "vec_d", "vec_e", "vec_f" };
        for (size_t c = 0; c < shown.size(); ++c)
            write (ids[c], (float) shown[c]);
        // The point at the shape's balance, where every corner sounds.
        const auto centre = VectorShape::balance (fitted);
        write ("vec_x", centre.x);
        write ("vec_y", centre.y);
    });
}

float VectorPage::readParam (const juce::String& id) const
{
    const auto* value = processorRef.apvts.getRawParameterValue (id);
    return value != nullptr ? value->load() : 0.0f;
}

void VectorPage::timerCallback ()
{
    if (! isVisible())
        return;

    // A corner menu names an oscillator the patch hasn't added as such
    // (the pad's caption says "none"; UI review 9, I9-17).
    auto shown = 0;
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        shown |= processorRef.isOscillatorShown (osc) ? 1 << osc : 0;
        // (A switched-off oscillator is a different menu text: bit 8 up.)
        shown |= processorRef.isOscillatorShown (osc) && readParam (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_on") < 0.5f ? 1 << (osc + 8) : 0;
    }

    // (The shape and the corners' picks: a change relayouts the corner rows.)
    auto corners = (juce::int64) processorRef.getVectorShape();
    for (int corner = 0; corner < VectorShape::maxCorners; ++corner)
        corners = corners * 16 + processorRef.getVectorCorner (corner) + 1;
    if (corners != cornerKey)
    {
        cornerKey = corners;
        updateCornerControls();
        resized();
    }

    if (shown != shownOscillators)
    {
        shownOscillators = shown;
        resized(); // (a corner row with no oscillator folds to its menus)
        for (auto& corner : cornerMenus)
        {
            auto& box = corner->getComboBox();
            const auto selected = box.getSelectedId();
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
            {
                // Worded as the pad's corners and every label: "OSC 4: none", "OSC 3: off" (V12-8).
                box.changeItemText (osc + 1, "OSC " + juce::String (osc + 1) + ((shown >> osc) & 1 ? ((shown >> (osc + 8)) & 1 ? ": off" : "") : ": none"));
                // A corner can't sound an oscillator that isn't there: the
                // choice is greyed unless it is already the corner's (V11-25).
                box.setItemEnabled (osc + 1, ((shown >> osc) & 1) != 0 || osc + 1 == selected);
            }
            box.setSelectedId (selected, juce::dontSendNotification);
        }
    }

    for (auto& corner : cornerMenus)
    {
        const auto id = corner->getComboBox().getSelectedId();
        if (id >= 1 && id <= OscillatorIds::count)
            corner->setTint (IlanaTheme::oscColour (id - 1));
    }

    const auto active = readParam ("vec_on") > 0.5f;
    if (const auto alpha = active ? 1.0f : FilterColours::offAlpha; pad.getAlpha() != alpha)
        pad.setAlpha (alpha);
    effectRules.apply();

    // The corners' LEVEL rings and switches dim with the card; a LEVEL also while its oscillator is off.
    for (int c = 0; c < VectorShape::maxCorners; ++c)
        if (auto* level = cornerLevels[(size_t) c].get(); level != nullptr && cornerSwitches[(size_t) c] != nullptr)
        {
            const auto oscOn = readParam (juce::String (OscillatorIds::prefixes[(size_t) cornerOsc[(size_t) c]]) + "_on") > 0.5f;
            const auto alpha = ! active ? FilterColours::offAlpha : oscOn ? 1.0f : IlanaTheme::dimmedAlpha;
            if (level->getAlpha() != alpha)
                level->setAlpha (alpha);
            if (const auto switchAlpha = active ? 1.0f : FilterColours::offAlpha; cornerSwitches[(size_t) c]->getAlpha() != switchAlpha)
                cornerSwitches[(size_t) c]->setAlpha (switchAlpha);
        }

    // PATH RATE in Hz, or as a note value while SYNC is on; the shape menu names the path's shape.
    const auto synced = readParam ("vec_sync") > 0.5f;
    if (rate.isVisible() == synced)
    {
        rate.setVisible (! synced);
        division.setVisible (synced);
    }
    if (const auto matched = matchedPathShape(); pathShapes.getSelectedId() != matched)
        pathShapes.setSelectedId (matched, juce::dontSendNotification);

    if (shownActive != active)
    {
        shownActive = active;
        repaint (vectorCard);
    }
}
