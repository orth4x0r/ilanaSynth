// Adversarial polish pass (2026-10-13). Included by TableTest.cpp;
// ILANA_STRESS_TEST=1 runs only these. A held chord is rendered through
// awkward host behaviour and the output must stay finite and bounded:
//  - 1-sample, odd and 4096-sample blocks mixed at random,
//  - the sample rate changing mid-session (44.1, 96, 48 kHz),
//  - preset spam (a load every few blocks) with notes held,
//  - parameter automation storm (every parameter jumped each block).

void runStressTests()
{
    std::cout << "--- stress ---" << std::endl;

    const auto run = [] (const juce::String& what, auto&& body)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        auto peak = 0.0f;
        auto bad = false;
        const auto render = [&] (int size, bool notes)
        {
            juce::AudioBuffer<float> buffer (2, size);
            juce::MidiBuffer midi;
            if (notes)
                for (const auto note : { 48, 55, 60, 64 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < size; ++i)
                {
                    const auto v = buffer.getSample (ch, i);
                    if (! std::isfinite (v))
                        bad = true;
                    peak = juce::jmax (peak, std::abs (v));
                }
        };
        body (processor, render);
        check (! bad && peak < 8.0f, what + " (finite, peak " + juce::String (peak, 3) + ")");
    };

    run ("mixed block sizes 1 to 4096", [] (auto&, auto& render)
    {
        juce::Random random (7);
        render (512, true);
        const int sizes[] { 1, 1, 2, 3, 7, 16, 17, 64, 333, 1024, 4096 };
        for (int i = 0; i < 300; ++i)
            render (sizes[random.nextInt ((int) std::size (sizes))], false);
    });

    run ("sample rate changes mid-session", [] (auto& processor, auto& render)
    {
        render (512, true);
        for (const auto rate : { 44100.0, 96000.0, 48000.0, 192000.0, 44100.0 })
        {
            processor.releaseResources();
            processor.prepareToPlay (rate, 256);
            for (int i = 0; i < 20; ++i)
                render (256, i == 0);
        }
    });

    run ("preset spam with notes held", [] (auto& processor, auto& render)
    {
        const auto count = juce::jmin (150, processor.getNumAllPresets());
        render (512, true);
        for (int i = 0; i < count; ++i)
        {
            processor.loadPresetByIndex (i * 3 % juce::jmax (1, processor.getNumAllPresets()));
            for (int b = 0; b < 3; ++b)
                render (128, b == 0);
        }
    });

    run ("parameter automation storm", [] (auto& processor, auto& render)
    {
        juce::Random random (11);
        render (512, true);
        for (int i = 0; i < 120; ++i)
        {
            for (auto* parameter : processor.getParameters())
                if (random.nextInt (4) == 0)
                    parameter->setValueNotifyingHost (random.nextFloat());
            render (256, i % 10 == 0);
        }
    });
}
