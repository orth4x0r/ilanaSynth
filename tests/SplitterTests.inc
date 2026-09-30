// FX splitters: a slot on one band works on that band only and the rest
// passes around it untouched. Included by TableTest.cpp;
// ILANA_SPLITTER_TEST=1 runs only these.

namespace SplitterLab
{
// Runs the rack over two seconds of a 50 Hz + 10 kHz pair (left) and the
// same with the 10 kHz tone inverted on the right (so it is all side), and
// returns both channels.
inline std::array<std::vector<float>, 2> run (IlanaSynthAudioProcessor& processor)
{
    processor.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    std::array<std::vector<float>, 2> out;
    for (int block = 0; block < 188; ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            const auto n = block * 512 + i;
            const auto low = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 50.0 * n / 48000.0);
            const auto high = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 10000.0 * n / 48000.0);
            buffer.setSample (0, i, low + high);
            buffer.setSample (1, i, low - high);
        }
        processor.processEffects (buffer);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 512; ++i)
                out[(size_t) channel].push_back (buffer.getSample (channel, i));
    }
    return out;
}

inline double levelDb (const std::vector<float>& x, double hz)
{
    return juce::Decibels::gainToDecibels (partialAmplitude (x, 48000, 48000, hz) / 0.3, -120.0);
}

inline void setUp (IlanaSynthAudioProcessor& processor, int band, bool solo, float utilityGainDb)
{
    const auto set = [&processor] (const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
        processor.assignFxSlot (slot, 0);
    processor.assignFxSlot (1, 19); // Utility
    set ("fx_util_gain", utilityGainDb);
    set ("fx_slot1_band", (float) band);
    set ("fx_slot1_solo", solo ? 1.0f : 0.0f);
    set ("fx_slot1_mix", 1.0f);
}
} // namespace SplitterLab

void runSplitterTests()
{
    using namespace SplitterLab;

    // An untouched band sums back flat: frequency bands through the
    // crossovers' allpasses (phase only), mid/side exactly.
    for (int band = 1; band <= 5; ++band)
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, band, false, 0.0f);
        const auto out = run (processor);
        auto worst = 0.0;
        for (const auto hz : { 50.0, 10000.0 })
            for (int channel = 0; channel < 2; ++channel)
                worst = juce::jmax (worst, std::abs (levelDb (out[(size_t) channel], hz)));
        check (worst < 0.05, "splitter band " + juce::String (band) + ": a unity slot keeps the levels (worst "
                                 + juce::String (worst, 3) + " dB)");
    }

    // A slot on the low band turns the low tone down and leaves the high one.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, 1, false, -12.0f);
        const auto out = run (processor);
        const auto low = levelDb (out[0], 50.0), high = levelDb (out[0], 10000.0);
        check (std::abs (low + 12.0) < 0.5 && std::abs (high) < 0.2,
               "LOW band: -12 dB on 50 Hz (" + juce::String (low, 2) + " dB), 10 kHz kept (" + juce::String (high, 2) + " dB)");
    }
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, 3, false, -12.0f);
        const auto out = run (processor);
        const auto low = levelDb (out[0], 50.0), high = levelDb (out[0], 10000.0);
        check (std::abs (low) < 0.2 && std::abs (high + 12.0) < 0.5,
               "HIGH band: 50 Hz kept (" + juce::String (low, 2) + " dB), -12 dB on 10 kHz (" + juce::String (high, 2) + " dB)");
    }
    // Solo: only the band is heard. MID (default 250 Hz to 2.5 kHz) holds neither tone.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, 2, true, 0.0f);
        const auto out = run (processor);
        check (levelDb (out[0], 50.0) < -20.0 && levelDb (out[0], 10000.0) < -20.0,
               "MID band soloed: both tones outside it are down more than 20 dB");
    }
    // Mid/side: the 10 kHz tone is all side, the 50 Hz all mid.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, 5, false, -12.0f);
        const auto out = run (processor);
        const auto low = levelDb (out[0], 50.0), high = levelDb (out[0], 10000.0);
        check (std::abs (low) < 0.1 && std::abs (high + 12.0) < 0.2,
               "SIDE: only the side (10 kHz) is turned down (" + juce::String (high, 2) + " dB; mid " + juce::String (low, 2) + " dB)");
        check (out[1].size() == out[0].size() && std::abs (levelDb (out[1], 10000.0) + 12.0) < 0.2, "SIDE: the right channel too");
    }
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, 4, true, 0.0f);
        const auto out = run (processor);
        check (std::abs (levelDb (out[0], 50.0)) < 0.1 && levelDb (out[0], 10000.0) < -60.0, "MID (M/S) soloed: the side is gone");
    }
}
