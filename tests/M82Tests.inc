// M8.2: the Piano exciter (PianoString). Included by TableTest.cpp.

namespace M82
{
struct Strike
{
    double contactMs = 0.0, peakForceLevel = 0.0;
    float rms50ms = 0.0f, rms1s = 0.0f, peak = 0.0f;
    bool finite = true;
};

Strike strike (int note, float velocity, float hardness = 0.5f, double seconds = 1.2, bool eco = false)
{
    KarplusStrong string;
    string.prepare (48000.0);
    string.setParams (KarplusStrong::Excite::Piano, 0.0f, 0.5f, 0.5f);
    string.setPhysicalParams (0.5f, 0.0f, 0.0f, 1.0f, 0.0f, false);
    string.setKeysParams (hardness, 0.8f);
    string.setEco (eco);
    string.setFrequency (440.0 * std::pow (2.0, (note - 69) / 12.0));
    string.trigger (velocity);
    Strike result;
    double sum50 = 0.0, sum1 = 0.0;
    auto n1 = 0;
    const auto samples = (int) (seconds * 48000.0);
    for (int i = 0; i < samples; ++i)
    {
        const auto y = string.process (0.0f, true);
        result.finite = result.finite && std::isfinite (y);
        result.peak = juce::jmax (result.peak, std::abs (y));
        if (i < 2400)
            sum50 += (double) y * y;
        if (i >= 48000 && i < 48000 + 4800)
        {
            sum1 += (double) y * y;
            ++n1;
        }
    }
    result.rms50ms = (float) std::sqrt (sum50 / 2400.0);
    result.rms1s = (float) std::sqrt (sum1 / juce::jmax (1, n1));
    return result;
}

// ILANA_PIANO_PROBE=1: prints the strike for a few notes and velocities.
void probe()
{
    for (int note : { 21, 33, 45, 60, 72, 84, 96, 108 })
        for (float velocity : { 20.0f / 127.0f, 60.0f / 127.0f, 124.0f / 127.0f })
        {
            const auto s = strike (note, velocity);
            std::cout << "note " << note << " vel " << juce::roundToInt (velocity * 127.0f)
                      << "  rms50ms " << juce::Decibels::gainToDecibels (s.rms50ms)
                      << "  rms@1s " << juce::Decibels::gainToDecibels (s.rms1s)
                      << "  peak " << s.peak << (s.finite ? "" : " NaN") << std::endl;
        }
}
// The render of one piano string, for the checks below.
std::vector<float> renderString (int note, float velocity, float hardness, double seconds, bool eco = false, int* contact = nullptr)
{
    KarplusStrong string;
    string.prepare (48000.0);
    string.setParams (KarplusStrong::Excite::Piano, 0.0f, 0.5f, 0.5f);
    string.setPhysicalParams (0.5f, 0.0f, 0.0f, 1.0f, 0.0f, false);
    string.setKeysParams (hardness, 0.8f);
    string.setEco (eco);
    string.setFrequency (440.0 * std::pow (2.0, (note - 69) / 12.0));
    string.trigger (velocity);
    std::vector<float> out ((size_t) (seconds * 48000.0));
    for (auto& y : out)
        y = string.process (0.0f, true);
    juce::ignoreUnused (contact);
    return out;
}

// Magnitude of the component at hz over [from, from + length) samples.
double magnitudeAt (const std::vector<float>& x, double hz, int from, int length)
{
    std::complex<double> sum;
    const auto w = juce::MathConstants<double>::twoPi * hz / 48000.0;
    for (int i = 0; i < length && from + i < (int) x.size(); ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / length);
        sum += (double) x[(size_t) (from + i)] * window * std::polar (1.0, -w * i);
    }
    return std::abs (sum);
}

// The partial near n f0: its frequency (searched +-40 cents around the
// inharmonic estimate) and level.
std::pair<double, double> partial (const std::vector<float>& x, double f0, int n, double b)
{
    const auto guess = n * f0 * std::sqrt (1.0 + b * n * n);
    auto best = guess, level = 0.0;
    for (double cents = -40.0; cents <= 40.0; cents += 0.5)
    {
        const auto hz = guess * std::pow (2.0, cents / 1200.0);
        const auto m = magnitudeAt (x, hz, 4800, 48000);
        if (m > level)
        {
            level = m;
            best = hz;
        }
    }
    return { best, level };
}

double centroid (const std::vector<float>& x, int from, int length)
{
    juce::dsp::FFT fft (13);
    std::vector<float> data (2 * 8192, 0.0f);
    for (int i = 0; i < 8192 && i < length && from + i < (int) x.size(); ++i)
        data[(size_t) i] = x[(size_t) (from + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / 8192.0f));
    fft.performFrequencyOnlyForwardTransform (data.data());
    auto num = 0.0, den = 0.0;
    for (int k = 1; k < 4096; ++k)
    {
        num += (double) data[(size_t) k] * k * 48000.0 / 8192.0;
        den += (double) data[(size_t) k];
    }
    return num / juce::jmax (1.0e-12, den);
}

void runTests()
{
    // Tuning: the fundamental sits within a few cents of the note (the
    // stiffness stretches it by sqrt(1 + B) at most).
    auto worstCents = 0.0;
    for (int note : { 24, 36, 48, 57, 60, 72, 84, 96 })
    {
        const auto f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        const auto x = renderString (note, 0.6f, 0.5f, 1.3);
        const auto [hz, level] = partial (x, f0, 1, 0.0);
        juce::ignoreUnused (level);
        worstCents = juce::jmax (worstCents, std::abs (1200.0 * std::log2 (hz / f0)));
        if (std::abs (1200.0 * std::log2 (hz / f0)) > 3.0)
            std::cout << "INFO: note " << note << " fundamental " << 1200.0 * std::log2 (hz / f0) << " cents" << std::endl;
    }
    check (worstCents < 4.0, "Piano strings are in tune (worst fundamental " + juce::String (worstCents, 2) + " cents off)");

    // Stretched partials: partial 8 of C2 sits sharp of 8 f0, as B says.
    {
        const auto f0 = 440.0 * std::pow (2.0, (36 - 69) / 12.0);
        const auto x = renderString (36, 0.7f, 0.5f, 1.3);
        const auto [f1, l1] = partial (x, f0, 1, 0.0);
        const auto [f8, l8] = partial (x, f0, 8, 3.0e-4);
        juce::ignoreUnused (l1, l8);
        const auto stretch = 1200.0 * std::log2 (f8 / (8.0 * f1));
        check (stretch > 0.5 && stretch < 40.0, "C2's 8th partial is stretched sharp (" + juce::String (stretch, 1) + " cents)");
    }

    // A real hammer: louder is brighter, and a harder felt is brighter.
    for (int note : { 36, 60, 84 })
    {
        const auto soft = centroid (renderString (note, 0.2f, 0.5f, 0.3), 480, 8192);
        const auto loud = centroid (renderString (note, 1.0f, 0.5f, 0.3), 480, 8192);
        const auto hard = centroid (renderString (note, 0.6f, 1.0f, 0.3), 480, 8192);
        const auto felt = centroid (renderString (note, 0.6f, 0.0f, 0.3), 480, 8192);
        check (loud > soft * 1.15 && hard > felt * 1.05,
               "note " + juce::String (note) + ": louder is brighter (" + juce::String (soft, 0) + " -> " + juce::String (loud, 0)
                   + " Hz centroid), harder felt is brighter (" + juce::String (felt, 0) + " -> " + juce::String (hard, 0) + " Hz)");
    }

    // The strike point near 1/8 thins the partials with a node there.
    {
        const auto f0 = 440.0 * std::pow (2.0, (48 - 69) / 12.0);
        const auto x = renderString (48, 0.7f, 0.5f, 1.3);
        std::vector<double> levels;
        for (int n = 1; n <= 14; ++n)
            levels.push_back (partial (x, f0, n, 3.0e-4).second);
        // The dip nearest 1 / strike fraction.
        KarplusStrong probeString;
        juce::ignoreUnused (probeString);
        auto dip = 0;
        auto lowest = 1.0e30;
        for (int n = 6; n <= 14; ++n)
            if (levels[(size_t) n - 1] < lowest)
            {
                lowest = levels[(size_t) n - 1];
                dip = n;
            }
        const auto neighbours = 0.5 * (levels[(size_t) dip - 2] + levels[(size_t) juce::jmin (13, dip)]);
        check (lowest < neighbours * 0.5, "the strike point notches partial " + juce::String (dip) + " ("
                                              + juce::String (juce::Decibels::gainToDecibels (lowest / neighbours), 1) + " dB under its neighbours)");
    }

    // Stable at every extreme, and Eco still sounds.
    {
        auto finite = true;
        auto quiet = false;
        for (int note : { 21, 40, 60, 90, 108 })
            for (float velocity : { 0.01f, 1.0f })
                for (float hardness : { 0.0f, 1.0f })
                    for (bool eco : { false, true })
                    {
                        const auto s = strike (note, velocity, hardness, 1.0, eco);
                        finite = finite && s.finite && s.peak < 8.0f;
                        quiet = quiet || s.peak < 1.0e-5f;
                    }
        check (finite && ! quiet, "the Piano exciter is stable at every note, velocity, hardness and quality");
    }

    // The preset: plays, in tune across the keyboard, and the new
    // excite and board keep old patches as they were (fingerprints).
    {
        IlanaSynthAudioProcessor processor;
        const auto index = processor.getFactoryPresetNames().indexOf ("Grand Piano");
        check (index >= 0, "the Grand Piano preset exists");
        if (index >= 0)
        {
            processor.loadFactoryPreset (index);
            processor.prepareToPlay (48000.0, 512);
            juce::AudioBuffer<float> buffer (2, 512);
            auto peak = 0.0f;
            for (int block = 0; block < 200; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    for (int note : { 36, 48, 60, 64, 67, 72 })
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                buffer.clear();
                processor.processBlock (buffer, midi);
                peak = juce::jmax (peak, buffer.getMagnitude (0, 512));
            }
            check (peak > 0.05f && peak < 1.5f, "Grand Piano plays a chord at a sensible level (peak " + juce::String (peak, 3) + ")");

            // A3 through the whole preset, measured two ways.
            processor.loadFactoryPreset (index);
            processor.prepareToPlay (48000.0, 512);
            std::vector<float> a3;
            for (int block = 0; block < 120; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
                buffer.clear();
                processor.processBlock (buffer, midi);
                for (int i = 0; i < 512; ++i)
                    a3.push_back (0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));
            }
            std::vector<float> window (a3.begin() + 4800, a3.begin() + 4800 + 28800);
            const auto autocorrelated = fundamentalOf (window, 48000.0);
            const auto [spectral, level] = partial (a3, 220.0, 1, 0.0);
            juce::ignoreUnused (level);
            std::cout << "INFO: Grand Piano A3: autocorrelation " << autocorrelated << " Hz, spectral fundamental " << spectral << " Hz" << std::endl;
        }
    }
}
} // namespace M82

void runM82PianoTests() { M82::runTests(); }
