#include "MatrixPage.h"

#include "PageHelpers.h"

namespace
{
// The modulated knob a destination is (its parameter), for the matrix's
// curve dock: the first parameter whose knob draws that destination's ring
// (an oscillator's pitch is its SEMI). Empty when no knob draws it.
inline juce::String knobParameterFor (IlanaSynthAudioProcessor& processor, int destination)
{
    static std::map<int, juce::String> ids;
    static bool built = false;
    if (! built)
    {
        built = true;
        for (auto* parameter : processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                if (const auto d = modRingConfigFor (withId->paramID).destination; d != 0)
                    if (const auto found = ids.find (d); found == ids.end() || withId->paramID.endsWith ("_semi"))
                        ids[d] = withId->paramID;
    }
    const auto found = ids.find (destination);
    return found != ids.end() ? found->second : juce::String();
}
} // namespace

void MatrixDockTarget::setSlot (int newSlot)
{
    slot = newSlot;
    refresh();
}

void MatrixDockTarget::refresh ()
{
    const auto routing = processorRef.readModSlot (juce::jmax (0, slot));
    const auto wantedDestination = slot >= 0 ? routing.destination : 0;
    juce::String key (wantedDestination);
    std::vector<int> others;
    if (wantedDestination != 0)
        for (int i = 0; i < Mod::maxSlots; ++i)
            if (const auto other = processorRef.readModSlot (i); i != slot && other.destination == wantedDestination && other.source != Mod::Source::None)
            {
                others.push_back (i);
                key << "," << i << ":" << (int) other.source << ":" << juce::roundToInt (other.depth * 100.0f) << (other.bypass ? "b" : "");
            }

    if (key == shownKey)
        return;
    shownKey = key;

    if (wantedDestination != destination || (knob == nullptr && wantedDestination != 0))
    {
        destination = wantedDestination;
        knob.reset();
        const auto id = destination != 0 ? knobParameterFor (processorRef, destination) : juce::String();
        if (id.isNotEmpty())
        {
            knob = std::make_unique<KnobControl> (processorRef.apvts, id, ModNames::destinationParts (destination).control.toUpperCase());
            // (Not the knob "Show destination" goes to: that one is on its page.)
            knob->getProperties().set ("matrixDock", true);
            addAndMakeVisible (*knob);
        }
    }

    chips.clear();
    for (const auto other : others)
    {
        const auto route = processorRef.readModSlot (other);
        const auto colour = modSourceColour ((int) route.source);
        const auto percent = Mod::shownPercent (route.depth, Mod::isBipolarRoute (route));
        auto chip = std::make_unique<juce::TextButton> (ModNames::source ((int) route.source, &processorRef).toUpperCase() + "  "
                                                        + (percent > 0 ? "+" : "") + juce::String (percent) + "%");
        chip->setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised.interpolatedWith (colour, 0.12f));
        chip->setColour (juce::TextButton::textColourOffId, colour.interpolatedWith (juce::Colours::white, 0.35f).withAlpha (route.bypass ? 0.5f : 1.0f));
        chip->setTooltip ("Row " + juce::String (other + 1) + ": " + ModNames::source ((int) route.source, &processorRef)
                          + " also moves this knob" + (route.bypass ? " (switched off)" : "") + ".\nClick to open its curve here.");
        chip->onClick = [this, other] { if (onPickRow != nullptr) onPickRow (other); };
        addAndMakeVisible (*chip);
        chips.push_back (std::move (chip));
    }

    resized();
    repaint();
}

void MatrixDockTarget::paint (juce::Graphics& g)
{
    auto column = textColumn();
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
    g.setColour (IlanaTheme::Ui::text2);
    g.drawText ("INTO", column.removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (IlanaTheme::Ui::text);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
    IlanaTheme::drawFitted (g, destination != 0 ? ModNames::destination (destination, processorRef) : juce::String ("No destination yet"),
                            column.removeFromTop (20), juce::Justification::centredLeft, 1);
    column.removeFromTop (6);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
    g.setColour (IlanaTheme::Ui::text2);
    g.drawText ("ALSO INTO IT", column.removeFromTop (14), juce::Justification::centredLeft);

    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
    g.setColour (IlanaTheme::Ui::text3);
    if (destination == 0)
        IlanaTheme::drawFitted (g, "Pick a destination in the row.", column.removeFromTop (20), juce::Justification::centredLeft, 1);
    else if (chips.empty())
        IlanaTheme::drawFitted (g, "No other route.", column.removeFromTop (20), juce::Justification::centredLeft, 1);
}

void MatrixDockTarget::resized ()
{
    // The knob: a square on the left, as tall as the area (with its name under the dial).
    const auto side = juce::jlimit (0, 120, juce::jmin (getHeight(), getWidth() / 3));
    if (knob != nullptr)
        knob->placeAsRing (getLocalBounds().removeFromLeft (side), 64);  // value inside the ring, name under it

    // The chips flow in rows under ALSO INTO IT.
    auto column = textColumn().withTrimmedTop (14 + 20 + 6 + 14);
    auto x = column.getX(), y = column.getY();
    const auto font = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true));
    for (auto& chip : chips)
    {
        const auto width = juce::jmin (column.getWidth(), juce::GlyphArrangement::getStringWidthInt (font, chip->getButtonText()) + 20);
        if (x + width > column.getRight() && x > column.getX())
        {
            x = column.getX();
            y += chipHeight + 4;
        }
        chip->setBounds (x, y, width, chipHeight);
        chip->setVisible (y + chipHeight <= column.getBottom());
        x += width + 6;
    }
}

juce::Rectangle<int> MatrixDockTarget::textColumn () const
{
    const auto side = juce::jlimit (0, 120, juce::jmin (getHeight(), getWidth() / 3));
    return getLocalBounds().withTrimmedLeft (knob != nullptr ? side + 12 : 0);
}

MatrixPage::MatrixPage (IlanaSynthAudioProcessor& p)
    : processorRef (p)
{
    for (int i = 0; i < Mod::maxSlots; ++i)
    {
        auto row = std::make_unique<MatrixRow> (p, i);
        row->getCurve().onOpenRemap = [this] (int slot) { toggleRemap (slot); };
        row->onDuplicateClicked = [this] (int slot) { showMergeMenu (slot); };
        row->onRowClicked = [this] (int slot) { showRemap (slot); };
        row->onRowMenu = [this] (int slot) { showRowMenu (slot); };
        row->onRowMenuChoice = [this, i] (int choice) { rowMenuChoice (i, choice); };
        list.addChildComponent (*row);
        rows.push_back (std::move (row));
    }

    // In the header while the matrix is empty; once it has routings,
    // the row after the last one adds the next (S7-35).
    addButton.setButtonText ("+  ADD MODULATION");
    addButton.setTooltip ("Add a routing.  You can also drag any source chip, macro name or LFO card onto a knob.");
    addButton.onClick = [this] { addRouting(); };
    addAndMakeVisible (addButton);
    addRow.setTooltip (addButton.getTooltip());
    addRow.onClick = [this] { addRouting(); };
    list.addChildComponent (addRow);

    // A long matrix: type part of a source or destination name to show
    // only those rows (review 10, S10-8). Shown from a few routes up.
    filterField.setTextToShowWhenEmpty ("Filter by source or destination", IlanaTheme::Ui::text3.withAlpha (0.7f));
    filterField.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
    filterField.setColour (juce::TextEditor::backgroundColourId, IlanaTheme::Ui::well);
    filterField.setColour (juce::TextEditor::outlineColourId, IlanaTheme::Ui::line);
    filterField.setColour (juce::TextEditor::textColourId, IlanaTheme::Ui::text);
    filterField.setIndents (8, 4);
    filterField.setTooltip ("Filter\nShows only the routings whose source or destination name contains this text.");
    filterField.onTextChange = [this] { updateRows (true); };
    filterField.onEscapeKey = [this] { filterField.clear(); };
    addChildComponent (filterField);

    // Shown while rows repeat a routing: folds each pair into one row.
    mergeButton.setTooltip ("Merge repeated routings\nRows with the same source and destination add up; each pair becomes "
                            "one row with the two depths added (the sound stays the same).");
    mergeButton.onClick = [this] { mergeAll(); };
    addChildComponent (mergeButton);

    setTooltip ("Rows are grouped by source.  Click SOURCE, AMOUNT or DESTINATION to sort the rows (again to reverse, a "
                "third time for slot order).");

    // One-click starting points for an empty matrix.
    for (size_t i = 0; i < starterRoutings().size(); ++i)
    {
        const auto& starter = starterRoutings()[i];
        auto button = std::make_unique<juce::TextButton> (starter.label);
        button->setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised.interpolatedWith (modSourceColour ((int) starter.source), 0.08f));
        button->setColour (juce::TextButton::textColourOffId, modSourceColour ((int) starter.source).interpolatedWith (juce::Colours::white, 0.4f));
        button->setTooltip (juce::String (starter.tip) + "\nAdds the routing; change it in its row afterwards.");
        button->onClick = [this, i] { addStarter (starterRoutings()[i]); };
        addChildComponent (*button);
        starterButtons.push_back (std::move (button));
    }

    // Quick shapes beside an open remap curve (the dock's spare width,
    // V8-20): one click sets the routing's curve.
    for (int shape = 0; shape < RemapEditor::getShapeNames().size(); ++shape)
    {
        auto tile = std::make_unique<RemapShapeTile> (shape);
        tile->onClick = [this, shape]
        {
            if (remapEditor != nullptr)
                remapEditor->applyShape (shape);
        };
        addChildComponent (*tile);
        shapeTiles.push_back (std::move (tile));
    }

    // A click on a source's heading switches its routes off or on.
    list.onHeadingClick = [this] (int source) { toggleSourceRoutes (source); };
    dockTarget.onPickRow = [this] (int slot) { toggleRemap (slot); };
    addChildComponent (dockTarget);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    updateRows();
    startTimerHz (20);
}

void MatrixPage::paint (juce::Graphics& g)
{
    IlanaTheme::paintPageBackground (g, getLocalBounds());

    const auto used = (int) visibleRows.size();

    const auto titleRight = (filterField.isVisible() ? filterField.getX() : mergeButton.isVisible() ? mergeButton.getX() : addButton.isVisible() ? addButton.getX() : getWidth() - 12) - 12;
    paintSectionTitle (g, "MODULATION", juce::Rectangle<int> (headingX, 12, juce::jmax (100, titleRight - headingX), headingHeight),
                       // A fragment caption (I9-9): the how-to is a hint
                       // line in the dock below.
                       (totalUsed > used ? juce::String (used) + " of " + juce::String (totalUsed) + " routes shown"
                                            : juce::String (used) + " of " + juce::String (Mod::maxSlots) + " routes")
                       + (numDuplicates > 0 ? juce::String::fromUTF8 (" \xc2\xb7 ") + repeatText : juce::String())
                       + (numIdle > 0 ? juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (numIdle) + " into a module that is off (dimmed)"
                                      : juce::String())
                       + (soloSlot >= 0 ? juce::String::fromUTF8 (" \xc2\xb7 row ") + juce::String (rows[(size_t) soloSlot]->getDisplayNumber())
                                              + " soloed" : juce::String()));

    // The hint, at the right of the title line while the matrix is new to
    // the user (a few routes; the filter field takes the place after that),
    // there is nothing else to report and the whole sentence fits: never cut
    // short (V11-8).
    if (! visibleRows.empty() && totalUsed < 5 && numDuplicates == 0 && numIdle == 0 && dockArea.isEmpty())
    {
        const juce::String hint ("Drag a source onto any knob, then drag its ring on the knob to set the depth.");
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::label);
        const auto room = juce::Rectangle<int> (headingX, 12, titleRight - headingX, headingHeight).withTrimmedLeft (280);
        if (juce::GlyphArrangement::getStringWidthInt (juce::Font (font), hint) + 4 <= room.getWidth())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (font);
            g.drawText (hint, room, juce::Justification::centredRight, false);
        }
    }

    // An empty matrix has no columns to head: just the ways in.
    if (totalUsed == 0)
    {
        paintEmptyState (g);
        return;
    }

    if (visibleRows.empty())
    {
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText ("No routing matches the filter.", getLocalBounds().withTrimmedTop (80), juce::Justification::centredTop);
        return;
    }

    // Column headings, aligned with MatrixRow's layout. #, SOURCE,
    // AMOUNT and DESTINATION sort the rows (click again to reverse,
    // a third time for slot order).
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));

    for (const auto& heading : headings())
    {
        const auto sortable = heading.sort != Sort::none && heading.sort != Sort::slot;
        const auto sorted = sortable && heading.sort == sort;
        const auto hot = heading.sort != Sort::none && heading.area.contains (getMouseXYRelative()) && isMouseOver (true);
        g.setColour (sorted || hot ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
        g.drawText (heading.text, heading.area, juce::Justification::centredLeft);

        // A sortable heading carries a faint up-down mark, solid once
        // it sorts (UI review 6, S6-29).
        if (sortable)
        {
            const auto textWidth = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), heading.text);
            const auto mark = juce::Rectangle<float> (8.0f, 10.0f).withCentre ({ (float) (heading.area.getX() + textWidth + 9),
                                                                                (float) heading.area.getCentreY() });
            juce::Path up, down;
            up.addTriangle (mark.getCentreX() - 3.0f, mark.getCentreY() - 1.0f, mark.getCentreX() + 3.0f, mark.getCentreY() - 1.0f,
                            mark.getCentreX(), mark.getY());
            down.addTriangle (mark.getCentreX() - 3.0f, mark.getCentreY() + 1.0f, mark.getCentreX() + 3.0f, mark.getCentreY() + 1.0f,
                              mark.getCentreX(), mark.getBottom());
            const auto colour = IlanaTheme::Ui::text2;
            g.setColour (colour.withAlpha (sorted ? (descending ? 0.25f : 1.0f) : (hot ? 0.7f : 0.35f)));
            g.fillPath (up);
            g.setColour (colour.withAlpha (sorted ? (descending ? 1.0f : 0.25f) : (hot ? 0.7f : 0.35f)));
            g.fillPath (down);
        }
    }

    paintDock (g);
}

void MatrixPage::paintDock (juce::Graphics& g)
{
    if (dockArea.isEmpty())
        return;

    // (Never a text box: a dock with room opens a curve by itself, V14-4.)
    if (remapEditor == nullptr)
        return;

    // The routing the curve belongs to.
    const auto slot = processorRef.readModSlot (remapEditor->getSlotIndex());
    auto info = dockArea.withTrimmedLeft (remapEditor->getRight() - dockArea.getX() + 16).reduced (0, 8);
    const auto colour = modSourceColour ((int) slot.source);

    g.setColour (IlanaTheme::Ui::text2);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
    g.drawText ("ROW " + juce::String (rows[(size_t) remapEditor->getSlotIndex()]->getDisplayNumber()) + juce::String::fromUTF8 (" \xc2\xb7 LIVE"),
                info.removeFromTop (16), juce::Justification::centredLeft);

    // The title strip over the curve names the route; here the numbers that
    // curve is sending right now (V15-13): the input along it, what it
    // answers, and that times the row's amount.
    {
        readoutArea = info.removeFromTop (46);
        const auto liveIn = remapEditor->getLiveInput();
        const auto out = remapEditor->getLiveOutput();
        const auto signedText = [] (float value, int places) { return (value >= 0.0f ? "+" : "") + juce::String (value, places); };
        const juce::String captions[] { "IN", "OUT", "SENDS" };
        const juce::String values[] { liveIn >= 0.0f ? juce::String (liveIn, 2) : juce::String ("--"),
                                      liveIn >= 0.0f ? signedText (out, 2) : juce::String ("--"),
                                      liveIn >= 0.0f ? signedText (out * slot.depth * 100.0f, 0) + "%" : juce::String ("--") };
        const auto cellWidth = juce::jmin (130, readoutArea.getWidth() / 3);
        auto cells = readoutArea;
        for (int i = 0; i < 3; ++i)
        {
            auto cell = cells.removeFromLeft (cellWidth);
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (captions[i], cell.removeFromTop (14), juce::Justification::centredLeft);
            g.setColour (i == 0 ? IlanaTheme::Ui::text : colour);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
            IlanaTheme::drawFitted (g, values[i], cell.reduced (0, 0).withTrimmedRight (8), juce::Justification::centredLeft, 1);
            if (i < 2)
            {
                g.setColour (IlanaTheme::Ui::line);
                g.fillRect (cell.getRight() - 4, readoutArea.getY() + 4, 1, readoutArea.getHeight() - 8);
            }
        }
    }

    // QUICK SHAPES over the tiles at the dock's bottom; between, the
    // destination's knob (MatrixDockTarget; the how-to sentences are in
    // the CURVE cell's tooltip).
    if (! shapeTiles.empty() && shapeTiles.front()->isVisible())
    {
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("QUICK SHAPES", juce::Rectangle<int> (info.getX(), shapeTiles.front()->getY() - 16, info.getWidth(), 14),
                    juce::Justification::centredLeft);
    }
}

void MatrixPage::mouseUp (const juce::MouseEvent& event)
{
    if (event.mouseWasDraggedSinceMouseDown() || event.mods.isPopupMenu())
        return;

    for (const auto& heading : headings())
        if (heading.sort != Sort::none && heading.area.contains (event.getPosition()))
            setSort (heading.sort);
}

void MatrixPage::setSort (Sort column)
{
    if (column == Sort::slot || column != sort)
    {
        sort = column;
        descending = column == Sort::amount;
    }
    else if (descending != (column == Sort::amount))
    {
        sort = Sort::slot; // third click
        descending = false;
    }
    else
    {
        descending = ! descending;
    }

    updateRows (true);
    repaint();
}

void MatrixPage::showRemap (int slot)
{
    if (remapEditor != nullptr && remapEditor->getSlotIndex() == slot)
    {
        if (dockAutomatic)
        {
            dockAutomatic = false;
            dockDismissed = false;
        }
        return;
    }

    toggleRemap (slot);
}

void MatrixPage::toggleRemap (int slot)
{
    if (remapEditor != nullptr && remapEditor->getSlotIndex() == slot)
    {
        closeRemap();
        return;
    }

    closeRemap();
    dockDismissed = false;
    const auto colour = modSourceColour ((int) processorRef.readModSlot (slot).source);
    remapEditor = std::make_unique<RemapEditor> (processorRef, slot, colour);
    dockAutomatic = false;
    remapEditor->setOnClose ([safeThis = juce::Component::SafePointer<MatrixPage> (this)]
    {
        // Not from inside the editor's own click.
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->closeRemap();
        });
    });
    addAndMakeVisible (*remapEditor);
    dockTarget.setSlot (slot);

    for (auto& row : rows)
        row->setSelected (row->getSlotIndex() == slot);

    resized();

    // Scroll so the row shows above the dock.
    const auto rowBounds = rows[(size_t) slot]->getBounds();
    const auto view = viewport.getViewArea();
    if (rowBounds.getBottom() > view.getBottom())
        viewport.setViewPosition (0, rowBounds.getBottom() - view.getHeight() + 4);
    else if (rowBounds.getY() < view.getY())
        viewport.setViewPosition (0, rowBounds.getY());

    repaint();
}

void MatrixPage::openRemapQuietly (int slot, bool automatic)
{
    remapEditor = std::make_unique<RemapEditor> (processorRef, slot, modSourceColour ((int) processorRef.readModSlot (slot).source));
    dockAutomatic = automatic;
    remapEditor->setOnClose ([safeThis = juce::Component::SafePointer<MatrixPage> (this)]
    {
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->closeRemap();
        });
    });
    addAndMakeVisible (*remapEditor);
    dockTarget.setSlot (slot);

    for (auto& row : rows)
        row->setSelected (row->getSlotIndex() == slot);
}

void MatrixPage::closeRemapQuietly ()
{
    removeChildComponent (remapEditor.get());
    remapEditor.reset();
    dockAutomatic = false;

    for (auto& row : rows)
        row->setSelected (false);
}

void MatrixPage::closeRemap ()
{
    if (remapEditor == nullptr)
        return;

    dockDismissed = true;

    removeChildComponent (remapEditor.get());
    remapEditor.reset();

    for (auto& row : rows)
        row->setSelected (false);

    resized();
    repaint();
}

void MatrixPage::focusSlot (int slot)
{
    updateRows (true);

    if (! juce::isPositiveAndBelow (slot, (int) rows.size()) || std::find (visibleRows.begin(), visibleRows.end(), slot) == visibleRows.end())
        return;

    const auto rowBounds = rows[(size_t) slot]->getBounds();
    const auto view = viewport.getViewArea();

    if (rowBounds.getBottom() > view.getBottom() || rowBounds.getY() < view.getY())
        viewport.setViewPosition (0, juce::jmax (0, rowBounds.getCentreY() - viewport.getHeight() / 2));

    if (remapEditor == nullptr)
    {
        rows[(size_t) slot]->setSelected (true);
        juce::Timer::callAfterDelay (1400, [safeThis = juce::Component::SafePointer<MatrixPage> (this), slot]
        {
            if (safeThis != nullptr && safeThis->remapEditor == nullptr)
                safeThis->rows[(size_t) slot]->setSelected (false);
        });
    }
}

void MatrixPage::showRowMenu (int slot)
{
    const auto routing = processorRef.readModSlot (slot);
    juce::PopupMenu menu;
    menu.addSectionHeader (ModNames::source ((int) routing.source, &processorRef) + juce::String::fromUTF8 ("  \xe2\x86\x92  ")
                           + (routing.destination != 0 ? ModNames::destination (routing.destination, processorRef) : juce::String ("no destination")));
    menu.addItem (1, "Duplicate (same source, pick a new destination)", freeSlot() >= 0 && routing.source != Mod::Source::None);
    menu.addItem (2, "Show destination", routing.destination != 0 && onShowDestination != nullptr);
    menu.addItem (3, soloSlot == slot ? "End solo (the other routes back on)" : "Solo this route", routing.source != Mod::Source::None || soloSlot == slot);
    menu.addSeparator();
    menu.addItem (4, "Remove");
    menu.showMenuAsync (IlanaTheme::menuOptions (rows[(size_t) slot].get()),
                        [safeThis = juce::Component::SafePointer<MatrixPage> (this), slot] (int result)
                        {
                            if (safeThis != nullptr && result > 0)
                                safeThis->rowMenuChoice (slot, result);
                        });
}

void MatrixPage::rowMenuChoice (int slot, int choice)
{
    if (choice == 1)
        duplicateRow (slot);
    else if (choice == 2)
    {
        if (const auto destination = processorRef.readModSlot (slot).destination; destination != 0 && onShowDestination != nullptr)
            onShowDestination (destination);
    }
    else if (choice == 3)
        soloRow (slot);
    else if (choice == 4)
    {
        if (soloSlot == slot)
            soloRow (slot); // (the others come back on first)
        processorRef.performEdit ("Remove modulation", [this, slot] { processorRef.clearModSlot (slot); });
        updateRows (true);
    }
}

int MatrixPage::duplicateRow (int slot)
{
    const auto target = freeSlot();
    if (target < 0)
        return -1;

    processorRef.performEdit ("Duplicate modulation", [this, slot, target]
    {
        processorRef.clearModSlot (target);
        for (const auto* field : { "src", "aux", "amt", "curve", "pol" })
            processorRef.setModSlotValue (target, field, processorRef.apvts.getParameter (processorRef.getModSlotParamId (slot, field)) != nullptr
                                                             ? processorRef.apvts.getParameter (processorRef.getModSlotParamId (slot, field))->convertFrom0to1 (
                                                                   processorRef.apvts.getParameter (processorRef.getModSlotParamId (slot, field))->getValue())
                                                             : 0.0f);
        if (processorRef.readModSlot (slot).remap != nullptr)
            processorRef.setModRemap (target, processorRef.getModRemap (slot));
    });
    updateRows (true);

    const auto rowBounds = rows[(size_t) target]->getBounds();
    if (! viewport.getViewArea().contains (rowBounds))
        viewport.setViewPosition (0, juce::jmax (0, rowBounds.getBottom() - viewport.getHeight() + rowHeight + 4));

    if (openListsOnDuplicate)
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MatrixPage> (this), target]
        {
            if (safeThis != nullptr && safeThis->rows[(size_t) target]->isShowing())
                safeThis->rows[(size_t) target]->showDestinationList();
        });
    return target;
}

MatrixPage::~MatrixPage()
{
    dropSoloOfOldPatch();
    if (soloSlot < 0)
        return;
    // The routes the solo switched off come back on (no rows to redraw now).
    processorRef.performEdit ("End solo", [this]
    {
        for (const auto other : soloMuted)
            processorRef.setModSlotValue (other, "byp", 0.0f);
        if (soloWasOff)
            processorRef.setModSlotValue (soloSlot, "byp", 1.0f);
    });
}

void MatrixPage::dropSoloOfOldPatch()
{
    if (soloSlot >= 0 && soloPatchGeneration != processorRef.getPatchGeneration())
    {
        soloSlot = -1;
        soloMuted.clear();
        soloWasOff = false;
    }
}

void MatrixPage::soloRow (int slot)
{
    dropSoloOfOldPatch();
    const auto ending = soloSlot == slot;
    const auto previous = soloSlot;
    processorRef.performEdit (ending ? "End solo" : "Solo modulation", [this, slot, ending, previous]
    {
        if (previous >= 0)
        {
            for (const auto other : soloMuted)
                processorRef.setModSlotValue (other, "byp", 0.0f);
            if (soloWasOff)
                processorRef.setModSlotValue (previous, "byp", 1.0f);
        }
        soloMuted.clear();
        soloSlot = -1;
        soloWasOff = false;
        if (ending)
            return;

        soloSlot = slot;
        soloPatchGeneration = processorRef.getPatchGeneration();
        soloWasOff = processorRef.readModSlot (slot).bypass;
        processorRef.setModSlotValue (slot, "byp", 0.0f);
        for (int other = 0; other < Mod::maxSlots; ++other)
            if (const auto route = processorRef.readModSlot (other); other != slot && ! route.bypass
                                                                     && (route.source != Mod::Source::None || route.destination != 0))
            {
                soloMuted.push_back (other);
                processorRef.setModSlotValue (other, "byp", 1.0f);
            }
    });
    updateRows (true);
    repaint();
}

void MatrixPage::toggleSourceRoutes (int source)
{
    std::vector<int> slots;
    auto anyOn = false;
    for (const auto index : visibleRows)
        if (sourceOfSlot (index) == source)
        {
            slots.push_back (index);
            anyOn = anyOn || ! processorRef.readModSlot (index).bypass;
        }
    if (slots.empty())
        return;

    processorRef.performEdit ((anyOn ? "Switch off " : "Switch on ") + ModNames::source (source, &processorRef) + " routes", [this, slots, anyOn]
    {
        for (const auto index : slots)
            processorRef.setModSlotValue (index, "byp", anyOn ? 1.0f : 0.0f);
    });
    updateRows (true);
    list.repaint();
}

void MatrixPage::mergeAll ()
{
    processorRef.performEdit ("Merge repeated routings", [this] { processorRef.mergeDuplicateModSlots(); });
    updateRows (true);
}

void MatrixPage::showMergeMenu (int slot)
{
    juce::PopupMenu menu;
    const auto routing = processorRef.readModSlot (slot);
    menu.addSectionHeader (ModNames::source ((int) routing.source, &processorRef) + juce::String::fromUTF8 ("  \xe2\x86\x92  ")
                           + ModNames::destination (routing.destination, processorRef));

    for (const auto other : visibleRows)
    {
        const auto twin = processorRef.readModSlot (other);

        if (other == slot || twin.source != routing.source || twin.destination != routing.destination)
            continue;

        juce::String why;
        const auto into = juce::jmin (slot, other), from = juce::jmax (slot, other);
        const auto canMerge = processorRef.canMergeModSlots (into, from, &why);
        const auto sum = juce::roundToInt ((routing.depth + twin.depth) * 100.0f);
        menu.addItem (1 + other, "Merge with row " + juce::String (rows[(size_t) other]->getDisplayNumber())
                                     + (canMerge ? "  (" + juce::String (sum >= 0 ? "+" : "") + juce::String (sum) + "%)"
                                                 : "  (can't: " + why + ")"),
                      canMerge);
    }

    menu.addSeparator();
    menu.addItem (1000, "Remove this row");

    menu.showMenuAsync (IlanaTheme::menuOptions (rows[(size_t) slot].get()),
                        [safeThis = juce::Component::SafePointer<MatrixPage> (this), slot] (int result)
                        {
                            if (safeThis == nullptr || result <= 0)
                                return;

                            auto& processor = safeThis->processorRef;

                            if (result == 1000)
                                processor.performEdit ("Remove modulation", [&processor, slot] { processor.clearModSlot (slot); });
                            else
                            {
                                const auto other = result - 1;
                                processor.performEdit ("Merge repeated routing", [&processor, slot, other]
                                {
                                    processor.mergeModSlots (juce::jmin (slot, other), juce::jmax (slot, other));
                                });
                            }

                            safeThis->updateRows (true);
                        });
}

void MatrixPage::paintGhostRows (juce::Graphics& g) const
{
    const auto heads = headings();
    if (heads.size() < 9)
        return;

    const auto bottom = viewport.getBottom();
    const auto stride = MatrixRow::rowHeight + 7;
    const auto ink = IlanaTheme::Ui::text3;
    const auto rim = IlanaTheme::Ui::line;

    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
    g.setColour (ink.withAlpha (0.5f));
    for (const auto& heading : heads)
        g.drawText (heading.text, heading.area, juce::Justification::centredLeft);

    auto y = headerArea.getBottom() + 3;
    for (int row = 0; y + stride - 4 <= bottom - 4; ++row, y += stride)
    {
        // (Behind the starters they step back further, so those read clearly.)
        const auto behindStarters = y + stride > starterArea().getY() - 26 && y < starterArea().getBottom();
        const auto fade = juce::jmax (0.14f, 0.7f - 0.05f * (float) row) * (behindStarters ? 0.45f : 1.0f);
        const auto box = [&] (int column, int inset = 0) { return heads[(size_t) column].area.withY (y).withHeight (MatrixRow::rowHeight).reduced (0, inset).toFloat(); };
        g.setColour (IlanaTheme::Ui::raised.withAlpha (0.35f * fade));
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) headerArea.getX(), (float) y - 3.0f, (float) (heads.back().area.getRight() + 30 - headerArea.getX()),
                                                        (float) MatrixRow::rowHeight + 6.0f), 6.0f);
        g.setColour (ink.withAlpha (fade));
        g.drawText (juce::String (row + 1), box (0).toNearestInt(), juce::Justification::centredLeft);
        // ON switch, SOURCE and DESTINATION boxes, VIA tile.
        g.setColour (rim.withAlpha (fade * 1.6f));
        g.drawRoundedRectangle (box (1, 5).withSizeKeepingCentre (30.0f, 16.0f).reduced (0.5f), 8.0f, 1.0f);
        g.drawRoundedRectangle (box (2, 2).reduced (0.5f), 5.0f, 1.0f);
        g.drawRoundedRectangle (box (8, 2).reduced (0.5f), 5.0f, 1.0f);
        g.drawRoundedRectangle (box (3, 4).reduced (0.5f), 5.0f, 1.0f);
        g.drawRoundedRectangle (box (7, 4).withSizeKeepingCentre (26.0f, box (7, 4).getHeight()).reduced (0.5f), 5.0f, 1.0f);
        g.drawRoundedRectangle (box (6, 3).reduced (0.5f), 5.0f, 1.0f);
        g.drawRoundedRectangle (box (5, 2).reduced (0.5f), 5.0f, 1.0f);
        // The AMOUNT slider: a track with its thumb at the middle; the CURVE's line.
        const auto amount = box (4);
        g.setColour (rim.withAlpha (fade * 2.0f));
        g.fillRect (juce::Rectangle<float> (amount.getWidth() - 56.0f, 2.0f).withCentre ({ amount.getX() + (amount.getWidth() - 56.0f) * 0.5f, amount.getCentreY() }));
        g.setColour (ink.withAlpha (fade * 1.4f));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ amount.getX() + (amount.getWidth() - 56.0f) * 0.5f, amount.getCentreY() }));
        g.drawText ("0%", amount.toNearestInt().withTrimmedLeft (amount.getWidth() - 44), juce::Justification::centredRight);
        const auto curveBox = box (5, 6);
        g.setColour (ink.withAlpha (fade * 1.2f));
        g.drawLine (curveBox.getX() + 6.0f, curveBox.getBottom() - 2.0f, curveBox.getRight() - 6.0f, curveBox.getY() + 2.0f, 1.0f);
        g.drawText ("+", box (6).toNearestInt().withTrimmedRight (box (6).getWidth() * 0.5f), juce::Justification::centred);
    }
}

juce::Rectangle<float> MatrixPage::emptyStateCard () const
{
    const auto area = viewport.getBounds().withTrimmedTop (56);
    // (The dialog holds the starters and the chevron too: P-5. 346 = the 250 px of
    // text and cable, the caption line, the 74 px of starters and the chevron.)
    return juce::Rectangle<float> (640.0f, 346.0f).withCentre (area.toFloat().getCentre()).withY ((float) area.getY() + 20.0f);
}

void MatrixPage::paintEmptyState (juce::Graphics& g)
{
    const auto now = emptyClock;
    const auto area = viewport.getBounds().withTrimmedTop (56);
    const auto card = emptyStateCard();
    paintGhostRows (g);
    IlanaTheme::paintCard (g, card, 10.0f, IlanaTheme::accent().withAlpha (0.3f));

    // Source dot -> animated cable -> knob.
    const auto sourceCentre = juce::Point<float> (card.getX() + 150.0f, card.getY() + 62.0f);
    const auto knobCentre = juce::Point<float> (card.getRight() - 150.0f, card.getY() + 62.0f);
    juce::Path cable;
    cable.startNewSubPath (sourceCentre);
    cable.cubicTo (sourceCentre.translated (80.0f, -40.0f + 10.0f * std::sin (now * 2.0f)),
                   knobCentre.translated (-80.0f, 40.0f - 10.0f * std::sin (now * 2.0f)), knobCentre);
    // Drawn in LFO 1's own colour, as its chip, cable and ring really are.
    const auto sourceColour = modSourceColour ((int) Mod::Source::Lfo1);
    g.setColour (sourceColour.withAlpha (0.45f));
    g.strokePath (cable, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto travel = std::fmod (now * 0.5f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (cable.getPointAlongPath (travel * cable.getLength())));

    g.setColour (sourceColour);
    g.fillRoundedRectangle (juce::Rectangle<float> (58.0f, 24.0f).withCentre (sourceCentre), 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
    g.drawText ("LFO 1", juce::Rectangle<float> (58.0f, 24.0f).withCentre (sourceCentre), juce::Justification::centred);

    const auto knob = juce::Rectangle<float> (34.0f, 34.0f).withCentre (knobCentre);
    g.setColour (IlanaTheme::Ui::raised);
    g.fillEllipse (knob);
    const auto sweep = 0.6f + 0.35f * std::sin (now * 2.0f);
    juce::Path arc;
    arc.addCentredArc (knobCentre.x, knobCentre.y, 21.0f, 21.0f, 0.0f, -2.4f, -2.4f + 4.8f * sweep, true);
    g.setColour (sourceColour);
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (IlanaTheme::Ui::text);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
    g.drawText ("Nothing is modulated yet", card.withTrimmedTop (104.0f).withHeight (24.0f), juce::Justification::centred);

    g.setColour (IlanaTheme::Ui::text2);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
    // (Review 22: two plain lines, not three starting with "or".)
    const char* const tips[] {
        "Drag a source chip from the bar below onto any knob,",
        "right-click a knob for quick modulation, or press  + ADD MODULATION  above"
    };

    for (int i = 0; i < 2; ++i)
        g.drawText (tips[i], card.withTrimmedTop (136.0f + (float) i * 22.0f).withHeight (20.0f), juce::Justification::centred);

    g.setColour (IlanaTheme::Ui::text);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
    g.drawText ("OR START FROM ONE OF THESE", starterArea().withHeight (16).translated (0, -22), juce::Justification::centred);

    // A chevron bobbing towards the source chips, under the starters and
    // inside the dialog (left out when there is no room).
    const auto bob = 4.0f * std::sin (now * 3.0f);
    const auto tipY = card.getBottom() - 16.0f;
    if (tipY + 6.0f > (float) getHeight())
        return;
    const auto tip = juce::Point<float> (area.toFloat().getCentreX(), tipY + bob);
    juce::Path chevron;
    chevron.startNewSubPath (tip.translated (-10.0f, -8.0f));
    chevron.lineTo (tip);
    chevron.lineTo (tip.translated (10.0f, -8.0f));
    g.setColour (IlanaTheme::accent().withAlpha (0.6f));
    g.strokePath (chevron, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void MatrixPage::resized ()
{
    auto area = getLocalBounds().reduced (12, 6);
    auto top = area.removeFromTop (32);
    addButton.setBounds (top.removeFromRight (190).withTrimmedTop (6).withTrimmedBottom (2));
    top.removeFromRight (8);
    mergeButton.setBounds (top.removeFromRight (210).withTrimmedTop (6).withTrimmedBottom (2));
    top.removeFromRight (8);
    filterField.setBounds (top.removeFromRight (210).withTrimmedTop (5).withTrimmedBottom (1));
    headerArea = area.removeFromTop (18);

    // The dock is decided first, with the rows at their base height
    // (UI review 31, item 11: growing the rows first ate the room it
    // needed, so 6 to 9 routes got neither). The dock always holds a curve
    // (V14-4): a matrix that leaves it room opens its first row's by itself;
    // once the rows need the room that automatic one goes again (a curve the
    // user opened stays). The rows never move for it, the list just scrolls
    // above it (V6-22).
    const auto baseRowsHeight = listContentHeight (MatrixRow::rowHeight, 0);
    const auto spareAtBase = area.getHeight() - baseRowsHeight;

    if (! visibleRows.empty() && remapEditor == nullptr && ! dockDismissed && spareAtBase >= dockHeight + 8)
        openRemapQuietly (visibleRows.front(), true);
    else if (remapEditor != nullptr && dockAutomatic && spareAtBase < dockHeight + 8)
        closeRemapQuietly();

    // (A dock the user opens where none fits leaves the rows as they were:
    // the list keeps the height and headings it had without it, V6-22.)
    const auto heightWithoutDock = area.getHeight();
    const auto dockFits = spareAtBase >= dockHeight + 8;
    if (! visibleRows.empty() && remapEditor != nullptr)
        dockArea = area.removeFromBottom (juce::jlimit ((int) dockHeight, 400, spareAtBase)).withTrimmedTop (8);
    else
        dockArea = {};
    dockTarget.setVisible (! dockArea.isEmpty() && remapEditor != nullptr);

    // Source headings only while every row still fits with them (a long
    // matrix scrolled only for its headings: UI review 31, item 11).
    // (Laid out for the dock whenever one fits, open or not, so opening or
    // closing a curve never moves a row.)
    const auto listHeight = dockFits ? heightWithoutDock - juce::jlimit ((int) dockHeight, 400, spareAtBase) : heightWithoutDock;
    headingsShown = wantsGroupHeadings() && baseRowsHeight + numGroups() * groupHeadingHeight <= listHeight;
    const auto headingsHeight = headingsShown ? numGroups() * groupHeadingHeight : 0;

    // Then the rows grow a little taller into what is left (V7-20), never
    // past 34 px: Serum's rows are about this tall and a long matrix should
    // not scroll more than it has to (review 9, S9-7).
    rowHeight = MatrixRow::rowHeight;
    while (rowHeight < 34 && listContentHeight (rowHeight + 1, headingsHeight) <= listHeight)
        ++rowHeight;
    const auto rowsHeight = listContentHeight (rowHeight, headingsHeight);

    // Over an open dock the list ends at a whole row, not half-way through
    // one (V11-9, S14-10): the last row that fits, with the headings the
    // list has, and the dock takes what that leaves.
    if (remapEditor != nullptr && ! dockArea.isEmpty() && area.getHeight() < rowsHeight)
    {
        viewport.setBounds (area);
        layoutList();
        auto fit = 0;
        for (const auto index : visibleRows)
            if (const auto bottom = rows[(size_t) index]->getBottom() + 2; bottom <= area.getHeight())
                fit = juce::jmax (fit, bottom);
        if (fit >= rowHeight + 1 && fit < area.getHeight())
        {
            dockArea = dockArea.withTop (dockArea.getY() + (fit - area.getHeight()));
            area = area.withHeight (fit);
        }
    }

    if (remapEditor != nullptr)
        remapEditor->setBounds (dockArea.withWidth (juce::jmin (dockArea.getWidth() * 3 / 5, juce::jmax (560, dockArea.getHeight() * 2))));

    // The tiles: one row along the bottom of the dock's info side.
    const auto tilesShown = remapEditor != nullptr;
    auto tiles = tilesShown ? dockArea.withTrimmedLeft (remapEditor->getRight() - dockArea.getX() + 16).removeFromBottom (48)
                            : juce::Rectangle<int>();
    // The destination's knob between the live readout and QUICK SHAPES
    // (the readout is 16 + 46 px under the dock's 8 px inset, paintDock).
    if (tilesShown)
    {
        const auto info = dockArea.withTrimmedLeft (remapEditor->getRight() - dockArea.getX() + 16).reduced (0, 8);
        dockTarget.setBounds (info.withTrimmedTop (16 + 46 + 8).withBottom (tiles.getY() - 22));
    }
    const auto tileWidth = juce::jmin (72, tiles.getWidth() / juce::jmax (1, (int) shapeTiles.size()));
    for (auto& tile : shapeTiles)
    {
        tile->setVisible (tilesShown);
        if (tilesShown)
        {
            tile->colour = modSourceColour ((int) processorRef.readModSlot (remapEditor->getSlotIndex()).source);
            tile->setBounds (tiles.removeFromLeft (tileWidth).reduced (3, 0)); // (names keep 6 px between them: V10-22)
        }
    }

    viewport.setBounds (area);
    layoutList();
    layoutStarters();
}

void MatrixPage::visibilityChanged ()
{
    if (isVisible())
    {
        dockDismissed = false; // (the X lasts while the page is open)
        updateRows();
    }
}

auto MatrixPage::headings () const
     -> std::vector<Heading>
{
    using C = MatrixRow::Columns;
    std::vector<Heading> result;
    auto x = headerArea.getX();
    const auto add = [&] (const char* text, int width, int gapAfter, Sort sortBy)
    {
        result.push_back ({ text, { x, headerArea.getY(), width, headerArea.getHeight() }, sortBy });
        x += width + gapAfter;
    };

    add ("#", C::number, 0, Sort::slot);
    add ("ON", C::bypass, C::gap * 2, Sort::none);
    add ("SOURCE", C::source, C::gap, Sort::source);
    add ("VIA", C::via (viaExpanded), C::gap * 2, Sort::none);
    add ("AMOUNT", C::amount (viaExpanded), C::gap, Sort::amount);
    add ("CURVE", C::curve, C::gap, Sort::none);
    add ("POLARITY", C::polarity, C::gap, Sort::none);
    add ("STEREO", C::stereo, C::gap * 2, Sort::none);
    add ("DESTINATION", C::destination, C::gap, Sort::destination);
    return result;
}

auto MatrixPage::starterRoutings ()
     -> const std::vector<Starter>&
{
    using S = Mod::Source;
    using D = Mod::Destination;
    static const std::vector<Starter> starters {
        { "LFO 1  >  Cutoff", "LFO 1 sweeps filter 1's cutoff.", S::Lfo1, S::None, { D::Filter1Cutoff }, 0.3f },
        { "ENV 4  >  Frame", "Envelope 4 moves OSC 1's wavetable position on every note.", S::ModEnv, S::None, { D::Osc1Frame }, 0.5f },
        { "MOD WHEEL  >  Vibrato", "LFO 2 wobbles the pitch of OSC 1-3, as far as the mod wheel lets it.", S::Lfo2, S::ModWheel,
          { D::Osc1Pitch, D::Osc2Pitch, D::SubPitch }, 0.006f },
        { "VELOCITY  >  Cutoff", "Harder notes open filter 1.", S::Velocity, S::None, { D::Filter1Cutoff }, 0.35f },
        { "LFO 2  >  Pan", "LFO 2 moves OSC 1 across the stereo field.", S::Lfo2, S::None, { D::Osc1Pan }, 0.5f },
        { "MACRO 1  >  Drive", "Macro 1 drives filter 1.", S::Macro1, S::None, { D::Filter1Drive }, 0.5f }
    };
    return starters;
}

juce::Rectangle<int> MatrixPage::starterArea () const
{
    const auto area = viewport.getBounds().withTrimmedTop (56);
    return juce::Rectangle<int> (600, 74).withCentre (area.getCentre()).withY (area.getY() + 20 + 234);
}

void MatrixPage::layoutStarters ()
{
    const auto area = starterArea();
    const auto width = area.getWidth() / 3;

    for (size_t i = 0; i < starterButtons.size(); ++i)
        starterButtons[i]->setBounds (juce::Rectangle<int> (area.getX() + (int) (i % 3) * width, area.getY() + (int) (i / 3) * 37,
                                                            width, 37).reduced (4, 3));
}

void MatrixPage::addStarter (const Starter& starter)
{
    processorRef.beginEdit ("Add " + juce::String (starter.label));
    auto next = 0;

    for (const auto destination : starter.destinations)
    {
        for (; next < Mod::maxSlots; ++next)
        {
            const auto slot = processorRef.readModSlot (next);

            if (slot.source == Mod::Source::None && slot.destination == 0)
                break;
        }

        if (next >= Mod::maxSlots)
            break;

        processorRef.clearModSlot (next);
        processorRef.setModSlotValue (next, "src", (float) starter.source);
        processorRef.setModSlotValue (next, "dst", (float) destination);
        processorRef.setModSlotValue (next, "aux", (float) starter.aux);
        processorRef.setModSlotValue (next, "amt", starter.depth);
        ++next;
    }

    processorRef.endEdit();
    updateRows();
}

void MatrixPage::addRouting ()
{
    for (int i = 0; i < Mod::maxSlots; ++i)
    {
        const auto slot = processorRef.readModSlot (i);

        if (slot.source == Mod::Source::None && slot.destination == 0)
        {
            // A new row starts from LFO 1 with no destination yet, so it
            // shows up but does nothing until a target is picked.
            processorRef.performEdit ("Add routing", [this, i]
            {
                processorRef.clearModSlot (i);
                processorRef.setModSlotValue (i, "src", (float) Mod::Source::Lfo1);
                processorRef.setModSlotValue (i, "amt", 0.5f);
            });
            updateRows (true);
            // (Grouped by source, the new row may not be the last.)
            const auto rowBounds = rows[(size_t) i]->getBounds();
            if (! viewport.getViewArea().contains (rowBounds))
                viewport.setViewPosition (0, juce::jmax (0, rowBounds.getBottom() - viewport.getHeight() + rowHeight + 4));
            return;
        }
    }
}

std::vector<int> MatrixPage::sortedSlots () const
{
    std::vector<int> used;

    for (int i = 0; i < Mod::maxSlots; ++i)
    {
        const auto slot = processorRef.readModSlot (i);

        if (slot.source != Mod::Source::None || slot.destination != 0)
            used.push_back (i);
    }

    if (sort == Sort::slot || sort == Sort::none)
        return used;

    // Destinations by the names the rows show; sources in the order the
    // bar and the source menus list them (LFOs, envelopes, macros, the
    // rest), so a source's rows sit together (S8-25).
    const auto key = [&] (int index) -> juce::String
    {
        return ModNames::destination (processorRef.readModSlot (index).destination, processorRef);
    };
    const auto sourceOrder = [&] (int index)
    {
        const auto& order = ModNames::sourcesInMenuOrder();
        const auto found = std::find (order.begin(), order.end(), (int) processorRef.readModSlot (index).source);
        return (int) std::distance (order.begin(), found);
    };

    std::stable_sort (used.begin(), used.end(), [&] (int a, int b)
    {
        if (sort == Sort::amount)
        {
            const auto depthA = std::abs (processorRef.readModSlot (a).depth);
            const auto depthB = std::abs (processorRef.readModSlot (b).depth);
            return descending ? depthA > depthB : depthA < depthB;
        }

        if (sort == Sort::source)
        {
            const auto orderA = sourceOrder (a), orderB = sourceOrder (b);
            return descending ? orderA > orderB : orderA < orderB;
        }

        const auto order = key (a).compareNatural (key (b));
        return descending ? order > 0 : order < 0;
    });

    return used;
}

void MatrixPage::updateRows (bool force)
{
    refreshMacroNames();
    refreshPoolSources();

    auto used = sortedSlots();
    totalUsed = (int) used.size();

    // The filter keeps the rows whose source or destination name has the
    // typed text in it.
    const auto wanted = filterField.getText().trim();
    if (wanted.isNotEmpty())
        used.erase (std::remove_if (used.begin(), used.end(), [&] (int index)
        {
            const auto slot = processorRef.readModSlot (index);
            return ! (ModNames::destination (slot.destination, processorRef).containsIgnoreCase (wanted)
                      || ModNames::source ((int) slot.source, &processorRef).containsIgnoreCase (wanted)
                      || ModNames::source ((int) slot.aux, &processorRef).containsIgnoreCase (wanted));
        }), used.end());
    filterField.setVisible (totalUsed >= 5 || wanted.isNotEmpty());

    for (auto& button : starterButtons)
        button->setVisible (totalUsed == 0);

    // Rows don't jump about under the mouse: while a button is held (an
    // amount being dragged, a menu open), a new order waits.
    const auto sameSet = used.size() == visibleRows.size()
                         && std::is_permutation (used.begin(), used.end(), visibleRows.begin());
    if (! force && sameSet && juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        used = visibleRows;

    // Number the rows as shown, and mark repeated source / destination
    // pairs (older patches can have them; both rows still play).
    std::map<std::tuple<int, int, int>, std::vector<int>> pairs; // source, destination, via
    auto anyVia = false;

    for (size_t i = 0; i < used.size(); ++i)
    {
        const auto slot = processorRef.readModSlot (used[i]);
        anyVia = anyVia || slot.aux != Mod::Source::None;

        if (slot.source != Mod::Source::None && slot.destination != 0)
            pairs[{ (int) slot.source, slot.destination, (int) slot.aux }].push_back ((int) i + 1);
    }

    auto duplicates = 0;

    for (size_t i = 0; i < used.size(); ++i)
    {
        const auto slot = processorRef.readModSlot (used[i]);
        juce::StringArray others;

        if (const auto found = pairs.find ({ (int) slot.source, slot.destination, (int) slot.aux }); found != pairs.end() && slot.destination != 0)
            for (const auto number : found->second)
                if (number != (int) i + 1)
                    others.add (juce::String (number));

        duplicates += others.isEmpty() ? 0 : 1;
        rows[(size_t) used[i]]->setDisplayNumber ((int) i + 1, others.joinIntoString (", "));
    }

    // One count, of routings (not rows), and which rows (V8-20).
    juce::StringArray repeated;
    for (const auto& [pair, numbers] : pairs)
        if (numbers.size() > 1)
        {
            juce::StringArray list;
            for (const auto number : numbers)
                list.add (juce::String (number));
            repeated.add (list.size() == 2 ? list[0] + " and " + list[1] : list.joinIntoString (", "));
        }
    const auto text = repeated.isEmpty() ? juce::String()
                      : repeated.size() == 1 ? "1 routing is repeated (rows " + repeated[0] + ")"
                                             : juce::String (repeated.size()) + " routings are repeated (rows " + repeated.joinIntoString ("; ") + ")";

    if (duplicates != numDuplicates || text != repeatText)
    {
        numDuplicates = duplicates;
        repeatText = text;
        mergeButton.setButtonText ("MERGE REPEATS");
        mergeButton.setTooltip ("Merge repeated routings\n" + text + ".  Rows with the same source and destination add up; "
                                "each pair becomes one row with the two depths added (the sound stays the same).");
        mergeButton.setVisible (duplicates > 0);
        repaint();
    }

    if (anyVia != viaExpanded)
    {
        viaExpanded = anyVia;

        for (auto& row : rows)
            row->setViaExpanded (anyVia);

        repaint();
    }

    if (used != visibleRows)
    {
        visibleRows = used;
        dockDismissed = false;

        for (auto& row : rows)
            row->setVisible (std::find (used.begin(), used.end(), row->getSlotIndex()) != used.end());

        addButton.setEnabled (used.size() < (size_t) Mod::maxSlots);
        addButton.setVisible (totalUsed == 0);
        resized();
        repaint();
    }

    // A source changed under the group headings: lay them out again.
    {
        juce::String key (wantsGroupHeadings() ? "g" : "n");
        if (wantsGroupHeadings())
            for (const auto index : visibleRows)
                key << sourceOfSlot (index) << (processorRef.readModSlot (index).bypass ? "b," : ",");
        if (key != headingKey)
        {
            headingKey = key;
            resized();
        }
    }

    // A solo undone, or a patch loaded over it, is forgotten (its "end"
    // would switch on routes that are off on purpose).
    if (soloSlot >= 0)
    {
        const auto solo = processorRef.readModSlot (soloSlot);
        auto intact = solo.source != Mod::Source::None && solo.destination != 0 && ! solo.bypass;
        for (const auto muted : soloMuted)
            intact = intact && processorRef.readModSlot (muted).bypass;
        if (! intact)
        {
            soloSlot = -1;
            soloMuted.clear();
            soloWasOff = false;
            repaint();
        }
    }

    auto idle = 0;
    for (const auto index : visibleRows)
    {
        rows[(size_t) index]->refresh();
        idle += rows[(size_t) index]->getIdleReason().isNotEmpty() ? 1 : 0;
    }

    if (idle != numIdle)
    {
        numIdle = idle;
        repaint();
    }

    if (dockTarget.isVisible())
        dockTarget.refresh(); // (a load keeps the dock's slot but not its destination)
}

bool MatrixPage::wantsGroupHeadings () const
{
    return sort == Sort::source && visibleRows.size() >= (size_t) groupHeadingsFrom;
}

int MatrixPage::listContentHeight (int height, int headingsHeight) const
{
    const auto count = (int) visibleRows.size();
    const auto addRowHeight = count > 0 && count < Mod::maxSlots ? DashedAddButton::standardHeight + 2 : 0;
    return count * height + headingsHeight + addRowHeight + 8;
}

int MatrixPage::numGroups () const
{
    auto groups = 0, last = -1;
    for (const auto slot : visibleRows)
        if (const auto source = sourceOfSlot (slot); source != last)
        {
            groups += source != 0 ? 1 : 0;
            last = source;
        }
    return groups;
}

int MatrixPage::freeSlot () const
{
    for (int i = 0; i < Mod::maxSlots; ++i)
        if (const auto slot = processorRef.readModSlot (i); slot.source == Mod::Source::None && slot.destination == 0)
            return i;
    return -1;
}

void MatrixPage::layoutList ()
{
    const auto width = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
    auto y = 0;

    // A routing removed while its remap editor was open takes the editor with it.
    if (remapEditor != nullptr
        && std::find (visibleRows.begin(), visibleRows.end(), remapEditor->getSlotIndex()) == visibleRows.end())
    {
        removeChildComponent (remapEditor.get());
        remapEditor.reset();
        for (auto& row : rows)
            row->setSelected (false);
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MatrixPage> (this)]
        {
            if (safeThis != nullptr)
                safeThis->resized();
        });
    }

    list.headings.clear();
    const auto headed = headingsShown;
    auto lastSource = -1;
    for (const auto index : visibleRows)
    {
        auto& row = *rows[(size_t) index];
        if (const auto source = sourceOfSlot (index); headed && source != lastSource && source != 0)
        {
            auto count = 0, off = 0;
            for (const auto other : visibleRows)
                if (sourceOfSlot (other) == source)
                {
                    ++count;
                    off += processorRef.readModSlot (other).bypass ? 1 : 0;
                }
            // The row's own name for the source (UI review 31, item 6: the
            // headings said FILTER ENV over FILT ENV rows, MACRO 1 over TONE).
            list.headings.push_back ({ { 0, y, width, groupHeadingHeight }, modSourceColour (source),
                                       ModNames::source (source, &processorRef).toUpperCase()
                                           + (count == 1 ? "   1 route" : "   " + juce::String (count) + " routes")
                                           + (off == count ? juce::String::fromUTF8 (" \xc2\xb7 off") : juce::String()),
                                       source, off == count });
            y += groupHeadingHeight;
            lastSource = source;
        }
        else if (source != lastSource)
            lastSource = source;
        row.setBounds (0, y, width, rowHeight);
        y += rowHeight;
    }
    list.repaint();

    // "+ ADD MODULATION" as the row after the last routing (S7-35).
    addRow.setVisible (! visibleRows.empty() && visibleRows.size() < (size_t) Mod::maxSlots);
    if (addRow.isVisible())
    {
        // (The one add tile's height, as on every page: V14-19.)
        addRow.setBounds (0, y + 1, width, DashedAddButton::standardHeight);
        y += DashedAddButton::standardHeight + 2;
    }

    list.setSize (width, y + 8);
}

void MatrixPage::timerCallback ()
{
    dropSoloOfOldPatch();
    if (! IlanaAnim::showing (*this))
    {
        emptyShownSeconds = 0.0f;
        return;
    }

    if (changeGate.check (processorRef.getUiEpoch()))
        updateRows();

    if (dockTarget.isVisible())
        dockTarget.refresh();

    // The dock's live readout follows the input.
    if (remapEditor != nullptr && ! readoutArea.isEmpty() && dockArea.isEmpty() == false)
    {
        const auto liveIn = remapEditor->getLiveInput();
        const auto out = remapEditor->getLiveOutput();
        const auto signature = (liveIn >= 0.0f ? juce::String (liveIn, 2) : juce::String ("--")) + juce::String (out, 2)
                               + juce::String (processorRef.readModSlot (remapEditor->getSlotIndex()).depth, 2);
        if (signature != readoutSignature)
        {
            readoutSignature = signature;
            repaint (readoutArea);
        }
    }

    // The empty state's cable plays for a few seconds after the page
    // opens, and while the mouse is over it, then rests.
    if (totalUsed == 0 && (emptyShownSeconds < 6.0f || isMouseOver (true)))
    {
        emptyShownSeconds += frameSeconds();
        emptyClock += frameSeconds();
        repaint (emptyStateCard().expanded (4.0f).getSmallestIntegerContainer());
    }
}

void MatrixPage::refreshMacroNames ()
{
    juce::StringArray macroNames;

    for (int m = 0; m < Mod::numMacros; ++m)
        macroNames.add (processorRef.getMacroName (m));

    if (macroNames != shownMacroNames)
    {
        shownMacroNames = macroNames;

        for (auto& row : rows)
            row->setMacroNames (macroNames);
    }
}

void MatrixPage::refreshPoolSources ()
{
    std::vector<bool> inPatch ((size_t) Mod::getSourceNames().size(), true);

    for (size_t source = 1; source < inPatch.size(); ++source)
        inPatch[source] = modSourceInPatch (processorRef, (Mod::Source) source);

    if (inPatch != shownPoolSources)
    {
        shownPoolSources = inPatch;

        for (auto& row : rows)
            row->setSourcesInPatch (inPatch);
    }
}
