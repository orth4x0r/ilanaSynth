// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
class MatrixPage : public juce::Component,
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
            list.addChildComponent (*row);
            rows.push_back (std::move (row));
        }

        // Pinned in the header, so it never scrolls away under the rows.
        addButton.setButtonText ("+  ADD MODULATION");
        addButton.setTooltip ("Add a routing.  You can also drag any source chip, macro name or LFO card onto a knob.");
        addButton.onClick = [this] { addRouting(); };
        addAndMakeVisible (addButton);

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

        paintSectionTitle (g, "MODULATION", juce::Rectangle<int> (headingX, 12, juce::jmax (100, addButton.getX() - headingX - 12), headingHeight),
                           juce::String (used) + " of " + juce::String (Mod::maxSlots) + " slots in use"
                           + (numDuplicates > 0 ? ",  " + juce::String (numDuplicates) + " repeated (marked !)" : juce::String())
                           + ".   Drag a source onto any knob, then drag the dot beside it to set the depth.");

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
            const auto sorted = heading.sort != Sort::slot && heading.sort == sort;
            const auto hot = heading.sort != Sort::none && heading.area.contains (getMouseXYRelative()) && isMouseOver (true);
            g.setColour (sorted || hot ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
            const auto arrow = sorted ? juce::String::fromUTF8 (descending ? "  \xe2\x96\xbc" : "  \xe2\x96\xb2") : juce::String();
            g.drawText (heading.text + arrow, heading.area, juce::Justification::centredLeft);
        }
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

    // Opens the slot's remap editor under its row (or closes it if open).
    void toggleRemap (int slot)
    {
        if (remapEditor != nullptr && remapEditor->getSlotIndex() == slot)
        {
            closeRemap();
            return;
        }

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
        list.addAndMakeVisible (*remapEditor);
        layoutList();

        // Scroll so the row and its editor show.
        const auto editorBounds = remapEditor->getBounds();
        const auto view = viewport.getViewArea();
        if (editorBounds.getBottom() > view.getBottom())
            viewport.setViewPosition (0, editorBounds.getBottom() - view.getHeight() + 4);
    }

    void closeRemap()
    {
        if (remapEditor == nullptr)
            return;

        list.removeChildComponent (remapEditor.get());
        remapEditor.reset();
        layoutList();
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
        addButton.setBounds (area.removeFromTop (32).removeFromRight (190).withTrimmedTop (6).withTrimmedBottom (2));
        headerArea = area.removeFromTop (18);
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
    static constexpr int rowHeight = MatrixRow::rowHeight;
    static constexpr int remapHeight = 200;

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
        add ("ON", C::bypass, C::gap + C::meter + C::gap, Sort::none);
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
            { "WHEEL  >  VIBRATO", "LFO 2 wobbles the pitch of OSC 1-3, as far as the mod wheel lets it.", S::Lfo2, S::ModWheel,
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
                updateRows();
                viewport.setViewPosition (0, list.getHeight());
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

        const auto sources = Mod::getSourceNames();
        const auto destinations = Mod::getDestinationNames();
        const auto key = [&] (int index) -> juce::String
        {
            const auto slot = processorRef.readModSlot (index);
            return sort == Sort::source ? sources[(int) slot.source] : destinations[slot.destination];
        };

        std::stable_sort (used.begin(), used.end(), [&] (int a, int b)
        {
            if (sort == Sort::amount)
            {
                const auto depthA = std::abs (processorRef.readModSlot (a).depth);
                const auto depthB = std::abs (processorRef.readModSlot (b).depth);
                return descending ? depthA > depthB : depthA < depthB;
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

        if (duplicates != numDuplicates)
        {
            numDuplicates = duplicates;
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
            layoutList();
            repaint();
        }

        for (const auto index : visibleRows)
            rows[(size_t) index]->refresh();
    }

    void layoutList()
    {
        const auto width = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
        auto y = 0;

        // A routing removed while its remap editor was open takes the editor with it.
        if (remapEditor != nullptr
            && std::find (visibleRows.begin(), visibleRows.end(), remapEditor->getSlotIndex()) == visibleRows.end())
        {
            list.removeChildComponent (remapEditor.get());
            remapEditor.reset();
        }

        for (const auto index : visibleRows)
        {
            auto& row = *rows[(size_t) index];
            row.setBounds (0, y, width, rowHeight);
            y += rowHeight;

            // The remap editor opens under its own row, beside the curve
            // it edits, so it covers no other row's amount.
            if (remapEditor != nullptr && remapEditor->getSlotIndex() == index)
            {
                const auto curveX = row.getCurve().getBounds().getCentreX();
                const auto editorWidth = juce::jmin (480, width - 8);
                const auto x = juce::jlimit (4, width - editorWidth - 4, curveX - editorWidth / 2);
                remapEditor->setTitle ("ROW " + juce::String (row.getDisplayNumber()) + "  (slot " + juce::String (index + 1) + ")");
                remapEditor->setBounds (x, y + 2, editorWidth, remapHeight);
                y += remapHeight + 6;
            }
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

    Sort sort = Sort::slot;
    bool descending = false, viaExpanded = false;
    int numDuplicates = 0;
    std::unique_ptr<RemapEditor> remapEditor;

    IlanaSynthAudioProcessor& processorRef;
    juce::StringArray shownMacroNames;
    std::vector<bool> shownPoolSources;
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<MatrixRow>> rows;
    std::vector<int> visibleRows;
    juce::TextButton addButton;
    std::vector<std::unique_ptr<juce::TextButton>> starterButtons;
    juce::Rectangle<int> headerArea;
};
} // namespace
