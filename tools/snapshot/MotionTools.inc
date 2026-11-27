// --motion <dir>: frames of the review-26 flair (Motion: Full), 30 a
// second, one folder per scene, for the PR's recordings.
juce::Button* findButtonWithText (juce::Component& parent, const juce::String& text)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* button = dynamic_cast<juce::Button*> (child); button != nullptr && (button->getButtonText() == text || button->getName() == text) && IlanaAnim::showing (*button))
            return button;
        if (auto* nested = findButtonWithText (*child, text))
            return nested;
    }
    return nullptr;
}

int runMotion (const juce::File& outDir)
{
    IlanaAnim::fullMotion() = true;
    // A frame here takes about 150 ms to render: the timed motions (pop-ups,
    // tabs, cards, shapes) run 10 times slower and a frame is taken every
    // 330 ms, so the GIFs at 30 fps play them at their real speed.
    IlanaAnim::motionClockRate() = 0.1;
    outDir.createDirectory();
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
    // A bare window, so the editor counts as on screen and animates (the
    // missing window manager's atom is ignored, as for ILANA_SNAPSHOT_FLASH).
#if JUCE_LINUX
    if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
        setHandler ([] (void*, void*) { return 0; });
#endif
    editor->addToDesktop (0);
    editor->setVisible (true);
    settle (800);
    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);
    auto* shell = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());

    std::atomic<bool> running { true }, playing { false };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (2, 256);
        auto wasPlaying = false;
        auto next = juce::Time::getMillisecondCounterHiRes();
        while (running)
        {
            juce::MidiBuffer midi;
            const auto now = playing.load();
            if (now != wasPlaying)
                for (auto note : { 48, 55, 60, 64 })
                    midi.addEvent (now ? juce::MidiMessage::noteOn (1, note, 0.9f) : juce::MidiMessage::noteOff (1, note), 0);
            wasPlaying = now;
            buffer.clear();
            processor.processBlock (buffer, midi);
            next += 256.0 / 48.0;
            const auto wait = next - juce::Time::getMillisecondCounterHiRes();
            if (wait > 0.0)
                std::this_thread::sleep_for (std::chrono::microseconds ((int) (wait * 1000.0)));
        }
    });

    const auto record = [&] (const juce::String& scene, int frames, std::function<void (int)> atFrame)
    {
        const auto started = juce::Time::getMillisecondCounterHiRes();
        auto dir = outDir.getChildFile (scene);
        dir.deleteRecursively();
        dir.createDirectory();
        // (Kept in memory and written afterwards: a PNG per frame took longer than the motion.)
        std::vector<juce::Image> images;
        auto lastFrame = juce::Time::getMillisecondCounterHiRes();
        for (int frame = 0; frame < frames; ++frame)
        {
            atFrame (frame);
            settle (juce::jmax (1, 330 - (int) (juce::Time::getMillisecondCounterHiRes() - lastFrame)));
            lastFrame = juce::Time::getMillisecondCounterHiRes();
            images.push_back (editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f));
        }
        for (size_t frame = 0; frame < images.size(); ++frame)
        {
            juce::FileOutputStream stream (dir.getChildFile (juce::String::formatted ("f%03d.png", (int) frame)));
            if (stream.openedOk())
            {
                stream.setPosition (0);
                stream.truncate();
                juce::PNGImageFormat().writeImageToStream (images[frame], stream);
            }
        }
        std::cout << scene << ": " << frames << " frames in " << (int) (juce::Time::getMillisecondCounterHiRes() - started) << " ms" << std::endl;
    };
    const auto set = [&processor] (const char* id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };

    shell->showPage ("MAIN");
    settle (600);
    set ("west_on", 0.0f);
    set ("subosc_on", 0.0f);
    settle (600);
    record ("switch", 30, [&] (int f) { if (f == 4) set ("west_on", 1.0f); if (f == 6) set ("subosc_on", 1.0f); if (f == 20) set ("west_on", 0.0f); });
    record ("breathe", 45, [&] (int f) { if (f == 2) playing = true; if (f == 30) playing = false; });
    record ("stereo", 30, [&] (int f)
    {
        if (f == 3 || f == 18)
            if (auto* button = findButtonWithText (*editor, "STEREO"))
                button->triggerClick();
    });
    record ("tabs", 36, [&] (int f) { if (f == 3) shell->showPage ("FX"); if (f == 15) shell->showPage ("MATRIX"); if (f == 27) shell->showPage ("MAIN"); });
    shell->showPage ("MAIN");
    settle (600);
    record ("morph", 24, [&] (int f)
    {
        if (f == 4 || f == 14)
        {
            IlanaAnim::armMorphs();
            processor.loadFactoryPreset (juce::jmin (f == 4 ? 30 : 60, (int) processor.getFactoryPresetNames().size() - 1));
            IlanaAnim::playMorphs();
        }
    });
    shell->showPage ("MAIN");
    playing = true;
    settle (500);
    record ("lfo-trail", 30, [&] (int f) { if (f == 29) playing = false; });
    shell->showPage ("MAIN");
    settle (500);
    record ("cards", 36, [&] (int f)
    {
        if (f == 3)
        {
            auto* add = findButtonWithText (*editor, "+  ADD OSC");
            if (add != nullptr)
                add->triggerClick();
        }
        if (f == 22)
        {
            // The last card's header x.
            juce::Button* last = nullptr;
            std::function<void (juce::Component&)> scan = [&] (juce::Component& parent)
            {
                for (auto* child : parent.getChildren())
                {
                    if (auto* button = dynamic_cast<juce::Button*> (child); button != nullptr && IlanaAnim::showing (*button)
                        && button->getTooltip().startsWith ("Remove this oscillator"))
                        last = button;
                    scan (*child);
                }
            };
            scan (*editor);
            if (last != nullptr)
                last->triggerClick();
        }
    });

    running = false;
    audio.join();
    std::cout << "motion frames in " << outDir.getFullPathName() << std::endl;
    return 0;
}

// --flair <dir>: frames of the 2026-11-01 flair at 175 % (Motion: Full),
// one folder per scene, cropped to what moves: the keyboard lighting up
// for host notes, the logo breathing, a knob's start tick, a preset name
// decoding, the scope's afterglow and the clip light's bloom.
int runFlair (const juce::File& outDir)
{
    IlanaAnim::fullMotion() = true;
    IlanaAnim::motionClockRate() = 0.1; // (as --motion: frames 330 ms apart play at real speed at 30 fps)
    outDir.createDirectory();
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
#if JUCE_LINUX
    if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
        setHandler ([] (void*, void*) { return 0; });
#endif
    editor->addToDesktop (0);
    editor->setVisible (true);
    settle (800);
    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);
    auto* shell = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
    const auto set = [&processor] (const char* id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };

    // Host notes: a chord, harder on the bass; `gain` turns the output up for the clip scene.
    std::atomic<bool> running { true }, playing { false }, single { false };
    std::atomic<float> gain { 1.0f };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (2, 256);
        auto wasPlaying = false;
        auto next = juce::Time::getMillisecondCounterHiRes();
        while (running)
        {
            juce::MidiBuffer midi;
            const auto now = playing.load();
            if (now != wasPlaying)
                for (auto [note, velocity] : { std::pair { 36, 1.0f }, { 60, 0.9f }, { 64, 0.6f }, { 67, 0.35f } })
                    if (! single || note == 60)
                        midi.addEvent (now ? juce::MidiMessage::noteOn (1, note, velocity) : juce::MidiMessage::noteOff (1, note), 0);
            wasPlaying = now;
            buffer.clear();
            processor.processBlock (buffer, midi);
            if (gain.load() != 1.0f)
            {
                buffer.applyGain (gain.load());
                processor.setOutputPeakForTest (buffer); // (the meter reads what the host would hear)
            }
            next += 256.0 / 48.0;
            const auto wait = next - juce::Time::getMillisecondCounterHiRes();
            if (wait > 0.0)
                std::this_thread::sleep_for (std::chrono::microseconds ((int) (wait * 1000.0)));
        }
    });

    const auto record = [&] (const juce::String& scene, juce::Rectangle<int> crop, int frames, std::function<void (int)> atFrame)
    {
        auto dir = outDir.getChildFile (scene);
        dir.deleteRecursively();
        dir.createDirectory();
        std::vector<juce::Image> images;
        auto lastFrame = juce::Time::getMillisecondCounterHiRes();
        for (int frame = 0; frame < frames; ++frame)
        {
            atFrame (frame);
            settle (juce::jmax (1, 330 - (int) (juce::Time::getMillisecondCounterHiRes() - lastFrame)));
            lastFrame = juce::Time::getMillisecondCounterHiRes();
            images.push_back (editor->createComponentSnapshot (crop.isEmpty() ? editor->getLocalBounds() : crop, true, 1.75f));
        }
        for (size_t frame = 0; frame < images.size(); ++frame)
        {
            juce::FileOutputStream stream (dir.getChildFile (juce::String::formatted ("f%03d.png", (int) frame)));
            if (stream.openedOk())
            {
                stream.setPosition (0);
                stream.truncate();
                juce::PNGImageFormat().writeImageToStream (images[frame], stream);
            }
        }
        std::cout << scene << ": " << frames << " frames" << std::endl;
    };
    const auto boundsOf = [&] (juce::Component* c, int margin) { return c == nullptr ? juce::Rectangle<int>() : editor->getLocalArea (c->getParentComponent(), c->getBounds()).expanded (margin).getIntersection (editor->getLocalBounds()); };

    IlanaTheme::uiScaleRef() = IlanaTheme::uiScaleRef() * 1.75f;
    IlanaTheme::notifyUiScaleListeners();
    shell->showPage ("MAIN");
    settle (600);

    // 1 The keyboard, open, lighting up for the host's chord and fading.
    if (auto* keys = findButtonWithText (*editor, "KEYBOARD"))
        keys->triggerClick();
    settle (800);
    record ("keyboard", boundsOf (findChild<KeyboardStrip> (*editor), 4), 24, [&] (int f) { if (f == 3) playing = true; if (f == 12) playing = false; });

    // 2 The logo breathing with the same chord.
    record ("logo", boundsOf (findChild<LogoComponent> (*editor), 6), 30, [&] (int f) { if (f == 3) playing = true; if (f == 20) playing = false; });
    if (auto* keys = findButtonWithText (*editor, "KEYBOARD"))
        keys->triggerClick();
    settle (600);

    // 3 A knob dragged from 30 % to 70 % and let go: the start tick, then its fade.
    std::vector<KnobControl*> knobs;
    findAll<KnobControl> (*editor, knobs);
    KnobControl* cutoff = nullptr;
    for (auto* knob : knobs)
        if (knob->getParameterId() == "f1_cutoff" && visibleInTree (knob))
            cutoff = knob;
    if (cutoff != nullptr)
    {
        auto& slider = cutoff->getSlider();
        slider.setValue (slider.proportionOfLengthToValue (0.3), juce::sendNotificationSync);
        settle (300);
        record ("knob", boundsOf (cutoff, 6), 22, [&] (int f)
        {
            if (f == 2)
            {
                slider.getProperties().set ("dragGhostPos", 0.3f);
                slider.getProperties().set ("dragGhostRelease", 0.0);
            }
            if (f >= 2 && f <= 8)
                slider.setValue (slider.proportionOfLengthToValue (0.3 + 0.4 * (f - 2) / 6.0), juce::sendNotificationSync);
            if (f == 10)
                slider.getProperties().set ("dragGhostRelease", juce::Time::getMillisecondCounterHiRes());
            slider.repaint();
        });
    }

    // 4 Two preset loads: the name decodes in.
    juce::Component* presetField = nullptr;
    {
        std::vector<PresetDisplay*> displays;
        findAll<PresetDisplay> (*editor, displays);
        if (! displays.empty())
            presetField = displays.front();
    }
    record ("decode", boundsOf (presetField, 4), 24, [&] (int f)
    {
        if (f == 3 || f == 14)
            processor.loadFactoryPreset (juce::jmin (f == 3 ? 30 : 60, (int) processor.getFactoryPresetNames().size() - 1));
    });

    // 5 The scope while the cutoff sweeps: the afterglow trails the change.
    processor.loadFactoryPreset (0);
    shell->showPage ("MAIN");
    settle (400);
    if (auto* scope = findButtonWithText (*editor, "SCOPE"))
        scope->triggerClick();
    settle (600);
    single = true; // (one note: a chord's wave jitters at the trigger)
    playing = true;
    record ("scope", {}, 26, [&] (int f) { set ("f1_cutoff", 300.0f * std::pow (2.0f, (float) (f % 13) * 0.45f)); });
    playing = false;
    single = false;
    if (auto* scope = findButtonWithText (*editor, "SCOPE"))
        scope->triggerClick();
    settle (600);

    // 6 The clip light: the output turned up past 0 dB, once.
    OutputMeter* meter = nullptr;
    {
        std::vector<OutputMeter*> meters;
        findAll<OutputMeter> (*editor, meters);
        if (! meters.empty())
            meter = meters.front();
    }
    record ("clip", boundsOf (meter, 6), 18, [&] (int f) { if (f == 3) { gain = 8.0f; playing = true; } if (f == 5) { playing = false; gain = 1.0f; } });

    running = false;
    audio.join();
    std::cout << "flair frames in " << outDir.getFullPathName() << std::endl;
    return 0;
}

int runUsability2 (const juce::File& outDir)
{
    IlanaAnim::fullMotion() = true;
    outDir.createDirectory();
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
#if JUCE_LINUX
    if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
        setHandler ([] (void*, void*) { return 0; });
#endif
    editor->addToDesktop (0);
    editor->setVisible (true);
    settle (800);
    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);
    auto* shell = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
    IlanaTheme::uiScaleRef() = IlanaTheme::uiScaleRef() * 1.75f;
    IlanaTheme::notifyUiScaleListeners();

    const auto shoot = [&] (const juce::String& name)
    {
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.75f);
        juce::FileOutputStream stream (outDir.getChildFile (name + ".png"));
        if (stream.openedOk())
        {
            stream.setPosition (0);
            stream.truncate();
            juce::PNGImageFormat().writeImageToStream (image, stream);
        }
        std::cout << name << std::endl;
    };
    const auto mouse = [&] (juce::Point<float> at, juce::ModifierKeys mods)
    {
        // (The real pointer too: isMouseOver and getMouseXYRelative ask X where it is.)
        juce::Desktop::setMousePosition (editor->localPointToGlobal (at.roundToInt()));
        if (auto* peer = editor->getPeer())
            peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse, at, mods, juce::MouseInputSource::defaultPressure,
                                    juce::MouseInputSource::defaultOrientation, juce::Time::currentTimeMillis());
    };
    const auto turn = [&] (const char* id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
        {
            processor.getUndoManager().beginNewTransaction();
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
            parameter->endChangeGesture();
            processor.getUndoManager().beginNewTransaction();
        }
    };
    const auto knobFor = [&] (const juce::String& id) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && visibleInTree (knob))
                return knob;
        return nullptr;
    };

    // A preset with LFO 1 routes on more than one page.
    for (int index = 0; index < processor.getFactoryPresetNames().size(); ++index)
    {
        processor.loadFactoryPreset (index);
        auto routes = 0;
        for (int i = 0; i < Mod::maxSlots; ++i)
            routes += processor.readModSlot (i).source == Mod::Source::Lfo1 ? 1 : 0;
        if (routes >= 3)
        {
            std::cout << "preset: " << processor.getFactoryPresetNames()[index] << std::endl;
            break;
        }
    }
    shell->showPage ("MAIN");
    settle (800);

    // 1 Edited since load: RESO (PLAY) and a reverb knob (FX) turned.
    {
        shell->showPage ("FX");
        settle (500);
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        juce::String fxId;
        for (auto* knob : knobs)
            if (visibleInTree (knob) && fxId.isEmpty() && knob->getParameterId().isNotEmpty())
                fxId = knob->getParameterId();
        shell->showPage ("MAIN");
        settle (500);
        turn ("f1_reso", 0.62f);
        if (fxId.isNotEmpty())
            if (auto* parameter = processor.apvts.getParameter (fxId))
                parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.2f : 0.8f);
        std::cout << "fx knob: " << fxId << std::endl;
    }
    settle (600);
    shoot ("1-edited-dots");

    // 2 A ring hovered: CUTOFF shows its depth and source.
    if (auto* knob = knobFor ("f1_cutoff"))
    {
        auto& slider = knob->getSlider();
        const auto centre = editor->getLocalPoint (&slider, slider.getLocalBounds().getCentre()).toFloat();
        auto found = false;
        for (float r = 10.0f; r < 60.0f && ! found; r += 1.0f)
            for (float a = -2.4f; a < 2.4f && ! found; a += 0.1f)
            {
                const auto at = centre + juce::Point<float> (std::sin (a), -std::cos (a)) * r;
                mouse (at, {});
                if (knob->hotRouting() != nullptr)
                    found = true;
            }
        std::cout << "ring hovered: " << (found ? "yes" : "no") << std::endl;
        settle (300);
        shoot ("2-ring-depth");
        mouse ({ 2.0f, 700.0f }, {});
        settle (300);
    }

    // 3 A|B hovered after B's RESO and DRIVE moved.
    if (auto* ab = findChild<ABButton> (*editor))
    {
        ab->triggerClick(); // (B starts as a copy of A)
        settle (300);
        turn ("f1_reso", 0.3f);
        turn ("f1_drive", 0.7f);
        settle (300);
        mouse (editor->getLocalPoint (ab, ab->getLocalBounds().getCentre()).toFloat(), {});
        settle (600);
        shoot ("3-ab-diff");
        std::cout << "A|B tooltip: " << ab->getTooltip() << std::endl;
        mouse ({ 2.0f, 700.0f }, {});
        settle (300);
    }

    // 4 OSC 3 (prefix "sub") switched off: its greyed TABLE hovered.
    {
        std::vector<ComboControl*> combos;
        findAll<ComboControl> (*editor, combos);
        for (auto* combo : combos)
            if (combo->getParameterId() == "sub_table" && visibleInTree (combo))
            {
                mouse (editor->getLocalPoint (combo, combo->getLocalBounds().getCentre()).toFloat(), {});
                break;
            }
        settle (600);
        shoot ("4-off-switch");
    }

    std::cout << "usability pass 2 frames in " << outDir.getFullPathName() << std::endl;
    return 0;
}

int runUsability3 (const juce::File& outDir)
{
    IlanaAnim::fullMotion() = true;
    outDir.createDirectory();
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
#if JUCE_LINUX
    if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
        setHandler ([] (void*, void*) { return 0; });
#endif
    editor->addToDesktop (0);
    editor->setVisible (true);
    settle (800);
    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);
    auto* shell = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
    IlanaTheme::uiScaleRef() = IlanaTheme::uiScaleRef() * 1.75f;
    IlanaTheme::notifyUiScaleListeners();

    const auto shoot = [&] (const juce::String& name)
    {
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.75f);
        juce::FileOutputStream stream (outDir.getChildFile (name + ".png"));
        if (stream.openedOk())
        {
            stream.setPosition (0);
            stream.truncate();
            juce::PNGImageFormat().writeImageToStream (image, stream);
        }
        std::cout << name << std::endl;
    };
    const auto mouse = [&] (juce::Point<float> at, juce::ModifierKeys mods)
    {
        // (The real pointer too: isMouseOver and getMouseXYRelative ask X where it is.)
        juce::Desktop::setMousePosition (editor->localPointToGlobal (at.roundToInt()));
        if (auto* peer = editor->getPeer())
            peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse, at, mods, juce::MouseInputSource::defaultPressure,
                                    juce::MouseInputSource::defaultOrientation, juce::Time::currentTimeMillis());
    };
    const auto turn = [&] (const char* id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
        {
            processor.getUndoManager().beginNewTransaction();
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
            parameter->endChangeGesture();
            processor.getUndoManager().beginNewTransaction();
        }
    };
    const auto knobFor = [&] (const juce::String& id) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && visibleInTree (knob))
                return knob;
        return nullptr;
    };

    // A preset with LFO 1 routes on more than one page.
    for (int index = 0; index < processor.getFactoryPresetNames().size(); ++index)
    {
        processor.loadFactoryPreset (index);
        auto routes = 0;
        for (int i = 0; i < Mod::maxSlots; ++i)
            routes += processor.readModSlot (i).source == Mod::Source::Lfo1 ? 1 : 0;
        if (routes >= 3)
        {
            std::cout << "preset: " << processor.getFactoryPresetNames()[index] << std::endl;
            break;
        }
    }
    shell->showPage ("MAIN");
    settle (800);

    // 1-2 Dragging CUTOFF: the hover line's full value mid-drag, then MAX at the top.
    if (auto* knob = knobFor ("f1_cutoff"))
    {
        auto& slider = knob->getSlider();
        const auto centre = editor->getLocalPoint (&slider, slider.getLocalBounds().getCentre()).toFloat();
        const auto drag = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
        mouse (centre, {});
        settle (100);
        mouse (centre, drag);
        settle (60);
        for (int i = 1; i <= 6; ++i)
        {
            mouse (centre.translated (0.0f, -4.0f * (float) i), drag);
            settle (40);
        }
        settle (300);
        shoot ("2-exact-value");
        mouse (centre.translated (0.0f, -24.0f), {});
        settle (300);
        // (Faked drags past the window's edge don't reach the knob here: the
        // value is put at the top and the knob told it was pushed past it,
        // as KnobSlider::mouseDrag does.)
        if (auto* parameter = processor.apvts.getParameter ("f1_cutoff"))
            parameter->setValueNotifyingHost (1.0f);
        settle (100);
        if (auto* knobSlider = dynamic_cast<KnobSlider*> (&slider); knobSlider != nullptr && knobSlider->onEndHit != nullptr)
            knobSlider->onEndHit (1);
        settle (60);
        shoot ("1-knob-end");
        settle (700);
        mouse ({ 2.0f, 700.0f }, {});
        settle (300);
    }

    // 3 Hovering the next-preset arrow.
    {
        std::vector<juce::Button*> buttons;
        findAll<juce::Button> (*editor, buttons);
        for (auto* button : buttons)
            if (button->getName() == "next" && visibleInTree (button))
            {
                mouse (editor->getLocalPoint (button, button->getLocalBounds().getCentre()).toFloat(), {});
                break;
            }
        settle (900);
        shoot ("3-preset-arrows");
        mouse ({ 2.0f, 700.0f }, {});
        settle (300);
    }

    // 4 Typing into CUTOFF: in range, then past the top.
    if (auto* knob = knobFor ("f1_cutoff"))
    {
        knob->showTypedValueBox();
        settle (200);
        std::vector<juce::TextEditor*> editors;
        findAll<juce::TextEditor> (*knob, editors);
        if (! editors.empty())
        {
            editors.front()->setText ("2400", true);
            settle (300);
            shoot ("4-type-range");
            editors.front()->setText ("99000", true);
            settle (300);
            shoot ("4-type-past");
            if (editors.front()->onEscapeKey != nullptr)
                editors.front()->onEscapeKey();
        }
        settle (300);
    }

    std::cout << "usability pass 3 frames in " << outDir.getFullPathName() << std::endl;
    return 0;
}

// --usability <dir>: still frames of the usability flair at 175 % (pass 1:
// the tabs' route badges, undo's note and flash, the default notch, FINE),
// full window, for the PR's pictures.
int runUsability (const juce::File& outDir)
{
    IlanaAnim::fullMotion() = true;
    outDir.createDirectory();
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
#if JUCE_LINUX
    if (auto* setHandler = (void* (*) (int (*) (void*, void*))) dlsym (RTLD_DEFAULT, "XSetErrorHandler"))
        setHandler ([] (void*, void*) { return 0; });
#endif
    editor->addToDesktop (0);
    editor->setVisible (true);
    settle (800);
    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);
    auto* shell = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
    IlanaTheme::uiScaleRef() = IlanaTheme::uiScaleRef() * 1.75f;
    IlanaTheme::notifyUiScaleListeners();

    const auto shoot = [&] (const juce::String& name)
    {
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.75f);
        juce::FileOutputStream stream (outDir.getChildFile (name + ".png"));
        if (stream.openedOk())
        {
            stream.setPosition (0);
            stream.truncate();
            juce::PNGImageFormat().writeImageToStream (image, stream);
        }
        std::cout << name << std::endl;
    };
    const auto mouse = [&] (juce::Point<float> at, juce::ModifierKeys mods)
    {
        if (auto* peer = editor->getPeer())
            peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse, at, mods, juce::MouseInputSource::defaultPressure,
                                    juce::MouseInputSource::defaultOrientation, juce::Time::currentTimeMillis());
    };
    const auto turn = [&] (const char* id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
        {
            processor.getUndoManager().beginNewTransaction();
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
            parameter->endChangeGesture();
            processor.getUndoManager().beginNewTransaction();
        }
    };
    const auto knobFor = [&] (const juce::String& id) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && visibleInTree (knob))
                return knob;
        return nullptr;
    };

    // A preset with LFO 1 routes on more than one page.
    for (int index = 0; index < processor.getFactoryPresetNames().size(); ++index)
    {
        processor.loadFactoryPreset (index);
        auto routes = 0;
        for (int i = 0; i < Mod::maxSlots; ++i)
            routes += processor.readModSlot (i).source == Mod::Source::Lfo1 ? 1 : 0;
        if (routes >= 3)
        {
            std::cout << "preset: " << processor.getFactoryPresetNames()[index] << std::endl;
            break;
        }
    }
    shell->showPage ("MAIN");
    settle (800);

    // 1 The route badges while LFO 1 is hovered.
    highlightedModSource() = (int) Mod::Source::Lfo1;
    settle (500);
    shoot ("1-tab-badges");
    highlightedModSource() = 0;
    settle (500);

    // 2 Undo on this page: RESO turned, then undone.
    turn ("f1_reso", 0.6f);
    settle (300);
    if (auto* undo = findButtonWithText (*editor, "undo"))
        undo->triggerClick();
    settle (120);
    shoot ("2-undo-here");
    settle (2600);

    // 2b Undo of a knob on another page.
    shell->showPage ("FX");
    settle (500);
    turn ("f1_reso", 0.7f);
    settle (300);
    if (auto* undo = findButtonWithText (*editor, "undo"))
        undo->triggerClick();
    settle (200);
    shoot ("2-undo-elsewhere");
    settle (2600);
    shell->showPage ("MAIN");
    settle (600);

    // 3 Hovering CUTOFF (turned away from its 12 kHz default): the notch.
    if (auto* parameter = processor.apvts.getParameter ("f1_cutoff"))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (2000.0f));
    settle (400);
    if (auto* knob = knobFor ("f1_cutoff"))
    {
        auto& slider = knob->getSlider();
        const auto centre = editor->getLocalPoint (&slider, slider.getLocalBounds().getCentre()).toFloat();
        mouse (centre, {});
        for (int i = 0; i < 12; ++i) // (an offscreen window isn't painted, so step the hover fade with throwaway pictures)
        {
            editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.75f);
            settle (40);
        }
        shoot ("3-default-notch");

        // 4 Shift-drag: FINE.
        const auto drag = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier);
        juce::ModifierKeys::currentModifiers = juce::ModifierKeys (juce::ModifierKeys::shiftModifier); // (events read the held keys from here)
        mouse (centre, drag);
        settle (60);
        mouse (centre.translated (0.0f, -6.0f), drag);
        settle (60);
        mouse (centre.translated (0.0f, -12.0f), drag);
        settle (200);
        shoot ("4-fine");
        juce::ModifierKeys::currentModifiers = {};
        mouse (centre.translated (0.0f, -12.0f), juce::ModifierKeys());
        settle (200);
        mouse ({ 2.0f, 700.0f }, {});
        settle (200);
    }

    std::cout << "usability frames in " << outDir.getFullPathName() << std::endl;
    return 0;
}

// The layout dump's card finder: paints one component on its own and keeps
// every big, solid, dark filled shape (a card's body) in the component's own
// coordinates, with its colour.
struct FillRecorder : public juce::LowLevelGraphicsSoftwareRenderer
{
    struct Fill { juce::Rectangle<float> area; juce::Colour colour; bool rounded; };

    explicit FillRecorder (const juce::Image& image) : juce::LowLevelGraphicsSoftwareRenderer (image) {}

    void setOrigin (juce::Point<int> o) override { transforms.getReference (transforms.size() - 1) = juce::AffineTransform::translation ((float) o.x, (float) o.y).followedBy (transforms.getLast()); LowLevelGraphicsSoftwareRenderer::setOrigin (o); }
    void addTransform (const juce::AffineTransform& t) override { transforms.getReference (transforms.size() - 1) = t.followedBy (transforms.getLast()); LowLevelGraphicsSoftwareRenderer::addTransform (t); }
    void saveState() override { transforms.add (transforms.getLast()); colours.add (colours.getLast()); LowLevelGraphicsSoftwareRenderer::saveState(); }
    void restoreState() override { if (transforms.size() > 1) { transforms.removeLast(); colours.removeLast(); } LowLevelGraphicsSoftwareRenderer::restoreState(); }
    void setFill (const juce::FillType& fill) override { colours.getReference (colours.size() - 1) = fill.isColour() ? fill.colour : juce::Colours::transparentBlack; LowLevelGraphicsSoftwareRenderer::setFill (fill); }
    void fillRoundedRectangle (const juce::Rectangle<float>& r, float corner) override { keep (r, true); LowLevelGraphicsSoftwareRenderer::fillRoundedRectangle (r, corner); }
    void fillRect (const juce::Rectangle<float>& r) override { keep (r, false); LowLevelGraphicsSoftwareRenderer::fillRect (r); }
    void fillRect (const juce::Rectangle<int>& r, bool replace) override { keep (r.toFloat(), false); LowLevelGraphicsSoftwareRenderer::fillRect (r, replace); }
    void fillPath (const juce::Path& path, const juce::AffineTransform& t) override
    {
        // A rounded rectangle path fills its bounding box almost fully.
        const auto box = path.getBounds();
        if (! box.isEmpty())
            keep (box.transformedBy (t), true);
        LowLevelGraphicsSoftwareRenderer::fillPath (path, t);
    }

    void keep (juce::Rectangle<float> r, bool rounded)
    {
        const auto colour = colours.getLast();
        if (colour.getAlpha() < 250 || r.getWidth() < 40.0f || r.getHeight() < 20.0f)
            return;
        if (colour.getRed() + colour.getGreen() + colour.getBlue() > 150)
            return;
        fills.push_back ({ r.transformedBy (transforms.getLast()), colour, rounded });
    }

    juce::Array<juce::AffineTransform> transforms { juce::AffineTransform() };
    juce::Array<juce::Colour> colours { juce::Colours::transparentBlack };
    std::vector<Fill> fills;
};

#if ILANA_VULKAN
// --vulkanlive <dir>: the editor on screen (Xvfb) at 1060 wide and 175 %,
// drawn through Vulkan. For every page: vk-<page>.png (the canvas, read
// back) and sw-<page>.png (the software snapshot at the same scale) for the
// parity script, and the frames that page took; then the idle frame rate
// (must be 0), real pointer input through the child window (X11 XTest: the
// server routes it exactly as a user's mouse), switching off and on, a
// resize and a window that closes and opens again.

int runVulkanLive (const juce::File& outDir)
{
    outDir.createDirectory();
    const auto scale = 1.75f;
    juce::Desktop::getInstance().setGlobalScaleFactor (scale);
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
    auto* shell = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());

    if (shell == nullptr)
        return 1;

    shell->setVulkanRendering (true); // (whatever the settings file says)
    HostWindow window (*editor);
    settle (1500);

    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);

    auto* vk = shell->getVulkanAttachment();

    if (vk == nullptr || ! vk->isActive())
    {
        std::cout << "FAIL: Vulkan didn't attach" << (vk != nullptr ? ": " + vk->describe() : juce::String()) << std::endl;
        return 1;
    }

    std::cout << "Vulkan: " << vk->describe() << std::endl;
    int failures = 0;
    const auto check = [&failures] (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
        failures += ok ? 0 : 1;
    };

    const auto mask = vk->getNativeInputEventMask();
    check (mask == 0, "the child window asks for no input events (mask " + juce::String (mask) + ")");

    // Text sized for the zoom it draws at, as in the host (as runFlair does).
    IlanaTheme::uiScaleRef() = IlanaTheme::uiScaleRef() * scale;
    IlanaTheme::notifyUiScaleListeners();
    settle (500);

    // Frames presented so far (0 while Vulkan is off).
    const auto framesSoFar = [shell]
    {
        auto* attachment = shell->getVulkanAttachment();
        return attachment != nullptr ? attachment->getStats().framesPresented : 0;
    };

    // Waits until no frame has come for 300 ms (at most 5 s).
    const auto settleFrames = [&]
    {
        auto last = framesSoFar();
        auto quietSince = juce::Time::getMillisecondCounterHiRes();
        const auto start = quietSince;
        while (juce::Time::getMillisecondCounterHiRes() - quietSince < 300.0 && juce::Time::getMillisecondCounterHiRes() - start < 5000.0)
        {
            settle (20);
            if (framesSoFar() != last)
            {
                last = framesSoFar();
                quietSince = juce::Time::getMillisecondCounterHiRes();
            }
        }
    };

    const auto writePng = [] (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
    };

    // A call-out (the TABLE browser) opens inside the editor, which the
    // Vulkan child window draws. Opened in the window around the editor (the
    // host's wrapper in a plugin), it sat under that child window, unseen
    // (ilana 2026-11-17, Vulkan on: "they never open").
    {
        shell->showPage ("MAIN");
        settle (400);
        std::vector<ComboControl*> combos;
        findAll<ComboControl> (*editor, combos);
        ComboControl::PopupCombo* table = nullptr;
        for (auto* control : combos)
            if (auto* combo = dynamic_cast<ComboControl::PopupCombo*> (&control->getComboBox()))
                if (table == nullptr && combo->ownMenu != nullptr && visibleInTree (combo) && control->getLabelText().containsIgnoreCase ("TABLE"))
                    table = combo; // (the first: OSC 1's)
        check (table != nullptr, "OSC 1 has a TABLE select");
        if (table != nullptr)
        {
            const auto position = table->getLocalBounds().getCentre().toFloat();
            const auto press = [&] (bool up)
            {
                return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position,
                                         up ? juce::ModifierKeys() : juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         table, table, juce::Time::getCurrentTime(), position, juce::Time::getCurrentTime(), 1, false);
            };
            table->mouseDown (press (false));
            settle (30);
            table->mouseUp (press (true));
            settle (400);
            settleFrames();
            auto* modal = juce::Component::getCurrentlyModalComponent();
            auto* callOut = dynamic_cast<juce::CallOutBox*> (modal);
            check (callOut != nullptr && callOut->isVisible(), "the TABLE browser opens");
            check (callOut != nullptr && callOut->getParentComponent() == editor.get(),
                   "the TABLE browser opens inside the editor (the canvas draws it)");
            if (auto* now = shell->getVulkanAttachment())
                writePng (now->readback(), outDir.getChildFile ("vk-table-browser.png"));
            if (callOut != nullptr)
                callOut->dismiss();
            settle (300);
        }
    }


    for (const auto& page : shell->getPageIds())
    {
        const auto before = vk->getStats();
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        shell->showPage (page);
        settle (400);
        settleFrames();
        const auto after = vk->getStats();
        const auto frames = after.framesPresented - before.framesPresented;
        const auto name = page.replaceCharacters ("/ ", "--");
        const auto gpu = vk->readback();
        const auto soft = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
        writePng (gpu, outDir.getChildFile ("vk-" + name + ".png"));
        writePng (soft, outDir.getChildFile ("sw-" + name + ".png"));

        // A first look at parity (the lead's script does the real comparison).
        double sum = 0.0;
        int over = 0, total = 0;
        if (gpu.getWidth() == soft.getWidth() && gpu.getHeight() == soft.getHeight())
        {
            const juce::Image::BitmapData a (gpu, juce::Image::BitmapData::readOnly), b (soft, juce::Image::BitmapData::readOnly);
            for (int y = 0; y < gpu.getHeight(); ++y)
                for (int x = 0; x < gpu.getWidth(); ++x)
                {
                    const auto p = a.getPixelColour (x, y), q = b.getPixelColour (x, y);
                    const auto d = juce::jmax (std::abs ((int) p.getRed() - (int) q.getRed()), std::abs ((int) p.getGreen() - (int) q.getGreen()),
                                               std::abs ((int) p.getBlue() - (int) q.getBlue()));
                    sum += d;
                    over += d > 16 ? 1 : 0;
                    ++total;
                }
        }
        std::cout << page << ": " << frames << " frames, " << juce::String (frames > 0 ? (after.totalFrameMs - before.totalFrameMs) / frames : 0.0, 2)
                  << " ms each (last " << juce::String (after.lastFrameMs, 2) << " ms), settled in "
                  << juce::String (juce::Time::getMillisecondCounterHiRes() - t0, 0) << " ms; "
                  << gpu.getWidth() << "x" << gpu.getHeight() << " vs " << soft.getWidth() << "x" << soft.getHeight()
                  << ", mean diff " << juce::String (total > 0 ? sum / total : -1.0, 3) << ", "
                  << juce::String (total > 0 ? 100.0 * over / total : -1.0, 3) << "% of pixels off by more than 16" << std::endl;
    }

    // Idle: nothing changes, so nothing may be drawn.
    shell->showPage ("MAIN");
    settle (400);
    settleFrames();
    {
        // (The editor itself repaints a few small areas now and then while
        // idle, in software too: --idlecpu counts them. Every frame here must
        // come from such a repaint; none may be drawn for nothing.)
        const auto before = vk->getStats();
        settle (3000);
        const auto after = vk->getStats();
        const auto idle = (after.framesPresented - before.framesPresented) / 3.0;
        const auto unasked = (after.framesPresented - before.framesPresented) - (after.repaintFrames - before.repaintFrames);
        std::cout << "idle: " << juce::String (idle, 2) << " frames per second, " << unasked << " of them without a repaint" << std::endl;
        check (unasked == 0 && idle <= 2.0, "an idle editor draws only the editor's own repaints");

        // Who keeps drawing: the deepest component at the last frame's dirty area.
        if (idle > 0.0)
        {
            const auto area = vk->getStats().lastDirty.toFloat() / scale;
            if (auto* who = editor->getComponentAt (area.getCentre().roundToInt()))
                std::cout << "  idle repaint at " << area.toNearestInt().toString() << " (editor px): " << typeid (*who).name()
                          << " '" << who->getName() << "', parent " << (who->getParentComponent() != nullptr ? typeid (*who->getParentComponent()).name() : "") << std::endl;
        }
    }

#if JUCE_LINUX
    // Real pointer input: XTest moves and clicks the server's pointer, and
    // the server delivers it to whichever window wants it under the child.
    using OpenDisplay = void* (*) (const char*);
    using FakeMotion = int (*) (void*, int, int, int, unsigned long);
    using FakeButton = int (*) (void*, unsigned int, int, unsigned long);
    using Flush = int (*) (void*);
    auto* x11 = dlopen ("libX11.so.6", RTLD_LAZY);
    auto* xtst = dlopen ("libXtst.so.6", RTLD_LAZY);
    auto openDisplay = x11 != nullptr ? (OpenDisplay) dlsym (x11, "XOpenDisplay") : nullptr;
    auto flush = x11 != nullptr ? (Flush) dlsym (x11, "XFlush") : nullptr;
    auto motion = xtst != nullptr ? (FakeMotion) dlsym (xtst, "XTestFakeMotionEvent") : nullptr;
    auto button = xtst != nullptr ? (FakeButton) dlsym (xtst, "XTestFakeButtonEvent") : nullptr;
    void* display = openDisplay != nullptr ? openDisplay (nullptr) : nullptr;

    // Counts the presses that reach the editor.
    struct PressProbe : juce::MouseListener
    {
        void mouseDown (const juce::MouseEvent&) override { ++downs; }
        int downs = 0;
    } probe;
    editor->addMouseListener (&probe, true);

    // Hover, click and drag through the server's pointer. Run with Vulkan
    // on (counted) and, at the end, off (the control: the same moves on the
    // editor's own window).
    const auto pointerTests = [&] (bool counted)
    {
    const auto checkInput = [&] (bool ok, const juce::String& what)
    {
        if (counted)
            check (ok, what);
        else
            std::cout << (ok ? "  (control) ok   " : "  (control) FAIL ") << what << std::endl;
    };
    if (display != nullptr && motion != nullptr && button != nullptr && flush != nullptr)
    {
        const auto toScreen = [&] (juce::Component& c, juce::Point<int> local)
        {
            const auto p = editor->getLocalPoint (&c, local.toFloat());
            return (editor->localPointToGlobal (p) * scale).roundToInt();
        };
        const auto moveTo = [&] (juce::Point<int> s) { motion (display, -1, s.x, s.y, 0); flush (display); settle (120); };
        const auto press = [&] (bool down) { button (display, 1, down ? 1 : 0, 0); flush (display); settle (120); };

        // Hover: the component under the mouse is the editor's own.
        if (auto* tab = findButtonWithText (*editor, "FX"))
        {
            const auto framesBefore = framesSoFar();
            moveTo (toScreen (*tab, tab->getLocalBounds().getCentre()));
            moveTo (toScreen (*tab, tab->getLocalBounds().getCentre() + juce::Point<int> (1, 0)));
            auto* under = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
            checkInput (under != nullptr && (under == tab || tab->isParentOf (under) || under->isParentOf (tab)),
                   "hover reaches the FX tab (under the mouse: " + juce::String (under != nullptr ? typeid (*under).name() : "nothing") + ")");
            settleFrames();
            std::cout << "  hover drew " << framesSoFar() - framesBefore << " frames" << std::endl;

            // Click: the FX page opens.
            const auto downsBefore = probe.downs;
            press (true);
            press (false);
            settleFrames();
            std::cout << "  presses seen by the editor: " << probe.downs - downsBefore << " of 1" << std::endl;
            checkInput (shell->getCurrentPageId() == "FX", "a click on the FX tab opens FX (now " + shell->getCurrentPageId() + ")");
        }
        else
        {
            checkInput (false, "found the FX tab");
        }

        // Drag: a knob on MAIN turns.
        shell->showPage ("MAIN");
        settleFrames();
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        KnobControl* knob = nullptr;
        for (auto* k : knobs)
        {
            if (k->isShowing() && k->getSlider().isShowing() && k->getSlider().getWidth() > 20)
            {
                knob = k;
                break;
            }
        }
        if (knob != nullptr)
        {
            auto& slider = knob->getSlider();
            slider.setValue (slider.proportionOfLengthToValue (0.5), juce::sendNotificationSync);
            settleFrames();
            const auto value = slider.getValue();
            const auto framesBefore = framesSoFar();
            const auto centre = toScreen (slider, slider.getLocalBounds().getCentre());
            moveTo (centre);
            press (true);
            for (int step = 1; step <= 8; ++step)
                moveTo (centre - juce::Point<int> (0, step * 12));
            press (false);
            settleFrames();
            checkInput (! juce::approximatelyEqual (slider.getValue(), value),
                   "dragging " + knob->getParameterId() + " turns it (" + juce::String (value, 3) + " -> " + juce::String (slider.getValue(), 3) + "), "
                       + juce::String (framesSoFar() - framesBefore) + " frames");
            if (counted)
                writePng (shell->getVulkanAttachment()->readback(), outDir.getChildFile ("vk-after-drag.png"));
            if (counted)
                writePng (editor->createComponentSnapshot (editor->getLocalBounds(), true, scale), outDir.getChildFile ("sw-after-drag.png"));
        }
        else
        {
            checkInput (false, "found a knob on MAIN");
        }
    }
    else
    {
        checkInput (false, "XTest is available");
    }
    };
    pointerTests (true);
#endif

    // Off and on again, live, as the settings menu does.
    shell->setVulkanRendering (false);
    settle (300);
    check (shell->getVulkanAttachment() == nullptr, "Vulkan detaches");
    shell->setVulkanRendering (true);
    settle (600);
    check (shell->getVulkanAttachment() != nullptr && shell->getVulkanAttachment()->isActive(), "Vulkan attaches again");

    // A host resize: the canvas and the swapchain follow.
    window.setSize (1325, 900);
    editor->setSize (1325, 900);
    settle (800);
    if (auto* again = shell->getVulkanAttachment())
    {
        std::cout << "after resize: " << again->describe() << std::endl;
        const auto width = again->readback().getWidth();
        check (width == juce::roundToInt (1325 * scale), "the canvas follows a resize (" + juce::String (width) + " wide)");
    }

    // The window closed and opened again (the peer goes and comes back).
    window.removeFromDesktop();
    settle (300);
    window.addToDesktop (juce::ComponentPeer::windowHasTitleBar);
    window.setVisible (true);
    settle (800);
    check (shell->getVulkanAttachment() != nullptr && shell->getVulkanAttachment()->isActive(), "Vulkan comes back with a new window");
    if (auto* again = shell->getVulkanAttachment())
        std::cout << "reopened: " << again->describe() << ", " << again->getStats().framesPresented << " frames in all" << std::endl;

#if JUCE_LINUX
    shell->setVulkanRendering (false);
    settle (300);
    pointerTests (false);
    editor->removeMouseListener (&probe);
#endif

    std::cout << (failures == 0 ? juce::String ("vulkanlive: all passed") : "vulkanlive: " + juce::String (failures) + " failed") << std::endl;
    return failures == 0 ? 0 : 1;
}
#endif

