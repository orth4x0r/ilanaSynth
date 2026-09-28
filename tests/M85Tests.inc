#include <set>
// M8.5: the Feedback exciter, Evolve and the vector pad. Included by
// TableTest.cpp.

namespace M85
{
constexpr double sr = 48000.0;

struct Bloom
{
    int harmonic = 0;          // the one that ends up loudest
    double bloomSeconds = 0.0; // when it first comes within 3 dB of its final level
    double finalDb = -200.0;   // its level at the end
    double pluckDb = -200.0;   // the fundamental just after the pluck
    bool finite = true;
};

// Harmonic levels (dB) of x around time t (0.1 s window).
std::array<double, 8> harmonicLevels (const std::vector<float>& x, double f0, double t)
{
    std::array<double, 8> out {};
    const auto length = 4800;
    const auto from = (int) (t * sr);
    for (int n = 1; n <= 8; ++n)
    {
        std::complex<double> sum;
        const auto w = juce::MathConstants<double>::twoPi * f0 * n / sr;
        for (int i = 0; i < length && from + i < (int) x.size(); ++i)
            sum += (double) x[(size_t) (from + i)] * (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / length)) * std::polar (1.0, -w * i);
        out[(size_t) n - 1] = 20.0 * std::log10 (std::abs (sum) / length + 1.0e-12);
    }
    return out;
}

Bloom pluck (int note, float feedback, float gain, float distance, double seconds = 6.0)
{
    KarplusStrong string (1234 + note); // a fixed pluck: which harmonic wins is sensitive to it
    string.prepare (sr);
    string.setParams (KarplusStrong::Excite::Feedback, feedback, 0.5f, 0.8f);
    string.setPhysicalParams (0.1f, 0.0f, 0.0f, 1.0f, 0.0f, false);
    string.setFeedbackParams (gain, distance);
    const auto f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    string.setFrequency (f0);
    string.trigger (0.8f);
    std::vector<float> x ((size_t) (seconds * sr));
    Bloom b;
    for (auto& v : x)
    {
        v = string.process (0.0f, true);
        b.finite = b.finite && std::isfinite (v);
    }
    const auto end = harmonicLevels (x, f0, seconds - 0.2);
    b.harmonic = (int) (std::max_element (end.begin(), end.end()) - end.begin()) + 1;
    b.finalDb = end[(size_t) b.harmonic - 1];
    b.pluckDb = harmonicLevels (x, f0, 0.05)[0];
    for (double t = 0.1; t < seconds - 0.2; t += 0.05)
        if (harmonicLevels (x, f0, t)[(size_t) b.harmonic - 1] > b.finalDb - 3.0)
        {
            b.bloomSeconds = t;
            break;
        }
    return b;
}

// ILANA_FEEDBACK_PROBE=1: which harmonic wins, and when, across the amp's
// distance, for a few notes.
void probe()
{
    for (int note : { 40, 45, 52, 57, 64 })
        for (float distance : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            const auto b = pluck (note, 0.6f, 0.5f, distance);
            std::cout << "note " << note << " distance " << distance << ": harmonic " << b.harmonic << " blooms at "
                      << b.bloomSeconds << " s, final " << b.finalDb << " dB (pluck " << b.pluckDb << ")"
                      << (b.finite ? "" : " NaN") << std::endl;
        }
}
} // namespace M85

namespace M85
{
void feedbackTests()
{
    auto sustained = 0, low = 0, bloomed = 0, total = 0;
    auto finite = true;
    for (int note : { 40, 45, 52, 57, 64 })
        for (float distance : { 0.25f, 0.5f, 0.75f })
        {
            const auto b = pluck (note, 0.6f, 0.5f, distance);
            ++total;
            finite = finite && b.finite;
            sustained += b.finalDb > b.pluckDb - 6.0 ? 1 : 0;
            low += b.harmonic <= 3 ? 1 : 0;
            bloomed += b.bloomSeconds >= 0.3 && b.bloomSeconds <= 4.0 ? 1 : 0;
        }
    check (finite && sustained == total, "Feedback: a plucked note holds instead of dying (" + juce::String (sustained) + " of " + juce::String (total) + ")");
    check (low * 10 >= total * 7, "Feedback: it settles on a low harmonic (1st-3rd) in " + juce::String (low) + " of " + juce::String (total) + " cases");
    check (bloomed * 10 >= total * 7, "Feedback: it blooms after 0.3-4 s in " + juce::String (bloomed) + " of " + juce::String (total) + " cases");

    // The amp's distance picks the harmonic.
    std::set<int> harmonics;
    for (float distance : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        harmonics.insert (pluck (45, 0.6f, 0.5f, distance).harmonic);
    check (harmonics.size() >= 2, "Feedback: moving the amp changes which harmonic wins (" + juce::String ((int) harmonics.size()) + " different)");

    // FEEDBACK 0 is a plain pluck: it dies away.
    const auto plain = pluck (45, 0.0f, 0.5f, 0.5f, 3.0);
    check (plain.finalDb < plain.pluckDb - 20.0, "Feedback at 0 is a plain pluck that dies away");

    // Stability at the extremes.
    auto bounded = true;
    for (float gain : { 0.0f, 1.0f })
        for (float feedback : { 0.0f, 1.0f })
        {
            const auto b = pluck (40, feedback, gain, 0.0f, 3.0);
            bounded = bounded && b.finite && b.finalDb < 20.0;
        }
    check (bounded, "Feedback stays finite and bounded at full feedback and gain");
}
} // namespace M85

namespace M85
{
float renderRms (IlanaSynthAudioProcessor& processor, int blocks = 120)
{
    juce::AudioBuffer<float> buffer (2, 256);
    auto sum = 0.0;
    for (int block = 0; block < blocks; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        if (block > 20)
            sum += (double) buffer.getRMSLevel (0, 0, 256) * buffer.getRMSLevel (0, 0, 256);
    }
    return (float) std::sqrt (sum / (blocks - 21));
}

void evolveAndVectorTests()
{
    // Evolve: off leaves the macro alone; on, it wanders within its range.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        setParam (processor, "macro1", 0.5f);
        renderRms (processor, 40);
        check (processor.macroValue (0) == 0.5f, "Evolve off: the macro stays where it is set");
        setParam (processor, "macro1_evolve", 0.3f);
        setParam (processor, "macro1_evolve_rate", 1.0f);
        auto low = 1.0f, high = 0.0f;
        juce::AudioBuffer<float> buffer (2, 256);
        for (int block = 0; block < 1875; ++block) // 10 s
        {
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            low = juce::jmin (low, processor.macroValue (0));
            high = juce::jmax (high, processor.macroValue (0));
        }
        check (high - low > 0.15f && low >= 0.2f - 1.0e-4f && high <= 0.8f + 1.0e-4f,
               "Evolve drifts the macro within its range (" + juce::String (low, 2) + ".." + juce::String (high, 2) + ")");
        const auto before = processor.macroValue (0);
        processor.freezeEvolve();
        processor.processBlock (buffer, *std::make_unique<juce::MidiBuffer>());
        check (std::abs (processor.macroValue (0) - before) < 2.0e-3f && processor.getMacroDrift (0) == 0.0f,
               "FREEZE keeps the drifted value and stops the drift");
    }

    // The vector pad: equal power everywhere, and a corner is that
    // oscillator alone.
    {
        auto worst = 0.0f;
        juce::Random random (85);
        for (int i = 0; i < 200; ++i)
        {
            const auto w = IlanaSynthAudioProcessor::vectorWeights (random.nextFloat(), random.nextFloat());
            worst = juce::jmax (worst, std::abs (w[0] * w[0] + w[1] * w[1] + w[2] * w[2] + w[3] * w[3] - 1.0f));
        }
        check (worst < 1.0e-5f, "the vector pad mixes at equal power");

        const auto render = [] (bool vector, float x, float y, bool osc2, bool osc1 = true)
        {
            IlanaSynthAudioProcessor processor;
            processor.prepareToPlay (48000.0, 256);
            setParam (processor, "osc1_on", osc1 ? 1.0f : 0.0f);
            setParam (processor, "osc2_on", osc2 ? 1.0f : 0.0f);
            setParam (processor, "osc2_semi", 7.0f);
            if (vector)
            {
                setParam (processor, "vec_on", 1.0f);
                setParam (processor, "vec_a", 0.0f); // osc 1 top left
                setParam (processor, "vec_b", 1.0f); // osc 2 top right
                setParam (processor, "vec_c", 0.0f);
                setParam (processor, "vec_d", 1.0f);
                setParam (processor, "vec_x", x);
                setParam (processor, "vec_y", y);
            }
            return renderRms (processor);
        };
        const auto onlyOsc1 = render (false, 0.5f, 0.5f, false);
        const auto left = render (true, 0.0f, 0.5f, true);
        const auto right = render (true, 1.0f, 0.5f, true);
        const auto onlyOsc2Plain = render (false, 0.5f, 0.5f, true);
        const auto onlyOsc2 = render (false, 0.5f, 0.5f, true, false);
        check (std::abs (juce::Decibels::gainToDecibels (left / onlyOsc1)) < 0.5f,
               "the vector pad's left edge is Osc 1 alone (" + juce::String (juce::Decibels::gainToDecibels (left / onlyOsc1), 2) + " dB)");
        check (right > 0.01f && std::abs (juce::Decibels::gainToDecibels (right / onlyOsc2)) < 0.5f && onlyOsc2Plain > left,
               "the vector pad's right edge is Osc 2 alone (" + juce::String (juce::Decibels::gainToDecibels (right / onlyOsc2), 2) + " dB)");
    }

    // The path moves the position, and Vector X / Y are mod sources.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        setParam (processor, "vec_on", 1.0f);
        setParam (processor, "vec_path", 1.0f);
        setParam (processor, "vec_rate", 1.0f);
        juce::AudioBuffer<float> buffer (2, 256);
        auto low = 1.0f, high = 0.0f;
        for (int block = 0; block < 188; ++block)
        {
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            low = juce::jmin (low, processor.getVectorPosition().x);
            high = juce::jmax (high, processor.getVectorPosition().x);
        }
        check (high - low > 0.5f, "with PATH on the position travels the path (x " + juce::String (low, 2) + ".." + juce::String (high, 2) + ")");
        const auto names = Mod::getSourceNames();
        check (names[(int) Mod::Source::VectorX] == "Vector X" && names[(int) Mod::Source::VectorY] == "Vector Y",
               "Vector X and Y are mod sources");
    }
}
} // namespace M85

void runM85Tests()
{
    M85::feedbackTests();
    M85::evolveAndVectorTests();
}
