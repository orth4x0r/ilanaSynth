// The vocoder FX module (type 31). Included by TableTest.cpp;
// ILANA_VOCODER_TEST=1 runs these alone.

#include "dsp/Vocoder.h"

namespace VocoderTests
{
constexpr double sr = 48000.0;
constexpr int block = 256;
constexpr double twoPi = juce::MathConstants<double>::twoPi;

// A voiced modulator whose spectrum has two formants (700 Hz and 2.5 kHz):
// many inharmonic tones with amplitudes on that envelope.
double formantEnvelope (double hz)
{
    const auto bump = [hz] (double centre, double width) { return std::exp (-0.5 * std::pow (std::log2 (hz / centre) / width, 2.0)); };
    return 0.05 + bump (700.0, 0.35) + 0.7 * bump (2500.0, 0.3);
}

std::vector<float> formantModulator (int length)
{
    std::vector<float> out ((size_t) length, 0.0f);
    juce::Random random (11);
    for (auto hz = 130.0; hz < 7600.0; hz *= 1.037)
    {
        const auto amp = 0.06 * formantEnvelope (hz);
        const auto phase = random.nextDouble() * twoPi;
        for (int i = 0; i < length; ++i)
            out[(size_t) i] += (float) (amp * std::sin (twoPi * hz * i / sr + phase));
    }
    return out;
}

std::vector<float> sawCarrier (int length, double hz = 110.0)
{
    std::vector<float> out ((size_t) length);
    auto phase = 0.0;
    for (auto& x : out)
    {
        x = (float) (0.4 * (2.0 * phase - 1.0));
        phase += hz / sr;
        phase -= std::floor (phase);
    }
    return out;
}

// Runs the vocoder over a mono carrier with a modulator (empty: TALK).
std::vector<float> render (const std::vector<float>& carrier, const std::vector<float>& modulator, const Vocoder::Settings& settings)
{
    Vocoder vocoder;
    vocoder.prepare (sr);
    auto out = carrier;
    for (int start = 0; start < (int) out.size(); start += block)
    {
        const auto length = std::min (block, (int) out.size() - start);
        vocoder.process (out.data() + start, nullptr, length, modulator.empty() ? nullptr : modulator.data() + start, settings);
    }
    return out;
}

double toneLevel (const std::vector<float>& x, int from, int length, double hz)
{
    std::complex<double> sum;
    for (int i = 0; i < length; ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (twoPi * i / length);
        sum += (double) x[(size_t) (from + i)] * window * std::polar (1.0, -twoPi * hz * i / sr);
    }
    return std::abs (sum) * 4.0 / length;
}

double peakOf (const std::vector<float>& x, int from = 0)
{
    auto peak = 0.0;
    for (int i = from; i < (int) x.size(); ++i)
        peak = std::max (peak, (double) std::abs (x[(size_t) i]));
    return peak;
}

double correlation (const std::vector<double>& a, const std::vector<double>& b)
{
    double ma = 0.0, mb = 0.0;
    for (size_t i = 0; i < a.size(); ++i) { ma += a[i]; mb += b[i]; }
    ma /= (double) a.size();
    mb /= (double) b.size();
    double sab = 0.0, saa = 0.0, sbb = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        sab += (a[i] - ma) * (b[i] - mb);
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
    }
    return sab / std::sqrt (saa * sbb + 1.0e-30);
}
} // namespace VocoderTests

void runVocoderTests()
{
    using namespace VocoderTests;
    std::cout << "Vocoder" << std::endl;
    const auto length = (int) sr; // one second
    const auto carrier = sawCarrier (length);
    const auto voice = formantModulator (length);
    Vocoder::Settings settings;
    settings.unvoiced = 0.0f;

    // 1. A voiced modulator imposes its spectral envelope on the saw: the
    // carrier's harmonics follow the modulator's formants.
    {
        Vocoder::Settings fine = settings;
        fine.bands = 24;
        const auto out = render (carrier, voice, fine);
        std::vector<double> outDb, wanted, sawDb;
        for (int k = 3; k <= 60; ++k)
        {
            const auto hz = 110.0 * k;
            // Against the saw's own harmonic, so its 1/k slope drops out.
            const auto sawLevel = 20.0 * std::log10 (toneLevel (carrier, 24000, 16384, hz) + 1.0e-6);
            outDb.push_back (20.0 * std::log10 (toneLevel (out, 24000, 16384, hz) + 1.0e-6) - sawLevel);
            sawDb.push_back (sawLevel);
            wanted.push_back (20.0 * std::log10 (formantEnvelope (hz)));
        }
        const auto followed = correlation (outDb, wanted);
        const auto untouched = correlation (sawDb, wanted);
        check (followed > 0.8, "Vocoder: a formant modulator shapes the saw's harmonics (correlation "
                                   + juce::String (followed, 2) + ", the plain saw " + juce::String (untouched, 2) + ")");
        // The level follows the modulator's: about its own level, in dB.
        double outSum = 0.0, modSum = 0.0;
        for (int i = 24000; i < length; ++i)
        {
            outSum += (double) out[(size_t) i] * out[(size_t) i];
            modSum += (double) voice[(size_t) i] * voice[(size_t) i];
        }
        const auto level = 10.0 * std::log10 (outSum / modSum);
        check (level > -12.0 && level < 9.0, "Vocoder: the output is about as loud as the modulator (" + juce::String (level, 1) + " dB)");
    }

    // 2. Silence in the modulator: near silence at full mix, even with UNVOICED up.
    {
        Vocoder::Settings loud = settings;
        loud.unvoiced = 1.0f;
        const auto out = render (carrier, std::vector<float> ((size_t) length, 0.0f), loud);
        check (peakOf (out) < 1.0e-4, "Vocoder: a silent modulator gives silence at full mix (" + juce::String (peakOf (out), 7) + ")");
    }

    // 3. MIX 0 is the carrier, bit for bit.
    {
        Vocoder::Settings dry = settings;
        dry.mix = 0.0f;
        const auto out = render (carrier, voice, dry);
        check (out == carrier, "Vocoder: MIX 0 passes the carrier unchanged");
    }

    // 4. Finite and bounded at the extremes, with noise on both inputs.
    {
        juce::Random random (3);
        std::vector<float> noiseCarrier ((size_t) length), noiseModulator ((size_t) length);
        for (int i = 0; i < length; ++i)
        {
            noiseCarrier[(size_t) i] = random.nextFloat() - 0.5f;
            noiseModulator[(size_t) i] = random.nextFloat() * 2.0f - 1.0f;
        }
        auto worst = 0.0;
        auto finite = true;
        for (const auto bands : { 8, 24 })
            for (const auto width : { 0.25f, 4.0f })
                for (const auto formant : { -12.0f, 12.0f })
                {
                    Vocoder::Settings s;
                    s.bands = bands;
                    s.width = width;
                    s.formantSemitones = formant;
                    s.unvoiced = 1.0f;
                    s.levelDb = 12.0f;
                    s.attackMs = 0.5f;
                    s.releaseMs = 500.0f;
                    for (const auto& mod : { noiseModulator, std::vector<float>() })
                    {
                        const auto out = render (noiseCarrier, mod, s);
                        for (const auto x : out)
                            finite = finite && std::isfinite (x);
                        worst = std::max (worst, peakOf (out));
                    }
                }
        check (finite && worst < 12.0, "Vocoder: extreme settings stay finite and bounded (peak " + juce::String (worst, 2) + ")");
    }

    // 5. FORMANT +12 st moves the carrier's bands up an octave: a 1 kHz
    // modulator tone now lights the band that carries 2 kHz.
    {
        const auto carrier2 = sawCarrier (length, 100.0);
        std::vector<float> tone ((size_t) length);
        for (int i = 0; i < length; ++i)
            tone[(size_t) i] = (float) (0.3 * std::sin (twoPi * 1000.0 * i / sr));
        Vocoder::Settings s = settings;
        s.width = 2.0f;
        const auto plain = render (carrier2, tone, s);
        s.formantSemitones = 12.0f;
        const auto shifted = render (carrier2, tone, s);
        const auto ratio = [] (const std::vector<float>& x)
        {
            return toneLevel (x, 24000, 16384, 2000.0) / (toneLevel (x, 24000, 16384, 1000.0) + 1.0e-9);
        };
        check (ratio (shifted) > 4.0 * ratio (plain), "Vocoder: FORMANT +12 moves the output up an octave (2k/1k "
                                                          + juce::String (ratio (plain), 2) + " -> " + juce::String (ratio (shifted), 2) + ")");
    }

    // 6. UNVOICED: a hissy modulator over a dull carrier brings out the highs.
    {
        const auto dull = [&]
        {
            std::vector<float> out ((size_t) length);
            for (int i = 0; i < length; ++i)
                out[(size_t) i] = (float) (0.4 * std::sin (twoPi * 110.0 * i / sr));
            return out;
        }();
        juce::Random random (9);
        std::vector<float> hiss ((size_t) length);
        auto previous = 0.0f;
        for (auto& x : hiss)
        {
            const auto white = random.nextFloat() * 2.0f - 1.0f;
            x = 0.3f * (white - previous); // first difference: high-passed
            previous = white;
        }
        Vocoder::Settings s = settings;
        const auto without = render (dull, hiss, s);
        s.unvoiced = 1.0f;
        const auto with = render (dull, hiss, s);
        double energyWithout = 0.0, energyWith = 0.0;
        for (const auto hz : { 3500.0, 4500.0, 5500.0, 6500.0 })
        {
            energyWithout += toneLevel (without, 24000, 16384, hz);
            energyWith += toneLevel (with, 24000, 16384, hz);
        }
        check (energyWith > 8.0 * energyWithout, "Vocoder: UNVOICED lets sibilance through (" + juce::String (energyWithout, 5)
                                                     + " -> " + juce::String (energyWith, 5) + ")");
    }

    // 7. TALK (no modulator): the saw is filtered by a moving vowel; not silent, not the saw.
    {
        Vocoder::Settings s = settings;
        s.talkRate = 2.0f;
        const auto out = render (carrier, {}, s);
        auto difference = 0.0;
        for (size_t i = 0; i < out.size(); ++i)
            difference = std::max (difference, (double) std::abs (out[i] - carrier[i]));
        const auto early = toneLevel (out, 4096, 8192, 110.0 * 7);
        const auto late = toneLevel (out, 16384 + 8192, 8192, 110.0 * 7);
        check (peakOf (out, 24000) > 0.02 && difference > 0.05, "Vocoder: TALK does something without an input (peak "
                                                                    + juce::String (peakOf (out, 24000), 3) + ")");
        check (std::abs (early - late) > 0.1 * std::max (early, late), "Vocoder: TALK moves over time (" + juce::String (early, 4)
                                                                             + " vs " + juce::String (late, 4) + ")");
    }

    // 8. In a slot: assignFxSlot, state round trip, the module changes a played note,
    // and a patch without it is unchanged (the module is skipped).
    {
        IlanaSynthAudioProcessor processor;
        processor.assignFxSlot (1, 31);
        auto* slot = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("fx_slot1"));
        check (slot != nullptr && slot->getIndex() == 31 && slot->getCurrentChoiceName() == "Vocoder"
                   && IlanaSynthAudioProcessor::numFxTypes == 31,
               "Vocoder: assignFxSlot puts type 31, Vocoder, in the slot");
        setParam (processor, "fx_voc_bands", 20.0f);
        setParam (processor, "fx_voc_formant", -5.0f);
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check ((int) restored.apvts.getRawParameterValue ("fx_slot1")->load() == 31
                   && (int) restored.apvts.getRawParameterValue ("fx_voc_bands")->load() == 20
                   && std::abs (restored.apvts.getRawParameterValue ("fx_voc_formant")->load() + 5.0f) < 1.0e-6f,
               "Vocoder: the slot and its parameters survive a state round trip");
        check (! dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("fx_voc_source"))->isAutomatable(),
               "Vocoder: the modulator choice is not host-automatable");

        const auto wet = AirwindowsTests::renderNote (processor);
        IlanaSynthAudioProcessor dryProcessor;
        const auto dry = AirwindowsTests::renderNote (dryProcessor);
        auto difference = 0.0f;
        auto finite = true;
        for (size_t i = 0; i < wet.size(); ++i)
        {
            finite = finite && std::isfinite (wet[i]);
            difference = std::max (difference, std::abs (wet[i] - dry[i]));
        }
        check (finite && difference > 1.0e-3f, "Vocoder: in a slot it changes a played note (TALK, difference "
                                                   + juce::String (difference, 4) + ")");

        IlanaSynthAudioProcessor again;
        const auto repeat = AirwindowsTests::renderNote (again);
        check (repeat == dry, "Vocoder: a patch without the module renders as before (bit for bit)");
    }
}
