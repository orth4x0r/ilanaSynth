// M8.3: the west-coast voice (wavefolder and low-pass gate). Included by
// TableTest.cpp.

namespace M83
{
constexpr double sr = 48000.0;

// Harmonic levels (dB re the strongest) of a periodic signal at f0.
std::vector<double> harmonics (const std::vector<float>& x, double f0, int count, int from)
{
    std::vector<double> levels;
    for (int n = 1; n <= count; ++n)
    {
        std::complex<double> sum;
        const auto w = juce::MathConstants<double>::twoPi * f0 * n / sr;
        const auto length = 16384;
        for (int i = 0; i < length; ++i)
        {
            const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / length);
            sum += (double) x[(size_t) (from + i)] * window * std::polar (1.0, -w * i);
        }
        levels.push_back (std::abs (sum));
    }
    const auto peak = *std::max_element (levels.begin(), levels.end());
    for (auto& level : levels)
        level = 20.0 * std::log10 (level / peak + 1.0e-12);
    return levels;
}

// Energy away from the harmonics of f0 (aliasing), relative to the total.
double inharmonicShare (const std::vector<float>& x, double f0)
{
    juce::dsp::FFT fft (14);
    std::vector<float> data (2 * 16384, 0.0f);
    for (int i = 0; i < 16384; ++i)
        data[(size_t) i] = x[(size_t) (4800 + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / 16384.0f));
    fft.performFrequencyOnlyForwardTransform (data.data());
    auto harmonic = 0.0, other = 0.0;
    for (int k = 1; k < 8192; ++k)
    {
        const auto hz = k * sr / 16384.0;
        const auto ratio = hz / f0;
        const auto distance = std::abs (ratio - std::round (ratio)) * f0;
        (distance < 15.0 ? harmonic : other) += (double) data[(size_t) k] * data[(size_t) k];
    }
    return other / juce::jmax (1.0e-20, harmonic + other);
}

std::vector<float> folded (double f0, float fold, float symmetry, int stages, bool antialiased = true, int samples = 48000)
{
    Wavefolder folder;
    folder.reset();
    folder.setParams (fold, symmetry, stages);
    std::vector<float> out ((size_t) samples);
    auto phase = 0.0;
    for (auto& y : out)
    {
        const auto x = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
        phase += f0 / sr;
        if (antialiased)
            y = folder.process (x);
        else
        {
            const auto gain = 0.35 + 11.65 * (double) (fold * fold);
            auto v = (double) x * gain + 0.5 * (double) symmetry;
            for (int s = 0; s < stages; ++s)
            {
                v = Wavefolder::fold (v);
                if (s + 1 < stages)
                    v *= 1.0 + 0.35 * gain / (double) stages;
            }
            y = (float) v;
        }
    }
    return out;
}

void runTests()
{
    // The vactrol against its reference: 63 % lit in ~12 ms, 63 % dark in
    // ~250 ms, 90 % dark in ~1 s.
    {
        Vactrol cell;
        cell.prepare (sr);
        cell.reset();
        auto t63 = 0.0;
        for (int i = 0; i < 48000 / 5; ++i)
            if (cell.process (1.0f) >= 0.63f && t63 == 0.0)
                t63 = i / sr;
        const auto start = cell.getConductance();
        auto dark63 = 0.0, dark90 = 0.0;
        for (int i = 0; i < 48000 * 3; ++i)
        {
            const auto c = cell.process (0.0f);
            if (dark63 == 0.0 && c <= start * 0.37f) dark63 = i / sr;
            if (dark90 == 0.0 && c <= start * 0.10f) dark90 = i / sr;
        }
        check (std::abs (t63 - 0.012) < 0.002, "the vactrol lights in " + juce::String (t63 * 1000.0, 1) + " ms (reference 12)");
        check (std::abs (dark63 - 0.25) < 0.02 && std::abs (dark90 - 1.0) < 0.08,
               "the vactrol goes 63 % dark in " + juce::String (dark63 * 1000.0, 0) + " ms (reference 250) and 90 % in "
                   + juce::String (dark90 * 1000.0, 0) + " ms (1000): slower as it darkens");
    }

    // A strike: the bongo. It opens fast, dies away over a few hundred ms,
    // and gets darker as it gets quieter.
    {
        LowPassGate gate;
        gate.prepare (sr);
        gate.setParams (LowPassGate::Mode::Combo, 1.0f, 0.3f);
        const auto source = folded (220.0, 0.5f, 0.0f, 2, true, 48000 * 2);
        std::vector<float> out (source.size());
        const auto strikeSamples = (int) (WestCoastTuning::get().strikeSeconds * sr);
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = gate.process (source[i], (int) i < strikeSamples ? 2.0f : 0.0f);
        const auto rmsAt = [&out] (double t)
        {
            auto sum = 0.0;
            const auto from = (int) (t * sr);
            for (int i = from; i < from + 2400; ++i)
                sum += (double) out[(size_t) i] * out[(size_t) i];
            return 10.0 * std::log10 (sum / 2400.0 + 1.0e-20);
        };
        const auto peak = rmsAt (0.01);
        auto end = 0.0;
        for (double t = 0.02; t < 1.9; t += 0.01)
            if (rmsAt (t) < peak - 40.0)
            {
                end = t;
                break;
            }
        check (end > 0.12 && end < 0.9, "a struck gate dies away (40 dB down) in " + juce::String (end * 1000.0, 0) + " ms");

        const auto centroidAt = [&out] (double t)
        {
            std::vector<float> slice (out.begin() + (long) (t * sr), out.begin() + (long) (t * sr) + 8192);
            juce::dsp::FFT fft (13);
            std::vector<float> data (2 * 8192, 0.0f);
            for (int i = 0; i < 8192; ++i)
                data[(size_t) i] = slice[(size_t) i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / 8192.0f));
            fft.performFrequencyOnlyForwardTransform (data.data());
            auto num = 0.0, den = 0.0;
            for (int k = 1; k < 4096; ++k)
            {
                num += (double) data[(size_t) k] * k * sr / 8192.0;
                den += (double) data[(size_t) k];
            }
            return num / juce::jmax (1.0e-12, den);
        };
        const auto early = centroidAt (0.01), middle = centroidAt (0.12), late = centroidAt (0.3);
        check (early > middle * 1.1 && middle > late * 1.05, "brightness falls with level (centroid " + juce::String (early, 0) + " -> "
                                                                   + juce::String (middle, 0) + " -> " + juce::String (late, 0) + " Hz)");
    }

    // The folder: FOLD adds harmonics, SYMMETRY adds even ones, more
    // stages add more, and the antialiasing keeps heavy folding clean.
    {
        const auto clean = harmonics (folded (220.0, 0.0f, 0.0f, 1), 220.0, 12, 4800);
        const auto folds = harmonics (folded (220.0, 0.8f, 0.0f, 1), 220.0, 12, 4800);
        auto cleanTop = -200.0, foldTop = -200.0;
        for (int n = 3; n <= 12; ++n)
        {
            cleanTop = juce::jmax (cleanTop, clean[(size_t) n - 1]);
            foldTop = juce::jmax (foldTop, folds[(size_t) n - 1]);
        }
        check (cleanTop < -25.0 && foldTop > -12.0, "FOLD adds harmonics (strongest above the 2nd: " + juce::String (cleanTop, 1) + " -> "
                                                       + juce::String (foldTop, 1) + " dB)");
        const auto even = harmonics (folded (220.0, 0.6f, 0.6f, 1), 220.0, 4, 4800);
        const auto odd = harmonics (folded (220.0, 0.6f, 0.0f, 1), 220.0, 4, 4800);
        check (even[1] > odd[1] + 20.0, "SYMMETRY brings in the even harmonics (2nd: " + juce::String (odd[1], 1) + " -> "
                                            + juce::String (even[1], 1) + " dB)");

        const auto countAbove = [] (const std::vector<double>& levels) { return (int) std::count_if (levels.begin(), levels.end(), [] (double l) { return l > -30.0; }); };
        const auto one = countAbove (harmonics (folded (110.0, 0.5f, 0.0f, 1), 110.0, 40, 4800));
        const auto four = countAbove (harmonics (folded (110.0, 0.5f, 0.0f, 4), 110.0, 40, 4800));
        check (four > one, "more stages, more harmonics (" + juce::String (one) + " -> " + juce::String (four) + " within 30 dB)");

        const auto naive = inharmonicShare (folded (1760.0, 1.0f, 0.2f, 3, false), 1760.0);
        const auto adaa = inharmonicShare (folded (1760.0, 1.0f, 0.2f, 3, true), 1760.0);
        check (adaa < naive * 0.5, "antialiasing cuts the aliases of a heavy fold at 1760 Hz ("
                                       + juce::String (10.0 * std::log10 (naive), 1) + " -> " + juce::String (10.0 * std::log10 (adaa), 1) + " dB)");
    }

    // Stability.
    {
        auto finite = true;
        juce::Random random (83);
        for (int trial = 0; trial < 40; ++trial)
        {
            Wavefolder folder;
            folder.setParams (random.nextFloat(), random.nextFloat() * 2.0f - 1.0f, 1 + random.nextInt (4));
            LowPassGate gate;
            gate.prepare (sr);
            gate.setParams ((LowPassGate::Mode) random.nextInt (3), 0.05f + random.nextFloat() * 4.0f, random.nextFloat());
            for (int i = 0; i < 24000; ++i)
            {
                const auto x = (random.nextFloat() * 2.0f - 1.0f) * 20.0f;
                const auto y = gate.process (folder.process (x), random.nextFloat());
                finite = finite && std::isfinite (y) && std::abs (y) < 100.0f;
            }
        }
        check (finite, "the folder and the gate stay finite at any setting and a +-20 input");
    }
}
} // namespace M83

// Through the processor: WEST off leaves a patch alone; on, a note strikes
// the gate and dies away; a mod source (LFO 1 B, Bounce's impacts) can
// strike it instead.
void runM83ProcessorTests()
{
    const auto renderLevels = [] (std::vector<std::pair<const char*, float>> settings)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        for (const auto& [id, value] : settings)
            setParam (processor, id, value);
        juce::AudioBuffer<float> buffer (2, 256);
        std::vector<float> rms;
        for (int block = 0; block < 375; ++block) // 2 s
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            rms.push_back (buffer.getRMSLevel (0, 0, 256));
        }
        return rms;
    };

    const auto plain = renderLevels ({});
    const auto west = renderLevels ({ { "west_on", 1.0f }, { "west_fold", 0.6f } });
    const auto peakAt = [] (const std::vector<float>& r, int from, int to) { return *std::max_element (r.begin() + from, r.begin() + to); };
    check (peakAt (plain, 300, 370) > 0.01f, "without WEST the held note sustains");
    check (peakAt (west, 0, 20) > 0.02f && peakAt (west, 300, 370) < peakAt (west, 0, 20) * 0.01f,
           "with WEST a note strikes the gate and dies away (" + juce::String (juce::Decibels::gainToDecibels (peakAt (west, 300, 370) / peakAt (west, 0, 20)), 1) + " dB after 1.6 s)");

    const auto bounced = renderLevels ({ { "west_on", 1.0f }, { "west_src", (float) Mod::Source::Lfo1B },
                                         { "lfo1_shape", (float) LfoSimShapes::Bounce }, { "lfo1_rate", 1.0f },
                                         { "lfo1_p3", 0.72f }, { "lfo1_p4", 0.0f } });
    auto hits = 0;
    for (size_t b = 2; b < bounced.size(); ++b)
        if (bounced[b] > bounced[b - 1] * 1.5f && bounced[b] > 0.005f)
            ++hits;
    check (hits >= 3, "Bounce's impacts (LFO 1 B) strike the gate again and again (" + juce::String (hits) + " strikes in 2 s)");
}

void runM83WestTests()
{
    M83::runTests();
    runM83ProcessorTests();
}
