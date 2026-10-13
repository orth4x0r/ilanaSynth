// CPU pass 2 (2026-10-12). Included by TableTest.cpp; ILANA_CPUPASS_TEST=1
// runs only these.
//  - Oversampling steps down at high host rates (88.2 kHz: one step,
//    176.4 kHz: two), and is used as set at 44.1 and 48 kHz.
//  - A voice that was idle picks up the block's parameters when a note
//    starts on it (idle voices no longer get a copy every block).

void runCpuPassTests()
{
    std::cout << "--- CPU pass 2 ---" << std::endl;

    const auto factorAt = [] (double rate, float osFactorNormalised)
    {
        IlanaSynthAudioProcessor processor;
        if (auto* on = processor.apvts.getParameter ("oversampling"))
            on->setValueNotifyingHost (1.0f);
        if (auto* factor = processor.apvts.getParameter ("os_factor"))
            factor->setValueNotifyingHost (osFactorNormalised);
        processor.flushAsyncUpdates();
        processor.prepareToPlay (rate, 512);
        return processor.getOversamplingFactor();
    };

    check (factorAt (48000.0, 0.0f) == 2 && factorAt (48000.0, 1.0f) == 4, "oversampling is used as set at 48 kHz");
    check (factorAt (44100.0, 1.0f) == 4, "4x stays 4x at 44.1 kHz");
    check (factorAt (88200.0, 0.0f) == 1 && factorAt (88200.0, 1.0f) == 2, "88.2 kHz: one step down (2x to off, 4x to 2x)");
    check (factorAt (96000.0, 0.0f) == 1 && factorAt (96000.0, 1.0f) == 2, "96 kHz: one step down");
    check (factorAt (192000.0, 0.0f) == 1 && factorAt (192000.0, 1.0f) == 1, "192 kHz: two steps down, off");

    // An idle voice gets this block's parameters at note start: a patch change
    // between two notes on the same (idle) voice takes effect on the second.
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    const auto render = [&processor] (int note)
    {
        juce::AudioBuffer<float> buffer (2, 256);
        auto peak = 0.0f;
        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (block == 20)
                midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            peak = juce::jmax (peak, buffer.getMagnitude (0, 256));
        }
        return peak;
    };

    const auto first = render (60);
    for (int i = 0; i < 400; ++i) // let the first note finish and its voice go idle
    {
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer none;
        processor.processBlock (buffer, none);
    }
    for (const auto* id : { "osc1_level", "osc2_level" })
        if (auto* level = processor.apvts.getParameter (id))
            level->setValueNotifyingHost (0.0f);
    const auto second = render (60);
    check (first > 0.01f && second < 0.1f * first,
           "a note on an idle voice uses the parameters of its own block (levels at 0 are quiet: peak "
           + juce::String (first, 4) + " then " + juce::String (second, 4) + ")");
}
