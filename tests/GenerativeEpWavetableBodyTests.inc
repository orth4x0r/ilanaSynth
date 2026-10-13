// M7.1: the Generative card (Euclid, probability sequencer, strum).
namespace
{
struct NoteLog
{
    struct Event { int time, note; bool on; };
    std::vector<Event> events;
    int exciterHits = 0;

    int ons() const
    {
        return (int) std::count_if (events.begin(), events.end(), [] (const Event& e) { return e.on; });
    }
};

// Holds NOTES from the first block for SECONDS, logging what reaches the voices.
NoteLog runGenerative (const std::function<void (IlanaSynthAudioProcessor&)>& configure,
                       std::initializer_list<int> notes, double seconds, std::vector<float>* audio = nullptr)
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    configure (processor);

    NoteLog log;
    juce::AudioBuffer<float> buffer (2, 512);
    const auto blocks = (int) (seconds * 48000.0 / 512.0);

    for (int block = 0; block < blocks; ++block)
    {
        juce::MidiBuffer midi;

        if (block == 0)
            for (auto note : notes)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

        buffer.clear();
        processor.processBlock (buffer, midi);

        for (const auto metadata : processor.getSynthMidiForTest())
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOnOrOff())
                log.events.push_back ({ block * 512 + metadata.samplePosition, message.getNoteNumber(), message.isNoteOn() });
            else if (message.isSysEx())
                ++log.exciterHits;
        }

        if (audio != nullptr)
            for (int i = 0; i < 512; ++i)
                audio->push_back (buffer.getSample (0, i));
    }

    return log;
}

// Debug pass after M7.5: a MIDI event in the middle of a block splits the
// voices' render, and the second part must keep reading the shared LFO,
// S&H and MSEG buffers where it is in the block (it used to start again at
// the block's first sample, so modulation jumped at every note or CC).
void runSplitRenderTest()
{
    std::cout << "split renders" << std::endl;
    const auto render = [] (bool withCc)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        for (const auto& [id, value] : std::vector<std::pair<const char*, float>> {
                 { "master_clip", 0.0f }, { "osc2_on", 0.0f }, { "sub_on", 0.0f }, { "osc1_table", 0.0f },
                 { "amp_attack", 0.001f }, { "amp_sustain", 1.0f }, { "lfo1_rate", 7.0f },
                 { "mod1_src", (float) Mod::Source::Lfo1 }, { "mod1_dst", (float) Mod::Destination::AmpLevel },
                 { "mod1_amt", 0.8f } })
            setParam (processor, id, value);
        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> out;
        for (int block = 0; block < 40; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (withCc)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, block % 128), 256);
            processor.processBlock (buffer, midi);
            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }
        return out;
    };
    const auto plain = render (false);
    const auto split = render (true);
    auto worst = 0.0f, peak = 0.0f;
    for (size_t i = 0; i < plain.size(); ++i)
    {
        worst = juce::jmax (worst, std::abs (plain[i] - split[i]));
        peak = juce::jmax (peak, std::abs (plain[i]));
    }
    check (peak > 0.05f && worst < peak * 1.0e-4f,
           "an unused CC mid-block leaves an LFO-modulated note unchanged (worst " + juce::String (worst, 6) + ")");
}

void runM73ElectricPianoTests()
{
    std::cout << "M7.3 electric pianos" << std::endl;

    {
        IlanaSynthAudioProcessor processor;
        for (const auto* id : { "osc1_excite", "sub_excite", "osc4_excite" })
        {
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id));
            check (choice != nullptr && choice->choices.size() >= 9 && choice->choices[6] == "Osc In"
                       && choice->choices[7] == "Tine" && choice->choices[8] == "Reed",
                   juce::String (id) + ": Tine and Reed are appended after Osc In (indices 7 and 8)");
        }
        check (processor.apvts.getParameter ("osc6_ep_distance") != nullptr
                   && processor.apvts.getParameter ("osc1_ep_position") != nullptr,
               "every oscillator has the pickup DISTANCE and OFFSET");
    }

    const auto render = [] (ElectricPiano::Model model, double hz, float velocity, float distance = 0.5f,
                            float position = 0.5f, float damper = 0.0f, int releaseAt = -1, int length = 48000)
    {
        ElectricPiano piano;
        piano.prepare (48000.0);
        piano.setModel (model);
        piano.setParams (0.75f, 0.35f, distance, position, 0.5f, damper);
        piano.setFrequency (hz);
        piano.trigger (velocity);
        std::vector<float> out ((size_t) length);
        for (int i = 0; i < length; ++i)
            out[(size_t) i] = piano.process (releaseAt < 0 || i < releaseAt);
        return out;
    };
    // Level of partials 2-6 against the fundamental, in dB, just after the attack.
    const auto upper = [] (const std::vector<float>& x, double hz)
    {
        const auto fundamental = partialAmplitude (x, 2400, 8192, hz);
        auto sum = 0.0;
        for (int k = 2; k <= 6; ++k)
            sum += std::pow (partialAmplitude (x, 2400, 8192, hz * k), 2.0);
        return 10.0 * std::log10 (sum / juce::jmax (1.0e-20, fundamental * fundamental));
    };
    const auto rms = [] (const std::vector<float>& x, int from, int to)
    {
        auto sum = 0.0;
        for (int i = from; i < to; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt (sum / juce::jmax (1, to - from));
    };

    for (const auto model : { ElectricPiano::Model::Tine, ElectricPiano::Model::Reed })
    {
        const juce::String name (model == ElectricPiano::Model::Tine ? "Tine" : "Reed");
        const auto soft = render (model, 261.63, 0.3f);
        const auto hard = render (model, 261.63, 1.0f);
        const auto valid = [] (const std::vector<float>& x)
        {
            return std::all_of (x.begin(), x.end(), [] (float v) { return std::isfinite (v) && std::abs (v) < 8.0f; });
        };
        check (valid (soft) && valid (hard) && rms (hard, 0, 24000) > 1.0e-4, name + " rings and stays bounded");
        check (rms (hard, 0, 24000) > rms (soft, 0, 24000) * 1.5, name + ": a hard hit is louder than a soft one");
        const auto softUpper = upper (soft, 261.63);
        const auto hardUpper = upper (hard, 261.63);
        check (hardUpper > softUpper + 3.0, name + (model == ElectricPiano::Model::Tine ? " barks" : " growls")
                                               + ": the upper partials rise from soft to hard ("
                                               + juce::String (softUpper, 1) + " -> " + juce::String (hardUpper, 1) + " dB)");
        const auto near = upper (render (model, 261.63, 0.6f, 0.1f), 261.63);
        const auto far = upper (render (model, 261.63, 0.6f, 0.9f), 261.63);
        check (near > far + 3.0, name + ": a closer pickup (DISTANCE) gives more upper partials ("
                                     + juce::String (far, 1) + " -> " + juce::String (near, 1) + " dB)");

        for (const auto note : { 36, 60, 84 })
        {
            const auto hz = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            // The strongest frequency within 50 cents of the note, in half-cent
            // steps (a bark can fool an autocorrelation pitch estimate low down).
            const auto tone = render (model, hz, 0.3f, 0.9f, 0.5f, 0.0f, -1, 48000);
            auto cents = 0.0, best = 0.0;
            for (auto trial = -50.0; trial <= 50.0; trial += 0.5)
                if (const auto level = partialAmplitude (tone, 4800, 38400, hz * std::exp2 (trial / 1200.0)); level > best)
                {
                    best = level;
                    cents = trial;
                }
            check (std::abs (cents) < 5.0, name + " at MIDI " + juce::String (note) + " is in tune ("
                                               + juce::String (cents, 2) + " cents)");
        }

        const auto ringing = render (model, 261.63, 0.8f, 0.5f, 0.5f, 0.0f, 12000);
        const auto damped = render (model, 261.63, 0.8f, 0.5f, 0.5f, 1.0f, 12000);
        check (rms (damped, 24000, 36000) < rms (ringing, 24000, 36000) * 0.05,
               name + ": DAMPER stops the note after the key is up");

        auto stable = true;
        for (const auto note : { 21, 60, 108, 120 })
            for (const auto distance : { 0.0f, 1.0f })
                for (const auto position : { 0.0f, 1.0f })
                    stable = stable && valid (render (model, 440.0 * std::pow (2.0, (note - 69) / 12.0), 1.0f,
                                                      distance, position, 0.0f, -1, 12000));
        check (stable, name + " stays finite and bounded at extreme settings");
    }

    // The presets play through the whole engine.
    for (const auto* presetName : { "Tine Keys", "Reed Keys" })
    {
        IlanaSynthAudioProcessor processor;
        const auto index = processor.getFactoryPresetNames().indexOf (presetName);
        check (index >= 0, juce::String (presetName) + " is a factory preset");
        if (index < 0)
            continue;
        processor.loadFactoryPreset (index);
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        auto peak = 0.0f;
        auto finite = true;
        for (int block = 0; block < 90; ++block)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            processor.processBlock (buffer, midi);
            for (int i = 0; i < 512; ++i)
            {
                finite = finite && std::isfinite (buffer.getSample (0, i));
                peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));
            }
        }
        check (finite && peak > 0.01f && peak < 1.5f, juce::String (presetName) + " plays (peak " + juce::String (peak, 3) + ")");
    }
}

void runM74WavetableEditorTests()
{
    std::cout << "M7.4 wavetable editor" << std::endl;
    const auto factoryCount = TableFactory::getNumFactoryTables();
    const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-m74-test");
    temp.deleteRecursively();
    temp.createDirectory();

    {
        IlanaSynthAudioProcessor processor;
        auto* table = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("osc1_table"));
        check (table != nullptr && table->choices.size() == factoryCount + 16
                   && table->choices[factoryCount] == "User 1" && table->choices[factoryCount + 3] == "User 4"
                   && table->choices[factoryCount + 15] == "User 16",
               "16 patch tables: User 5-16 are appended after User 1-4");
    }

    // Formulas.
    {
        Formula formula;
        check (formula.parse ("sin(2*pi*x) + 0.5*saw(x*2) - clamp(f, 0, 1)^2").isEmpty(), "a formula parses");
        check (formula.parse ("sin(2*pi*x").isNotEmpty() && formula.parse ("foo(x)").isNotEmpty()
                   && formula.parse ("1 +").isNotEmpty(),
               "a broken formula reports an error");
        const auto sine = WavetableDoc::renderFormula ("sin(2*pi*x)", 0.0, 0);
        std::vector<float> magnitudes, phases;
        WavetableDoc::analyse (sine, magnitudes, phases, 16);
        check (std::abs (magnitudes[0] - 1.0f) < 1.0e-3f && magnitudes.size() < 3,
               "sin(2*pi*x) is one harmonic at full level");
        const auto square = WavetableDoc::renderFormula ("square(x)", 0.0, 0);
        WavetableDoc::analyse (square, magnitudes, phases, 16);
        check (magnitudes.size() >= 5 && magnitudes[1] < 0.01f && std::abs (magnitudes[2] / magnitudes[0] - 1.0f / 3.0f) < 0.02f,
               "square(x) has the odd harmonics at 1/n");
        const auto morphing = WavetableDoc::renderFormula ("f", 0.75, 3);
        check (std::abs (morphing[100] - 0.75f) < 1.0e-6f, "f is the frame's position through the table");
    }

    // Harmonics round trip and the draw recipe.
    {
        const std::vector<float> magnitudes { 1.0f, 0.5f, 0.25f }, phases { 0.0f, 1.0f, 2.0f };
        const auto frame = WavetableDoc::renderHarmonics (magnitudes, phases);
        std::vector<float> readMagnitudes, readPhases;
        WavetableDoc::analyse (frame, readMagnitudes, readPhases, 8);
        auto error = 0.0f;
        for (size_t k = 0; k < 3; ++k)
            error = juce::jmax (error, std::abs (readMagnitudes[k] - magnitudes[k]),
                                std::abs (std::remainder (readPhases[k] - phases[k], juce::MathConstants<float>::twoPi)));
        check (error < 1.0e-3f, "harmonic levels and phases survive a render and analysis");

        const std::vector<float> points { 0.0f, 1.0f, 0.5f, 1.0f, 0.5001f, -1.0f, 0.9999f, -1.0f };
        const auto hardEdges = WavetableDoc::renderDraw (points, 0.0f);
        const auto smoothed = WavetableDoc::renderDraw (points, 1.0f);
        std::vector<float> hardMagnitudes, smoothMagnitudes, unused;
        WavetableDoc::analyse (hardEdges, hardMagnitudes, unused, 64);
        WavetableDoc::analyse (smoothed, smoothMagnitudes, unused, 64);
        check (hardMagnitudes.size() > 20 && smoothMagnitudes[20] < hardMagnitudes[20] * 0.5f,
               "a drawn square has high harmonics, and SMOOTH takes them off");
    }

    // A doc of recipes rebuilds exactly and stores no frames; raw frames are
    // embedded at 16 bits.
    WavetableDoc recipeDoc;
    {
        recipeDoc.name = "Recipes";
        FrameRecipe drawn;
        drawn.kind = FrameRecipe::Kind::Draw;
        drawn.points = { 0.0f, 0.0f, 0.25f, 1.0f, 0.75f, -1.0f };
        drawn.smoothing = 0.3f;
        FrameRecipe harmonics;
        harmonics.kind = FrameRecipe::Kind::Harmonics;
        harmonics.magnitudes = { 1.0f, 0.0f, 0.3f };
        harmonics.phases = { 0.0f, 0.0f, 1.5f };
        FrameRecipe formula;
        formula.kind = FrameRecipe::Kind::Formula;
        formula.formula = "tanh(4*sin(2*pi*x)) * (1 - f)";
        for (const auto& recipe : { drawn, harmonics, formula })
            recipeDoc.insertFrame (recipeDoc.getNumFrames(), {}, recipe);
        recipeDoc.renderAll();

        const auto tree = recipeDoc.toValueTree();
        WavetableDoc restored;
        juce::String notice;
        check (WavetableDoc::fromValueTree (tree, restored, notice) && notice.isEmpty()
                   && ! tree.hasProperty ("data") && restored.frames == recipeDoc.frames
                   && restored.recipes == recipeDoc.recipes && restored.name == "Recipes",
               "a table of drawn, harmonic and formula frames is stored as its recipe and rebuilds exactly");

        auto raw = WavetableDoc::fromFactory (3);
        raw.factoryIndex = -1;
        raw.name = "Raw";
        const auto rawTree = raw.toValueTree();
        WavetableDoc rawRestored;
        auto worst = 0.0f, peak = 0.0f;
        const auto ok = WavetableDoc::fromValueTree (rawTree, rawRestored, notice) && rawRestored.frames.size() == raw.frames.size();
        for (size_t f = 0; ok && f < raw.frames.size(); ++f)
            for (size_t i = 0; i < raw.frames[f].size(); ++i)
            {
                worst = juce::jmax (worst, std::abs (raw.frames[f][i] - rawRestored.frames[f][i]));
                peak = juce::jmax (peak, std::abs (raw.frames[f][i]));
            }
        check (ok && rawTree.hasProperty ("data") && worst <= peak / 32767.0f * 1.01f,
               "raw frames are embedded at 16 bits (worst error " + juce::String (worst / juce::jmax (1.0e-9f, peak), 7) + " of peak)");

        const auto factoryTree = WavetableDoc::fromFactory (5).toValueTree();
        WavetableDoc factoryRestored;
        check (! factoryTree.hasProperty ("data") && WavetableDoc::fromValueTree (factoryTree, factoryRestored, notice)
                   && factoryRestored.frames == WavetableDoc::fromFactory (5).frames,
               "an unedited factory copy is stored as its index");

        std::vector<std::vector<float>> big (256, std::vector<float> (2048));
        juce::Random random (7);
        for (auto& frame : big)
            for (auto& value : frame)
                value = random.nextFloat() * 2.0f - 1.0f;
        const auto encoded = WavetableDoc::encodeFrames (big);
        check (encoded.length() < 1500000, "256 frames of noise embed in "
                                               + juce::String (encoded.length() / 1024) + " KB (16-bit, compressed, base64)");

        juce::ValueTree lost ("Wavetable");
        lost.setProperty ("name", "Lost", nullptr);
        lost.setProperty ("path", temp.getChildFile ("missing.wav").getFullPathName(), nullptr);
        WavetableDoc lostDoc;
        check (! WavetableDoc::fromValueTree (lost, lostDoc, notice) && notice.contains ("Lost"),
               "a table with no recipe, data or file fails with a notice");
    }

    // Frame editing and morphs.
    {
        auto doc = WavetableDoc::sine();
        FrameRecipe saw;
        saw.kind = FrameRecipe::Kind::Formula;
        saw.formula = "saw(x)";
        for (int i = 0; i < 6; ++i)
            doc.insertFrame (1, WavetableDoc::sine().frames[0], WavetableDoc::sine().recipes[0]);
        doc.insertFrame (doc.getNumFrames(), {}, saw);
        doc.renderFrame (doc.getNumFrames() - 1);
        doc.morph (0, doc.getNumFrames() - 1, true);
        std::vector<float> middle, unused;
        WavetableDoc::analyse (doc.frames[4], middle, unused, 8);
        check (doc.getNumFrames() == 8 && doc.recipes[4].kind == FrameRecipe::Kind::Harmonics
                   && middle[1] > 0.05f && middle[1] < 0.45f,
               "a spectral morph from a sine to a saw fills the frames between with harmonics");
        doc.moveFrame (7, 0);
        doc.removeFrame (1);
        check (doc.getNumFrames() == 7 && doc.recipes[0].kind == FrameRecipe::Kind::Formula, "frames move and delete");
        for (int i = 0; i < 400; ++i)
            doc.insertFrame (0, {}, {});
        check (doc.getNumFrames() == WavetableDoc::maxFrames, "a table stops at 256 frames");
    }

    // Export for Serum and Vital, and import.
    {
        const auto file = temp.getChildFile ("export.wav");
        check (recipeDoc.exportWav (file) && Wavetable::readWavFrameSize (file) == 2048,
               "an exported table carries a clm chunk with its 2048-sample frames");
        WavetableDoc imported;
        const auto loaded = WavetableDoc::loadFromFile (file, imported);
        // The export scales a table that peaks over 1 down to 1.
        auto peak = 1.0f;
        for (const auto& frame : recipeDoc.frames)
            for (auto value : frame)
                peak = juce::jmax (peak, std::abs (value));
        auto worst = 0.0f;
        for (size_t f = 0; loaded && f < recipeDoc.frames.size() && f < imported.frames.size(); ++f)
            for (size_t i = 0; i < 2048; ++i)
                worst = juce::jmax (worst, std::abs (recipeDoc.frames[f][i] / peak - imported.frames[f][i]));
        check (loaded && imported.getNumFrames() == recipeDoc.getNumFrames() && worst < 1.0e-5f,
               "the exported .wav loads back frame for frame");

        // A sidecar makes a library table editable again.
        auto recipe = recipeDoc.recipeTree();
        if (auto xml = recipe.createXml())
            xml->writeTo (file.withFileExtension ("ilwt"));
        WavetableDoc editable;
        check (WavetableDoc::loadFromFile (file, editable) && editable.recipes == recipeDoc.recipes,
               "a library .wav with its .ilwt sidecar opens with its recipe");

        // Another tool's table with 1024-sample frames.
        const auto small = temp.getChildFile ("small.wav");
        {
            juce::MemoryOutputStream data;
            for (int frame = 0; frame < 4; ++frame)
                for (int i = 0; i < 1024; ++i)
                    data.writeFloat ((float) std::sin (juce::MathConstants<double>::twoPi * (frame + 1) * i / 1024.0));
            juce::MemoryOutputStream format;
            format.writeShort (3); format.writeShort (1); format.writeInt (44100); format.writeInt (44100 * 4);
            format.writeShort (4); format.writeShort (32);
            const juce::String clm ("<!>1024 00000000 test");
            juce::MemoryOutputStream body;
            body.write ("WAVE", 4);
            body.write ("fmt ", 4); body.writeInt ((int) format.getDataSize()); body.write (format.getData(), format.getDataSize());
            body.write ("clm ", 4); body.writeInt (clm.length()); body.write (clm.toRawUTF8(), (size_t) clm.length());
            if (clm.length() & 1) body.writeByte (0);
            body.write ("data", 4); body.writeInt ((int) data.getDataSize()); body.write (data.getData(), data.getDataSize());
            juce::FileOutputStream out (small);
            out.write ("RIFF", 4);
            out.writeInt ((int) body.getDataSize());
            out.write (body.getData(), body.getDataSize());
        }
        std::vector<std::vector<float>> frames;
        std::vector<float> magnitudes, unused;
        const auto read = Wavetable::readFrames (small, Wavetable::LoadMode::Automatic, frames);
        if (read && frames.size() == 4)
            WavetableDoc::analyse (frames[2], magnitudes, unused, 8);
        check (read && frames.size() == 4 && magnitudes.size() >= 3 && magnitudes[2] > 0.9f,
               "a table whose clm chunk says 1024-sample frames reads as 4 frames");
    }

    // Patch tables travel with the patch.
    {
        IlanaSynthAudioProcessor original;
        auto doc = recipeDoc;
        doc.name = "Travelling";
        check (original.setUserTable (5, doc), "a doc goes into patch table 6");
        setParam (original, "osc1_table", (float) (factoryCount + 5));
        juce::MemoryBlock state;
        original.getStateInformation (state);

        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.isUserSlotEdited (5) && restored.getUserTableDoc (5).frames == doc.frames
                   && restored.getUserTableDoc (5).name == "Travelling" && ! restored.isUserSlotEdited (4),
               "an edited table is saved in the patch and restored exactly");

        const auto* table = restored.getWavetable (factoryCount + 5);
        check (table != nullptr && table->getNumFrames() == 3, "the restored table is the one the oscillator plays");

        // A later state without it puts the slot back to its default.
        IlanaSynthAudioProcessor plain;
        juce::MemoryBlock plainState;
        plain.getStateInformation (plainState);
        restored.setStateInformation (plainState.getData(), (int) plainState.getSize());
        check (! restored.isUserSlotEdited (5), "loading a patch without the table resets the slot");

        // Loading a factory preset does too.
        original.loadFactoryPreset (1);
        check (! original.isUserSlotEdited (5), "loading a factory preset resets edited tables");

        // An old patch: only a path. It loads as before, and the next save
        // embeds the table.
        const auto file = temp.getChildFile ("old.wav");
        recipeDoc.exportWav (file);
        auto tree = plain.apvts.copyState();
        tree.setProperty ("userTablePath2", file.getFullPathName(), nullptr);
        tree.setProperty ("userTableMode2", 0, nullptr);
        IlanaSynthAudioProcessor old;
        if (auto xml = tree.createXml())
        {
            juce::MemoryBlock block;
            juce::AudioProcessor::copyXmlToBinary (*xml, block);
            old.setStateInformation (block.getData(), (int) block.getSize());
        }
        old.flushAsyncUpdates();
        juce::MemoryBlock saved;
        old.getStateInformation (saved);
        const auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        const auto savedTree = xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
        const auto tables = savedTree.getChildWithName ("Wavetables");
        check (old.isUserSlotEdited (1) && old.getUserTableDoc (1).getNumFrames() == 3
                   && tables.getNumChildren() == 1 && tables.getChild (0).hasProperty ("data"),
               "an old path-only table loads, and the next save embeds it");

        // The free slot finder skips slots in use.
        IlanaSynthAudioProcessor fresh;
        setParam (fresh, "osc1_table", (float) factoryCount);
        check (fresh.findFreeUserSlot() == 1, "EDIT on a factory table takes the first free patch table");
    }

    temp.deleteRecursively();
}

} // namespace

void runM71GenerativeTests()
{
    std::cout << "M7.1 generative card" << std::endl;

    // The Euclidean pattern: E(3,8) is x..x..x., and rotation shifts it.
    {
        juce::String pattern, rotated;
        for (int step = 0; step < 8; ++step)
        {
            pattern += euclidHit (step, 3, 8, 0) ? "x" : ".";
            rotated += euclidHit (step, 3, 8, 1) ? "x" : ".";
        }
        check (pattern == "x..x..x." && rotated == ".x..x..x", "Euclid E(3,8) is x..x..x. (rotated " + rotated + ")");
        check (! euclidHit (0, 0, 8, 0) && euclidHit (5, 8, 8, 0), "Euclid with no hits is silent and with all hits is solid");
    }

    // Euclid alone plays the held chord on its hits: 5 of 16 sixteenths.
    {
        const auto log = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "euc_on", 1.0f);
            setParam (p, "euc_steps", 16.0f);
            setParam (p, "euc_hits", 5.0f);
        }, { 60, 64, 67 }, 2.0 - 0.01);   // 16 sixteenths at 120 BPM, just under

        check (log.ons() == 15, "Euclid 5/16 plays the three-note chord five times per bar (" + juce::String (log.ons()) + " note-ons)");
    }

    // Euclid gates the arp: 3 of 8 arp steps sound.
    {
        const auto log = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "arp_on", 1.0f);
            setParam (p, "arp_div", 3.0f);   // 1/8: 8 steps in 2 s
            setParam (p, "euc_on", 1.0f);
            setParam (p, "euc_steps", 8.0f);
            setParam (p, "euc_hits", 3.0f);
        }, { 60 }, 2.0 - 0.01);

        check (log.ons() == 3, "Euclid 3/8 rests five of eight arp steps (" + juce::String (log.ons()) + " note-ons)");
    }

    // Probability sequencer: chance 0 is silent; ratchets repeat; range
    // stays in the scale and above the held key.
    {
        const auto silent = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "pseq_on", 1.0f);
            for (int step = 1; step <= 16; ++step)
                setParam (p, "pseq_chance" + juce::String (step), 0.0f);
        }, { 60 }, 1.0);
        check (silent.ons() == 0, "a probability sequencer at chance 0 plays nothing");

        const auto ratchets = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "pseq_on", 1.0f);
            setParam (p, "pseq_length", 4.0f);
            setParam (p, "pseq_ratchet1", 3.0f);
        }, { 60 }, 0.5 - 0.01);   // four sixteenths
        check (ratchets.ons() == 6, "a ratchet of 3 on step 1 of 4 gives six notes (" + juce::String (ratchets.ons()) + ")");

        const auto ranged = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "pseq_on", 1.0f);
            setParam (p, "gen_scale", 1.0f);   // C major
            for (int step = 1; step <= 16; ++step)
                setParam (p, "pseq_range" + juce::String (step), 12.0f);
        }, { 60 }, 4.0);

        auto inScale = ranged.ons() > 20;   // 32 sixteenths in 4 s
        auto moved = false;
        for (const auto& event : ranged.events)
            if (event.on)
            {
                inScale = inScale && event.note >= 60 && event.note <= 72 && Scales::contains (event.note, 1, 0);
                moved = moved || event.note != 60;
            }
        check (inScale && moved, "RANGE 12 keeps the sequence in C major within an octave above the key");
    }

    // Strum: a chord comes out in pitch order, 30 ms apart; Down reverses it.
    for (const auto direction : { 1, 2 })
    {
        const auto log = runGenerative ([direction] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "spray_strum", (float) direction);
            setParam (p, "spray_strum_time", 30.0f);
        }, { 64, 60, 67 }, 0.3);

        std::vector<NoteLog::Event> ons;
        for (const auto& event : log.events)
            if (event.on)
                ons.push_back (event);

        const auto spacing = (int) (0.030 * 48000.0);
        auto ordered = ons.size() == 3;
        if (ordered)
        {
            const int expected[2][3] { { 60, 64, 67 }, { 67, 64, 60 } };
            for (size_t i = 0; i < 3; ++i)
                ordered = ordered && ons[i].note == expected[direction - 1][i]
                          && std::abs (ons[i].time - (int) i * spacing) <= 2 * (int) i;   // the time snaps to 0.25 ms
        }
        juce::String got;
        for (const auto& event : ons)
            got << event.note << "@" << event.time << " ";
        check (ordered, juce::String (direction == 1 ? "strum up" : "strum down") + " plays the chord in order, 30 ms apart (" + got + ")");
    }

    // A strummed chord released before its last note starts leaves nothing on.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, "spray_strum", 1.0f);
        setParam (processor, "spray_strum_time", 200.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        std::array<int, 128> sounding {};

        for (int block = 0; block < 400; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                for (auto note : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (block == 20)   // after the second note, before the third
                for (auto note : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            for (const auto metadata : processor.getSynthMidiForTest())
            {
                const auto message = metadata.getMessage();
                if (message.isNoteOn())
                    ++sounding[(size_t) message.getNoteNumber()];
                else if (message.isNoteOff())
                    --sounding[(size_t) message.getNoteNumber()];
            }
        }

        check (std::all_of (sounding.begin(), sounding.end(), [] (int count) { return count == 0; })
                   && processor.getActiveVoiceCount() == 0,
               "releasing a chord mid-strum leaves no stuck notes");
    }

    // Exciter: one marker per hit reaches the voices.
    {
        const auto log = runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "euc_on", 1.0f);
            setParam (p, "euc_target", 1.0f);
            setParam (p, "euc_steps", 8.0f);
            setParam (p, "euc_hits", 3.0f);
            setParam (p, "euc_div", 3.0f);   // 1/8
            setParam (p, "osc1_mode", 1.0f);
        }, { 48 }, 2.0 - 0.01);

        check (log.exciterHits == 3 && log.ons() == 1,
               "Euclid on the exciter re-strikes 3 times in a bar without new notes ("
                   + juce::String (log.exciterHits) + " hits)");
    }

    // Trance gate target: only the hit steps pass.
    {
        std::vector<float> audio;
        runGenerative ([] (IlanaSynthAudioProcessor& p)
        {
            for (const auto& [id, value] : std::vector<std::pair<const char*, float>> {
                     { "master_clip", 0.0f }, { "osc1_table", 8.0f }, { "amp_attack", 0.001f },
                     { "amp_sustain", 1.0f }, { "osc2_on", 0.0f }, { "sub_on", 0.0f } })
                setParam (p, id, value);
            p.assignFxSlot (1, 16);
            setParam (p, "fx_gate_smooth", 0.0f);
            setParam (p, "euc_on", 1.0f);
            setParam (p, "euc_target", 2.0f);
            setParam (p, "euc_steps", 4.0f);
            setParam (p, "euc_hits", 1.0f);
            setParam (p, "euc_div", 2.0f);   // 1/4: a step is 24000 samples
        }, { 60 }, 2.5, &audio);   // 117 248 samples: the second hit is measured up to 116 000

        const auto rms = [&audio] (int from, int to)
        {
            double sum = 0.0;
            for (int i = from; i < to; ++i)
                sum += (double) audio[(size_t) i] * audio[(size_t) i];
            return std::sqrt (sum / (double) (to - from));
        };

        const auto open = rms (4000, 20000), shut = rms (28000, 90000), openAgain = rms (100000, 116000);
        check (open > 0.01 && openAgain > 0.01 && shut < open * 0.05,
               "the trance gate follows Euclid 1/4 (open " + juce::String (open, 3) + ", shut " + juce::String (shut, 4) + ")");
    }
}

void runM72BodyTests()
{
    std::cout << "M7.2 material bodies" << std::endl;
    const auto render = [] (int shape, float material, float size, float decay, int quality)
    {
        MaterialBody body;
        body.prepare (48000.0);
        body.configure (shape, material, size, decay, 261.625565, quality);
        std::vector<float> signal (48000);
        for (int i = 0; i < (int) signal.size(); ++i)
            signal[(size_t) i] = body.process (i == 0 ? 1.0f : 0.0f);
        return signal;
    };
    const auto energy = [] (const std::vector<float>& x, int from, int to)
    {
        auto sum = 0.0;
        for (int i = from; i < to; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt (sum / (double) (to - from));
    };
    const auto bar = render (0, 0.0f, 0.5f, 0.5f, 1);
    const auto plate = render (1, 0.65f, 0.5f, 0.5f, 1);
    const auto bell = render (2, 0.7f, 0.5f, 0.5f, 1);
    const auto shell = render (3, 1.0f, 0.5f, 0.5f, 1);
    for (const auto* sound : { &bar, &plate, &bell, &shell })
    {
        const auto valid = std::all_of (sound->begin(), sound->end(), [] (float x) { return std::isfinite (x) && std::abs (x) <= 2.0f; });
        check (valid && energy (*sound, 1000, 5000) > 1.0e-5, "each body rings and stays bounded");
    }
    auto distinct = 0.0, total = 0.0;
    for (int i = 0; i < 12000; ++i)
    {
        distinct += std::abs (bar[(size_t) i] - plate[(size_t) i]);
        total += std::abs (bar[(size_t) i]) + std::abs (plate[(size_t) i]);
    }
    check (distinct > total * 0.1, "bar and plate have different modal spectra ("
                                       + juce::String (distinct / juce::jmax (1.0e-12, total), 3) + ")");

    // Switching a patch from Classic to a material body keeps its loudness
    // in range: the wet body sits within 12 dB of the dry sound, for a
    // sustained saw and for a plucked string, at short and long DECAY. A pluck
    // lands a few cents off the body's narrow first mode, so it rings the
    // body through STRING TO BODY coupling, as the Hybrid presets do.
    for (const auto pluck : { false, true })
        for (const auto decay : { 0.3f, 0.9f })
            for (const auto shape : { 1, 2, 3, 4 })
            {
                const auto source = [pluck] (IlanaSynthAudioProcessor& p) { setParam (p, "osc1_mode", pluck ? 1.0f : 0.0f); };
                const auto dry = renderCleanPatch (source, 60, 1.0);
                const auto wet = renderCleanPatch ([source, shape, decay, pluck] (IlanaSynthAudioProcessor& p)
                {
                    source (p);
                    setParam (p, "res_on", 1.0f);
                    setParam (p, "res_amount", 1.0f);
                    setParam (p, "res_decay", decay);
                    setParam (p, "body_type", (float) shape);
                    if (pluck)
                    {
                        setParam (p, "body_coupling_mode", 1.0f);
                        setParam (p, "body_coupling", 0.8f);
                    }
                }, 60, 1.0);
                const auto ratioDb = 20.0 * std::log10 (energy (wet, 0, 48000) / juce::jmax (1.0e-9, energy (dry, 0, 48000)));
                check (std::abs (ratioDb) < 12.0, juce::String (pluck ? "a plucked string" : "a sustained saw")
                                                      + " through BODY type " + juce::String (shape) + " at DECAY "
                                                      + juce::String (decay, 1) + " stays near the dry level ("
                                                      + juce::String (ratioDb, 1) + " dB)");
            }

    const auto small = render (0, 0.0f, 0.0f, 0.6f, 1);
    const auto large = render (0, 0.0f, 1.0f, 0.6f, 1);
    const auto smallHz = fundamentalOf (std::vector<float> (small.begin() + 3000, small.begin() + 18000), 48000.0);
    const auto largeHz = fundamentalOf (std::vector<float> (large.begin() + 3000, large.begin() + 18000), 48000.0);
    check (smallHz > largeHz * 2.5, "BODY SIZE lowers the modal pitch");

    const auto shortRing = render (1, 0.65f, 0.5f, 0.1f, 1);
    const auto longRing = render (1, 0.65f, 0.5f, 0.9f, 1);
    check (energy (longRing, 20000, 24000) > energy (shortRing, 20000, 24000) * 10.0,
           "BODY DECAY lengthens the modal tail");

    const auto dryRelease = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "amp_release", 0.01f);
    }, 60, 0.6, 0.1);
    const auto bodyRelease = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
    {
        setParam (p, "amp_release", 0.01f);
        setParam (p, "res_on", 1.0f);
        setParam (p, "body_type", 1.0f);
        setParam (p, "res_amount", 1.0f);
        setParam (p, "res_decay", 0.8f);
    }, 60, 0.6, 0.1);
    check (energy (bodyRelease, 18000, 22000) > energy (dryRelease, 18000, 22000) * 5.0
           && energy (bodyRelease, 18000, 22000) > 1.0e-5,
           "BODY continues ringing after the exciter envelope ends");

    const auto renderCoupling = [] (int mode)
    {
        return renderCleanPatch ([mode] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_mode", 1.0f);
            setParam (p, "osc1_excite", 0.0f);
            setParam (p, "osc2_on", 1.0f);
            setParam (p, "osc2_mode", 1.0f);
            setParam (p, "osc2_excite", 0.0f);
            setParam (p, "osc2_level", 0.7f);
            setParam (p, "res_on", 1.0f);
            setParam (p, "body_type", 2.0f);
            setParam (p, "res_amount", 0.7f);
            setParam (p, "body_coupling_mode", (float) mode);
            setParam (p, "body_coupling", 1.0f);
        }, 60, 0.35);
    };
    const auto uncoupled = renderCoupling (0);
    auto uncoupledSquared = 0.0;
    for (const auto sample : uncoupled)
        uncoupledSquared += (double) sample * sample;
    const auto uncoupledRms = std::sqrt (uncoupledSquared / (double) juce::jmax ((size_t) 1, uncoupled.size()));
    for (int mode = 1; mode <= 3; ++mode)
    {
        const auto coupled = renderCoupling (mode);
        auto squared = 0.0;
        for (size_t i = 0; i < coupled.size(); ++i)
        {
            const auto delta = (double) coupled[i] - (double) uncoupled[i];
            squared += delta * delta;
        }
        const auto change = std::sqrt (squared / (double) coupled.size());
        check (change > uncoupledRms * 0.01,
               "coupling path " + juce::String (mode) + " changes the voice (by "
                   + juce::String (juce::Decibels::gainToDecibels (change / juce::jmax (1.0e-12, uncoupledRms)), 1) + " dB of its level)");
    }

    MaterialBody stress;
    stress.prepare (48000.0);
    stress.configure (2, 1.0f, 0.0f, 1.0f, 4000.0, 2);
    auto finite = true;
    for (int i = 0; i < 48000; ++i)
    {
        const auto value = stress.process (i & 1 ? 2.0f : -2.0f);
        finite = finite && std::isfinite (value) && std::abs (value) <= 2.0f;
    }
    check (finite, "maximum material resonance stays finite under full drive");
}

// ILANA_BODY_FIT=<folder> writes impulse responses of the four material
// bodies, so tools/fit_model.py can compare their modes with struck objects.
void renderBodyFit (const juce::File& folder)
{
    folder.createDirectory();
    struct Target { const char* name; int shape; float material, decay; double hz; };
    for (const auto& target : { Target { "bar", 0, 0.0f, 0.45f, 261.625565 },
                                { "plate", 1, 0.62f, 0.88f, 261.625565 },
                                { "bell", 2, 0.72f, 0.72f, 523.251131 },
                                { "glass", 3, 1.0f, 0.6f, 787.31 } })
    {
        MaterialBody body;
        body.prepare (48000.0);
        body.configure (target.shape, target.material, 0.5f, target.decay, target.hz, 2);
        juce::AudioBuffer<float> buffer (1, 48000 * 3);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, body.process (i == 0 ? 1.0f : 0.0f));
        const auto file = folder.getChildFile (juce::String (target.name) + ".wav");
        file.deleteFile();
        juce::WavAudioFormat format;
        if (auto stream = file.createOutputStream())
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (format.createWriterFor (stream.get(), 48000.0, 1, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            }
    }
}
