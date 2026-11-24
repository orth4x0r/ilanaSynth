void uiGroup_MOD_RINGS (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Modulation rings: every knob on every page that drives a destination
    // shows that destination's live depth. Each destination is routed from a
    // macro at full travel, a note is played, and the depth the ring reads
    // must move.
    if (pages != nullptr)
    {
        std::map<int, juce::String> destinations;
        juce::StringArray ringless;
        for (const auto& id : pages->getPageIds())
        {
            pages->showPage (id);
            settle (60);
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
            {
                if (knob->getRingDestination() > 0)
                    destinations.emplace (knob->getRingDestination(), knob->getParameterId());
                else if (knob->getParameterId().isNotEmpty())
                    ringless.addIfNotAlreadyThere (knob->getParameterId());
            }
        }
        pages->showPage ("MAIN");

        juce::StringArray silent;
        for (const auto& [destination, id] : destinations)
        {
            IlanaSynthAudioProcessor probe;
            probe.prepareToPlay (48000.0, 512);
            if (auto* macro = probe.apvts.getParameter ("macro1"))
                macro->setValueNotifyingHost (1.0f);
            probe.assignModSlot ((int) Mod::Source::Macro1, destination, 0.5f);
            juce::AudioBuffer<float> buffer (2, 512);
            for (int block = 0; block < 3; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                buffer.clear();
                probe.processBlock (buffer, midi);
            }
            if (std::abs (probe.getModDisplay (destination)) < 0.001f)
                silent.add (id);
        }
        expect (silent.isEmpty(), "every modulatable knob's ring shows its depth (" + juce::String ((int) destinations.size())
                                      + " destinations" + (silent.isEmpty() ? juce::String() : "; silent: " + silent.joinIntoString (", ")) + ")");
        if (juce::SystemStats::getEnvironmentVariable ("ILANA_RING_REPORT", "").isNotEmpty())
            std::cout << "knobs without a ring: " << ringless.joinIntoString (", ") << std::endl;
    }
}

void uiGroup_REVIEW4C (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 4, batch C (V4, V10, V12, V29, S5, S7, S19, S21, S22):
    // LFOs and envelopes.
    {
        processor.loadFactoryPreset (neuroWobble);
        settle (200);

        const auto event = [] (juce::Component& component, juce::Point<float> position, bool dragged)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys(),
                                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(),
                                     position, juce::Time::getCurrentTime(), 1, dragged);
        };
        const auto readShape = [&processor] (int lfo)
        {
            return juce::roundToInt (processor.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape")->load());
        };
        const auto setShape = [&processor] (int lfo, int shape)
        {
            auto* parameter = processor.apvts.getParameter ("lfo" + juce::String (lfo + 1) + "_shape");
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) shape));
        };
        const auto clearHistory = [&processor]
        {
            processor.apvts.copyState();
            processor.getUndoManager().clearUndoHistory();
        };
        const auto undoSteps = [&processor]
        {
            processor.apvts.copyState();
            return processor.getUndoManager().getUndoDescriptions();
        };

        // The first drag on a preset wave makes it a Curve with the wave's
        // points, as one undo step; a plain click changes nothing.
        {
            setShape (0, LfoShapes::Triangle);
            const auto curveBefore = processor.getLfoCurve (0).toString();
            LfoDisplay lfoDisplay (processor, 0);
            juce::Component& display = lfoDisplay;
            display.setSize (300, 150);
            clearHistory();
            display.mouseDown (event (display, { 80.0f, 110.0f }, false));
            display.mouseUp (event (display, { 80.0f, 110.0f }, false));
            const auto clickChanged = readShape (0) != LfoShapes::Triangle || ! undoSteps().isEmpty();

            display.mouseDown (event (display, { 80.0f, 110.0f }, false));
            display.mouseDrag (event (display, { 90.0f, 70.0f }, true));
            display.mouseUp (event (display, { 90.0f, 70.0f }, true));
            const auto converted = processor.getLfoCurve (0);
            const auto steps = undoSteps();
            const auto shapeAfter = readShape (0);
            processor.getUndoManager().undo();
            const auto undoneShape = readShape (0);
            const auto undoneCurve = processor.getLfoCurve (0).toString();
            processor.getUndoManager().redo();

            expect (! clickChanged, "a click on a Triangle LFO changes nothing");
            expect (shapeAfter == IlanaSynthAudioProcessor::curveShape && steps.size() == 1 && steps[0] == "LFO 1 curve"
                        && std::abs (converted.valueAt (0.5) - 1.0f) < 0.02f && std::abs (converted.valueAt (0.75)) < 0.05f
                        && std::abs (converted.valueAt (0.0) + 1.0f) < 0.02f,
                    "dragging a Triangle LFO turns it into a Curve with the triangle's points in one undo step ('"
                        + steps.joinIntoString ("', '") + "', peak " + juce::String (converted.valueAt (0.5), 2) + ")");
            expect (undoneShape == LfoShapes::Triangle && undoneCurve == curveBefore && readShape (0) == IlanaSynthAudioProcessor::curveShape,
                    "undo puts the Triangle and the old curve back, redo the converted curve");
            expect (lfoDisplay.getGridDivisions() == 8, "the LFO graph shows its snap grid control (GRID 8)");
            setShape (0, LfoShapes::Triangle);
        }

        // Steps are an LFO shape edited in that LFO's own graph (UI review 6,
        // V6-20 / I6-20): a drag on a bar sets that step, one undo step.
        {
            pages->showPage ("MAIN");
            settle (200);
            const auto shapeBefore = readShape (0);
            setShape (0, LfoShapes::Steps);
            auto* lfoPage = uiOpenSourcePopover (ctx, Mod::Source::Lfo1); // (LFO 1's pop-up edits its steps)
            settle (200);

            std::vector<LfoDisplay*> lfoDisplays;
            if (lfoPage != nullptr)
                findAll<LfoDisplay> (*lfoPage, lfoDisplays);
            LfoDisplay* shown = nullptr;
            for (auto* display : lfoDisplays)
                if (visibleInTree (display))
                    shown = display;

            if (shown != nullptr)
            {
                auto* step5 = processor.apvts.getParameter ("lfo1_step5");
                const auto stepBefore = step5->getValue();
                // The time ruler sits under the well (rulerHeight + 2), outside the plot.
                const auto plot = shown->getLocalBounds().toFloat().withTrimmedBottom (16.0f).reduced (10.0f, 14.0f);
                const juce::Point<float> at (plot.getX() + plot.getWidth() * 4.5f / 16.0f, plot.getCentreY() - plot.getHeight() * 0.42f * 0.5f);
                clearHistory();
                auto& component = static_cast<juce::Component&> (*shown);
                component.mouseDown (event (component, at, false));
                component.mouseDrag (event (component, at, true));
                component.mouseUp (event (component, at, true));
                settle (50);
                const auto value = step5->convertFrom0to1 (step5->getValue());
                const auto steps = undoSteps();
                processor.getUndoManager().undo();
                expect (std::abs (value - 0.5f) < 0.05f && steps.size() == 1 && std::abs (step5->getValue() - stepBefore) < 0.001f,
                        "a Steps LFO's graph edits its steps: a drag on bar 5 sets it to " + juce::String (value, 2)
                            + " (want 0.50) in one undo step, undo puts it back");
            }
            else
            {
                expect (false, "LFO 1's pop-up shows its graph");
            }

            uiCloseSourcePopover (ctx);
            setShape (0, shapeBefore);
            settle (100);
        }

        // One RATE knob: while synced it shows the division, on MOD and PLAY.
        {
            const auto findKnob = [&editor, &pages] (const juce::String& id) -> KnobControl*
            {
                std::vector<KnobControl*> knobs;
                auto* popover = pages->getSourcePopover(); // (an open pop-up is searched alone)
                findAll<KnobControl> (popover != nullptr && popover->isVisible() ? *popover : static_cast<juce::Component&> (*editor), knobs);
                for (auto* knob : knobs)
                    if (knob->getParameterId() == id && visibleInTree (knob) && knob->getWidth() > 0)
                        return knob;
                return nullptr;
            };

            // PLAY's MODULATION card: SYNC swaps RATE for the division in place.
            {
                pages->showPage ("MAIN");
                settle (300);
                if (auto* page = pages->getCurrentPage())
                    if (auto* card = findChild<ModulationCardView> (*page))
                        card->selectSource ((int) Mod::lfoSourceFor (0));
                settle (100);
                auto* sync = processor.apvts.getParameter ("lfo1_sync");
                auto* divParam = processor.apvts.getParameter ("lfo1_div");
                if (sync != nullptr && divParam != nullptr)
                {
                    const auto syncBefore = sync->getValue();
                    sync->setValueNotifyingHost (1.0f);
                    settle (300);
                    auto* division = findKnob ("lfo1_div");
                    const auto divText = divParam->getText (divParam->getValue(), 0);
                    const auto shownText = division != nullptr ? division->getSlider().getTextFromValue (division->getSlider().getValue()) : juce::String();
                    expect (division != nullptr && shownText == divText && findKnob ("lfo1_rate") == nullptr,
                            "MAIN: a synced LFO's RATE knob shows the division ('" + shownText + "', want '" + divText + "')");
                    sync->setValueNotifyingHost (0.0f);
                    settle (300);
                    expect (findKnob ("lfo1_rate") != nullptr && findKnob ("lfo1_div") == nullptr, "MAIN: free-running, RATE is the Hz knob again");
                    sync->setValueNotifyingHost (syncBefore);
                    settle (100);
                }
                else
                    expect (false, "MAIN: an LFO RATE knob is on the page");
            }

            for (const auto* page : { "LFO 1 pop-up" })
            {
                pages->showPage ("MAIN");
                uiOpenSourcePopover (ctx, Mod::Source::Lfo1);
                settle (300);

                // Whichever LFO the page shows.
                auto lfo = -1;
                for (int index = 0; index < IlanaSynthAudioProcessor::numLfos && lfo < 0; ++index)
                    if (findKnob ("lfo" + juce::String (index + 1) + "_rate") != nullptr || findKnob ("lfo" + juce::String (index + 1) + "_div") != nullptr)
                        lfo = index;

                const auto prefix = "lfo" + juce::String (lfo + 1);
                auto* sync = processor.apvts.getParameter (prefix + "_sync");
                auto* divParam = processor.apvts.getParameter (prefix + "_div");

                if (lfo < 0 || sync == nullptr || divParam == nullptr)
                {
                    expect (false, juce::String (page) + ": an LFO RATE knob is on the page");
                    continue;
                }

                const auto syncBefore = sync->getValue();
                sync->setValueNotifyingHost (1.0f);
                settle (300);
                const auto divText = divParam->getText (divParam->getValue(), 0);
                auto* division = findKnob (prefix + "_div");
                const auto shownText = division != nullptr ? division->getSlider().getTextFromValue (division->getSlider().getValue()) : juce::String();
                expect (division != nullptr && division->getLabelText() == "RATE" && shownText == divText && findKnob (prefix + "_rate") == nullptr,
                        juce::String (page) + ": a synced LFO's RATE knob shows the division ('" + shownText + "', want '" + divText + "')");

                sync->setValueNotifyingHost (0.0f);
                settle (300);
                expect (findKnob (prefix + "_rate") != nullptr && findKnob (prefix + "_div") == nullptr,
                        juce::String (page) + ": free-running, RATE is the Hz knob again");
                sync->setValueNotifyingHost (syncBefore);
                settle (100);
                uiCloseSourcePopover (ctx);
            }
        }

        // UI review 6, P4: LFO panels, MSEG and envelope graphs (in the source pop-ups; the MOD page's pool cards
        // and their checks went with that page).
        {
            using M = IlanaSynthAudioProcessor::Module;
            constexpr auto numLfos = IlanaSynthAudioProcessor::numLfos;
            pages->showPage ("MAIN");
            settle (300);
            juce::Component* root = pages->getCurrentPage();

            {
                const auto click = [&event] (juce::Component& component, juce::Point<float> at)
                {
                    component.mouseDown (event (component, at, false));
                    component.mouseUp (event (component, at, false));
                };
                const auto visibleOf = [&root] (auto* dummy)
                {
                    using Type = std::remove_pointer_t<decltype (dummy)>;
                    std::vector<Type*> all, shown;
                    findAll<Type> (*root, all);
                    for (auto* component : all)
                        if (visibleInTree (component) && component->getWidth() > 0)
                            shown.push_back (component);
                    return shown;
                };

                // Times of 0 read "0 ms": "Off" is for switches (S7-29).
                expect (describeValue ("amp_delay", 0.0f) == "0 ms" && describeValue ("fe_hold", 0.0f) == "0 ms"
                            && describeValue ("opeg_lfo_delay", 0.0f) == "0 ms",
                        "an envelope's DELAY / HOLD and the Op LFO's DELAY at 0 read 0 ms, not Off");
                // A word in a number's place is a value, not a dim label that forgot one
                // (I9-25, review 11 S11-17): SEED reads Random, EXCITE POS Auto, in the value colour.
                expect (describeValue ("lfo1_seed", 0.0f) == "Random" && describeValue ("osc1_string_excite_pos", 0.0f) == "Auto"
                            && ! IlanaLookAndFeel::isPlaceholderValue (describeValue ("lfo1_seed", 0.0f))
                            && ! IlanaLookAndFeel::isPlaceholderValue (describeValue ("osc1_string_excite_pos", 0.0f))
                            && ! IlanaLookAndFeel::isPlaceholderValue (describeValue ("lfo1_seed", 3.0f)),
                        "SEED reads Random and EXCITE POS Auto in the value colour, as the numbers do");

                // Every simulated shape's graph: output B is a draggable tag on it (I6-24), and the LFO's
                // source chip carries a "B" for it too. (The MOD page's panel-grid checks, V6-11 / S6-12, went
                // with the page.)
                {
                    const auto shapeBefore = readShape (0);
                    root = uiOpenSourcePopover (ctx, Mod::Source::Lfo1);
                    juce::StringArray problems;
                    for (int shape = LfoSimShapes::RandomHold; shape <= LfoSimShapes::Friction; ++shape)
                    {
                        setShape (0, shape);
                        settle (150);

                        LfoDisplay* display = nullptr;
                        for (auto* candidate : visibleOf ((LfoDisplay*) nullptr))
                            display = candidate;
                        const auto tagB = display != nullptr ? display->getOutputTagBounds (1) : juce::Rectangle<float>();
                        if (tagB.isEmpty() || ! display->getLocalBounds().toFloat().contains (tagB))
                            problems.add (juce::String (shape) + ": no output B tag");
                    }
                    // The LFO's source chip carries a "B" for output B too.
                    {
                        std::vector<ModSourceChip*> chips;
                        findAll<ModSourceChip> (*editor, chips);
                        for (auto* chip : chips)
                            if (chip->getSourceIndex() == (int) Mod::Source::Lfo1)
                            {
                                settle (100);
                                if ((chip->getSecondOutputBounds().isEmpty() || chip->secondIndex != (int) Mod::Source::Lfo1B))
                                    problems.add ("LFO 1's chip has no OUT 2");
                                // S8-16 / V8-28 / V9-20: over the sub-chip, the
                                // tooltip names the second output, spelt out.
                                else if (! chip->tooltipAt (chip->getSecondOutputBounds().getCentre()).contains ("OUT 2")
                                         || chip->tooltipAt ({ 4.0f, 4.0f }).contains ("OUT 2"))
                                    problems.add ("LFO 1's OUT 2 has no tooltip naming it");
                            }
                    }
                    expect (problems.isEmpty(), "every simulated LFO shape's graph has its output B tag, and the chip names OUT 2"
                                                    + (problems.isEmpty() ? juce::String() : " (" + problems.joinIntoString ("; ") + ")"));
                    setShape (0, shapeBefore);
                    settle (100);
                }

                // The SHAPE list is grouped for display only: every shape in
                // one group, the box's items still in parameter order.
                {
                    std::vector<int> counts (30, 0);
                    for (const auto& group : LfoShapeMenu::groups())
                        for (const auto shape : group.shapes)
                            if (juce::isPositiveAndBelow (shape, 30))
                                ++counts[(size_t) shape];
                    auto eachOnce = std::all_of (counts.begin(), counts.end(), [] (int count) { return count == 1; });
                    ComboControl* shapeBox = nullptr;
                    for (auto* combo : visibleOf ((ComboControl*) nullptr))
                        if (combo->getComboBox().getNumItems() == 30)
                            shapeBox = combo;
                    auto inOrder = shapeBox != nullptr;
                    for (int item = 0; shapeBox != nullptr && item < 30; ++item)
                        inOrder = inOrder && shapeBox->getComboBox().getItemId (item) == item + 1;
                    expect (eachOnce && inOrder && shapeBox->getComboBox().getItemText (LfoSimShapes::Rossler).contains ("ssler"),
                            "the LFO SHAPE list groups every shape once and keeps the saved indices");
                }

                // The MSEG is a card in the LFO pool, edited in the LFO's
                // place; a dragged point follows the mouse, one undo step.
                // (The old module has a card only while the patch uses it, UI
                // review 8, I8-4: here OSC 1's warp envelope plays it.)
                auto* warpEnvelope = processor.apvts.getParameter ("osc1_pd_env");
                const auto warpBefore = warpEnvelope != nullptr ? warpEnvelope->getValue() : 0.0f;
                if (warpEnvelope != nullptr)
                    warpEnvelope->setValueNotifyingHost (warpEnvelope->convertTo0to1 (17.0f));
                settle (200);
                {
                    root = uiOpenSourcePopover (ctx, Mod::Source::Mseg);
                    settle (200);
                    MsegEditor* mseg = nullptr;
                    for (auto* candidate : visibleOf ((MsegEditor*) nullptr))
                        mseg = candidate;
                    if (mseg != nullptr)
                    {
                        // GRID snaps the time (UI review 7, I7-30): point 2
                        // lands on a column; then free, it follows the mouse.
                        {
                            const auto plotArea = mseg->getPlotArea();
                            const auto from = mseg->getPointPosition (1);
                            const auto to = juce::Point<float> (plotArea.getX() + plotArea.getWidth() * 0.3f, from.y);
                            auto& component = static_cast<juce::Component&> (*mseg);
                            component.mouseDown (event (component, from, false));
                            component.mouseDrag (event (component, to, true));
                            const auto landed = mseg->getPointPosition (1);
                            component.mouseUp (event (component, to, true));
                            settle (50);
                            const auto column = (landed.x - plotArea.getX()) / plotArea.getWidth() * (float) mseg->getGridDivisions();
                            processor.getUndoManager().undo();
                            settle (50);
                            expect (mseg->getGridDivisions() == 8 && std::abs (column - std::round (column)) < 0.05f,
                                    "the MSEG's GRID 8 snaps a dragged point to a column (" + juce::String (column, 2) + ")");
                        }
                        mseg->setGridDivisions (0);
                        const auto from = mseg->getPointPosition (1);
                        // Up or down to +0.3 (clear of the level's limits), a
                        // little sideways.
                        const auto plotArea = mseg->getPlotArea();
                        const auto to = juce::Point<float> (from.x + 8.0f, plotArea.getCentreY() - 0.3f * plotArea.getHeight() * 0.5f);
                        clearHistory();
                        auto& component = static_cast<juce::Component&> (*mseg);
                        component.mouseDown (event (component, from, false));
                        component.mouseDrag (event (component, to, true));
                        const auto landed = mseg->getPointPosition (1);
                        component.mouseUp (event (component, to, true));
                        settle (50);
                        const auto steps = undoSteps();
                        processor.getUndoManager().undo();
                        settle (50);
                        expect (landed.getDistanceFrom (to) < 3.0f && steps.size() == 1 && mseg->getPointPosition (1).getDistanceFrom (from) < 1.0f,
                                "the MSEG's pop-up has its editor; point 2 follows the mouse (" + juce::String (landed.getDistanceFrom (to), 1)
                                    + " px off) in one undo step");
                        mseg->setGridDivisions (8);
                    }
                    else
                    {
                        expect (false, "the MSEG's pop-up opens the MSEG editor");
                    }
                    uiCloseSourcePopover (ctx);
                    settle (100);
                }
                if (warpEnvelope != nullptr)
                    warpEnvelope->setValueNotifyingHost (warpBefore);
                settle (100);

                // Envelope graphs: the time ruler sits below the plot
                // (V6-17), and each segment's dot curves that segment alone,
                // with its value beside it while dragged (V5-18).
                {
                    root = uiOpenSourcePopover (ctx, Mod::Source::AmpEnv);
                    settle (150);
                    EnvelopeDisplay* amp = nullptr;
                    for (auto* candidate : visibleOf ((EnvelopeDisplay*) nullptr))
                        amp = candidate;
                    if (amp != nullptr)
                    {
                        const auto plot = amp->getPlotArea();
                        const auto read = [&processor] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
                        const auto curvesBefore = std::array<float, 3> { read ("amp_curve"), read ("amp_dcurve"), read ("amp_rcurve") };
                        const auto from = amp->getHandlePosition (EnvelopeDisplay::firstCurveHandle);
                        const auto to = from + juce::Point<float> (0.0f, -12.0f);
                        clearHistory();
                        auto& component = static_cast<juce::Component&> (*amp);
                        component.mouseDown (event (component, from, false));
                        component.mouseDrag (event (component, to, true));
                        const auto readout = amp->getReadout();
                        const auto landed = amp->getHandlePosition (EnvelopeDisplay::firstCurveHandle);
                        component.mouseUp (event (component, to, true));
                        settle (50);
                        const auto changed = read ("amp_acurve");
                        const auto others = std::array<float, 3> { read ("amp_curve"), read ("amp_dcurve"), read ("amp_rcurve") };
                        const auto steps = undoSteps();
                        processor.getUndoManager().undo();
                        settle (50);
                        expect (plot.getBottom() + EnvelopeDisplay::rulerHeight <= (float) amp->getHeight() && plot.getY() > 0.0f,
                                "the envelope's time ruler is below its plot");
                        expect (changed > 0.05f && others == curvesBefore && readout.startsWith ("ATTACK CURVE")
                                    && landed.getDistanceFrom (to) < 3.0f && steps.size() == 1 && std::abs (read ("amp_acurve")) < 0.001f,
                                "the attack's curve dot bends the attack alone (" + juce::String (changed, 2) + ", '" + readout
                                    + "', the dot " + juce::String (landed.getDistanceFrom (to), 1) + " px from the mouse), one undo step");
                        // Impeccable re-score toward 35: a handle says its value on hover too.
                        const auto hoverText = amp->getReadoutFor (0);
                        expect (hoverText.startsWith ("ATTACK") && amp->getReadoutFor (1).contains ("SUSTAIN"),
                                "an envelope handle names its value for the hover readout ('" + hoverText + "')");
                    }
                    else
                    {
                        expect (false, "the amp envelope's pop-up shows its graph");
                    }
                    uiCloseSourcePopover (ctx);
                }

                // (PLAY's envelope carousel following the pool is covered in PlayPageUiTests.h.)
                {
                    pages->showPage ("MAIN");
                    settle (200);
                    root = pages->getCurrentPage();
                }

                // No routing to an LFO or envelope that isn't in its pool:
                // a knob's "Modulate with" lists the pool's (and offers the
                // next as "New LFO"), the matrix greys the rest out.
                {
                    KnobControl* knob = nullptr;
                    for (auto* candidate : visibleOf ((KnobControl*) nullptr))
                        if (knob == nullptr && candidate->getRingDestination() != 0)
                            knob = candidate;
                    auto hiddenLfo = -1;
                    for (int lfo = numLfos - 1; lfo >= 0; --lfo)
                        if (! processor.isLfoShown (lfo))
                            hiddenLfo = lfo;
                    auto hiddenEnv = -1;
                    for (int env = 15; env >= 0; --env)
                        if (! envelopeShown (processor, env))
                            hiddenEnv = env;
                    juce::StringArray offered;
                    if (knob != nullptr)
                    {
                        const auto menu = knob->buildModulateWithMenu();
                        for (juce::PopupMenu::MenuItemIterator item (menu, true); item.next();)
                            if (item.getItem().itemID != 0)
                                offered.add (item.getItem().text);
                    }
                    const auto names = Mod::getSourceNames();
                    auto onlyPool = knob != nullptr && hiddenLfo >= 0 && hiddenEnv >= 0;
                    for (int source = 1; source < names.size() && onlyPool; ++source)
                        if (offered.contains (names[source]) && ! modSourceInPatch (processor, (Mod::Source) source))
                            onlyPool = false;
                    onlyPool = onlyPool && offered.contains ("LFO 1") && offered.contains (juce::String::fromUTF8 ("LFO 1 \xc2\xb7 OUT 2"))
                               && offered.contains ("AMP ENV")
                               && ! offered.contains (names[(int) Mod::lfoSourceFor (hiddenLfo)])
                               && offered.contains ("New LFO  (LFO " + juce::String (hiddenLfo + 1) + ")");

                    MatrixRow row (processor, 0);
                    std::vector<bool> inPatch ((size_t) names.size(), true);
                    for (int source = 1; source < names.size(); ++source)
                        inPatch[(size_t) source] = modSourceInPatch (processor, (Mod::Source) source);
                    row.setSourcesInPatch (inPatch);
                    const auto lfoItem = (int) Mod::lfoSourceFor (hiddenLfo) + 1;
                    const auto greyed = hiddenLfo >= 0 && ! row.isSourceItemEnabled (lfoItem)
                                        && row.isSourceItemEnabled ((int) Mod::Source::Lfo1 + 1)
                                        && row.isSourceItemEnabled ((int) Mod::Source::ModWheel + 1);
                    expect (onlyPool && greyed, "knob menus offer only the pools' LFOs and envelopes (" + juce::String (offered.size())
                                                    + " items) and a New LFO; the matrix greys out LFO " + juce::String (hiddenLfo + 1));
                }
            }
        }

        // The envelope graph's playhead rides the curve for the playing note
        // and goes when it ends.
        {
            EnvelopeDisplay display (processor, "amp");
            display.setSize (400, 160);
            juce::AudioBuffer<float> buffer (2, 512);
            const auto run = [&processor, &buffer] (int blocks, bool noteOn, bool noteOff)
            {
                for (int block = 0; block < blocks; ++block)
                {
                    juce::MidiBuffer midi;
                    if (block == 0 && noteOn)
                        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                    if (block == 0 && noteOff)
                        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                    buffer.clear();
                    processor.processBlock (buffer, midi);
                }
            };
            run (40, true, false); // 0.4 s: past Neuro Wobble's 5 ms attack and 300 ms decay
            const auto held = display.playheadPoint();
            const auto stage = (int) processor.getEnvMonitorPosition (0);
            run (60, false, true); // past the 50 ms release
            const auto after = display.playheadPoint();
            expect (held.has_value() && stage == 4 && ! after.has_value(),
                    "the envelope's playhead sits on the sustain while a note is held (stage " + juce::String (stage)
                        + ") and goes once it has released");
        }

        // The dot follows the newest note, not the loudest voice, and a stage
        // shorter than a block is still crossed.
        {
            juce::AudioBuffer<float> buffer (2, 512);
            const auto run = [&processor, &buffer] (int blocks, int note, bool on)
            {
                for (int block = 0; block < blocks; ++block)
                {
                    juce::MidiBuffer midi;
                    if (block == 0 && note >= 0)
                        midi.addEvent (on ? juce::MidiMessage::noteOn (1, note, 0.8f) : juce::MidiMessage::noteOff (1, note), 0);
                    buffer.clear();
                    processor.processBlock (buffer, midi);
                }
            };
            run (40, 60, true);
            const auto held = processor.getEnvMonitorPosition (0);
            const auto serial = processor.getNoteSerial();
            if (auto* parameter = processor.apvts.getParameter ("amp_attack"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.3f));
            run (2, 64, true); // a second note, slowly rising
            const auto second = processor.getEnvMonitorPosition (0);
            expect ((int) held == 4 && (int) second == 1 && processor.getNoteSerial() == serial + 1,
                    "the playhead follows the newest note (held " + juce::String (held) + ", then " + juce::String (second)
                        + ") and the note count goes up");
            run (1, 60, false);
            run (1, 64, false);
            run (80, -1, false);

            IlanaAnim::EnvTracker tracker;
            const std::array<float, 5> seconds { 0.0f, 0.002f, 0.0f, 0.3f, 0.05f };
            tracker.get (4.0f, seconds, 0);
            const auto atNote = tracker.get (3.2f, seconds, 1); // the block already past a 2 ms attack
            juce::Thread::sleep (20);
            const auto later = tracker.get (3.3f, seconds, 1);
            expect ((int) atNote == 1 && (int) later == 3,
                    "a new note starts the dot at the attack and runs it on (" + juce::String (atNote) + ", then " + juce::String (later) + ")");
        }

        // The dot rides the drawn line, curved or not, through a whole note,
        // an early key up and a retrigger; it leaves a trail while it moves
        // and no repaints are asked for once it is gone.
        for (const auto curveValue : { 0.0f, 0.8f, -0.8f })
        {
            IlanaSynthAudioProcessor local;
            local.prepareToPlay (48000.0, 512);
            const auto setLocal = [&local] (const juce::String& id, float plain)
            {
                if (auto* parameter = local.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
            };
            setLocal ("amp_attack", 0.15f);
            setLocal ("amp_decay", 0.3f);
            setLocal ("amp_sustain", 0.5f);
            setLocal ("amp_release", 0.4f);
            setLocal ("amp_curve", curveValue);
            EnvelopeDisplay display (local, "amp");
            display.setSize (400, 160);
            juce::AudioBuffer<float> buffer (2, 512);
            auto worst = 0.0f;
            auto seen = 0;
            const auto frame = [&] (int note, bool on)
            {
                for (int block = 0; block < 3; ++block)
                {
                    juce::MidiBuffer midi;
                    if (block == 0 && note >= 0)
                        midi.addEvent (on ? juce::MidiMessage::noteOn (1, note, 0.8f) : juce::MidiMessage::noteOff (1, note), 0);
                    buffer.clear();
                    local.processBlock (buffer, midi);
                }
                juce::Thread::sleep (20);
                display.createComponentSnapshot (display.getLocalBounds()); // paints: the trail follows the dot
                const auto off = display.playheadOffLine();
                if (off >= 0.0f)
                {
                    worst = juce::jmax (worst, off);
                    ++seen;
                }
            };
            frame (60, true);
            for (int i = 0; i < 25; ++i) frame (-1, false);   // attack, decay, sustain
            frame (60, false);
            for (int i = 0; i < 8; ++i) frame (-1, false);    // part of the release
            frame (64, true);                                  // a new note, on the way down
            for (int i = 0; i < 3; ++i) frame (-1, false);
            frame (64, false);                                 // let go in the attack
            for (int i = 0; i < 30; ++i) frame (-1, false);
            expect (seen > 20 && worst < 1.0f,
                    "the envelope dot rides the drawn line at curve " + juce::String (curveValue, 1) + " (worst " + juce::String (worst, 2)
                        + " px over " + juce::String (seen) + " frames)");
        }

        {
            IlanaSynthAudioProcessor local;
            local.prepareToPlay (48000.0, 512);
            EnvelopeDisplay display (local, "amp");
            display.setSize (400, 160);
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            local.processBlock (buffer, midi);
            display.createComponentSnapshot (display.getLocalBounds());
            const auto lit = display.trailIsLit();
            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            local.processBlock (buffer, off);
            for (int i = 0; i < 400; ++i)
            {
                buffer.clear();
                juce::MidiBuffer none;
                local.processBlock (buffer, none);
            }
            for (int i = 0; i < 60; ++i) // the display's timer, 30 frames a second
            {
                juce::Thread::sleep (30);
                display.createComponentSnapshot (display.getLocalBounds());
            }
            expect (lit && ! display.trailIsLit(), "the envelope's trail lights with a note and has died away (no repaints asked) once it is over");
        }
    }
}

void uiGroup_REVIEW4E (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 4, batch E: oscillators, filter, PLAY.
    {
        const auto setParam = [&processor] (const juce::String& id, float plain)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
        };
        const auto readParam = [&processor] (const juce::String& id)
        {
            const auto* value = processor.apvts.getRawParameterValue (id);
            return value != nullptr ? value->load() : 0.0f;
        };
        const auto findKnob = [&editor] (const juce::String& id) -> KnobControl*
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
                if (knob->getParameterId() == id && visibleInTree (knob) && knob->getWidth() > 0)
                    return knob;
            return nullptr;
        };
        const auto loadNamed = [&processor] (const juce::String& name)
        {
            processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (name));
            settle (500);
        };
        const juce::StringArray presets { "Init", "Neuro Wobble", "E.PIANO 1 (ROM1A)" };

        // No card is cut mid-knob: every visible knob on PLAY and OSC is
        // wholly inside each of its ancestors (the scrolling viewports
        // included) or wholly out of view, at 100 % and 75 % (V13, S8).
        const auto clippedKnobs = [&editor] (juce::StringArray& names)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
            {
                if (! visibleInTree (knob) || knob->getWidth() <= 0 || knob->getHeight() <= 0)
                    continue;
                // The dial and its value: the label above may tuck under a header.
                const auto area = editor->getLocalArea (knob, knob->getLocalBounds().withTrimmedTop (13));
                // A knob scrolled wholly out of its view is hidden, not cut
                // (the scrolled column may still reach past the page).
                auto hidden = false;
                for (auto* parent = knob->getParentComponent(); parent != nullptr && parent != editor.get(); parent = parent->getParentComponent())
                    hidden = hidden || ! editor->getLocalArea (parent, parent->getLocalBounds()).intersects (area);
                if (hidden)
                    continue;
                for (auto* parent = knob->getParentComponent(); parent != nullptr && parent != editor.get(); parent = parent->getParentComponent())
                {
                    // (A scrolling column, PLAY's operators past the third, is cut by its viewport on purpose.)
                    if (dynamic_cast<juce::Viewport*> (parent) != nullptr || dynamic_cast<juce::Viewport*> (parent->getParentComponent()) != nullptr)
                        break;
                    const auto view = editor->getLocalArea (parent, parent->getLocalBounds());
                    if (view.intersects (area) && ! view.contains (area))
                    {
                        names.add (knob->getParameterId());
                        break;
                    }
                }
            }
        };

        for (const auto width : { 1060, 795 })
        {
            editor->setSize (width, width * 720 / 1060);
            settle (300);
            for (const auto& preset : presets)
            {
                loadNamed (preset);
                for (const auto* page : { "MAIN", "OSC" })
                {
                    pages->showPage (page);
                    settle (400);
                    juce::StringArray clipped;
                    clippedKnobs (clipped);
                    expect (clipped.isEmpty() && processor.getFactoryPresetNames().contains (preset),
                            juce::String (page) + " at " + juce::String (width) + " px, " + preset + ": no knob is cut by its card or the page"
                                + (clipped.isEmpty() ? juce::String() : " (cut: " + clipped.joinIntoString (", ") + ")"));
                }
            }
        }
        editor->setSize (1060, 720);
        settle (300);

        // Knob names and modulation rings are never cut (ilana's Windows
        // screenshots, design sweep): on every page, at 75 %, 100 % and
        // 150 %, each visible knob's name and the box its rings draw in sit
        // inside the knob and inside every view above it, and the rings stay
        // under the name. Neuro Wobble's FRAME has three rings.
        const auto cutKnobText = [&editor] (juce::StringArray& names)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
            {
                if (! visibleInTree (knob) || knob->getWidth() <= 0 || knob->getHeight() <= 0)
                    continue;
                const auto own = knob->getLocalBounds().toFloat().expanded (0.5f);
                const auto name = knob->getNameTextBounds();
                const auto rings = knob->getRingsBounds();
                const auto whole = editor->getLocalArea (knob, knob->getLocalBounds());
                // A knob scrolled out of view, wholly or in part, is hidden, not cut (the scroll
                // decides that, not the layout: the OSC tab's one-row strip can sit at a viewport's edge).
                auto hidden = false;
                for (auto* parent = knob->getParentComponent(); parent != nullptr && parent != editor.get(); parent = parent->getParentComponent())
                    hidden = hidden || ! editor->getLocalArea (parent, parent->getLocalBounds()).intersects (whole)
                             || (dynamic_cast<juce::Viewport*> (parent) != nullptr && ! editor->getLocalArea (parent, parent->getLocalBounds()).contains (whole));
                if (hidden)
                    continue;
                juce::StringArray why;
                if (! name.isEmpty() && ! own.contains (name))
                    why.add ("name outside the knob");
                if (! rings.isEmpty() && ! own.contains (rings))
                    why.add ("rings outside the knob");
                // (The rings may pass under the name's line, never into its letters.)
                const auto ink = knob->getNameInkBounds();
                if (! ink.isEmpty() && ! rings.isEmpty() && rings.getY() < ink.getBottom() + 0.5f
                    && rings.getX() < ink.getRight() && rings.getRight() > ink.getX())
                    why.add ("rings reach the name");
                const std::pair<const char*, juce::Rectangle<float>> parts[] { { "name", name }, { "rings", rings } };
                for (const auto& [what, part] : parts)
                {
                    if (part.isEmpty())
                        continue;
                    const auto area = editor->getLocalArea (knob, part.getSmallestIntegerContainer().reduced (1));
                    for (auto* parent = knob->getParentComponent(); parent != nullptr && parent != editor.get(); parent = parent->getParentComponent())
                        if (! editor->getLocalArea (parent, parent->getLocalBounds()).contains (area))
                        {
                            why.add (juce::String (what) + " cut by " + parent->getName() + " " + juce::String (typeid (*parent).name()));
                            break;
                        }
                }
                if (! why.isEmpty())
                    names.add (knob->getParameterId() + " (" + why.joinIntoString ("; ") + "; knob " + knob->getLocalBounds().toString()
                               + ", name " + name.toString() + ", rings " + rings.toString() + ")");
            }
        };

        loadNamed ("Neuro Wobble");
        for (const auto width : { 795, 1060, 1590 })
        {
            editor->setSize (width, width * 720 / 1060);
            settle (300);
            for (const auto& page : pages->getPageIds())
            {
                pages->showPage (page);
                settle (300);
                juce::StringArray cut;
                cutKnobText (cut);
                expect (cut.isEmpty(), page + " at " + juce::String (width) + " px: no knob's name or rings are cut"
                                           + (cut.isEmpty() ? juce::String() : " (" + cut.joinIntoString (", ") + ")"));
            }
        }
        editor->setSize (1060, 720);
        settle (300);

        // The DX7 voice's six operators: six strips on PLAY, each showing
        // its RATIO (it is an operator: UI review 6, V3, I6-5), nothing
        // cut; on OSC the OSC 4 tab shows OSC 4 alone.
        loadNamed ("E.PIANO 1 (ROM1A)");
        {
            pages->showPage ("MAIN");
            settle (400);
            juce::StringArray clipped;
            clippedKnobs (clipped);
            auto ratios = 0;
            for (const auto* prefix : OscillatorIds::prefixes)
                ratios += findKnob (juce::String (prefix) + "_ratio") != nullptr ? 1 : 0;
            expect (ratios == OscillatorIds::count && clipped.isEmpty(),
                    "MAIN: the DX7 voice's six operators each show their RATIO (" + juce::String (ratios) + "), nothing cut ("
                        + clipped.joinIntoString (", ") + ")");

            // (PLAY's envelope carousel on a DX7 voice is covered in PlayPageUiTests.h.)

            pages->showPage ("MAIN");
            settle (400);
            selectOscTab (3);
            clipped.clear();
            clippedKnobs (clipped);
            expect (findKnob ("osc4_ratio") != nullptr && findKnob ("osc1_level") == nullptr && clipped.isEmpty(),
                    "OSC: the OSC 4 tab shows OSC 4's RATIO alone, nothing cut (" + clipped.joinIntoString (", ") + ")");
            selectOscTab (0);
        }

        // No remove button on an oscillator's title (S16): it is on the
        // title's right-click menu.
        loadNamed ("Neuro Wobble");
        for (const auto* page : { "MAIN", "OSC" })
        {
            pages->showPage (page);
            settle (300);
            std::vector<juce::Button*> buttons;
            findAll<juce::Button> (*editor, buttons);
            auto crosses = 0;
            for (auto* button : buttons)
                crosses += visibleInTree (button) && (button->getButtonText() == juce::String (juce::CharPointer_UTF8 ("\xc3\x97"))
                                                      || button->getButtonText() == "x") ? 1 : 0;
            expect (crosses == 0, juce::String (page) + ": no remove button on the oscillator titles");

            // One way to add an oscillator (S33); on OSC one oscillator at a
            // time, behind its tab (V19).
            auto adds = 0;
            for (auto* button : buttons)
                adds += visibleInTree (button) && button->getButtonText().contains ("ADD OSC") ? 1 : 0;
            auto switches = 0;
            for (const auto* prefix : OscillatorIds::prefixes)
                switches += toggleFor (juce::String (prefix) + "_on") != nullptr ? 1 : 0;
            auto shown = 0;
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                shown += processor.isOscillatorShown (osc) ? 1 : 0;
            // (The approved OSC design stacks every shown oscillator as a card,
            // each with its own switch, so OSC counts like PLAY now.)
            expect ((juce::String (page) == "MAIN" ? adds <= 1 : adds == 1) && switches == shown,
                    juce::String (page) + ": one ADD OSC button (" + juce::String (adds) + "), "
                        + "a switch per oscillator" + " ("
                        + juce::String (switches) + ")");
        }

        // PLAY: one FILTER card, one graph drawing both filters (one-tab PLAY, 2026-10-25).
        loadNamed ("Init");
        pages->showPage ("MAIN");
        settle (300);
        {
            std::vector<FilterDisplay*> displays;
            findAll<FilterDisplay> (*editor, displays);
            auto shownGraphs = 0;
            for (auto* display : displays)
                shownGraphs += visibleInTree (display) ? 1 : 0;
            expect (shownGraphs == 1, "PLAY shows one filter graph for both filters (" + juce::String (shownGraphs) + ")");
        }

        // PLAY's filter TYPE reads like FILTER's (its short names), SLOPE is
        // FILTER's 12 / 24 dB switch (S10).
        {
            std::vector<ComboControl*> combos;
            findAll<ComboControl> (*editor, combos);
            auto typeText = juce::String();
            for (auto* combo : combos)
                if (visibleInTree (combo) && combo->getComboBox().getNumItems() == FilterTypeGrid::shortNames().size()
                    && combo->getComboBox().getItemText (0) == FilterTypeGrid::shortNames()[0])
                    typeText = combo->getComboBox().getText();
            expect (typeText.isNotEmpty() && typeText == FilterTypeGrid::shortNames()[juce::roundToInt (readParam ("f1_type"))],
                    "PLAY's filter TYPE shows FILTER's name ('" + typeText + "')");
        }

        // The WAVE view: the frame as a readout and scrubber (V12).
        pages->showPage ("MAIN");
        settle (300);
        {
            std::vector<WaveDisplay*> waves;
            findAll<WaveDisplay> (*editor, waves);
            auto readout = juce::String();
            for (auto* wave : waves)
                if (visibleInTree (wave) && readout.isEmpty())
                    readout = wave->getFrameReadout();
            expect (readout.startsWith ("frame ") && readout.contains (" of "), "the WAVE view reads out the frame ('" + readout + "')");
        }

        // One dimming rule (V26): a control that does nothing now dims, says
        // why on hover, and still takes edits.
        {
            auto* spectralOff = findKnob ("osc1_spectral_amt");
            const auto dimmed = spectralOff != nullptr && spectralOff->getAlpha() < 0.99f; // dim while Off (S13-2)
            setParam ("osc1_spectral", 1.0f);
            settle (500);
            auto* spectral = findKnob ("osc1_spectral_amt");
            const auto lit = spectral != nullptr && spectral->getAlpha() > 0.99f && ! spectral->getSlider().getTooltip().contains ("No effect now");
            setParam ("osc1_spectral", 0.0f);
            settle (300);
            auto* detune = findKnob ("osc1_detune");
            expect (dimmed && lit && detune != nullptr && detune->getAlpha() < 0.99f,
                    "OSC: SPEC AMT is dim while SPECTRAL is Off, lights when it is on; DETUNE dims at UNISON 1");

            pages->showPage ("MAIN");
            settle (400);
            const auto serialShown = uiBalanceSliderShown (*editor);
            setParam ("filters_parallel", 1.0f);
            settle (400);
            expect (! serialShown && uiBalanceSliderShown (*editor), "FILTER: BALANCE shows while the filters run in parallel, not in serial");
            setParam ("filters_parallel", 0.0f);
            settle (200);
        }

        // Three knob sizes by role (V22): FM sends are mini, OSC's core
        // knobs as large as PLAY's.
        {
            pages->showPage ("FM");
            settle (400);
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            // (2026-10-29: the sends are the ROUTES list's and the cables' small rings; a route makes them show.)
            processor.apvts.getParameter ("fm_amount")->setValueNotifyingHost (0.5f);
            settle (400);
            knobs.clear();
            findAll<KnobControl> (*editor, knobs);
            auto sends = 0, miniSends = 0;
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getParameterId().startsWith ("fm_") && knob->getLabelText().isEmpty())
                {
                    ++sends;
                    miniSends += knob->getWidth() <= 60 ? 1 : 0;
                }
            expect (sends > 0 && sends == miniSends, "FM: the route knobs are small (" + juce::String (miniSends) + " of " + juce::String (sends) + ")");
            processor.apvts.getParameter ("fm_amount")->setValueNotifyingHost (0.0f);

            pages->showPage ("MAIN");
            settle (300);
            auto* playLevel = findKnob ("osc1_level");
            const auto playDial = playLevel != nullptr ? playLevel->getDialSize() : 0;
            pages->showPage ("MAIN");
            settle (300);
            auto* oscLevel = findKnob ("osc1_level");
            // (The approved OSC design draws every oscillator as a compact card with
            // a 26 px inline dial, so OSC's LEVEL is deliberately smaller than PLAY's.
            // The check is now that it exists and is still a usable size.)
            expect (playDial > 0 && oscLevel != nullptr && oscLevel->getDialSize() >= 24,
                    "OSC's LEVEL dial (" + juce::String (oscLevel != nullptr ? oscLevel->getDialSize() : 0) + " px) is a usable inline dial (PLAY's is "
                        + juce::String (playDial) + " px)");
        }

        // The physical card: LOAD .WAV only for a wavetable (V30).
        {
            setParam ("osc1_mode", 1.0f);
            settle (500);
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*editor, buttons);
            auto loadShown = false;
            // (Every oscillator is a card on OSC now: look inside OSC 1's.)
            for (auto* button : buttons)
                if (visibleInTree (button) && button->getButtonText().startsWith ("LOAD")
                    && pages->getOscCardBounds (0).contains (editor->getLocalArea (button, button->getLocalBounds()).getCentre()))
                    loadShown = true;
            expect (! loadShown && findKnob ("osc1_string_decay") != nullptr && findKnob ("osc1_unison") != nullptr,
                    "a Physical card has no LOAD .WAV, and shows its string and voice rows (load "
                        + juce::String ((int) loadShown) + ", decay " + juce::String ((int) (findKnob ("osc1_string_decay") != nullptr))
                        + ", unison " + juce::String ((int) (findKnob ("osc1_unison") != nullptr)) + ")");
            setParam ("osc1_mode", 0.0f);
            settle (300);
        }

        // VECTOR: a corner whose oscillator is off says so (S27).
        {
            pages->showPage ("VECTOR");
            settle (300);
            auto* pad = findChild<VectorPadDisplay> (*editor);
            auto offCorner = -1;
            for (int corner = 0; corner < 4 && pad != nullptr; ++corner)
                if (! pad->isCornerSounding (corner))
                    offCorner = corner;
            expect (pad != nullptr && offCorner >= 0 && (pad->getCornerLabel (offCorner, 0.5f).endsWith (": off") || pad->getCornerLabel (offCorner, 0.5f).endsWith (": none"))
                        && pad->isCornerSounding (0) && ! pad->getCornerLabel (0, 0.5f).contains (":"),
                    "VECTOR: Init's corners on switched-off oscillators say so, OSC 1's does not");

            // An oscillator the patch doesn't have reads "(none)"; no share
            // shows while the vector is off (UI review 6, V29).
            auto none = false;
            for (int corner = 0; corner < 4 && pad != nullptr; ++corner)
                if (! processor.isOscillatorShown (processor.getVectorCorner (corner)))
                    none = pad->getCornerLabel (corner, 0.5f).endsWith (": none");
            auto* vecOn = processor.apvts.getParameter ("vec_on");
            const auto wasOn = vecOn->getValue();
            vecOn->setValueNotifyingHost (0.0f);
            settle (100);
            expect (none && pad != nullptr && ! pad->getCornerLabel (0, 0.5f).contains ("%"),
                    "VECTOR: a corner without its oscillator reads ': none'; no shares while the vector is off");
            vecOn->setValueNotifyingHost (wasOn);
        }

        // The synced RATE knob takes modulation for RATE (V14).
        {
            pages->showPage ("MAIN");
            settle (300);
            setParam ("lfo1_sync", 1.0f);
            settle (400);
            auto* division = findKnob ("lfo1_div");
            const juce::DragAndDropTarget::SourceDetails drag (juce::var ("modsource:0"), nullptr, {});
            expect (division != nullptr && division->getRingDestination() == (int) Mod::Destination::Lfo1Rate
                        && division->isInterestedInDragSource (drag),
                    "a synced LFO's RATE knob shows RATE's modulation and takes a dropped source");
            setParam ("lfo1_sync", 0.0f);
            settle (200);
        }
        loadNamed ("Neuro Wobble");
    }
}

void uiGroup_MOD (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 6, P1: rings, badges, the macro strip and the matrix.
    runModulationTests (processor, *pages);
}

void uiGroup_SMALL (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Batch H (UI review 4: V19, V27, V28, S17, S20, S23, S25).
    runSmallThingsTests (processor, *pages);
}

void uiGroup_T2 (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 9, T2: voice, SEQ, matrix, browser, macros.
    runReview9T2Tests (processor, *pages);
}

void uiGroup_PLAYOSC7 (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 7, Q4: PLAY, OSC, PHYSICAL, VECTOR.
    runPlayOscReview7Tests (processor, *pages);
}

void uiGroup_FF (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 7, FILTER and FX.
    runFilterFxTests (processor, *pages);
}

void uiGroup_MOD8 (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 8, R4: modulation.
    runModulationReview8Tests (processor, *pages);
}

void uiGroup_L8 (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 8, R5: PLAY / OSC / PHYSICAL / VECTOR / FILTER / FX layout.
    runLayoutReview8Tests (processor, *pages);
}

void uiGroup_T1 (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    runLayoutReview9Tests (processor, *pages);
}

void uiGroup_U1 (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    runLayoutReview10Tests (processor, *pages);
}

void uiGroup_OV (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    runOscCardOverlapTests (processor, *pages);
}

void uiGroup_PLAY (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    runPlayPageTests (processor, *pages);
}

void uiGroup_VECTOR_FIT (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 29: switching VECTOR on fits the pad's shape to the patch (one corner per oscillator), once.
    {
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf ("Neuro Wobble")); // (three oscillators)
        pages->showPage ("VECTOR");
        settle (150);
        ToggleControl* onSwitch = nullptr;
        std::function<void (juce::Component&)> find = [&] (juce::Component& parent)
        {
            for (auto* child : parent.getChildren())
            {
                if (auto* toggle = dynamic_cast<ToggleControl*> (child); toggle != nullptr && toggle->getParameterId() == "vec_on" && visibleInTree (toggle))
                    onSwitch = toggle;
                find (*child);
            }
        };
        find (*pages);
        expect (onSwitch != nullptr, "VECTOR page: the ON switch is found");
        if (onSwitch != nullptr)
        {
            onSwitch->getButton().triggerClick();
            settle (150);
            const auto shown = [&processor] { auto n = 0; for (int osc = 0; osc < OscillatorIds::count; ++osc) n += processor.isOscillatorShown (osc) ? 1 : 0; return n; }();
            expect (shown == 3 && processor.getVectorShape() == VectorShape::triangle && processor.getVectorCorner (2) == 2,
                    "VECTOR on, three oscillators: the pad becomes a triangle on OSC 1-3 (shape " + juce::String (processor.getVectorShape()) + ")");
            // Off and on again keeps what the pad has (a corner picked by hand stays).
            if (auto* corner = processor.apvts.getParameter ("vec_c"))
                corner->setValueNotifyingHost (corner->convertTo0to1 (0.0f));
            onSwitch->getButton().triggerClick();
            onSwitch->getButton().triggerClick();
            settle (150);
            expect (processor.getVectorShape() == VectorShape::triangle && processor.getVectorCorner (2) == 0,
                    "VECTOR off and on again keeps the shape and the corners");
        }
        // The other counts: 2 gives the slider, 5 a pentagon, 6 a hexagon (corners in oscillator order); 4 keeps the square.
        if (onSwitch != nullptr)
        {
            const auto setParamValue = [&processor] (const juce::String& id, float value)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            const std::pair<int, int> cases[] { { 2, VectorShape::slider }, { 5, VectorShape::pentagon }, { 6, VectorShape::hexagon }, { 4, VectorShape::square } };
            for (const auto& [count, shape] : cases)
            {
                for (int osc = 0; osc < OscillatorIds::count; ++osc)
                {
                    setParamValue (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_on", osc < count ? 1.0f : 0.0f);
                    processor.setRevealed (IlanaSynthAudioProcessor::Module::Oscillator, osc, false);
                }
                setParamValue ("vec_on", 0.0f);
                setParamValue ("vec_shape", (float) VectorShape::square);
                for (int corner = 0; corner < 4; ++corner)
                    setParamValue (juce::String ("vec_") + juce::String::charToString ((juce::juce_wchar) ('a' + corner)), (float) corner);
                settle (100);
                int shown = 0;
                for (int osc = 0; osc < OscillatorIds::count; ++osc)
                    shown += processor.isOscillatorShown (osc) ? 1 : 0;
                onSwitch->getButton().triggerClick();
                settle (150);
                auto cornersInOrder = true;
                for (int corner = 0; corner < juce::jmin (count, processor.getVectorCornerCount()); ++corner)
                    cornersInOrder = cornersInOrder && processor.getVectorCorner (corner) == corner;
                expect (shown == count && processor.getVectorShape() == shape && cornersInOrder,
                        "VECTOR on with " + juce::String (count) + " oscillators fits shape " + juce::String (shape) + " (got " + juce::String (processor.getVectorShape()) + ", " + juce::String (shown) + " shown)");
            }
        }
        processor.loadFactoryPreset (0);
        settle (100);
    }
}

void uiGroup_PLAY_LFO_DRAG (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 29, L5: PLAY's LFO graph takes a drag, as the pop-up's does (a preset wave becomes a curve).
    {
        processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, 0, true);
        if (auto* lfoShape = processor.apvts.getParameter ("lfo1_shape"))
            lfoShape->setValueNotifyingHost (lfoShape->convertTo0to1 (0.0f));
        pages->showPage ("MAIN");
        settle (300);
        LfoDisplay* graph = nullptr;
        std::function<void (juce::Component&, bool)> find = [&] (juce::Component& parent, bool inCard)
        {
            for (auto* child : parent.getChildren())
            {
                const auto card = inCard || child->getName() == "PLAY LFO card";
                if (auto* display = dynamic_cast<LfoDisplay*> (child); display != nullptr && card && visibleInTree (display))
                    graph = display;
                find (*child, card);
            }
        };
        find (*pages, false);
        expect (graph != nullptr, "PLAY: the LFO card shows its graph");
        if (graph != nullptr)
        {
            auto& component = static_cast<juce::Component&> (*graph);
            const auto area = component.getLocalBounds().toFloat();
            const auto event = [&component] (juce::Point<float> at, juce::Point<float> from)
            {
                return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                         1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(),
                                         from, juce::Time::getCurrentTime(), 1, false);
            };
            const juce::Point<float> down (area.getX() + 8.0f, area.getCentreY()), to (area.getX() + 8.0f, area.getY() + 10.0f);
            component.mouseDown (event (down, down));
            component.mouseDrag (event (down.translated (0.0f, -6.0f), down));
            component.mouseDrag (event (to, down));
            component.mouseUp (event (to, down));
            settle (100);
            expect ((int) std::round (processor.apvts.getRawParameterValue ("lfo1_shape")->load()) == IlanaSynthAudioProcessor::curveShape,
                    "PLAY: a drag on the LFO graph edits the shape (it becomes a curve)");
        }
        processor.loadFactoryPreset (0);
        settle (100);
    }
}

void uiGroup_ECHO_SLIDER (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Knobs jumped back a frame mid-drag (2026-11-12): a VST3 host (Live)
    // echoes each edit back through the audio thread a block late; the slider
    // drew that older value. The knob's attachment now ignores the parameter
    // while the user moves it, then catches up with the parameter's current value.
    {
        auto* parameter = processor.apvts.getParameter ("f1_cutoff");
        const auto fromAudioThread = [parameter] (float normalised)
        {
            std::thread host ([parameter, normalised] { parameter->setValueNotifyingHost (normalised); });
            host.join();
        };
        const auto original = parameter->getValue();
        const auto plain = [parameter] (float normalised) { return (double) parameter->convertFrom0to1 (normalised); };

        // juce's own attachment: the stale echo moves the slider back (the bug, so the check below means something).
        {
            juce::Slider slider;
            juce::AudioProcessorValueTreeState::SliderAttachment plainAttachment (processor.apvts, "f1_cutoff", slider);
            slider.setValue (plain (0.6f), juce::sendNotificationSync);
            fromAudioThread (0.5f);
            settle (30);
            expect (std::abs (slider.getValue() - plain (0.5f)) < 1.0e-3,
                    "knob echo: juce's SliderAttachment draws the host's stale echo (the old behaviour)");
        }
        settle ((int) HeldSliderAttachment::holdMs + 50);
        {
            juce::Slider slider;
            HeldSliderAttachment attachment (processor.apvts, "f1_cutoff", slider);
            slider.setValue (plain (0.6f), juce::sendNotificationSync);
            fromAudioThread (0.5f); // the host's echo of an older drag step
            settle (30);
            expect (std::abs (slider.getValue() - plain (0.6f)) < 1.0e-3, "knob echo: a stale echo mid-drag doesn't move the knob back");
            slider.setValue (plain (0.65f), juce::sendNotificationSync); // the next drag step
            expect (std::abs (parameter->getValue() - 0.65f) < 1.0e-3f, "knob echo: the next drag step sets the parameter");
            fromAudioThread (0.7f); // the parameter moves for real (automation) after the drag
            settle ((int) HeldSliderAttachment::holdMs + 150);
            expect (std::abs (slider.getValue() - plain (0.7f)) < 1.0e-3, "knob echo: after the hold the knob shows the parameter's current value");
            fromAudioThread (0.3f); // not held: follows at once
            settle (30);
            expect (std::abs (slider.getValue() - plain (0.3f)) < 1.0e-3, "knob echo: a knob nobody touches follows the parameter at once");
        }
        parameter->setValueNotifyingHost (original);
        settle (30);
    }
}

void uiGroup_ECHO_GRAPH (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // The echo also stepped the parameter itself back (2026-11-13, ilana's
    // recording: "stuttery when moving the knob"): the filter graph and the
    // sound read the parameter, not the knob. ParameterEchoGuard puts the
    // user's latest value back when an echo of an earlier drag step arrives.
    {
        settle ((int) ParameterEchoGuard::holdMs + 50);
        auto* parameter = processor.apvts.getParameter ("f1_cutoff");
        const auto* raw = processor.apvts.getRawParameterValue ("f1_cutoff");
        const auto fromAudioThread = [parameter] (float normalised)
        {
            std::thread host ([parameter, normalised] { parameter->setValueNotifyingHost (normalised); });
            host.join();
        };
        const auto original = parameter->getValue();
        const auto blockedBefore = processor.echoGuard.getEchoesBlocked();

        parameter->beginChangeGesture();
        for (const auto step : { 0.50f, 0.52f, 0.54f, 0.56f })
            parameter->setValueNotifyingHost (step); // drag steps, from the editor
        fromAudioThread (0.52f); // the host's echo of an older step
        expect (std::abs (parameter->getValue() - 0.56f) < 1.0e-4f, "parameter echo: an echo of an earlier drag step doesn't move the parameter back");
        expect (std::abs (raw->load() - parameter->convertFrom0to1 (0.56f)) < 1.0e-2f, "parameter echo: the value the graph reads stays on the drag");
        expect (processor.echoGuard.getEchoesBlocked() == blockedBefore + 1, "parameter echo: the echo was counted");
        fromAudioThread (0.56f); // the echo of the latest step changes nothing
        expect (std::abs (parameter->getValue() - 0.56f) < 1.0e-4f, "parameter echo: the latest step's echo is harmless");
        fromAudioThread (0.61f); // a value the user never set (automation) still lands
        expect (std::abs (parameter->getValue() - 0.61f) < 1.0e-4f, "parameter echo: a new value from the host still lands mid-drag");
        parameter->setValueNotifyingHost (0.58f);
        parameter->endChangeGesture();
        fromAudioThread (0.54f); // late echo just after letting go
        expect (std::abs (parameter->getValue() - 0.58f) < 1.0e-4f, "parameter echo: a late echo just after letting go is blocked too");
        settle ((int) ParameterEchoGuard::holdMs + 100);
        fromAudioThread (0.54f); // after the hold: automation back to an old value lands
        expect (std::abs (parameter->getValue() - 0.54f) < 1.0e-4f, "parameter echo: once the user let go, any value from the host lands");

        parameter->setValueNotifyingHost (original);
        settle ((int) ParameterEchoGuard::holdMs + 50);
    }
}

void uiGroup_HELD_GRAPH (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // The filter graph still stuttered (2026-11-14, ilana: "still happening but
    // only in the illustration", playing or stopped): it drew the parameter,
    // which an echo can still step back. It now draws a held knob's value (as
    // Vital's graph reads its sliders), and its own drag's values.
    {
        settle ((int) HeldSliderAttachment::holdMs + 50);
        auto* parameter = processor.apvts.getParameter ("f1_cutoff");
        const auto original = parameter->getValue();
        const auto plain = [parameter] (float normalised) { return (double) parameter->convertFrom0to1 (normalised); };
        FilterDisplay graph (processor);
        graph.setSize (500, 200);
        {
            juce::Slider slider;
            HeldSliderAttachment attachment (processor.apvts, "f1_cutoff", slider);
            slider.setValue (plain (0.60f), juce::sendNotificationSync);
            const auto stepOneX = graph.getMarkerCentres()[0].x;
            slider.setValue (plain (0.65f), juce::sendNotificationSync);
            const auto stepTwoX = graph.getMarkerCentres()[0].x;
            parameter->setValueNotifyingHost (0.60f); // a late echo the guard lets through (message thread)
            expect (std::abs (graph.getMarkerCentres()[0].x - stepTwoX) < 0.01f && stepTwoX > stepOneX + 1.0f,
                    "graph echo: while the knob is held the graph draws the knob, not an echo on the parameter");
            settle ((int) HeldSliderAttachment::holdMs + 100);
            expect (std::abs (graph.getMarkerCentres()[0].x - stepOneX) < 0.01f,
                    "graph echo: once the knob is let go the graph draws the parameter again");
        }
        parameter->setValueNotifyingHost (original);
        settle (30);
    }
}

void uiGroup_MODAL_SNAP (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // The modal SIZE / SCALE knobs snap to real instruments (2026-11-12, ilana: "snap to the real instrument
    // sizes ... holding alt while draggin"): a drag that comes within a few pixels of a ring mark lands on its
    // exact value; Alt held passes it.
    {
        expect (ModalMarks::markAt (13, false, 1.17f) != nullptr && ModalMarks::markAt (13, false, 2.1f) != nullptr
                    && ModalMarks::markAt (13, false, 1.0f) != nullptr,
                "snap marks: the bowed SIZE marks hold the engine's violin / viola / cello (the presets' 1, 1.17, 2.1)");
        expect (ModalMarks::name (13, false, 1.17f) == "VIOLA" && ModalMarks::name (12, false, 1.0f) == "CLASSICAL"
                    && ModalMarks::name (12, true, 0.51f) == "SOPRANO UKE" && ModalMarks::name (11, false, 1.0f) == "CONCERT GRAND",
                "snap marks: each engine names its instruments");
        expect (ModalMarks::marks (3, false).empty() && ModalMarks::ringMarks (3, true).isEmpty(), "snap marks: other exciters have none");

        KnobSlider knob;
        knob.setBounds (0, 0, 60, 60);
        knob.setNormalisableRange ({ 0.25, 4.0, 0.0, 0.4307 });
        knob.setValue (1.0, juce::dontSendNotification);
        knob.getProperties().set ("ringMarks", ModalMarks::ringMarks (13, false));
        const auto drag = [&knob] (float pixelsUp, juce::ModifierKeys mods)
        {
            const auto at = [&knob, mods] (float y)
            {
                return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 30.0f, y }, mods.withFlags (juce::ModifierKeys::leftButtonModifier),
                                         1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &knob, &knob, juce::Time::getCurrentTime(), { 30.0f, 30.0f },
                                         juce::Time::getCurrentTime(), 1, false);
            };
            knob.mouseDown (at (30.0f));
            knob.mouseDrag (at (30.0f - pixelsUp * 0.5f));
            knob.mouseDrag (at (30.0f - pixelsUp));
            knob.mouseUp (at (30.0f - pixelsUp));
        };
        // From the violin, the viola is log2 (1.17) / 4 of the travel up: about 11.3 px of KnobSlider's 200.
        const auto violaPixels = (float) (knob.valueToProportionOfLength (1.17) - knob.valueToProportionOfLength (1.0)) * KnobSlider::fullRangePixels;
        drag (violaPixels - 2.0f, {});
        expect (std::abs (knob.getValue() - 1.17) < 1.0e-6, "snap marks: a drag 2 px short of the viola lands on x1.17 exactly");
        knob.setValue (1.0, juce::dontSendNotification);
        drag (violaPixels - 2.0f, juce::ModifierKeys (juce::ModifierKeys::altModifier));
        expect (std::abs (knob.getValue() - 1.17) > 0.005 && knob.getValue() > 1.1, "snap marks: with Alt held the drag passes the mark");
        knob.setValue (1.0, juce::dontSendNotification);
        drag (violaPixels * 0.5f, {});
        expect (knob.getValue() > 1.03 && knob.getValue() < 1.14, "snap marks: halfway between marks the knob moves freely");
        knob.getProperties().remove ("ringMarks");
        knob.setValue (1.0, juce::dontSendNotification);
        drag (violaPixels - 2.0f, {});
        expect (std::abs (knob.getValue() - 1.17) > 0.005, "snap marks: a knob without marks never snaps");
    }
}

void uiGroup_R6 (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 8, R6: text fitting, header, browser, SEQ, dialogs.
    runGlobalReview8Tests (processor, *pages);
}

void uiGroup_R31C (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 31 (builder C): MATRIX, + ASSIGN, the dock, the wheels.
    runReview31MatrixTests (processor, *pages);
}

void uiGroup_DD (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Step 18: every drop-down select opens its list, click after click.
    runDropdownTests (processor, *pages);
}

void uiGroup_PG (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Step A: the cable view.
    runPatchCableTests (processor);
}

void uiGroup_OR (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Step 16: the oscillators' ROUTING cables.
    runOscRoutingUiTests (processor, *pages);
}

void uiGroup_OS (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Duplicate, drag to reorder, x to remove (2026-10-26).
    runOscSlotUiTests (processor, *pages);
}

void uiGroup_HR (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Bug hunt 2026-10-31: every visible control takes a click at its centre, at 100 % and 175 %.
    runHitReachTests (processor, *pages);
}

void uiGroup_WF (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // The RC 1.0 workflow fixes (2026-10-28).
    runWorkflowUiTests (processor, *pages);
}

void uiGroup_FL (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // The PATCH tab's LFO boxes.
    runFxLfoUiTests (processor, *pages);
    // ARCH-10: the knob geometry table.
    runKnobGeometryUiTests (processor, *pages);
}

void uiGroup_FC (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    runFxCarouselUiTests (processor, *pages);
}

void uiGroup_OP8 (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 8, R1: operator editors and names.
    runOperatorReview8Tests (processor, *pages);
}

void uiGroup_OT (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    runOscTypeTests (processor, *pages);
}

void uiGroup_LAF (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Design sweep: every pop-up draws in the design's look. A menu shown on
    // the desktop never asks its target for a look, nor does an AlertWindow
    // with no owner or JUCE's file browser: they use the process default,
    // which is the editor's own look while it is open. A knob's right-click
    // menu and its "Modulate with" submenu are windows of that look.
    {
        expect (&juce::LookAndFeel::getDefaultLookAndFeel() == &editor->getLookAndFeel()
                    && dynamic_cast<IlanaLookAndFeel*> (&editor->getLookAndFeel()) != nullptr,
                "sweep: while the editor is open its look is the default one (desktop menus, dialogs)");

        // (Real menu windows can't open under xvfb: unit-level checks.) A
        // second editor opening and closing leaves the default in place;
        // only the last one to close hands it back.
        {
            DefaultLookAndFeelScope second;
            expect (&second.get() == &editor->getLookAndFeel(), "sweep: every open editor shares one look");
        }
        expect (&juce::LookAndFeel::getDefaultLookAndFeel() == &editor->getLookAndFeel(),
                "sweep: a second editor closing keeps the default look while one is still open");

        // A menu with no look of its own (a knob's right-click menu, the
        // header's and the dock's) takes the default; its submenus take
        // their parent window's. The default's menu rows are the sheet's.
        auto& lf = juce::LookAndFeel::getDefaultLookAndFeel();
        int w = 0, h = 0;
        lf.getIdealPopupMenuItemSize ("Modulate with", false, 18, w, h);
        expect (h == IlanaLookAndFeel::menuRowHeight && lf.getPopupMenuBorderSize() == IlanaLookAndFeel::menuBorder,
                "sweep: the default look's menu rows are the design's (26 px, 5 px border)");
        const auto tip = lf.getTooltipBounds ("Frame\nWhere in the wavetable the oscillator plays.", { 100, 100 }, { 0, 0, 1000, 1000 });
        expect (tip.getWidth() <= 362 && tip.getHeight() >= 30, "sweep: a tooltip is the design's small card");
        // The one animator: a state change eases (it does not jump) and settles.
        {
            static const int key = 0;
            auto moving = false;
            IlanaTheme::fadeValue (&key, 0, 0.0f, IlanaTheme::FadeRate::slide, true, moving);
            juce::Thread::sleep (30);
            const auto early = IlanaTheme::fadeValue (&key, 0, 1.0f, IlanaTheme::FadeRate::slide, true, moving);
            const auto wasMoving = moving;
            juce::Thread::sleep (100);
            const auto settled = IlanaTheme::fadeValue (&key, 0, 1.0f, IlanaTheme::FadeRate::slide, true, moving);
            expect (early > 0.05f && early < 0.95f && wasMoving && settled > early,
                    "sweep: a switch or a choice eases to its new state (" + juce::String (early, 2) + " then " + juce::String (settled, 2) + ")");
        }
        juce::Slider probe;
        expect (dynamic_cast<IlanaLookAndFeel*> (&probe.getLookAndFeel()) != nullptr,
                "sweep: a component outside the editor (a dialog's, a file browser's) draws in the design's look");
    }
}

void uiGroup_DOCK_CHIP (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // A dock chip's click opens its source's editor over the page (pinned,
    // the knobs it drives lit); a second click closes it.
    {
        pages->showPage ("MAIN");
        settle (200);
        std::vector<ModSourceChip*> chips;
        findAll<ModSourceChip> (*editor, chips);
        ModSourceChip* chip = nullptr;
        for (auto* candidate : chips)
            if (candidate->isVisible() && candidate->onClick != nullptr && chip == nullptr)
                chip = candidate;
        if (chip != nullptr)
        {
            chip->onClick (*chip);
            settle (200);
            auto* popover = pages->getSourcePopover();
            const auto opened = popover != nullptr && popover->isVisible() && pages->getSourcePopoverSource() == chip->getSourceIndex()
                                && pinnedModSource() == chip->getSourceIndex();
            chip->onClick (*chip);
            settle (200);
            expect (opened && ! popover->isVisible() && pinnedModSource() != chip->getSourceIndex(),
                    "a dock chip's click opens its editor over the page, pinned; a second click closes it");
        }
        else
            expect (false, "the dock's chips open their source's editor");
    }
}

void uiGroup_OP_LFO (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // An LFO's pop-up: its SHAPE sits in the title row, but the cross and the
    // rest of that row (the drag strip) still take the mouse (ilana, 2026-10-30:
    // "i cant close the lfo thing", "cant drag it either").
    {
        pages->showPage ("MAIN");
        settle (200);
        std::vector<ModSourceChip*> chips;
        findAll<ModSourceChip> (*editor, chips);
        ModSourceChip* chip = nullptr;
        for (auto* candidate : chips)
            if (candidate->isVisible() && candidate->onClick != nullptr && chip == nullptr
                && Mod::lfoIndexFor ((Mod::Source) candidate->getSourceIndex()) >= 0)
                chip = candidate;
        if (chip != nullptr)
        {
            chip->onClick (*chip);
            settle (200);
            auto* popover = pages->getSourcePopover();
            juce::TextButton* cross = nullptr;
            if (popover != nullptr)
                for (auto* child : popover->getChildren())
                    if (auto* button = dynamic_cast<juce::TextButton*> (child); button != nullptr && button->isVisible())
                        cross = button;
            const auto onCross = cross != nullptr && popover->getComponentAt (cross->getBounds().getCentre()) == cross;
            // (18, 8): the title strip, left of the source name (SourcePopover::padding is 4).
            const auto onStrip = popover != nullptr && popover->getComponentAt (juce::Point<int> (18, 8)) == popover;
            expect (onCross && onStrip, "an LFO's pop-up: the cross and the title strip take the mouse (close, hover, drag)");
            // UI review 30 (S8): a shape drawn by hand gets a pop-up twice as wide, all of it graph; back
            // on a plain shape it is ilana's compact card again.
            if (popover != nullptr)
            {
                const auto lfo = Mod::lfoIndexFor ((Mod::Source) chip->getSourceIndex());
                auto* shape = processor.apvts.getParameter ("lfo" + juce::String (lfo + 1) + "_shape");
                const auto before = shape->getValue();
                const auto compactWidth = popover->getWidth();
                shape->setValueNotifyingHost (shape->convertTo0to1 (7.0f)); // Steps
                settle (300);
                const auto wideWidth = popover->getWidth();
                shape->setValueNotifyingHost (before);
                settle (300);
                expect (wideWidth >= compactWidth + 200 && popover->getWidth() == compactWidth && editor->getLocalBounds().contains (popover->getBounds()),
                        "an LFO's pop-up: Steps widens the graph (" + juce::String (compactWidth) + " to " + juce::String (wideWidth) + " px), a plain shape is compact again");
            }
            if (cross != nullptr)
                cross->triggerClick();
            settle (200);
            expect (popover != nullptr && ! popover->isVisible(), "an LFO's pop-up: the cross closes it");
        }
        else
            expect (false, "the dock has an LFO chip");
    }
}

void uiGroup_POPUP_STACK (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // ARCH-9: the pop-up stack. Esc closes one overlay per press, top first: save, wavetable, zones,
    // samples, tutorial, presets, source, dock, mod cards, remap, stereo, tuning, scope (the old chain's order,
    // whichever was opened first).
    {
        const auto esc = [&] { return pages->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)); };
        const auto names = pages->getPopups().names();
        const auto order = juce::StringArray { "save", "wavetable", "zones", "samples", "tutorial", "presets", "source", "dock",
                                               "modcard", "remap", "stereo", "tuning", "scope" };
        auto sameOrder = (int) names.size() == order.size();
        for (size_t i = 0; sameOrder && i < names.size(); ++i)
            sameOrder = order[(int) i] == juce::String (names[i]);
        expect (sameOrder, "pop-up stack: registered in the old Esc order");

        pages->showPage ("MAIN");
        settle (200);
        while (pages->getPopups().closeTop()) {} // start clean
        expect (! pages->getPopups().closeTop(), "pop-up stack: nothing open, closeTop is false");

        const auto closedName = [&] { return juce::String (pages->getPopups().lastClosed()); };

        pages->setScopeOpen (true);
        settle (200);
        expect (pages->isScopeOpen(), "pop-up stack: the scope opens");
        esc();
        settle (200);
        expect (! pages->isScopeOpen() && closedName() == "scope", "pop-up stack: Esc closes the scope");

        pages->openWavetableEditor (0, IlanaTheme::accent());
        settle (200);
        expect (pages->getWavetableEditor() != nullptr, "pop-up stack: the wavetable editor opens");
        esc();
        settle (200);
        expect (pages->getWavetableEditor() == nullptr && closedName() == "wavetable", "pop-up stack: Esc closes the wavetable editor");

        pages->openZoneEditor (0, IlanaTheme::accent());
        settle (200);
        expect (pages->getZoneEditor() != nullptr, "pop-up stack: the zone map opens");
        esc();
        settle (200);
        expect (pages->getZoneEditor() == nullptr && closedName() == "zones", "pop-up stack: Esc closes the zone map");

        pages->openSampleBrowser (0);
        settle (200);
        expect (pages->getSampleBrowser() != nullptr, "pop-up stack: the sample browser opens");
        esc();
        settle (200);
        expect (pages->getSampleBrowser() == nullptr && closedName() == "samples", "pop-up stack: Esc closes the sample browser");

        // Two open at once: the wavetable editor goes before the scope, though the scope opened first.
        pages->setScopeOpen (true);
        pages->openWavetableEditor (0, IlanaTheme::accent());
        settle (200);
        pages->getPopups().closeTop();
        expect (closedName() == "wavetable" && pages->getWavetableEditor() == nullptr && pages->isScopeOpen(),
                "pop-up stack: wavetable editor and scope open: the editor closes first");
        pages->getPopups().closeTop();
        expect (closedName() == "scope" && ! pages->isScopeOpen(), "pop-up stack: then the scope");

        // The save panel is above the wavetable editor, even though it opened after... and before.
        pages->openWavetableEditor (0, IlanaTheme::accent());
        pages->savePresetAs();
        settle (200);
        const auto bothOpen = pages->getSaveOverlay().isShowing() && pages->getWavetableEditor() != nullptr;
        pages->getPopups().closeTop();
        expect (bothOpen && closedName() == "save" && ! pages->getSaveOverlay().isShowing() && pages->getWavetableEditor() != nullptr,
                "pop-up stack: save panel and wavetable editor open: the save panel closes first");
        pages->getPopups().closeTop();
        expect (closedName() == "wavetable" && pages->getWavetableEditor() == nullptr, "pop-up stack: then the wavetable editor");

        // The zone map before the sample browser before the scope.
        pages->setScopeOpen (true);
        pages->openSampleBrowser (0);
        pages->openZoneEditor (0, IlanaTheme::accent());
        settle (200);
        pages->getPopups().closeTop();
        const auto first = closedName();
        pages->getPopups().closeTop();
        const auto second = closedName();
        pages->getPopups().closeTop();
        expect (first == "zones" && second == "samples" && closedName() == "scope" && ! pages->isScopeOpen(),
                "pop-up stack: zone map, then sample browser, then scope (" + first + ", " + second + ")");

        // The source pop-up goes before the scope.
        pages->showPage ("FILTER");
        settle (200);
        {
            std::vector<ModSourceChip*> chips;
            findAll<ModSourceChip> (*editor, chips);
            ModSourceChip* chip = nullptr;
            for (auto* candidate : chips)
                if (candidate->isVisible() && candidate->onClick != nullptr && chip == nullptr
                    && Mod::lfoIndexFor ((Mod::Source) candidate->getSourceIndex()) >= 0)
                    chip = candidate;
            if (chip != nullptr)
            {
                pages->setScopeOpen (true);
                chip->onClick (*chip);
                settle (200);
                const auto opened = pages->getSourcePopover() != nullptr && pages->getSourcePopover()->isVisible();
                pages->getPopups().closeTop();
                expect (opened && closedName() == "source" && pages->isScopeOpen(), "pop-up stack: source pop-up and scope open: the pop-up closes first");
                pages->getPopups().closeTop();
                expect (closedName() == "scope" && ! pages->isScopeOpen(), "pop-up stack: then the scope");
            }
            else
                expect (false, "pop-up stack: a source chip to open");
        }
        expect (! pages->getPopups().closeTop(), "pop-up stack: all closed, nothing left");
        pages->showPage ("MAIN");
        settle (100);
    }
}

