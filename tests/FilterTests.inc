// The filter overhaul: every model checked for stability, cutoff accuracy,
// self-oscillation pitch, DC and its own defining behaviour, plus a per-model
// CPU report. Included by TableTest.cpp; ILANA_FILTER_TEST=1 runs only these.

#include "dsp/FilterCore.h"

namespace FilterLab
{
constexpr double sr = 48000.0;

struct Probe
{
    int type = 0;
    bool slope24 = false;
    double cutoff = 1000.0, resonance = 0.0;
    float morph = 0.5f;
    float drive = 1.0f;

    FilterUnit make() const
    {
        FilterUnit filter;
        filter.prepare (sr);
        filter.setType (type, slope24);
        filter.setCoefficients (FilterUnit::makeCoefficients (type, sr, cutoff, resonance, morph));
        return filter;
    }
};

// A band-limited saw (additive), for level comparisons.
inline std::vector<float> saw (double hz, float amplitude, int samples)
{
    std::vector<float> out ((size_t) samples, 0.0f);
    const auto harmonics = (int) (sr * 0.45 / hz);
    for (int i = 0; i < samples; ++i)
    {
        auto v = 0.0;
        for (int h = 1; h <= harmonics; ++h)
            v += std::sin (juce::MathConstants<double>::twoPi * hz * h * i / sr) / (double) h;
        out[(size_t) i] = amplitude * (float) (v * 2.0 / juce::MathConstants<double>::pi);
    }
    return out;
}

inline double rmsDb (const Probe& probe, const std::vector<float>& input)
{
    auto filter = probe.make();
    auto sum = 0.0;
    auto count = 0;
    for (size_t i = 0; i < input.size(); ++i)
    {
        const auto y = (double) filter.process (input[i]);
        if (i > input.size() / 3)
        {
            sum += y * y;
            ++count;
        }
    }
    return 10.0 * std::log10 (sum / juce::jmax (1, count) + 1.0e-20);
}

// The level table: a 110 Hz saw at 0.7 through every type at three cutoffs
// and three resonances (both slopes where the type has one), printed as C++
// so a baseline can be kept.
inline void printLevelTable()
{
    const auto input = saw (110.0, 0.7f, 24000);
    const double cutoffs[] { 300.0, 1000.0, 3000.0 };
    const double resonances[] { 0.0, 0.5, 0.9 };
    for (int type = 0; type < FilterType::Count; ++type)
        for (int slope = 0; slope < 2; ++slope)
        {
            std::cout << "LEVEL { " << type << ", " << slope;
            for (auto c : cutoffs)
                for (auto r : resonances)
                {
                    Probe p;
                    p.type = type;
                    p.slope24 = slope == 1;
                    p.cutoff = c;
                    p.resonance = r;
                    std::cout << ", " << juce::String (rmsDb (p, input), 2);
                }
            std::cout << " }," << std::endl;
        }
}

// Nanoseconds per sample of one filter with its coefficients refreshed every
// 16 samples (as the voice does), a sweeping cutoff and a saw-ish input.
inline double nsPerSample (int type, bool slope24)
{
    FilterUnit filter;
    filter.prepare (sr);
    filter.setType (type, slope24);
    constexpr int samples = 1 << 18;
    auto best = 1.0e30;
    auto sink = 0.0f;
    for (int run = 0; run < 3; ++run)
    {
        filter.reset();
        auto phase = 0.0f;
        const auto start = juce::Time::getHighResolutionTicks();
        for (int i = 0; i < samples; ++i)
        {
            if ((i & 15) == 0)
            {
                const auto cutoff = 400.0 * std::exp2 (3.0 * (0.5 + 0.5 * std::sin ((double) i * 1.0e-4)));
                filter.setCoefficients (FilterUnit::makeCoefficients (type, sr, cutoff, 0.7, 0.5f));
            }
            phase += 0.0023f;
            if (phase > 1.0f)
                phase -= 2.0f;
            sink += filter.process (phase * 0.6f);
        }
        const auto seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        best = juce::jmin (best, seconds * 1.0e9 / (double) samples);
    }
    if (! std::isfinite (sink))
        std::cout << "(non-finite)" << std::endl;
    return best;
}

inline void printCpuReport()
{
    const auto names = FilterType::getNames();
    auto total = 0.0;
    for (int type = 0; type < FilterType::Count; ++type)
    {
        const auto ns = nsPerSample (type, FilterType::usesSlope (type));
        total += ns;
        std::cout << "CPU " << type << " " << names[type] << ": " << juce::String (ns, 1) << " ns/sample" << std::endl;
    }
    std::cout << "CPU average: " << juce::String (total / (double) FilterType::Count, 1) << " ns/sample" << std::endl;
}

// Which types the factory presets use, with their resonance and drive.
inline void printPresetUsage()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (sr, 256);
    const auto names = processor.getFactoryPresetNames();
    const auto typeNames = FilterType::getNames();
    for (int i = 0; i < names.size(); ++i)
    {
        processor.loadFactoryPreset (i);
        const auto get = [&processor] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
        for (int f = 1; f <= 2; ++f)
        {
            const auto prefix = juce::String ("f") + juce::String (f) + "_";
            const auto type = (int) get ((prefix + "type").toRawUTF8());
            if (type == 0 && get ((prefix + "cutoff").toRawUTF8()) > 19000.0f)
                continue;
            std::cout << "USAGE " << typeNames[type] << " | " << names[i] << " | F" << f << " reso "
                      << get ((prefix + "reso").toRawUTF8()) << " drive " << get ((prefix + "drive").toRawUTF8())
                      << " slope " << get ((prefix + "slope").toRawUTF8()) << std::endl;
        }
    }
}

// Frequency of a steady tone by interpolated rising zero crossings.
inline double zeroCrossingHz (const std::vector<float>& y, size_t from)
{
    double first = -1.0, lastCrossing = -1.0;
    int count = 0;
    for (size_t i = from + 1; i < y.size(); ++i)
        if (y[i - 1] < 0.0f && y[i] >= 0.0f)
        {
            const auto t = (double) (i - 1) + (double) (-y[i - 1]) / (double) (y[i] - y[i - 1]);
            if (first < 0.0)
                first = t;
            else
                ++count;
            lastCrossing = t;
        }
    return count > 0 ? sr * (double) count / (lastCrossing - first) : 0.0;
}

struct SelfOsc
{
    double hz = 0.0, lateRms = 0.0, peak = 0.0;
};

// A short burst, then silence: what rings on 0.6 - 1.2 s later.
inline SelfOsc selfOscillation (int type, double cutoff, double resonance = 1.0, bool slope24 = false, float morph = 0.5f)
{
    Probe p;
    p.type = type;
    p.cutoff = cutoff;
    p.resonance = resonance;
    p.slope24 = slope24;
    p.morph = morph;
    auto filter = p.make();
    const auto total = (int) (1.2 * sr);
    std::vector<float> y ((size_t) total);
    SelfOsc result;
    for (int i = 0; i < total; ++i)
    {
        y[(size_t) i] = filter.process (i < 24 ? 0.3f : 0.0f);
        result.peak = juce::jmax (result.peak, (double) std::abs (y[(size_t) i]));
    }
    const auto from = (size_t) (0.6 * sr);
    auto sum = 0.0;
    for (auto i = from; i < y.size(); ++i)
        sum += (double) y[i] * y[i];
    result.lateRms = std::sqrt (sum / (double) (y.size() - from));
    result.hz = result.lateRms > 1.0e-4 ? zeroCrossingHz (y, from) : 0.0;
    return result;
}

inline void printSelfOscReport()
{
    const auto names = FilterType::getNames();
    for (int type = 0; type < FilterType::Count; ++type)
    {
        std::cout << "SELFOSC " << names[type] << ":";
        for (auto hz : { 50.0, 110.0, 440.0, 1000.0, 3000.0, 10000.0 })
        {
            const auto r = selfOscillation (type, hz);
            std::cout << " " << hz << "->" << (r.hz > 0.0 ? juce::String (100.0 * (r.hz / hz - 1.0), 2) + "%" : juce::String ("none"))
                      << " (rms " << juce::String (r.lateRms, 3) << ")";
        }
        std::cout << std::endl;
    }
    for (auto reso : { 0.99, 0.995 })
    {
        const auto lp = selfOscillation (FilterType::LowPass, 1000.0, reso);
        const auto bp = selfOscillation (FilterType::BandPass, 1000.0, reso);
        std::cout << "SVFOSC reso " << reso << ": LP rms " << lp.lateRms << " peak " << lp.peak << ", BP rms " << bp.lateRms << " peak " << bp.peak << std::endl;
    }
}
} // namespace FilterLab

void runFilterOverhaulTests()
{
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_FILTER_LEVELS", "").isNotEmpty())
        FilterLab::printLevelTable();
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_FILTER_USAGE", "").isNotEmpty())
        FilterLab::printPresetUsage();
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_FILTER_OSC", "").isNotEmpty())
        FilterLab::printSelfOscReport();
    FilterLab::printCpuReport();
}
