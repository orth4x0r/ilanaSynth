namespace
{
// ---------------------------------------------------------------------------
// M5 deep FM, M6 phase distortion, M6b 64-slot matrix.

// A note through the whole processor with a clean signal path: sine OSC 1
// straight to the output (no filter, no clip, no velocity), then configure.
std::vector<float> renderCleanPatch (const std::function<void (IlanaSynthAudioProcessor&)>& configure,
                                     int note, double seconds, double releaseAt = -1.0, int velocity = 100)
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    for (const auto& [id, value] : std::vector<std::pair<const char*, float>> {
             { "master_clip", 0.0f }, { "master", 0.0f }, { "amp_velocity", 0.0f }, { "drift", 0.0f },
             { "osc1_table", 8.0f }, { "osc1_frame", 0.0f }, { "osc1_route", 3.0f }, { "osc1_level", 1.0f },
             { "amp_attack", 0.001f }, { "amp_decay", 1.0f }, { "amp_sustain", 1.0f }, { "amp_release", 0.05f } })
        setParam (processor, id, value);

    configure (processor);

    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 512);
    const auto blocks = (int) std::ceil (seconds * 48000.0 / 512.0);
    const auto releaseBlock = releaseAt >= 0.0 ? (int) (releaseAt * 48000.0 / 512.0) : -1;

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
        if (block == releaseBlock)
            midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

        processor.processBlock (buffer, midi);

        for (int i = 0; i < 512; ++i)
            out.push_back (buffer.getSample (0, i));
    }

    return out;
}

// Amplitude of one frequency in a Hann-windowed stretch (a sine of
// amplitude A reads as A).
double partialAmplitude (const std::vector<float>& x, size_t start, size_t length, double frequency, double sampleRate = 48000.0)
{
    std::complex<double> sum;
    auto windowSum = 0.0;

    for (size_t n = 0; n < length && start + n < x.size(); ++n)
    {
        const auto w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) n / (double) length);
        sum += (double) x[start + n] * w * std::polar (1.0, -juce::MathConstants<double>::twoPi * frequency * (double) n / sampleRate);
        windowSum += w;
    }

    return 2.0 * std::abs (sum) / juce::jmax (1.0e-9, windowSum);
}

bool allFinite (const std::vector<float>& x, float limit = 8.0f)
{
    for (auto value : x)
        if (! std::isfinite (value) || std::abs (value) > limit)
            return false;

    return true;
}

double rmsOf (const std::vector<float>& x, size_t start = 0, size_t end = (size_t) -1)
{
    auto sum = 0.0;
    end = juce::jmin (end, x.size());
    for (auto i = start; i < end; ++i)
        sum += (double) x[i] * x[i];
    return std::sqrt (sum / (double) juce::jmax ((size_t) 1, end - start));
}

// The DX-style reference patches (M5). Each operator is an oscillator with
// a sine table; the same settings drive an ideal phase-modulation renderer
// below, which the engine must match.
struct FmReferencePatch
{
    const char* name = "";
    int note = 48;
    int numOps = 2;
    double ratio[6] { 1, 1, 1, 1, 1, 1 };
    float level[6] { 1, 1, 1, 1, 1, 1 };
    bool carrier[6] { true, false, false, false, false, false };
    float amount[6][6] {}; // [source][target]
    TensionAdsr::Parameters env[6];
    int snap[6] { 0, 0, 0, 0, 0, 0 };
};

std::vector<FmReferencePatch> fmReferencePatches()
{
    std::vector<FmReferencePatch> patches;

    // Electric piano, DX "two pairs": a 1:1 body and a 14:1 tine that dies fast.
    {
        FmReferencePatch p;
        p.name = "DX Keys";
        p.note = 48;
        p.numOps = 4;
        p.ratio[3] = 14.0;
        p.snap[3] = 1;
        p.level[2] = 0.6f;
        p.carrier[2] = true;
        p.amount[1][0] = 0.16f;
        p.amount[3][2] = 0.10f;
        p.env[0] = { 0.002f, 2.5f, 0.0f, 0.4f, 0.0f };
        p.env[1] = { 0.002f, 0.9f, 0.2f, 0.4f, 0.0f };
        p.env[2] = { 0.002f, 1.8f, 0.0f, 0.4f, 0.0f };
        p.env[3] = { 0.001f, 0.12f, 0.0f, 0.2f, 0.0f };
        patches.push_back (p);
    }

    // Bell: an inharmonic modulator (sqrt 12) on a long carrier, and a
    // second carrier on the bar's 2.756 mode.
    {
        FmReferencePatch p;
        p.name = "DX Bell";
        p.note = 72;
        p.numOps = 3;
        p.ratio[1] = 3.46;
        p.snap[1] = 2;
        p.ratio[2] = 2.756;
        p.snap[2] = 3;
        p.level[2] = 0.45f;
        p.carrier[2] = true;
        p.amount[1][0] = 0.3f;
        p.amount[1][2] = 0.15f;
        p.env[0] = { 0.001f, 4.0f, 0.0f, 1.0f, 0.0f };
        p.env[1] = { 0.001f, 2.0f, 0.0f, 1.0f, 0.0f };
        p.env[2] = { 0.001f, 2.2f, 0.0f, 1.0f, 0.0f };
        patches.push_back (p);
    }

    // Bass: a 1:1 pair whose modulator feeds back on itself, plus a sub
    // carrier an octave down. Plain one-sample feedback splits into a
    // Nyquist-rate oscillation above about 0.2 (in the ideal renderer too),
    // so the reference stays below that; Filtered feedback goes further.
    {
        FmReferencePatch p;
        p.name = "DX Bass";
        p.note = 36;
        p.numOps = 3;
        p.ratio[2] = 0.5;
        p.snap[2] = 1;
        p.level[2] = 0.5f;
        p.carrier[2] = true;
        p.amount[1][0] = 0.3f;
        p.amount[1][1] = 0.12f;
        p.env[0] = { 0.002f, 1.0f, 0.8f, 0.2f, 0.0f };
        p.env[1] = { 0.001f, 0.35f, 0.25f, 0.2f, 0.0f };
        p.env[2] = { 0.002f, 1.0f, 0.8f, 0.2f, 0.0f };
        patches.push_back (p);
    }

    return patches;
}

void configureFmReference (IlanaSynthAudioProcessor& processor, const FmReferencePatch& patch)
{
    const char* const prefixes[] { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" };

    for (int op = 0; op < patch.numOps; ++op)
    {
        const juce::String prefix (prefixes[op]);
        setParam (processor, prefix + "_on", 1.0f);
        setParam (processor, prefix + "_table", 8.0f);
        setParam (processor, prefix + "_frame", 0.0f);
        setParam (processor, prefix + "_route", 3.0f);
        setParam (processor, prefix + "_level", patch.level[op]);
        setParam (processor, prefix + "_fine", 0.0f);
        setParam (processor, prefix + "_semi", 0.0f);
        setParam (processor, prefix + "_unison", 1.0f);
        setParam (processor, prefix + "_out", patch.carrier[op] ? 1.0f : 0.0f);
        setParam (processor, prefix + "_tune", (float) OscTuning::Ratio);
        setParam (processor, prefix + "_ratio", (float) patch.ratio[op]);
        setParam (processor, prefix + "_ratio_snap", (float) patch.snap[op]);
        // Operator n uses ENV 5 + n (ENV 6..11), set to this operator's envelope.
        const auto env = "env" + juce::String (6 + op);
        setParam (processor, prefix + "_amp_env", (float) (5 + op));
        setParam (processor, env + "_attack", patch.env[op].attack);
        setParam (processor, env + "_decay", patch.env[op].decay);
        setParam (processor, env + "_sustain", patch.env[op].sustain);
        setParam (processor, env + "_release", patch.env[op].release);
        setParam (processor, env + "_curve", patch.env[op].curve);
    }

    for (int source = 0; source < 6; ++source)
        for (int target = 0; target < 6; ++target)
            setParam (processor, IlanaSynthAudioProcessor::fmRouteId (source, target), patch.amount[source][target]);
}

// Ideal phase modulation: each operator is level * env * sin(2 pi (phase +
// sum of amount * previous output)), with every route one sample late, as
// in the engine. Snapped ratios, as the engine plays them.
std::vector<float> renderFmReference (const FmReferencePatch& patch, double seconds)
{
    constexpr double sampleRate = 48000.0;
    const auto f0 = juce::MidiMessage::getMidiNoteInHertz (patch.note);
    std::array<TensionAdsr, 6> envs;
    std::array<double, 6> phase {}, previous {}, ratio {};

    for (int op = 0; op < patch.numOps; ++op)
    {
        envs[(size_t) op].setSampleRate (sampleRate);
        envs[(size_t) op].setParameters (patch.env[op]);
        envs[(size_t) op].noteOn();
        ratio[(size_t) op] = OscTuning::snapRatio (patch.ratio[op], patch.snap[op]);
    }

    std::vector<float> out ((size_t) (seconds * sampleRate));

    for (auto& sample : out)
    {
        std::array<double, 6> now {};
        auto mix = 0.0;

        for (int op = 0; op < patch.numOps; ++op)
        {
            auto input = 0.0;
            for (int source = 0; source < patch.numOps; ++source)
                input += (double) patch.amount[source][op] * previous[(size_t) source];

            const auto envValue = (double) envs[(size_t) op].getNextSample();
            now[(size_t) op] = (double) patch.level[op] * envValue
                               * std::sin (juce::MathConstants<double>::twoPi * (phase[(size_t) op] + input));
            phase[(size_t) op] += f0 * ratio[(size_t) op] / sampleRate;
            phase[(size_t) op] -= std::floor (phase[(size_t) op]);

            if (patch.carrier[op])
                mix += now[(size_t) op];
        }

        previous = now;
        sample = (float) mix;
    }

    return out;
}

// Mean dB difference over the reference's strong bins (within 50 dB of the
// loudest), after matching overall level once for the whole note.
double spectralErrorDb (const std::vector<float>& ours, const std::vector<float>& reference,
                        const std::vector<double>& windowStarts, double& levelOffsetDb)
{
    constexpr int order = 12;
    constexpr int size = 1 << order;
    juce::dsp::FFT fft (order);

    const auto spectrum = [&fft] (const std::vector<float>& x, size_t start)
    {
        std::vector<float> work ((size_t) size * 2, 0.0f);
        for (int i = 0; i < size && start + (size_t) i < x.size(); ++i)
            work[(size_t) i] = x[start + (size_t) i]
                               * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));
        fft.performFrequencyOnlyForwardTransform (work.data());
        work.resize ((size_t) size / 2);
        return work;
    };

    std::vector<std::vector<float>> a, b;
    auto peak = 0.0f, peakOurs = 0.0f;

    for (const auto t : windowStarts)
    {
        const auto start = (size_t) (t * 48000.0);
        a.push_back (spectrum (ours, start));
        b.push_back (spectrum (reference, start));
        for (auto value : b.back()) peak = juce::jmax (peak, value);
        for (auto value : a.back()) peakOurs = juce::jmax (peakOurs, value);
    }

    levelOffsetDb = juce::Decibels::gainToDecibels (peakOurs / juce::jmax (1.0e-12f, peak));
    const auto scale = peak / juce::jmax (1.0e-12f, peakOurs);
    auto sum = 0.0;
    auto count = 0;

    for (size_t w = 0; w < a.size(); ++w)
        for (size_t bin = 1; bin < a[w].size(); ++bin)
        {
            if (b[w][bin] < peak * 0.00316f) // -50 dB
                continue;

            // Only bins at spectral peaks of the reference (the partials).
            if (bin + 1 < b[w].size() && (b[w][bin] < b[w][bin - 1] || b[w][bin] < b[w][bin + 1]))
                continue;

            const auto oursDb = juce::Decibels::gainToDecibels (a[w][bin] * scale, -160.0f);
            const auto refDb = juce::Decibels::gainToDecibels (b[w][bin], -160.0f);
            sum += std::abs ((double) oursDb - (double) refDb);
            ++count;
        }

    return count > 0 ? sum / count : 999.0;
}

void runM5DeepFmTests()
{
    std::cout << "M5 deep FM" << std::endl;
    const char* const prefixes[] { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" };

    // Snap sets.
    check (std::abs (OscTuning::snapRatio (1.43, 2) - std::sqrt (2.0)) < 1.0e-9
               && OscTuning::snapRatio (2.9, 1) == 3.0 && OscTuning::snapRatio (2.9, 0) == 2.9
               && std::abs (OscTuning::snapRatio (2.74, 3) - 2.756) < 1.0e-9
               && std::abs (OscTuning::snapRatio (3.5, 2) - std::sqrt (12.0)) < 1.0e-9,
           "ratio SNAP picks the nearest harmonic, inharmonic (square root) or bell ratio");

    // Tuning modes: a ratio follows the note, a fixed pitch doesn't.
    {
        const auto ratioOut = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_tune", (float) OscTuning::Ratio);
            setParam (p, "osc1_ratio", 2.0f);
        }, 45, 1.0);
        const auto fixedLow = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_tune", (float) OscTuning::Fixed);
            setParam (p, "osc1_fixed_hz", 300.0f);
        }, 40, 1.0);
        const auto fixedHigh = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_tune", (float) OscTuning::Fixed);
            setParam (p, "osc1_fixed_hz", 300.0f);
        }, 70, 1.0);
        const auto ratioHz = fundamentalOf (std::vector<float> (ratioOut.begin() + 9600, ratioOut.end()), 48000.0);
        const auto lowHz = fundamentalOf (std::vector<float> (fixedLow.begin() + 9600, fixedLow.end()), 48000.0);
        const auto highHz = fundamentalOf (std::vector<float> (fixedHigh.begin() + 9600, fixedHigh.end()), 48000.0);
        check (std::abs (ratioHz - 220.0) < 0.5, "a ratio of 2 plays an octave above the note (" + juce::String (ratioHz, 2) + " Hz)");
        check (std::abs (lowHz - 300.0) < 0.5 && std::abs (highHz - 300.0) < 0.5,
               "a fixed-frequency operator ignores the note (" + juce::String (lowHz, 2) + ", " + juce::String (highHz, 2) + " Hz)");
    }

    // Two operators follow Bessel's sideband levels: carrier f, modulator
    // 5f, index 1.5 rad -> partials f (J0), 4f and 6f (J1), 9f and 11f (J2).
    {
        const auto beta = 1.5;
        const auto out = renderCleanPatch ([beta] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc2_on", 1.0f);
            setParam (p, "osc2_table", 8.0f);
            setParam (p, "osc2_frame", 0.0f);
            setParam (p, "osc2_level", 1.0f);
            setParam (p, "osc2_fine", 0.0f);
            setParam (p, "osc2_out", 0.0f);
            setParam (p, "osc2_tune", (float) OscTuning::Ratio);
            setParam (p, "osc2_ratio", 5.0f);
            setParam (p, "fm_amount", (float) (beta / juce::MathConstants<double>::twoPi));
        }, 45, 1.0);

        const auto f = 110.0;
        const auto at = [&out, f] (double multiple) { return partialAmplitude (out, 12000, 32768, f * multiple); };
        const auto j0 = 0.511828, j1 = 0.557937, j2 = 0.232088;
        const auto errorDb = [] (double measured, double expected) { return std::abs (20.0 * std::log10 (measured / expected)); };
        const auto e0 = errorDb (at (1) / at (6), j0 / j1);
        const auto e1 = errorDb (at (4) / at (6), 1.0);
        const auto e2 = errorDb (at (11) / at (6), j2 / j1);
        const auto e3 = errorDb (at (9) / at (6), j2 / j1);
        check (e0 < 0.2 && e1 < 0.2 && e2 < 0.2 && e3 < 0.2,
               "two operators match Bessel sideband levels (errors " + juce::String (e0, 3) + ", " + juce::String (e1, 3) + ", "
                   + juce::String (e2, 3) + ", " + juce::String (e3, 3) + " dB)");
    }

    // Reference patches: engine against an ideal phase-modulation renderer.
    for (const auto& patch : fmReferencePatches())
    {
        const auto ours = renderCleanPatch ([&patch] (IlanaSynthAudioProcessor& p) { configureFmReference (p, patch); },
                                            patch.note, 1.6);
        const auto reference = renderFmReference (patch, 1.6);
        auto offset = 0.0;
        const auto error = spectralErrorDb (ours, reference, { 0.01, 0.15, 0.5, 1.0 }, offset);
        std::cout << "  fit: " << patch.name << " mean partial error " << juce::String (error, 3)
                  << " dB (level offset " << juce::String (offset, 2) << " dB)" << std::endl;
        check (error < 0.5, juce::String (patch.name) + " matches the ideal FM reference (mean partial error "
                                + juce::String (error, 3) + " dB, target 0.5)");
    }

    // Feedback types: all bounded at full depth; filtered is calmer; cross
    // runs between a pair.
    {
        const auto feedbackPatch = [] (int type, float amount)
        {
            return renderCleanPatch ([type, amount] (IlanaSynthAudioProcessor& p)
            {
                setParam (p, "osc2_on", 1.0f);
                setParam (p, "osc2_table", 8.0f);
                setParam (p, "osc2_frame", 0.0f);
                setParam (p, "osc2_level", 1.0f);
                setParam (p, "osc2_route", 3.0f);
                setParam (p, "osc1_fb_type", (float) type);
                setParam (p, "osc2_fb_type", (float) type);
                setParam (p, "fm_feedback", amount);
                setParam (p, "fm_fb2", amount);
            }, 45, 0.8);
        };

        for (int type = 0; type < FmFeedback::Count; ++type)
        {
            const auto out = feedbackPatch (type, 1.0f);
            check (allFinite (out) && rmsOf (out, 9600) > 0.05,
                   "feedback type " + FmFeedback::getNames()[type] + " stays finite and bounded at full depth");
        }

        double plainCentroid = 0.0, filteredCentroid = 0.0;
        {
            const auto plain = feedbackPatch (FmFeedback::Plain, 0.5f);
            const auto filtered = feedbackPatch (FmFeedback::Filtered, 0.5f);
            plainCentroid = centroidOf (plain, 20000, 48000.0);
            filteredCentroid = centroidOf (filtered, 20000, 48000.0);
        }
        // Plain feedback past about 0.2 splits into a buzz at half the sample
        // rate; the filtered type doesn't.
        {
            const auto plain = feedbackPatch (FmFeedback::Plain, 0.3f);
            const auto filtered = feedbackPatch (FmFeedback::Filtered, 0.3f);
            const auto nyquistShare = [] (const std::vector<float>& x)
            {
                auto alternating = 0.0;
                for (size_t i = 20000; i + 1 < x.size(); ++i)
                    alternating += std::abs ((double) x[i] - (double) x[i + 1]);
                auto total = 0.0;
                for (size_t i = 20000; i < x.size(); ++i)
                    total += 2.0 * std::abs ((double) x[i]);
                return alternating / juce::jmax (1.0e-9, total);
            };
            check (nyquistShare (plain) > 0.5 && nyquistShare (filtered) < 0.1,
                   "filtered feedback stays clear of the half-sample-rate buzz plain feedback falls into ("
                       + juce::String (nyquistShare (plain), 2) + " vs " + juce::String (nyquistShare (filtered), 3) + ")");
        }

        check (filteredCentroid < plainCentroid * 0.9,
               "filtered feedback is calmer than plain (centroid " + juce::String (plainCentroid, 0) + " -> "
                   + juce::String (filteredCentroid, 0) + " Hz)");

        // Cross: OSC 2 is a silent modulator; with only its FB cell set, cross
        // feedback reaches OSC 1 (plain would not).
        const auto crossOnly = [] (int type)
        {
            return renderCleanPatch ([type] (IlanaSynthAudioProcessor& p)
            {
                setParam (p, "osc2_on", 1.0f);
                setParam (p, "osc2_table", 8.0f);
                setParam (p, "osc2_level", 1.0f);
                setParam (p, "osc2_out", 0.0f);
                setParam (p, "osc2_fine", 0.0f);
                setParam (p, "osc2_fb_type", (float) type);
                setParam (p, "fm_fb2", 0.4f);
            }, 45, 0.6);
        };
        const auto plainOut = crossOnly (FmFeedback::Plain);
        const auto crossOut = crossOnly (FmFeedback::Cross);
        const auto sine = partialAmplitude (plainOut, 9600, 16384, 110.0);
        const auto h2Plain = partialAmplitude (plainOut, 9600, 16384, 220.0) / sine;
        const auto h2Cross = partialAmplitude (crossOut, 9600, 16384, 220.0) / partialAmplitude (crossOut, 9600, 16384, 110.0);
        check (h2Plain < 0.01 && h2Cross > 0.05,
               "cross feedback runs between a pair (OSC 1's 2nd harmonic " + juce::String (h2Plain, 4) + " -> "
                   + juce::String (h2Cross, 3) + ")");
    }

    // The noise operator.
    {
        const auto clean = renderCleanPatch ([] (IlanaSynthAudioProcessor&) {}, 57, 0.5);
        const auto noisy = renderCleanPatch ([] (IlanaSynthAudioProcessor& p) { setParam (p, "fm_noise1", 0.3f); }, 57, 0.5);
        const auto dark = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "fm_noise1", 0.3f);
            setParam (p, "fm_noise_color", 0.0f);
        }, 57, 0.5);
        const auto cleanCentroid = centroidOf (clean, 12000, 48000.0);
        const auto noisyCentroid = centroidOf (noisy, 12000, 48000.0);
        const auto darkCentroid = centroidOf (dark, 12000, 48000.0);
        check (allFinite (noisy) && noisyCentroid > cleanCentroid * 3.0 && darkCentroid < noisyCentroid * 0.7,
               "the noise operator spreads a sine into noise, and COLOUR darkens it (centroid "
                   + juce::String (cleanCentroid, 0) + " -> " + juce::String (noisyCentroid, 0) + " / dark "
                   + juce::String (darkCentroid, 0) + " Hz)");
    }

    // Level key scaling: +6 dB per octave at KEY LVL 1.
    {
        const auto levelAt = [] (int note, float keyLevel)
        {
            const auto out = renderCleanPatch ([keyLevel] (IlanaSynthAudioProcessor& p) { setParam (p, "osc1_key_level", keyLevel); },
                                               note, 0.5);
            return rmsOf (out, 9600);
        };
        const auto flat = levelAt (84, 0.0f);
        const auto up = levelAt (84, 1.0f);
        const auto down = levelAt (84, -0.5f);
        const auto upDb = 20.0 * std::log10 (up / flat);
        const auto downDb = 20.0 * std::log10 (down / flat);
        check (std::abs (upDb - 12.0) < 0.3 && std::abs (downDb + 6.0) < 0.3,
               "level key scaling two octaves up: +" + juce::String (upDb, 2) + " dB at KEY LVL 100 %, "
                   + juce::String (downDb, 2) + " dB at -50 %");
    }

    // DAHDSR: the delay waits, the hold holds.
    {
        const auto delayed = renderCleanPatch ([] (IlanaSynthAudioProcessor& p) { setParam (p, "amp_delay", 0.1f); }, 57, 0.4);
        check (rmsOf (delayed, 0, 4500) < 1.0e-4 && rmsOf (delayed, 6000, 9000) > 0.1,
               "envelope DELAY keeps the note silent, then plays it");

        const auto held = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "amp_hold", 0.2f);
            setParam (p, "amp_decay", 0.02f);
            setParam (p, "amp_sustain", 0.0f);
        }, 57, 0.5);
        check (rmsOf (held, 7000, 8500) > 0.3 && rmsOf (held, 14000, 16000) < 0.01,
               "envelope HOLD stays at the peak, then decays");
    }

    // Rate key scaling: a higher note's decay is shorter.
    {
        const auto decayRatio = [] (float keyRate)
        {
            const auto energyAfter = [keyRate] (int note)
            {
                const auto out = renderCleanPatch ([keyRate] (IlanaSynthAudioProcessor& p)
                {
                    setParam (p, "amp_decay", 0.4f);
                    setParam (p, "amp_sustain", 0.0f);
                    setParam (p, "amp_keyrate", keyRate);
                }, note, 0.6);
                return rmsOf (out, 7200, 9600) / rmsOf (out, 480, 960);
            };
            return energyAfter (84) / energyAfter (60);
        };
        check (decayRatio (0.0f) > 0.9 && decayRatio (1.0f) < 0.5,
               "KEY RATE shortens the envelope up the keyboard (" + juce::String (decayRatio (0.0f), 2) + " -> "
                   + juce::String (decayRatio (1.0f), 2) + ")");
    }

    // The MSEG as an operator envelope: silent while it is below zero.
    {
        const auto out = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_amp_env", 16.0f);
            setParam (p, "mseg_rate", 2.0f);
            setParam (p, "mseg_loop", 0.0f);
        }, 57, 0.5);
        // Default shape 0 -> 1 -> 0 -> -1 over 0.5 s: loud in the first
        // quarter-to-half, silent in the last quarter.
        check (rmsOf (out, 5000, 7000) > 0.3 && rmsOf (out, 20000, 23000) < 1.0e-3,
               "an oscillator can use the MSEG as a one-shot envelope");
    }

    // Algorithms: each sets its routing, is recognised, and plays.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        auto allMatch = true;
        auto allPlay = true;
        juce::String failed;

        for (int index = 0; index < FmAlgorithms::count(); ++index)
        {
            processor.applyFmAlgorithm (index);
            const auto matched = processor.findMatchingFmAlgorithm();

            if (matched != index)
            {
                allMatch = false;
                failed << FmAlgorithms::all()[(size_t) index].name << " (matched " << matched << ") ";
            }

            juce::AudioBuffer<float> buffer (2, 512);
            std::vector<float> out;
            for (int block = 0; block < 20; ++block)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                if (block == 15)
                    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                processor.processBlock (buffer, midi);
                for (int i = 0; i < 512; ++i)
                    out.push_back (buffer.getSample (0, i));
            }

            allPlay = allPlay && allFinite (out) && rmsOf (out) > 1.0e-3;
        }

        check (allMatch, "all 16 algorithms set their routing and are recognised " + failed);
        check (allPlay, "all 16 algorithms play finite, audible notes");

        processor.applyFmAlgorithm (10); // DX 5 Keys: six operators
        auto sixShown = true;
        for (int osc = 0; osc < 6; ++osc)
            sixShown = sixShown && processor.isOscillatorShown (osc);
        check (sixShown && processor.apvts.getRawParameterValue ("osc6_out")->load() < 0.5f
                   && processor.apvts.getRawParameterValue ("osc5_out")->load() > 0.5f,
               "a six-operator algorithm adds OSC 4-6 and sets carriers and modulators");

        // Existing route amounts survive re-applying.
        setParam (processor, "fm_amount", 0.8f);
        processor.applyFmAlgorithm (10);
        check (std::abs (processor.apvts.getRawParameterValue ("fm_amount")->load() - 0.8f) < 1.0e-4f,
               "applying an algorithm keeps the amounts of routes it keeps");
    }

    // Everything at once, at the extremes: stays finite.
    {
        const auto out = renderCleanPatch ([&prefixes] (IlanaSynthAudioProcessor& p)
        {
            for (int osc = 0; osc < 6; ++osc)
            {
                const juce::String prefix (prefixes[osc]);
                setParam (p, prefix + "_on", 1.0f);
                setParam (p, prefix + "_level", 1.0f);
                setParam (p, prefix + "_tune", (float) (osc % 3));
                setParam (p, prefix + "_ratio", 17.0f);
                setParam (p, prefix + "_fixed_hz", 15000.0f);
                setParam (p, prefix + "_key_level", 1.0f);
                setParam (p, prefix + "_fb_type", (float) (osc % 3));
                setParam (p, prefix + "_warp", (float) Warp::PdRes3);
                setParam (p, prefix + "_warp_amt", 1.0f);
                setParam (p, prefix + "_warp2", 12.0f);
                setParam (p, prefix + "_warp2_amt", 1.0f);
                setParam ((p), "fm_noise" + juce::String (osc + 1), 1.0f);
                for (int target = 0; target < 6; ++target)
                    setParam (p, IlanaSynthAudioProcessor::fmRouteId (osc, target), 1.0f);
            }
        }, 96, 1.0);
        check (allFinite (out, 16.0f), "six operators at full depth with every feedback type, noise and PD stay finite");
    }
}

// The Casio CZ's phase distortion, written out from its definition (a cosine
// read through a bent phase), as the reference for M6.
double czReference (int mode, double a, double p)
{
    const auto twoPi = juce::MathConstants<double>::twoPi;

    switch (mode)
    {
        case Warp::PdSaw:
        {
            const auto d = 0.5 - 0.49 * a;
            const auto phi = p < d ? p / (2.0 * d) : 0.5 + (p - d) / (2.0 * (1.0 - d));
            return std::cos (twoPi * phi);
        }
        case Warp::PdSquare:
        {
            const auto d = 0.5 - 0.49 * a;
            const auto local = std::fmod (p, 0.5);
            const auto phi = (p < 0.5 ? 0.0 : 0.5) + (local < d ? 0.5 * local / d : 0.5);
            return std::cos (twoPi * phi);
        }
        case Warp::PdPulse:
        {
            const auto d = 1.0 - 0.95 * a;
            return std::cos (twoPi * (p < d ? p / d : 1.0));
        }
        default:
        {
            // Resonance: a sine at 1 + 15a times the pitch, restarting every
            // cycle under a saw, triangle or trapezoid window.
            const auto window = mode == Warp::PdRes1 ? 1.0 - p
                              : mode == Warp::PdRes2 ? 1.0 - std::abs (2.0 * p - 1.0)
                                                     : juce::jmin (1.0, 2.0 * (1.0 - p));
            const auto depth = juce::jmin (1.0, 4.0 * a);
            return (1.0 - depth * (1.0 - window)) * std::sin (twoPi * p * (1.0 + 15.0 * a));
        }
    }
}

void runM6PhaseDistortionTests()
{
    std::cout << "M6 phase distortion" << std::endl;
    IlanaSynthAudioProcessor processor;
    const auto* sine = processor.getWavetable (8);

    // Waveforms: one cycle at 2048 samples against the CZ definition. The
    // engine reads a sine-phase table, so the non-resonant waves come out
    // inverted and a quarter cycle late (inaudible).
    for (int mode = Warp::PdSaw; mode <= Warp::PdRes3; ++mode)
    {
        for (const auto amount : { 0.3f, 1.0f })
        {
            WavetableOscillator osc;
            osc.setSampleRate (48000.0);
            osc.setWavetable (sine);
            osc.setFrequency (48000.0 / 2048.0);
            osc.setWarp (mode, amount);
            osc.resetPhase (0.0);

            std::vector<double> ours (2048), reference (2048);
            auto maxError = 0.0;

            for (int n = 0; n < 2048; ++n)
            {
                const auto p = (double) n / 2048.0;
                ours[(size_t) n] = (double) osc.getNextSample();
                reference[(size_t) n] = Warp::isResonance (mode) ? czReference (mode, amount, p)
                                                                 : -czReference (mode, amount, std::fmod (p + 0.25, 1.0));
                maxError = juce::jmax (maxError, std::abs (ours[(size_t) n] - reference[(size_t) n]));
            }

            // Harmonic magnitudes 1-16 (sign and phase free).
            auto worstDb = 0.0;
            for (int h = 1; h <= 16; ++h)
            {
                std::complex<double> a, b;
                for (int n = 0; n < 2048; ++n)
                {
                    const auto e = std::polar (1.0, -juce::MathConstants<double>::twoPi * h * n / 2048.0);
                    a += ours[(size_t) n] * e;
                    b += reference[(size_t) n] * e;
                }
                if (std::abs (b) / 1024.0 > 0.001)
                    worstDb = juce::jmax (worstDb, std::abs (20.0 * std::log10 (std::abs (a) / std::abs (b))));
            }

            // Sharp PD corners can't be traced exactly by a band-limited read.
            check (maxError < 0.03 && worstDb < 0.5,
                   Warp::getNames()[mode] + " at " + juce::String (juce::roundToInt (amount * 100.0f))
                       + " % matches the CZ waveform (max error " + juce::String (maxError, 4) + ", harmonics within "
                       + juce::String (worstDb, 3) + " dB)");
        }
    }

    // Spectra: saw has every harmonic, square mostly odd ones, and the
    // resonance peak follows the amount.
    {
        const auto harmonics = [sine] (int mode, float amount)
        {
            WavetableOscillator osc;
            osc.setSampleRate (48000.0);
            osc.setWavetable (sine);
            osc.setFrequency (48000.0 / 2048.0);
            osc.setWarp (mode, amount);
            std::vector<double> x (2048);
            for (auto& value : x)
                value = (double) osc.getNextSample();
            std::vector<double> levels (33, 0.0);
            for (int h = 1; h <= 32; ++h)
            {
                std::complex<double> sum;
                for (int n = 0; n < 2048; ++n)
                    sum += x[(size_t) n] * std::polar (1.0, -juce::MathConstants<double>::twoPi * h * n / 2048.0);
                levels[(size_t) h] = std::abs (sum) / 1024.0;
            }
            return levels;
        };

        const auto saw = harmonics (Warp::PdSaw, 1.0f);
        const auto square = harmonics (Warp::PdSquare, 1.0f);
        auto sawFull = true;
        for (int h = 1; h <= 8; ++h)
            sawFull = sawFull && saw[(size_t) h] > saw[1] * 0.05;
        const auto evenOdd = (square[2] + square[4] + square[6]) / (square[1] + square[3] + square[5]);
        check (sawFull, "PD Saw at full depth has all of its first eight harmonics");
        check (evenOdd < 0.1, "PD Square at full depth is mostly odd harmonics (even/odd " + juce::String (evenOdd, 3) + ")");

        const auto peakOf = [] (const std::vector<double>& levels)
        {
            int best = 1;
            for (int h = 2; h <= 32; ++h)
                if (levels[(size_t) h] > levels[(size_t) best])
                    best = h;
            return best;
        };
        const auto low = peakOf (harmonics (Warp::PdRes1, 0.3f));
        const auto high = peakOf (harmonics (Warp::PdRes1, 0.9f));
        check (std::abs (low - 5.5) <= 1.5 && std::abs (high - 14.5) <= 1.5,
               "PD Res I's peak sits at 1 + 15 x amount (harmonic " + juce::String (low) + " at 30 %, "
                   + juce::String (high) + " at 90 %)");
    }

    // Through the engine: the warp envelope sweeps brightness like a CZ DCW,
    // and the second stage changes the wave.
    {
        const auto sweep = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_warp", (float) Warp::PdSaw);
            setParam (p, "osc1_warp_amt", 0.001f);
            setParam (p, "osc1_pd_env", 2.0f); // ENV 2 (filter envelope)
            setParam (p, "osc1_pd_env_amt", 1.0f);
            setParam (p, "fe_attack", 0.001f);
            setParam (p, "fe_decay", 0.5f);
            setParam (p, "fe_sustain", 0.0f);
        }, 45, 1.2);
        const auto early = centroidOf (sweep, 2400, 48000.0);
        const auto late = centroidOf (sweep, 48000, 48000.0);
        check (early > late * 2.0, "the warp envelope closes the PD wave as it decays (centroid "
                                       + juce::String (early, 0) + " -> " + juce::String (late, 0) + " Hz)");

        const auto one = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_warp", (float) Warp::PdSaw);
            setParam (p, "osc1_warp_amt", 0.6f);
        }, 45, 0.5);
        const auto chain = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_warp", (float) Warp::PdSaw);
            setParam (p, "osc1_warp_amt", 0.6f);
            setParam (p, "osc1_warp2", 11.0f); // PD Res I in the stage-two list
            setParam (p, "osc1_warp2_amt", 0.5f);
        }, 45, 0.5);
        check (Warp::modeForStageTwoChoice (11) == Warp::PdRes1 && Warp::getStageTwoNames()[11] == "PD Res I",
               "the second stage's list skips FM and Ring and maps onto the warp modes");
        check (rmsOf (chain, 9600) > 0.05 && std::abs (centroidOf (chain, 9600, 48000.0) - centroidOf (one, 9600, 48000.0)) > 100.0,
               "the PD chain's second stage changes the wave");
    }

    // Old patches: warp modes keep their indices; PD extras default off.
    check (Warp::getNames()[9] == "Ring" && Warp::getNames()[10] == "PD Saw" && Warp::getNames()[16] == "Formant"
               && Warp::getNames()[17] == "Squeeze" && Warp::Count == 18,
           "PD warps are appended after the existing warp modes");
    check (processor.apvts.getRawParameterValue ("osc1_warp2")->load() == 0.0f
               && processor.apvts.getRawParameterValue ("osc1_pd_env")->load() == 0.0f
               && processor.apvts.getRawParameterValue ("osc1_tune")->load() == 0.0f,
           "new operator and PD settings default to the old behaviour");
}

void runM6bMatrixTests()
{
    std::cout << "M6b modulation depth" << std::endl;
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto allSlots = Mod::maxSlots == 64;
    for (int slot = 1; slot <= 64; ++slot)
        for (const auto* field : { "_src", "_dst", "_amt", "_curve", "_pol", "_aux", "_byp" })
            allSlots = allSlots && processor.apvts.getParameter ("mod" + juce::String (slot) + field) != nullptr;
    check (allSlots, "the matrix has 64 slots, each with all seven settings");

    // Slots 1-32 keep their place in the parameter list; 33-64 come after
    // every older parameter.
    const auto& parameters = processor.getParameters();
    int indexOf32 = -1, indexOf33 = -1, indexOfLastM4 = -1;
    for (int i = 0; i < parameters.size(); ++i)
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]))
        {
            if (withId->paramID == "mod32_byp") indexOf32 = i;
            if (withId->paramID == "mod33_src") indexOf33 = i;
            if (withId->paramID == "mech_pedal") indexOfLastM4 = i;
        }
    check (indexOf32 >= 0 && indexOf33 > indexOfLastM4 && indexOfLastM4 > indexOf32,
           "slots 33-64 are appended after every existing parameter");

    // A routing in slot 64 modulates.
    {
        const auto plain = renderCleanPatch ([] (IlanaSynthAudioProcessor&) {}, 57, 0.4);
        const auto routed = renderCleanPatch ([] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "mod64_src", (float) Mod::Source::Lfo1);
            setParam (p, "mod64_dst", (float) Mod::Destination::Osc1Pitch);
            setParam (p, "mod64_amt", 0.2f);
        }, 57, 0.4);
        auto difference = 0.0;
        for (size_t i = 0; i < plain.size(); ++i)
            difference += std::abs (plain[i] - routed[i]);
        check (difference / (double) plain.size() > 0.05, "a routing in slot 64 modulates its target");
    }

    // A slot past 32 survives a state round trip, and old states load with
    // slots 33-64 empty.
    {
        IlanaSynthAudioProcessor source;
        setParam (source, "mod50_src", (float) Mod::Source::Lfo2);
        setParam (source, "mod50_dst", (float) Mod::Destination::Filter1Cutoff);
        setParam (source, "mod50_amt", -0.4f);
        juce::MemoryBlock data;
        source.getStateInformation (data);
        IlanaSynthAudioProcessor target;
        target.setStateInformation (data.getData(), (int) data.getSize());
        const auto slot = target.readModSlot (49);
        check (slot.source == Mod::Source::Lfo2 && slot.destination == (int) Mod::Destination::Filter1Cutoff
                   && std::abs (slot.depth + 0.4f) < 1.0e-3f,
               "slot 50 survives a state round trip");

        auto state = source.apvts.copyState();
        for (int i = state.getNumChildren(); --i >= 0;)
        {
            const auto id = state.getChild (i).getProperty ("id").toString();
            if (id.startsWith ("mod") && id.fromFirstOccurrenceOf ("mod", false, false).getIntValue() > 32)
                state.removeChild (i, nullptr);
        }
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        juce::MemoryBlock oldData;
        juce::AudioProcessor::copyXmlToBinary (*xml, oldData);
        IlanaSynthAudioProcessor fromOld;
        setParam (fromOld, "mod40_src", 3.0f);
        fromOld.setStateInformation (oldData.getData(), (int) oldData.getSize());
        auto empty = true;
        for (int slotIndex = 32; slotIndex < 64; ++slotIndex)
            empty = empty && fromOld.readModSlot (slotIndex).source == Mod::Source::None;
        check (empty, "a 32-slot state loads with slots 33-64 empty");
    }

    // The new operator settings are parameter destinations; destinations
    // still fit.
    check (Mod::destinationForParamId ("osc2_warp2_amt") > 0 && Mod::destinationForParamId ("fm_noise3") > 0
               && Mod::destinationForParamId ("fm_4to1") > 0 && Mod::destinationForParamId ("fm_fb6") > 0
               && Mod::getNumDestinations() < IlanaSynthAudioProcessor::maxDestinations
               && Mod::getDestinationNames().size() == Mod::getNumDestinations(),
           "PD, noise, key level and OSC 4-6 FM cells are modulation destinations ("
               + juce::String (Mod::getNumDestinations()) + " of " + juce::String (IlanaSynthAudioProcessor::maxDestinations) + ")");
    check (Mod::destinationForParamId ("mech_pedal") == (int) Mod::Destination::Count + 88,
           "the M4 destinations keep their indices");

    // CPU: empty slots cost nothing measurable; a full matrix stays cheap.
    {
        const auto timeWith = [] (int usedSlots)
        {
            IlanaSynthAudioProcessor p;
            p.prepareToPlay (48000.0, 512);
            const Mod::Destination targets[] { Mod::Destination::Osc1Pitch, Mod::Destination::Osc1Frame,
                                               Mod::Destination::Filter1Cutoff, Mod::Destination::Pan,
                                               Mod::Destination::Osc1Warp, Mod::Destination::AmpLevel };
            for (int slot = 1; slot <= usedSlots; ++slot)
            {
                const auto prefix = "mod" + juce::String (slot);
                setParam (p, prefix + "_src", (float) Mod::lfoSourceFor ((slot - 1) % 16));
                setParam (p, prefix + "_dst", (float) targets[(slot - 1) % 6]);
                setParam (p, prefix + "_amt", 0.01f);
            }

            juce::AudioBuffer<float> buffer (2, 512);
            auto best = 1.0e9;
            for (int run = 0; run < 3; ++run)
            {
                const auto start = juce::Time::getHighResolutionTicks();
                for (int block = 0; block < 94; ++block)
                {
                    buffer.clear();
                    juce::MidiBuffer midi;
                    if (block == 0 && run == 0)
                        for (const auto note : { 48, 52, 55, 59, 62, 67, 71, 74 })
                            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                    p.processBlock (buffer, midi);
                }
                best = juce::jmin (best, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) * 1000.0);
            }
            return best; // ms per second of audio
        };

        const auto empty = timeWith (0);
        const auto full = timeWith (64);
        std::cout << "  cpu: matrix empty " << juce::String (empty, 1) << " ms/s, all 64 slots "
                  << juce::String (full, 1) << " ms/s" << std::endl;
        checkTiming (full < 400.0, "a full 64-slot matrix with eight notes stays well inside real time ("
                                 + juce::String (full, 1) + " ms/s)");
    }
}

// Listening material for M5/M6: the DX reference patches and the CZ waves.
void renderFmPdDemos (const juce::File& folder)
{
    folder.createDirectory();

    const auto write = [&folder] (const juce::String& name, const std::vector<float>& samples)
    {
        juce::AudioBuffer<float> buffer (1, (int) samples.size());
        auto peak = 1.0e-6f;
        for (auto value : samples)
            peak = juce::jmax (peak, std::abs (value));
        for (int i = 0; i < (int) samples.size(); ++i)
            buffer.setSample (0, i, samples[(size_t) i] * 0.7f / peak);
        const auto file = folder.getChildFile (name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (file), 48000.0, 1, 24, {}, 0));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        std::cout << file.getFullPathName() << std::endl;
    };

    for (const auto& patch : fmReferencePatches())
    {
        const auto name = juce::String (patch.name).replaceCharacter (' ', '-').toLowerCase();
        write (name, renderCleanPatch ([&patch] (IlanaSynthAudioProcessor& p) { configureFmReference (p, patch); },
                                       patch.note, 2.5, 1.8));
        write (name + "-reference", renderFmReference (patch, 2.5));
    }

    for (int mode = Warp::PdSaw; mode <= Warp::PdRes3; ++mode)
    {
        const auto name = "cz-" + Warp::getNames()[mode].replace (" ", "-").toLowerCase();
        write (name, renderCleanPatch ([mode] (IlanaSynthAudioProcessor& p)
        {
            setParam (p, "osc1_warp", (float) mode);
            setParam (p, "osc1_warp_amt", 0.05f);
            setParam (p, "osc1_pd_env", 2.0f);
            setParam (p, "osc1_pd_env_amt", 0.9f);
            setParam (p, "fe_attack", 0.4f);
            setParam (p, "fe_decay", 1.2f);
            setParam (p, "fe_sustain", 0.0f);
        }, 45, 2.5, 2.0));
    }
}

} // namespace

// M7.0: FM cells to or from OSC 4-6 are modulated per voice, like the nine
// original cells (an envelope or velocity acts on each note separately).
void runM70ExtendedFmModTests()
{
    std::cout << "M7.0 per-voice FM for OSC 4-6" << std::endl;

    // The cell lookup matches every appended FM parameter.
    auto mapped = true;
    for (int source = 0; source < 6; ++source)
        for (int target = 0; target < 6; ++target)
        {
            if (source < 3 && target < 3)
                continue;

            const auto id = source == target ? "fm_fb" + juce::String (source + 1)
                                             : "fm_" + juce::String (source + 1) + "to" + juce::String (target + 1);
            mapped = mapped && Mod::extendedFmCellFor (Mod::destinationForParamId (id)) == source * 6 + target;
        }
    check (mapped, "all 27 OSC 4-6 FM cells map to their destination");
    check (Mod::extendedFmCellFor (Mod::destinationForParamId ("master")) < 0
               && Mod::extendedFmCellFor ((int) Mod::Destination::Fm1to2) < 0,
           "other destinations are not FM cells");

    // Velocity > FM 4>1 with a loud low note and a quiet high note at once:
    // only the loud one should grow sidebands.
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    for (const auto& [id, value] : std::vector<std::pair<const char*, float>> {
             { "master_clip", 0.0f }, { "master", 0.0f }, { "amp_velocity", 0.0f }, { "drift", 0.0f },
             { "osc1_table", 8.0f }, { "osc1_frame", 0.0f }, { "osc1_route", 3.0f }, { "osc1_level", 1.0f },
             { "osc2_on", 0.0f }, { "sub_on", 0.0f }, { "osc1_unison", 1.0f },
             { "amp_attack", 0.001f }, { "amp_decay", 1.0f }, { "amp_sustain", 1.0f }, { "amp_release", 0.05f } })
        setParam (processor, id, value);

    processor.addOscillator (3);
    setParam (processor, "osc4_table", 8.0f);
    setParam (processor, "osc4_frame", 0.0f);
    setParam (processor, "osc4_out", 0.0f);
    setParam (processor, "osc4_level", 1.0f);
    setParam (processor, "mod1_src", (float) Mod::Source::Velocity);
    setParam (processor, "mod1_dst", (float) Mod::destinationForParamId ("fm_4to1"));
    setParam (processor, "mod1_amt", 1.0f);

    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 512);

    for (int block = 0; block < 60; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 127), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 73, (juce::uint8) 4), 0);
        }

        processor.processBlock (buffer, midi);

        for (int i = 0; i < 512; ++i)
            out.push_back (buffer.getSample (0, i));
    }

    const auto quiet = juce::MidiMessage::getMidiNoteInHertz (73);
    const auto loud = juce::MidiMessage::getMidiNoteInHertz (48);
    const auto quietSideband = partialAmplitude (out, 12000, 16384, quiet * 2.0) / juce::jmax (1.0e-9, partialAmplitude (out, 12000, 16384, quiet));
    const auto loudSideband = partialAmplitude (out, 12000, 16384, loud * 2.0) / juce::jmax (1.0e-9, partialAmplitude (out, 12000, 16384, loud));

    check (loudSideband > 0.2 && quietSideband < loudSideband * 0.5,
           "velocity drives FM 4>1 per note (quiet note sideband " + juce::String (quietSideband, 3)
               + ", loud note " + juce::String (loudSideband, 3) + ")");
}
