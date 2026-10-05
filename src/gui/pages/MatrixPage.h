// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
class MatrixPage : public juce::Component,
                   public juce::SettableTooltipClient,
                   private IlanaAnim::FrameTimer
{
public:
    explicit MatrixPage (IlanaSynthAudioProcessor& p)
        : processorRef (p)
    {
        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            auto row = std::make_unique<MatrixRow> (p, i);
            row->getCurve().onOpenRemap = [this] (int slot) { toggleRemap (slot); };
            row->onDuplicateClicked = [this] (int slot) { showMergeMenu (slot); };
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

        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        addAndMakeVisible (viewport);

        updateRows();
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        const auto used = (int) visibleRows.size();

        const auto titleRight = (mergeButton.isVisible() ? mergeButton.getX() : addButton.isVisible() ? addButton.getX() : getWidth() - 12) - 12;
        paintSectionTitle (g, "MODULATION", juce::Rectangle<int> (headingX, 12, juce::jmax (100, titleRight - headingX), headingHeight),
                           juce::String (used) + " of " + juce::String (Mod::maxSlots) + " slots in use"
                           // (The how-to only while there's nothing to report.)
                           + (numDuplicates > 0 ? ",  " + repeatText : juce::String())
                           + (numIdle > 0 ? ",  " + juce::String (numIdle) + " into a module that is off (dimmed)" : juce::String())
                           + (numDuplicates > 0 || numIdle > 0 ? juce::String (".")
                                                               : juce::String (".   Drag a source onto any knob, then drag its ring "
                                                                               "on the knob to set the depth.")));

        // An empty matrix has no columns to head: just the ways in.
        if (visibleRows.empty())
        {
            paintEmptyState (g);
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

    // Under the rows: the docked remap editor beside a summary of its row,
    // or, when there is room and none is open, a note on how to open it.
    void paintDock (juce::Graphics& g)
    {
        if (dockArea.isEmpty())
            return;

        if (remapEditor == nullptr)
        {
            const auto area = dockArea.toFloat().reduced (2.0f);
            g.setColour (IlanaTheme::Ui::line);
            const float dashes[] { 4.0f, 4.0f };
            juce::Path outline;
            outline.addRoundedRectangle (area, 8.0f);
            juce::Path dashed;
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
            g.fillPath (dashed);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText ("REMAP", area.withTrimmedTop (area.getHeight() * 0.5f - 22.0f).withHeight (18.0f), juce::Justification::centred);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("Click a row's CURVE to draw how its source maps to the amount here.",
                        area.withTrimmedTop (area.getHeight() * 0.5f - 2.0f).withHeight (18.0f), juce::Justification::centred);
            return;
        }

        // The routing the curve belongs to.
        const auto slot = processorRef.readModSlot (remapEditor->getSlotIndex());
        auto info = dockArea.withTrimmedLeft (remapEditor->getRight() - dockArea.getX() + 16).reduced (0, 8);
        const auto colour = modSourceColour ((int) slot.source);

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("ROW " + juce::String (rows[(size_t) remapEditor->getSlotIndex()]->getDisplayNumber()), info.removeFromTop (16),
                    juce::Justification::centredLeft);

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
        g.setColour (colour);
        g.drawFittedText (ModNames::source ((int) slot.source, &processorRef), info.removeFromTop (24), juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (IlanaTheme::Ui::text);
        g.drawFittedText (juce::String::fromUTF8 ("\xe2\x86\x92 ") + ModNames::destination (slot.destination), info.removeFromTop (24),
                          juce::Justification::centredLeft, 1, 0.8f);

        info.removeFromTop (8);
        // QUICK SHAPES over the tiles at the dock's bottom.
        if (! shapeTiles.empty() && shapeTiles.front()->isVisible())
        {
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("QUICK SHAPES", juce::Rectangle<int> (info.getX(), shapeTiles.front()->getY() - 16, info.getWidth(), 14),
                        juce::Justification::centredLeft);
            info = info.withBottom (shapeTiles.front()->getY() - 18);
        }
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.setColour (IlanaTheme::Ui::text2);
        const auto bipolar = slot.polarity == Mod::Polarity::Bipolar
                             || (slot.polarity == Mod::Polarity::Natural && Mod::isBipolarSource (slot.source));
        const juce::String lines[] {
            "Amount " + juce::String (slot.depth >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (slot.depth * 100.0f)) + "%,  "
                + (bipolar ? "bipolar (-1 to 1 across)" : "unipolar (0 to 1 across)"),
            "Left to right: the source's range.  Bottom to top: what it sends.",
            "Click to add a point, drag the middle dots to bend, double-click to remove.",
        };
        for (const auto& line : lines)
            g.drawFittedText (line, info.removeFromTop (20), juce::Justification::centredLeft, 1, 0.85f);
    }

    // A click on a sortable heading.
    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mouseWasDraggedSinceMouseDown() || event.mods.isPopupMenu())
            return;

        for (const auto& heading : headings())
            if (heading.sort != Sort::none && heading.area.contains (event.getPosition()))
                setSort (heading.sort);
    }

    void mouseMove (const juce::MouseEvent&) override { repaint (headerArea); }
    void mouseExit (const juce::MouseEvent&) override { repaint (headerArea); }

    enum class Sort { none, slot, source, destination, amount };

    // Sorts by a column: a first click sorts (amounts largest first), a
    // second reverses, a third goes back to slot order. Public for tests.
    void setSort (Sort column)
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

    Sort getSort() const { return sort; }
    const std::vector<int>& getVisibleSlots() const { return visibleRows; }
    RemapEditor* getRemapEditor() const { return remapEditor.get(); }

    // Opens the slot's remap editor in the dock under the rows (or closes
    // it if open). The rows never move for it (UI review 6, V6-22).
    void toggleRemap (int slot)
    {
        if (remapEditor != nullptr && remapEditor->getSlotIndex() == slot)
        {
            closeRemap();
            return;
        }

        closeRemap();
        const auto colour = modSourceColour ((int) processorRef.readModSlot (slot).source);
        remapEditor = std::make_unique<RemapEditor> (processorRef, slot, colour);
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

    void closeRemap()
    {
        if (remapEditor == nullptr)
            return;

        removeChildComponent (remapEditor.get());
        remapEditor.reset();

        for (auto& row : rows)
            row->setSelected (false);

        resized();
        repaint();
    }

    // The dock's area (empty when it isn't shown); the tests.
    juce::Rectangle<int> getDockArea() const { return dockArea; }

    // Folds every repeated routing it can into one row.
    void mergeAll()
    {
        processorRef.performEdit ("Merge repeated routings", [this] { processorRef.mergeDuplicateModSlots(); });
        updateRows (true);
    }

    // The "!" of a repeated row: merge it with each of its twins.
    void showMergeMenu (int slot)
    {
        juce::PopupMenu menu;
        const auto routing = processorRef.readModSlot (slot);
        menu.addSectionHeader (ModNames::source ((int) routing.source, &processorRef) + juce::String::fromUTF8 ("  \xe2\x86\x92  ")
                               + ModNames::destination (routing.destination));

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

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (rows[(size_t) slot].get()),
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

    juce::Rectangle<float> emptyStateCard() const
    {
        const auto area = viewport.getBounds().withTrimmedTop (56);
        return juce::Rectangle<float> (560.0f, 250.0f).withCentre (area.toFloat().getCentre()).withY ((float) area.getY() + 20.0f);
    }

    // An empty matrix explains the three ways in, with a little animated
    // routing and an arrow down to the source chips.
    void paintEmptyState (juce::Graphics& g)
    {
        const auto now = emptyClock;
        const auto area = viewport.getBounds().withTrimmedTop (56);
        const auto card = emptyStateCard();
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
        const char* const tips[] {
            "Drag a source chip from the bar below onto any knob",
            "or right-click a knob for quick modulation",
            "or press  + ADD MODULATION  above to build a routing here"
        };

        for (int i = 0; i < 3; ++i)
            g.drawText (tips[i], card.withTrimmedTop (136.0f + (float) i * 22.0f).withHeight (20.0f), juce::Justification::centred);

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("OR START FROM ONE OF THESE", starterArea().withHeight (16).translated (0, -22), juce::Justification::centred);

        // A chevron bobbing towards the source chips, under the starters
        // (left out when there is no room: it used to sit on a starter).
        const auto bob = 4.0f * std::sin (now * 3.0f);
        const auto tipY = juce::jmax ((float) getHeight() - 22.0f, (float) starterArea().getBottom() + 18.0f);
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

    void resized() override
    {
        auto area = getLocalBounds().reduced (12, 6);
        auto top = area.removeFromTop (32);
        addButton.setBounds (top.removeFromRight (190).withTrimmedTop (6).withTrimmedBottom (2));
        top.removeFromRight (8);
        mergeButton.setBounds (top.removeFromRight (210).withTrimmedTop (6).withTrimmedBottom (2));
        headerArea = area.removeFromTop (18);

        // Rows grow a little taller while there are few (V7-20), as long as
        // the dock's note still fits under them; with no room for the note,
        // they grow to fill the page instead of leaving its bottom empty
        // (V8-20).
        const auto listRows = (int) visibleRows.size() + 1; // (and the add row)
        rowHeight = MatrixRow::rowHeight;
        while (rowHeight < 34 && listRows * (rowHeight + 1) + 8 + dockHeight + 12 <= area.getHeight())
            ++rowHeight;

        // The dock takes the bottom while a remap is open, and also when the
        // rows leave that much room (as a note on how to open it).
        const auto rowsHeight = listRows * rowHeight + 8;
        const auto showDock = ! visibleRows.empty()
                              && (remapEditor != nullptr || area.getHeight() - rowsHeight >= dockHeight + 12);

        // (Decided by the note alone, so opening a remap never moves a row:
        // the list just scrolls above the dock, V6-22.)
        if (area.getHeight() - rowsHeight < dockHeight + 12)
            while (rowHeight < 38 && listRows * (rowHeight + 1) + 8 <= area.getHeight())
                ++rowHeight;
        dockArea = showDock ? area.removeFromBottom (dockHeight).withTrimmedTop (8) : juce::Rectangle<int>();

        if (remapEditor != nullptr)
            remapEditor->setBounds (dockArea.withWidth (juce::jmin (560, dockArea.getWidth() * 3 / 5)));

        // The tiles: one row along the bottom of the dock's info side.
        const auto tilesShown = remapEditor != nullptr;
        auto tiles = tilesShown ? dockArea.withTrimmedLeft (remapEditor->getRight() - dockArea.getX() + 16).removeFromBottom (48)
                                : juce::Rectangle<int>();
        const auto tileWidth = juce::jmin (64, tiles.getWidth() / juce::jmax (1, (int) shapeTiles.size()));
        for (auto& tile : shapeTiles)
        {
            tile->setVisible (tilesShown);
            if (tilesShown)
            {
                tile->colour = modSourceColour ((int) processorRef.readModSlot (remapEditor->getSlotIndex()).source);
                tile->setBounds (tiles.removeFromLeft (tileWidth).reduced (2, 0));
            }
        }

        viewport.setBounds (area);
        layoutList();
        layoutStarters();
    }

    // Switching to the tab shows the current routings straight away.
    void visibilityChanged() override
    {
        if (isVisible())
            updateRows();
    }

private:
    int rowHeight = MatrixRow::rowHeight;
    static constexpr int dockHeight = 210;

    struct Heading
    {
        juce::String text;
        juce::Rectangle<int> area;
        Sort sort;
    };

    // The column headings and where they sit, aligned with MatrixRow.
    std::vector<Heading> headings() const
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
        add ("AMOUNT", C::amount, C::gap, Sort::amount);
        add ("CURVE", C::curve, C::gap, Sort::none);
        add ("POLARITY", C::polarity, C::gap * 3, Sort::none);
        add ("DESTINATION", C::destination, C::gap, Sort::destination);
        return result;
    }

    struct Starter
    {
        const char* label;
        const char* tip;
        Mod::Source source, aux;
        std::vector<Mod::Destination> destinations;
        float depth;
    };

    static const std::vector<Starter>& starterRoutings()
    {
        using S = Mod::Source;
        using D = Mod::Destination;
        static const std::vector<Starter> starters {
            { "LFO 1  >  CUTOFF", "LFO 1 sweeps filter 1's cutoff.", S::Lfo1, S::None, { D::Filter1Cutoff }, 0.3f },
            { "MOD ENV  >  FRAME", "The mod envelope moves OSC 1's wavetable position on every note.", S::ModEnv, S::None, { D::Osc1Frame }, 0.5f },
            { "MOD WHEEL  >  VIBRATO", "LFO 2 wobbles the pitch of OSC 1-3, as far as the mod wheel lets it.", S::Lfo2, S::ModWheel,
              { D::Osc1Pitch, D::Osc2Pitch, D::SubPitch }, 0.006f },
            { "VELOCITY  >  CUTOFF", "Harder notes open filter 1.", S::Velocity, S::None, { D::Filter1Cutoff }, 0.35f },
            { "LFO 2  >  PAN", "LFO 2 moves OSC 1 across the stereo field.", S::Lfo2, S::None, { D::Osc1Pan }, 0.5f },
            { "MACRO 1  >  DRIVE", "Macro 1 drives filter 1.", S::Macro1, S::None, { D::Filter1Drive }, 0.5f }
        };
        return starters;
    }

    juce::Rectangle<int> starterArea() const
    {
        const auto area = viewport.getBounds().withTrimmedTop (56);
        return juce::Rectangle<int> (600, 74).withCentre (area.getCentre()).withY (area.getY() + 20 + 250 + 36);
    }

    void layoutStarters()
    {
        const auto area = starterArea();
        const auto width = area.getWidth() / 3;

        for (size_t i = 0; i < starterButtons.size(); ++i)
            starterButtons[i]->setBounds (juce::Rectangle<int> (area.getX() + (int) (i % 3) * width, area.getY() + (int) (i / 3) * 37,
                                                                width, 37).reduced (4, 3));
    }

    void addStarter (const Starter& starter)
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

    void addRouting()
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

    // The used slots, in the chosen order.
    std::vector<int> sortedSlots() const
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
            return ModNames::destination (processorRef.readModSlot (index).destination);
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

    // force: re-sort now, even mid-gesture (a heading click).
    void updateRows (bool force = false)
    {
        refreshMacroNames();
        refreshPoolSources();

        auto used = sortedSlots();

        for (auto& button : starterButtons)
            button->setVisible (used.empty());

        // Rows don't jump about under the mouse: while a button is held (an
        // amount being dragged, a menu open), a new order waits.
        const auto sameSet = used.size() == visibleRows.size()
                             && std::is_permutation (used.begin(), used.end(), visibleRows.begin());
        if (! force && sameSet && juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
            used = visibleRows;

        // Number the rows as shown, and mark repeated source / destination
        // pairs (older patches can have them; both rows still play).
        std::map<std::pair<int, int>, std::vector<int>> pairs;
        auto anyVia = false;

        for (size_t i = 0; i < used.size(); ++i)
        {
            const auto slot = processorRef.readModSlot (used[i]);
            anyVia = anyVia || slot.aux != Mod::Source::None;

            if (slot.source != Mod::Source::None && slot.destination != 0)
                pairs[{ (int) slot.source, slot.destination }].push_back ((int) i + 1);
        }

        auto duplicates = 0;

        for (size_t i = 0; i < used.size(); ++i)
        {
            const auto slot = processorRef.readModSlot (used[i]);
            juce::StringArray others;

            if (const auto found = pairs.find ({ (int) slot.source, slot.destination }); found != pairs.end() && slot.destination != 0)
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

            for (auto& row : rows)
                row->setVisible (std::find (used.begin(), used.end(), row->getSlotIndex()) != used.end());

            addButton.setEnabled (used.size() < (size_t) Mod::maxSlots);
            addButton.setVisible (used.empty());
            resized();
            repaint();
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
    }

    void layoutList()
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

        for (const auto index : visibleRows)
        {
            auto& row = *rows[(size_t) index];
            row.setBounds (0, y, width, rowHeight);
            y += rowHeight;
        }

        // "+ ADD MODULATION" as the row after the last routing (S7-35).
        addRow.setVisible (! visibleRows.empty() && visibleRows.size() < (size_t) Mod::maxSlots);
        if (addRow.isVisible())
        {
            addRow.setBounds (0, y + 2, width, rowHeight - 4);
            y += rowHeight;
        }

        list.setSize (width, y + 8);
    }

    void timerCallback() override
    {
        if (! isShowing())
        {
            emptyShownSeconds = 0.0f;
            return;
        }

        if (changeGate.check (processorRef.getUiEpoch()))
            updateRows();

        // The empty state's cable plays for a few seconds after the page
        // opens, and while the mouse is over it, then rests.
        if (visibleRows.empty() && (emptyShownSeconds < 6.0f || isMouseOver (true)))
        {
            emptyShownSeconds += frameSeconds();
            emptyClock += frameSeconds();
            repaint (emptyStateCard().expanded (4.0f).getSmallestIntegerContainer());
        }
    }

    float emptyShownSeconds = 0.0f, emptyClock = 0.0f;
    IlanaAnim::ChangeGate changeGate;

    // Shows the patch's macro names in the source lists.
    void refreshMacroNames()
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

    // Greys out the LFOs and envelopes that aren't in their pools.
    void refreshPoolSources()
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

    // Grouped by source unless the user picks another order (S8-25).
    Sort sort = Sort::source;
    bool descending = false, viaExpanded = false;
    int numDuplicates = 0, numIdle = 0;
    juce::String repeatText;
    juce::Rectangle<int> dockArea;
    std::unique_ptr<RemapEditor> remapEditor;

    IlanaSynthAudioProcessor& processorRef;
    juce::StringArray shownMacroNames;
    std::vector<bool> shownPoolSources;
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<MatrixRow>> rows;
    std::vector<int> visibleRows;
    juce::TextButton addButton, mergeButton;
    DashedAddButton addRow { "+  ADD MODULATION", "+  ADD MODULATION" };
    std::vector<std::unique_ptr<juce::TextButton>> starterButtons;
    std::vector<std::unique_ptr<RemapShapeTile>> shapeTiles;
    juce::Rectangle<int> headerArea;
};
} // namespace
