void uiGroup_M5_PAGES (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // M5/M6/M6b pages.
    {
        const auto visibleKnob = [&] (const juce::String& id)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getParameterId() == id && ! knob->getBounds().isEmpty())
                    return true;
            return false;
        };
        const auto set = [&processor] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        // FM: clicking an algorithm routes the operators.
        pages->showPage ("FM");
        settle (300);

        if (auto* strip = findChild<FmAlgorithmStrip> (*editor))
        {
            // An earlier edit in its own undo step, so undoing the algorithm
            // must leave it alone.
            processor.getUndoManager().beginNewTransaction();
            set ("osc2_level", 0.33f);
            settle (200);

            // The DX7 page: cell 5 is DX7 algorithm 5, under its own number.
            strip->setPage (FmAlgorithmStrip::dx7Low);
            settle (100);
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            const auto at = strip->getCellCentre (4).toFloat();
            const auto now = juce::Time::getCurrentTime();
            const juce::MouseEvent click (source, at, juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                          strip, strip, now, at, now, 1, false);
            static_cast<juce::Component&> (*strip).mouseDown (click);
            static_cast<juce::Component&> (*strip).mouseUp (click);
            settle (400);
            expect (processor.findMatchingDx7Algorithm() == 5 && processor.isOscillatorShown (5),
                    "clicking DX7 algorithm 5 routes six operators and is found as 5");
            expect (visibleKnob ("fm_6to5"), "the FM matrix grows to six operators (its noise row follows the EXTRAS line, I11-5)");

            processor.getUndoManager().undo();
            settle (300);
            const auto afterUndo = processor.findMatchingDx7Algorithm();
            const auto level = processor.apvts.getRawParameterValue ("osc2_level")->load();
            processor.getUndoManager().redo();
            settle (300);
            expect (afterUndo != 5 && std::abs (level - 0.33f) < 0.01f && processor.findMatchingDx7Algorithm() == 5,
                    "undo reverts an algorithm as one step and keeps the edit before it (level "
                        + juce::String (level, 2) + ")");
            expect (strip->getNumCells() == 16 && strip->dx7NumberAt (15) == 16,
                    "the DX7 1-16 page holds sixteen DX7 algorithms by number");
            strip->setPage (FmAlgorithmStrip::dx7High);
            expect (strip->getNumCells() == 16 && strip->dx7NumberAt (0) == 17 && strip->dx7NumberAt (15) == 32,
                    "the DX7 17-32 page holds the other sixteen");
            strip->setPage (FmAlgorithmStrip::basic);
        }
        else
        {
            expect (false, "the FM page has the algorithm strip");
        }

        // The operator panel follows the selected operator and its tuning.
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*editor, buttons);
        // The operators on the one picker (UI review 8, I8-10).
        std::vector<OscPicker*> pickers;
        findAll<OscPicker> (*editor, pickers);
        for (auto* picker : pickers)
            if (visibleInTree (picker))
                picker->pick (1);
        set ("osc2_tune", (float) OscTuning::Ratio);
        settle (400);
        expect (visibleKnob ("osc2_ratio") && ! visibleKnob ("osc1_ratio") && ! visibleKnob ("osc2_fixed_hz"),
                "selecting OSC 2 in Ratio tuning shows its RATIO knob");
        set ("osc2_tune", (float) OscTuning::Fixed);
        settle (400);
        expect (visibleKnob ("osc2_fixed_hz") && ! visibleKnob ("osc2_ratio"), "Fixed tuning swaps RATIO for FIXED");

        // UI review 6: DX7 voices on the FM page. Every eighth factory DX7
        // voice: its algorithm lit under its number, the diagram's nodes
        // apart and inside it with their captions, at least its minimum
        // height; the Amp Env shown as unused; the wheel and pressure routed.
        {
            auto voices = 0, lit = 0, laidOut = 0, ampUnused = 0, wheel = 0;
            juce::String firstBad;
            for (int index = 0; index < names.size(); ++index)
            {
                if (! names[index].contains ("(ROM") || index % 8 != 0)
                    continue;
                processor.loadFactoryPreset (index);
                pages->showPage ("FM");
                settle (150);
                ++voices;
                auto* strip = findChild<FmAlgorithmStrip> (*editor);
                auto* diagram = findChild<FmDiagram> (*editor);
                if (strip == nullptr || diagram == nullptr)
                    continue;
                strip->refreshMatch();
                const auto stored = juce::roundToInt (processor.apvts.getRawParameterValue (OperatorEg::dx7AlgorithmId)->load());
                lit += stored > 0 && strip->getMatchingDx7() == stored ? 1 : 0;

                const auto nodes = diagram->getNodeBounds();
                const auto captions = diagram->getCaptionBoundsList();
                const auto area = diagram->getLocalBounds().toFloat();
                auto apart = diagram->getHeight() >= diagram->getMinimumHeight();
                for (size_t i = 0; i < nodes.size(); ++i)
                {
                    apart = apart && area.contains (nodes[i]) && area.contains (captions[i]);
                    for (size_t j = 0; j < nodes.size(); ++j)
                        apart = apart && (i == j || (! nodes[i].intersects (nodes[j]) && ! captions[i].intersects (nodes[j])
                                                     && ! captions[i].intersects (captions[j])));
                }
                laidOut += apart ? 1 : 0;
                if (! apart && firstBad.isEmpty())
                {
                    firstBad = names[index] + " in " + diagram->getLocalBounds().toString() + " (minimum height " + juce::String (diagram->getMinimumHeight()) + "):";
                    for (size_t i = 0; i < nodes.size(); ++i)
                        firstBad << " [" << nodes[i].toString() << " / " << captions[i].toString() << "]";
                }
                ampUnused += FmOperatorInfo::ampEnvelopeInUse (processor) ? 0 : 1;

                auto routes = 0;
                for (int slot = 0; slot < Mod::maxSlots; ++slot)
                {
                    const auto routing = processor.readModSlot (slot);
                    routes += (routing.source == Mod::Source::ModWheel || routing.source == Mod::Source::Aftertouch)
                                      && Mod::getDestinationNames()[routing.destination] == "OP LFO Pitch Depth"
                                  ? 1 : 0;
                }
                wheel += routes == 2 ? 1 : 0;
            }
            expect (voices >= 30 && lit == voices, "every DX7 voice lights its algorithm by number ("
                                                       + juce::String (lit) + " of " + juce::String (voices) + ")");
            expect (laidOut == voices, "the FM diagram never overlaps or clips a DX7 voice's operators ("
                                           + juce::String (laidOut) + " of " + juce::String (voices)
                                           + (firstBad.isNotEmpty() ? ", first " + firstBad : juce::String()) + ")");
            expect (ampUnused == voices, "a DX7 voice's Amp Env reads as unused (its operators play the Operator Env)");
            expect (wheel == voices, "a DX7 voice routes the wheel and pressure to the Op LFO's pitch depth");

            // The tallest DX7 stacks fit too, with every depth label clear of
            // the nodes, captions and other labels, and the operator card
            // does not move between algorithms (review 7, V7-3 and I7-10).
            processor.loadFactoryPreset (names.indexOf ("E.PIANO 1 (ROM1A)"));
            juce::Rectangle<int> firstDiagramBounds;
            auto cardFixed = true;
            for (int number = 1; number <= 32; ++number)
            {
                processor.applyDx7Algorithm (number);
                settle (150);
                auto* diagram = findChild<FmDiagram> (*editor);
                auto apart = diagram != nullptr && diagram->getHeight() >= diagram->getMinimumHeight();
                auto labelsClear = diagram != nullptr;
                juce::String badLabels;
                if (diagram != nullptr)
                {
                    const auto nodes = diagram->getNodeBounds();
                    const auto captions = diagram->getCaptionBoundsList();
                    const auto labels = diagram->getAmountLabelBoundsList();
                    for (size_t i = 0; i < nodes.size(); ++i)
                        for (size_t j = 0; j < nodes.size(); ++j)
                            apart = apart && diagram->getLocalBounds().toFloat().contains (nodes[i]) && (i == j || ! nodes[i].intersects (nodes[j]));
                    for (size_t i = 0; i < labels.size(); ++i)
                    {
                        auto clear = diagram->getLocalBounds().toFloat().contains (labels[i]);
                        for (size_t j = 0; j < nodes.size(); ++j)
                            clear = clear && ! labels[i].intersects (nodes[j].reduced (nodes[j].getWidth() * 0.06f))
                                    && ! labels[i].intersects (captions[j]);
                        for (size_t j = 0; j < labels.size(); ++j)
                            clear = clear && (i == j || ! labels[i].intersects (labels[j]));
                        if (! clear)
                            badLabels << " [" << labels[i].toString() << "]";
                        labelsClear = labelsClear && clear;
                    }
                    // (UI review 31, 17: the diagram is as tall as its rows need and the card takes the rest, so the card
                    // now changes with the algorithm; it never gets shorter than its old fixed height.)
                    if (firstDiagramBounds.isEmpty())
                        firstDiagramBounds = diagram->getBounds();
                    cardFixed = cardFixed && diagram->getHeight() >= juce::jmin (150, diagram->getPreferredHeight())
                                && diagram->getHeight() <= juce::jmax (150, juce::jmax (diagram->getPreferredHeight(), firstDiagramBounds.getHeight()));
                }
                // Review 8 (V8-7): each stack stands straight, as a DX7
                // chart (an operator driving only one that it alone drives
                // sits right over it), halos never meet, and no depth sits
                // on an arrowhead.
                auto straight = diagram != nullptr, halosApart = diagram != nullptr, headsClear = diagram != nullptr;
                auto columnsClose = diagram != nullptr;
                if (diagram != nullptr)
                {
                    const auto nodes = diagram->getNodeBounds();
                    // Neighbouring columns no further apart than a node, its
                    // caption and a gap: algorithm 1's two stacks read as one
                    // chart, not islands at the box's edges.
                    std::vector<float> xs;
                    for (const auto& node : nodes)
                        xs.push_back (node.getCentreX());
                    std::sort (xs.begin(), xs.end());
                    for (size_t i = 1; i < xs.size(); ++i)
                        columnsClose = columnsClose && xs[i] - xs[i - 1] <= nodes.front().getWidth() + 58.0f + 101.0f;
                    const auto routed = [&] (int a, int b)
                    {
                        return a != b && processor.apvts.getRawParameterValue (FmDiagram::routeId (a, b))->load() > 0.001f;
                    };
                    for (int m = 0; m < 6 && nodes.size() == 6; ++m)
                        for (int t = 0; t < 6; ++t)
                        {
                            if (! routed (m, t))
                                continue;
                            auto drivers = 0, driven = 0;
                            for (int other = 0; other < 6; ++other)
                            {
                                drivers += routed (other, t) ? 1 : 0;
                                driven += routed (m, other) ? 1 : 0;
                            }
                            if (drivers == 1 && driven == 1)
                                straight = straight && std::abs (nodes[(size_t) m].getCentreX() - nodes[(size_t) t].getCentreX()) < 1.0f
                                           && nodes[(size_t) m].getBottom() + 12.0f < nodes[(size_t) t].getY();
                        }
                    const auto halo = diagram->getHaloRoom();
                    for (size_t i = 0; i < nodes.size(); ++i)
                        for (size_t j = i + 1; j < nodes.size(); ++j)
                            halosApart = halosApart && ! nodes[i].expanded (halo).intersects (nodes[j].expanded (halo));
                    for (const auto& label : diagram->getAmountLabelBoundsList())
                        for (const auto& path : diagram->getRoutePathsList())
                            for (size_t k = path.size() - 2; k < path.size(); ++k)
                                headsClear = headsClear && ! label.intersects (juce::Line<float> (path[k - 1], path[k]));
                }
                expect (straight, "DX7 algorithm " + juce::String (number) + "'s stacks stand straight over what they drive");
                expect (columnsClose, "DX7 algorithm " + juce::String (number) + "'s columns sit close, as one chart");
                expect (halosApart, "DX7 algorithm " + juce::String (number) + "'s node halos never meet");
                expect (headsClear, "DX7 algorithm " + juce::String (number) + "'s depth labels sit off the arrowheads");
                expect (apart, "DX7 algorithm " + juce::String (number) + "'s stack fits the diagram without overlaps");
                expect (labelsClear, "DX7 algorithm " + juce::String (number) + "'s depth labels sit clear of nodes, captions and each other" + badLabels);
            }
            // UI review 31, 4 and 34: a WARP FM label never sits under a route knob, however tight the stack; a click on
            // a node selects its operator and leaves OUT / MOD alone, a click on that word switches it.
            {
                processor.loadFactoryPreset (names.indexOf ("Init"));
                const auto setPlain = [&processor] (const char* id, float value)
                {
                    if (auto* parameter = processor.apvts.getParameter (id))
                        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                };
                setPlain ("osc2_on", 1.0f);
                setPlain ("osc2_out", 0.0f);
                setPlain ("fm_amount", 0.35f);
                setPlain ("osc2_warp", 8.0f);
                setPlain ("osc2_warp_amt", 0.35f);
                pages->showPage ("FM");
                settle (200);
                auto* diagram = findChild<FmDiagram> (*editor);
                auto warpClear = diagram != nullptr;
                juce::String warpBad;
                if (warpClear)
                {
                    const auto before = diagram->getBounds();
                    for (const auto height : { diagram->getMinimumHeight(), diagram->getMinimumHeight() + 30, 160, 200, 260 })
                    {
                        diagram->setSize (before.getWidth(), height);
                        const auto labels = diagram->getWarpLabelBoundsList();
                        const auto handle = diagram->getRouteHandle (1, 0);
                        warpClear = warpClear && ! labels.empty() && handle.has_value();
                        if (handle.has_value())
                            for (const auto& label : labels)
                                if (juce::Rectangle<float> (FmDiagram::getHandleDisc(), FmDiagram::getHandleDisc()).withCentre (*handle).intersects (label))
                                {
                                    warpClear = false;
                                    warpBad << " " << height << ": " << label.toString() << " / " << handle->toString();
                                }
                    }
                    diagram->setBounds (before);
                }
                expect (warpClear, "the WARP FM label never sits under a route's depth knob (" + warpBad + ")");

                auto clickSelects = diagram != nullptr && diagram->onSelectOperator != nullptr;
                if (clickSelects)
                {
                    diagram->onSelectOperator (0);
                    auto source = juce::Desktop::getInstance().getMainMouseSource();
                    const auto click = [&] (juce::Point<float> at)
                    {
                        const auto now = juce::Time::getCurrentTime();
                        const auto make = [&] (bool up)
                        {
                            return juce::MouseEvent (source, at, up ? juce::ModifierKeys() : juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                     diagram, diagram, now, at, now, 1, false);
                        };
                        diagram->mouseDown (make (false));
                        diagram->mouseUp (make (true));
                    };
                    const auto outBefore = processor.apvts.getRawParameterValue ("osc2_out")->load();
                    const auto nodes = diagram->getNodeBounds(); // (OSC 1, OSC 2)
                    click (nodes.size() > 1 ? nodes[1].getCentre().translated (0.0f, -nodes[1].getHeight() * 0.3f) : juce::Point<float>());
                    clickSelects = diagram->getSelectedOperator() == 1 && processor.apvts.getRawParameterValue ("osc2_out")->load() == outBefore;
                    click (diagram->getRoleWordBounds (1).getCentre());
                    clickSelects = clickSelects && processor.apvts.getRawParameterValue ("osc2_out")->load() != outBefore;
                    processor.getUndoManager().undo();
                }
                expect (clickSelects, "a click on an FM node selects its operator; a click on its OUT / MOD word switches whether it is heard");
            }

            expect (cardFixed, "across DX7 algorithms 1-32 the diagram is as tall as its rows need, or the old height, the card the rest");

            // One node style in every layout: "OSC n" inside the circle,
            // also for a three-oscillator patch (V7-13).
            processor.loadFactoryPreset (names.indexOf ("Neuro Wobble"));
            pages->showPage ("FM");
            settle (200);
            if (auto* diagram = findChild<FmDiagram> (*editor))
                expect (diagram->getOperatorRadius() >= 21.0f && diagram->getNodeBounds().size() >= 2,
                        "a basic FM patch draws full-size OSC n nodes");
            expect (FmAlgorithmStrip::nearestBasic (processor) == 0,
                    "a one-modulator routing that no tile matches is named after the nearest tile (B1)");

            // Review 8. One operator card height on every patch and for
            // every kind of operator, panel and tab (I8-11): the diagram
            // above it never moves. An off oscillator's node shows no tuning
            // or level (S8-36). One matrix cell size on every patch, and the
            // card no taller than its rows (V8-6, V8-35). The ring mod, sync
            // and noise rows fold away on a DX7 voice (S8-21). No OP PITCH
            // link on a patch without the Operator Env.
            {
                const auto diagramBounds = [&]
                {
                    auto* diagram = findChild<FmDiagram> (*editor);
                    return diagram != nullptr ? diagram->getBounds() : juce::Rectangle<int>();
                };
                const auto knobBounds = [&] (const juce::String& id)
                {
                    std::vector<KnobControl*> all;
                    findAll<KnobControl> (*editor, all);
                    for (auto* knob : all)
                        if (visibleInTree (knob) && knob->getParameterId() == id)
                            return knob->getBounds();
                    return juce::Rectangle<int>();
                };
                const auto clickButton = [&] (const juce::String& text)
                {
                    std::vector<juce::TextButton*> all;
                    findAll<juce::TextButton> (*editor, all);
                    for (auto* button : all)
                        if (button->getButtonText() == text && visibleInTree (button))
                        {
                            button->triggerClick();
                            settle (250);
                            return true;
                        }
                    return false;
                };
                const auto pickOsc = [&] (int osc)
                {
                    std::vector<OscPicker*> pickers;
                    findAll<OscPicker> (*editor, pickers);
                    for (auto* picker : pickers)
                        if (visibleInTree (picker))
                            picker->pick (osc);
                    settle (250);
                };
                const auto pitchLfoLinkShown = [&]
                {
                    std::vector<juce::TextButton*> all;
                    findAll<juce::TextButton> (*editor, all);
                    return std::any_of (all.begin(), all.end(), [] (juce::TextButton* button)
                                        { return button->getButtonText().contains ("OP PITCH") && visibleInTree (button); });
                };
                const auto heightOnNeuro = diagramBounds().getHeight();
                auto* neuroDiagram = findChild<FmDiagram> (*editor);
                const auto offCaption = neuroDiagram != nullptr && neuroDiagram->getCaptionTexts().size() == 3
                                        && neuroDiagram->getCaptionTexts()[2].isEmpty() && neuroDiagram->getCaptionTexts()[0].isNotEmpty();
                expect (offCaption, "FM: an off oscillator's node shows no tuning or level captions (S8-36)");
                const auto neuroCell = knobBounds ("fm_amount"), neuroNext = knobBounds ("fm_fb2");
                // (ilana's FM option A, 2026-10-29: the extras always stand in the right column under FM MODE.)
                expect (visibleKnob ("ring_mod") && visibleKnob ("fm_noise_color"),
                        "FM: a basic patch shows RING MOD, SYNC 2 TO 1 and the noise's COLOUR in the EXTRAS row");
                expect (! pitchLfoLinkShown(), "FM: no OP PITCH link on a patch without the Operator Env");
                std::vector<EnvelopeDisplay*> graphs;
                findAll<EnvelopeDisplay> (*editor, graphs);
                expect (std::any_of (graphs.begin(), graphs.end(), [] (EnvelopeDisplay* graph) { return visibleInTree (graph); }),
                        "FM: an Amp Env operator's card shows its envelope where an Operator Env one shows its own");
                pickOsc (1);
                const auto sameOnOsc2 = diagramBounds().getHeight() == heightOnNeuro;

                processor.loadFactoryPreset (names.indexOf ("E.PIANO 1 (ROM1A)"));
                settle (300);
                pickOsc (0);
                // (2026-10-29: a patch without the Operator Env gets the one-row card, a DX7 voice the taller one; within a
                // patch the height never changes.)
                const auto heightOnDx = diagramBounds().getHeight();
                auto sameKinds = sameOnOsc2;
                std::vector<CardTabs*> tabs;
                findAll<CardTabs> (*editor, tabs);
                for (auto* tab : tabs)
                    if (visibleInTree (tab) && tab->getNames().contains ("KEYS & VELOCITY"))
                    {
                        tab->setSelected (1, true);
                        settle (250);
                        sameKinds = sameKinds && diagramBounds().getHeight() == heightOnDx;
                        tab->setSelected (0, true);
                    }
                expect (pitchLfoLinkShown(), "FM: a DX7 voice's operator card links to OP PITCH and OP LFO");
                pickOsc (1);
                sameKinds = sameKinds && diagramBounds().getHeight() == heightOnDx;
                expect (sameKinds, "FM: the operator card keeps one height across a patch's operators and tabs (I8-11)");

                // Each route in use: a row in ROUTES and a knob on its cable, both the same parameter.
                {
                    std::vector<KnobControl*> all;
                    findAll<KnobControl> (*editor, all);
                    const auto shownFor = [&] (const juce::String& id)
                    {
                        return (int) std::count_if (all.begin(), all.end(), [&] (KnobControl* knob) { return visibleInTree (knob) && knob->getParameterId() == id; });
                    };
                    expect (shownFor ("fm_amount") == 2 && shownFor ("fm_fb6") == 2 && shownFor ("fm_1to2") == 0,
                            "FM: a route in use has its ROUTES row and its knob on the cable; one not in use has neither ("
                                + juce::String (shownFor ("fm_amount")) + ", " + juce::String (shownFor ("fm_fb6")) + ", " + juce::String (shownFor ("fm_1to2")) + ")");
                }
                expect (visibleKnob ("ring_mod"), "FM: a DX7 voice shows the EXTRAS too (one layout)");
            }

            // The operator card speaks the synth's words: a rate as a time, a
            // level as dB, the output as OUTPUT and the oscillator level as LEVEL.
            processor.loadFactoryPreset (names.indexOf ("E.PIANO 1 (ROM1A)"));
            pages->showPage ("FM");
            settle (300);
            {
                std::vector<OscPicker*> pickers;
                findAll<OscPicker> (*editor, pickers);
                for (auto* picker : pickers)
                    if (visibleInTree (picker))
                        picker->pick (0);
            }
            settle (300);
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            juce::String attackText, peakText, levelLabel, trimLabel;
            for (auto* knob : knobs)
            {
                const auto id = knob->getParameterId();
                if (id == "osc1_eg_r1")
                    attackText = knob->getLabelText() + " " + knob->getSlider().getTextFromValue (knob->getSlider().getValue());
                if (id == "osc1_eg_l1")
                    peakText = knob->getLabelText() + " " + knob->getSlider().getTextFromValue (knob->getSlider().getValue());
                if (id == "osc1_eg_out" && visibleInTree (knob))
                    levelLabel = knob->getLabelText();
                if (id == "osc1_level")
                    trimLabel = knob->getLabelText();
            }
            expect (attackText.startsWith ("ATTACK ") && (attackText.endsWith (" ms") || attackText.endsWith (" s"))
                        && peakText.startsWith ("PEAK ") && peakText.endsWith (" dB") && levelLabel == "OUTPUT" && (trimLabel == "LEVEL" || trimLabel == "VOICE LEVEL"),
                    "the Operator Env reads in the synth's words and units (" + attackText + ", " + peakText + ")");

            // Review 7: the time knobs turn the normal way (clockwise is
            // longer), a ratio reads "×1.00", a silent level "-inf dB", and
            // no visible text or help names the "Operator EG" or "Op EG".
            for (auto* knob : knobs)
                if (knob->getParameterId() == "osc1_eg_r1")
                    expect (knob->getSlider().valueToProportionOfLength (10.0) > knob->getSlider().valueToProportionOfLength (90.0),
                            "an Operator Env time knob turns clockwise for a longer stage (I7-3)");
            expect (describeValue ("osc2_ratio", 1.0f) == juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + "1.00"
                        && describeValue ("osc2_eg_l2", 0.0f) == silentDecibels() && describeValue ("osc2_eg_out", 0.0f) == silentDecibels(),
                    "a ratio reads x1.00 and a silent Operator Env level " + silentDecibels());
            {
                juce::StringArray stale;
                for (auto* parameter : processor.getParameters())
                    if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                        for (const auto& text : { describeParameter (withId->paramID), withId->getName (100) })
                            if (text.contains ("Operator EG") || text.contains ("Op EG") || text.contains ("OP EG"))
                                stale.add (withId->paramID);
                std::vector<juce::Component*> all;
                findAll<juce::Component> (*editor, all);
                for (auto* component : all)
                {
                    juce::String text;
                    if (auto* label = dynamic_cast<juce::Label*> (component))
                        text = label->getText();
                    if (auto* button = dynamic_cast<juce::Button*> (component))
                        text = button->getButtonText() + " " + button->getTooltip();
                    if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (component))
                        text << " " << tip->getTooltip();
                    if (text.contains ("Operator EG") || text.contains ("Op EG") || text.contains ("OP EG"))
                        stale.add (text.substring (0, 40));
                }
                for (const auto& name : Mod::getDestinationNames())
                    if (name.contains ("Op EG") || name.contains ("Op Env") || name.contains ("Op LFO") || name.contains ("Op Pitch"))
                        stale.add (name);
                expect (stale.isEmpty(), "the Operator Env has one name everywhere (I7-6): " + stale.joinIntoString (", "));
            }
            // SPACE is live on a DX7 voice: the reverb is on, its dry kept.
            expect (processor.apvts.getRawParameterValue ("fx_reverb_on")->load() > 0.5f
                        && processor.apvts.getRawParameterValue ("fx_reverb_keep_dry")->load() > 0.5f,
                    "a DX7 voice's SPACE macro drives a reverb that is on (I7-4)");

            // Its graph is an editor: four handles inside the plot.
            OperatorEnvDisplay* graph = nullptr;
            std::vector<OperatorEnvDisplay*> graphs;
            findAll<OperatorEnvDisplay> (*editor, graphs);
            for (auto* candidate : graphs)
                if (visibleInTree (candidate))
                    graph = candidate;
            auto handlesInside = graph != nullptr;
            if (graph != nullptr)
                for (const auto point : graph->getHandlePositions())
                    handlesInside = handlesInside && graph->getLocalBounds().toFloat().contains (point);
            expect (handlesInside, "the Operator Env graph shows its four stage handles");

            // FB TYPE dims without a feedback route (OSC 1 on E.PIANO 1);
            // empty matrix cells are dots (their knobs hidden until hovered).
            std::vector<ComboControl*> combos;
            findAll<ComboControl> (*editor, combos);
            auto fbDimmed = false;
            for (auto* combo : combos)
                if (visibleInTree (combo) && combo->getComboBox().getText() == "DX7")
                    fbDimmed = fbDimmed || combo->getAlpha() < 1.0f;
            expect (fbDimmed, "FB TYPE dims on an operator with no feedback route");
            auto emptyHidden = true, usedShown = false;
            for (auto* knob : knobs)
            {
                if (! visibleInTree (knob))
                    continue;
                if (knob->getParameterId() == FmDiagram::routeId (0, 1))
                    emptyHidden = false;
                if (knob->getParameterId() == FmDiagram::routeId (1, 0))
                    usedShown = usedShown || knob->getAlpha() > 0.99f;
            }
            expect (emptyHidden && usedShown, "an unused FM route shows no knob; a route in use shows its knob");

            // A switched-off oscillator's routes stay listed but can't be edited.
            set ("osc2_on", 0.0f);
            settle (300);
            auto offRow = true;
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getParameterId() == FmDiagram::routeId (1, 0))
                    offRow = offRow && ! knob->isEnabled();
            expect (offRow, "an off oscillator's FM routes can't be edited");
            set ("osc2_on", 1.0f);

            // PITCH & LFO opens the voice's pitch envelope.
            findAll<juce::TextButton> (*editor, buttons);
            for (auto* button : buttons)
                if (button->getButtonText().contains ("OP PITCH") && visibleInTree (button))
                    button->triggerClick();
            settle (300);
            graphs.clear();
            findAll<OperatorEnvDisplay> (*editor, graphs);
            auto pitchShown = false;
            for (auto* candidate : graphs)
                pitchShown = pitchShown || (visibleInTree (candidate) && candidate->isPitch());
            expect (pitchShown && pages->getCurrentPageId() == "MAIN", "OP PITCH · OP LFO opens the pitch envelope's graph in its pop-up on PLAY");
            // (Closed again: its cross, not an FM route's remove button.)
            findAll<juce::TextButton> (*editor, buttons);
            for (auto* button : buttons)
                if (button->getButtonText() == juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) && button->getName() != "removeRoute" && visibleInTree (button))
                {
                    button->triggerClick();
                    break;
                }
            settle (200);

            // (The MOD pools carried OP ENV, OP PITCH and OP LFO as pool cards, I7-7 / I7-8: the MOD page is gone.)
            pages->showPage ("MAIN");
            settle (600);

            processor.loadFactoryPreset (neuroWobble);
            settle (300);
            // UI review 8, I8-5 / S8-1 / V8-1: elsewhere they are absent
            // (not greyed), and so is the old MSEG module (I8-4), from the
            // source menus.
            expect (! FmOperatorInfo::anyOperatorEnv (processor) && ! modSourceInPatch (processor, Mod::Source::OpLfo)
                        && ! modSourceInPatch (processor, Mod::Source::OpPitchEnv) && ! modSourceInPatch (processor, Mod::Source::Mseg),
                    "a patch without the Operator Env uses no OP ENV / OP PITCH / OP LFO, nor an MSEG it doesn't use");
            {
                pages->showPage ("MAIN");
                settle (300);
                std::vector<CardTabs*> tabs;
                if (auto* page = pages->getCurrentPage())
                    findAll<CardTabs> (*page, tabs);
                auto noOpTab = true;
                for (auto* candidate : tabs)
                    if (candidate->getNames().contains ("AMP ENV"))
                        noOpTab = noOpTab && ! candidate->getNames().contains ("OP ENV") && candidate->getNames()[0] == "AMP ENV";
                expect (noOpTab, "PLAY's ENVELOPE card has no OP ENV tab on a patch without the Operator Env");
                pages->showPage ("MAIN");
                settle (100);
            }
            pages->showPage ("FM");
            settle (200);
        }

        // OSC: the PD chain row is always there (V14-6); its amounts dim while their stage is Off.
        pages->showPage ("MAIN");
        settle (300);
        set ("osc1_mode", 0.0f);
        set ("osc1_warp", 0.0f);
        set ("osc1_warp2", 0.0f);
        settle (300);
        const auto drawnBefore = visibleKnob ("osc1_warp2_amt");
        const auto boundsOf = [&] (const juce::String& id)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getParameterId() == id)
                    return knob->getScreenBounds();
            return juce::Rectangle<int>();
        };
        const auto ringBefore = boundsOf ("osc1_warp2_amt");
        set ("osc1_warp", (float) Warp::PdSaw);
        settle (300);
        set ("osc1_warp2", 1.0f);
        settle (300);
        // (2026-10-22: WARP 2 always has its row, so picking it never changes the card's height.)
        expect (drawnBefore && visibleKnob ("osc1_warp2_amt")
                    && boundsOf ("osc1_warp2_amt") == ringBefore,
                "OSC 1 shows its WARP 2 row while Off, and picking a stage keeps the card's height");

        // M7.3: Tine and Reed swap the string controls for the pickup (on
        // the oscillator's card).
        set ("osc1_mode", 1.0f);
        set ("osc1_excite", 7.0f);
        settle (300);
        expect (visibleKnob ("osc1_ep_distance") && visibleKnob ("osc1_ep_position") && visibleKnob ("osc1_hammer_hard")
                    && ! visibleKnob ("osc1_string_stiffness") && ! visibleKnob ("osc1_string_sustain"),
                "Tine shows DISTANCE, OFFSET and HAMMER instead of the string controls");
        set ("osc1_excite", 0.0f);
        settle (300);
        expect (! visibleKnob ("osc1_ep_distance") && visibleKnob ("osc1_string_stiffness"),
                "a plucked string hides the pickup controls again");
        set ("osc1_mode", 0.0f);
        settle (300);

        // M7.4: EDIT copies a factory table into a free patch table and
        // opens the editor on it; edits reach the table the oscillator plays.
        {
            const auto factoryCount = TableFactory::getNumFactoryTables();
            set ("osc1_table", 3.0f);
            settle (200);
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*editor, buttons);
            juce::TextButton* editButton = nullptr;
            for (auto* button : buttons)
                if (button->getButtonText().startsWith ("EDIT TABLE") && visibleInTree (button) && editButton == nullptr)
                    editButton = button;
            expect (editButton != nullptr, "the OSC page has an EDIT button for the wavetable");
            if (editButton != nullptr)
            {
                editButton->triggerClick();
                settle (300);
                auto* host = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
                auto* tableEditor = host != nullptr ? host->getWavetableEditor() : nullptr;
                const auto choice = juce::roundToInt (processor.apvts.getRawParameterValue ("osc1_table")->load());
                expect (tableEditor != nullptr && tableEditor->isVisible() && choice >= factoryCount
                            && processor.isUserSlotEdited (choice - factoryCount),
                        "EDIT on a factory table opens the editor on a patch table the oscillator now plays");
                if (tableEditor != nullptr)
                {
                    const auto slot = tableEditor->getSlot();
                    const auto before = processor.getUserTableDoc (slot).frames[0];
                    tableEditor->selectFrame (0);
                    tableEditor->drawLine (0.0f, 1.0f, 0.5f, -1.0f);
                    auto doc = processor.getUserTableDoc (slot);
                    expect (doc.recipes[0].kind == FrameRecipe::Kind::Draw && doc.frames[0] != before,
                            "drawing in the editor changes the table's first frame");
                    tableEditor->setHarmonic (3, 0.8f);
                    doc = processor.getUserTableDoc (slot);
                    expect (doc.recipes[0].kind == FrameRecipe::Kind::Harmonics && doc.recipes[0].magnitudes.size() >= 3
                                && std::abs (doc.recipes[0].magnitudes[2] - 0.8f) < 1.0e-6f,
                            "a SPECTRUM bar sets that harmonic");
                    expect (tableEditor->applyFormulaText ("sin(2*pi*x) * (1 - f)", true)
                                && processor.getUserTableDoc (slot).recipes.back().kind == FrameRecipe::Kind::Formula,
                            "a formula applies to every frame");
                    expect (! tableEditor->applyFormulaText ("sin(", false), "a broken formula is refused");
                    tableEditor->morphTable (16, true);
                    expect (processor.getUserTableDoc (slot).getNumFrames() == 16
                                && processor.getWavetable (factoryCount + slot)->getNumFrames() == 16,
                            "MORPH rebuilds the table at 16 frames and the oscillator plays it");
                    // SPECTRUM keeps its scale through a drag: holding the
                    // loudest (here the only) bar at -30 dB leaves it at -30 dB,
                    // where it used to sink further on every mouse move; a fast
                    // sweep sets every bar it passes.
                    tableEditor->applyFormulaText ("sin(2*pi*x)", false);
                    std::vector<juce::TextButton*> modeButtons;
                    findAll<juce::TextButton> (*tableEditor, modeButtons);
                    for (auto* button : modeButtons)
                        if (button->getButtonText() == "SPECTRUM")
                            button->triggerClick();
                    settle (50);
                    {
                        auto& canvas = tableEditor->getCanvas();
                        auto bars = canvas.getLocalBounds().toFloat().reduced (10.0f);
                        bars.removeFromBottom (bars.getHeight() * 0.22f);
                        bars.removeFromBottom (22.0f);
                        const auto barAt = [&] (int harmonic, float t)
                        {
                            return juce::Point<float> (bars.getX() + ((float) harmonic - 0.5f) * bars.getWidth() / 64.0f,
                                                       bars.getBottom() - t * bars.getHeight());
                        };
                        const auto press = barAt (1, 0.5f);
                        canvas.mouseDown (OscSlotUi::mouse (canvas, press, true, press));
                        for (int i = 0; i < 6; ++i)
                            canvas.mouseDrag (OscSlotUi::mouse (canvas, press, true, press));
                        canvas.mouseUp (OscSlotUi::mouse (canvas, press, false, press));
                        tableEditor->commitNow();
                        auto held = processor.getUserTableDoc (slot).recipes[0];
                        const auto level = held.magnitudes.empty() ? 0.0f : held.magnitudes[0];
                        expect (held.kind == FrameRecipe::Kind::Harmonics
                                    && std::abs (juce::Decibels::gainToDecibels (level) + 30.0f) < 1.5f,
                                "a SPECTRUM bar held at half height stays at -30 dB through the drag (was "
                                    + juce::String (juce::Decibels::gainToDecibels (level), 1) + " dB)");

                        const auto from = barAt (2, 0.98f), to = barAt (12, 0.98f);
                        canvas.mouseDown (OscSlotUi::mouse (canvas, from, true, from));
                        canvas.mouseDrag (OscSlotUi::mouse (canvas, to, true, from));
                        canvas.mouseUp (OscSlotUi::mouse (canvas, to, false, from));
                        tableEditor->commitNow();
                        held = processor.getUserTableDoc (slot).recipes[0];
                        auto filled = held.magnitudes.size() >= 12;
                        for (int k = 1; filled && k < 12; ++k)
                            filled = held.magnitudes[(size_t) k] > 0.5f;
                        expect (filled, "a fast SPECTRUM sweep sets every bar between its two ends");
                    }
                    tableEditor->applyShape (1);
                    save (*tableEditor, juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-wt-editor.png"));
                    host->closeWavetableEditor();
                    settle (100);
                    expect (host->getWavetableEditor() == nullptr, "CLOSE takes the editor away");
                }
            }
            set ("osc1_table", 7.0f);
            settle (200);
        }

        // ENV: DAHDSR and key-rate knobs (in the Amp Env's pop-up).
        uiOpenSourcePopover (ctx, Mod::Source::AmpEnv);
        expect (visibleKnob ("amp_delay") && visibleKnob ("amp_hold") && visibleKnob ("amp_keyrate"),
                "the amp envelope's pop-up shows DELAY, HOLD and KEY RATE");
        uiCloseSourcePopover (ctx);

        // The destination menu files the M5/M6 targets with their oscillator
        // and under FM, not under Physical & Keys.
        {
            juce::ComboBox combo;
            MatrixMenus::fillDestinations (combo);
            const auto submenuHolding = [&combo] (const juce::String& itemText)
            {
                for (juce::PopupMenu::MenuItemIterator top (*combo.getRootMenu()); top.next();)
                    if (auto* sub = top.getItem().subMenu.get())
                        for (juce::PopupMenu::MenuItemIterator inner (*sub); inner.next();)
                            if (inner.getItem().text == itemText)
                                return top.getItem().text;
                return juce::String();
            };
            // (The menu shows display names; look them up from the saved ones.)
            const auto shown = [] (const juce::String& savedName)
            {
                return ModNames::destination (Mod::getDestinationNames().indexOf (savedName));
            };
            expect (submenuHolding (shown ("Osc2 Warp 2")) == "OSC 2" && submenuHolding (shown ("FM Noise > Osc3")) == "FM"
                        && submenuHolding (shown ("FM Osc4 > Osc1")) == "FM" && submenuHolding (shown ("Osc1 Hammer")) == "Physical & Keys",
                    "the destination menu files new targets by oscillator and FM ("
                        + shown ("Osc2 Warp 2") + ": " + submenuHolding (shown ("Osc2 Warp 2")) + ", "
                        + shown ("FM Noise > Osc3") + ": " + submenuHolding (shown ("FM Noise > Osc3")) + ", "
                        + shown ("Osc1 Hammer") + ": " + submenuHolding (shown ("Osc1 Hammer")) + ")");
        }

        // MATRIX: a routing in slot 60 shows up as a row.
        set ("mod60_src", (float) Mod::Source::Lfo3);
        set ("mod60_dst", (float) Mod::Destination::Filter1Cutoff);
        set ("mod60_amt", 0.3f);
        pages->showPage ("MATRIX");
        settle (400);
        std::vector<MatrixRow*> matrixRows;
        findAll<MatrixRow> (*editor, matrixRows);
        auto slot60 = false;
        for (auto* row : matrixRows)
            slot60 = slot60 || (row->isVisible() && row->getSlotIndex() == 59);
        expect (matrixRows.size() == (size_t) Mod::maxSlots && slot60, "the matrix page has 64 rows and shows slot 60");
    }
}

void uiGroup_NEXT_PRESET (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Header: next steps to the following preset and the display follows.
    std::vector<IconButton*> buttons;
    findAll<IconButton> (*editor, buttons);

    for (auto* button : buttons)
    {
        if (button->getName() == "next")
        {
            button->triggerClick();
            settle (400);
            // (the arrows walk the browser's list, which the click made)
            juce::String following;
            if (auto* panel = findChild<PresetPanel> (*editor))
            {
                const auto listed = panel->getListedNames();
                following = listed[(listed.indexOf (names[neuroWobble]) + 1) % juce::jmax (1, listed.size())];
            }
            expect (following.isNotEmpty() && processor.getCurrentPresetName() == following,
                    "next-preset button loads the browser's following preset ('" + processor.getCurrentPresetName() + "', expected '"
                        + following + "')");
        }
    }
}

void uiGroup_UNDO_DRAWN (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 4 (V1, S1): drawn data (LFO curves, remaps, clips) is undone
    // one gesture at a time, EDITED sees it, and a load over an edited patch
    // asks once.
    {
        const auto event = [] (juce::Component& component, juce::Point<float> position, bool dragged, int clicks = 1,
                               juce::Point<float> down = { -1.0e9f, -1.0e9f })
        {
            if (down.x < -1.0e8f)
                down = position;
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys(),
                                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(),
                                     down, juce::Time::getCurrentTime(), clicks, dragged);
        };
        const auto gesture = [&event] (juce::Component& component, juce::Point<float> from, juce::Point<float> to, int clicks = 1)
        {
            component.mouseDown (event (component, from, false, clicks));
            component.mouseDrag (event (component, to, true, clicks, from));
            component.mouseUp (event (component, to, true, clicks, from));
        };
        const auto sameCurve = [] (const LfoCurve& a, const LfoCurve& b) { return a.toString() == b.toString(); };

        // An LFO curve stroke.
        {
            if (auto* shape = processor.apvts.getParameter ("lfo1_shape"))
                shape->setValueNotifyingHost (shape->convertTo0to1 ((float) IlanaSynthAudioProcessor::curveShape));

            LfoDisplay display (processor, 0);
            display.setSize (300, 150);
            const auto before = processor.getLfoCurve (0);
            clearHistory();
            gesture (display, { 100.0f, 50.0f }, { 120.0f, 40.0f });
            settle (50);
            const auto drawn = processor.getLfoCurve (0);
            const auto steps = undoSteps();
            processor.getUndoManager().undo();
            const auto undone = processor.getLfoCurve (0);
            processor.getUndoManager().redo();
            expect (! sameCurve (before, drawn) && steps.size() == 1 && steps[0] == "LFO 1 curve" && sameCurve (undone, before)
                        && sameCurve (processor.getLfoCurve (0), drawn),
                    "an LFO curve stroke is one undo step ('" + steps.joinIntoString ("', '") + "'); undo and redo restore the curve");
        }

        // A remap curve drag.
        {
            RemapEditor remap (processor, 5, juce::Colours::orange);
            clearHistory();
            gesture (remap, { 150.0f, 60.0f }, { 160.0f, 50.0f });
            const auto drawn = processor.isModRemapOn (5);
            const auto steps = undoSteps();
            processor.getUndoManager().undo();
            expect (drawn && steps.size() == 1 && steps[0] == "Remap curve 6" && ! processor.isModRemapOn (5),
                    "a remap curve drag is one undo step and undo straightens it again");
        }

        // A clip note: one step, and EDITED follows it (no parameter changes).
        std::vector<IconButton*> buttons;
        findAll<IconButton> (*editor, buttons);
        auto* confirm = findChild<ConfirmOverlay> (*editor);
        IconButton* nextButton = nullptr;
        for (auto* button : buttons)
            if (button->getName() == "next")
                nextButton = button;

        if (confirm != nullptr && nextButton != nullptr)
        {
            // Start from a freshly loaded preset.
            nextButton->triggerClick();
            settle (300);
            const auto loadedName = processor.getCurrentPresetName();
            expect (! pages->isPatchEdited() && ! processor.getClipState().hasAny(), "a freshly loaded preset is not EDITED and has no clips");

            ClipEditor clips (processor, juce::Colours::orange);
            clips.setSize (400, 200);
            clips.setDrawMode (false); // (DRAW, on by default, places a note with one click)
            clearHistory();
            gesture (clips, { 80.0f, 100.0f }, { 80.0f, 100.0f }, 2); // a double-click adds a note
            settle (50);
            const auto steps = undoSteps();
            const auto edited = pages->isPatchEdited();
            processor.getUndoManager().undo();
            const auto undone = ! processor.getClipState().hasAny() && ! pages->isPatchEdited();
            processor.getUndoManager().redo();
            expect (steps.size() == 1 && steps[0] == "Add clip note" && undone && processor.getClipState().hasAny(),
                    "adding a clip note is one undo step ('" + steps.joinIntoString ("', '") + "'); undo and redo bring it back");
            expect (edited && pages->isPatchEdited(), "a clip edit lights EDITED, and undoing it clears it");

            // A load over the edit asks; Cancel keeps the patch.
            pages->setAsksBeforeReplacingEdits (true);
            nextButton->triggerClick();
            settle (100);
            expect (confirm->isAsking() && processor.getCurrentPresetName() == loadedName,
                    "the next-preset button asks before replacing an edited patch");
            // UI review 6: one dirty state. If the dialog asks, the header
            // says EDITED; and it offers to save first.
            expect (pages->isEditedBadgeShown(), "when the dialog asks, the header's EDITED badge shows");
            expect (confirm->hasAlternative() && confirm->getAlternativeText() == "SAVE AS... AND LOAD",
                    "the dialog offers Save as and load on a factory preset (" + confirm->getAlternativeText() + ")");

            // Save and load on a factory preset opens Save As; cancelling
            // it keeps the edited patch.
            if (auto* saveOverlay = findChild<SavePresetOverlay> (*editor))
            {
                confirm->chooseAlternative();
                settle (200);
                expect (saveOverlay->isShowing() && processor.getCurrentPresetName() == loadedName,
                        "Save and load opens SAVE AS first");
                saveOverlay->cancel();
                settle (200);
                expect (! saveOverlay->isShowing() && processor.getCurrentPresetName() == loadedName && pages->isPatchEdited(),
                        "cancelling that SAVE AS keeps the edited patch");
                nextButton->triggerClick();
                settle (100);
            }

            confirm->finish (false);
            settle (100);
            expect (processor.getCurrentPresetName() == loadedName && pages->isPatchEdited() && processor.getClipState().hasAny(),
                    "Cancel keeps the edited patch");

            // Load anyway loads it, and EDITED clears.
            nextButton->triggerClick();
            settle (100);
            confirm->finish (true);
            settle (300);
            const auto landed = processor.getCurrentPresetName();
            expect (landed != loadedName && ! pages->isPatchEdited() && ! processor.getClipState().hasAny(),
                    "Load anyway loads the next preset and EDITED clears ('" + landed + "')");

            // Stepping on from an unedited patch doesn't ask again.
            nextButton->triggerClick();
            settle (300);
            expect (! confirm->isAsking() && processor.getCurrentPresetName() != landed,
                    "stepping on from the loaded preset doesn't ask again");

            // Undoing the load brings the clip back with the old patch.
            processor.getUndoManager().undo();
            processor.getUndoManager().undo();
            settle (100);
            expect (processor.getClipState().hasAny(), "undoing a load restores the clips it replaced");

            // UI review 7 (S7-32): the dialog is as tall as what it says,
            // and while the browser is open one "Load anyway" covers the
            // rest of that browsing session.
            if (auto* display = findChild<PresetDisplay> (*editor); display != nullptr && display->onClick != nullptr)
            {
                auto* cutoff = processor.apvts.getParameter ("f1_cutoff");
                const auto edit = [&] { cutoff->setValueNotifyingHost (cutoff->getValue() > 0.5f ? 0.3f : 0.7f); settle (50); };
                display->onClick();
                settle (400);
                auto* panel = findChild<PresetPanel> (*editor);
                edit();
                nextButton->triggerClick();
                settle (100);
                const auto asked = confirm->isAsking();
                const auto tight = confirm->panelBounds().getHeight();
                confirm->finish (true);
                settle (200);
                const auto first = processor.getCurrentPresetName();
                edit();
                nextButton->triggerClick();
                settle (200);
                const auto askedAgain = confirm->isAsking();
                expect (asked && ! askedAgain && processor.getCurrentPresetName() != first,
                        "while browsing, one Load anyway covers the session (asked " + juce::String (asked ? "once" : "never") + ")");
                expect (tight > 100 && tight <= 160, "the confirm is as tall as its text (" + juce::String (tight) + " px)");
                if (askedAgain)
                    confirm->finish (false);
                if (panel != nullptr)
                    panel->close();
                settle (400);
                edit();
                nextButton->triggerClick();
                settle (100);
                expect (confirm->isAsking(), "after the browser closes, a load over an edit asks again");
                confirm->finish (false);
                settle (100);
            }

            pages->setAsksBeforeReplacingEdits (false);
            processor.loadFactoryPreset (0);
            settle (100);
        }
        else
        {
            expect (false, "the editor has the confirm overlay and the next button");
        }
    }
}

void uiGroup_PIANO_ROLL (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 4 (S9): the clip piano roll's grid menu, velocity lane,
    // selection and copy / paste / duplicate / nudge keys, each edit one
    // undo step. A 400 x 200 roll on a 2-bar clip, reached through its own
    // geometry (beat to x, note to y, velocity to y).
    {
        const auto event = [] (juce::Component& component, juce::Point<float> position, bool dragged, int clicks,
                               juce::ModifierKeys mods)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, mods,
                                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(),
                                     position, juce::Time::getCurrentTime(), clicks, dragged);
        };
        const auto gesture = [&event] (juce::Component& component, juce::Point<float> from, juce::Point<float> to,
                                       int clicks = 1, juce::ModifierKeys mods = {})
        {
            component.mouseDown (event (component, from, false, clicks, mods));
            component.mouseDrag (event (component, to, true, clicks, mods));
            component.mouseUp (event (component, to, true, clicks, mods));
        };
        const auto command = [] (char letter) { return juce::KeyPress (letter, juce::ModifierKeys::commandModifier, 0); };
        const auto starts = [] (const Clip& clip)
        {
            juce::StringArray list;
            for (const auto& n : clip.notes)
                list.add (juce::String (n.start, 3) + "/" + juce::String (n.note) + "/" + juce::String (n.velocity));
            return list.joinIntoString (" ");
        };

        processor.loadFactoryPreset (0);
        settle (100);
        ClipEditor roll (processor, juce::Colours::orange);
        roll.setSize (400, 200);
        // Review 8 (S8 speed table): a new roll opens in DRAW. The selection
        // tests below run with it off.
        expect (roll.isDrawMode(), "the clip roll opens with DRAW on");
        roll.setDrawMode (false);
        const auto x = [&roll] (float beat) { return roll.xForBeat (beat); };
        const auto gridY = [&roll] (float fraction) { return roll.getGridArea().getY() + roll.getGridArea().getHeight() * fraction; };

        // GRID: 1/32 puts a double-clicked note on 32nds (1/16 would give
        // 2.5 here); the double-click has no drag, so the note keeps its
        // length (the drag-to-lengthen gesture is tested in the step 22 block).
        roll.setGrid (3);
        clearHistory();
        gesture (roll, { x (2.64f), 100.0f }, { x (2.64f), 100.0f }, 2);
        auto steps = undoSteps();
        auto shown = processor.getClipState().getClip (0);
        expect (roll.getGrid() == 3 && shown.notes.size() == 1 && std::abs (shown.notes[0].start - 2.625f) < 1.0e-4f
                    && shown.notes[0].velocity == 100 && steps.size() == 1,
                "the clip grid set to 1/32 snaps a double-clicked note to 32nds, in one undo step (" + starts (shown) + ")");
        expect (std::abs (ClipEditor::gridBeats (5) - 1.0f / 3.0f) < 1.0e-5f && std::abs (ClipEditor::gridBeats (3) - 0.125f) < 1.0e-6f
                    && ClipEditor::numGrids == 8 && ClipEditor::defaultGrid == 2,
                "the clip grid has 1/4 to 1/32 straight and triplet, 1/16 by default");

        // Velocity: a drag in the lane sets the bar under it; new notes take it.
        clearHistory();
        gesture (roll, { x (2.625f) + 1.0f, roll.yForVelocity (30) }, { x (2.625f) + 1.0f, roll.yForVelocity (118) });
        steps = undoSteps();
        const auto velocity = processor.getClipState().getClip (0).notes[0].velocity;
        processor.getUndoManager().undo();
        const auto undoneVelocity = processor.getClipState().getClip (0).notes[0].velocity;
        processor.getUndoManager().redo();
        roll.reload (false);
        expect (velocity > 110 && velocity < 127 && steps.size() == 1 && steps[0] == "Clip velocity" && undoneVelocity == 100,
                "dragging a clip note's velocity bar sets it (" + juce::String (velocity) + ") in one undo step; undo puts back 100");
        gesture (roll, { x (5.1f), 100.0f }, { x (5.1f), 100.0f }, 2);
        shown = processor.getClipState().getClip (0);
        expect (shown.notes.size() == 2 && shown.notes[1].velocity == velocity && std::abs (shown.notes[1].start - 5.0f) < 1.0e-4f,
                "a note added after a velocity drag takes that velocity (" + starts (shown) + ")");

        // A single click on empty space no longer writes a note.
        clearHistory();
        gesture (roll, { x (6.6f), 60.0f }, { x (6.6f), 60.0f });
        expect (processor.getClipState().getClip (0).notes.size() == 2 && undoSteps().isEmpty() && roll.getNumSelected() == 0,
                "a single click on an empty clip row writes nothing and clears the selection");

        // Rubber band: notes on beats 0-3 (60, 62, 64, 65); a band over
        // beats 1.2-2.5 picks the second and third; Delete removes them.
        {
            Clip four;
            four.bars = 2;
            for (const auto& [start, note] : { std::pair<float, int> { 0.0f, 60 }, { 1.0f, 62 }, { 2.0f, 64 }, { 3.0f, 65 } })
                four.notes.push_back ({ start, 1.0f, note, 100 });
            processor.getClipState().setClip (0, four);
            processor.clipsEdited();
        }
        roll.setGrid (ClipEditor::defaultGrid);
        roll.reload (true);
        clearHistory();
        gesture (roll, { x (1.2f), gridY (0.05f) }, { x (2.5f), gridY (0.95f) });
        const auto banded = roll.getNumSelected();
        const auto bandSteps = undoSteps().size();
        roll.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
        steps = undoSteps();
        shown = processor.getClipState().getClip (0);
        expect (banded == 2 && bandSteps == 0 && shown.notes.size() == 2 && shown.notes[0].note == 60 && shown.notes[1].note == 65
                    && steps.size() == 1 && steps[0] == "Delete clip notes",
                "a rubber band over two clip notes selects them (no undo step) and Delete removes both in one step (" + starts (shown) + ")");
        processor.getUndoManager().undo();
        expect (processor.getClipState().getClip (0).notes.size() == 4, "undoing the delete brings both notes back");

        // A click picks one note, shift-click adds one; neither edits.
        roll.reload (false);
        const auto noteY = [&roll] (int note) { return roll.yForNote (note); };
        clearHistory();
        gesture (roll, { x (0.5f), noteY (60) }, { x (0.5f), noteY (60) });
        gesture (roll, { x (3.5f), noteY (65) }, { x (3.5f), noteY (65) }, 1, juce::ModifierKeys::shiftModifier);
        const auto shiftPicked = roll.getNumSelected();
        gesture (roll, { x (1.5f), noteY (62) }, { x (1.5f), noteY (62) });
        expect (shiftPicked == 2 && roll.getNumSelected() == 1 && undoSteps().isEmpty(),
                "clicking a clip note selects it, shift-click adds one, neither changes the clip");

        // Copy and paste after the selection; duplicate; each one step.
        clearHistory();
        roll.keyPressed (command ('a'));
        const auto all = roll.getNumSelected();
        roll.keyPressed (command ('c'));
        roll.keyPressed (command ('v'));
        steps = undoSteps();
        shown = processor.getClipState().getClip (0);
        const auto pasted = shown.notes.size() == 8 && std::abs (shown.notes[4].start - 4.0f) < 1.0e-4f
                         && std::abs (shown.notes[7].start - 7.0f) < 1.0e-4f;
        expect (all == 4 && pasted && steps.size() == 1 && steps[0] == "Paste clip notes" && roll.getNumSelected() == 4,
                "Ctrl+A, Ctrl+C, Ctrl+V pastes the four notes after them in one undo step, selected (" + starts (shown) + ")");
        processor.getUndoManager().undo();
        roll.reload (false);
        clearHistory();
        roll.keyPressed (command ('a'));
        roll.keyPressed (juce::KeyPress (juce::KeyPress::leftKey)); // already at the start: stays
        roll.keyPressed (command ('d'));
        steps = undoSteps();
        shown = processor.getClipState().getClip (0);
        expect (shown.notes.size() == 8 && std::abs (shown.notes[4].start - 4.0f) < 1.0e-4f && steps.size() == 1
                    && steps[0] == "Duplicate clip notes",
                "Ctrl+D duplicates the selection after itself in one undo step ('" + steps.joinIntoString ("', '") + "')");
        processor.getUndoManager().undo();
        expect (processor.getClipState().getClip (0).notes.size() == 4, "undoing the duplicate leaves the four notes");

        // Arrows nudge: up a semitone, shift+right a bar.
        roll.reload (false);
        clearHistory();
        roll.keyPressed (command ('a'));
        roll.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
        roll.keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys::shiftModifier, 0));
        steps = undoSteps();
        shown = processor.getClipState().getClip (0);
        expect (shown.notes.size() == 4 && shown.notes[0].note == 61 && std::abs (shown.notes[0].start - 4.0f) < 1.0e-4f
                    && steps.size() == 2 && steps[0] == "Nudge clip notes",
                "up nudges the selected notes a semitone and shift+right a bar, a step each (" + starts (shown) + ")");

        // Ctrl+wheel zooms in time; the plain wheel still scrolls pitch.
        juce::MouseWheelDetails wheel {};
        wheel.deltaY = 0.5f;
        roll.mouseWheelMove (event (roll, { x (2.0f), 80.0f }, false, 0, juce::ModifierKeys::commandModifier), wheel);
        const auto zoomed = roll.getZoom();
        roll.mouseWheelMove (event (roll, { x (2.0f), 80.0f }, false, 0, {}), wheel);
        expect (zoomed > 1.1f && std::abs (roll.getZoom() - zoomed) < 1.0e-6f,
                "Ctrl+wheel zooms the clip roll in time, the wheel alone doesn't");

        // A touchpad's stream of small deltas scrolls as far as one mouse notch.
        {
            const auto before = roll.getLowNote();
            roll.mouseWheelMove (event (roll, { x (2.0f), 80.0f }, false, 0, {}), wheel);
            const auto notch = roll.getLowNote() - before;
            const auto start = roll.getLowNote();
            juce::MouseWheelDetails tiny {};
            tiny.deltaY = -0.025f;
            tiny.isSmooth = true;
            for (int i = 0; i < 20; ++i)
                roll.mouseWheelMove (event (roll, { x (2.0f), 80.0f }, false, 0, {}), tiny);
            const auto moved = roll.getLowNote() - start;
            expect (notch >= 2 && notch <= 4 && std::abs (moved + notch) <= 1,
                    "twenty tiny touchpad deltas scroll the clip roll as far as one mouse notch ("
                        + juce::String (moved) + " vs " + juce::String (notch) + ")");
        }
        // Review 6 (V5-22, S6-10): a keyboard column left of the grid and a
        // bar ruler over it; zoom buttons' calls; QUANTISE snaps starts to
        // the grid in one undo step (every note with none selected).
        roll.zoomToFit();
        roll.zoomBy (2.0f);
        const auto zoomedIn = roll.getZoom();
        roll.zoomToFit();
        expect (roll.getKeysArea().getWidth() >= 40.0f && roll.getKeysArea().getRight() <= roll.getGridArea().getX()
                    && roll.getRulerArea().getBottom() <= roll.getGridArea().getY() && roll.getRulerArea().getHeight() >= 16.0f,
                "the clip roll has a keyboard column left of the grid and a ruler over it");
        expect (std::abs (zoomedIn - 2.0f) < 1.0e-4f && roll.getZoom() == 1.0f,
                "the roll's zoom in doubles the zoom and FIT shows the whole clip again");
        {
            Clip loose;
            loose.bars = 2;
            loose.notes.push_back ({ 0.1f, 0.5f, 60, 100 });
            loose.notes.push_back ({ 1.37f, 0.5f, 62, 100 });
            processor.getClipState().setClip (0, loose);
            processor.clipsEdited();
        }
        roll.reload (true);
        clearHistory();
        const auto moved = roll.quantise();
        steps = undoSteps();
        shown = processor.getClipState().getClip (0);
        expect (moved && shown.notes.size() == 2 && std::abs (shown.notes[0].start) < 1.0e-4f && std::abs (shown.notes[1].start - 1.25f) < 1.0e-4f
                    && steps.size() == 1 && steps[0] == "Quantise clip notes" && ! roll.quantise(),
                "QUANTISE puts every note's start on the 1/16 grid in one undo step, and then has nothing to do (" + starts (shown) + ")");

        // Review 7 (S7-18): DRAW. A click on empty space places a note (no
        // double-click), a drag paints a run along the grid; each one undo
        // step. D toggles it.
        {
            Clip empty;
            empty.bars = 2;
            processor.getClipState().setClip (0, empty);
            processor.clipsEdited();
        }
        roll.reload (true);
        roll.setGrid (ClipEditor::defaultGrid);
        roll.keyPressed (juce::KeyPress ('d'));
        const auto drawOn = roll.isDrawMode();
        clearHistory();
        gesture (roll, { x (0.1f), roll.yForNote (60) }, { x (0.1f), roll.yForNote (60) });
        steps = undoSteps();
        shown = processor.getClipState().getClip (0);
        expect (drawOn && shown.notes.size() == 1 && std::abs (shown.notes[0].start) < 1.0e-4f && shown.notes[0].note == 60
                    && steps.size() == 1,
                "DRAW (the D key) places a note with a single click, in one undo step (" + starts (shown) + ")");
        clearHistory();
        gesture (roll, { x (2.1f), roll.yForNote (62) }, { x (5.5f), roll.yForNote (62) });
        steps = undoSteps();
        shown = processor.getClipState().getClip (0);
        expect (shown.notes.size() == 5 && steps.size() == 1 && steps[0] == "Draw clip notes",
                "a drag in DRAW paints a run of notes on beats 2-5 in one undo step (" + starts (shown) + ")");
        // Step 24: the spray lands where the pointer went. A single jump
        // across the grid gives each cell the pitch the pointer had where it
        // crossed it (not all the last one); a pointer above the roll keeps
        // the top row; a spray across notes that are already there doesn't
        // stack a second note on them.
        {
            const auto kept = processor.getClipState().getClip (0);
            Clip blank;
            blank.bars = 2;
            const auto freshRoll = [&] (const Clip& clip)
            {
                processor.getClipState().setClip (0, clip);
                processor.clipsEdited();
                roll.reload (true);
            };
            const auto sorted = [&] (Clip clip)
            {
                std::sort (clip.notes.begin(), clip.notes.end(), [] (const ClipNote& a, const ClipNote& b) { return a.start < b.start; });
                return clip;
            };

            freshRoll (blank);
            gesture (roll, { x (2.1f), roll.yForNote (58) }, { x (5.5f), roll.yForNote (64) });
            auto sprayed = sorted (processor.getClipState().getClip (0));
            auto rising = sprayed.notes.size() == 4 && sprayed.notes.front().note == 58;
            for (size_t i = 1; i < sprayed.notes.size(); ++i)
                rising = rising && sprayed.notes[i].note > sprayed.notes[i - 1].note && sprayed.notes[i].note <= 64;
            expect (rising && sprayed.notes.back().note >= 62,
                    "a spray dragged in one jump from note 58 up to 64 puts each cell on the pitch the pointer crossed it at (" + starts (sprayed) + ")");

            freshRoll (blank);
            gesture (roll, { x (1.1f), roll.getGridArea().getY() + 0.5f }, { x (3.5f), roll.getGridArea().getY() + 0.5f });
            const auto top = processor.getClipState().getClip (0).notes.front().note;
            freshRoll (blank);
            gesture (roll, { x (1.1f), roll.getGridArea().getY() + 0.5f }, { x (3.5f), roll.getGridArea().getY() - 80.0f });
            sprayed = processor.getClipState().getClip (0);
            auto kept_in_view = ! sprayed.notes.empty();
            for (const auto& n : sprayed.notes)
                kept_in_view = kept_in_view && n.note <= top;
            expect (kept_in_view, "a spray whose pointer leaves the roll at the top stays on the top row (" + starts (sprayed) + ")");

            Clip busy = blank;
            busy.notes.push_back ({ 3.0f, 0.25f, 60, 100 });
            busy.notes.push_back ({ 4.0f, 0.25f, 60, 100 });
            freshRoll (busy);
            gesture (roll, { x (1.1f), roll.yForNote (60) }, { x (6.5f), roll.yForNote (60) });
            sprayed = processor.getClipState().getClip (0);
            auto stacked = false;
            for (size_t i = 0; i < sprayed.notes.size(); ++i)
                for (size_t j = i + 1; j < sprayed.notes.size(); ++j)
                    stacked = stacked || (sprayed.notes[i].note == sprayed.notes[j].note
                                          && std::abs (sprayed.notes[i].start - sprayed.notes[j].start) < 1.0e-3f);
            expect (! stacked && sprayed.notes.size() > 2, "a spray across notes that are already there doesn't stack a second one on them (" + starts (sprayed) + ")");

            freshRoll (kept);
        }

        roll.keyPressed (juce::KeyPress ('d'));
        clearHistory();
        gesture (roll, { x (7.1f), roll.yForNote (64) }, { x (7.1f), roll.yForNote (64) });
        expect (! roll.isDrawMode() && processor.getClipState().getClip (0).notes.size() == 5 && undoSteps().isEmpty(),
                "with DRAW off a single click on empty space writes nothing again");

        // Step 22: a double-click on empty space adds a note; holding the
        // second click and dragging lengthens it; a plain double-click keeps
        // the length last used. DRAW still places one note per click. The
        // clip is restored afterwards, so the tests below see the same notes.
        {
            roll.reload (false);
            clearHistory();
            const auto saved = processor.getClipState().getClip (0);
            Clip blank;
            blank.bars = saved.bars;
            processor.getClipState().setClip (0, blank);
            processor.clipsEdited();
            roll.reload (true);
            clearHistory();
            gesture (roll, { x (6.0f), noteY (62) }, { x (7.5f), noteY (62) }, 2);
            const auto lengthened = processor.getClipState().getClip (0);
            const auto added = lengthened.notes.empty() ? ClipNote {} : lengthened.notes.back();
            const auto addedSteps = undoSteps();
            gesture (roll, { x (5.0f), noteY (62) }, { x (5.0f), noteY (62) }, 2);
            const auto plain = processor.getClipState().getClip (0);
            ClipNote plainNote {};
            for (const auto& n : plain.notes)
                if (std::abs (n.start - 5.0f) < 1.0e-4f)
                    plainNote = n;
            roll.setDrawMode (true);
            gesture (roll, { x (7.0f), noteY (60) }, { x (7.0f), noteY (60) });
            roll.setDrawMode (false);
            const auto drawn = processor.getClipState().getClip (0);
            ClipNote drawnNote {};
            for (const auto& n : drawn.notes)
                if (std::abs (n.start - 7.0f) < 1.0e-4f)
                    drawnNote = n;
            const auto allSteps = undoSteps();
            expect (lengthened.notes.size() == 1 && added.note == 62 && std::abs (added.start - 6.0f) < 1.0e-4f
                        && added.length > 1.5f && added.length < 2.5f && addedSteps.size() == 1,
                    "a double-click then drag adds a note at the click and lengthens it by the drag (" + starts (lengthened) + ")");
            expect (plain.notes.size() == 2 && plainNote.note == 62 && std::abs (plainNote.start - 5.0f) < 1.0e-4f
                        && std::abs (plainNote.length - added.length) < 1.0e-6f,
                    "a plain double-click adds a note at the length last used (" + starts (plain) + " lengthened " + juce::String (added.length) + ")");
            expect (drawn.notes.size() == 3 && drawnNote.note == 60 && std::abs (drawnNote.start - 7.0f) < 1.0e-4f
                        && allSteps.size() == 3 && allSteps[0] == "Draw clip notes" && allSteps[1] == "Add clip note" && allSteps[2] == "Add clip note",
                    "DRAW places one note per click, one undo step, and the two double-click edits stay separate steps (" + starts (drawn) + ")");
            processor.getClipState().setClip (0, saved);
            processor.clipsEdited();
            roll.reload (false);
            clearHistory();
        }

        // Review 7 (S7-17): the roll opens fitted to the clip's notes, counts
        // the notes scrolled out of view at its edges, and a click on a
        // count brings them back.
        {
            Clip wide;
            wide.bars = 2;
            for (const auto note : { 55, 57, 60, 62, 64, 67 })
                wide.notes.push_back ({ (float) (note % 8), 0.5f, note, 100 });
            processor.getClipState().setClip (0, wide);
            processor.clipsEdited();
        }
        roll.reload (true);
        const auto fitted = roll.notesOutOfView();
        juce::MouseWheelDetails scroll {};
        scroll.deltaY = 1.0f;
        for (int i = 0; i < 12; ++i)
            roll.mouseWheelMove (event (roll, { x (2.0f), 80.0f }, false, 0, {}), scroll);
        const auto scrolled = roll.notesOutOfView();
        const auto low = roll.getLowestShownNote();
        gesture (roll, { roll.getGridArea().getRight() - 26.0f, roll.getGridArea().getBottom() - 12.0f },
                 { roll.getGridArea().getRight() - 26.0f, roll.getGridArea().getBottom() - 12.0f });
        expect (fitted.first == 0 && fitted.second == 0, "the clip roll opens with every note of the clip in view");
        expect (scrolled.second > 0 && roll.getLowestShownNote() < low && roll.notesOutOfView().second < scrolled.second,
                "scrolled up, the roll counts the notes below the view (" + juce::String (scrolled.second)
                    + "), and a click on that count scrolls down to them");

        processor.loadFactoryPreset (0);
        settle (100);
    }
}

void uiGroup_INIT_MODULES (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Init puts the modules back to two oscillators and three envelopes and LFOs, and forgets the old patch's
    // macro CCs and drawn LFO shapes.
    {
        using M = IlanaSynthAudioProcessor::Module;
        processor.addOscillator (4);
        processor.setRevealed (M::Lfo, 8, true);
        processor.setRevealed (M::Envelope, 9, true);
        processor.setLfoCustomPoint (0, 3, 0.9f);
        processor.loadFactoryPreset (0);
        settle (100);
        expect (! processor.isOscillatorShown (4) && ! processor.isLfoShown (8)
                    && ! processor.isRevealed (M::Envelope, 9) && processor.isOscillatorShown (1) && ! processor.isOscillatorShown (2),
                "Init shows two oscillators and three LFOs and envelopes again");
        expect (std::abs (processor.getLfoCustomPoint (0, 3) - 0.9f) > 0.1f && processor.getMacroCc (0) == 20,
                "Init resets drawn LFO shapes and macro CCs");
    }
}

void uiGroup_INIT_STARTERS (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // M7.0: on Init, a matrix starter adds its routing and an FX quick-add
    // fills the first slot.
    {
        processor.loadFactoryPreset (0);
        pages->showPage ("MAIN");
        settle (100);
        // (Init carries the two cutoff routes that replaced its ENV AMOUNT knob; the starters want an empty matrix.)
        for (int slot = 0; slot < Mod::maxSlots; ++slot)
            processor.clearModSlot (slot);
        pages->showPage ("MATRIX");
        settle (300);

        const auto findButton = [&editor] (const juce::String& text) -> juce::TextButton*
        {
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*editor, buttons);
            for (auto* button : buttons)
                if (button->getButtonText() == text && button->isVisible())
                    return button;
            return nullptr;
        };

        auto* starter = findButton ("MOD WHEEL  >  Vibrato");
        expect (starter != nullptr, "an empty matrix shows the starter routings");

        if (starter != nullptr)
        {
            starter->triggerClick();
            settle (300);
            const auto slot = processor.readModSlot (0);
            expect (slot.source == Mod::Source::Lfo2 && slot.aux == Mod::Source::ModWheel
                        && slot.destination == (int) Mod::Destination::Osc1Pitch
                        && processor.readModSlot (2).destination == (int) Mod::Destination::SubPitch,
                    "WHEEL > VIBRATO routes LFO 2 via the wheel to OSC 1-3 pitch");
            expect (findButton ("MOD WHEEL  >  Vibrato") == nullptr, "the starters hide once something is routed");
        }

        pages->showPage ("FX");
        settle (300);
        auto* tapeStop = findButton ("TAPE STOP");
        expect (tapeStop != nullptr, "the empty rack offers every effect, grouped");

        if (tapeStop != nullptr)
        {
            tapeStop->triggerClick();
            settle (300);
            expect ((int) processor.apvts.getRawParameterValue ("fx_slot1")->load() == 17, "a quick-add button fills slot 1");
        }

        processor.loadFactoryPreset (0);
        settle (200);
    }
}

void uiGroup_GENERATIVE (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // M7.1: the Generative card switches between ARP, EUCLID and PROB SEQ.
    {
        pages->showPage ("ARP/SEQ");
        settle (100);
        std::vector<CardTabs*> cardTabs;
        findAll<CardTabs> (*editor, cardTabs);
        CardTabs* engineTabs = nullptr;
        for (auto* candidate : cardTabs)
            if (candidate->getNames().contains ("PROB SEQ"))
                engineTabs = candidate;

        expect (engineTabs != nullptr, "the ARP/SEQ page has a Generative card with ARP, EUCLID and PROB SEQ");

        if (engineTabs != nullptr)
        {
            pages->showPage ("ARP/SEQ");
            engineTabs->setSelected (1, true);
            settle (200);

            const auto visibleWith = [&editor] (const juce::String& id)
            {
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*editor, knobs);
                for (auto* knob : knobs)
                    if (knob->getParameterId() == id)
                        return knob->isVisible();
                return false;
            };

            expect (visibleWith ("euc_hits") && ! visibleWith ("arp_gate") && ! visibleWith ("pseq_length"),
                    "the EUCLID tab shows only Euclid's controls");
            engineTabs->setSelected (2, true);
            settle (200);
            expect (visibleWith ("pseq_length") && ! visibleWith ("euc_hits"), "the PROB SEQ tab shows the sequencer");
            expect (visibleWith ("spray_strum_time"), "the GENERATE card has STRUM TIME");
            engineTabs->setSelected (0, true);

            // Review 6 (I6-15): a tab lights while its engine is on, and the
            // note path says PROB SEQ plays instead of the ARP.
            const auto set = [&processor] (const juce::String& id, float value)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            set ("arp_on", 1.0f);
            set ("pseq_on", 1.0f);
            settle (400);
            auto* chain = findChild<NoteChainView> (*editor);
            juce::String note;
            const auto stages = chain != nullptr ? chain->stages (note) : std::vector<NoteChainView::Stage> {};
            auto arpWaits = false, seqPlays = false;
            for (const auto& stage : stages)
            {
                arpWaits = arpWaits || (stage.name == "ARP" && stage.waiting);
                seqPlays = seqPlays || (stage.name == "PROB SEQ" && ! stage.waiting);
            }
            expect (engineTabs->isTabOn (0) && engineTabs->isTabOn (2) && ! engineTabs->isTabOn (1) && ! engineTabs->isTabOn (3),
                    "the ARP and PROB SEQ tabs light while they are on, EUCLID's and CLIP's don't");
            expect (chain != nullptr && visibleInTree (chain) && arpWaits && seqPlays && note == "PROB SEQ replaces the ARP.",
                    "the note path shows PROB SEQ playing and the ARP waiting ('" + note + "')");

            // S12-4: the chain lists the engines in the tab order, and each opens its tab.
            {
                set ("euc_on", 1.0f);
                set ("euc_target", 0.0f);
                set ("clip_on", 1.0f);
                settle (300);
                juce::String orderNote;
                auto last = -1, opens = 0;
                auto ordered = true;
                for (const auto& stage : chain->stages (orderNote))
                    if (stage.engine >= 0)
                    {
                        ordered = ordered && stage.engine >= last;
                        last = stage.engine;
                        ++opens;
                    }
                expect (ordered && opens >= 3, "the note path lists ARP, EUCLID, PROB SEQ, CLIP in the tab order, each opening its tab (S12-4)");
                set ("euc_on", 0.0f);
                set ("clip_on", 0.0f);
                settle (200);
            }

            // The arp's settings step back while PROB SEQ plays instead.
            KnobControl* arpGate = nullptr;
            {
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*editor, knobs);
                for (auto* knob : knobs)
                    if (knob->getParameterId() == "arp_gate")
                        arpGate = knob;
            }
            expect (arpGate != nullptr && arpGate->getAlpha() < 0.99f, "the ARP's GATE dims while PROB SEQ plays instead");
            set ("pseq_on", 0.0f);
            settle (300);
            expect (arpGate != nullptr && arpGate->getAlpha() > 0.99f && ! engineTabs->isTabOn (2), "and lights again once PROB SEQ is off");

            // Review 7 (V7-11, I7-36): each engine's power is the switch in
            // its tab; the engines' rows have no switch of their own. A click
            // on a tab's switch toggles that engine (one undo step) without
            // changing the tab shown; a click on its name shows it.
            {
                std::vector<ToggleControl*> toggles;
                findAll<ToggleControl> (*editor, toggles);
                juce::StringArray rowSwitches;
                for (auto* toggle : toggles)
                    for (const auto* id : { "arp_on", "euc_on", "pseq_on", "clip_on" })
                        if (visibleInTree (toggle) && toggle->getButton().getTooltip().startsWith (processor.apvts.getParameter (id)->getName (64)))
                            rowSwitches.add (id);
                expect (rowSwitches.isEmpty(), "the PATTERN engines have no switch in their rows (" + rowSwitches.joinIntoString (" ") + ")");

                const auto click = [] (juce::Component& component, juce::Point<float> position)
                {
                    const juce::MouseEvent down (juce::Desktop::getInstance().getMainMouseSource(), position, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                 &component, &component, juce::Time::getCurrentTime(), position, juce::Time::getCurrentTime(),
                                                 1, false);
                    component.mouseDown (down);
                    component.mouseUp (down);
                };
                engineTabs->setSelected (0, true);
                processor.getUndoManager().clearUndoHistory();
                click (*engineTabs, engineTabs->switchBounds (1).getCentre());
                settle (100);
                const auto steps = processor.getUndoManager().getUndoDescriptions();
                const auto euclidOn = processor.apvts.getRawParameterValue ("euc_on")->load() > 0.5f;
                expect (euclidOn && engineTabs->isTabOn (1) && engineTabs->getSelected() == 0 && steps.size() == 1
                            && steps[0] == "EUCLID on",
                        "the switch in EUCLID's tab turns EUCLID on in one undo step and leaves ARP shown ('"
                            + steps.joinIntoString ("', '") + "')");
                click (*engineTabs, engineTabs->switchBounds (1).getCentre());
                settle (100);
                expect (processor.apvts.getRawParameterValue ("euc_on")->load() < 0.5f && ! engineTabs->isTabOn (1),
                        "a second click on the switch turns EUCLID off");
                click (*engineTabs, { engineTabs->switchBounds (2).getRight() + 20.0f, engineTabs->switchBounds (2).getCentreY() });
                settle (100);
                expect (engineTabs->getSelected() == 2 && processor.apvts.getRawParameterValue ("pseq_on")->load() < 0.5f,
                        "a click on PROB SEQ's name shows it without switching it on");
                {
                    std::vector<KnobControl*> knobs;
                    findAll<KnobControl> (*editor, knobs);
                    juce::String label;
                    for (auto* knob : knobs)
                        if (knob->getParameterId() == "pseq_length")
                            label = knob->getLabelText();
                    expect (label == "STEPS", "PROB SEQ's step count is STEPS, as the arp's (I7-29: '" + label + "')");
                }

                // An engine shown while off says so in the header (no plate
                // over its display: S7-19).
                auto* chain = findChild<NoteChainView> (*editor);
                juce::String offNote;
                if (chain != nullptr)
                    chain->stages (offNote);
                expect (offNote.startsWith ("PROB SEQ is off"), "the PATTERN header says the shown engine is off ('" + offNote + "')");

                // S7-37: the note follows the chain when EUCLID drives the clip.
                set ("arp_on", 0.0f);
                set ("euc_on", 1.0f);
                set ("clip_on", 1.0f);
                engineTabs->setSelected (3, true);
                settle (200);
                juce::String clipNote;
                if (chain != nullptr)
                    chain->stages (clipNote);
                expect (clipNote == "EUCLID's hits trigger the clip.",
                        "with EUCLID and CLIP on the header says how they combine ('" + clipNote + "')");
                set ("euc_on", 0.0f);
                set ("clip_on", 0.0f);
                set ("arp_on", 1.0f);
                engineTabs->setSelected (0, true);
                settle (200);
            }

            // The arp's lanes (S6-23): a drag down VELOCITY over steps 3-5
            // draws them in one undo step; the defaults leave every step as
            // it was.
            if (auto* lanes = findChild<ArpLanesEditor> (*editor); lanes != nullptr && visibleInTree (lanes))
            {
                const auto event = [] (juce::Component& component, juce::Point<float> position, bool dragged)
                {
                    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                             &component, &component, juce::Time::getCurrentTime(), position, juce::Time::getCurrentTime(),
                                             1, dragged);
                };
                const auto lane = lanes->laneArea (ArpLanesEditor::velocity);
                const auto low = lane.getBottom() - lane.getHeight() * 0.25f;
                processor.apvts.copyState();
                processor.getUndoManager().clearUndoHistory();
                lanes->mouseDown (event (*lanes, { lanes->cellCentre (0, 2).x, low }, false));
                lanes->mouseDrag (event (*lanes, { lanes->cellCentre (0, 4).x, low }, true));
                lanes->mouseUp (event (*lanes, { lanes->cellCentre (0, 4).x, low }, true));
                processor.apvts.copyState();
                const auto steps = processor.getUndoManager().getUndoDescriptions();
                const auto read = [&processor] (const char* id) { return juce::roundToInt (processor.apvts.getRawParameterValue (id)->load()); };
                expect (read ("arp_vel3") < 50 && read ("arp_vel4") < 50 && read ("arp_vel5") < 50 && read ("arp_vel6") == 100
                            && read ("arp_vel2") == 100 && steps.size() == 1 && steps[0] == "Arp velocity",
                        "dragging across the arp's VELOCITY lane draws steps 3-5 (" + juce::String (read ("arp_vel4"))
                            + ") in one undo step ('" + steps.joinIntoString ("', '") + "')");
                lanes->mouseDoubleClick (event (*lanes, lanes->cellCentre (0, 3), false));
                expect (read ("arp_vel4") == 100, "a double-click puts an arp step back to its defaults");

                // A click on a step number sets how many steps loop.
                lanes->mouseDown (event (*lanes, lanes->cellCentre (-1, 11), false));
                lanes->mouseUp (event (*lanes, lanes->cellCentre (-1, 11), false));
                expect (read ("arp_steps") == 12, "clicking step 12's number loops 12 arp steps");
                // S7-19: a colour per lane.
                expect (lanes->laneColour (0) != lanes->laneColour (1) && lanes->laneColour (1) != lanes->laneColour (2)
                            && lanes->laneColour (0) != lanes->laneColour (2),
                        "the arp's VELOCITY, GATE and PITCH lanes each have their own colour");
                for (const auto* id : { "arp_steps", "arp_vel3", "arp_vel5" })
                    set (id, processor.apvts.getParameter (id)->convertFrom0to1 (processor.apvts.getParameter (id)->getDefaultValue()));

                // UI review 31, item 32: a right-click on a lane never draws (it opens the lane's tools); the tools
                // each take one undo step; Shift-drag draws a straight line and gives back the steps it leaves.
                {
                    const auto withMods = [] (juce::Component& component, juce::Point<float> position, juce::ModifierKeys mods, bool dragged)
                    {
                        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                 &component, &component, juce::Time::getCurrentTime(), position, juce::Time::getCurrentTime(),
                                                 1, dragged);
                    };
                    const auto velocityOf = [&processor] (int step) { return processor.apvts.getRawParameterValue ("arp_vel" + juce::String (step + 1))->load(); };
                    set ("arp_steps", 8.0f);
                    juce::String menuFor;
                    GenerativeWidgets::laneMenuProbe() = &menuFor; // (no real pop-up: Xvfb has no window manager)
                    lanes->mouseDown (withMods (*lanes, { lanes->cellCentre (0, 2).x, low }, juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier), false));
                    lanes->mouseUp (withMods (*lanes, { lanes->cellCentre (0, 2).x, low }, juce::ModifierKeys(), false));
                    const auto onBars = menuFor;
                    menuFor.clear();
                    lanes->mouseDown (withMods (*lanes, { lane.getX() + 10.0f, low }, juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier), false));
                    lanes->mouseUp (withMods (*lanes, { lane.getX() + 10.0f, low }, juce::ModifierKeys(), false));
                    expect (juce::roundToInt (velocityOf (2)) == 100 && onBars == "VEL" && menuFor == "VEL",
                            "a right-click on the VEL lane (its bars or its name) opens VEL's tools and draws nothing ('" + onBars + "', '" + menuFor + "')");

                    processor.getUndoManager().clearUndoHistory();
                    lanes->applyLaneTool (ArpLanesEditor::velocity, GenerativeWidgets::LaneTool::rampUp);
                    auto rising = true;
                    for (int step = 1; step < 8; ++step)
                        rising = rising && velocityOf (step) > velocityOf (step - 1);
                    const auto rampSteps = processor.getUndoManager().getUndoDescriptions();
                    expect (rising && juce::roundToInt (velocityOf (0)) == 1 && juce::roundToInt (velocityOf (7)) == 127
                                && juce::roundToInt (velocityOf (8)) == 100 && rampSteps.size() == 1,
                            "Ramp up on VEL rises 1..127 over the 8 looped steps, leaves step 9, in one undo step ('"
                                + rampSteps.joinIntoString ("', '") + "')");
                    lanes->applyLaneTool (ArpLanesEditor::velocity, GenerativeWidgets::LaneTool::shiftLeft);
                    expect (juce::roundToInt (velocityOf (7)) == 1 && juce::roundToInt (velocityOf (6)) == 127,
                            "Shift left moves VEL one step left, the first step round to the loop's end");
                    lanes->applyLaneTool (ArpLanesEditor::velocity, GenerativeWidgets::LaneTool::copy);
                    lanes->applyLaneTool (ArpLanesEditor::pitch, GenerativeWidgets::LaneTool::paste);
                    const auto pitchOf = [&processor] (int step) { return juce::roundToInt (processor.apvts.getRawParameterValue ("arp_pitch" + juce::String (step + 1))->load()); };
                    expect (pitchOf (6) == 12 && pitchOf (7) == -12, "Copy on VEL, Paste on PITCH: the lane's shape, in PITCH's range");
                    for (const auto lane : { ArpLanesEditor::velocity, ArpLanesEditor::pitch })
                        lanes->applyLaneTool (lane, GenerativeWidgets::LaneTool::reset);
                    expect (juce::roundToInt (velocityOf (6)) == 100 && pitchOf (6) == 0, "Reset puts a lane back to its defaults");

                    // Shift-drag from step 1 at the top to step 4 at the foot: a straight line falling over 1-4.
                    const auto bars = lane.withTrimmedBottom (14.0f);
                    const auto top = bars.getY() + 1.0f, foot = bars.getBottom() - 1.0f;
                    const auto shift = juce::ModifierKeys (juce::ModifierKeys::shiftModifier | juce::ModifierKeys::leftButtonModifier);
                    lanes->mouseDown (withMods (*lanes, { lanes->cellCentre (0, 0).x, top }, shift, false));
                    lanes->mouseDrag (withMods (*lanes, { lanes->cellCentre (0, 3).x, foot }, shift, true));
                    const auto falling = velocityOf (0) > velocityOf (1) && velocityOf (1) > velocityOf (2) && velocityOf (2) > velocityOf (3)
                                      && std::abs ((velocityOf (0) - velocityOf (1)) - (velocityOf (1) - velocityOf (2))) <= 2.0f;
                    lanes->mouseDrag (withMods (*lanes, { lanes->cellCentre (0, 1).x, foot }, shift, true));
                    lanes->mouseUp (withMods (*lanes, { lanes->cellCentre (0, 1).x, foot }, shift, true));
                    expect (falling && juce::roundToInt (velocityOf (2)) == 100 && juce::roundToInt (velocityOf (3)) == 100,
                            "Shift-drag on VEL draws a straight line and gives back the steps it leaves ("
                                + juce::String (velocityOf (0)) + ", " + juce::String (velocityOf (1)) + ")");
                    GenerativeWidgets::laneMenuProbe() = nullptr;
                    lanes->applyLaneTool (ArpLanesEditor::velocity, GenerativeWidgets::LaneTool::reset);
                    set ("arp_steps", processor.apvts.getParameter ("arp_steps")->convertFrom0to1 (processor.apvts.getParameter ("arp_steps")->getDefaultValue()));
                }

                // PROB SEQ's lanes have the same tools; a right-click draws nothing there either.
                if (auto* seqLanes = findChild<ProbSeqEditor> (*editor))
                {
                    const auto chanceOf = [&processor] (int step) { return processor.apvts.getRawParameterValue ("pseq_chance" + juce::String (step + 1))->load(); };
                    const auto position = juce::Point<float> (seqLanes->cellCentre (ProbSeqEditor::chance, 3).x, seqLanes->barsArea (ProbSeqEditor::chance).getBottom() - 4.0f);
                    const auto before = chanceOf (3);
                    juce::String menuFor;
                    GenerativeWidgets::laneMenuProbe() = &menuFor;
                    seqLanes->mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position,
                                                           juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                           seqLanes, seqLanes, juce::Time::getCurrentTime(), position, juce::Time::getCurrentTime(), 1, false));
                    GenerativeWidgets::laneMenuProbe() = nullptr;
                    expect (std::abs (chanceOf (3) - before) < 1.0e-6f && menuFor == "CHANCE",
                            "a right-click on PROB SEQ's CHANCE opens its tools and draws nothing ('" + menuFor + "')");
                    processor.getUndoManager().clearUndoHistory();
                    seqLanes->applyLaneTool (ProbSeqEditor::chance, GenerativeWidgets::LaneTool::rampDown);
                    const auto ramp = processor.getUndoManager().getUndoDescriptions();
                    expect (chanceOf (0) > 0.99f && chanceOf (1) < chanceOf (0) && ramp.size() == 1 && ramp[0] == "Ramp down CHANCE lane",
                            "Ramp down on PROB SEQ's CHANCE falls from 100 %, in one undo step ('" + ramp.joinIntoString ("', '") + "')");
                    seqLanes->applyLaneTool (ProbSeqEditor::chance, GenerativeWidgets::LaneTool::reset);
                }
            }
            else
            {
                expect (false, "the ARP tab shows the arp's step lanes");
            }
            set ("arp_on", 0.0f);

            // EXPAND (S6-10): the clip roll takes the page, GENERATE folds to
            // its title line; COLLAPSE brings it back.
            engineTabs->setSelected (3, true);
            settle (200);
            juce::TextButton* expandButton = nullptr;
            {
                std::vector<juce::TextButton*> buttons;
                findAll<juce::TextButton> (*editor, buttons);
                for (auto* button : buttons)
                    if (button->getButtonText() == "EXPAND" && visibleInTree (button))
                        expandButton = button;
            }
            auto* roll = findChild<ClipEditor> (*editor);
            expect (expandButton != nullptr && roll != nullptr, "the CLIP tab has the roll and an EXPAND button");

            if (expandButton != nullptr && roll != nullptr)
            {
                const auto before = roll->getHeight();
                expandButton->triggerClick();
                settle (200);
                const auto expanded = roll->getHeight();
                const auto generateHidden = ! visibleWith ("spray_count") && ! visibleWith ("spray_strum_time");
                const auto collapseText = expandButton->getButtonText();
                expandButton->triggerClick();
                settle (200);
                expect (expanded > before + 80 && generateHidden && collapseText == "COLLAPSE",
                        "EXPAND gives the clip roll the page (" + juce::String (before) + " -> " + juce::String (expanded)
                            + " px) and folds GENERATE away");
                expect (roll->getHeight() == before && visibleWith ("spray_count") && expandButton->getButtonText() == "EXPAND",
                        "COLLAPSE brings GENERATE back");
            }

            engineTabs->setSelected (0, true);
        }
    }
}

void uiGroup_LFO_CARD (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // M8.1: the LFO card for a simulated shape.
    {
        processor.loadFactoryPreset (0); // (Init: the patch the checks below were written on)
        // LFO 1's pop-up editor holds the whole panel (the MOD page's LFO card is gone).
        auto* lfoPopover = uiOpenSourcePopover (ctx, Mod::Source::Lfo1);
        juce::Component& lfoRoot = lfoPopover != nullptr ? *lfoPopover : static_cast<juce::Component&> (*editor);
        const auto knobFor = [&lfoRoot] (const juce::String& id) -> KnobControl*
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (lfoRoot, knobs);
            for (auto* knob : knobs)
                if (knob->getParameterId() == id)
                    return knob;
            return nullptr;
        };
        const auto setShape = [&processor] (int shape)
        {
            if (auto* parameter = processor.apvts.getParameter ("lfo1_shape"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) shape));
        };
        setShape (LfoSimShapes::Lorenz);
        settle (300);
        // The LFO's graph draws a simulated shape from the live simulation, not one fixed picture: its
        // pixels move between two moments.
        {
            std::vector<LfoDisplay*> graphs;
            findAll<LfoDisplay> (lfoRoot, graphs);
            LfoDisplay* graph = nullptr;
            for (auto* candidate : graphs)
                if (visibleInTree (candidate) && candidate->getWidth() > 0)
                    graph = candidate;
            if (graph != nullptr)
            {
                const auto grab = [&] { return graph->createComponentSnapshot (graph->getLocalBounds(), false, 1.0f); };
                const auto before = grab();
                settle (500);
                const auto after = grab();
                auto changed = 0;
                for (int y = 0; y < before.getHeight() && y < after.getHeight(); ++y)
                    for (int x = 0; x < before.getWidth() && x < after.getWidth(); ++x)
                        changed += before.getPixelAt (x, y) != after.getPixelAt (x, y) ? 1 : 0;
                expect (changed > 20, "a simulated LFO's graph moves with the live simulation (" + juce::String (changed) + " pixels changed in 0.5 s)");
            }
            else
                expect (false, "the LFO's pop-up has a graph");
        }
        auto* p1 = knobFor ("lfo1_p1");
        auto* p4 = knobFor ("lfo1_p4");
        auto* start = knobFor ("lfo1_phase");
        auto* smooth = knobFor ("lfo1_smooth");
        expect (p1 != nullptr && p1->isVisible() && p1->getLabelText() == "SIGMA", "Lorenz shows a SIGMA knob");
        expect (p1 != nullptr && p1->getSlider().getTextFromValue (0.5) == "10.00", "SIGMA reads 10.00 at the middle");
        expect (p4 != nullptr && ! p4->isVisible(), "Lorenz hides the knobs it doesn't use");
        expect (start != nullptr && ! start->isVisible() && smooth != nullptr && smooth->isVisible(),
                "simulated shapes show SMOOTH instead of START");
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (lfoRoot, buttons);
        auto fireShown = false;
        for (auto* button : buttons)
            fireShown = fireShown || (button->getButtonText() == "FIRE" && button->isVisible());
        expect (fireShown, "the card has a FIRE button");

        setShape (LfoSimShapes::Bounce);
        settle (300);
        expect (p1 != nullptr && p1->getLabelText() == "GRAVITY" && p4 != nullptr && p4->isVisible() && p4->getLabelText() == "DRAG",
                "Bounce names its knobs GRAVITY ... DRAG");
        setShape (0);
        settle (300);
        expect (p1 != nullptr && ! p1->isVisible() && smooth != nullptr && smooth->isVisible() && start != nullptr && start->isVisible(),
                "classic shapes keep START and gain SMOOTH");
        expect (Mod::getSourceNames().contains ("LFO 16 B"), "every LFO's output B is a mod source");

        // (UI review 8, I8-17: the MOD page's one panel grid, for every shape, went with that page; the pop-up has its own
        // compact layout.)
        {
            // (Combos and switches carry their parameter's name first in
            // their tooltips.)
            const auto findToggle = [&lfoRoot, &processor] (const juce::String& id) -> juce::Component*
            {
                std::vector<ParamBoundComponent*> pills;
                findAll<ParamBoundComponent> (lfoRoot, pills);
                for (auto* pill : pills)
                    if (visibleInTree (pill) && pill->getWidth() > 0 && pill->getParameterId() == id)
                        return pill;
                auto* parameter = processor.apvts.getParameter (id);
                std::vector<ToggleControl*> toggles;
                findAll<ToggleControl> (lfoRoot, toggles);
                for (auto* toggle : toggles)
                    if (parameter != nullptr && visibleInTree (toggle) && toggle->getWidth() > 0
                        && toggle->getButton().getTooltip().startsWith (parameter->getName (64)))
                        return toggle;
                return nullptr;
            };
            // I8-16: a simulated shape's caption follows TRIGGER.
            const auto setParam = [&processor] (const juce::String& id, float value)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            // I9-1 / I9-19: one trigger model. The switch says where it runs
            // (RETRIG on a plain shape, PER VOICE on a simulated one, whose
            // TRIGGER says when it restarts), and the caption says both in
            // the same words for every shape.
            const auto runSwitchText = [&]
            {
                if (auto* pill = dynamic_cast<ParamBoundComponent*> (findToggle ("lfo1_retrig")))
                    return pill->getDisplayText();
                auto* toggle = dynamic_cast<ToggleControl*> (findToggle ("lfo1_retrig"));
                return toggle != nullptr ? toggle->getButton().getButtonText() : juce::String();
            };
            const auto plainSwitch = runSwitchText();
            const auto plainCaption = LfoShapeMenu::runCaption (processor, 0);
            setShape (LfoSimShapes::Bounce);
            settle (150);
            const auto simSwitch = runSwitchText();
            setParam ("lfo1_trigger", 0.0f);
            const auto onNote = LfoShapeMenu::runCaption (processor, 0);
            setParam ("lfo1_trigger", 1.0f);
            const auto free = LfoShapeMenu::runCaption (processor, 0);
            setParam ("lfo1_trigger", 0.0f);
            setShape (0);
            expect (onNote.contains ("restarts on note") && free.contains ("runs free")
                        && onNote.startsWith ("shared") && plainCaption == juce::String::fromUTF8 ("shared \xc2\xb7 runs free")
                        && onNote.contains ("OUT 2") && onNote.length() < 48,
                    "an LFO's caption says where it runs and when it restarts, the same words for plain and simulated shapes ('"
                        + plainCaption + "' / '" + onNote + "' / '" + free + "')");
            expect (plainSwitch == "RETRIG" && simSwitch == "VOICE",
                    "the run switch reads RETRIG on a plain shape and VOICE on a simulated one, beside its TRIGGER ('" + plainSwitch
                        + "' / '" + simSwitch + "')");
            settle (200);
        }
        uiCloseSourcePopover (ctx);

        // I8-26: physics units read as units; I8-36: no two shapes share a
        // name in the SHAPE list.
        {
            const auto gravity = LfoSimInfo::text (LfoSimShapes::Bounce, 0, 0.5f), drag = LfoSimInfo::text (LfoSimShapes::Bounce, 3, 0.0f);
            expect (gravity.endsWith (juce::String::fromUTF8 ("m/s\xc2\xb2")) && drag.endsWith ("per s"),
                    "physics units read m/s squared and 'per s' (" + gravity + ", " + drag + ")");
            juce::StringArray shown;
            juce::String clash;
            auto* shapeParameter = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("lfo1_shape"));
            if (shapeParameter != nullptr)
                for (int shape = 0; shape < shapeParameter->choices.size(); ++shape)
                {
                    const auto name = LfoShapeMenu::displayName (shape, shapeParameter->choices[shape]);
                    if (shown.contains (name))
                        clash << name << " ";
                    shown.add (name);
                }
            expect (shapeParameter != nullptr && clash.isEmpty(), "every LFO shape has its own name in the SHAPE list (" + clash + ")");
        }
    }
}

void uiGroup_MSEG_CARD (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 8, I8-4 / S8-5 / V8-2: one MSEG. UI review 9, I9-2: an old
    // patch that routes a looping MSEG module loads with it drawn on an LFO
    // (SHAPE > MSEG, the same points and RATE) and its routes moved there, so
    // no MSEG card or chip shows. A route made to it later (or a one-shot
    // one) shows its card, in an LFO pastel, and MOVE TO LFO still moves it;
    // undo brings it back.
    {
        const auto msegSweep = names.indexOf ("MSEG Sweep");
        processor.loadFactoryPreset (msegSweep);
        pages->showPage ("MAIN");
        settle (400);
        {
            auto target = -1, fromLfo = 0;
            for (int slot = 0; slot < Mod::maxSlots; ++slot)
            {
                const auto routing = processor.readModSlot (slot);
                if (routing.destination != 0 && Mod::lfoIndexFor (routing.source) >= 0 && target < 0
                    && juce::roundToInt (processor.apvts.getRawParameterValue ("lfo" + juce::String (Mod::lfoIndexFor (routing.source) + 1) + "_shape")->load())
                           == IlanaSynthAudioProcessor::curveShape)
                    target = Mod::lfoIndexFor (routing.source);
            }
            for (int slot = 0; target >= 0 && slot < Mod::maxSlots; ++slot)
            {
                const auto routing = processor.readModSlot (slot);
                fromLfo += routing.destination != 0 && routing.source == Mod::lfoSourceFor (target) ? 1 : 0;
            }
            const auto curve = target >= 0 ? processor.getLfoCurve (target) : LfoCurve();
            auto pointsMatch = curve.points.size() == 5;
            for (int i = 0; pointsMatch && i < 4; ++i)
                pointsMatch = std::abs (curve.points[(size_t) i].y - processor.apvts.getRawParameterValue ("mseg_level" + juce::String (i + 1))->load()) < 1.0e-4f;
            const auto rate = target >= 0 ? processor.apvts.getRawParameterValue ("lfo" + juce::String (target + 1) + "_rate")->load() : 0.0f;
            expect (msegSweep >= 0 && ! modSourceInPatch (processor, Mod::Source::Mseg)
                        && target >= 0 && fromLfo == 2 && pointsMatch && processor.isLfoShown (target)
                        && std::abs (rate - processor.apvts.getRawParameterValue ("mseg_rate")->load()) < 1.0e-3f,
                    "MSEG Sweep loads with its MSEG drawn on LFO " + juce::String (target + 1)
                        + " (SHAPE > MSEG, its points and RATE) and both routes there; no MSEG chip");
            expect (modSourceColour ((int) Mod::Source::Mseg) != juce::Colour (0xffe0e6f0) && modSourceColour ((int) Mod::Source::Mseg).getSaturation() > 0.1f,
                    "the MSEG source has an LFO pastel, not the white of a source without a family");
        }

        // A route made to the module afterwards: its card, and MOVE TO LFO.
        processor.setModSlotValue (40, "src", (float) (int) Mod::Source::Mseg);
        processor.setModSlotValue (40, "dst", 2.0f);
        processor.setModSlotValue (40, "amt", 0.5f);
        settle (300);
        auto* msegPopover = uiOpenSourcePopover (ctx, Mod::Source::Mseg);
        expect (msegPopover != nullptr && modSourceInPatch (processor, Mod::Source::Mseg),
                "a patch that routes the MSEG module has its editor (the source's pop-up)");
        if (msegPopover != nullptr)
        {
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*msegPopover, buttons);
            juce::TextButton* move = nullptr;
            for (auto* button : buttons)
                if (button->getButtonText().startsWith ("MOVE TO LFO") && visibleInTree (button))
                    move = button;
            expect (move != nullptr, "the MSEG panel offers MOVE TO LFO");
            if (move != nullptr)
            {
                const auto target = move->getButtonText().getTrailingIntValue() - 1;
                move->triggerClick();
                settle (300);
                auto routesMoved = true;
                for (int slot = 0; slot < Mod::maxSlots; ++slot)
                    routesMoved = routesMoved && processor.readModSlot (slot).source != Mod::Source::Mseg;
                const auto shape = juce::roundToInt (processor.apvts.getRawParameterValue ("lfo" + juce::String (target + 1) + "_shape")->load());
                expect (target >= 0 && routesMoved && processor.readModSlot (40).source == Mod::lfoSourceFor (target)
                            && shape == IlanaSynthAudioProcessor::curveShape && processor.isLfoShown (target),
                        "MOVE TO LFO draws the MSEG on LFO " + juce::String (target + 1) + " (SHAPE > MSEG) with its routes");
                processor.getUndoManager().undo();
                settle (300);
                expect (modSourceRouted (processor, Mod::Source::Mseg), "undo puts the MSEG's routes back");
            }
        }
        uiCloseSourcePopover (ctx);

        // A one-shot MSEG (LOOP off) stays a module: an LFO always cycles.
        if (auto* loop = processor.apvts.getParameter ("mseg_loop"))
            loop->setValueNotifyingHost (0.0f);
        settle (100);
        expect (processor.legacyMsegTargetLfo() < 0 && ! processor.moveLegacyMsegToLfo() && modSourceRouted (processor, Mod::Source::Mseg),
                "a one-shot MSEG stays the module (no LFO can play it once)");
        processor.loadFactoryPreset (neuroWobble);
        settle (300);
    }
}

void uiGroup_VECTOR_PAGE (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // M8.5: the VECTOR page has the pad (EVOLVE moved onto the macro card,
    // and VECTOR X / Y live in the source bar only: review 8, I8-13/14).
    {
        pages->showPage ("VECTOR");
        settle (200);
        auto* page = pages->getCurrentPage();
        expect (page != nullptr && findChild<VectorPadDisplay> (*page) != nullptr, "the VECTOR page shows the vector pad");
        std::vector<KnobControl*> knobs;
        std::vector<ModSourceChip*> pageChips;
        if (page != nullptr)
        {
            findAll<KnobControl> (*page, knobs);
            findAll<ModSourceChip> (*page, pageChips);
        }
        auto evolveKnobs = 0;
        for (auto* knob : knobs)
            evolveKnobs += knob->getParameterId().contains ("_evolve") ? 1 : 0;
        expect (evolveKnobs == 0 && pageChips.empty(), "VECTOR has no EVOLVE pane and no VECTOR X / Y chips of its own (I8-13, I8-14)");

        // UI review 31, items 18 and 36: each corner has its oscillator's LEVEL under its picture; PATH's shapes
        // write the eight points in one undo step; SYNC runs the path at the host tempo.
        {
            auto cornerLevel = false;
            for (auto* knob : knobs)
                cornerLevel = cornerLevel || (knob->getParameterId() == "osc1_level" && visibleInTree (knob));
            expect (cornerLevel, "VECTOR: the TOP LEFT corner shows OSC 1's LEVEL under its picture");

            juce::ComboBox* shapes = nullptr;
            std::vector<juce::ComboBox*> boxes;
            if (page != nullptr)
                findAll<juce::ComboBox> (*page, boxes);
            for (auto* box : boxes)
                if (box->getProperties()["textPrefix"].toString() == "PATH")
                    shapes = box;
            expect (shapes != nullptr, "VECTOR: POSITION AND MOTION has PATH's shape menu");

            if (shapes != nullptr)
            {
                const auto pointsText = [&processor]
                {
                    juce::String text;
                    for (int i = 1; i <= 8; ++i)
                        text << juce::String (processor.apvts.getRawParameterValue ("vec_px" + juce::String (i))->load(), 2) << ","
                             << juce::String (processor.apvts.getRawParameterValue ("vec_py" + juce::String (i))->load(), 2) << " ";
                    return text.trim();
                };
                expect (shapes->getText() == "Circle", "VECTOR: the path every patch starts with reads Circle ('" + shapes->getText() + "')");
                processor.getUndoManager().clearUndoHistory();
                shapes->setSelectedId (2, juce::sendNotificationSync);
                const auto steps = processor.getUndoManager().getUndoDescriptions();
                expect (pointsText() == "0.50,0.50 0.78,0.80 0.90,0.50 0.78,0.20 0.50,0.50 0.22,0.80 0.10,0.50 0.22,0.20" && steps.size() == 1,
                        "VECTOR: Figure 8 writes the path's eight points in one undo step (" + pointsText() + "; '" + steps.joinIntoString ("', '") + "')");

                // SYNC at 1/1 (4 beats) on the Circle: a quarter bar turns the point a quarter round (the octagon's
                // symmetry makes that exactly 90 degrees), a whole bar brings it back.
                shapes->setSelectedId (1, juce::sendNotificationSync);
                for (const auto& [id, value] : { std::pair<const char*, float> { "vec_on", 1.0f }, { "vec_path", 1.0f }, { "vec_sync", 1.0f },
                                                 { "vec_div", 0.0f }, { "vec_shape", 2.0f }, { "vec_x", 0.5f }, { "vec_y", 0.5f } })
                    processor.apvts.getParameter (id)->setValueNotifyingHost (processor.apvts.getParameter (id)->convertTo0to1 (value));
                const auto runFor = [&processor] (double seconds)
                {
                    juce::AudioBuffer<float> buffer (2, 256);
                    juce::MidiBuffer midi;
                    const auto blocks = juce::roundToInt (seconds * 48000.0 / 256.0); // (prepareToPlay's rate above)
                    for (int block = 0; block < blocks; ++block)
                    {
                        buffer.clear();
                        processor.processBlock (buffer, midi);
                    }
                };
                const auto angle = [&processor]
                {
                    const auto p = processor.getVectorPosition();
                    return std::atan2 (p.y - 0.5f, p.x - 0.5f) * 180.0f / juce::MathConstants<float>::pi;
                };
                runFor (0.1);
                const auto bpm = processor.getCurrentBpm();
                const auto start = angle();
                runFor (60.0 / bpm);
                const auto quarter = angle();
                runFor (60.0 / bpm * 3.0);
                const auto whole = angle();
                const auto turned = std::abs (std::remainder (quarter - start, 360.0f));
                expect (std::abs (turned - 90.0f) < 3.0f && std::abs (std::remainder (whole - start, 360.0f)) < 3.0f,
                        "VECTOR: PATH with SYNC at 1/1 goes round once per bar at the host tempo (a beat turns it "
                            + juce::String (turned, 1) + " degrees, a bar " + juce::String (std::remainder (whole - start, 360.0f), 1)
                            + ", " + juce::String (bpm, 1) + " BPM)");
                for (const auto* id : { "vec_on", "vec_path", "vec_sync", "vec_div", "vec_shape", "vec_x", "vec_y" })
                    processor.apvts.getParameter (id)->setValueNotifyingHost (processor.apvts.getParameter (id)->getDefaultValue());
                shapes->setSelectedId (1, juce::sendNotificationSync);
                processor.getUndoManager().clearUndoHistory();
            }
        }

        // EVOLVE on the macro's card: AMOUNT and RATE sliders for that
        // macro, and FREEZE; a macro that evolves wears a mark in the strip.
        pages->showPage ("MAIN");
        settle (200);
        std::vector<StripKnob*> macros;
        findAll<StripKnob> (*editor, macros);
        StripKnob* macro3 = nullptr;
        for (auto* macro : macros)
            if (macro->getMacroIndex() == 2)
                macro3 = macro;
        auto* card = ModHoverPopup::instance();
        if (macro3 != nullptr && card != nullptr)
        {
            macro3->openCard();
            card->holdOpen (true);
            settle (100);
            auto* amount = card->getEvolveAmount();
            auto* rate = card->getEvolveRate();
            const auto ids = amount != nullptr && rate != nullptr
                             && amount->getSlider().getTooltip().startsWith (processor.apvts.getParameter ("macro3_evolve")->getName (64))
                             && rate->getSlider().getTooltip().startsWith (processor.apvts.getParameter ("macro3_evolve_rate")->getName (64));
            const auto inside = amount != nullptr && card->getLocalBounds().contains (amount->getBounds())
                                && rate != nullptr && card->getLocalBounds().contains (rate->getBounds())
                                && ! amount->getBounds().intersects (rate->getBounds())
                                && card->getLocalBounds().contains (card->getFreezeButton().getBounds())
                                && card->getFreezeButton().isVisible();
            expect (ids && inside, "a macro's card carries its EVOLVE (AMOUNT, RATE) and FREEZE, inside the card, not overlapping");
            auto* evolve = processor.apvts.getParameter ("macro3_evolve");
            const auto evolveWas = evolve->getValue();
            evolve->setValueNotifyingHost (0.5f);
            macro3->refreshTargets();
            const auto marked = macro3->isEvolving() && macro3->isAssigned();
            evolve->setValueNotifyingHost (evolveWas);
            macro3->refreshTargets();
            expect (marked, "an evolving macro is marked in the strip and counts as assigned");
            card->holdOpen (false);
            card->close();
            // A knob's card has no EVOLVE.
            expect (card->getEvolveAmount() == nullptr || ! card->isVisible(), "only a macro's card has EVOLVE");
        }
        else
        {
            expect (false, "the macro strip and its card exist");
        }
    }
}

// Bounces the patch into OSC 2 (which turns it to Sample mode) and shows OSC 2's tab.
void uiBounceIntoOsc2 (UiCtx& ctx)
{
    auto& processor = ctx.processor;
    IlanaSynthAudioProcessor::BounceRequest request;
    request.targetOsc = 1;
    request.holdSeconds = 0.5;
    request.tailSeconds = 0.5;
    request.muteOthers = false;
    processor.startBounce (request);
    for (int i = 0; i < 300 && processor.getBounceState() == IlanaSynthAudioProcessor::BounceState::Rendering; ++i)
        settle (50);
    settle (300);
    uiSelectOscTab (ctx, 1);
}

void uiGroup_BOUNCE (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // M8.6: BOUNCE on every oscillator card; a bounce turns the card to
    // Sample mode.
    {
        pages->showPage ("MAIN");
        settle (200);
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*editor, buttons);
        auto bounces = 0;
        for (auto* button : buttons)
            bounces += button->getButtonText() == "RESAMPLE" && visibleInTree (button) ? 1 : 0;
        expect (bounces >= 1, "the switched-on oscillator cards have RESAMPLE buttons (" + juce::String (bounces) + ")");

        uiBounceIntoOsc2 (ctx);
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        auto sampleShown = false;
        for (auto* knob : knobs)
            sampleShown = sampleShown || (knob->getParameterId() == "osc2_sample_start" && visibleInTree (knob));
        expect (processor.getBounceState() == IlanaSynthAudioProcessor::BounceState::Done && sampleShown,
                "a bounce puts OSC 2 in Sample mode with the sample controls showing (state "
                    + juce::String ((int) processor.getBounceState()) + ", sample knobs " + (sampleShown ? "shown" : "hidden") + ")");
    }
}

void uiGroup_WARP2 (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // WARP 2 as FM / Ring / FM Noise: the wave display draws each one (pictures with ILANA_UITEST_SHOTS).
    {
        selectOscTab (0);
        settle (150);
        std::vector<WaveDisplay*> waves;
        findAll<WaveDisplay> (*editor, waves);
        WaveDisplay* wave = nullptr;
        for (auto* candidate : waves)
            if (candidate->getOscIndex() == 0 && (wave == nullptr || (visibleInTree (candidate) && ! candidate->isCompact())))
                wave = candidate;
        expect (wave != nullptr, "OSC 1's wave display is shown for the WARP 2 pictures");

        if (wave != nullptr)
        {
            const auto set = [&processor] (const char* id, float value)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            const auto shotsDir = juce::SystemStats::getEnvironmentVariable ("ILANA_UITEST_SHOTS", "");
            const auto picture = [&] (const char* name)
            {
                settle (200);
                auto image = wave->createComponentSnapshot (wave->getLocalBounds());
                if (shotsDir.isNotEmpty())
                    save (*wave, juce::File (shotsDir).getChildFile (name));
                return image;
            };
            const auto differs = [] (const juce::Image& x, const juce::Image& y)
            {
                auto count = 0;
                for (int j = 0; j < juce::jmin (x.getHeight(), y.getHeight()); ++j)
                    for (int i = 0; i < juce::jmin (x.getWidth(), y.getWidth()); ++i)
                        count += x.getPixelAt (i, j) != y.getPixelAt (i, j) ? 1 : 0;
                return count;
            };

            set ("osc1_warp", 0.0f);
            set ("osc1_warp2", 0.0f);
            set ("osc1_warp2_amt", 0.0f);
            const auto plain = picture ("warp2-off.png");
            const char* names[] { "warp2-fm.png", "warp2-ring.png", "warp2-fm-noise.png" };
            const char* labels[] { "FM", "Ring", "FM Noise" };

            for (int k = 0; k < 3; ++k)
            {
                set ("osc1_warp2", (float) (16 + k));
                set ("osc1_warp2_amt", 0.6f);
                const auto shown = picture (names[k]);
                expect (differs (plain, shown) > 100, juce::String ("the wave display draws WARP 2 ") + labels[k]);
            }

            set ("osc1_warp2", 0.0f);
            set ("osc1_warp2_amt", 0.0f);
            settle (150);
        }
    }
}

