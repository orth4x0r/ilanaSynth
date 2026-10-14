// Roadmap step 45, granular controls: the grain position as its own
// parameter, SCAN, FREEZE and the grain destinations in the matrix. Included
// by TableTest.cpp; ILANA_GRANULAR_TEST=1 runs only these.

namespace GranularLab
{
// A ramp: the sample's value is how far through it you are, so the mean of
// the grains' output says where the cloud sits.
std::shared_ptr<SampleData> makeRamp (int numSamples, double rate)
{
    auto data = std::make_shared<SampleData>();
    data->sampleRate = rate;
    data->buffer.setSize (1, numSamples);

    for (int i = 0; i < numSamples; ++i)
        data->buffer.setSample (0, i, (float) i / (float) numSamples);

    return data;
}

// The mean output over the next `seconds` of a cloud.
double meanOver (GranularOsc& osc, double rate, double seconds)
{
    auto sum = 0.0;
    const auto count = (int) (seconds * rate);

    for (int i = 0; i < count; ++i)
    {
        float l = 0.0f, r = 0.0f;
        osc.process (l, r);
        sum += 0.5 * (l + r);
    }

    return sum / (double) count;
}
}

void runGranularControlTests()
{
    std::cout << "--- granular controls (step 45) ---" << std::endl;
    constexpr auto rate = 48000.0;
    const auto sample = GranularLab::makeRamp (48000, rate);

    const auto makeCloud = [&] (float position, float scan, bool freeze)
    {
        GranularOsc osc;
        osc.prepare (rate);
        osc.reset (1234);
        GranularOsc::Params params;
        params.sample = sample.get();
        params.position = position;
        params.sizeMs = 20.0f;
        params.density = 0.6f;
        params.spray = 0.0f;
        params.spread = 0.0f;
        params.scan = scan;
        params.freeze = freeze;
        osc.setParams (params);
        return std::make_pair (std::move (osc), params);
    };

    // The cloud sits at POSITION.
    {
        auto [osc, params] = makeCloud (0.25f, 0.0f, false);
        const auto low = GranularLab::meanOver (osc, rate, 0.2);
        auto [osc2, params2] = makeCloud (0.75f, 0.0f, false);
        const auto high = GranularLab::meanOver (osc2, rate, 0.2);
        check (high > low * 2.0 && low > 0.0, "Grain position places the cloud (low " + juce::String (low, 4)
                                                                                  + ", high " + juce::String (high, 4) + ")");
    }

    // No scan: the cloud stays; scan 0.5: it travels half the sample in a second.
    {
        auto [still, p1] = makeCloud (0.2f, 0.0f, false);
        const auto stillFirst = GranularLab::meanOver (still, rate, 0.2);
        GranularLab::meanOver (still, rate, 0.5);
        const auto stillLast = GranularLab::meanOver (still, rate, 0.2);
        check (std::abs (stillLast / stillFirst - 1.0) < 0.15, "Grain scan at 0 holds the position (first "
                                                                  + juce::String (stillFirst, 4) + ", last " + juce::String (stillLast, 4) + ")");

        auto [moving, p2] = makeCloud (0.2f, 0.5f, false);
        const auto first = GranularLab::meanOver (moving, rate, 0.2);
        GranularLab::meanOver (moving, rate, 0.5);
        const auto last = GranularLab::meanOver (moving, rate, 0.2);
        check (last > first * 1.8, "Grain scan moves the cloud through the sample (first " + juce::String (first, 4)
                                                                                    + ", last " + juce::String (last, 4) + ")");

        auto [backwards, p3] = makeCloud (0.9f, -0.5f, false);
        const auto b1 = GranularLab::meanOver (backwards, rate, 0.2);
        GranularLab::meanOver (backwards, rate, 0.5);
        const auto b2 = GranularLab::meanOver (backwards, rate, 0.2);
        check (b2 < b1 * 0.7, "A negative scan speed travels backwards (first " + juce::String (b1, 4) + ", last " + juce::String (b2, 4) + ")");
    }

    // FREEZE holds the cloud where it is, then lets go from the same spot.
    {
        auto [osc, params] = makeCloud (0.2f, 0.5f, false);
        GranularLab::meanOver (osc, rate, 0.4);
        params.freeze = true;
        osc.setParams (params);
        const auto frozenA = GranularLab::meanOver (osc, rate, 0.2);
        GranularLab::meanOver (osc, rate, 0.4);
        const auto frozenB = GranularLab::meanOver (osc, rate, 0.2);
        check (std::abs (frozenB / frozenA - 1.0) < 0.15, "Freeze holds the scanning cloud (" + juce::String (frozenA, 4)
                                                                                  + " then " + juce::String (frozenB, 4) + ")");
        params.freeze = false;
        osc.setParams (params);
        GranularLab::meanOver (osc, rate, 0.1);
        const auto released = GranularLab::meanOver (osc, rate, 0.2);
        check (released > frozenB * 1.1, "Letting go of freeze carries on scanning from there");
    }

    // The position is its own parameter, and the matrix reaches it, the
    // spread and the scan speed.
    {
        IlanaSynthAudioProcessor processor;
        for (const auto* id : { "osc1_grain_pos", "osc1_grain_scan", "osc1_grain_freeze", "sub_grain_pos", "osc6_grain_scan" })
            check (processor.apvts.getParameter (id) != nullptr, juce::String ("Parameter ") + id + " exists");

        for (const auto* id : { "osc1_grain_pos", "osc2_grain_spread", "sub_grain_spread", "osc6_grain_scan" })
            check (Mod::destinationForParamId (id) > 0, juce::String (id) + " is a modulation destination");
    }

    // An older patch plays its cloud where Sample Start was.
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        processor.prepareToPlay (48000.0, 512);
        processor.loadFactoryPreset (0);

        if (auto* parameter = processor.apvts.getParameter ("osc1_sample_start"))
            parameter->setValueNotifyingHost (0.4f);

        juce::MemoryBlock block;
        processor.getStateInformation (block);
        auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize());
        check (xml != nullptr, "The patch state is readable");

        if (xml != nullptr)
        {
            xml->removeAttribute ("grainPosSchema");

            for (auto* child = xml->getFirstChildElement(); child != nullptr;)
            {
                auto* next = child->getNextElement();

                if (child->getStringAttribute ("id") == "osc1_grain_pos")
                    xml->removeChildElement (child, true);

                child = next;
            }

            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
            processor.setStateInformation (old.getData(), (int) old.getSize());
            const auto* position = processor.apvts.getRawParameterValue ("osc1_grain_pos");
            check (position != nullptr && std::abs (position->load() - 0.4f) < 0.005f,
                   "A patch saved before Grain Position takes Sample Start's value (got "
                       + juce::String (position != nullptr ? position->load() : -1.0f, 3) + ")");
        }
    }

    // A factory preset written before it does the same.
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        processor.prepareToPlay (48000.0, 512);
        const auto index = processor.getFactoryPresetNames().indexOf ("Granular Pad");
        check (index >= 0, "Granular Pad exists");

        if (index >= 0)
        {
            processor.loadFactoryPreset (index);
            const auto* position = processor.apvts.getRawParameterValue ("osc1_grain_pos");
            check (position != nullptr && std::abs (position->load() - 0.3f) < 0.005f,
                   "Granular Pad's cloud sits at its old Sample Start (got "
                       + juce::String (position != nullptr ? position->load() : -1.0f, 3) + ")");
        }
    }
}
