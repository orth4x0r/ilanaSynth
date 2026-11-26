// UI review 4, batch H: the tour, text sizes, the scope and meters, spelled-out
// labels and SEQ GENERATE's grid.
// UI review 7, package Q4: PLAY's strips, the OSC card of an operator, SF2
// zones, the exciters, BODY's names, VECTOR and PHYSICAL.
void runPlayOscReview7Tests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto loadNamed = [&processor] (const juce::String& name)
    {
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (name));
        settle (500);
    };
    const auto setParam = [&processor] (const juce::String& id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };
    const auto knobFor = [&editor] (const juce::String& id, const juce::String& label = {}) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && visibleInTree (knob) && knob->getWidth() > 0
                && (label.isEmpty() || knob->getLabelText() == label))
                return knob;
        return nullptr;
    };
    const auto centreX = [&editor] (juce::Component* c) { return c == nullptr ? -1 : editor.getLocalArea (c, c->getLocalBounds()).getCentreX(); };
    const auto buttonNamed = [&editor] (const juce::String& text) -> juce::Button*
    {
        std::vector<juce::Button*> buttons;
        findAll<juce::Button> (editor, buttons);
        for (auto* button : buttons)
            if (visibleInTree (button) && button->getButtonText() == text)
                return button;
        return nullptr;
    };
    const auto selectOscTab = [&editor] (int osc)
    {
        std::vector<StateTabs*> rows;
        findAll<StateTabs> (editor, rows);
        for (auto* tabs : rows)
            for (int i = 0; i < tabs->getNumItems(); ++i)
                if (tabs->getItem (i).name == "OSC " + juce::String (osc + 1) && tabs->onSelect != nullptr
                    && (dynamic_cast<OscPicker*> (tabs) == nullptr || tabs->getName() == "OSC tabs"))
                {
                    tabs->setSelected (i);
                    tabs->onSelect (i);
                    settle (200);
                }
    };
    const auto oscTabState = [&editor] (int osc)
    {
        std::vector<StateTabs*> rows;
        findAll<StateTabs> (editor, rows);
        for (auto* tabs : rows)
            for (int i = 0; i < tabs->getNumItems(); ++i)
                if (tabs->getItem (i).name == "OSC " + juce::String (osc + 1) && (dynamic_cast<OscPicker*> (tabs) == nullptr || tabs->getName() == "OSC tabs"))
                    return tabs->getItem (i).state;
        return juce::String();
    };
    const auto oscWave = [&editor] (int osc) -> WaveDisplay*
    {
        std::vector<WaveDisplay*> waves;
        findAll<WaveDisplay> (editor, waves);
        for (auto* wave : waves)
            if (visibleInTree (wave) && wave->getOscIndex() == osc && ! wave->isCompact())
                return wave;
        return nullptr;
    };

    // PLAY: no empty slots, one slim ADD OSC row, the knobs in one order
    // whatever the mode or FM role (V7-4, V7-26, S7-2, S7-3).
    loadNamed ("Neuro Wobble");
    editor.showPage ("MAIN");
    settle (400);
    {
        // (OSC 3's parameters are "sub_".)
        const juce::String osc3 (OscillatorIds::prefixes[2]);
        // (OSC 3 is off, so folded: UI review 8, V8-17; OSC 2 is the plain one.)
        auto* add = buttonNamed ("+  ADD OSC 4");
        auto* level1 = knobFor ("osc1_level");
        auto* level3 = knobFor ("osc2_level");
        const auto addArea = add != nullptr ? editor.getLocalArea (add, add->getLocalBounds()) : juce::Rectangle<int>();
        const auto level3Area = level3 != nullptr ? editor.getLocalArea (level3, level3->getLocalBounds()) : juce::Rectangle<int>();
        // (Past three cards the add row sits under them and PLAY's oscillator area scrolls to it, ilana 2026-10-23.)
        expect (add != nullptr && level3 != nullptr && knobFor (osc3 + "_level") != nullptr,
                "PLAY: three oscillator cards (OSC 3 shown, dimmed while off) and the + ADD OSC 4 row under them");
        // (ilana 2026-10-23, "i cant scroll to add another osc": scrolled to the end, the whole row shows.)
        if (auto* view = add != nullptr ? add->findParentComponentOfClass<juce::Viewport>() : nullptr)
        {
            view->setViewPosition (0, 1 << 20);
            settle (100);
            const auto shownRow = editor.getLocalArea (add, add->getLocalBounds());
            const auto shownView = editor.getLocalArea (view, view->getLocalBounds());
            expect (shownView.contains (shownRow), "PLAY: scrolled to the end, the + ADD OSC 4 row is in view (" + shownRow.toString() + " in " + shownView.toString() + ")");
            view->setViewPosition (0, 0);
            settle (100);
        }
        expect (level1 != nullptr && level3 != nullptr && centreX (level1) == centreX (level3)
                    && centreX (knobFor ("osc1_semi")) == centreX (knobFor ("osc2_semi")),
                "PLAY: SEMI, LEVEL in the same columns on every wavetable card");
        // SUB and NOISE: two cards, each with its own ON switch, VOLUME knob
        // and controls (SUB: wave menu, octave, COLOUR; NOISE: six types).
        {
            std::vector<ToggleControl*> toggles;
            findAll<ToggleControl> (editor, toggles);
            auto subSwitch = false, noiseSwitch = false;
            for (auto* toggle : toggles)
                if (visibleInTree (toggle))
                {
                    subSwitch = subSwitch || toggle->getTooltip().startsWith (processor.apvts.getParameter ("subosc_on")->getName (64));
                    noiseSwitch = noiseSwitch || toggle->getTooltip().startsWith (processor.apvts.getParameter ("noise_on")->getName (64));
                }
            auto* subVolume = knobFor ("subosc_level");
            auto* subColour = knobFor ("sub_colour", "COLOUR");
            auto* noise = knobFor ("noise_level");
            // (ilana's arranger pass, 2026-10-29: SUB's LEVEL and COLOUR in one row, the NOISE card under SUB.)
            expect (subSwitch && noiseSwitch && subVolume != nullptr && subColour != nullptr && noise != nullptr && centreX (subColour) > centreX (subVolume)
                        && editor.getLocalArea (noise, noise->getLocalBounds()).getY() > editor.getLocalArea (subVolume, subVolume->getLocalBounds()).getBottom()
                        && std::abs (editor.getLocalArea (subColour, subColour->getLocalBounds()).getCentreY()
                                     - editor.getLocalArea (subVolume, subVolume->getLocalBounds()).getCentreY()) < 2,
                    "PLAY: SUB and NOISE each have a switch and a LEVEL knob, SUB's COLOUR beside its LEVEL, NOISE under SUB");
        }

        // The same controls at three window sizes: no two overlap, none is cut off.
        for (const auto width : { 795, 1060, 1400 })
        {
            editor.setSize (width, width * 720 / 1060);
            settle (300);
            std::vector<juce::Rectangle<int>> areas;
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (editor, knobs);
            for (auto* knob : knobs)
                if (visibleInTree (knob) && (knob->getParameterId() == "subosc_level" || knob->getParameterId() == "sub_colour" || knob->getParameterId() == "noise_level"))
                    areas.push_back (knob->isPacked() ? editor.getLocalArea (knob, knob->packedCell()) : editor.getLocalArea (knob, knob->getLocalBounds()));
            std::vector<ToggleControl*> toggles;
            findAll<ToggleControl> (editor, toggles);
            for (auto* toggle : toggles)
                if (visibleInTree (toggle) && (toggle->getTooltip().startsWith (processor.apvts.getParameter ("subosc_on")->getName (64))
                                               || toggle->getTooltip().startsWith (processor.apvts.getParameter ("noise_on")->getName (64))))
                    areas.push_back (editor.getLocalArea (toggle, toggle->getLocalBounds()));
            auto overlap = false;
            juce::String pairs;
            for (size_t i = 0; i < areas.size(); ++i)
                for (size_t j = i + 1; j < areas.size(); ++j)
                    if (areas[i].reduced (2).intersects (areas[j].reduced (2)))
                    {
                        overlap = true;
                        pairs << areas[i].toString() << " x " << areas[j].toString() << "; ";
                    }
            expect (areas.size() == 5 && ! overlap, "PLAY at " + juce::String (width) + " px: SUB and NOISE's switches and knobs are all there and none overlaps ("
                                                       + juce::String ((int) areas.size()) + " found " + pairs + ")");
        }
        editor.setSize (1060, 720);
        settle (300);

        // A wavetable in an FM route reads its part in the FM diagram's
        // words, operator or not (I8-19).
        editor.showPage ("MAIN");
        settle (300);
        // (Only the chosen tab carries its role: one rule on every page, I12-2.)
        selectOscTab (0);
        const auto firstState = oscTabState (0), otherState = oscTabState (1);
        selectOscTab (1);
        // (The tag is the engine, never the role: I13-2.)
        expect (firstState == "WAVETABLE" && otherState.isEmpty() && oscTabState (1) == "WAVETABLE",
                "OSC: the chosen tab's tag is its engine on every patch (" + firstState + ", " + oscTabState (1) + ")");
        selectOscTab (0);
        // A DX7 operator is OSC n and its kind is OPERATOR (I13-1, I13-2).
        loadNamed ("E.PIANO 1 (ROM1A)");
        editor.showPage ("MAIN");
        settle (400);
        selectOscTab (0);
        expect (oscTabState (0) == "FM / DX7", "OSC: a DX7 operator's tab tag is its type, FM / DX7 (" + oscTabState (0) + ")");
        loadNamed ("Neuro Wobble");
        editor.showPage ("MAIN");
        settle (300);
    }

    // A DX7 voice: each strip shows the operator's OUTPUT in dB (the FM
    // card's), LEVEL and FINE, its Operator Env instead of a sine, no FRAME
    // (I7-2, I7-19).
    loadNamed ("E.PIANO 1 (ROM1A)");
    editor.showPage ("MAIN");
    settle (400);
    {
        auto* level = knobFor ("osc2_eg_out", "OUTPUT"); // (a modulator too: never DEPTH, I15-1)
        auto* trim = knobFor ("osc2_level"); // (none on PLAY: one level, OUTPUT, I10-1)
        const auto levelText = level != nullptr ? level->getSlider().getTextFromValue (level->getSlider().getValue()) : juce::String();
        std::vector<WaveDisplay*> waves;
        findAll<WaveDisplay> (editor, waves);
        auto compactWaves = 0;
        for (auto* wave : waves)
            compactWaves += visibleInTree (wave) && wave->isCompact() ? 1 : 0;
        expect (level != nullptr && levelText.endsWith ("dB") && trim == nullptr && knobFor ("osc2_frame") == nullptr
                    && centreX (knobFor ("osc2_fine")) < centreX (knobFor ("osc2_ratio")) && centreX (knobFor ("osc2_ratio")) < centreX (level)
                    && compactWaves > 0, // (the FM card's compact picture is the Operator Env itself, #133)
                "PLAY: an operator card is FINE, RATIO, OUTPUT (" + levelText + ") as on FM (V8-5, I10-1), its Operator Env "
                "pictured, no FRAME (Operator Env pictures " + juce::String (compactWaves) + ", frame " + juce::String (knobFor ("osc2_frame") != nullptr ? 1 : 0)
                + ", trim " + juce::String (trim != nullptr ? 1 : 0) + ")");
    }

    // Review 14 (I14-3): a DX7 voice with both filters open has no filter;
    // PLAY says so, dims everything but CUTOFF, which brings one in.
    {
        auto* reso = knobFor ("f1_reso");
        auto* cutoff = knobFor ("f1_cutoff");
        expect (reso != nullptr && cutoff != nullptr && reso->getAlpha() < 0.99f && cutoff->getAlpha() > 0.99f,
                "PLAY: a DX7 voice's filter knobs dim but CUTOFF, which brings a filter in (I14-3)");
    }

    // OSC: the operator's card is its Operator Env (the FM graph), LEVEL,
    // pitch with LEVEL and one WAVE row, no warp, spectral or unison spread;
    // the one-frame sine shows as WAVE (I7-20, V7-16, V7-31, S7-12).
    editor.showPage ("MAIN");
    settle (300);
    selectOscTab (0);
    {
        std::vector<OperatorEnvDisplay*> graphs;
        findAll<OperatorEnvDisplay> (editor, graphs);
        auto graph = false;
        for (auto* g : graphs)
            graph = graph || (visibleInTree (g) && g->getPrefix() == "osc1" && g->getWidth() > 120); // (the approved card gives it its 150 px well)
        auto* wave = oscWave (0);
        const auto editOpEnv = juce::String::fromUTF8 ("EDIT OP ENV \xe2\x80\xba");
        expect (graph && knobFor ("osc1_eg_out", "OUTPUT") != nullptr && knobFor ("osc1_level", "VOICE LEVEL") == nullptr
                    && knobFor ("osc1_warp_amt") == nullptr && knobFor ("osc1_spectral_amt") == nullptr && knobFor ("osc1_detune") == nullptr
                    && knobFor ("osc1_frame") == nullptr && buttonNamed (editOpEnv) == nullptr,
                "OSC: an operator's card shows its Operator Env graph and OUTPUT alone (no VOICE LEVEL, I12-1), no wavetable warp or unison spread ("
                    + juce::String ((int) graph) + juce::String ((int) (knobFor ("osc1_eg_out", "OUTPUT") != nullptr)) + juce::String ((int) (knobFor ("osc1_level", "VOICE LEVEL") == nullptr))
                    + juce::String ((int) (knobFor ("osc1_warp_amt") == nullptr)) + juce::String ((int) (knobFor ("osc1_spectral_amt") == nullptr))
                    + juce::String ((int) (knobFor ("osc1_detune") == nullptr)) + juce::String ((int) (knobFor ("osc1_frame") == nullptr))
                    + juce::String ((int) (buttonNamed (editOpEnv) == nullptr)) + ")");
        // No full-height sine beside the envelope (review 11, V11-7): the WAVE menu in the rows
        // is the one place the wave is chosen, and VOICE LEVEL is not drawn on the page (I12-1).
        expect (wave == nullptr && knobFor ("osc1_level", "VOICE LEVEL") == nullptr && knobFor ("osc1_level") == nullptr,
                "OSC: an Operator Env operator has the envelope and one WAVE menu, no sine picture (V11-7)");

        // (The mock has no EDIT OP ENV button on the card: a click on the envelope picture opens the editor.)
        setParam ("osc1_mode", 1.0f);
        settle (400);
        expect (buttonNamed (editOpEnv) == nullptr, "OSC: an operator's card has no EDIT OP ENV button (its picture opens the editor)");
        setParam ("osc1_mode", 0.0f);
        settle (300);
    }

    // OSC rows packed from the top (S7-25): Init's three rows sit within
    // about 3 x 110 px.
    loadNamed ("Init");
    editor.showPage ("MAIN");
    settle (400);
    {
        auto* frame = knobFor ("osc1_semi");
        auto* unison = knobFor ("osc1_unison");
        const auto spread = frame != nullptr && unison != nullptr
                                ? editor.getLocalArea (unison, unison->getLocalBounds()).getY() - editor.getLocalArea (frame, frame->getLocalBounds()).getY()
                                : 999;
        expect (spread <= 2 * 134, "OSC: the card's rows are packed (SHAPE to UNISON " + juce::String (spread) + " px)");
        expect (buttonNamed ("RESAMPLE") != nullptr && buttonNamed ("BOUNCE") == nullptr, "OSC: the resampler's button is RESAMPLE (I7-25)");

        // Sample mode: a LOAD button, and an SF2 / SFZ's zones under the
        // wave (I7-24).
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-q4-sfz");
        folder.createDirectory();
        {
            juce::AudioBuffer<float> tone (1, 4410);
            for (int i = 0; i < tone.getNumSamples(); ++i)
                tone.setSample (0, i, 0.5f * std::sin ((float) i * 0.06f));
            juce::WavAudioFormat wav;
            auto file = folder.getChildFile ("tone.wav");
            file.deleteFile();
            if (auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 44100.0, 1, 16, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (tone, 0, tone.getNumSamples());
                }
            folder.getChildFile ("test.sfz").replaceWithText ("<region> sample=tone.wav lokey=36 hikey=59 pitch_keycenter=48\n"
                                                              "<region> sample=tone.wav lokey=60 hikey=84 pitch_keycenter=72\n");
        }
        setParam ("osc1_mode", 2.0f);
        settle (300);
        const auto loaded = processor.loadUserSample (0, folder.getChildFile ("test.sfz"));
        settle (400);
        auto* wave = oscWave (0);
        expect (buttonNamed ("LOAD...") != nullptr, "OSC: Sample mode has a LOAD... button (I10-11)");
        expect (loaded && wave != nullptr && wave->getZoneCount() == 2, "OSC: an SFZ's two zones show under the sample ("
                                                                            + juce::String (wave != nullptr ? wave->getZoneCount() : -1) + ")");
        folder.deleteRecursively();

        // The exciters: one name table, in groups; a hammer has no pick
        // controls (I7-27).
        // (On the oscillator's card, the one string editor since the
        // PHYSICAL tab folded in.)
        setParam ("osc1_mode", 1.0f);
        setParam ("osc1_excite", 5.0f);
        editor.showPage ("MAIN");
        settle (400);
        // (Review 23: the exciter is a row of pills; the menu behind them holds the choice.)
        const auto hammerName = processor.apvts.getParameter ("osc1_excite")->getCurrentValueAsText();
        expect (hammerName == "Bright Hammer" && knobFor ("osc1_string_pick_hardness") == nullptr && knobFor ("osc1_string_pick_pos") == nullptr
                    && knobFor ("osc1_couple", "COUPLE") != nullptr,
                "OSC: the M4 hammer reads Bright Hammer, without the pick's HARDNESS / PICK POS ('" + hammerName + "')");
        setParam ("osc1_excite", 0.0f);
        settle (300);
        expect (knobFor ("osc1_string_pick_hardness") != nullptr, "OSC: a plucked burst has HARDNESS");
        setParam ("osc1_mode", 0.0f);
        settle (300);
    }

    // BODY by one name: the comb filter type is COMB BODY (I7-26).
    // (Its case is the FILTER page's own name, I7-40.)
    expect (FilterTypeGrid::shortNames()[FilterType::CombBody].equalsIgnoreCase ("COMB BODY"), "the comb body filter's short name is COMB BODY");

    // VECTOR: off, every control dims and says why; on, VEC X / Y are in
    // the chip bar (V7-21, I7-22).
    loadNamed ("Init");
    editor.showPage ("VECTOR");
    settle (300);
    {
        setParam ("vec_on", 0.0f);
        settle (300);
        // The corners named as the pad names them, and showing it.
        juce::StringArray corners;
        std::vector<ComboControl*> combos;
        findAll<ComboControl> (editor, combos);
        for (auto* combo : combos)
            if (visibleInTree (combo) && combo->getComboBox().getNumItems() > 0 && combo->getComboBox().getItemText (0) == "OSC 1")
                corners.add (combo->getComboBox().getText());
        corners.sort (false);
        const auto cornerTexts = corners.joinIntoString ("|");
        auto* x = knobFor ("vec_x");
        const auto dim = x != nullptr && x->getAlpha() < 0.9f && x->getSlider().getTooltip().contains ("VECTOR is off");
        setParam ("vec_on", 1.0f);
        settle (300);
        const auto lit = x != nullptr && x->getAlpha() > 0.99f;
        editor.showPage ("MAIN");
        settle (300);
        std::vector<ModSourceChip*> chips;
        findAll<ModSourceChip> (editor, chips);
        // In the bar as chips of their own: played sources fold last (S8-20).
        const auto inBar = [&] (Mod::Source source)
        {
            for (auto* chip : chips)
                if (visibleInTree (chip) && chip->getSourceIndex() == (int) source)
                    return 1;
            return 0;
        };
        // (Init's two oscillators fit the pad to the Slider, which uses X only: VECTOR Y then stays out, UI review 30.)
        const auto wantedChips = processor.getVectorShape() == VectorShape::slider ? 1 : 2;
        const auto barChips = inBar (Mod::Source::VectorX) + inBar (Mod::Source::VectorY);
        setParam ("vec_on", 0.0f);
        settle (300);
        const auto offChips = inBar (Mod::Source::VectorX);
        expect (dim && lit, "VECTOR: off, X dims and says VECTOR is off; on, it lights");
        expect (cornerTexts.replace (": none", "").replace (": off", "") == "OSC 1|OSC 2|OSC 3|OSC 4", "VECTOR: the corners read OSC 1..4 (" + cornerTexts + ")");
        expect (barChips == wantedChips && offChips == 0, "VEC X / VEC Y are in the chip bar while VECTOR is on (" + juce::String (barChips) + ")");
    }

    // UI review 30: the Slider shape uses X only, so Y dims and VECTOR Y leaves the chip bar; a corner on an
    // oscillator the patch doesn't have is a "+ ADD OSC n" button, one undo step.
    {
        setParam ("vec_on", 1.0f);
        setParam ("vec_shape", (float) VectorShape::slider);
        editor.showPage ("VECTOR");
        settle (300);
        auto* x = knobFor ("vec_x");
        auto* y = knobFor ("vec_y");
        const auto sliderDims = x != nullptr && y != nullptr && x->getAlpha() > 0.99f && y->getAlpha() < 0.9f;
        editor.showPage ("MAIN");
        settle (300);
        std::vector<ModSourceChip*> chips;
        findAll<ModSourceChip> (editor, chips);
        auto xChip = false, yChip = false;
        for (auto* chip : chips)
            if (visibleInTree (chip))
            {
                xChip = xChip || chip->getSourceIndex() == (int) Mod::Source::VectorX;
                yChip = yChip || chip->getSourceIndex() == (int) Mod::Source::VectorY;
            }
        expect (sliderDims && xChip && ! yChip, "VECTOR: the Slider shape dims Y and keeps only VECTOR X in the chip bar");

        setParam ("vec_shape", (float) VectorShape::hexagon);
        editor.showPage ("VECTOR");
        settle (300);
        auto* pad = findChild<VectorPadDisplay> (editor);
        if (pad != nullptr)
            pad->createComponentSnapshot (pad->getLocalBounds()); // (the pills are placed as they are painted)
        auto corner = -1;
        for (int c = 0; pad != nullptr && c < processor.getVectorCornerCount(); ++c)
            if (! processor.isOscillatorShown (processor.getVectorCorner (c)) && ! pad->addPillBounds (c).isEmpty() && corner < 0)
                corner = c;
        expect (corner >= 0, "VECTOR: a Hexagon corner on a missing oscillator shows + ADD OSC");
        if (corner >= 0)
        {
            const auto osc = processor.getVectorCorner (corner);
            const auto at = pad->addPillBounds (corner).getCentre();
            const juce::MouseEvent down (juce::Desktop::getInstance().getMainMouseSource(), at, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         pad, pad, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false);
            processor.getUndoManager().clearUndoHistory();
            pad->mouseDown (down);
            pad->mouseUp (down);
            settle (300);
            pad->createComponentSnapshot (pad->getLocalBounds());
            const auto added = processor.isOscillatorShown (osc) && pad->addPillBounds (corner).isEmpty();
            const auto steps = processor.getUndoManager().getUndoDescriptions();
            processor.getUndoManager().undo();
            settle (300);
            expect (added && steps.size() == 1 && ! processor.isOscillatorShown (osc),
                    "VECTOR: + ADD OSC " + juce::String (osc + 1) + " adds the oscillator in one undo step (" + steps.joinIntoString ("|") + ")");
        }
        setParam ("vec_shape", (float) VectorShape::square);
        setParam ("vec_on", 0.0f);
        settle (200);
    }

    // A Physical oscillator's card: the moving string in its well, STRING and
    // EXCITER rows of the card's one grid (the PHYSICAL tab folded in), the
    // body a link to FILTER.
    loadNamed ("Felt Hammer Board");
    editor.showPage ("MAIN");
    settle (400);
    {
        auto* decay = knobFor ("osc1_string_decay");
        auto* hammer = knobFor ("osc1_hammer_hard");
        std::vector<PhysicalView*> views;
        findAll<PhysicalView> (editor, views);
        auto stringShown = false;
        for (auto* view : views)
            stringShown = stringShown || (visibleInTree (view) && view->getOscillator() == "osc1");
        // (STRING and EXCITER are two rows of one grid: the hammer sits under the decay.)
        // (Review 23: TONE and EXCITER are two panels of one card, side by side.)
        const auto sameGrid = decay != nullptr && hammer != nullptr && visibleInTree (decay) && visibleInTree (hammer)
                              && decay->getParentComponent() == hammer->getParentComponent();
        expect (stringShown && sameGrid && buttonNamed (juce::CharPointer_UTF8 ("EDIT BODY \xe2\x80\xba")) == nullptr,
                "OSC: a Physical card shows its moving string, STRING and EXCITER on one grid, and no EDIT BODY link (PLAY's BODY tab is beside it)");
        const auto exciteText = processor.apvts.getParameter ("osc1_excite")->getCurrentValueAsText();
        expect (exciteText.startsWith ("Piano"), "OSC: EXCITE reads Piano ('" + exciteText + "')");
        expect (editor.getPageIds().indexOf ("PHYSICAL") < 0, "the PHYSICAL tab is gone: the card holds the whole string");
    }
}

void runSmallThingsTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    // The tour's "new in" list: the header's version, real pages, current
    // names, every chip shown, and a click opens the chip's page.
    {
        const auto& features = TutorialOverlay::whatsNew();
        const auto pageIds = editor.getPageIds();
        auto* logo = findChild<LogoComponent> (editor);
        juce::StringArray labels, badPages;
        for (const auto& feature : features)
        {
            labels.add (feature.label);
            if (! pageIds.contains (feature.page))
                badPages.add (feature.label + " -> " + feature.page);
        }
        expect (logo != nullptr && logo->version == "v" + juce::String (TutorialOverlay::whatsNewVersion),
                "the tour's NEW IN version is the header's (" + juce::String (TutorialOverlay::whatsNewVersion) + ")");
        expect (badPages.isEmpty(), "every tour chip opens a page that exists" + (badPages.isEmpty() ? juce::String() : ": " + badPages.joinIntoString (", ")));
        // Names retired since: the piano preset was renamed Felt Hammer Board.
        expect (! labels.joinIntoString ("|").containsIgnoreCase ("GRAND PIANO") && labels.contains ("FELT HAMMER BOARD"),
                "the tour names the Felt Hammer Board, not the retired GRAND PIANO");
        expect (labels.contains (juce::String (FilterType::Count) + " FILTERS"), "the tour's filter count is the code's ("
                                                                                     + juce::String (FilterType::Count) + ")");
        for (const auto* name : { "DX7 BANKS + OPERATOR ENV", "AIRWINDOWS", "SF2 / SFZ", "VOCODER", "CLIP SEQUENCER" })
            expect (labels.contains (name), juce::String ("the tour lists ") + name);

        // UI review 7 (I7-35): header captions are lower-case fragments.
        expect (IlanaTheme::captionFragment ("Drag the graph's points.") == "drag the graph's points"
                    && IlanaTheme::captionFragment ("OSC 1 carrier") == "OSC 1 carrier"
                    && IlanaTheme::captionFragment ("rows modulate columns") == "rows modulate columns",
                "card captions read as lower-case fragments");

        auto* tutorial = findChild<TutorialOverlay> (editor);
        if (tutorial != nullptr)
        {
            // UI review 6: three tips, the box unticked, no "DX7 MODE".
            expect (tutorial->getTips().size() == 3 && ! tutorial->isDontShowTicked()
                        && ! (labels.joinIntoString ("|") + tutorial->getTips().joinIntoString ("|")).contains ("DX7 MODE"),
                    "the tour has three tips, \"Don't show this again\" unticked and no DX7 MODE");
            tutorial->setVisible (true);
            settle (600);
            // UI review 7 (S7-30): the tips are as tall as their lines, no
            // fixed allowance leaving a gap above NEW IN.
            expect (tutorial->panelBounds().getHeight() < 431, "the tour's panel is as tall as its tips ("
                                                                   + juce::String (tutorial->panelBounds().getHeight()) + " px)");
            // Review 9, S9-20: the pills are behind WHAT'S NEW; closed, the
            // tour is the tips and one line.
            expect (! tutorial->isWhatsNewOpen() && tutorial->chipBounds().empty(), "the tour opens with the NEW IN pills folded away");
            const auto closedHeight = tutorial->panelBounds().getHeight();
            tutorial->setWhatsNewOpen (true);
            settle (100);
            expect (tutorial->panelBounds().getHeight() > closedHeight && tutorial->panelBounds().getHeight() < 520, // (the 720 px column wraps the pills to five rows since UI review 30; the panel still clears the window by 200 px)
                    "opening WHAT'S NEW grows the panel (" + juce::String (closedHeight) + " to " + juce::String (tutorial->panelBounds().getHeight()) + " px)");
            expect (tutorial->chipBounds().size() == features.size(), "every tour chip fits on the panel ("
                                                                          + juce::String ((int) tutorial->chipBounds().size()) + " of "
                                                                          + juce::String ((int) features.size()) + ")");
            // Impeccable re-score (H7): MOUSE MOVES lists the hidden moves,
            // folded until clicked; one of it and WHAT'S NEW opens at a time,
            // so the panel is never cut by a 720 px window.
            tutorial->setMovesOpen (true);
            settle (100);
            expect (tutorial->isMovesOpen() && ! tutorial->isWhatsNewOpen() && TutorialOverlay::moves().size() == 8
                        && tutorial->panelBounds().getHeight() < tutorial->getHeight() - 140,
                    "MOUSE MOVES opens its eight moves, folds WHAT'S NEW and fits ("
                        + juce::String (tutorial->panelBounds().getHeight()) + " px)");
            tutorial->setWhatsNewOpen (true);
            settle (100);
            expect (! tutorial->isMovesOpen() && tutorial->isWhatsNewOpen(), "opening WHAT'S NEW folds MOUSE MOVES");
            tutorial->openChip (labels.indexOf ("CLIP SEQUENCER"));
            settle (300);
            expect (! tutorial->isVisible() && editor.getCurrentPageId() == "ARP/SEQ",
                    "clicking the tour's CLIP SEQUENCER chip closes the tour and opens SEQ");
        }
        else
        {
            expect (false, "the tour exists");
        }
    }

    // Impeccable re-score (H9): a step that throws something away shows the
    // undo note with its name; UNDO takes it back; a plain edit shows none.
    if (auto* toast = findChild<UndoToast> (editor); toast != nullptr)
    {
        auto* cutoff = processor.apvts.getParameter ("f1_cutoff");
        const auto before = cutoff->getValue();
        processor.performEdit ("Change cutoff", [cutoff, before] { cutoff->setValueNotifyingHost (before > 0.5f ? 0.2f : 0.8f); });
        settle (200);
        expect (! toast->isVisible(), "a plain edit shows no undo note");
        processor.performEdit ("Remove test step", [cutoff] { cutoff->setValueNotifyingHost (0.5f); });
        settle (200);
        expect (toast->isVisible() && toast->getText() == "Remove test step" && toast->getBottom() <= editor.getHeight(),
                "a Remove step shows the undo note with its name ('" + toast->getText() + "')");
        toast->getUndoButton().triggerClick();
        settle (200);
        expect (processor.getUndoManager().getUndoDescription() == "Change cutoff",
                "UNDO on the note takes the step back ('" + processor.getUndoManager().getUndoDescription() + "')");
        settle (200);
        // (Usability flair pass 1: the undo then says what it changed. CUTOFF
        // isn't on the open page here, so the key is SHOW, which opens it.)
        const auto key = toast->getUndoButton().getButtonText();
        expect (toast->isVisible() && toast->getText().startsWith ("Undid CUTOFF") && (key == "REDO" || key == "SHOW"),
                "the undo says what it changed ('" + toast->getText() + "', " + key + ")");
        if (key == "SHOW")
        {
            toast->getUndoButton().triggerClick();
            settle (400);
            expect (editor.getCurrentPageId() == "MAIN", "SHOW opens the page the undone knob is on (" + editor.getCurrentPageId() + ")");
        }
        processor.getUndoManager().undo();
        settle (100);
    }
    else
    {
        expect (false, "the undo note exists");
    }

    // Text floors (S25): every visible label and button on every page, at
    // 100 %, against the theme's floors. Interactive: buttons, menus,
    // editable fields and knob value boxes; the rest is passive.
    {
        juce::StringArray small;
        std::set<juce::String> seen;
        auto checked = 0;
        const auto check = [&] (const juce::String& page)
        {
            std::vector<juce::Label*> labels;
            std::vector<juce::TextButton*> buttons;
            findAll<juce::Label> (editor, labels);
            findAll<juce::TextButton> (editor, buttons);

            for (auto* label : labels)
            {
                if (! visibleInTree (label) || label->getWidth() <= 0 || label->getText().trim().isEmpty())
                    continue;
                const auto interactive = label->isEditable() || dynamic_cast<juce::Slider*> (label->getParentComponent()) != nullptr
                                         || dynamic_cast<juce::ComboBox*> (label->getParentComponent()) != nullptr;
                const auto height = label->getLookAndFeel().getLabelFont (*label).getHeight();
                ++checked;
                // (A name under a well in a row of rings, SEQ GENERATE: the rings' own name size and floor.)
                if ((bool) label->getProperties().getWithDefault ("ringName", false))
                {
                    if (height * IlanaTheme::uiScaleRef() < IlanaTheme::TextSize::screenFloorPx - 0.6f)
                        small.add (page + ": '" + label->getText() + "' ring name " + juce::String (height, 1));
                    continue;
                }
                const auto floor = interactive ? IlanaTheme::TextSize::minInteractive : IlanaTheme::TextSize::minPassive;
                const auto key = page + ": '" + label->getText() + "' " + juce::String (height, 1);
                if (height < floor - 0.01f && seen.insert (key).second)
                    small.add (key);
            }

            // PLAY's ring knobs paint their names themselves (FX, SEQ, VECTOR and FM too since review 26):
            // held to the 10 px screen floor, as the packed buttons.
            std::vector<KnobControl*> rings;
            findAll<KnobControl> (editor, rings);
            for (auto* knob : rings)
            {
                if (! knob->isPacked() || ! visibleInTree (knob) || knob->getWidth() <= 0)
                    continue;
                ++checked;
                if (knob->packedNameDevicePx() < IlanaTheme::TextSize::screenFloorPx - 0.6f)
                    small.add (page + ": (" + knob->getLabelText() + ") ring " + juce::String (knob->packedNameDevicePx(), 1) + " px");
            }

            for (auto* button : buttons)
            {
                if (! visibleInTree (button) || button->getWidth() <= 0 || button->getButtonText().trim().isEmpty()
                    || button->getProperties().contains ("switch"))
                    continue;
                // (The OSC cards' packed header buttons keep the 10 px screen floor instead: review 23.)
                if (button->getProperties().contains ("packedButton"))
                {
                    const auto device = button->getLookAndFeel().getTextButtonFont (*button, button->getHeight()).getHeight() * IlanaTheme::uiScaleRef();
                    ++checked;
                    if (device < IlanaTheme::TextSize::screenFloorPx - 0.6f)
                        small.add (page + ": [" + button->getButtonText() + "] packed " + juce::String (device, 1) + " px");
                    continue;
                }
                const auto height = button->getProperties().contains ("pill")
                                        ? juce::Font (IlanaTheme::pillFont()).getHeight()
                                        : button->getLookAndFeel().getTextButtonFont (*button, button->getHeight()).getHeight();
                const auto key = page + ": [" + button->getButtonText() + "] " + juce::String (height, 1);
                ++checked;
                if (height < IlanaTheme::TextSize::minInteractive - 0.01f && seen.insert (key).second)
                    small.add (key);
            }
        };

        for (const auto& page : editor.getPageIds())
        {
            editor.showPage (page);
            settle (250);
            check (page);
        }
        editor.setScopeOpen (true);
        settle (300);
        check ("SCOPE");
        editor.setScopeOpen (false);
        settle (100);

        expect (small.isEmpty() && checked > 200, "no text under the floors (" + juce::String (checked) + " checked; "
                                     + juce::String (IlanaTheme::TextSize::minInteractive) + " interactive, "
                                     + juce::String (IlanaTheme::TextSize::minPassive) + " passive)"
                                     + (small.isEmpty() ? juce::String() : ": " + small.joinIntoString (", ")));
    }

    // The 75 % floor (UI review 5 #31, 6 #46): every page (and the scope)
    // painted at the smallest zoom, with the theme's font helper watching:
    // no text drawn under 10 screen pixels.
    {
        auto* top = editor.getTopLevelComponent();
        const auto before = top->getBounds();
        top->setSize (795, 540);
        settle (300);
        auto& probe = IlanaTheme::fontProbe();
        juce::StringArray under;
        auto smallest = 1.0e6f;
        const auto paintPage = [&] (const juce::String& page)
        {
            probe = {};
            probe.armed = true;
            probe.limit = IlanaTheme::TextSize::screenFloorPx;
            editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
            probe.armed = false;
            smallest = juce::jmin (smallest, probe.smallest);
            if (probe.under > 0)
                under.add (page + " (" + juce::String (probe.smallest, 2) + " px)");
        };
        for (const auto& page : editor.getPageIds())
        {
            editor.showPage (page);
            settle (250);
            paintPage (page);
        }
        editor.setScopeOpen (true);
        settle (300);
        paintPage ("SCOPE");
        editor.setScopeOpen (false);
        top->setBounds (before);
        settle (300);
        expect (under.isEmpty() && smallest < 1.0e5f, "at 75 % no text is drawn under " + juce::String (IlanaTheme::TextSize::screenFloorPx, 0)
                                                          + " px (smallest " + juce::String (smallest, 2) + ")"
                                                          + (under.isEmpty() ? juce::String() : ": " + under.joinIntoString (", ")));
    }

    // The scope's meters (S23, V27): held peak numbers after sound, reset by
    // a click; the clip light starts dark. The header meter has its own.
    {
        editor.showPage ("MAIN");
        editor.setScopeOpen (true);
        settle (300);
        auto* scope = findChild<ScopeDisplay> (editor);
        expect (scope != nullptr, "the scope panel has its display");

        if (scope != nullptr)
        {
            // UI review 7 (I7-31): the scope docks over the page area by
            // default, with the browser's FLOAT / × pair; FLOAT makes it a
            // small panel and DOCK puts it back. The engine quality settings
            // live in the settings menu only (review 6).
            std::vector<juce::ComboBox*> boxes;
            findAll<juce::ComboBox> (*scope, boxes);
            auto* panel = scope->getParentComponent();
            juce::TextButton* floatButton = nullptr;
            auto hasCross = false;
            if (panel != nullptr)
                for (auto* child : panel->getChildren())
                    if (auto* button = dynamic_cast<juce::TextButton*> (child))
                    {
                        if (button->getButtonText() == "FLOAT")
                            floatButton = button;
                        hasCross = hasCross || button->getButtonText() == juce::String (juce::CharPointer_UTF8 ("\xc3\x97"));
                    }
            const auto dockedBounds = panel != nullptr ? panel->getBounds() : juce::Rectangle<int>();
            expect (boxes.empty() && panel != nullptr && dockedBounds.getWidth() > 900 && dockedBounds.getY() > 56 && floatButton != nullptr && hasCross,
                    "the scope docks over the page with FLOAT and a cross, without quality or oversampling controls ("
                        + dockedBounds.toString() + ")");
            if (floatButton != nullptr)
            {
                floatButton->triggerClick();
                settle (100);
                const auto floating = panel->getBounds();
                const auto dockText = floatButton->getButtonText();
                floatButton->triggerClick();
                settle (100);
                expect (floating.getWidth() < 600 && floating.getY() > 56 && dockText == "DOCK" && panel->getBounds() == dockedBounds,
                        "FLOAT makes the scope a small panel, DOCK puts it back (" + floating.toString() + ")");
            }
            scope->resetPeaks();
            juce::AudioBuffer<float> audio (2, 512);
            for (int block = 0; block < 24; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
                audio.clear();
                processor.processBlock (audio, midi);
            }
            // (A paint reads the output into the meters.)
            const auto image = scope->createComponentSnapshot (scope->getLocalBounds());
            juce::ignoreUnused (image);
            const auto held = juce::jmax (scope->getHeldPeakDb (0), scope->getHeldPeakDb (1));
            expect (held > -60.0f && held < 12.0f, "the scope's meters hold the peak in dB after a note (" + juce::String (held, 1) + " dB)");
            scope->resetPeaks();
            expect (scope->getHeldPeakDb (0) < -90.0f && ! scope->isClipLit(), "resetting the scope's meters clears the peaks and the clip light");

            juce::AudioBuffer<float> silence (2, 512);
            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            for (int block = 0; block < 200; ++block)
            {
                silence.clear();
                processor.processBlock (silence, off);
                off.clear();
            }
        }
        editor.setScopeOpen (false);
        settle (100);

        // WINDOW pops the scope out beside the synth (step 14, S23), so it
        // covers no page. (A real top-level window can't open under xvfb,
        // which has no window manager: unit-level checks, as for menus.)
        {
            editor.setScopeOpen (true);
            settle (200);
            juce::TextButton* windowButton = nullptr;
            if (auto* shown = findChild<ScopeDisplay> (editor); shown != nullptr && shown->getParentComponent() != nullptr)
                for (auto* child : shown->getParentComponent()->getChildren())
                    if (auto* button = dynamic_cast<juce::TextButton*> (child); button != nullptr && button->getButtonText() == "WINDOW")
                        windowButton = button;
            expect (windowButton != nullptr && visibleInTree (windowButton) && windowButton->getTooltip().contains ("own window"),
                    "the scope panel has WINDOW, which opens it in its own window");
            expect (! editor.isScopeWindowOpen(), "the scope opens in the page until WINDOW is clicked");
            editor.setScopeOpen (false);
            settle (100);
            expect (! editor.isScopeOpen(), "the scope closes");
        }

        auto* meter = findChild<OutputMeter> (editor);
        expect (meter != nullptr && meter->getTooltip().contains ("dB") && ! meter->isClipLit(),
                "the OUT meter has a clip light and a tooltip in dB");
    }

    // The voice dots' tooltip counts them (V28).
    {
        settle (400);
        juce::String voices;
        std::vector<juce::Component*> all;
        findAll<juce::Component> (editor, all);
        for (auto* component : all)
            if (auto* client = dynamic_cast<juce::SettableTooltipClient*> (component))
                if (client->getTooltip().startsWith ("Voices: "))
                    voices = client->getTooltip();
        expect (voices.contains (" of ") && voices.contains ("playing"), "the voice dots' tooltip counts them (" + voices.upToFirstOccurrenceOf ("\n", false, false) + ")");
    }

    // Chaos LFO outputs are spelled out, following OUTPUT A's axis (V28).
    {
        const auto& lorenz = LfoSimInfo::get (LfoSimShapes::RandomHold + 4);
        const auto x = LfoSimPreview::outputNames (lorenz, 0);
        const auto z = LfoSimPreview::outputNames (lorenz, 2);
        const auto mix = LfoSimPreview::outputNames (lorenz, 3);
        expect (juce::String (lorenz.name) == "Lorenz" && x.first == "X axis" && x.second == "Y axis" && z.first == "Z axis"
                    && z.second == "X axis" && mix.first.contains ("mix") && mix.second == "Y axis",
                "a chaos LFO's outputs read 'X axis' / 'Y axis' and follow OUTPUT A's axis");
        const auto bounce = LfoSimPreview::outputNames (LfoSimInfo::get (LfoSimShapes::RandomHold + 10), 0);
        expect (bounce.first == "height" && bounce.second == "impacts", "the bounce LFO's outputs read height / impacts");
    }

    // SEQ GENERATE (review 6: V5-21, S5-17, S6-24, I6-30): three boxes,
    // SNAP TO KEY, STRUM and SPRAY, each with its own switch on its title
    // line and its controls inside, every name on one line; SNAP TO KEY's
    // and STRUM's switches turn their choice Off and back, one undo step.
    {
        editor.showPage ("ARP/SEQ");
        settle (400);
        const auto controlFor = [&editor, &processor] (const juce::String& id) -> juce::Component*
        {
            auto* parameter = processor.apvts.getParameter (id);
            std::vector<juce::Component*> all;
            findAll<juce::Component> (editor, all);
            for (auto* component : all)
            {
                if (! visibleInTree (component) || component->getWidth() <= 0)
                    continue;
                if (auto* knob = dynamic_cast<KnobControl*> (component); knob != nullptr && knob->getParameterId() == id)
                    return knob;
                if (auto* combo = dynamic_cast<ComboControl*> (component); combo != nullptr && parameter != nullptr
                    && combo->getTooltip().startsWith (parameter->getName (64)))
                    return combo;
                if (auto* toggle = dynamic_cast<ToggleControl*> (component); toggle != nullptr && parameter != nullptr
                    && toggle->getButton().getTooltip().startsWith (parameter->getName (64)))
                    return toggle;
            }
            return nullptr;
        };
        const auto switchFor = [&editor] (const juce::String& what) -> ChoiceSwitch*
        {
            std::vector<ChoiceSwitch*> switches;
            findAll<ChoiceSwitch> (editor, switches);
            for (auto* candidate : switches)
                if (visibleInTree (candidate) && candidate->getTooltip().startsWith (what))
                    return candidate;
            return nullptr;
        };
        const auto boundsOf = [&editor] (juce::Component* component)
        {
            return component != nullptr ? editor.getLocalArea (component->getParentComponent(), component->getBounds()) : juce::Rectangle<int>();
        };
        const auto topOf = [&boundsOf] (juce::Component* component) { return component != nullptr ? boundsOf (component).getY() : -1; };
        const auto xOf = [&boundsOf] (juce::Component* component) { return component != nullptr ? boundsOf (component).getCentreX() : -1; };
        auto* scale = controlFor ("gen_scale");
        auto* root = controlFor ("gen_root");
        auto* snap = controlFor ("gen_snap");
        auto* strum = controlFor ("spray_strum");
        auto* strumTime = controlFor ("spray_strum_time");
        auto* pitch = controlFor ("spray_direction");
        auto* count = controlFor ("spray_count");
        auto* spread = controlFor ("spray_spread");
        auto* velocity = controlFor ("spray_velocity");
        auto* sprayOn = controlFor ("spray_on");
        auto* scaleSwitch = switchFor ("SNAP TO KEY");
        auto* strumSwitch = switchFor ("STRUM");
        const auto found = scale != nullptr && root != nullptr && snap != nullptr && strum != nullptr && strumTime != nullptr
                        && pitch != nullptr && count != nullptr && spread != nullptr && velocity != nullptr && sprayOn != nullptr
                        && scaleSwitch != nullptr && strumSwitch != nullptr;
        expect (found, "GENERATE shows SNAP TO KEY, STRUM and SPRAY with a switch each");

        if (found)
        {
            const auto row = topOf (count);
            // (Menu-only boxes centre their row, S10-12, so a menu sits a few px lower than a knob.)
            const auto sameRow = [&] (juce::Component* c) { return std::abs (topOf (c) - row) <= 24; };
            expect (sameRow (scale) && sameRow (root) && sameRow (snap) && sameRow (strum)
                        && sameRow (strumTime) && sameRow (pitch) && sameRow (spread),
                    "GENERATE's controls are one row, every name on one line");
            expect (xOf (root) < xOf (scale) && xOf (scale) < xOf (snap) && xOf (snap) < xOf (strum) && xOf (strum) < xOf (strumTime)
                        && xOf (strumTime) < xOf (pitch) && xOf (pitch) < xOf (count) && xOf (spread) < xOf (velocity),
                    "SNAP TO KEY's, STRUM's and SPRAY's controls sit together, in that order");
            expect (topOf (scaleSwitch) < row && topOf (strumSwitch) < row && topOf (sprayOn) < row
                        && xOf (scaleSwitch) > xOf (snap) - 60 && xOf (scaleSwitch) < xOf (strum)
                        && xOf (strumSwitch) > xOf (strum) && xOf (strumSwitch) < xOf (pitch) && xOf (sprayOn) > xOf (spread),
                    "each box's switch closes its own title line, over its controls");

            // SNAP TO KEY's switch: Off, then back to the key it had.
            if (auto* parameter = processor.apvts.getParameter ("gen_scale"))
            {
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (3.0f));
                settle (150);
                processor.apvts.copyState();
                processor.getUndoManager().clearUndoHistory();
                const auto onBefore = scaleSwitch->isOn();
                scaleSwitch->getButton().triggerClick();
                settle (50);
                const auto off = juce::roundToInt (processor.apvts.getRawParameterValue ("gen_scale")->load()) == 0 && ! scaleSwitch->isOn();
                processor.apvts.copyState();
                const auto steps = processor.getUndoManager().getUndoDescriptions();
                scaleSwitch->getButton().triggerClick();
                settle (50);
                const auto back = juce::roundToInt (processor.apvts.getRawParameterValue ("gen_scale")->load());
                expect (onBefore && off && back == 3 && steps.size() == 1 && steps[0] == "SNAP TO KEY off",
                        "SNAP TO KEY's switch sets the key Off in one undo step ('" + steps.joinIntoString ("', '")
                            + "') and on again brings back Dorian (" + juce::String (back) + ")");
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
            }

            // STRUM's switch: on from Off is Up.
            if (auto* parameter = processor.apvts.getParameter ("spray_strum"))
            {
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
                settle (150);
                strumSwitch->getButton().triggerClick();
                settle (50);
                const auto up = juce::roundToInt (processor.apvts.getRawParameterValue ("spray_strum")->load());
                strumSwitch->getButton().triggerClick();
                settle (50);
                expect (up == 1 && juce::roundToInt (processor.apvts.getRawParameterValue ("spray_strum")->load()) == 0,
                        "STRUM's switch turns the strum on (Up) and off");
            }
        }
    }
}

// Drives the real editor the way a user would and checks the processor
// state, for the wiring that unit tests can't see.
// UI review 31, builder C: MATRIX (items 5, 6, 11, 33, 42), the macros' + ASSIGN
// (7), the dock's chip groups (10), the performance pop-ups and wheels (19, 28),
// click to assign and Alt-drag (26) and the filter graph as a drop target (27).
void runReview31MatrixTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto names = processor.getFactoryPresetNames();
    const auto load = [&] (const juce::String& name)
    {
        processor.loadFactoryPreset (names.indexOf (name));
        settle (300);
    };
    // (The editor here has no window, so the MATRIX page's timer doesn't run:
    // a page change shows it the processor's routes again.)
    const auto reshow = [&]
    {
        const auto id = editor.getCurrentPageId();
        editor.showPage (id == "PATCH" ? "MAIN" : "PATCH");
        editor.showPage (id);
        settle (300);
    };
    const auto mouseAt = [] (juce::Component& component, juce::Point<float> at, juce::Point<float> from, juce::ModifierKeys mods)
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &component, &component, juce::Time::getCurrentTime(), from, juce::Time::getCurrentTime(), 1, false);
    };
    const auto shownRows = [&]
    {
        std::vector<MatrixRow*> found, visible;
        findAll<MatrixRow> (editor, found);
        for (auto* row : found)
            if (visibleInTree (row))
                visible.push_back (row);
        std::sort (visible.begin(), visible.end(), [] (auto* a, auto* b) { return a->getY() < b->getY(); });
        return visible;
    };
    const auto render = [] (IlanaSynthAudioProcessor& synth)
    {
        std::vector<float> out;
        for (int block = 0; block < 160; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 512);
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 1)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (block == 90)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            synth.processBlock (buffer, midi);
            for (int channel = 0; channel < 2; ++channel)
                out.insert (out.end(), buffer.getReadPointer (channel), buffer.getReadPointer (channel) + buffer.getNumSamples());
        }
        return out;
    };

    // 5: no factory preset keeps a route with no source, and clearing them
    // (the load's last step) changes no sound: each preset that had one renders
    // bit for bit the same with and without the clearing, on fresh processors.
    {
        juce::StringArray orphans, had;
        IlanaSynthAudioProcessor::clearSourcelessRoutes = false;
        for (int index = 0; index < names.size(); ++index)
        {
            processor.loadFactoryPreset (index);
            for (int slot = 0; slot < Mod::maxSlots; ++slot)
                if (const auto routing = processor.readModSlot (slot); routing.source == Mod::Source::None && routing.destination != 0)
                {
                    had.add (names[index]);
                    break;
                }
        }
        IlanaSynthAudioProcessor::clearSourcelessRoutes = true;
        for (int index = 0; index < names.size(); ++index)
        {
            processor.loadFactoryPreset (index);
            for (int slot = 0; slot < Mod::maxSlots; ++slot)
                if (const auto routing = processor.readModSlot (slot); routing.source == Mod::Source::None && routing.destination != 0)
                    orphans.add (names[index] + " slot " + juce::String (slot + 1));
        }
        expect (orphans.isEmpty(), "no factory preset loads a route with no source (" + juce::String (had.size()) + " had one before the load clears them"
                                       + (orphans.isEmpty() ? juce::String() : "; still: " + orphans.joinIntoString (", ")) + ")");
        std::cout << "  presets with a sourceless route in their recipe or voicing: " << had.joinIntoString (", ") << std::endl;

        // The audio sees only active routes (Mod::Slot::isActive) and the other
        // parameters: those must match exactly with and without the clearing.
        // Then the render: bit for bit where the preset renders the same twice
        // (a preset whose own render varies run to run is reported, its state
        // check stands).
        const auto audibleState = [] (IlanaSynthAudioProcessor& synth)
        {
            std::map<juce::String, float> state;
            for (auto* parameter : synth.getParameters())
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                {
                    const auto id = withId->paramID;
                    if (id.startsWith ("mod") && juce::CharacterFunctions::isDigit (id[3]) && id.containsChar ('_'))
                        if (! synth.readModSlot (id.substring (3).getIntValue() - 1).isActive())
                            continue;
                    state[id] = parameter->getValue();
                }
            return state;
        };
        juce::StringArray differ, stateDiffer, varies;
        auto rendered = 0;
        const auto renderPreset = [&] (const juce::String& name, bool clearing, std::map<juce::String, float>* state)
        {
            IlanaSynthAudioProcessor::clearSourcelessRoutes = clearing;
            auto synth = std::make_unique<IlanaSynthAudioProcessor>();
            synth->prepareToPlay (48000.0, 512);
            synth->loadFactoryPreset (names.indexOf (name));
            if (state != nullptr)
                *state = audibleState (*synth);
            auto out = render (*synth);
            IlanaSynthAudioProcessor::clearSourcelessRoutes = true;
            return out;
        };
        for (const auto& name : had)
        {
            // Three unchanged renders and up to three cleared ones: the cleared
            // render must equal an unchanged one; a preset whose unchanged
            // renders differ among themselves is reported as varying.
            std::map<juce::String, float> stateOff, stateOn;
            std::vector<std::vector<float>> off, on;
            off.push_back (renderPreset (name, false, &stateOff));
            on.push_back (renderPreset (name, true, &stateOn));
            off.push_back (renderPreset (name, false, nullptr));
            off.push_back (renderPreset (name, false, nullptr));
            const auto matches = [&]
            {
                for (const auto& a : on)
                    for (const auto& b : off)
                        if (a == b)
                            return true;
                return false;
            };
            while (! matches() && on.size() < 3)
                on.push_back (renderPreset (name, true, nullptr));
            ++rendered;
            if (stateOff != stateOn)
                stateDiffer.add (name);
            const auto offVaries = off[0] != off[1] || off[0] != off[2];
            if (offVaries)
                varies.add (name);
            if (! matches() && ! offVaries)
                differ.add (name);
        }
        expect (stateDiffer.isEmpty(), "clearing the sourceless routes leaves every parameter the audio reads as it was"
                                           + (stateDiffer.isEmpty() ? juce::String() : " (differ: " + stateDiffer.joinIntoString (", ") + ")"));
        expect (differ.isEmpty() && rendered == had.size(), "clearing the sourceless routes leaves every such preset bit-identical ("
                                                                + juce::String (rendered - varies.size()) + " rendered bit for bit"
                                                                + (varies.isEmpty() ? juce::String() : "; " + varies.joinIntoString (", ") + " renders differently run to run even unchanged")
                                                                + (differ.isEmpty() ? juce::String() : "; differ: " + differ.joinIntoString (", ")) + ")");
    }

    // A source-less row draws its switch off; STEREO is left out where the destination can't be stereo.
    load ("FM E-Piano");
    editor.showPage ("MATRIX");
    settle (400);
    {
        auto stereoShown = 0, stereoHiddenWrong = 0, rows = 0;
        for (auto* row : shownRows())
        {
            ++rows;
            const auto routing = processor.readModSlot (row->getSlotIndex());
            const auto can = Mod::canBeStereoAnywhere (routing.destination);
            stereoShown += row->getStereoButton().isVisible() ? 1 : 0;
            stereoHiddenWrong += row->getStereoButton().isVisible() != can ? 1 : 0;
        }
        expect (rows > 0 && stereoHiddenWrong == 0, "MATRIX: STEREO shows only where the destination can be stereo ("
                                                       + juce::String (stereoShown) + " of " + juce::String (rows) + " rows)");

        const auto spare = [&]
        {
            for (int slot = 0; slot < Mod::maxSlots; ++slot)
                if (const auto routing = processor.readModSlot (slot); routing.source == Mod::Source::None && routing.destination == 0)
                    return slot;
            return -1;
        }();
        processor.setModSlotValue (spare, "dst", (float) (int) Mod::Destination::FxReverbMix);
        processor.setModSlotValue (spare, "amt", 0.3f);
        reshow();
        MatrixRow* orphan = nullptr;
        for (auto* row : shownRows())
            if (row->getSlotIndex() == spare)
                orphan = row;
        expect (orphan != nullptr && ! orphan->getBypassButton().getToggleState() && ! orphan->getBypassButton().isEnabled(),
                "MATRIX: a row with no source draws its switch off");
        processor.clearModSlot (spare);
        reshow();
    }

    // 11: Cello Section (7 routes) opens the curve dock (rows at their base
    // height); Neuro Wobble's 15 routes fit without scrolling (no headings then).
    load ("Cello Section");
    reshow();
    if (auto* page = editor.getCurrentPage())
    {
        auto* remap = findChild<RemapEditor> (*page);
        const auto visible = shownRows();
        expect (remap != nullptr && visibleInTree (remap) && ! visible.empty() && visible[0]->getHeight() == MatrixRow::rowHeight,
                "MATRIX: 7 routes open the curve dock, the rows at " + juce::String (MatrixRow::rowHeight) + " px ("
                    + juce::String (visible.empty() ? 0 : visible[0]->getHeight()) + ")");

        // 33: the dock's right half is the destination's knob, live.
        KnobControl* dockKnob = nullptr;
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*page, knobs);
        for (auto* knob : knobs)
            if (knob->getProperties().contains ("matrixDock") && visibleInTree (knob))
                dockKnob = knob;
        const auto slot = remap != nullptr ? remap->getSlotIndex() : -1;
        if (dockKnob != nullptr)
            dockKnob->syncRoutings(); // (its timer doesn't run without a window)
        expect (dockKnob != nullptr && slot >= 0 && dockKnob->getRingDestination() == processor.readModSlot (slot).destination
                    && dockKnob->getNumRoutings() >= 1,
                "MATRIX: the curve dock shows its route's destination knob with its rings ("
                    + (dockKnob == nullptr ? juce::String ("no knob") : juce::String (dockKnob->getRingDestination()) + " / "
                       + juce::String (slot >= 0 ? processor.readModSlot (slot).destination : -1) + ", " + juce::String (dockKnob->getNumRoutings()) + " rings")
                    + ")");
    }

    load ("Neuro Wobble");
    reshow();
    if (auto* page = editor.getCurrentPage())
    {
        const auto visible = shownRows();
        auto lastBottom = 0;
        for (auto* row : visible)
            lastBottom = juce::jmax (lastBottom, page->getLocalArea (row->getParentComponent(), row->getBounds()).getBottom());
        auto* viewport = visible.empty() ? nullptr : visible[0]->findParentComponentOfClass<juce::Viewport>();
        const auto scrolls = viewport != nullptr && viewport->getViewedComponent() != nullptr
                             && viewport->getViewedComponent()->getHeight() > viewport->getHeight();
        expect (visible.size() >= 15 && ! scrolls, "MATRIX: Neuro Wobble's " + juce::String (visible.size()) + " routes fit without scrolling");

        // 6: a heading (where there is room for them) names its source as its rows do.
        processor.clearModSlot (visible.back()->getSlotIndex());
        processor.clearModSlot (visible[visible.size() - 2]->getSlotIndex());
        processor.clearModSlot (visible[visible.size() - 3]->getSlotIndex());
        processor.clearModSlot (visible[visible.size() - 4]->getSlotIndex());
        processor.clearModSlot (visible[visible.size() - 5]->getSlotIndex());
        reshow();
        const auto fewer = shownRows();
        // (The first row's group heading sits right over it when headings show.)
        if (! fewer.empty() && fewer[0]->getY() >= 18)
        {
            auto& canvas = *fewer[0]->getParentComponent();
            const auto source = (int) processor.readModSlot (fewer[0]->getSlotIndex()).source;
            const juce::Point<float> at (20.0f, (float) fewer[0]->getY() - 9.0f);
            auto groupOn = [&] (bool wanted)
            {
                auto all = true;
                for (auto* row : fewer)
                    if ((int) processor.readModSlot (row->getSlotIndex()).source == source)
                        all = all && processor.readModSlot (row->getSlotIndex()).bypass != wanted;
                return all;
            };
            canvas.mouseUp (mouseAt (canvas, at, at, juce::ModifierKeys()));
            settle (200);
            const auto off = groupOn (false);
            canvas.mouseUp (mouseAt (canvas, at, at, juce::ModifierKeys()));
            settle (200);
            expect (off && groupOn (true), "MATRIX: a click on a source's heading switches its routes off, again on");
            auto* tip = dynamic_cast<juce::TooltipClient*> (&canvas);
            expect (tip != nullptr, "MATRIX: the list canvas gives its headings a tooltip");
        }
        else
            expect (false, "MATRIX: 10 routes by source show their group headings (first row at " + juce::String (fewer.empty() ? -1 : fewer[0]->getY()) + ")");

        // 33: the row menu: duplicate, solo, show destination, remove.
        auto rows = shownRows();
        if (rows.size() >= 3 && rows[0]->onRowMenuChoice != nullptr)
        {
            auto* first = rows[0];
            const auto original = processor.readModSlot (first->getSlotIndex());
            const auto before = (int) rows.size();
            first->onRowMenuChoice (1);
            settle (300);
            juce::PopupMenu::dismissAllActiveMenus();
            reshow();
            auto duplicated = false;
            for (auto* row : shownRows())
                if (const auto routing = processor.readModSlot (row->getSlotIndex()); row != first && routing.source == original.source
                                                                                      && routing.destination == 0 && std::abs (routing.depth - original.depth) < 1.0e-4f)
                    duplicated = true;
            expect ((int) shownRows().size() == before + 1 && duplicated, "MATRIX: Duplicate adds a row with the same source and amount and no destination");
            for (auto* row : shownRows())
                if (processor.readModSlot (row->getSlotIndex()).destination == 0)
                    processor.clearModSlot (row->getSlotIndex());
            reshow();

            rows = shownRows();
            first = rows[0];
            first->onRowMenuChoice (3);
            settle (200);
            auto othersOff = true;
            for (auto* row : rows)
                if (row != first)
                    othersOff = othersOff && processor.readModSlot (row->getSlotIndex()).bypass;
            const auto soloOn = ! processor.readModSlot (first->getSlotIndex()).bypass;
            first->onRowMenuChoice (3);
            settle (200);
            auto othersBack = true;
            for (auto* row : rows)
                othersBack = othersBack && ! processor.readModSlot (row->getSlotIndex()).bypass;
            expect (othersOff && soloOn && othersBack, "MATRIX: Solo switches every other route off, and back on when it ends");

            const auto destination = processor.readModSlot (first->getSlotIndex()).destination;
            first->onRowMenuChoice (2);
            settle (500);
            KnobControl* shown = nullptr;
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (editor, knobs);
            for (auto* knob : knobs)
                if (knob->getRingDestination() == destination && visibleInTree (knob) && ! knob->getProperties().contains ("matrixDock"))
                    shown = knob;
            expect (editor.getCurrentPageId() != "MATRIX" && shown != nullptr,
                    "MATRIX: Show destination opens " + editor.getCurrentPageId() + " with the knob on screen");
            editor.showPage ("MATRIX");
            settle (300);
            const auto removed = first->getSlotIndex();
            first->onRowMenuChoice (4);
            settle (300);
            expect (processor.readModSlot (removed).source == Mod::Source::None && processor.readModSlot (removed).destination == 0,
                    "MATRIX: Remove clears the row");
        }
    }

    // 7: on Init only the first idle macro shows + ASSIGN; the others on hover.
    load ("Init");
    editor.showPage ("MAIN");
    settle (400);
    {
        std::vector<StripKnob*> macros;
        findAll<StripKnob> (editor, macros);
        auto shown = 0, idle = 0;
        for (auto* macro : macros)
            if (macro->getMacroIndex() >= 0 && visibleInTree (macro) && ! macro->isAssigned())
            {
                ++idle;
                shown += macro->showsAssignLabel() ? 1 : 0;
            }
        expect (idle >= 2 && shown == 1, "Init: one + ASSIGN of " + juce::String (idle) + " idle macros (" + juce::String (shown) + " shown)");

        // 26: + ASSIGN arms the macro; the next knob clicked gets the route, on PLAY.
        StripKnob* first = nullptr;
        for (auto* macro : macros)
            if (macro->getMacroIndex() == 0)
                first = macro;
        KnobControl* cutoff = nullptr;
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == "f1_cutoff" && visibleInTree (knob))
                cutoff = knob;
        if (first != nullptr && first->onAssign != nullptr && cutoff != nullptr)
        {
            const auto before = processor.getNumUsedModSlots();
            first->onAssign (0);
            settle (100);
            const auto armed = armedModSource() == (int) Mod::macroSourceFor (0) && editor.getCurrentPageId() == "MAIN";
            auto& slider = cutoff->getSlider();
            const auto valueBefore = slider.getValue();
            const auto centre = slider.getLocalBounds().getCentre().toFloat();
            slider.mouseDown (mouseAt (slider, centre, centre, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
            slider.mouseUp (mouseAt (slider, centre, centre, juce::ModifierKeys()));
            settle (200);
            auto routed = false;
            for (int slot = 0; slot < Mod::maxSlots; ++slot)
                if (const auto routing = processor.readModSlot (slot); routing.source == Mod::macroSourceFor (0)
                                                                       && routing.destination == (int) Mod::Destination::Filter1Cutoff)
                    routed = true;
            expect (armed && routed && armedModSource() == 0 && processor.getNumUsedModSlots() == before + 1 && slider.getValue() == valueBefore,
                    "+ ASSIGN arms the macro and the next knob clicked gets the route (the knob doesn't turn, PLAY stays)");

            // Esc (or CANCEL) disarms.
            setArmedModSource ((int) Mod::Source::Lfo2);
            editor.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            expect (armedModSource() == 0, "Esc cancels click to assign");

            // Alt-drag with a source pinned makes the route and sets its depth.
            KnobControl* reso = nullptr;
            for (auto* knob : knobs)
                if (knob->getParameterId() == "f1_reso" && visibleInTree (knob))
                    reso = knob;
            if (reso != nullptr)
            {
                pinnedModSource() = (int) Mod::Source::Lfo2;
                auto& resoSlider = reso->getSlider();
                resoSlider.setPopupDisplayEnabled (false, false, nullptr); // never open a real popup window in a test
                const auto resoBefore = resoSlider.getValue();
                const auto at = resoSlider.getLocalBounds().getCentre().toFloat();
                const juce::ModifierKeys alt (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier);
                resoSlider.mouseDown (mouseAt (resoSlider, at, at, alt));
                reso->mouseDrag (mouseAt (resoSlider, at.translated (0.0f, -50.0f), at, alt));
                reso->mouseUp (mouseAt (resoSlider, at.translated (0.0f, -50.0f), at, juce::ModifierKeys()));
                resoSlider.mouseUp (mouseAt (resoSlider, at.translated (0.0f, -50.0f), at, juce::ModifierKeys()));
                pinnedModSource() = 0;
                settle (200);
                auto depth = 0.0f;
                for (int slot = 0; slot < Mod::maxSlots; ++slot)
                    if (const auto routing = processor.readModSlot (slot); routing.source == Mod::Source::Lfo2
                                                                           && routing.destination == (int) Mod::Destination::Filter1Reso)
                        depth = routing.depth;
                expect (depth > 0.2f && resoSlider.getValue() == resoBefore, "Alt-drag with LFO 2 pinned routes it to RESO and sets the depth ("
                                                                                 + juce::String (depth, 2) + ")");
            }
        }
        else
            expect (false, "Init: macro 1's + ASSIGN and PLAY's CUTOFF knob are there");
    }

    // 27: a source dropped on the filter graph routes to CUTOFF.
    load ("Init");
    {
        std::vector<FilterDisplay*> graphs;
        findAll<FilterDisplay> (editor, graphs);
        FilterDisplay* graph = nullptr;
        for (auto* found : graphs)
            if (visibleInTree (found))
                graph = found;
        auto routed = false;
        if (graph != nullptr && graph->dropSource ((int) Mod::Source::Lfo3))
            for (int slot = 0; slot < Mod::maxSlots; ++slot)
                if (const auto routing = processor.readModSlot (slot); routing.source == Mod::Source::Lfo3 && routing.destination == graph->dropDestination())
                    routed = true;
        expect (routed, "a source dropped on the filter graph routes to CUTOFF");
    }

    // 28: the keyboard strip's mod wheel and the MOD WHEEL pop-up's bar send what MIDI would.
    {
        auto* keyboard = findChild<KeyboardStrip> (editor);
        const auto runBlock = [&]
        {
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
        };
        if (keyboard != nullptr)
        {
            keyboard->moveWheel (1, 0.5f);
            runBlock();
            const auto wheel = processor.getSourceDisplayValue ((int) Mod::Source::ModWheel);
            keyboard->moveWheel (1, 0.0f);
            runBlock();
            expect (std::abs (wheel - 64.0f / 127.0f) < 0.01f && processor.getSourceDisplayValue ((int) Mod::Source::ModWheel) < 0.01f,
                    "the keyboard's mod wheel sends CC 1 (" + juce::String (wheel, 3) + ")");
            if (keyboard->getWidth() > 0)
                expect (! keyboard->wheelBounds (0).isEmpty() && keyboard->wheelBounds (1).getRight() < 80.0f,
                        "the keyboard strip has bend and mod wheels at its left");
        }
        std::vector<ModSourceChip*> chips;
        findAll<ModSourceChip> (editor, chips);
        for (auto* chip : chips)
            if (chip->getSourceIndex() == (int) Mod::Source::ModWheel && visibleInTree (chip))
            {
                editor.toggleSourcePopover (chip->getSourceIndex(), *chip);
                settle (400);
                auto* popover = editor.getSourcePopover();
                juce::Component* bar = nullptr;
                std::function<void (juce::Component&)> find = [&] (juce::Component& parent)
                {
                    for (auto* child : parent.getChildren())
                    {
                        if (auto* client = dynamic_cast<juce::SettableTooltipClient*> (child); client != nullptr && child->isVisible()
                                                                                               && client->getTooltip().startsWith ("Drag to move MOD WHEEL"))
                            bar = child;
                        find (*child);
                    }
                };
                if (popover != nullptr)
                    find (*popover);
                // 19: about 300 x 150 design px, not 620 x 340.
                expect (popover != nullptr && popover->getWidth() <= 320 && popover->getHeight() <= 260,
                        "the MOD WHEEL pop-up is compact (" + (popover != nullptr ? juce::String (popover->getWidth()) + " x " + juce::String (popover->getHeight()) : juce::String ("none")) + ")");
                if (bar != nullptr)
                {
                    const juce::Point<float> at (3.0f + (float) (bar->getWidth() - 6) * 0.25f, (float) bar->getHeight() * 0.5f);
                    bar->mouseDown (mouseAt (*bar, at, at, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
                    bar->mouseUp (mouseAt (*bar, at, at, juce::ModifierKeys()));
                    runBlock();
                    const auto wheel = processor.getSourceDisplayValue ((int) Mod::Source::ModWheel);
                    expect (std::abs (wheel - 32.0f / 127.0f) < 0.02f, "the MOD WHEEL pop-up's bar moves the wheel (" + juce::String (wheel, 3) + ")");
                    if (keyboard != nullptr)
                        keyboard->moveWheel (1, 0.0f);
                    runBlock();
                }
                else
                    expect (false, "the MOD WHEEL pop-up has a value bar to drag");
                editor.toggleSourcePopover (chip->getSourceIndex(), *chip);
                settle (300);
                break;
            }
    }

    // OP LFO's pop-up (ilana 2026-11-14: "op lfo looks wrong"): its editor ignored the pop-up's title row, so the
    // graph covered the title and SHAPE sat over the close button. SHAPE now sits in the title row as the pool
    // LFOs' does, clear of the close button, the graph and knobs under the title, and the close button still clicks.
    {
        if (auto* envChoice = processor.apvts.getParameter ("osc1_amp_env"))
            envChoice->setValueNotifyingHost (envChoice->convertTo0to1 ((float) OperatorEg::envelopeChoice));
        settle (400);
        std::vector<ModSourceChip*> chips;
        findAll<ModSourceChip> (editor, chips);
        auto checked = false;
        for (auto* chip : chips)
            if (chip->getSourceIndex() == (int) Mod::Source::OpLfo && visibleInTree (chip))
            {
                editor.toggleSourcePopover (chip->getSourceIndex(), *chip);
                settle (400);
                auto* popover = editor.getSourcePopover();
                std::vector<OperatorLfoEditor*> lfoEditors;
                if (popover != nullptr)
                    findAll<OperatorLfoEditor> (*popover, lfoEditors);
                juce::Button* close = nullptr;
                std::vector<juce::TextButton*> buttons;
                if (popover != nullptr)
                    findAll<juce::TextButton> (*popover, buttons);
                for (auto* button : buttons)
                    if (button->getButtonText() == juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) && button->getName() != "removeRoute" && visibleInTree (button))
                        close = button;
                OperatorLfoEditor* lfoEditor = nullptr;
                for (auto* candidate : lfoEditors)
                    if (visibleInTree (candidate))
                        lfoEditor = candidate;
                checked = true;
                if (lfoEditor == nullptr || close == nullptr)
                {
                    expect (false, "the OP LFO pop-up shows its editor and a close button");
                    break;
                }
                const auto inPopover = [popover] (juce::Component& c) { return popover->getLocalArea (c.getParentComponent(), c.getBounds()); };
                const auto shapeBox = inPopover (lfoEditor->getShape());
                const auto closeBox = inPopover (*close);
                juce::Rectangle<int> graph;
                for (auto* child : lfoEditor->getChildren())
                    if (dynamic_cast<OperatorLfoView*> (child) != nullptr)
                        graph = inPopover (*child);
                const auto titleBottom = closeBox.getCentreY() + 13; // (the pop-up's 26 px title row, centred on the close button)
                expect (shapeBox.getBottom() <= titleBottom && shapeBox.getRight() < closeBox.getX() && ! shapeBox.intersects (closeBox),
                        "OP LFO's SHAPE sits in the pop-up's title row, clear of the close button (" + shapeBox.toString() + " / " + closeBox.toString() + ")");
                expect (! graph.isEmpty() && graph.getY() >= titleBottom,
                        "OP LFO's graph starts under the pop-up's title (" + graph.toString() + ", title to " + juce::String (titleBottom) + ")");
                expect (popover->getComponentAt (closeBox.getCentre()) == close,
                        "the OP LFO pop-up's close button takes clicks");
                editor.toggleSourcePopover (chip->getSourceIndex(), *chip);
                settle (300);
                break;
            }
        expect (checked, "an OP LFO chip shows while an oscillator plays the Operator Env");
        if (auto* envChoice = processor.apvts.getParameter ("osc1_amp_env"))
            envChoice->setValueNotifyingHost (0.0f);
        settle (300);
    }

    processor.loadFactoryPreset (names.indexOf ("Neuro Wobble"));
    editor.showPage ("MAIN");
    settle (300);
}


