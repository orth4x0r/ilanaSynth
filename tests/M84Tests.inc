// M8.4: the new filter models, checked against the responses of the
// circuits they copy. Included by TableTest.cpp.

namespace M84
{
constexpr double sr = 48000.0;

// Steady-state gain (dB) of a filter at hz, with a quiet sine (small-signal).
template <typename Filter>
double gainAt (Filter filter, double hz, float level = 0.01f)
{
    filter.reset();
    const auto samples = (int) juce::jmax (sr * 0.25, 40.0 * sr / hz);
    auto inSum = 0.0, outSum = 0.0;
    for (int i = 0; i < samples; ++i)
    {
        const auto x = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sr);
        const auto y = filter.process (x);
        if (i > samples / 2)
        {
            inSum += (double) x * x;
            outSum += (double) y * y;
        }
    }
    return 10.0 * std::log10 (outSum / juce::jmax (1.0e-30, inSum) + 1.0e-30);
}

template <typename Filter>
std::vector<double> sweep (const Filter& filter, double cutoff, const std::vector<double>& ratios)
{
    std::vector<double> out;
    for (auto r : ratios)
        out.push_back (gainAt (filter, cutoff * r));
    return out;
}

// Slope in dB per octave between two frequencies (ratio 2 apart).
double slope (double a, double b) { return b - a; }

template <typename Filter>
bool stableWhenHot (Filter filter)
{
    filter.reset();
    juce::Random random (84);
    for (int i = 0; i < 48000; ++i)
    {
        const auto y = filter.process ((random.nextFloat() * 2.0f - 1.0f) * 10.0f);
        if (! std::isfinite (y) || std::abs (y) > 50.0f)
            return false;
    }
    return true;
}

template <typename Filter>
double ringAfterImpulse (Filter filter)
{
    // Energy 200-400 ms after an impulse, relative to the first 20 ms: a
    // self-oscillating filter keeps ringing.
    filter.reset();
    auto early = 0.0, late = 0.0;
    for (int i = 0; i < (int) (0.4 * sr); ++i)
    {
        const auto y = (double) filter.process (i == 0 ? 0.5f : 0.0f);
        if (i < (int) (0.02 * sr)) early += y * y;
        if (i > (int) (0.2 * sr)) late += y * y;
    }
    return 10.0 * std::log10 ((late / 10.0) / (early + 1.0e-30) + 1.0e-30);
}

void runTests()
{
    using namespace Filters2;
    const auto fc = 1000.0;

    // 4-pole low-passes: flat below, -24 dB/octave above (Moog, CEM3320).
    {
        DriveLadder ladder;
        ladder.set (sr, fc, 0.0, false);
        OtaFilter ota;
        ota.set (sr, fc, 0.0, false);
        for (const auto& [name, g] : { std::pair<const char*, std::vector<double>> { "Ladder Drive", sweep (ladder, fc, { 0.125, 4.0, 8.0 }) },
                                       { "OTA LP", sweep (ota, fc, { 0.125, 4.0, 8.0 }) } })
        {
            const auto s = slope (g[1], g[2]);
            check (std::abs (g[0]) < 3.0 && s < -18.0 && s > -30.0, juce::String (name) + ": flat passband (" + juce::String (g[0], 1)
                                                                       + " dB), " + juce::String (s, 1) + " dB/octave above the cutoff (4-pole: -24)");
        }
        ladder.set (sr, fc, 1.0, false);
        check (ringAfterImpulse (ladder) > -40.0, "Ladder Drive self-oscillates at full resonance");
    }

    // 2-pole state-variables: SEM low-pass -12 dB/oct, notch in the middle
    // of its mode sweep, high-pass +12 dB/oct.
    {
        SemFilter sem;
        sem.set (sr, fc, 0.0, 0.0f);
        const auto lp = sweep (sem, fc, { 0.125, 4.0, 8.0 });
        sem.set (sr, fc, 0.0, 1.0f);
        const auto hp = sweep (sem, fc, { 0.125, 0.25, 8.0 });
        sem.set (sr, fc, 0.3, 0.5f);
        const auto notch = gainAt (sem, fc);
        const auto around = 0.5 * (gainAt (sem, fc / 8.0) + gainAt (sem, fc * 8.0));
        check (std::abs (slope (lp[1], lp[2]) + 12.0) < 3.0 && std::abs (slope (hp[0], hp[1]) - 12.0) < 3.0 && notch < around - 15.0,
               "SEM: LP " + juce::String (slope (lp[1], lp[2]), 1) + " dB/oct, HP +" + juce::String (slope (hp[0], hp[1]), 1)
                   + " dB/oct, the notch " + juce::String (notch - around, 1) + " dB at the cutoff");
    }

    // Steiner-Parker: the same 2-pole loop through its low and high inputs.
    {
        SteinerParker sp;
        sp.set (sr, fc, 0.0, 0.0f);
        const auto lp = sweep (sp, fc, { 0.125, 4.0, 8.0 });
        sp.set (sr, fc, 0.0, 1.0f);
        const auto hp = sweep (sp, fc, { 0.125, 0.25, 8.0 });
        check (std::abs (slope (lp[1], lp[2]) + 12.0) < 3.0 && std::abs (slope (hp[0], hp[1]) - 12.0) < 3.0,
               "Steiner-Parker: low input " + juce::String (slope (lp[1], lp[2]), 1) + " dB/oct, high input +"
                   + juce::String (slope (hp[0], hp[1]), 1) + " dB/oct");
        sp.set (sr, fc, 1.0, 0.0f);
        check (stableWhenHot (sp), "Steiner-Parker at full resonance stays bounded with a hot input");
    }

    // MS-20 high-pass: +12 dB/oct below the cutoff.
    {
        Ms20HighPass hp;
        hp.set (sr, fc, 0.0);
        const auto g = sweep (hp, fc, { 0.125, 0.25, 8.0 });
        check (std::abs (slope (g[0], g[1]) - 12.0) < 3.0 && std::abs (g[2]) < 3.0,
               "MS-20 HP: +" + juce::String (slope (g[0], g[1]), 1) + " dB/oct below the cutoff, " + juce::String (g[2], 1) + " dB above");
    }

    // Band-passes peak at the cutoff.
    {
        DriveLadder ladder;
        ladder.set (sr, fc, 0.3, true);
        OtaFilter ota;
        ota.set (sr, fc, 0.3, true);
        for (const auto& [name, g] : { std::pair<const char*, std::vector<double>> { "Ladder BP", sweep (ladder, fc, { 0.125, 1.0, 8.0 }) },
                                       { "OTA BP", sweep (ota, fc, { 0.125, 1.0, 8.0 }) } })
            check (g[1] > g[0] + 12.0 && g[1] > g[2] + 12.0, juce::String (name) + " peaks at the cutoff (" + juce::String (g[0], 1) + " / "
                                                                  + juce::String (g[1], 1) + " / " + juce::String (g[2], 1) + " dB)");
    }

    // Phaser notch: notches as deep as an analogue phaser's (> 20 dB).
    {
        PhaserNotch phaser;
        phaser.set (sr, fc, 0.0, 0.3f);
        auto deepest = 0.0;
        for (double r = 0.2; r < 5.0; r *= 1.05)
            deepest = juce::jmin (deepest, gainAt (phaser, fc * r));
        check (deepest < -20.0, "Phaser Notch cuts " + juce::String (deepest, 1) + " dB at its notch");
    }

    // Vowels: the formants where the tables put them.
    {
        VowelFilter vowel;
        vowel.set (sr, 1000.0, 0.5, 0.0f, false); // A: 600, 1040 Hz
        const auto peak600 = gainAt (vowel, 600.0), dip = gainAt (vowel, 800.0), peak1040 = gainAt (vowel, 1040.0);
        check (peak600 > dip + 6.0 && peak1040 > dip, "Vowel A has its formants at 600 and 1040 Hz ("
                                                          + juce::String (peak600, 1) + " / " + juce::String (dip, 1) + " / " + juce::String (peak1040, 1) + " dB)");
        vowel.set (sr, 1000.0, 0.5, 1.0f, false); // U: 350 Hz
        check (gainAt (vowel, 350.0) > gainAt (vowel, 1000.0) + 15.0, "Vowel U is dark (350 Hz formant)");
        vowel.set (sr, 1000.0, 0.5, 0.0f, true); // i: 310, 2790 Hz
        check (gainAt (vowel, 2790.0) > gainAt (vowel, 1500.0) + 6.0, "Talking starts on /i/ (2790 Hz formant)");
    }

    // Twin Peak: its second peak moves up with morph.
    {
        TwinPeak twin;
        twin.set (sr, fc, 0.8, 0.5f); // second peak an octave up
        check (gainAt (twin, 2000.0) > gainAt (twin, 1400.0) + 6.0 && gainAt (twin, 1000.0) > gainAt (twin, 1400.0) + 6.0,
               "Twin Peak peaks at the cutoff and an octave above");
    }

    // Every model survives full resonance with a hot input.
    {
        DriveLadder a; a.set (sr, fc, 1.0, false);
        DriveLadder b; b.set (sr, fc, 1.0, true);
        SemFilter c; c.set (sr, fc, 1.0, 0.3f);
        OtaFilter d; d.set (sr, fc, 1.0, false);
        OtaFilter e; e.set (sr, fc, 1.0, true);
        Ms20HighPass f; f.set (sr, fc, 1.0);
        PhaserNotch g; g.set (sr, fc, 1.0, 1.0f);
        VowelFilter h; h.set (sr, fc, 1.0, 0.5f, true);
        TwinPeak i; i.set (sr, fc, 1.0, 1.0f);
        check (stableWhenHot (a) && stableWhenHot (b) && stableWhenHot (c) && stableWhenHot (d) && stableWhenHot (e)
                   && stableWhenHot (f) && stableWhenHot (g) && stableWhenHot (h) && stableWhenHot (i),
               "every new filter model stays bounded at full resonance with a +-10 input");
    }
}
} // namespace M84

// Through FilterUnit and the voice: every new type is reachable by index,
// keeps its name, and a note through it at full resonance stays finite.
void runM84FilterTests()
{
    M84::runTests();

    const auto names = FilterType::getNames();
    check (names.size() == FilterType::Count && names[12] == "Ladder BP" && names[24] == "Twin Peak" && names[11] == "Morph",
           "the new filter models are appended after Morph (12..24)");

    auto finite = true, audible = true;
    for (int type = FilterType::LadderBand; type < FilterType::Count; ++type)
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);
        setParam (processor, "f1_type", (float) type);
        setParam (processor, "f1_cutoff", 900.0f);
        setParam (processor, "f1_reso", 1.0f);
        setParam (processor, "f1_drive", 8.0f);
        setParam (processor, "f1_morph", 0.7f);
        juce::AudioBuffer<float> buffer (2, 256);
        auto peak = 0.0f;
        for (int block = 0; block < 190; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                for (int note : { 36, 48, 60, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                {
                    const auto v = buffer.getSample (ch, i);
                    finite = finite && std::isfinite (v);
                    peak = juce::jmax (peak, std::abs (v));
                }
        }
        if (peak < 0.001f || peak > 20.0f)
        {
            audible = false;
            std::cout << "INFO: filter model " << names[type] << " peak " << peak << std::endl;
        }
    }
    check (finite && audible, "every new model plays a loud chord at full resonance and drive, finite and bounded");
}
