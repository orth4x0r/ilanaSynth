// LFO modulation of the Physical oscillator's knobs must not click (ilana's
// "phys_bowed pad", 2026-10-12: an LFO on the bowed string's EXCITE
// POSITION clicked once per host block). Modulation reaches the string once
// per block, so a knob that moves a delay tap, a waveguide split or the
// bow's force has to glide there sample by sample. Each case renders a held
// note with an LFO on one knob and compares the waveform's bend (second
// difference) at the block boundaries with its bend elsewhere in the block:
// a step at the boundary shows up as a spike there. Included by
// TableTest.cpp; ILANA_PHYSMOD_TEST=1 runs only these.
//
// ILANA_PHYSMOD_RENDER="<preset file>|<out.wav>" renders a preset's chord
// (C3 E3 G3 C4, 4 s held, 1.5 s release, 48 kHz, 512-sample blocks) to a
// WAV, prints its click ratio and its CPU, and exits.

namespace PhysModLab
{
constexpr double rate = 48000.0;
constexpr int block = 512;

// Mean |second difference| at each offset in the block, over the blocks
// from `skip` samples on; returns the worst of the two boundary offsets
// over the median of all offsets.
inline double boundaryRatio (const juce::AudioBuffer<float>& audio, int skip)
{
    std::vector<double> sums ((size_t) block, 0.0);
    auto counts = 0;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        const auto* x = audio.getReadPointer (channel);
        for (int start = (skip / block) * block; start + block <= audio.getNumSamples(); start += block)
        {
            if (start < 2)
                continue;
            for (int k = 0; k < block; ++k)
            {
                const auto n = start + k;
                sums[(size_t) k] += std::abs ((double) x[n] - 2.0 * x[n - 1] + x[n - 2]);
            }
            ++counts;
        }
    }

    if (counts == 0)
        return 0.0;

    auto sorted = sums;
    std::nth_element (sorted.begin(), sorted.begin() + block / 2, sorted.end());
    const auto median = sorted[(size_t) block / 2];
    return median > 0.0 ? std::max (sums[0], sums[1]) / median : 0.0;
}

// Renders `notes` held for `held` seconds, then `release` seconds more.
inline juce::AudioBuffer<float> renderNotes (IlanaSynthAudioProcessor& processor, const std::vector<int>& notes,
                                             double held, double release, double* cpuSeconds = nullptr)
{
    processor.prepareToPlay (rate, block);
    const auto heldBlocks = (int) (held * rate / block);
    const auto totalBlocks = heldBlocks + (int) (release * rate / block);
    juce::AudioBuffer<float> out (2, totalBlocks * block);
    juce::AudioBuffer<float> buffer (2, block);
    auto elapsed = 0.0;

    for (int b = 0; b < totalBlocks; ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        for (const auto note : notes)
        {
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (b == heldBlocks)
                midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        }
        const auto start = juce::Time::getMillisecondCounterHiRes();
        processor.processBlock (buffer, midi);
        elapsed += juce::Time::getMillisecondCounterHiRes() - start;
        for (int channel = 0; channel < 2; ++channel)
            out.copyFrom (channel, b * block, buffer, channel, 0, block);
    }

    if (cpuSeconds != nullptr)
        *cpuSeconds = elapsed / 1000.0;
    return out;
}

inline void writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio)
{
    file.deleteFile();
    juce::WavAudioFormat format;
    if (auto stream = file.createOutputStream())
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (
                format.createWriterFor (stream.get(), rate, 2, 24, {}, 0)))
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
        }
}

struct Case
{
    int excite;
    const char* knob;
};

// A Physical OSC 1 on `excite` (Saw keeps a pluck sounding), LFO 1 (a 4 Hz
// sine) into `knob` at half depth, the knob at its middle.
inline void setUp (IlanaSynthAudioProcessor& processor, const Case& c, float depth)
{
    setParam (processor, "osc1_mode", 1.0f);
    setParam (processor, "osc1_excite", (float) c.excite);
    setParam (processor, "osc1_string_sustain", 0.5f);
    setParam (processor, "osc1_string_decay", 0.8f);
    setParam (processor, juce::String ("osc1_") + c.knob, 0.5f);
    setParam (processor, "lfo1_sync", 0.0f);
    setParam (processor, "lfo1_shape", 0.0f);
    setParam (processor, "lfo1_rate", 4.0f);
    setParam (processor, "mod1_src", 1.0f);
    setParam (processor, "mod1_dst", (float) Mod::destinationForParamId (juce::String ("osc1_") + c.knob));
    setParam (processor, "mod1_amt", depth);
}
} // namespace PhysModLab

// The preset render (ILANA_PHYSMOD_RENDER); returns the process exit code.
int renderPhysModPreset (const juce::String& spec)
{
    using namespace PhysModLab;
    const auto preset = juce::File (spec.upToFirstOccurrenceOf ("|", false, false));
    const auto output = juce::File (spec.fromFirstOccurrenceOf ("|", false, false));
    IlanaSynthAudioProcessor processor;
    if (! processor.loadPresetFromFile (preset))
    {
        std::cout << "cannot load " << preset.getFullPathName() << std::endl;
        return 1;
    }

    double best = 1.0e9;
    juce::AudioBuffer<float> audio;
    for (int pass = 0; pass < 3; ++pass) // the best of three for the CPU figure
    {
        processor.loadPresetFromFile (preset);
        processor.panic();
        auto seconds = 0.0;
        audio = renderNotes (processor, { 48, 52, 55, 60 }, 4.0, 1.5, &seconds);
        best = std::min (best, seconds);
    }
    writeWav (output, audio);
    std::cout << "click ratio " << juce::String (boundaryRatio (audio, (int) (0.3 * rate)), 2)
              << ", peak " << juce::String (audio.getMagnitude (0, audio.getNumSamples()), 3)
              << ", CPU " << juce::String (100.0 * best / ((double) audio.getNumSamples() / rate), 2)
              << " % of one core" << std::endl;
    return 0;
}

void runPhysModTests()
{
    using namespace PhysModLab;
    std::cout << "--- physical knobs under LFO modulation ---" << std::endl;

    constexpr int burst = 0, saw = 2, bow = 4, hammer = 5, tine = 7, reed = 8, piano = 9, feedback = 10;
    const Case cases[] {
        { saw, "string_stiffness" }, { saw, "string_pickup" }, { saw, "string_damp" }, { saw, "string_decay" },
        { saw, "string_sustain" }, { saw, "bridge_buzz" }, { saw, "register" }, { saw, "string_excite_pos" },
        { saw, "string_pick_pos" }, { saw, "string_pick_hardness" },
        { bow, "string_excite_pos" }, { bow, "bow_pressure" }, { bow, "bow_speed" }, { bow, "string_damp" },
        { bow, "string_decay" }, { bow, "bridge_buzz" }, { bow, "string_stiffness" }, { bow, "register" },
        { hammer, "hammer_hard" }, { hammer, "string_stiffness" }, { hammer, "string_damp" }, { hammer, "string_decay" },
        { hammer, "string_pickup" },
        { piano, "hammer_hard" }, { piano, "string_stiffness" }, { piano, "string_excite_pos" }, { piano, "string_damp" },
        { piano, "string_decay" },
        { tine, "ep_distance" }, { tine, "ep_position" }, { tine, "string_damp" }, { tine, "string_decay" },
        { reed, "ep_distance" }, { reed, "ep_position" },
        { feedback, "fb_gain" }, { feedback, "fb_distance" }, { feedback, "string_damp" },
        { burst, "string_stiffness" }, { burst, "string_pickup" },
    };

    const auto verbose = juce::SystemStats::getEnvironmentVariable ("ILANA_PHYSMOD_TEST", "").isNotEmpty();

    for (const auto& c : cases)
    {
        double ratios[2] {};
        for (int modulated = 0; modulated < 2; ++modulated)
        {
            IlanaSynthAudioProcessor processor;
            setUp (processor, c, modulated ? 0.5f : 0.0f);
            const auto audio = renderNotes (processor, { 48 }, 2.0, 0.0);
            ratios[modulated] = boundaryRatio (audio, (int) (0.3 * rate));
            check (std::isfinite (audio.getMagnitude (0, audio.getNumSamples())), "finite output");
        }

        const auto name = juce::String ("excite ") + juce::String (c.excite) + " " + c.knob;
        if (verbose)
            std::cout << "INFO: " << name << " boundary bend " << juce::String (ratios[0], 2) << " still, "
                      << juce::String (ratios[1], 2) << " under the LFO" << std::endl;
        check (ratios[1] < std::max (1.5, ratios[0] * 1.3), name + " glides under an LFO (boundary bend "
                                                       + juce::String (ratios[1], 2) + ", still "
                                                       + juce::String (ratios[0], 2) + ")");
    }
}
