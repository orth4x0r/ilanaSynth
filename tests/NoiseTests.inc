// The compact SUB and NOISE cards: six noise colours (src/dsp/NoiseColours.h),
// NOISE's own switch and type, SUB's own COLOUR, noise volume as a
// modulation destination, and older patches loading as they did. Included by
// TableTest.cpp; ILANA_NOISE_TEST=1 runs these alone.

namespace NoiseTests
{
// The power of `x` around `frequency` (24 log-spaced bins over +-40 %, Hann
// window, averaged: a single bin of noise swings by several dB), in dB.
inline double bandDb (const std::vector<float>& x, double sampleRate, double frequency)
{
    const auto n = x.size();
    std::vector<double> windowed (n);
    for (size_t i = 0; i < n; ++i)
        windowed[i] = (double) x[i] * (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * (double) i / (double) (n - 1)));
    double sum = 0.0;
    const auto bins = 24;
    for (int b = 0; b < bins; ++b)
    {
        const auto f = frequency * std::exp (std::log (1.4) * (2.0 * b / (bins - 1) - 1.0));
        const auto w = 2.0 * juce::MathConstants<double>::pi * f / sampleRate;
        const auto c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (const auto v : windowed)
        {
            const auto t = v + c * s1 - s2;
            s2 = s1;
            s1 = t;
        }
        sum += (s1 * s1 + s2 * s2 - c * s1 * s2) / ((double) n * (double) n) * 8.0;
    }
    return 10.0 * std::log10 (sum / bins + 1.0e-30);
}

// One Hann-windowed bin's power, in dB.
inline double bandDbAt (const std::vector<float>& x, double sampleRate, double frequency)
{
    const auto n = x.size();
    const auto w = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const auto c = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const auto v = (double) x[i] * (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * (double) i / (double) (n - 1)));
        const auto t = v + c * s1 - s2;
        s2 = s1;
        s1 = t;
    }
    return 10.0 * std::log10 ((s1 * s1 + s2 * s2 - c * s1 * s2) / ((double) n * (double) n) + 1.0e-30);
}

inline std::vector<float> generate (int type, double sampleRate, int n = 1 << 17)
{
    NoiseColours::Generator generator;
    generator.prepare (sampleRate);
    juce::Random random (99);
    std::vector<float> out ((size_t) n);
    for (int i = 0; i < 4000; ++i)
        generator.process (type, random.nextFloat() * 2.0f - 1.0f);
    for (auto& v : out)
        v = generator.process (type, random.nextFloat() * 2.0f - 1.0f);
    return out;
}

inline double rms (const std::vector<float>& x)
{
    double sum = 0.0;
    for (const auto v : x)
        sum += (double) v * v;
    return std::sqrt (sum / juce::jmax (1, (int) x.size()));
}

inline void setRaw (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

inline float valueOf (IlanaSynthAudioProcessor& processor, const juce::String& id)
{
    const auto* raw = processor.apvts.getRawParameterValue (id);
    return raw != nullptr ? raw->load() : -1.0f;
}

// A noise-only (or sub-only) patch: OSC 1 muted, the sub and noise as asked.
inline void setUp (IlanaSynthAudioProcessor& processor, bool sub, bool noise, int type, float noiseLevel = 0.6f)
{
    for (const auto* prefix : { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" })
        setRaw (processor, juce::String (prefix) + "_on", 0.0f);
    setRaw (processor, "subosc_on", sub ? 1.0f : 0.0f);
    setRaw (processor, "subosc_level", 0.8f);
    setRaw (processor, "sub_shape", 0.0f);
    setRaw (processor, "noise_on", noise ? 1.0f : 0.0f);
    setRaw (processor, "noise_type", (float) type);
    setRaw (processor, "noise_level", noiseLevel);
    setRaw (processor, "noise_color", 1.0f);
    setRaw (processor, "f1_cutoff", 20000.0f);
    setRaw (processor, "f1_env", 0.0f);
    setRaw (processor, "f1_type", 0.0f);
    setRaw (processor, "f1_drive", 1.0f); // (no drive: the sub measured clean)
}

// One held C3 after the attack, the left channel.
inline std::vector<float> render (IlanaSynthAudioProcessor& processor, float macro = 0.0f)
{
    setRaw (processor, "macro1", macro);
    processor.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);
    std::vector<float> out;
    for (int block = 0; block < 90; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        if (block >= 30)
            for (int i = 0; i < 256; ++i)
                out.push_back (buffer.getSample (0, i));
    }
    return out;
}

inline double zeroCrossings (const std::vector<float>& x)
{
    auto crossings = 0;
    for (size_t i = 1; i < x.size(); ++i)
        crossings += (x[i - 1] < 0.0f) != (x[i] < 0.0f) ? 1 : 0;
    return (double) crossings / (double) x.size();
}

inline bool identical (const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0;
}

void runGeneratorTests()
{
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        const auto label = juce::String (juce::roundToInt (sampleRate / 1000.0)) + " kHz";
        const auto white = generate (NoiseColours::white, sampleRate);
        const auto whiteRms = rms (white);
        double lowDb[NoiseColours::count], highDb[NoiseColours::count], midDb[NoiseColours::count];
        for (int type = 0; type < NoiseColours::count; ++type)
        {
            const auto x = generate (type, sampleRate);
            const auto level = 20.0 * std::log10 (rms (x) / whiteRms);
            auto mean = 0.0, peak = 0.0;
            for (const auto v : x)
            {
                mean += v;
                peak = juce::jmax (peak, (double) std::abs (v));
            }
            mean /= (double) x.size();
            check (std::abs (level) < 4.0, juce::String (NoiseColours::names[type]) + " at " + label + ": level within 4 dB of white ("
                                               + juce::String (level, 1) + " dB)");
            check (std::abs (mean) < 0.15 * rms (x), juce::String (NoiseColours::names[type]) + " at " + label + ": no DC (mean " + juce::String (mean, 4) + ")");
            check (peak < 7.0 * whiteRms && std::isfinite (peak), juce::String (NoiseColours::names[type]) + " at " + label + ": bounded peaks");
            lowDb[type] = bandDb (x, sampleRate, 200.0);
            midDb[type] = bandDb (x, sampleRate, 2500.0);
            highDb[type] = bandDb (x, sampleRate, 8000.0);
        }

        const auto tilt = [&] (int type) { return highDb[type] - lowDb[type]; };
        check (std::abs (tilt (NoiseColours::white)) < 2.5, "white at " + label + " is flat (" + juce::String (tilt (NoiseColours::white), 1) + " dB tilt)");
        check (tilt (NoiseColours::pink) < -5.0 && tilt (NoiseColours::pink) > -20.0,
               "pink at " + label + " falls with frequency (" + juce::String (tilt (NoiseColours::pink), 1) + " dB, 200 Hz to 8 kHz)");
        check (tilt (NoiseColours::brown) < tilt (NoiseColours::pink) - 8.0, "brown at " + label + " is darker than pink");
        check (tilt (NoiseColours::blue) > 15.0 && tilt (NoiseColours::blue) < 40.0, "blue at " + label + " rises with frequency");
        check (tilt (NoiseColours::violet) > tilt (NoiseColours::blue) + 8.0, "violet at " + label + " is brighter than blue");
        check (midDb[NoiseColours::grey] < lowDb[NoiseColours::grey] - 6.0 && midDb[NoiseColours::grey] < highDb[NoiseColours::grey] + 3.0,
               "grey at " + label + ": lows lifted, the presence band left down");
    }
}

void runSynthTests()
{
    // Noise alone, per type: each is audible, and they differ by brightness.
    double crossings[NoiseColours::count] {};
    std::vector<float> renders[NoiseColours::count];
    for (int type = 0; type < NoiseColours::count; ++type)
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, false, true, type);
        renders[type] = render (processor);
        crossings[type] = zeroCrossings (renders[type]);
        check (rms (renders[type]) > 0.02 && rms (renders[type]) < 0.6, juce::String (NoiseColours::names[type]) + " through the synth: audible, level "
                                                                          + juce::String (rms (renders[type]), 3));
    }
    check (crossings[NoiseColours::brown] < crossings[NoiseColours::pink] && crossings[NoiseColours::pink] < crossings[NoiseColours::white]
               && crossings[NoiseColours::white] < crossings[NoiseColours::blue] && crossings[NoiseColours::blue] <= crossings[NoiseColours::violet] + 0.02,
           "the six types differ by brightness (zero crossings brown < pink < white < blue <= violet)");
    for (int a = 0; a < NoiseColours::count; ++a)
        for (int b = a + 1; b < NoiseColours::count; ++b)
            check (! identical (renders[a], renders[b]), juce::String (NoiseColours::names[a]) + " and " + NoiseColours::names[b] + " render differently");

    // Independent switches and volumes.
    IlanaSynthAudioProcessor noiseOnly, subOnly, both, neither, noiseOffTypeA, noiseOffTypeB;
    setUp (noiseOnly, false, true, 1);
    setUp (subOnly, true, false, 1);
    setUp (both, true, true, 1);
    setUp (neither, false, false, 1);
    setUp (noiseOffTypeA, true, false, 0);
    setUp (noiseOffTypeB, true, false, 5);
    const auto noiseRender = render (noiseOnly), subRender = render (subOnly), bothRender = render (both), neitherRender = render (neither);
    check (rms (noiseRender) > 0.02 && rms (subRender) > 0.02 && rms (neitherRender) < 1.0e-4,
           "noise and sub each sound alone, and neither sounds with both off (" + juce::String (rms (neitherRender), 6) + ")");
    const auto powerSum = rms (noiseRender) * rms (noiseRender) + rms (subRender) * rms (subRender);
    check (std::abs (rms (bothRender) * rms (bothRender) / powerSum - 1.0) < 0.2,
           "sub and noise add up independently (power " + juce::String (rms (bothRender) * rms (bothRender), 4) + " vs " + juce::String (powerSum, 4) + ")");
    check (identical (render (noiseOffTypeA), render (noiseOffTypeB)), "with NOISE off its type changes nothing");

    IlanaSynthAudioProcessor quieter;
    setUp (quieter, false, true, 1, 0.3f);
    check (rms (render (quieter)) < rms (noiseRender) * 0.7, "NOISE volume turns the noise down");
    IlanaSynthAudioProcessor subQuiet;
    setUp (subQuiet, true, false, 1);
    setRaw (subQuiet, "subosc_level", 0.2f);
    check (rms (render (subQuiet)) < rms (subRender) * 0.5, "SUB volume turns the sub down, apart from the noise");

    // SUB COLOUR shapes the sub; the noise does not hear it.
    IlanaSynthAudioProcessor plain, coloured, noiseA, noiseB;
    setUp (plain, true, false, 0);
    setUp (coloured, true, false, 0);
    setRaw (coloured, "sub_colour", 1.0f);
    setUp (noiseA, false, true, 2);
    setUp (noiseB, false, true, 2);
    const auto plainSub = render (plain), colouredSub = render (coloured);
    const auto f0 = 261.6256 / 2.0; // C3, one octave down
    const auto third = [&] (const std::vector<float>& x)
    {
        // The strongest of 41 bins within +-3 % of each partial.
        const auto peak = [&] (double f)
        {
            auto best = -1.0e9;
            for (int k = -20; k <= 20; ++k)
                best = juce::jmax (best, bandDbAt (x, 48000.0, f * (1.0 + 0.0015 * k)));
            return best;
        };
        return peak (f0 * 3.0) - peak (f0);
    };
    check (third (plainSub) < -40.0, "sub colour 0: a clean sine (3rd harmonic " + juce::String (third (plainSub), 1) + " dB)");
    check (third (colouredSub) > -20.0, "sub colour 1: warm harmonics (3rd harmonic " + juce::String (third (colouredSub), 1) + " dB)");
    IlanaSynthAudioProcessor noiseC;
    setUp (noiseC, false, true, 2);
    check (std::abs (rms (render (noiseA)) / rms (render (noiseC)) - 1.0) < 0.1, "two identical fresh noise patches render at the same level");
    setRaw (noiseB, "sub_colour", 1.0f); // (fresh processors: a voice's random state carries over between renders)
    check (std::abs (rms (render (noiseA)) / rms (render (noiseB)) - 1.0) < 0.1, "SUB COLOUR leaves the noise alone (same level)");
    check (std::abs (rms (colouredSub) / rms (plainSub) - 1.0) < 0.5, "sub colour keeps the level in the same range");

    // Noise volume takes a source like any other destination.
    IlanaSynthAudioProcessor modulated;
    setUp (modulated, false, true, 0, 0.0f);
    modulated.assignModSlot ((int) Mod::Source::Macro1, (int) Mod::Destination::NoiseLevel, 1.0f);
    const auto silent = render (modulated, 0.0f), open = render (modulated, 1.0f);
    check (rms (silent) < 1.0e-3 && rms (open) > 0.05, "a source assigned to NOISE volume opens it (" + juce::String (rms (silent), 5) + " -> " + juce::String (rms (open), 3) + ")");
    IlanaSynthAudioProcessor modulatedOff;
    setUp (modulatedOff, false, false, 0, 0.0f);
    modulatedOff.assignModSlot ((int) Mod::Source::Macro1, (int) Mod::Destination::NoiseLevel, 1.0f);
    check (rms (render (modulatedOff, 1.0f)) < 1.0e-3, "a modulated noise stays silent while NOISE is off");
}

void runStateTests()
{
    IlanaSynthAudioProcessor fresh;
    check (valueOf (fresh, "noise_on") > 0.5f && valueOf (fresh, "noise_type") == 0.0f && valueOf (fresh, "sub_colour") == 0.0f,
           "defaults: noise on, White, sub colour 0 (older patches sound as before)");

    IlanaSynthAudioProcessor saved;
    setRaw (saved, "noise_on", 0.0f);
    setRaw (saved, "noise_type", 5.0f);
    setRaw (saved, "sub_colour", 0.4f);
    setRaw (saved, "noise_level", 0.35f);
    juce::MemoryBlock block;
    saved.getStateInformation (block);
    IlanaSynthAudioProcessor loaded;
    loaded.setStateInformation (block.getData(), (int) block.getSize());
    check (valueOf (loaded, "noise_on") < 0.5f && valueOf (loaded, "noise_type") == 5.0f && std::abs (valueOf (loaded, "sub_colour") - 0.4f) < 1.0e-4f
               && std::abs (valueOf (loaded, "noise_level") - 0.35f) < 1.0e-4f,
           "state round trip: switch, type, sub colour and volume");

    // A patch saved before the cards: no noise_on / noise_type / sub_colour.
    // It loads with noise on, White and colour 0, and renders as the same
    // patch with those defaults.
    IlanaSynthAudioProcessor original;
    setUp (original, true, true, 0);
    setRaw (original, "noise_level", 0.4f);
    juce::MemoryBlock current;
    original.getStateInformation (current);
    auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (current.getData(), (int) current.getSize()));
    auto removed = 0;
    if (xml != nullptr)
        for (int i = xml->getNumChildElements() - 1; i >= 0; --i)
        {
            const auto id = xml->getChildElement (i)->getStringAttribute ("id");
            if (id == "noise_on" || id == "noise_type" || id == "sub_colour")
            {
                xml->removeChildElement (xml->getChildElement (i), true);
                ++removed;
            }
        }
    juce::MemoryBlock old;
    if (xml != nullptr)
        juce::AudioProcessor::copyXmlToBinary (*xml, old);
    IlanaSynthAudioProcessor fromOld;
    setRaw (fromOld, "noise_on", 0.0f); // (loading must put the default back, not keep this)
    setRaw (fromOld, "noise_type", 3.0f);
    setRaw (fromOld, "sub_colour", 0.9f);
    fromOld.setStateInformation (old.getData(), (int) old.getSize());
    check (removed == 3 && valueOf (fromOld, "noise_on") > 0.5f && valueOf (fromOld, "noise_type") == 0.0f && valueOf (fromOld, "sub_colour") == 0.0f,
           "an older patch loads with noise on, White and sub colour 0");
    IlanaSynthAudioProcessor fromCurrent;
    fromCurrent.setStateInformation (current.getData(), (int) current.getSize());
    IlanaSynthAudioProcessor fromOldFresh;
    fromOldFresh.setStateInformation (old.getData(), (int) old.getSize());
    check (std::abs (rms (render (fromCurrent)) / rms (render (fromOldFresh)) - 1.0) < 0.1, "an older patch renders at the level of the same patch with those defaults (bit-exactness: the fingerprint gate)");

    // Her own presets (older files, no new parameters) still load.
    auto loadedPresets = 0;
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile ("content/user-presets");
    if (folder.isDirectory())
        for (const auto& file : folder.findChildFiles (juce::File::findFiles, false, "*.ilanapreset"))
        {
            IlanaSynthAudioProcessor processor;
            if (processor.loadPresetFromFile (file) && valueOf (processor, "noise_on") > 0.5f && valueOf (processor, "noise_type") == 0.0f)
                ++loadedPresets;
        }
    check (! folder.isDirectory() || loadedPresets > 0, "user presets from before the cards load with the migration defaults (" + juce::String (loadedPresets) + ")");
}
} // namespace NoiseTests

void runNoiseTests()
{
    NoiseTests::runGeneratorTests();
    NoiseTests::runSynthTests();
    NoiseTests::runStateTests();
}
