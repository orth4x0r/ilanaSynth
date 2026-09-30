// The filter overhaul: every model checked for stability, cutoff accuracy,
// self-oscillation pitch, DC and its own defining behaviour, plus a CPU check
// (ILANA_FILTER_CPU=1 prints the per-model report). Included by
// TableTest.cpp; ILANA_FILTER_TEST=1 runs only these.

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
        filter.setDrive (drive);
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

namespace FilterLab
{
// The level table made on Linux before any model was rebuilt (c911267):
// { type, slope24, rms dB at 300 / 1000 / 3000 Hz x resonance 0 / 0.5 / 0.9 }.
struct LevelRow { int type, slope; double db[9]; };
const LevelRow levelsBefore[] {
    { 0, 0, -10.43, -8.08, -4.45, -8.59, -7.91, -6.15, -8.12, -7.89, -7.24 },
    { 0, 1, -11.89, -7.51, 2.84, -8.94, -7.71, 1.42, -8.22, -7.82, -2.64 },
    { 1, 0, -16.65, -13.03, -6.38, -21.07, -17.90, -10.76, -25.71, -22.66, -15.64 },
    { 1, 1, -24.54, -15.52, 3.09, -29.79, -20.76, 0.43, -34.66, -25.63, -4.66 },
    { 2, 0, -15.59, -12.55, -5.95, -21.00, -17.87, -10.75, -26.26, -22.96, -15.70 },
    { 2, 1, -17.68, -12.55, 4.63, -23.18, -17.87, 0.56, -28.67, -22.98, -4.50 },
    { 3, 0, -11.24, -9.50, -8.15, -8.84, -8.36, -7.99, -8.20, -8.05, -7.93 },
    { 3, 1, -13.50, -10.44, -8.33, -9.36, -8.61, -8.04, -8.36, -8.13, -7.94 },
    { 4, 0, -11.04, -9.03, -8.60, -9.30, -10.92, -10.65, -8.87, -11.39, -11.84 },
    { 4, 1, -12.45, -11.27, -11.42, -9.62, -11.68, -12.13, -8.97, -11.64, -12.32 },
    { 5, 0, -16.81, -16.23, -13.68, -22.31, -21.66, -16.37, -27.64, -26.89, -21.53 },
    { 5, 1, -18.96, -19.43, -17.09, -24.52, -24.94, -21.34, -30.10, -30.38, -26.72 },
    { 6, 0, -12.24, -12.37, -12.19, -10.20, -11.38, -11.73, -9.74, -10.95, -11.46 },
    { 6, 1, -13.03, -13.52, -13.58, -10.63, -11.91, -12.46, -10.11, -11.33, -11.92 },
    { 7, 0, -11.12, -9.69, -8.91, -9.54, -9.17, -8.97, -9.14, -8.94, -8.81 },
    { 7, 1, -11.86, -10.52, -9.89, -9.71, -9.39, -9.23, -9.20, -9.01, -8.89 },
    { 8, 0, -7.90, -11.44, -13.94, -7.90, -8.27, -9.75, -7.90, -6.09, -4.11 },
    { 8, 1, -7.90, -11.44, -13.94, -7.90, -8.27, -9.75, -7.90, -6.09, -4.11 },
    { 9, 0, -7.90, -8.60, -9.71, -7.90, -11.23, -12.94, -7.90, -12.70, -15.01 },
    { 9, 1, -7.90, -8.60, -9.71, -7.90, -11.23, -12.94, -7.90, -12.70, -15.01 },
    { 10, 0, -6.81, -9.26, -16.16, -9.11, -11.60, -17.80, -11.27, -13.78, -18.09 },
    { 10, 1, -6.81, -9.26, -16.16, -9.11, -11.60, -17.80, -11.27, -13.78, -18.09 },
    { 11, 0, -16.65, -13.03, -6.38, -21.07, -17.90, -10.76, -25.71, -22.66, -15.64 },
    { 11, 1, -24.54, -15.52, 3.09, -29.79, -20.76, 0.43, -34.66, -25.63, -4.66 },
    { 12, 0, -9.49, -4.21, -2.60, -13.47, -10.14, -6.47, -18.32, -15.58, -12.31 },
    { 12, 1, -9.49, -4.21, -2.60, -13.47, -10.14, -6.47, -18.32, -15.58, -12.31 },
    { 13, 0, -12.27, -10.51, -10.19, -9.11, -11.28, -11.48, -8.29, -11.42, -12.10 },
    { 13, 1, -12.27, -10.51, -10.19, -9.11, -11.28, -11.48, -8.29, -11.42, -12.10 },
    { 14, 0, -11.80, -10.17, -8.95, -9.41, -8.99, -8.67, -8.81, -8.68, -8.58 },
    { 14, 1, -11.80, -10.17, -8.95, -9.41, -8.99, -8.67, -8.81, -8.68, -8.58 },
    { 15, 0, -13.78, -14.21, -14.36, -11.39, -14.04, -15.00, -10.88, -13.85, -14.67 },
    { 15, 1, -13.78, -14.21, -14.36, -11.39, -14.04, -15.00, -10.88, -13.85, -14.67 },
    { 16, 0, -20.70, -16.54, -14.18, -26.37, -22.40, -20.18, -31.25, -27.21, -24.97 },
    { 16, 1, -20.70, -16.54, -14.18, -26.37, -22.40, -20.18, -31.25, -27.21, -24.97 },
    { 17, 0, -18.28, -16.47, -13.26, -24.01, -22.16, -18.45, -29.00, -27.02, -22.56 },
    { 17, 1, -18.28, -16.47, -13.26, -24.01, -22.16, -18.45, -29.00, -27.02, -22.56 },
    { 18, 0, -9.52, -12.93, -23.39, -13.67, -17.03, -26.25, -18.20, -21.35, -29.41 },
    { 18, 1, -9.52, -12.93, -23.39, -13.67, -17.03, -26.25, -18.20, -21.35, -29.41 },
    { 19, 0, -13.91, -13.50, -12.99, -10.35, -10.95, -12.07, -8.67, -7.28, -6.01 },
    { 19, 1, -13.91, -13.50, -12.99, -10.35, -10.95, -12.07, -8.67, -7.28, -6.01 },
    { 20, 0, -7.90, -11.60, -14.37, -7.90, -8.57, -10.73, -7.90, -6.38, -4.92 },
    { 20, 1, -7.90, -11.60, -14.37, -7.90, -8.57, -10.73, -7.90, -6.38, -4.92 },
    { 21, 0, -7.90, -7.90, -7.90, -7.90, -7.90, -7.90, -7.90, -7.90, -7.90 },
    { 21, 1, -7.90, -7.90, -7.90, -7.90, -7.90, -7.90, -7.90, -7.90, -7.90 },
    { 22, 0, -9.25, -12.07, -16.98, -11.85, -14.09, -18.28, -14.27, -15.94, -17.50 },
    { 22, 1, -9.25, -12.07, -16.98, -11.85, -14.09, -18.28, -14.27, -15.94, -17.50 },
    { 23, 0, -9.48, -11.90, -15.66, -12.24, -13.88, -15.49, -14.79, -16.93, -19.89 },
    { 23, 1, -9.48, -11.90, -15.66, -12.24, -13.88, -15.49, -14.79, -16.93, -19.89 },
    { 24, 0, -10.14, -12.78, -14.13, -13.80, -15.69, -15.96, -16.31, -17.30, -17.42 },
    { 24, 1, -10.14, -12.78, -14.13, -13.80, -15.69, -15.96, -16.31, -17.30, -17.42 },
};

inline void checkLevels()
{
    // Tone may change, level may not: every rebuilt model stays within
    // 1.25 dB of its old row up to resonance 0.5 and within 2 dB at 0.9 (the
    // old models clipped their states there). The presets' own levels are
    // held to 1 dB by the fingerprint comparison.
    const auto input = saw (110.0, 0.7f, 24000);
    const double cutoffs[] { 300.0, 1000.0, 3000.0 };
    const double resonances[] { 0.0, 0.5, 0.9 };
    auto worst = 0.0;
    juce::String where;
    for (const auto& row : levelsBefore)
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 3; ++r)
            {
                Probe p;
                p.type = row.type;
                p.slope24 = row.slope == 1;
                p.cutoff = cutoffs[c];
                p.resonance = resonances[r];
                const auto delta = rmsDb (p, input) - row.db[c * 3 + r];
                const auto allowed = r < 2 ? 1.25 : 2.0;
                if (std::abs (delta) / allowed > worst)
                {
                    worst = std::abs (delta) / allowed;
                    where = FilterType::getNames()[row.type] + " " + juce::String (cutoffs[c]) + " Hz reso " + juce::String (resonances[r])
                            + ": " + juce::String (delta, 2) + " dB";
                }
            }
    check (worst <= 1.0, "every rebuilt filter keeps its old level (worst: " + where + ")");
}

// Steady output of a sine through a filter, dB relative to the input.
inline double sineGainDb (int type, double cutoff, double resonance, double hz, float morph = 0.5f)
{
    Probe p;
    p.type = type;
    p.cutoff = cutoff;
    p.resonance = resonance;
    p.morph = morph;
    auto filter = p.make();
    const auto samples = 19200;
    auto sum = 0.0;
    for (int i = 0; i < samples; ++i)
    {
        const auto y = (double) filter.process ((float) (0.01 * std::sin (juce::MathConstants<double>::twoPi * hz * i / sr)));
        if (i >= samples / 2)
            sum += y * y;
    }
    return 10.0 * std::log10 (sum / (samples / 2) / (0.01 * 0.01 / 2.0) + 1.0e-20);
}

// The strongest frequency of an impulse response within +-3 % of 'around'.
inline double ringHz (int type, double cutoff, double resonance, double around, float morph = 0.0f)
{
    Probe p;
    p.type = type;
    p.cutoff = cutoff;
    p.resonance = resonance;
    p.morph = morph;
    auto filter = p.make();
    std::vector<double> y (24000);
    for (size_t i = 0; i < y.size(); ++i)
        y[i] = filter.process (i == 0 ? 1.0f : 0.0f);
    auto best = 0.0, bestHz = 0.0;
    for (auto hz = around * 0.97; hz <= around * 1.03; hz += around * 0.0002)
    {
        std::complex<double> sum;
        for (size_t i = 0; i < y.size(); ++i)
            sum += y[i] * std::exp (std::complex<double> (0.0, -juce::MathConstants<double>::twoPi * hz * (double) i / sr));
        if (std::norm (sum) > best)
        {
            best = std::norm (sum);
            bestHz = hz;
        }
    }
    return bestHz;
}

inline void checkSelfOscillation()
{
    using namespace FilterType;
    // Every resonant model self-oscillates at its cutoff at full resonance
    // (within 0.5 %, under 9 cents), so it plays in tune with key tracking.
    for (const auto type : { Morph, LadderLow, LadderHigh, DiodeLow, Ms20Low, LadderBand, LadderDrive,
                             OtaLow, OtaBand, Ms20High, Steiner, Acid303, MoogDrive })
    {
        auto worst = 0.0, quietest = 1.0;
        for (const auto hz : { 110.0, 440.0, 1000.0, 3000.0 })
        {
            const auto r = selfOscillation (type, hz, 1.0, false, 0.5f);
            worst = juce::jmax (worst, r.hz > 0.0 ? std::abs (r.hz / hz - 1.0) : 1.0);
            quietest = juce::jmin (quietest, r.lateRms);
        }
        check (worst < 0.005 && quietest > 1.0e-3,
               getNames()[type] + " self-oscillates at the cutoff (worst " + juce::String (100.0 * worst, 2) + " %, quietest rms "
                   + juce::String (quietest, 4) + ")");
    }

    // Low / Band / High Pass are Airwindows' Y filters: they never oscillate
    // on their own, but at full resonance they ring at the cutoff.
    for (const auto type : { LowPass, BandPass, HighPass })
    {
        auto worst = 0.0;
        for (const auto hz : { 110.0, 440.0, 1000.0, 3000.0 })
            worst = juce::jmax (worst, std::abs (ringHz (type, hz, 1.0, hz) / hz - 1.0));
        check (worst < 0.005, getNames()[type] + " rings at the cutoff (worst " + juce::String (100.0 * worst, 2) + " %)");
    }

    // The combs ring at the cutoff (Comb -: an octave below).
    for (const auto type : { CombPlus, CombMinus, CombDamped })
    {
        auto worst = 0.0;
        for (const auto hz : { 110.0, 440.0, 1000.0 })
        {
            const auto target = type == CombMinus ? hz * 0.5 : hz;
            worst = juce::jmax (worst, std::abs (ringHz (type, hz, 1.0, target) / target - 1.0));
        }
        check (worst < 0.005, getNames()[type] + " rings at its pitch (worst " + juce::String (100.0 * worst, 2) + " %)");
    }
}

inline void checkStability()
{
    // Loud noise, a cutoff sweeping 20 Hz - 20 kHz and back, resonance and
    // morph moving: every model stays finite and bounded (the linear SVFs at
    // 24 dB and resonance near 1 have a gain of several hundred, so the bound
    // only catches a blow-up).
    juce::Random random (7);
    for (int type = 0; type < FilterType::Count; ++type)
    {
        FilterUnit filter;
        filter.prepare (sr);
        filter.setType (type, true);
        filter.setDrive (6.0f);
        auto peak = 0.0;
        auto finite = true;
        for (int i = 0; i < 96000; ++i)
        {
            if ((i & 15) == 0)
            {
                const auto phase = (double) i / 96000.0;
                const auto cutoff = 20.0 * std::pow (1000.0, 1.0 - std::abs (2.0 * phase - 1.0));
                filter.setCoefficients (FilterUnit::makeCoefficients (type, sr, cutoff, 0.5 + 0.5 * std::sin (phase * 40.0),
                                                                      (float) (0.5 + 0.5 * std::sin (phase * 23.0))));
            }
            const auto y = filter.process (i < 90000 ? 4.0f * (random.nextFloat() * 2.0f - 1.0f) : 0.0f);
            finite = finite && std::isfinite (y);
            peak = juce::jmax (peak, (double) std::abs (y));
        }
        check (finite && peak < 1000.0, FilterType::getNames()[type] + " stays stable under loud noise and sweeps (peak " + juce::String (peak, 1) + ")");
    }
}

inline void checkModels()
{
    using namespace FilterType;
    // Low-pass models pass DC (unity-ish), high-pass models block it.
    for (const auto type : { LowPass, LadderLow, DiodeLow, Ms20Low, OtaLow, Acid303, MoogDrive })
    {
        const auto gain = sineGainDb (type, 1000.0, 0.0, 20.0);
        check (gain > -9.0 && gain < 6.0, getNames()[type] + " passes the bass (" + juce::String (gain, 1) + " dB at 20 Hz)");
    }
    for (const auto type : { HighPass, LadderHigh, Ms20High })
    {
        const auto gain = sineGainDb (type, 1000.0, 0.0, 20.0);
        check (gain < -40.0, getNames()[type] + " blocks the bass (" + juce::String (gain, 1) + " dB at 20 Hz)");
    }

    // 303 Acid: resonance keeps the bass (the feedback's high-pass), where
    // the Moog-style ladder thins it.
    const auto acidBass = sineGainDb (Acid303, 1000.0, 0.9, 40.0) - sineGainDb (Acid303, 1000.0, 0.0, 40.0);
    const auto moogBass = sineGainDb (MoogDrive, 1000.0, 0.9, 40.0) - sineGainDb (MoogDrive, 1000.0, 0.0, 40.0);
    check (acidBass > moogBass + 3.0, "303 Acid keeps its bass with resonance up and Moog Drive thins it ("
                                          + juce::String (acidBass, 1) + " vs " + juce::String (moogBass, 1) + " dB)");

    // Diode ladders roll off between the 12 and 24 dB slopes far above the
    // cutoff (Moog Drive, a 4-pole ladder, at 24).
    const auto acidSlope = sineGainDb (Acid303, 500.0, 0.0, 8000.0) - sineGainDb (Acid303, 500.0, 0.0, 4000.0);
    const auto moogSlope = sineGainDb (MoogDrive, 500.0, 0.0, 8000.0) - sineGainDb (MoogDrive, 500.0, 0.0, 4000.0);
    check (acidSlope < -15.0 && acidSlope > -24.5 && moogSlope < -21.0,
           "303 Acid rolls off at about 18-24 dB/oct and Moog Drive at 24 (" + juce::String (acidSlope, 1) + ", "
               + juce::String (moogSlope, 1) + ")");

    // Drive inside: more drive, more level into the saturating loop, bounded.
    for (const int type : { Acid303, MoogDrive })
    {
        const auto input = saw (110.0, 0.5f, 24000);
        const auto level = [type, &input] (float drive)
        {
            Probe p;
            p.type = type;
            p.cutoff = 800.0;
            p.resonance = 0.7;
            auto filter = p.make();
            filter.setDrive (drive);
            auto sum = 0.0;
            for (size_t i = 0; i < input.size(); ++i)
            {
                const auto y = (double) filter.process (input[i]);
                if (i > input.size() / 2)
                    sum += y * y;
            }
            return 10.0 * std::log10 (sum / (double) (input.size() / 2) + 1.0e-20);
        };
        const auto soft = level (1.0f), hard = level (8.0f);
        check (drivesInside (type) && hard > soft + 3.0 && hard < 3.0,
               getNames()[type] + " takes the drive inside (" + juce::String (soft, 1) + " -> " + juce::String (hard, 1) + " dB)");
    }
    check (! drivesInside (LadderLow) && ! drivesInside (DiodeLow), "the older models keep the drive in front");

    // Vowel Morph: A's first formant (650 Hz) against I's (290 Hz).
    const auto a = sineGainDb (VowelMorph, 1000.0, 0.5, 650.0, 0.0f) - sineGainDb (VowelMorph, 1000.0, 0.5, 290.0, 0.0f);
    const auto i = sineGainDb (VowelMorph, 1000.0, 0.5, 650.0, 0.5f) - sineGainDb (VowelMorph, 1000.0, 0.5, 290.0, 0.5f);
    check (a > 6.0 && i < -6.0, "Vowel Morph walks from A to I (" + juce::String (a, 1) + ", " + juce::String (i, 1) + " dB)");
    // The cutoff moves from a male voice to a female one: A's F1 650 -> 800 Hz.
    const auto male = Filters2::VowelMorph::formantsFor (1000.0, 0.5, 0.0), female = Filters2::VowelMorph::formantsFor (2000.0, 0.5, 0.0);
    check (std::abs (male.hz[0] - 650.0) < 1.0 && std::abs (female.hz[0] - 800.0) < 1.0, "Vowel Morph's cutoff moves male to female");

    // Comb Body: the body rings the tube's second mode (2.756 x) that a plain
    // comb at that pitch doesn't have.
    const auto bodyMode = sineGainDb (CombBody, 400.0, 0.8, 400.0 * 2.756, 1.0f) - sineGainDb (CombBody, 400.0, 0.8, 1500.0, 1.0f);
    const auto combMode = sineGainDb (CombBody, 400.0, 0.8, 400.0 * 2.756, 0.0f) - sineGainDb (CombBody, 400.0, 0.8, 1500.0, 0.0f);
    check (bodyMode > 20.0 && bodyMode > combMode + 10.0,
           "Comb Body rings a chime's modes (" + juce::String (bodyMode, 1) + " vs comb " + juce::String (combMode, 1) + " dB)");

    // The response display agrees with the models at the cutoff region
    // (small-signal, resonance 0: within 3 dB an octave below the cutoff).
    for (const auto type : { LadderLow, DiodeLow, Ms20Low, OtaLow, MoogDrive, Acid303 })
    {
        const auto shown = 20.0 * std::log10 (std::abs (response (type, false, 0.0, { 0.0, 0.5 }, 0.5, 1000.0)));
        const auto measured = sineGainDb (type, 1000.0, 0.0, 500.0);
        check (std::abs (shown - measured) < 3.0, getNames()[type] + "'s displayed response matches it (" + juce::String (shown, 1)
                                                      + " vs " + juce::String (measured, 1) + " dB)");
    }
}

// Step 4's other half: a spectral warp's amount is a modulation destination
// (a macro sweeps a Stretch warp and the tone follows).
inline void checkSpectralModulation()
{
    const auto centroidWith = [] (float macro, bool routed)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        setParam (processor, "osc1_spectral", 1.0f); // Stretch
        setParam (processor, "osc1_spectral_amt", 0.0f);
        setParam (processor, "macro1", macro);
        if (routed)
        {
            setParam (processor, "mod1_src", (float) Mod::Source::Macro1);
            setParam (processor, "mod1_dst", (float) Mod::destinationForParamId ("osc1_spectral_amt"));
            setParam (processor, "mod1_amt", 1.0f);
        }
        processor.prepareToPlay (48000.0, 512);
        auto centroid = 0.0;
        renderPeakAndCentroid (processor, 45, 30, centroid);
        return centroid;
    };
    const auto still = centroidWith (0.0f, true), swept = centroidWith (1.0f, true), unrouted = centroidWith (1.0f, false);
    check (Mod::destinationForParamId ("osc1_spectral_amt") > 0 && Mod::destinationForParamId ("osc6_spectral_amt") > 0
               && std::abs (swept / still - 1.0) > 0.05 && std::abs (unrouted / still - 1.0) < 0.01,
           "a macro sweeps a spectral warp's amount (centroid " + juce::String (still, 0) + " -> " + juce::String (swept, 0)
               + " Hz; unrouted " + juce::String (unrouted, 0) + ")");
}

inline void checkCpu()
{
    // Rebuilt models on average within 1.5x of the old ones (59 ns/sample on
    // the Linux container when the baseline was made; see
    // docs/filter-overhaul/cpu-before-linux.txt).
    auto total = 0.0;
    for (int type = 0; type < FilterType::Acid303; ++type)
        total += nsPerSample (type, FilterType::usesSlope (type));
    const auto average = total / (double) FilterType::Acid303;
    checkTiming (average < 59.0 * 1.5, "rebuilt filters' average CPU within 1.5x of the old (" + juce::String (average, 1) + " ns/sample)");
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
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_FILTER_CPU", "").isNotEmpty())
        FilterLab::printCpuReport();
    FilterLab::checkLevels();
    FilterLab::checkSelfOscillation();
    FilterLab::checkStability();
    FilterLab::checkModels();
    FilterLab::checkSpectralModulation();
    FilterLab::checkCpu();
}
