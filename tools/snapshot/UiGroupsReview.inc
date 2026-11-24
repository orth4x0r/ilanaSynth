
// Display blind spots (ilana 2026-11-21): each scene's picture changes with
// the route, note or knob it used to miss.
// A group that loads presets puts the patch back as it found it (later groups
// lean on earlier ones' state, e.g. SAMPLE_ZOOM on BOUNCE's sample in OSC 2).
struct UiKeepPatch
{
    explicit UiKeepPatch (IlanaSynthAudioProcessor& p) : processor (p) { processor.getStateInformation (state); }
    ~UiKeepPatch()
    {
        processor.setStateInformation (state.getData(), (int) state.getSize());
        settle (200);
    }
    IlanaSynthAudioProcessor& processor;
    juce::MemoryBlock state;
};

void uiGroup_BLINDSPOTS (UiCtx& ctx)
{
    const UiKeepPatch keep (ctx.processor);
    const auto shotsDir = juce::SystemStats::getEnvironmentVariable ("ILANA_UITEST_SHOTS", "");
    const auto shots = runBlindspotScenes (ctx, shotsDir.isNotEmpty() ? juce::File (shotsDir) : juce::File());
    expect (shots.size() >= 13, "every blind-spot scene ran (" + juce::String ((int) shots.size()) + ")");
    for (const auto& shot : shots)
    {
        auto count = 0;
        for (int j = 0; j < juce::jmin (shot.plain.getHeight(), shot.routed.getHeight()); ++j)
            for (int i = 0; i < juce::jmin (shot.plain.getWidth(), shot.routed.getWidth()); ++i)
                count += shot.plain.getPixelAt (i, j) != shot.routed.getPixelAt (i, j) ? 1 : 0;
        expect (count > 40, "the picture follows what it used to miss: " + shot.name + " (" + juce::String (count) + " pixels moved)");
    }
}

// The report's pictures: ilanaSnapshot --blindspots DIR
int runBlindspots (const juce::File& dir)
{
    dir.createDirectory();
    UiCtx ctx;
    const auto shots = runBlindspotScenes (ctx, dir);
    ctx.closeEditor();
    return shots.empty() ? 1 : 0;
}

void uiGroup_SHAPE_DISPLAY (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    const auto shotsDir = juce::SystemStats::getEnvironmentVariable ("ILANA_UITEST_SHOTS", "");
    const auto findWave = [&editor] (int osc)
    {
        std::vector<WaveDisplay*> waves;
        findAll<WaveDisplay> (*editor, waves);
        WaveDisplay* wave = nullptr;
        for (auto* candidate : waves)
            if (candidate->getOscIndex() == osc && (wave == nullptr || (visibleInTree (candidate) && ! candidate->isCompact())))
                wave = candidate;
        return wave;
    };
    const auto picture = [&] (WaveDisplay& wave, const juce::String& name)
    {
        settle (250);
        auto image = wave.createComponentSnapshot (wave.getLocalBounds());
        if (shotsDir.isNotEmpty())
            save (wave, juce::File (shotsDir).getChildFile (name));
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

    // A table of many frames whose neighbours differ, for the FRAME BLEND pictures.
    auto tableChoice = -1;
    auto numFrames = 0;
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("osc1_table")))
        for (int index = 0; index < choice->choices.size() && tableChoice < 0; ++index)
            if (const auto* table = processor.getWavetable (index); table != nullptr && table->getNumFrames() >= 4 && table->getNumFrames() <= 64)
            {
                const auto middle = table->getNumFrames() / 2;
                const auto* a = table->getFrameData (0, middle);
                const auto* b = table->getFrameData (0, middle + 1);
                auto gap = 0.0f;
                for (int i = 1; i <= Wavetable::frameSize; ++i)
                    gap = juce::jmax (gap, std::abs (a[i] - b[i]));
                if (gap > 0.5f)
                {
                    tableChoice = index;
                    numFrames = table->getNumFrames();
                }
            }
    expect (tableChoice >= 0, "a table with neighbouring frames that differ, for the FRAME BLEND pictures");

    // (Every parameter this group sets goes back as it was.)
    std::vector<std::pair<juce::String, float>> before;
    for (const auto* id : { "osc1_on", "osc1_mode", "osc1_warp", "osc1_warp_amt", "osc1_warp2", "osc1_warp2_amt", "osc1_unison", "osc1_table",
                            "osc1_frame", "osc1_spectral", "osc1_spectral_amt", "osc1_frame_blend", "osc4_on", "osc4_mode", "sub_spectral",
                            "osc4_spectral", "osc4_spectral_amt", "osc4_table", "osc2_on", "osc2_mode", "osc2_table", "osc2_semi", "hard_sync" })
        before.emplace_back (id, readParam (id));

    setParam ("osc1_on", 1.0f);
    setParam ("osc1_mode", 0.0f);
    setParam ("osc1_warp", 0.0f);
    setParam ("osc1_warp_amt", 0.0f);
    setParam ("osc1_warp2", 0.0f);
    setParam ("osc1_warp2_amt", 0.0f);
    setParam ("osc1_unison", 1.0f);
    if (tableChoice >= 0)
        setParam ("osc1_table", (float) tableChoice);
    setParam ("osc1_frame", 0.0f);
    selectOscTab (0);
    settle (150);

    if (auto* wave = findWave (0))
    {
        // Spectral warp: the picture follows the amount without the audio thread building anything.
        setParam ("osc1_spectral", 0.0f);
        const auto plain = picture (*wave, "spectral-off.png");
        setParam ("osc1_spectral", 1.0f); // Stretch
        setParam ("osc1_spectral_amt", 0.15f);
        const auto low = picture (*wave, "spectral-stretch-15.png");
        setParam ("osc1_spectral_amt", 0.9f);
        const auto high = picture (*wave, "spectral-stretch-90.png");
        expect (differs (plain, low) > 100, "the wave display draws the spectral warp (Stretch 15 %)");
        expect (differs (low, high) > 100, "the wave display follows the SPEC AMT knob (Stretch 15 % -> 90 %)");

        for (const auto& [mode, file] : { std::pair<int, const char*> { 5, "spectral-smear-90.png" }, { 3, "spectral-odd-even-90.png" },
                                          { 11, "spectral-skew-90.png" } })
        {
            setParam ("osc1_spectral", (float) mode);
            expect (differs (plain, picture (*wave, file)) > 100, juce::String ("the wave display draws spectral warp ") + file);
        }

        // The warped frame matches the table the oscillator plays (same samples up to the table's normalising).
        if (tableChoice >= 0)
            if (const auto* table = processor.getWavetable (tableChoice))
            {
                juce::dsp::FFT fft (11);
                for (const auto mode : { 1, 5, 10 })
                {
                    const auto whole = SpectralWarp::warpTable (*table, mode, 0.6f);
                    const auto frame = numFrames / 2;
                    const auto one = SpectralWarp::warpOneFrame (*table, frame, mode, 0.6f, fft);
                    const auto* played = whole->getFrameData (0, frame);
                    auto peakOne = 0.0f, peakPlayed = 0.0f;
                    for (int i = 0; i < Wavetable::frameSize; ++i)
                    {
                        peakOne = juce::jmax (peakOne, std::abs (one[(size_t) i]));
                        peakPlayed = juce::jmax (peakPlayed, std::abs (played[i + 1]));
                    }
                    auto error = 0.0f;
                    for (int i = 0; i < Wavetable::frameSize; ++i)
                        error = juce::jmax (error, std::abs (one[(size_t) i] / juce::jmax (1.0e-6f, peakOne) - played[i + 1] / juce::jmax (1.0e-6f, peakPlayed)));
                    expect (error < 0.02f, "the display's warped frame is the played table's frame (mode " + juce::String (mode)
                                               + ", error " + juce::String (error, 4) + ")");
                }
            }

        setParam ("osc1_spectral", 0.0f);
        setParam ("osc1_spectral_amt", 0.5f);

        // FRAME between two frames: Crossfade and Spectral draw the in-between cycle, Stepped the nearest frame.
        if (numFrames >= 2)
        {
            const auto between = ((float) (numFrames / 2) + 0.5f) / (float) (numFrames - 1);
            setParam ("osc1_frame_blend", 2.0f); // Stepped
            setParam ("osc1_frame", between);
            const auto stepped = picture (*wave, "blend-stepped.png");
            setParam ("osc1_frame_blend", 0.0f); // Crossfade
            const auto crossfade = picture (*wave, "blend-crossfade.png");
            setParam ("osc1_frame_blend", 1.0f); // Spectral
            const auto spectral = picture (*wave, "blend-spectral.png");
            expect (differs (stepped, crossfade) > 100, "FRAME between two frames draws the crossfaded cycle");
            expect (differs (crossfade, spectral) > 100, "FRAME BLEND Spectral draws its own in-between cycle");
            setParam ("osc1_frame_blend", 0.0f);
        }

        setParam ("osc1_frame", 0.0f);
    }
    else
    {
        expect (false, "OSC 1's wave display is shown for the shape pictures");
    }

    // OSC 4 draws its own spectral warp (it used to show OSC 3's).
    setParam ("osc4_on", 1.0f);
    setParam ("osc4_mode", 0.0f);
    setParam ("sub_spectral", 0.0f);
    setParam ("osc4_spectral", 0.0f);
    if (tableChoice >= 0)
        setParam ("osc4_table", (float) tableChoice);
    selectOscTab (3);
    settle (150);

    if (auto* wave = findWave (3))
    {
        const auto plain = picture (*wave, "osc4-spectral-off.png");
        setParam ("osc4_spectral", 1.0f);
        setParam ("osc4_spectral_amt", 0.9f);
        expect (differs (plain, picture (*wave, "osc4-spectral-stretch-90.png")) > 100, "OSC 4's wave display draws its own spectral warp");
        setParam ("osc4_spectral", 0.0f);
    }
    else
    {
        expect (false, "OSC 4's wave display is shown");
    }
    setParam ("osc4_on", 0.0f);

    // SYNC 2 TO 1: OSC 2's picture is one OSC 1 cycle with OSC 2 restarting at its end.
    setParam ("osc2_on", 1.0f);
    setParam ("osc2_mode", 0.0f);
    if (tableChoice >= 0)
        setParam ("osc2_table", (float) tableChoice);
    setParam ("osc2_semi", 7.0f);
    setParam ("hard_sync", 0.0f);
    selectOscTab (1);
    settle (150);

    if (auto* wave = findWave (1))
    {
        const auto free = picture (*wave, "sync-off.png");
        setParam ("hard_sync", 1.0f);
        const auto synced = picture (*wave, "sync-on-semi7.png");
        setParam ("osc2_semi", 19.0f);
        const auto higher = picture (*wave, "sync-on-semi19.png");
        expect (differs (free, synced) > 100, "SYNC 2 TO 1 shows on OSC 2's wave display");
        expect (differs (synced, higher) > 100, "the synced picture follows OSC 2's pitch");
    }
    else
    {
        expect (false, "OSC 2's wave display is shown");
    }

    for (const auto& [id, value] : before)
        setParam (id, value);
    selectOscTab (0);
    settle (150);
}

void uiGroup_SAMPLE_ZOOM (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Step 30: the sample display zooms, and a typed position sets START / END.
    {
        // (Needs OSC 2 in Sample mode: BOUNCE leaves it so; alone, this bounces it first.)
        if (processor.getBounceState() != IlanaSynthAudioProcessor::BounceState::Done)
        {
            pages->showPage ("MAIN");
            settle (200);
            uiBounceIntoOsc2 (ctx);
        }
        std::vector<WaveDisplay*> waves;
        findAll<WaveDisplay> (*editor, waves);
        WaveDisplay* wave = nullptr;
        for (auto* candidate : waves)
            if (visibleInTree (candidate) && candidate->getOscIndex() == 1)
                wave = candidate;
        expect (wave != nullptr, "OSC 2's sample display is shown");

        if (wave != nullptr)
        {
            const auto read = [&processor] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
            expect (wave->getSampleView().isFull(), "a sample display starts fitted");
            // ILANA_UITEST_SHOTS=<dir>: pictures of the zoom, for review.
            const auto shotsDir = juce::SystemStats::getEnvironmentVariable ("ILANA_UITEST_SHOTS", "");
            const auto shot = [&] (const char* name)
            {
                if (shotsDir.isNotEmpty())
                {
                    settle (150);
                    save (*editor, juce::File (shotsDir).getChildFile (name));
                }
            };
            shot ("sample-fitted.png");
            wave->zoomSample (0.5, 4.0);
            expect (std::abs (wave->getSampleView().span() - 0.25) < 1.0e-6, "zooming in 4x shows a quarter of the sample");
            wave->setSamplePosition (1, 0.9f);
            const auto start = read ("osc2_sample_start"), end = read ("osc2_sample_end");
            expect (start < end && start > 0.8f, "a typed START lands, and stays under END (" + juce::String (start, 3) + " < " + juce::String (end, 3) + ")");
            const auto view = wave->getSampleView();
            expect ((double) start >= view.lo - 1.0e-6 && (double) start <= view.hi + 1.0e-6, "the zoom window follows a typed position");
            shot ("sample-zoomed.png");
            wave->showPositionEditor (1);
            shot ("sample-typed-start.png");
            wave->zoomSample (0.5, 0.001);
            expect (wave->getSampleView().isFull(), "zooming out fits the sample");
            wave->setSamplePosition (1, 0.0f);
            expect (read ("osc2_sample_start") < 0.001f, "a typed START of zero goes back to the head");

            // Step 27: LOOP PTS gives the loop its own handles.
            const auto set = [&processor] (const char* id, float value)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            const auto plot = wave->samplePlotForTest();
            const auto midY = plot.getCentreY();
            set ("osc2_sample_loop", 1.0f);
            set ("osc2_sample_loop_own", 0.0f);
            wave->setSamplePosition (2, 1.0f);
            settle (150);
            expect (wave->handleAtForTest (wave->sampleXForTest (0.25), midY) == 0, "LOOP PTS off: no loop handles on the wave");
            shot ("sample-loop-whole.png");
            set ("osc2_sample_loop_own", 1.0f);
            set ("osc2_sample_loop_start", 0.25f);
            set ("osc2_sample_loop_end", 0.75f);
            settle (150);
            expect (wave->handleAtForTest (wave->sampleXForTest (0.25), midY) == 3, "LOOP PTS on: the loop's start handle is on the wave");
            expect (wave->handleAtForTest (wave->sampleXForTest (0.75), midY) == 4, "LOOP PTS on: the loop's end handle is on the wave");
            expect (wave->handleAtForTest (plot.getX() + 1.0f, midY) == 1 && wave->handleAtForTest (plot.getRight() - 1.0f, midY) == 2,
                    "START and END keep their handles");
            shot ("sample-loop-points.png");

            // Step 29: ROOT and ROOT FINE on the card, and DETECT in the header.
            set ("osc2_sample_root", 45.0f);
            settle (150);
            expect (! processor.detectSampleRoot (1).isEmpty(), "DETECT always says what it found");
            shot ("sample-root.png");
            wave->setSamplePosition (3, 0.4f);
            expect (std::abs (read ("osc2_sample_loop_start") - 0.4f) < 0.002f, "a typed LOOP START lands");
            wave->setSamplePosition (4, 0.3f);
            expect (read ("osc2_sample_loop_end") > read ("osc2_sample_loop_start"), "LOOP END stays after LOOP START");
            wave->setSamplePosition (3, 1.0f);
            expect (read ("osc2_sample_loop_start") < read ("osc2_sample_loop_end"), "LOOP START stays before LOOP END");
            wave->setSamplePosition (4, 0.6f);
            wave->setSamplePosition (3, 0.0f);
            wave->setSamplePosition (1, 0.5f);
            expect (wave->handleAtForTest (wave->sampleXForTest (0.5), plot.getY() + 4.0f) == 1,
                    "a loop edge sitting on START: a press on START's flag takes START");
            expect (wave->handleAtForTest (wave->sampleXForTest (0.5), midY) == 3,
                    "a loop edge sitting on START: a press lower down takes the loop's edge");
            shot ("sample-loop-attack-cut.png");
            wave->setSamplePosition (1, 0.0f);
            set ("osc2_sample_loop_own", 0.0f);
            set ("osc2_sample_loop", 0.0f);

            // Step 28: SNAP puts a typed marker on a zero crossing of the wave.
            if (const auto* bounced = processor.getSampleForOsc (1); bounced != nullptr && bounced->getNumSamples() > 4000)
            {
                const auto frames = bounced->getNumSamples();
                const auto* left = bounced->buffer.getReadPointer (0);
                const auto* right = bounced->getNumChannels() > 1 ? bounced->buffer.getReadPointer (1) : nullptr;
                const auto mix = [&] (int i) { return right != nullptr ? (left[i] + right[i]) * 0.5f : left[i]; };
                const auto crossesAt = [&] (float fraction)
                {
                    const auto i = juce::jlimit (0, frames - 2, juce::roundToInt (fraction * (float) frames));
                    for (int k = -2; k <= 2; ++k)
                        if (i + k >= 0 && i + k + 1 < frames && ((mix (i + k) <= 0.0f) != (mix (i + k + 1) <= 0.0f)))
                            return true;
                    return false;
                };
                set ("osc2_sample_snap", 0.0f);
                wave->setSamplePosition (1, 0.31337f);
                expect (std::abs (read ("osc2_sample_start") - 0.31337f) < 0.0001f, "SNAP off: a typed START stays where typed");
                set ("osc2_sample_snap", 1.0f);
                wave->setSamplePosition (1, 0.31337f);
                expect (crossesAt (read ("osc2_sample_start")) && std::abs (read ("osc2_sample_start") - 0.31337f) < 0.02f,
                        "SNAP on: a typed START lands on a zero crossing of the wave");
                set ("osc2_sample_snap", 0.0f);
                wave->setSamplePosition (1, 0.0f);
            }

            // The crossfade knob and SNAP sit on the sample card's third row.
            std::vector<KnobControl*> loopKnobs;
            findAll<KnobControl> (*editor, loopKnobs);
            auto xfadeShown = false;
            for (auto* knob : loopKnobs)
                xfadeShown = xfadeShown || (knob->getParameterId() == "osc2_sample_loop_xfade" && visibleInTree (knob));
            expect (xfadeShown, "the XFADE knob shows on a sample card");
            set ("osc2_sample_loop", 1.0f);
            set ("osc2_sample_loop_xfade", 0.5f);
            shot ("sample-loop-xfade.png");
            set ("osc2_sample_loop_xfade", 0.0f);
            set ("osc2_sample_loop", 0.0f);
        }
    }
}

void uiGroup_PHYSICAL (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // The string has one editor, its oscillator's card (the PHYSICAL tab
    // folded in, 2026-10-10): every control of the physical list is on the
    // card, in its order, with the moving string in the well; the body is
    // on PLAY's BODY tab (no link on the card), the soundboard is the strip's.
    {
        processor.loadFactoryPreset (names.indexOf ("Felt Hammer Board"));
        pages->showPage ("MAIN");
        settle (400);
        juce::String physicalPrefix;
        for (const auto* prefix : OscillatorIds::prefixes)
            if (physicalPrefix.isEmpty() && juce::roundToInt (processor.apvts.getRawParameterValue (juce::String (prefix) + "_mode")->load()) == 1)
                physicalPrefix = prefix;
        selectOscTab (physicalPrefix.getTrailingIntValue() - 1);

        for (const auto excite : { 9, 0, 4, 10, 7 })
        {
            if (auto* parameter = processor.apvts.getParameter (physicalPrefix + "_excite"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) excite));
            settle (300);
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            juce::StringArray listIds, missing;
            for (const auto& row : physicalControlRows (excite))
                for (const auto& spec : row.second)
                    if (dynamic_cast<juce::AudioParameterFloat*> (processor.apvts.getParameter (physicalPrefix + spec.suffix)) != nullptr)
                        listIds.add (physicalPrefix + spec.suffix);
            std::vector<juce::Rectangle<int>> boxes;
            for (const auto& id : listIds)
            {
                auto shown = false;
                for (auto* knob : knobs)
                    if (knob->getParameterId() == id && visibleInTree (knob) && ! knob->getBounds().isEmpty())
                    {
                        shown = true;
                        boxes.push_back (editor->getLocalArea (knob, knob->packedCell().reduced (1, 3)));
                    }
                if (! shown)
                    missing.add (id);
            }
            auto overlap = false;
            for (size_t i = 0; i < boxes.size(); ++i)
                for (size_t j = i + 1; j < boxes.size(); ++j)
                    overlap = overlap || boxes[i].intersects (boxes[j]);
            expect (! listIds.isEmpty() && missing.isEmpty() && ! overlap,
                    "OSC: excite " + juce::String (excite) + ", the card shows every string and exciter control, none overlapping"
                        + (missing.isEmpty() ? juce::String() : " (missing " + missing.joinIntoString (", ") + ")"));
        }
        // The six exciter types and their engine switch (2026-11-11): an old patch's engine shows on its type's
        // switch (Bow = BOWED on LEGACY), every engine is one type's position, and a type icon picks MODAL first.
        {
            auto everyEngineOnce = true;
            for (int excite = 0; excite < 14; ++excite)
            {
                auto seen = 0;
                for (const auto& type : Exciters::types())
                    seen += (int) std::count (type.engines.begin(), type.engines.end(), excite);
                everyEngineOnce = everyEngineOnce && seen == 1;
            }
            expect (Exciters::types().size() == 6 && everyEngineOnce, "OSC: 6 exciter types hold each of the 14 engines once");
            const auto shown = [&] (auto*& found)
            {
                using T = std::remove_pointer_t<std::remove_reference_t<decltype (found)>>;
                std::vector<T*> all;
                findAll<T> (*editor, all);
                found = nullptr;
                for (auto* item : all)
                    if (visibleInTree (item))
                        found = item;
            };
            for (const auto& [excite, type, position, positions] : { std::tuple<int, int, int, int> { 4, 2, 1, 2 }, { 9, 0, 1, 3 }, { 5, 0, 2, 3 },
                                                                       { 0, 1, 1, 3 }, { 13, 2, 0, 2 }, { 10, 5, 0, 1 } })
            {
                if (auto* parameter = processor.apvts.getParameter (physicalPrefix + "_excite"))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) excite));
                settle (300);
                IconPicker* picker = nullptr;
                PositionSwitch* engineSwitch = nullptr;
                shown (picker);
                shown (engineSwitch);
                const auto switchOk = positions == 1 ? engineSwitch == nullptr
                                                     : engineSwitch != nullptr && engineSwitch->size() == positions && engineSwitch->getChosen() == position
                                                           && engineSwitch->getWidth() >= engineSwitch->idealWidth();
                expect (picker != nullptr && picker->size() == 6 && picker->getChosen() == type && switchOk,
                        "OSC: excite " + juce::String (excite) + " shows type " + juce::String (type) + ", switch position " + juce::String (position)
                            + " of " + juce::String (positions) + ", every label fits");
            }
            // A type icon picks its first (modal) engine; a click on the type in use keeps the engine.
            IconPicker* picker = nullptr;
            shown (picker);
            if (picker != nullptr && picker->onChoose != nullptr)
            {
                picker->onChoose (0);
                settle (200);
                const auto piano = juce::roundToInt (processor.apvts.getRawParameterValue (physicalPrefix + "_excite")->load());
                if (auto* parameter = processor.apvts.getParameter (physicalPrefix + "_excite"))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (4.0f));
                settle (200);
                picker->onChoose (2);
                settle (200);
                const auto keptBow = juce::roundToInt (processor.apvts.getRawParameterValue (physicalPrefix + "_excite")->load());
                expect (piano == 11 && keptBow == 4, "OSC: the PIANO icon picks Modal Piano; BOWED clicked on the LEGACY bow keeps it");
            }
        }

        if (auto* parameter = processor.apvts.getParameter (physicalPrefix + "_excite"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (9.0f));
        settle (300);

        std::vector<PhysicalView*> views;
        findAll<PhysicalView> (*editor, views);
        auto stringShown = false;
        for (auto* view : views)
            stringShown = stringShown || (visibleInTree (view) && view->getOscillator() == physicalPrefix);
        expect (stringShown, "the OSC card shows the moving string");
        {
            pages->showPage ("MAIN");
            settle (200);
        }
    }
}

void uiGroup_FILTER_PICKERS (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 6 (V5-17, S5-12, S6-20): the filter type is one compact
    // picker per filter (no 12-button grids), every model once in its menu,
    // the analogue models under ANALOG, and arrows that step through it as
    // one undo step each.
    {
        processor.loadFactoryPreset (0);
        auto order = FilterTypes::order();
        std::sort (order.begin(), order.end());
        expect ((int) order.size() == FilterType::Count && std::adjacent_find (order.begin(), order.end()) == order.end()
                    && order.front() == 0 && order.back() == FilterType::Count - 1,
                "the type menu lists every filter model exactly once");
        expect (FilterTypes::shortNames().size() == FilterType::Count, "every filter type has a short name");
        auto analogue = true;
        for (const auto type : { FilterType::LadderLow, FilterType::LadderHigh, FilterType::DiodeLow, FilterType::Ms20Low,
                                 FilterType::MoogDrive, FilterType::Acid303, FilterType::Sem })
            analogue = analogue && FilterTypes::getPageName (FilterTypes::pageOf (type)) == "ANALOG";
        expect (analogue, "Ladder, Diode, MS-20, Moog, 303 and SEM are under ANALOG");
        expect (FilterTypes::displayName (FilterType::AwZLow).startsWith ("Smooth") && FilterTypes::shortNames()[FilterType::AwYNotLow] == "Reso LP",
                "the Airwindows filters are named by what they do");
    }
}

void uiGroup_WEST_CARD (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 6 (I6-15, I6-16): with PLACE Replace Filter 2, the graph drops Filter 2. (The old FILTER page's
    // dimmed Filter 2 card went with it: PLAY's card shows WEST in its own view.)
    {
        pages->showPage ("MAIN");
        settle (200);
        setParam ("west_on", 1.0f);
        setParam ("west_pos", 1.0f);
        settle (400);
        auto markerFor2 = 1;
        if (auto* display = findChild<FilterDisplay> (*editor); display != nullptr)
            markerFor2 = display->filterAt (display->getMarkerCentres()[1]);
        setParam ("west_pos", 0.0f);
        setParam ("west_on", 0.0f);
        settle (400);
        expect (markerFor2 == 0, "WEST in Filter 2's place: the graph stops offering Filter 2's marker");
    }
}

void uiGroup_WEST_BODY (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Review 14 (V14-12): Filter 2 has a switch in its header that parks CUTOFF at the top and brings it back.
    {
        processor.loadFactoryPreset (0);
        pages->showPage ("MAIN");
        settle (300);
        pages->showPlayViews (1, -1, -1); // (PLAY's FILTER card on Filter 2)
        settle (300);
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*editor, buttons);
        juce::TextButton* f2Switch = nullptr;
        for (auto* button : buttons)
            if (visibleInTree (button) && button->getButtonText().isEmpty() && button->getProperties().contains ("switch"))
                f2Switch = button;
        expect (f2Switch != nullptr && f2Switch->getToggleState() == false, "FILTER 2 has a switch in its header, off while it is open at 20 kHz (V14-12)");
        if (f2Switch != nullptr)
        {
            f2Switch->triggerClick();
            settle (300);
            const auto cutoffOn = readParam ("f2_cutoff");
            f2Switch->triggerClick();
            settle (300);
            // Sweep round 2: OFF is a real bypass (f2_on), the cutoff stays where it was.
            expect (cutoffOn < 19000.0f && readParam ("f2_on") < 0.5f && std::abs (readParam ("f2_cutoff") - cutoffOn) < 1.0f
                        && ! f2Switch->getToggleState(),
                    "the F2 switch brings a cutoff in, and OFF bypasses Filter 2 leaving the cutoff (V14-12)");
        }
        setParam ("f2_cutoff", 20000.0f);
        setParam ("f2_on", 1.0f);
        pages->showPlayViews (0, -1, -1);
        settle (200);
    }
}

void uiGroup_FILTER_MARKERS (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // UI review 6 (V6-18, S6-21): the graph's markers sit on their filter's
    // response, 8 px inside the plot: Init's two filters, open at 20 kHz,
    // are high on the right (not in the bottom corner) and fanned apart.
    {
        processor.loadFactoryPreset (0);
        pages->showPage ("MAIN");
        settle (300);
        if (auto* display = findChild<FilterDisplay> (*editor); display != nullptr && visibleInTree (display))
        {
            const auto markers = display->getMarkerCentres();
            // (PLAY's small graph plots nearly edge to edge: FilterDisplay::isCompact.)
            const auto compact = display->getHeight() < 150 || display->getWidth() < 400;
            const auto plot = display->getLocalBounds().toFloat().reduced (compact ? 3.0f : 10.0f, compact ? 3.0f : 12.0f);
            const auto inset = plot.reduced (8.0f);
            // (High: above the middle of a tall graph; PLAY's short one only keeps them off its bottom.)
            const auto highLimit = compact ? plot.getY() + plot.getHeight() * 0.75f : plot.getCentreY();
            expect (inset.contains (markers[0]) && inset.contains (markers[1]) && markers[0].getDistanceFrom (markers[1]) >= 18.0f
                        && markers[0].y < highLimit && markers[1].y < highLimit,
                    "Init: the open filters' markers sit high on the response, inside the plot, apart ("
                        + markers[0].toString() + " / " + markers[1].toString() + ")");
        }
        else
            expect (false, "FILTER shows the response graph");

        // BALANCE does nothing in serial: dimmed and disabled (V6-18).
        setParam ("filters_parallel", 0.0f);
        settle (300);
        const auto balanceShown = [&] { return uiBalanceSliderShown (*editor); };
        const auto serialOff = ! balanceShown();
        setParam ("filters_parallel", 1.0f);
        settle (300);
        expect (serialOff && balanceShown(), "BALANCE shows while the filters run in parallel, not in serial");
        setParam ("filters_parallel", 0.0f);
        settle (200);
    }
}

void uiGroup_REVIEW1 (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Knobs, chips, the filter graph, the browser and SUB + NOISE (UI review 1 fixes).
    {
        const auto makeEvent = [] (juce::Component& component, juce::Point<float> position, bool dragged)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys(),
                                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(),
                                     position, juce::Time::getCurrentTime(), 1, dragged);
        };

        // A click on a source chip keeps every knob it drives lit; another click clears it.
        processor.loadFactoryPreset (neuroWobble);
        pages->showPage ("MAIN");
        settle (400);
        std::vector<ModSourceChip*> chipsFound;
        std::vector<KnobControl*> knobsFound;
        findAll<ModSourceChip> (*editor, chipsFound);
        findAll<KnobControl> (*editor, knobsFound);
        ModSourceChip* chosen = nullptr;
        auto drivenKnobs = 0;

        for (auto* chip : chipsFound)
        {
            if (! visibleInTree (chip))
                continue;

            auto count = 0;

            for (auto* knob : knobsFound)
                if (visibleInTree (knob) && knob->isDrivenBy (chip->getSourceIndex()))
                    ++count;

            if (count > 0)
            {
                chosen = chip;
                drivenKnobs = count;
                break;
            }
        }

        expect (chosen != nullptr, "Neuro Wobble has a source chip that drives a visible knob");

        if (chosen != nullptr)
        {
            auto lit = [&]
            {
                auto count = 0, wrong = 0;
                for (auto* knob : knobsFound)
                    if (visibleInTree (knob))
                    {
                        count += knob->isLitByPinnedSource() ? 1 : 0;
                        wrong += knob->isLitByPinnedSource() != knob->isDrivenBy (chosen->getSourceIndex()) ? 1 : 0;
                    }
                return std::pair<int, int> { count, wrong };
            };

            expect (pinnedModSource() == 0 && lit().first == 0, "no source is pinned to start with");
            chosen->mouseUp (makeEvent (*chosen, { 8.0f, 8.0f }, false));
            settle (100);
            expect (pinnedModSource() == chosen->getSourceIndex() && chosen->isPinned(), "clicking a chip pins its source");
            expect (lit().first == drivenKnobs && lit().second == 0,
                    "the pinned chip lights exactly the knobs it drives (" + juce::String (drivenKnobs) + ")");
            chosen->mouseUp (makeEvent (*chosen, { 8.0f, 8.0f }, true)); // a drag is not a click
            expect (pinnedModSource() == chosen->getSourceIndex(), "dragging a chip does not unpin it");
            chosen->mouseUp (makeEvent (*chosen, { 8.0f, 8.0f }, false));
            expect (pinnedModSource() == 0 && lit().first == 0, "clicking the chip again clears the highlight");
        }

        // UI review 4, batch B (V8, V9, V17, V18, V24, S7, S12): modulation.
        {
            using Module = IlanaSynthAudioProcessor::Module;
            const auto usedSource = [&processor] (int source)
            {
                for (int i = 0; i < Mod::maxSlots; ++i)
                {
                    const auto slot = processor.readModSlot (i);
                    if (slot.destination != 0 && ((int) slot.source == source || (int) slot.aux == source))
                        return true;
                }
                return false;
            };
            const auto poolKindOf = [] (int source) -> std::pair<int, int>
            {
                if (const auto lfo = Mod::lfoIndexFor ((Mod::Source) source); lfo >= 0)
                    return { (int) Module::Lfo, lfo };
                using S = Mod::Source;
                const S fixedEnvs[] { S::AmpEnv, S::FilterEnv, S::FilterEnv2, S::ModEnv, S::Env4 };
                for (int i = 0; i < 5; ++i)
                    if ((S) source == fixedEnvs[i])
                        return { (int) Module::Envelope, i };
                if (source >= (int) S::Env6 && source <= (int) S::Env16)
                    return { (int) Module::Envelope, 5 + source - (int) S::Env6 };
                return { -1, 0 };
            };
            // (Chips in a group's tray are counted through their group chip.)
            const auto chipState = [&]
            {
                std::vector<ModSourceChip*> found;
                findAll<ModSourceChip> (*editor, found);
                auto shown = 0, compact = 0, strays = 0;
                juce::StringArray strayNames;
                for (auto* chip : found)
                {
                    if (! chip->isVisible() || chip->getParentComponent() == nullptr
                        || dynamic_cast<ModSourceTray*> (chip->getParentComponent()) != nullptr)
                        continue;
                    ++shown;
                    // "Compact" now means a chip not showing its source's
                    // one name (a code such as "E6" or "AT").
                    compact += chip->getSourceName() != ModNames::sourceUpper (chip->getSourceIndex()) ? 1 : 0;
                    const auto [kind, index] = poolKindOf (chip->getSourceIndex());
                    if (kind >= 0 && ! processor.isRevealed ((Module) kind, index) && ! usedSource (chip->getSourceIndex()))
                    {
                        ++strays;
                        strayNames.add (chip->getSourceName());
                    }
                }
                return std::make_tuple (shown, compact, strays, strayNames.joinIntoString (", "));
            };

            // The chip row follows the pool, and every chip shows its
            // source's one name, never a code (UI review 6, V6-9 / S6-19).
            {
                const auto [shown, compact, strays, strayNames] = chipState();
                expect (shown > 0 && strays == 0, "the chip row shows only sources in the pool or in use (" + juce::String (shown)
                                                      + " shown" + (strays > 0 ? "; not in the pool: " + strayNames : juce::String()) + ")");
                expect (compact == 0, "every chip shows its source's full name (" + juce::String (compact) + " codes)");
            }

            // "+" adds a source to the pool: LFO 9 (chip index 9, after OP
            // LFO) gets a chip.
            const auto lfoMask = processor.isRevealed (Module::Lfo, 8);
            pages->addPoolSource (9);
            settle (100);
            {
                std::vector<ModSourceChip*> found;
                findAll<ModSourceChip> (*editor, found);
                auto lfo9 = false;
                for (auto* chip : found)
                    lfo9 = lfo9 || (chip->isVisible() && chip->getSourceIndex() == (int) Mod::lfoSourceFor (8));
                // (Or folded into its region's "+N", the LFO region being full.)
                std::vector<ModSourceGroupChip*> groups;
                findAll<ModSourceGroupChip> (*editor, groups);
                for (auto* group : groups)
                    lfo9 = lfo9 || (group->isVisible() && std::find (group->getSources().begin(), group->getSources().end(),
                                                                     (int) Mod::lfoSourceFor (8)) != group->getSources().end());
                juce::StringArray seen;
                for (auto* chip : found)
                    if (chip->isVisible())
                        seen.add (chip->getSourceName());
                for (auto* group : groups)
                    if (group->isVisible())
                        seen.add (group->getLabel());
                expect (processor.isRevealed (Module::Lfo, 8) && lfo9, "the chip picker adds LFO 9 to the pool and the row ("
                                                                           + seen.joinIntoString (" | ") + ")");
            }
            processor.setRevealed (Module::Lfo, 8, lfoMask);

            // A full pool: every chip shown, all shortened alike.
            std::vector<bool> lfoBefore, envBefore;
            for (int i = 0; i < 16; ++i)
            {
                lfoBefore.push_back (processor.isRevealed (Module::Lfo, i));
                envBefore.push_back (processor.isRevealed (Module::Envelope, i));
                processor.setRevealed (Module::Lfo, i, true);
                processor.setRevealed (Module::Envelope, i, true);
            }
            // The row follows the pool on the editor's 250 ms timer, so a
            // fixed 200 ms settle sometimes checked it before the timer had
            // run (the flake): wait for that tick, up to two seconds.
            // A full pool: the LFOs and envelopes fold their last chips into
            // group chips ("ENV +11"), whose tray shows them; no chip is
            // shortened, none is lost, and the chips keep their full width.
            const auto groupState = [&]
            {
                std::vector<ModSourceGroupChip*> groups;
                findAll<ModSourceGroupChip> (*editor, groups);
                auto folded = 0, visibleGroups = 0;
                ModSourceGroupChip* biggest = nullptr;
                for (auto* group : groups)
                    if (group->isVisible())
                    {
                        ++visibleGroups;
                        folded += (int) group->getSources().size();
                        if (biggest == nullptr || group->getSources().size() > biggest->getSources().size())
                            biggest = group;
                    }
                return std::make_tuple (folded, visibleGroups, biggest);
            };
            // (32 pool chips and the five performance sources; the MSEG chip
            // only shows while the patch uses the old module: I8-4.)
            for (int wait = 0; wait < 40 && std::get<0> (chipState()) + std::get<0> (groupState()) < 32 + 5; ++wait)
                settle (50);
            {
                const auto [shown, compact, strays, strayNames] = chipState();
                const auto [folded, visibleGroups, biggest] = groupState();
                expect (shown + folded >= 32 + 5 && compact == 0 && visibleGroups > 0,
                        "a full pool keeps every source (" + juce::String (shown) + " chips, " + juce::String (folded) + " in "
                            + juce::String (visibleGroups) + " group chips), none shortened (" + juce::String (compact) + ")");

                std::vector<ModSourceChip*> found;
                findAll<ModSourceChip> (*editor, found);
                auto narrow = 0;
                for (auto* chip : found)
                    if (chip->isVisible() && (float) chip->getWidth() < chip->getNaturalWidth() * 0.9f - 4.0f)
                        ++narrow;
                expect (narrow == 0, "no chip is squeezed below its name's width (" + juce::String (narrow) + ")");

                // The tray: its chips are the folded sources, draggable.
                if (biggest != nullptr && biggest->onOpen != nullptr)
                {
                    biggest->onOpen (*biggest);
                    settle (100);
                    auto* tray = findChild<ModSourceTray> (*editor);
                    const auto trayChips = tray != nullptr ? (int) tray->getChips().size() : 0;
                    auto inside = tray != nullptr && tray->isVisible();
                    if (tray != nullptr)
                        for (auto& chip : tray->getChips())
                            inside = inside && tray->getLocalBounds().contains (chip->getBounds());
                    expect (trayChips == (int) biggest->getSources().size() && inside,
                            "a group chip opens a tray with its " + juce::String ((int) biggest->getSources().size())
                                + " chips (" + juce::String (trayChips) + ")");
                    if (tray != nullptr)
                        tray->close();
                }
            }
            for (int i = 0; i < 16; ++i)
            {
                processor.setRevealed (Module::Lfo, i, lfoBefore[(size_t) i]);
                processor.setRevealed (Module::Envelope, i, envBefore[(size_t) i]);
            }
            settle (200);

            // One colour per source: eight macros, the performance sources,
            // and LFO 1 off the accent.
            {
                using S = Mod::Source;
                std::vector<int> sources;
                for (int m = 0; m < Mod::numMacros; ++m)
                    sources.push_back ((int) Mod::macroSourceFor (m));
                for (const auto s : { S::Velocity, S::KeyTrack, S::Random, S::ClockSh, S::ModWheel, S::Aftertouch, S::Expression,
                                      S::Lfo1, S::Lfo2, S::Lfo3, S::Lfo4, S::AmpEnv, S::FilterEnv, S::FilterEnv2, S::ModEnv, S::Env4 })
                    sources.push_back ((int) s);
                for (int s = (int) S::Env6; s <= (int) S::Env16; ++s)
                    sources.push_back (s);
                for (int s = (int) S::Lfo5; s <= (int) S::Lfo16; ++s)
                    sources.push_back (s);

                auto clashes = 0;
                juce::String example;
                for (size_t a = 0; a < sources.size(); ++a)
                    for (size_t b = a + 1; b < sources.size(); ++b)
                    {
                        const auto ca = modSourceColour (sources[a]), cb = modSourceColour (sources[b]);
                        const auto distance = std::abs (ca.getFloatRed() - cb.getFloatRed()) + std::abs (ca.getFloatGreen() - cb.getFloatGreen())
                                              + std::abs (ca.getFloatBlue() - cb.getFloatBlue());
                        if (distance < 0.06f)
                        {
                            ++clashes;
                            example = Mod::getSourceNames()[sources[a]] + " / " + Mod::getSourceNames()[sources[b]];
                        }
                    }
                expect (clashes == 0, "every source has its own colour (" + juce::String (clashes) + " clashes"
                                          + (example.isNotEmpty() ? ", e.g. " + example : juce::String()) + ")");

                std::set<juce::uint32> macroColours;
                for (int m = 0; m < Mod::numMacros; ++m)
                    macroColours.insert (modSourceColour ((int) Mod::macroSourceFor (m)).getARGB());
                expect (macroColours.size() == (size_t) Mod::numMacros, "the eight macros have eight colours");

                auto hueGap = std::abs (modSourceColour ((int) S::Lfo1).getHue() - IlanaTheme::accent().getHue());
                hueGap = juce::jmin (hueGap, 1.0f - hueGap);
                expect (hueGap > 0.08f, "LFO 1 is not the accent's orange");
            }

            // MATRIX: rows numbered 1..n as shown, repeats flagged, sortable,
            // the remap editor docked under its row.
            pages->showPage ("MATRIX");
            settle (400);
            const auto shownRows = [&]
            {
                std::vector<MatrixRow*> found, visible;
                findAll<MatrixRow> (*editor, found);
                for (auto* row : found)
                    if (row->isVisible())
                        visible.push_back (row);
                std::sort (visible.begin(), visible.end(), [] (auto* a, auto* b) { return a->getY() < b->getY(); });
                return visible;
            };
            {
                const auto visible = shownRows();
                auto numbered = ! visible.empty();
                auto duplicates = 0;
                for (size_t i = 0; i < visible.size(); ++i)
                {
                    numbered = numbered && visible[i]->getDisplayNumber() == (int) i + 1;
                    duplicates += visible[i]->isDuplicate() ? 1 : 0;
                }
                auto lastSlot = 0;
                for (auto* row : visible)
                    lastSlot = juce::jmax (lastSlot, row->getSlotIndex() + 1);
                expect (numbered && lastSlot > (int) visible.size(),
                        "matrix rows are numbered 1.." + juce::String (visible.size()) + " as shown (the last slot used is " + juce::String (lastSlot) + ")");
                // Factory presets load with their repeated routings merged
                // (review 6, S5-9): Neuro Wobble's two LFO 1 > Filter 1
                // Cutoff rows are one row now.
                expect (duplicates == 0, "a factory preset loads with no repeated routing (" + juce::String (duplicates) + " flagged)");
                // Rows grow to fill a page the dock's note doesn't fit on
                // (V8-20), up to 38 px, their controls at their own size.
                if (! visible.empty())
                    expect (visible[0]->getHeight() <= 34, "matrix rows stay compact (" + juce::String (visible[0]->getHeight()) + " px)");
            }

            if (auto* page = pages->getCurrentPage())
            {
                // With routings, "+ ADD MODULATION" is the row after the last
                // one (review 7, S7-35); the header's button is for an empty matrix.
                std::vector<juce::Button*> buttons;
                findAll<juce::Button> (*page, buttons);
                juce::Button* add = nullptr;
                auto headerAdd = false;
                for (auto* button : buttons)
                    if (button->getButtonText().contains ("ADD MODULATION"))
                    {
                        if (dynamic_cast<DashedAddButton*> (button) != nullptr)
                            add = button;
                        else
                            headerAdd = headerAdd || button->isVisible();
                    }
                auto lastRowBottom = 0;
                for (auto* row : shownRows())
                    lastRowBottom = juce::jmax (lastRowBottom, row->getBottom());
                expect (add != nullptr && add->isVisible() && ! headerAdd && add->getY() >= lastRowBottom
                            && add->getY() < lastRowBottom + 10,
                        "+ ADD MODULATION is the row after the last routing, not a header button");

                // The rows open grouped by source (S8-25), in the bar's order;
                // a click on SOURCE reverses that, on # goes back to slot order.
                const auto clickAt = [&] (juce::Point<float> at)
                {
                    page->mouseUp (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(), 1.0f,
                                                     0.0f, 0.0f, 0.0f, 0.0f, page, page, juce::Time::getCurrentTime(), at,
                                                     juce::Time::getCurrentTime(), 1, false));
                    settle (100);
                };
                const auto sourceRank = [&] (MatrixRow* row)
                {
                    const auto& order = ModNames::sourcesInMenuOrder();
                    return (int) std::distance (order.begin(), std::find (order.begin(), order.end(),
                                                                          (int) processor.readModSlot (row->getSlotIndex()).source));
                };
                const auto sortedBySource = [&] (bool reversed)
                {
                    const auto visible = shownRows();
                    auto sorted = ! visible.empty() && visible[0]->getDisplayNumber() == 1;
                    for (size_t i = 1; i < visible.size(); ++i)
                        sorted = sorted && (reversed ? sourceRank (visible[i - 1]) >= sourceRank (visible[i])
                                                     : sourceRank (visible[i - 1]) <= sourceRank (visible[i]));
                    return sorted;
                };
                using C = MatrixRow::Columns;
                const auto headingY = 6.0f + 32.0f + 9.0f;
                const auto grouped = sortedBySource (false);
                clickAt ({ 12.0f + (float) (C::number + C::bypass + C::gap * 2) + 20.0f, headingY });
                const auto bySource = grouped && sortedBySource (true);
                clickAt ({ 20.0f, headingY });
                auto bySlot = true;
                {
                    const auto visible = shownRows();
                    for (size_t i = 1; i < visible.size(); ++i)
                        bySlot = bySlot && visible[i - 1]->getSlotIndex() < visible[i]->getSlotIndex();
                }
                expect (bySource && bySlot, "the matrix opens grouped by source, SOURCE reverses it, # goes back to slot order");
                // (Back to the opening order for the tests after.)
                clickAt ({ 12.0f + (float) (C::number + C::bypass + C::gap * 2) + 20.0f, headingY });

                // The remap editor opens in the dock under the rows: no row
                // moves, none is covered, and its X closes it (V6-22).
                const auto visible = shownRows();
                if (visible.size() >= 3)
                {
                    auto* row = visible[1];
                    std::vector<int> rowsY;
                    for (auto* other : visible)
                        rowsY.push_back (other->getY());
                    row->getCurve().openRemapEditor();
                    settle (200);
                    auto* remap = findChild<RemapEditor> (*page);
                    auto covers = 0, moved = 0;
                    if (remap != nullptr)
                        for (auto* other : shownRows())
                            if (other->isShowing() || true)
                            {
                                const auto rowArea = page->getLocalArea (other, other->getLocalBounds());
                                const auto visibleArea = rowArea.getIntersection (page->getLocalArea (other->getParentComponent()->getParentComponent(),
                                                                                                      other->getParentComponent()->getParentComponent()->getLocalBounds()));
                                if (! visibleArea.isEmpty() && visibleArea.intersects (page->getLocalArea (remap, remap->getLocalBounds())))
                                    ++covers;
                            }
                    const auto after = shownRows();
                    for (size_t i = 0; i < after.size() && i < rowsY.size(); ++i)
                        moved += after[i]->getY() != rowsY[i] ? 1 : 0;
                    const auto docked = remap != nullptr && remap->getParentComponent() == page
                                        && page->getLocalArea (remap, remap->getLocalBounds()).getBottom() >= page->getHeight() - 20;
                    expect (remap != nullptr && covers == 0 && moved == 0 && docked,
                            "the remap editor opens in the dock under the rows, covering none (" + juce::String (covers)
                                + ") and moving none (" + juce::String (moved) + ")");
                    if (remap != nullptr)
                    {
                        remap->createComponentSnapshot (remap->getLocalBounds());
                        expect (remap->getLiveInput() >= 0.0f, "the remap editor shows the live input on the curve");
                        std::vector<juce::TextButton*> tools;
                        findAll<juce::TextButton> (*remap, tools);
                        juce::TextButton* close = nullptr;
                        auto shapes = false;
                        for (auto* button : tools)
                        {
                            close = button->getButtonText() == juce::String::fromUTF8 ("\xc3\x97") && button->isVisible() ? button : close;
                            shapes = shapes || (button->getButtonText().startsWith ("SHAPES") && button->isVisible());
                        }
                        expect (close != nullptr && shapes, "the remap editor has a close button and a shapes button");
                        if (close != nullptr)
                        {
                            close->triggerClick();
                            settle (200);
                            expect (findChild<RemapEditor> (*page) == nullptr, "the remap editor's X closes it");
                        }
                    }
                }
            }

            // MIDI learn on any automatable knob: the next CC drives it, and
            // the map is saved with the patch.
            {
                processor.startParamLearn ("f1_cutoff");
                const auto learning = processor.getParamLearnTarget() == "f1_cutoff";
                juce::AudioBuffer<float> buffer (2, 512);
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 100), 0);
                processor.processBlock (buffer, midi);
                settle (100);
                auto* cutoff = processor.apvts.getParameter ("f1_cutoff");
                const auto mapped = processor.getParamCc ("f1_cutoff") == 74 && processor.getParamLearnTarget().isEmpty();
                const auto moved = cutoff != nullptr && std::abs (cutoff->getValue() - 100.0f / 127.0f) < 0.01f;
                expect (learning && mapped && moved, "MIDI learn on a knob maps the next CC (74) and the CC moves it");

                juce::MemoryBlock saved;
                processor.getStateInformation (saved);
                processor.clearParamCc ("f1_cutoff");
                const auto cleared = processor.getParamCc ("f1_cutoff") < 0;
                processor.setStateInformation (saved.getData(), (int) saved.getSize());
                settle (100);
                expect (cleared && processor.getParamCc ("f1_cutoff") == 74, "a learned CC is saved and restored with the patch");
                processor.clearParamCc ("f1_cutoff");
                processor.loadFactoryPreset (neuroWobble);
                settle (200);
            }
        }

        // The macro names the browser lists match what the preset loads with.
        {
            const auto listed = processor.getFactoryMacroNames (neuroWobble);
            auto matches = true;
            for (const auto& name : listed)
            {
                auto found = false;
                for (int macro = 0; macro < 4; ++macro)
                    found = found || processor.getMacroName (macro) == name;
                matches = matches && found;
            }
            expect (! listed.isEmpty() && matches, "the browser's macro names for Neuro Wobble are the ones it loads ("
                                                       + listed.joinIntoString (", ") + ")");
        }

        // Worked out without loading, the list's macro names equal what every factory preset loads with.
        {
            juce::StringArray mismatches;
            for (int index = 0; index < names.size(); ++index)
            {
                processor.loadFactoryPreset (index);
                const auto listed = processor.getFactoryMacroNames (index);
                for (int macro = 0; macro < 4; ++macro)
                {
                    const auto loaded = processor.apvts.state.getProperty ("macroName" + juce::String (macro + 1)).toString();
                    if (loaded != listed[macro])
                        mismatches.add (names[index] + " #" + juce::String (macro + 1) + " listed '" + listed[macro] + "' loads '" + loaded + "'");
                }
            }
            expect (mismatches.isEmpty(), "the browser lists every factory preset's macro names as loaded ("
                                              + juce::String (names.size()) + " presets"
                                              + (mismatches.isEmpty() ? juce::String() : "; " + juce::String (mismatches.size()) + " differ, e.g. " + mismatches[0]) + ")");
            for (const auto& line : mismatches)
                std::cout << "  " << line << std::endl;
            processor.loadFactoryPreset (neuroWobble);
        }

        // Up / down in the open browser load the next / previous preset and leave it open.
        if (auto* display = findChild<PresetDisplay> (*editor); display != nullptr && display->onClick != nullptr)
        {
            display->onClick();
            settle (500);
            auto* panel = findChild<PresetPanel> (*editor);
            expect (panel != nullptr && panel->isOpen(), "the preset browser opens");

            if (panel != nullptr)
            {
                const auto start = processor.getCurrentPresetName();
                panel->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
                settle (150);
                const auto next = processor.getCurrentPresetName();
                expect (next.isNotEmpty() && next != start && panel->isOpen(), "Down loads the next preset and keeps the browser open");
                panel->keyPressed (juce::KeyPress (juce::KeyPress::upKey));
                settle (150);
                expect (processor.getCurrentPresetName() == start && panel->isOpen(), "Up steps back to the preset before");
            }

            display->onClick();
            settle (500);
        }

        // SUB + NOISE keeps its slot whatever is on (nothing folds: UI
        // review 6, V7, V19); the sub's controls follow its switch.
        {
            pages->showPage ("MAIN");
            const auto set = [&] (const char* id, float value)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            const auto shown = [&] (const juce::String& id) -> KnobControl*
            {
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*editor, knobs);
                for (auto* knob : knobs)
                    if (visibleInTree (knob) && knob->getParameterId() == id)
                        return knob;
                return nullptr;
            };

            processor.loadFactoryPreset (0);
            set ("subosc_on", 0.0f);
            set ("noise_level", 0.0f);
            settle (600);
            auto* subLevel = shown ("subosc_level");
            expect (shown ("noise_level") != nullptr && subLevel != nullptr && subLevel->getAlpha() < 0.99f,
                    "SUB and NOISE cards stay in place while both are off, the sub's controls dimmed");
            set ("subosc_on", 1.0f);
            settle (600);
            subLevel = shown ("subosc_level");
            expect (subLevel != nullptr && subLevel->getAlpha() > 0.99f, "switching the sub on lights its controls");
            set ("subosc_on", 0.0f);
            settle (300);
        }

        // Dragging the filter graph: across for cutoff, up and down for resonance, one gesture each.
        if (auto* display = findChild<FilterDisplay> (*editor); display != nullptr)
        {
            struct GestureCounter : juce::AudioProcessorParameter::Listener
            {
                int begins = 0, ends = 0;
                void parameterValueChanged (int, float) override {}
                void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
            } cutoffGestures, resoGestures;

            processor.loadFactoryPreset (0);
            pages->showPage ("MAIN");
            settle (400);
            auto* cutoff = processor.apvts.getParameter ("f1_cutoff");
            auto* reso = processor.apvts.getParameter ("f1_reso");
            cutoff->addListener (&cutoffGestures);
            reso->addListener (&resoGestures);
            const auto before = processor.apvts.getRawParameterValue ("f1_cutoff")->load();
            const auto resoBefore = processor.apvts.getRawParameterValue ("f1_reso")->load();
            const auto w = (float) display->getWidth();
            const auto h = (float) display->getHeight();
            auto* component = static_cast<juce::Component*> (display);
            clearHistory();
            component->mouseDown (makeEvent (*component, { 10.0f + 0.5f * (w - 20.0f), 0.5f * h }, false));
            component->mouseDrag (makeEvent (*component, { 10.0f + 0.3f * (w - 20.0f), 0.2f * h }, true));
            component->mouseUp (makeEvent (*component, { 10.0f + 0.3f * (w - 20.0f), 0.2f * h }, true));
            const auto after = processor.apvts.getRawParameterValue ("f1_cutoff")->load();
            const auto resoAfter = processor.apvts.getRawParameterValue ("f1_reso")->load();
            {
                settle (100);
                const auto steps = undoSteps();
                processor.getUndoManager().undo();
                const auto undone = processor.apvts.getRawParameterValue ("f1_cutoff")->load();
                processor.getUndoManager().redo();
                expect (steps.size() == 1 && steps[0] == "Filter 1 graph" && std::abs (undone / before - 1.0f) < 0.01f
                            && std::abs (processor.apvts.getRawParameterValue ("f1_cutoff")->load() / after - 1.0f) < 0.01f,
                        "the filter graph drag is one undo step ('" + steps.joinIntoString ("', '") + "'); undo and redo move the cutoff");
            }
            expect (before > 5000.0f && std::abs (after / 159.0f - 1.0f) < 0.15f,
                    "dragging across the filter graph sets the cutoff (" + juce::String (before, 0) + " -> " + juce::String (after, 0) + " Hz)");
            expect (resoAfter > resoBefore + 0.1f, "dragging up on the filter graph raises the resonance");
            expect (cutoffGestures.begins == 1 && cutoffGestures.ends == 1 && resoGestures.begins == 1 && resoGestures.ends == 1,
                    "the filter drag is one gesture per parameter");
            cutoff->removeListener (&cutoffGestures);
            reso->removeListener (&resoGestures);
            processor.loadFactoryPreset (0);
            settle (200);
        }

        // The oscillator display: across scrubs the frame, up raises the warp, one gesture each; the corner key cycles WAVE / 3D / SPEC.
        {
            processor.loadFactoryPreset (0);
            pages->showPage ("MAIN");
            const auto set = [&] (const char* id, float value)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            set ("osc1_warp", 2.0f); // Bend +
            set ("osc1_warp_amt", 0.2f);
            settle (400);

            std::vector<WaveDisplay*> waves;
            findAll<WaveDisplay> (*editor, waves);
            WaveDisplay* wave = nullptr;
            for (auto* candidate : waves)
                if (visibleInTree (candidate) && candidate->getOscIndex() == 0)
                    wave = candidate;
            expect (wave != nullptr, "PLAY shows OSC 1's display");

            if (wave != nullptr)
            {
                struct GestureCounter : juce::AudioProcessorParameter::Listener
                {
                    int begins = 0, ends = 0;
                    void parameterValueChanged (int, float) override {}
                    void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
                } frameGestures, warpGestures;

                auto* frame = processor.apvts.getParameter ("osc1_frame");
                auto* warp = processor.apvts.getParameter ("osc1_warp_amt");
                frame->addListener (&frameGestures);
                warp->addListener (&warpGestures);
                const auto w = (float) wave->getWidth();
                const auto h = (float) wave->getHeight();
                auto* component = static_cast<juce::Component*> (wave);
                component->mouseDown (makeEvent (*component, { 10.0f + 0.25f * (w - 20.0f), 0.6f * h }, false));
                component->mouseDrag (makeEvent (*component, { 10.0f + 0.75f * (w - 20.0f), 0.2f * h }, true));
                const auto frameAfter = frame->getValue();
                const auto warpAfter = processor.apvts.getRawParameterValue ("osc1_warp_amt")->load();
                expect (wave->isDraggingWarp(), "a drag on a warped table moves the warp");
                component->mouseUp (makeEvent (*component, { 10.0f + 0.75f * (w - 20.0f), 0.2f * h }, true));
                expect (std::abs (frameAfter - 0.75f) < 0.03f, "dragging across the oscillator display scrubs the frame (" + juce::String (frameAfter, 2) + ")");
                expect (warpAfter > 0.3f, "dragging up on the oscillator display raises the warp amount (0.20 -> " + juce::String (warpAfter, 2) + ")");
                expect (frameGestures.begins == 1 && frameGestures.ends == 1 && warpGestures.begins == 1 && warpGestures.ends == 1,
                        "the oscillator drag is one gesture per parameter");
                component->mouseDoubleClick (makeEvent (*component, { 20.0f, 20.0f }, false));
                expect (processor.apvts.getRawParameterValue ("osc1_warp_amt")->load() < 0.001f, "a double-click sets the warp back to zero");
                frame->removeListener (&frameGestures);
                warp->removeListener (&warpGestures);

                // With no warp chosen a vertical drag leaves the amount alone.
                set ("osc1_warp", 0.0f);
                set ("osc1_warp_amt", 0.4f);
                settle (100);
                component->mouseDown (makeEvent (*component, { 30.0f, 0.8f * h }, false));
                component->mouseDrag (makeEvent (*component, { 30.0f, 0.1f * h }, true));
                component->mouseUp (makeEvent (*component, { 30.0f, 0.1f * h }, true));
                expect (std::abs (processor.apvts.getRawParameterValue ("osc1_warp_amt")->load() - 0.4f) < 0.001f,
                        "without a warp the vertical drag changes nothing");

            }

            // On OSC the display opens on the cycle (the design's 2D well), its views a WAVE | 3D | SPEC
            // control, its table named with arrows on it (UI review 5, V6,
            // V27; review 6, V21, V40).
            pages->showPage ("MAIN");
            settle (300);
            waves.clear();
            findAll<WaveDisplay> (*editor, waves);
            wave = nullptr;
            for (auto* candidate : waves)
                if (visibleInTree (candidate) && candidate->getOscIndex() == 0)
                    wave = candidate;
            expect (wave != nullptr && wave->getViewMode() == 0, "OSC opens OSC 1's display on the cycle (WAVE), as the design draws it");

            if (wave != nullptr)
            {
                std::vector<juce::TextButton*> keys;
                findAll<juce::TextButton> (*wave, keys);
                const auto key = [&keys] (const juce::String& text) -> juce::TextButton*
                {
                    for (auto* button : keys)
                        if (button->getButtonText() == text && visibleInTree (button))
                            return button;
                    return nullptr;
                };
                // (OSC's card shows the display slim: its view chip and table arrows
                // are drawn on its top line, so the test clicks them there.)
                const auto clickSlim = [&] (int which)
                {
                    const auto at = wave->getSlimTarget (which);
                    const auto now = juce::Time::getCurrentTime();
                    const auto make = [&] (bool up)
                    {
                        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys::leftButtonModifier,
                                                 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, wave, wave, now, at, now, 1, false);
                    };
                    static_cast<juce::Component*> (wave)->mouseDown (make (false));
                    static_cast<juce::Component*> (wave)->mouseUp (make (true));
                    settle (60);
                };

                juce::StringArray seen;
                auto spec = juce::String();
                if (wave->isSlim())
                {
                    // The chip cycles WAVE > 3D > SPEC > WAVE; it starts on WAVE.
                    for (int i = 0; i < 3; ++i)
                    {
                        clickSlim (0);
                        seen.add (juce::String (wave->getViewMode()));
                        if (wave->getViewMode() == 2)
                            spec = wave->getFrameReadout();
                    }
                    juce::StringArray sorted (seen);
                    sorted.sort (false);
                    expect (sorted.joinIntoString (",") == "0,1,2", "WAVE, 3D and SPEC each pick their view (" + seen.joinIntoString (",") + ")");
                    expect (spec.isEmpty(), "SPEC shows no frame readout");
                    while (wave->getViewMode() != 1)
                        clickSlim (0);
                }
                else
                {
                    for (const auto* name : { "WAVE", "3D", "SPEC" })
                        if (auto* button = key (name))
                        {
                            button->triggerClick();
                            settle (60);
                            seen.add (juce::String (wave->getViewMode()));
                        }
                    expect (seen.joinIntoString (",") == "0,1,2", "WAVE, 3D and SPEC each pick their view (" + seen.joinIntoString (",") + ")");
                    expect (wave->getFrameReadout().isEmpty(), "SPEC shows no frame readout");
                    if (auto* button = key ("3D"))
                        button->triggerClick();
                }
                settle (60);
                expect (wave->getFrameReadout().startsWith ("frame "), "3D reads its frame under the plot (" + wave->getFrameReadout() + ")");

                const auto table = [&processor] { return juce::roundToInt (processor.apvts.getRawParameterValue ("osc1_table")->load()); };
                const auto before = table();
                const auto nameBefore = wave->getTableName();
                if (wave->isSlim())
                    clickSlim (2);
                else if (auto* next = key (">"))
                    next->triggerClick();
                settle (100);
                const auto after = table();
                if (wave->isSlim())
                    clickSlim (1);
                else if (auto* previous = key ("<"))
                    previous->triggerClick();
                settle (100);
                expect (after != before && table() == before && wave->getTableName() == nameBefore && nameBefore.isNotEmpty(),
                        "the display's arrows step the table and back (" + nameBefore + ")");
            }

            processor.loadFactoryPreset (0);
            settle (200);
        }

        // A macro's arc on an OSC 1 knob (yellow on gold) is drawn paler; an unrelated hue is left alone.
        {
            const auto macro = modSourceColour ((int) Mod::Source::Macro1);
            const auto onGold = modArcColour (macro, IlanaTheme::oscColour (0));
            const auto onBlue = modArcColour (macro, IlanaTheme::oscColour (1));
            expect (onGold.getBrightness() > macro.getBrightness() + 0.02f || onGold.getSaturation() < macro.getSaturation() - 0.2f,
                    "a macro arc on an OSC 1 knob stands apart from its gold");
            expect (onBlue == macro, "a macro arc on an OSC 2 knob keeps the macro colour");
        }

        // Resting on a modulated knob opens a card listing its sources with live bars.
        {
            processor.loadFactoryPreset (neuroWobble);
            pages->showPage ("MAIN");
            settle (400);
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            KnobControl* modulated = nullptr;
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getNumRoutings() > 0)
                {
                    modulated = knob;
                    break;
                }
            expect (modulated != nullptr, "Neuro Wobble has a modulated knob on PLAY");
            auto* card = findChild<ModHoverPopup> (*editor);
            expect (card != nullptr, "the editor has the modulation card");

            if (modulated != nullptr && card != nullptr)
            {
                modulated->openModCard(); // (hover opens it after a short rest; the test has no mouse)
                const auto inside = card->getParentComponent() != nullptr
                                    && card->getParentComponent()->getLocalBounds().contains (card->getBounds());
                expect (card->isShowingFor (*modulated) && card->getNumRows() >= modulated->getNumRoutings() && inside,
                        "the card lists the knob's " + juce::String (modulated->getNumRoutings()) + " source(s) inside the window ("
                            + modulated->getParameterId() + ")");
                modulated->closeModCard();
                settle (60);
                expect (! card->isVisible(), "leaving the knob closes the card");

                // The card's rows are controls: drag for depth (one gesture), double-click for zero, right-click to bypass or remove.
                modulated->openModCard (true);
                const auto slot = card->getRowSlot (0);
                auto* depth = slot >= 0 ? processor.apvts.getParameter (processor.getModSlotParamId (slot, "amt")) : nullptr;
                expect (depth != nullptr, "the card's first row names its routing");

                if (depth != nullptr)
                {
                    struct GestureCounter : juce::AudioProcessorParameter::Listener
                    {
                        int begins = 0, ends = 0;
                        void parameterValueChanged (int, float) override {}
                        void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
                    } gestures;
                    depth->addListener (&gestures);

                    const auto row = card->getRowBounds (0).toFloat();
                    const auto from = row.getCentre();
                    const auto to = from.translated (25.0f, 0.0f);
                    const auto rowEvent = [&] (juce::Point<float> at, bool dragged, juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
                    {
                        const auto now = juce::Time::getCurrentTime();
                        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                 card, card, now, from, now, 1, dragged);
                    };
                    const auto read = [&] { return processor.readModSlot (slot); };
                    const auto start = read().depth;
                    // Away from the clamp, so the move shows.
                    processor.setModSlotValue (slot, "amt", 0.2f);
                    settle (80); // the card re-reads its rows each frame
                    auto& component = static_cast<juce::Component&> (*card);
                    clearHistory();
                    component.mouseDown (rowEvent (from, false));
                    component.mouseDrag (rowEvent (to, true));
                    component.mouseUp (rowEvent (to, true));
                    settle (50);
                    const auto dragSteps = undoSteps();
                    expect (dragSteps.size() == 1 && dragSteps[0].endsWith (" depth"),
                            "a card row drag is one undo step ('" + dragSteps.joinIntoString ("', '") + "')");
                    expect (std::abs (read().depth - 0.35f) < 0.02f && gestures.begins == 1 && gestures.ends == 1,
                            "dragging a card row sets its depth in one gesture (0.20 -> " + juce::String (read().depth, 2) + ")");
                    component.mouseDoubleClick (rowEvent (from, false));
                    expect (std::abs (read().depth) < 0.001f, "double-clicking a card row zeroes its depth");
                    depth->removeListener (&gestures);
                    processor.setModSlotValue (slot, "amt", start);

                    card->applyRowAction (0, ModHoverPopup::RowAction::toggleBypass);
                    expect (read().bypass, "the row menu bypasses the routing");
                    card->applyRowAction (0, ModHoverPopup::RowAction::toggleBypass);
                    expect (! read().bypass, "and turns it back on");

                    const auto rowsBefore = card->getNumRows();
                    const auto sourceBefore = read().source;
                    clearHistory();
                    card->applyRowAction (0, ModHoverPopup::RowAction::remove);
                    expect (read().source == Mod::Source::None && (card->getNumRows() == rowsBefore - 1 || ! card->isVisible()),
                            "the row menu removes the routing and the card follows");
                    const auto removeSteps = undoSteps();
                    processor.getUndoManager().undo();
                    expect (removeSteps.size() == 1 && removeSteps[0] == "Remove modulation" && read().source == sourceBefore,
                            "the card's Remove is one undo step and undo restores the routing");
                }

                modulated->closeModCard();
                processor.loadFactoryPreset (neuroWobble);
                settle (200);
            }
        }

        // DOCK opens the browser over the page (UI review 6: inside the
        // window, which keeps its size), loading doesn't close it, the name
        // toggles it, FLOAT returns it.
        if (auto* display = findChild<PresetDisplay> (*editor); display != nullptr && display->onClick != nullptr)
        {
            processor.loadFactoryPreset (0);
            const auto baseWidth = editor->getWidth();
            display->onClick();
            settle (400);
            auto* panel = findChild<PresetPanel> (*editor);
            expect (panel != nullptr && panel->onDockRequest != nullptr, "the browser has a DOCK key");

            if (panel != nullptr && panel->onDockRequest != nullptr)
            {
                panel->onDockRequest (true);
                settle (300);
                expect (panel->isDocked() && panel->isVisible() && editor->getWidth() == baseWidth && editor->getHeight() == 720
                            && panel->getWidth() >= 1000 && panel->getY() > 60 && panel->getBottom() <= 720,
                        "docking opens the browser over the page, the window keeps its size (" + juce::String (baseWidth) + " -> "
                            + juce::String (editor->getWidth()) + "x" + juce::String (editor->getHeight()) + ", panel " + panel->getBounds().toString()
                            + (panel->isDocked() ? " docked" : " floating") + (panel->isVisible() ? " shown)" : " hidden)"));
                const auto start = processor.getCurrentPresetName();
                panel->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
                settle (150);
                panel->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
                settle (150);
                expect (processor.getCurrentPresetName() != start && panel->isVisible(),
                        "the docked browser loads with the arrows and stays open on Enter");
                display->onClick();
                settle (300);
                expect (! panel->isVisible() && editor->getWidth() == baseWidth, "the preset name closes the docked browser");
                display->onClick();
                settle (300);
                expect (panel->isVisible() && panel->isDocked() && editor->getWidth() == baseWidth, "the preset name reopens it docked");
                panel->onDockRequest (false);
                settle (400);
                expect (! panel->isDocked() && panel->isOpen() && editor->getWidth() == baseWidth, "FLOAT returns it to the drop-down");
                panel->close();
                settle (400);
            }
        }

        // UI review 4 (V3, V15, V25, V31, S11, S28, S29): SAVE / SAVE AS, the
        // browser's sort, DX7 banks, stars and tags, the keys and the table
        // search. Saves go to a temporary folder, never the user's presets.
        {
            const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("ilanaUiTestPresets-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
            tempDir.createDirectory();
            IlanaSynthAudioProcessor::userPresetDirectoryOverride = tempDir;
            const auto countFiles = [&tempDir] { return tempDir.findChildFiles (juce::File::findFiles, true, "*.ilanapreset").size(); };
            const auto setCutoff = [&processor] (float hz)
            {
                if (auto* parameter = processor.apvts.getParameter ("f1_cutoff"))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (hz));
            };
            const auto cutoffIn = [&processor] (const juce::File& file)
            {
                IlanaSynthAudioProcessor other;
                other.loadPresetFromFile (file);
                return other.apvts.getRawParameterValue ("f1_cutoff")->load();
            };

            processor.loadFactoryPreset (neuroWobble);
            settle (200);
            auto& saveOverlay = pages->getSaveOverlay();
            pages->savePreset();
            settle (100);
            expect (saveOverlay.isShowing(), "SAVE on a factory preset opens the SAVE AS panel");

            saveOverlay.getNameField().setText (juce::String::fromUTF8 ("Rock'n (Roll): \xc3\xb1/1?"), true);
            settle (100);
            // V12-20: nothing warns after the fact; text that did not come through typing is cleaned in place.
            expect (! saveOverlay.getNameField().getText().containsAnyOf (":/?") && saveOverlay.getNote().isEmpty(),
                    "SAVE AS cleans a name that holds characters a file name can't, quietly (" + saveOverlay.getNameField().getText() + ")");
            saveOverlay.getNameField().clear();
            saveOverlay.getNameField().insertTextAtCaret ("A:B");
            settle (60);
            expect (saveOverlay.getNameField().getText() == "AB", "a character a file name can't hold never gets into the name field as typed");
            saveOverlay.getNameField().setText (juce::String::fromUTF8 ("Rock'n (Roll): \xc3\xb1/1?"), true);
            settle (60);
            const auto suggested = saveOverlay.getSuggestedTags();
            expect (suggested.size() >= 3, "SAVE AS suggests tags to tick (" + suggested.joinIntoString (", ") + ")");
            // (The field starts with the loaded preset's own tags, which a click would take out: pick one it lacks.)
            auto freeTag = juce::String();
            auto carried = juce::StringArray::fromTokens (saveOverlay.getTagsField().getText(), ",", "");
            carried.trim();
            for (const auto& tag : suggested)
                if (freeTag.isEmpty() && ! carried.contains (tag, true))
                    freeTag = tag;
            expect (freeTag.isNotEmpty(), "SAVE AS offers a tag the patch does not carry yet");
            if (freeTag.isNotEmpty())
            {
                saveOverlay.toggleSuggestedTag (freeTag);
                expect (saveOverlay.getTagsField().getText().contains (freeTag), "a suggested tag goes into TAGS when clicked");
                saveOverlay.toggleSuggestedTag (freeTag);
                expect (! saveOverlay.getTagsField().getText().contains (freeTag), "and comes out when clicked again");
            }
            saveOverlay.getTagsField().setText ("dark, wide", false);
            saveOverlay.getAuthorField().setText ("Test Author", false);
            saveOverlay.getCommentField().setText ("A note about the sound.", false);
            setCutoff (900.0f);
            saveOverlay.save();
            settle (100);
            const auto savedName = juce::String::fromUTF8 ("Rock'n (Roll) \xc3\xb1" "1");
            const auto savedFile = tempDir.getChildFile (savedName + ".ilanapreset");
            expect (! saveOverlay.isShowing() && savedFile.existsAsFile() && processor.getCurrentPresetName() == savedName,
                    "SAVE AS keeps apostrophes, brackets and accents, dropping only : / ? (" + processor.getCurrentPresetName() + ")");
            {
                const auto info = processor.getPresetInfo (processor.getAllPresetNames().indexOf (savedName));
                expect (info.author == "Test Author" && info.comment == "A note about the sound.",
                        "SAVE AS stores the author and the comment (" + info.author + ", " + info.comment + ")");
            }
            expect (! pages->isPatchEdited(), "a saved patch is no longer EDITED");

            setCutoff (2400.0f);
            pages->savePreset();
            settle (100);
            auto* display = findChild<PresetDisplay> (*editor);
            expect (! saveOverlay.isShowing() && countFiles() == 1 && std::abs (cutoffIn (savedFile) - 2400.0f) < 5.0f,
                    "SAVE overwrites the loaded user preset in place, with no dialog");
            expect (display != nullptr && display->getNotice() == "SAVED", "SAVE says SAVED by the preset name");

            setCutoff (3100.0f);
            editor->keyPressed (juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0));
            settle (100);
            expect (! saveOverlay.isShowing() && countFiles() == 1 && std::abs (cutoffIn (savedFile) - 3100.0f) < 5.0f, "Ctrl+S saves in place");

            editor->keyPressed (juce::KeyPress ('s', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0));
            settle (100);
            expect (saveOverlay.isShowing(), "Ctrl+Shift+S opens SAVE AS");
            saveOverlay.getNameField().setText (savedName, true);
            setCutoff (500.0f);
            saveOverlay.save();
            expect (saveOverlay.isAskingOverwrite() && countFiles() == 1, "SAVE AS asks before overwriting a name in use");
            saveOverlay.finishOverwrite (false);
            expect (saveOverlay.isShowing() && ! saveOverlay.isAskingOverwrite() && std::abs (cutoffIn (savedFile) - 3100.0f) < 5.0f,
                    "Cancel keeps the old preset and goes back to the name");
            saveOverlay.save();
            saveOverlay.finishOverwrite (true);
            settle (100);
            expect (! saveOverlay.isShowing() && countFiles() == 1 && std::abs (cutoffIn (savedFile) - 500.0f) < 5.0f,
                    "Overwrite replaces it, with no \"Name 2\" copy");

            pages->savePresetAs();
            saveOverlay.getNameField().setText ("neuro wobble", true);
            saveOverlay.save();
            expect (saveOverlay.isShowing() && ! saveOverlay.isAskingOverwrite() && saveOverlay.getNote().contains ("factory") && countFiles() == 1,
                    "a factory preset's name can't be taken");
            editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            expect (! saveOverlay.isShowing(), "Esc closes the SAVE AS panel");

            // Ctrl+Right / Left step through the presets, through the
            // confirm over an edited patch.
            processor.loadFactoryPreset (neuroWobble);
            settle (200);
            editor->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys::commandModifier, 0));
            settle (150);
            const auto afterRight = processor.getCurrentPresetName();
            editor->keyPressed (juce::KeyPress (juce::KeyPress::leftKey, juce::ModifierKeys::commandModifier, 0));
            settle (150);
            expect (afterRight != "Neuro Wobble" && processor.getCurrentPresetName() == "Neuro Wobble",
                    "Ctrl+Right / Ctrl+Left load the next and previous preset (" + afterRight + ")");
            pages->setAsksBeforeReplacingEdits (true);
            setCutoff (700.0f);
            settle (100);
            editor->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys::commandModifier, 0));
            settle (100);
            auto* confirm = findChild<ConfirmOverlay> (*editor);
            expect (confirm != nullptr && confirm->isAsking() && processor.getCurrentPresetName() == "Neuro Wobble",
                    "Ctrl+Right over an edited patch asks first");
            if (confirm != nullptr && confirm->isAsking())
                confirm->finish (false);
            pages->setAsksBeforeReplacingEdits (false);
            processor.loadFactoryPreset (neuroWobble);
            settle (150);

            // Ctrl/Cmd + 1-9 pick tabs (UI review 6: bare digits are left
            // to the host and the keyboard); there is no tenth tab for 0.
            if (auto* tabbed = findChild<juce::TabbedComponent> (*editor))
            {
                const auto before = tabbed->getCurrentTabIndex();
                tabbed->setCurrentTabIndex (0);
                const auto bareTaken = editor->keyPressed (juce::KeyPress ('2'));
                const auto onBare = tabbed->getCurrentTabIndex();
                editor->keyPressed (juce::KeyPress ('2', juce::ModifierKeys::commandModifier, 0));
                const auto onTwo = tabbed->getCurrentTabIndex();
                expect (! bareTaken && onBare == 0 && onTwo == 1
                            && ! editor->keyPressed (juce::KeyPress ('0', juce::ModifierKeys::commandModifier, 0))
                            && tabbed->getCurrentTabIndex() == 1,
                        "Ctrl+2 picks the second tab, a bare 2 and Ctrl+0 do nothing");
                tabbed->setCurrentTabIndex (before);
                settle (150);
            }

            // Esc closes the drop-down browser.
            if (display != nullptr && display->onClick != nullptr)
            {
                display->onClick();
                settle (400);
                auto* panel = findChild<PresetPanel> (*editor);
                expect (panel != nullptr && panel->isOpen(), "the browser opens for the browser checks");

                if (panel != nullptr)
                {
                    const auto sortBefore = panel->getSortMode();
                    panel->setSortMode (PresetPanel::sortByName);
                    panel->selectFilter ("");
                    auto listed = panel->getListedNames();
                    auto sorted = true;
                    const auto listedShown = panel->getListedDisplayNames();
                    for (int i = 1; i < listedShown.size(); ++i)
                        sorted = sorted && listedShown[i - 1].compareNatural (listedShown[i], false) <= 0;
                    auto dx7Shown = 0;
                    for (const auto& name : listed)
                        dx7Shown += name.endsWith ("(ROM1A)") || name.endsWith ("(DEXED01)") ? 1 : 0;
                    expect (sorted && listed.size() > 100, "the browser sorts by the names it shows (" + listedShown[0] + ", " + listedShown[1] + ", "
                                                              + listedShown[2] + "...)");
                    expect (dx7Shown >= 64 && listed.contains (savedName) && ! panel->getSidebarKeys().contains ("DX7"),
                            "All means all: the DX7 voices are in it, with no DX7 folder (" + juce::String (listed.size()) + " listed)");

                    // UI review 6: the DX7 chip narrows to the voices, a chip
                    // per bank; names show in Title Case without the bank.
                    auto chipKeys = panel->getChipKeys();
                    expect (chipKeys.contains ("pack:dx7") && ! chipKeys.contains ("bank:ROM1A") && ! chipKeys.contains ("tag:DX7"),
                            "the DX7 chip leads the chip row, banks folded away (" + chipKeys.joinIntoString (" ").substring (0, 120) + ")");
                    panel->clickChip ("pack:dx7");
                    chipKeys = panel->getChipKeys();
                    expect (panel->getListedNames().size() >= 270 && chipKeys.contains ("bank:ROM1A") && chipKeys.contains ("bank:DEXED01"),
                            "the DX7 chip lists the voices with a chip per bank (" + juce::String (panel->getListedNames().size()) + ")");

                    // UI review 7 (S7-10, I7-41, V7-25): a voice a later
                    // cartridge repeats is listed once, with "also in", and
                    // under its own bank's chip; DX7 rows drop the FM tag and
                    // their bank tag while a bank chip is on; the tags get a
                    // line of their own.
                    {
                        const auto all = panel->getListedNames();
                        const auto original = all.indexOf ("ORCHESTRA (ROM1A)");
                        const auto tip = panel->getRowTooltip (original);
                        expect (original >= 0 && ! all.contains ("ORCHESTRA (ROM4B)") && tip.contains ("Also in ROM4B")
                                    && panel->getRowTooltip (all.indexOf ("BRASS 2 (ROM1A)")).contains ("ROM4A (as Synth Brass)"),
                                "a repeated DX7 voice is listed once, with the bank that repeats it (" + tip.fromLastOccurrenceOf ("\n", false, false) + ")");
                        auto fmTagged = 0;
                        for (int row = 0; row < juce::jmin (40, all.size()); ++row)
                            fmTagged += panel->getRowTags (row).contains ("FM") ? 1 : 0;
                        expect (fmTagged == 0, "DX7 rows carry no FM tag (" + juce::String (fmTagged) + " of 40 do)");
                        panel->clickChip ("bank:ROM4B");
                        const auto rom4b = panel->getListedNames();
                        expect (rom4b.size() == 32 && rom4b.contains ("ORCHESTRA (ROM4B)"), "the ROM4B chip lists all its 32 voices, repeats included");
                        panel->clickChip ("bank:ROM4B");
                        expect (Presets::dx7DisplayName ("HARPSICH 1 (ROM1A)") == "Harpsichord 1"
                                    && Presets::dx7DisplayName ("JAZZ GUIT1 (ROM1B)") == "Jazz Guitar 1"
                                    && Presets::dx7DisplayName ("STRG ENS 1 (ROM2A)") == "Strings Ensemble 1"
                                    && Presets::dx7DisplayName ("E.P-BRS BC (ROM3B)") == "E.Piano-Brass BC"
                                    && Presets::dx7DisplayName ("PIANO 5THS (ROM3A)") == "Piano 5ths",
                                "DX7 names spell out the cartridge's cuts (" + Presets::dx7DisplayName ("HARPSICH 1 (ROM1A)") + ", "
                                    + Presets::dx7DisplayName ("JAZZ GUIT1 (ROM1B)") + ", " + Presets::dx7DisplayName ("E.P-BRS BC (ROM3B)") + ")");
                        const auto categories = processor.getAllPresetCategories();
                        const auto allNames = processor.getAllPresetNames();
                        const auto factoryNames = processor.getFactoryPresetNames();
                        const auto macros = [&] (const juce::String& name) { return processor.getFactoryMacroNames (factoryNames.indexOf (name)).joinIntoString (" "); };
                        expect (categories[allNames.indexOf ("BRASS 1 (ROM1A)")] == "Brass" && categories[allNames.indexOf ("FLUTE 1 (ROM1A)")] == "Wind"
                                    && panel->getSidebarKeys().contains ("Brass") && panel->getSidebarKeys().contains ("Wind"),
                                "brass and wind DX7 voices have their own categories");
                        expect (macros ("E.PIANO 1 (ROM1A)") == "BARK DARKEN WOBBLE ROOM" && macros ("BASS 1 (ROM1A)") == "GROWL DARKEN DETUNE ROOM",
                                "DX7 voices name their macros for their kind of sound (" + macros ("E.PIANO 1 (ROM1A)") + " / "
                                    + macros ("BASS 1 (ROM1A)") + ")");
                    }
                    panel->clickChip ("bank:ROM1B");
                    listed = panel->getListedNames();
                    const auto shown = panel->getListedDisplayNames();
                    auto allRom1B = listed.size() == 32;
                    for (const auto& name : listed)
                        allRom1B = allRom1B && name.endsWith ("(ROM1B)");
                    expect (allRom1B, "the ROM1B chip lists that bank's 32 voices");
                    const auto piano = listed.indexOf ("E.PIANO 1 (ROM1B)") >= 0 ? listed.indexOf ("E.PIANO 1 (ROM1B)") : 0;
                    expect (shown.joinIntoString ("|").indexOf ("(ROM1B)") < 0 && shown[piano] == Presets::dx7DisplayName (listed[piano])
                                && Presets::dx7DisplayName ("E.PIANO 1 (ROM1A)") == "E.Piano 1"
                                && Presets::dx7DisplayName ("SYN-LEAD 1 (ROM1A)") == "Syn-Lead 1",
                            "DX7 names show in Title Case without the bank (" + shown[0] + ", " + shown[1] + ")");
                    panel->clickChip ("bank:ROM1B");
                    panel->clickChip ("pack:dx7");
                    expect (panel->getListedNames().size() > 600, "the DX7 chip clicked again shows everything");

                    // DX7 voices are filed by sound, beside the factory sounds.
                    panel->selectFilter ("Keys");
                    listed = panel->getListedNames();
                    expect (listed.contains ("E.PIANO 1 (ROM1A)") && listed.contains ("Init") == false,
                            "the DX7 voices are filed by sound (E.Piano 1 under Keys)");
                    panel->selectFilter ("Bass");
                    expect (panel->getListedNames().contains ("PLUCK BASS (ROM3B)"), "PLUCK BASS is filed under Bass");
                    panel->selectFilter ("FX");
                    expect (panel->getListedNames().contains ("GRAND PRIX (ROM3A)") || ! processor.getAllPresetNames().contains ("GRAND PRIX (ROM3A)"),
                            "GRAND PRIX is filed under FX");

                    // Every factory preset carries 3-6 descriptive tags, shown
                    // on its row and as chips; macro names only on hover.
                    {
                        const auto tags = processor.getAllPresetTags();
                        const auto factoryCount = processor.getFactoryPresetNames().size();
                        auto tagged = 0;
                        juce::String untagged;
                        for (int i = 0; i < factoryCount; ++i)
                        {
                            const auto count = juce::StringArray::fromTokens (tags[i], ",", "").size();
                            if (count >= 3 && count <= 6)
                                ++tagged;
                            else if (untagged.isEmpty())
                                untagged = processor.getAllPresetNames()[i] + " (" + tags[i] + ")";
                        }
                        expect (tagged == factoryCount, "every factory preset has 3-6 tags (" + juce::String (tagged) + "/"
                                                            + juce::String (factoryCount) + " " + untagged + ")");

                        panel->selectFilter ("");
                        chipKeys = panel->getChipKeys();
                        expect (chipKeys.contains ("tag:Bright") && chipKeys.contains ("tag:FM"), "the tags show as chips");
                        panel->clickChip ("tag:Bell");
                        listed = panel->getListedNames();
                        auto allBells = listed.size() > 10;
                        for (const auto& name : listed)
                            allBells = allBells && tags[processor.getAllPresetNames().indexOf (name)].contains ("Bell");
                        expect (allBells, "a tag chip lists the presets with that tag (" + juce::String (listed.size()) + " bells)");
                        expect (panel->getRowTags (0).contains ("Bell") || panel->getRowTags (0).size() > 0,
                                "a row shows its tags (" + panel->getRowTags (0).joinIntoString (", ") + ")");
                        panel->clickChip ("tag:Bell");
                        const auto rowTip = panel->getRowTooltip (panel->getListedNames().indexOf ("Rip Bass"));
                        expect (rowTip.contains ("Macros:") && rowTip.contains ("TONE"), "the macro names show on hover: " + rowTip.replace ("\n", " / "));
                    }

                    // The list shows whole rows only: no clipped last row.
                    expect (panel->getListBounds().getHeight() % panel->getListRowHeight() == 0,
                            "the list ends on a whole row (" + juce::String (panel->getListBounds().getHeight()) + " px, rows of "
                                + juce::String (panel->getListRowHeight()) + ")");

                    // DELETE says why it can't delete a factory preset.
                    panel->selectFilter ("");
                    expect (! panel->isDeleteEnabled() && panel->getDeleteTooltip().containsIgnoreCase ("factory"),
                            "DELETE explains itself on a factory preset: " + panel->getDeleteTooltip());

                    // S15-10: DOCK is as tall as the bottom buttons. (UI audit 20: DELETE's reason lives in its tooltip, checked
                    // above, so the details pane keeps its room for the preset's macros and routes.)
                    expect (panel->getDockBounds().getHeight() == panel->getFooterButtonBounds().getHeight(),
                            "DOCK is the height of the bottom buttons (" + juce::String (panel->getDockBounds().getHeight()) + " vs "
                                + juce::String (panel->getFooterButtonBounds().getHeight()) + ")");

                    // S15-1: the audition sounds C3 and stops by itself.
                    if (! panel->getPlayBounds().isEmpty())
                    {
                        panel->startAudition();
                        const auto sounding = panel->isAuditioning();
                        const auto lit = panel->isPlayLit();
                        settle (1300);
                        expect (sounding && ! panel->isAuditioning(), "PLAY sounds C3 and lets go after about a second");
                        expect (lit && ! panel->isPlayLit(), "PLAY is lit while its note sounds and fades out after it");

                        // (No audio device runs here: the preview events wait in the FIFO; hand them to the processor.)
                        juce::AudioBuffer<float> drain (2, 512);
                        juce::MidiBuffer noMidi;
                        processor.processBlock (drain, noMidi);
                        processor.processBlock (drain, noMidi);
                    }

                    panel->setSortMode (PresetPanel::sortByCategory);
                    listed = panel->getListedNames();
                    const auto categories = processor.getAllPresetCategories();
                    const auto allNames = processor.getAllPresetNames();
                    expect (categories[allNames.indexOf (listed[1])] == "Bass", "SORT: CATEGORY puts the basses first after Init (" + listed[1] + ")");
                    panel->setSortMode (PresetPanel::sortByName);

                    // The star on a row toggles the favourite without loading it.
                    listed = panel->getListedNames();
                    const auto row = listed.indexOf ("Metal Pad");
                    panel->clickRow (row, 12);
                    panel->selectFilter ("*fav");
                    const auto starred = panel->getListedNames().contains ("Metal Pad");
                    panel->selectFilter ("");
                    panel->clickRow (panel->getListedNames().indexOf ("Metal Pad"), 12);
                    panel->selectFilter ("*fav");
                    const auto unstarred = ! panel->getListedNames().contains ("Metal Pad");
                    expect (row >= 0 && starred && unstarred && processor.getCurrentPresetName() == "Neuro Wobble",
                            "a row's star toggles the favourite (and doesn't load the preset)");

                    // Tags saved with user presets become chips.
                    panel->selectFilter ("*user");
                    expect (panel->getChipKeys().contains ("tag:dark") && panel->getChipKeys().contains ("tag:wide"),
                            "user preset tags show as chips (" + panel->getChipKeys().joinIntoString (" ") + ")");
                    panel->clickChip ("tag:dark");
                    expect (panel->getListedNames().size() == 1, "a tag chip filters the list");
                    panel->clickChip ("tag:dark");

                    panel->selectFilter ("");
                    panel->setSortMode (sortBefore);
                    editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
                    settle (400);
                    expect (! panel->isOpen(), "Esc closes the browser");
                }
            }

            // The wavetable browser: a search field, spaced Title Case names.
            {
                TableBrowser browser (processor, "osc1_table", IlanaTheme::accent());
                browser.setLookAndFeel (&editor->getLookAndFeel());
                browser.setSize (740, 520);
                const auto all = browser.getShownNames();
                browser.setSearchText ("saw");
                settle (100);
                const auto saws = browser.getShownNames();
                auto allSaws = saws.size() > 2 && saws.size() < all.size();
                for (const auto& name : saws)
                    allSaws = allSaws && name.containsIgnoreCase ("saw");
                expect (allSaws && saws.contains ("Drive Saw") && saws.contains ("Analog Saw"),
                        "the table search filters (" + saws.joinIntoString (", ") + ")");

                // The built-in packs: listed after the factory tables, found by the search,
                // and a pick selects it like a factory table.
                expect (BuiltInTables::count() > 300, "the synth carries its built-in wavetables");
                const auto builtIn = BuiltInTables::count() / 2;
                expect (all.contains (BuiltInTables::name (builtIn)), "the browser lists the built-in tables");
                browser.setSearchText (BuiltInTables::name (builtIn));
                settle (100);
                expect (browser.getShownNames().contains (BuiltInTables::name (builtIn)), "the search finds a built-in table");
                browser.setSearchText ({});
                const auto slotBefore = processor.findFreeUserSlot();
                browser.pickBuiltInTable (builtIn);
                const auto table = juce::roundToInt (processor.apvts.getRawParameterValue ("osc1_table")->load());
                expect (table == TableFactory::getNumFactoryTables() + IlanaSynthAudioProcessor::numUserSlots + builtIn
                            && processor.findFreeUserSlot() == slotBefore && BuiltInTables::table (builtIn) != nullptr
                            && processor.getWavetable (table) == BuiltInTables::table (builtIn),
                        "picking a built-in table selects it as a factory table (no User slot used) and it is ready to play");
                expect (processor.getWavetable (table)->getName() == BuiltInTables::name (builtIn)
                            && processor.apvts.getParameter ("osc1_table") != nullptr
                            && dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("osc1_table"))->getCurrentChoiceName() == BuiltInTables::name (builtIn),
                        "the choice reads as the table's name");
                expect (all.contains ("Hard Sync") && ! all.contains ("DriveSaw") && ! all.contains ("Basic"),
                        "table names show as spaced Title Case, ANALOG's Basic as Analog Saw");
            }

            // UI review 6 (I6-35, I6-36): a .syx dropped on the window imports
            // the bank, filed by sound, and "Show in browser" opens it there.
            if (auto* confirm = findChild<ConfirmOverlay> (*editor); confirm != nullptr)
            {
                juce::MemoryOutputStream syx;
                for (const auto byte : { 0xF0, 0x43, 0x00, 0x09, 0x20, 0x00 })
                    syx.writeByte ((char) byte);
                syx.write (Dx7Banks::banks[0].data, 4096);
                auto sum = 0;
                for (int i = 0; i < 4096; ++i)
                    sum += Dx7Banks::banks[0].data[i];
                syx.writeByte ((char) ((128 - (sum & 127)) & 127));
                syx.writeByte ((char) 0xF7);
                const auto syxFile = tempDir.getChildFile ("TESTBANK.syx");
                syxFile.replaceWithData (syx.getData(), syx.getDataSize());

                expect (pages->isInterestedInFileDrag ({ syxFile.getFullPathName() })
                            && ! pages->isInterestedInFileDrag ({ tempDir.getChildFile ("x.wav").getFullPathName() }),
                        "the window takes .syx drops (and leaves other files to the sample zones)");
                pages->filesDropped ({ syxFile.getFullPathName() }, 10, 10);
                settle (200);
                expect (confirm->isAsking() && confirm->getTitle() == "Imported TESTBANK",
                        "a dropped bank imports and says so (" + confirm->getTitle() + ")");
                confirm->finish (true);
                settle (500);
                auto* panel = findChild<PresetPanel> (*editor);
                const auto listed = panel != nullptr ? panel->getListedNames() : juce::StringArray();
                const auto categories = processor.getAllPresetCategories();
                const auto names = processor.getAllPresetNames();
                const auto firstCategory = listed.isEmpty() ? juce::String() : categories[names.indexOf (listed[0])];
                expect (panel != nullptr && panel->isOpen() && listed.size() == 32 && panel->getChipKeys().contains ("bank:TESTBANK")
                            && firstCategory.isNotEmpty() && firstCategory != "DX7",
                        "Show in browser opens the new bank's 32 voices, filed by sound (" + juce::String (listed.size()) + ", "
                            + firstCategory + ")");
                if (panel != nullptr)
                {
                    panel->close();
                    settle (400);
                }
            }

            IlanaSynthAudioProcessor::userPresetDirectoryOverride = juce::File();
            tempDir.deleteRecursively();
            processor.loadFactoryPreset (neuroWobble);
            settle (200);
        }

        // The header's live waveform and the VOICES hotspot.
        {
            auto* scope = findChild<OutputView> (*editor);
            std::vector<OutputView*> views;
            findAll<OutputView> (*editor, views);
            auto strips = 0;
            for (auto* view : views)
                strips += view->isStrip() && visibleInTree (view) && view->getWidth() > 100 ? 1 : 0;
            juce::ignoreUnused (scope);
            expect (strips == 1, "the header shows one live waveform strip");
        }
    }
}

