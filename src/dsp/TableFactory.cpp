#include "TableFactory.h"

#include <juce_dsp/juce_dsp.h>

#include <complex>

#include <cmath>
#include <functional>

namespace
{
constexpr int frameSize = 2048;
constexpr int numFrames = 64;
constexpr int additivePartials = 160;

constexpr auto twoPi = juce::MathConstants<double>::twoPi;

float sine (double phase) { return (float) std::sin (twoPi * phase); }

float saw (double phase) { return (float) (2.0 * (phase - std::floor (phase + 0.5))); }

float pulse (double phase, double width)
{
    const auto frac = phase - std::floor (phase);
    return frac < width ? 1.0f : -1.0f;
}

float foldTriangle (float value)
{
    value = std::fmod (value + 1.0f, 4.0f);

    if (value < 0.0f)
        value += 4.0f;

    return value <= 2.0f ? value - 1.0f : 3.0f - value;
}

template <typename ShapeFunction>
std::vector<std::vector<float>> generateTable (ShapeFunction&& shapeForFrame)
{
    std::vector<std::vector<float>> frames ((size_t) numFrames, std::vector<float> ((size_t) frameSize, 0.0f));

    for (int frame = 0; frame < numFrames; ++frame)
    {
        const auto t = (double) frame / (double) (numFrames - 1);
        auto& target = frames[(size_t) frame];

        for (int i = 0; i < frameSize; ++i)
            target[(size_t) i] = shapeForFrame (t, (double) i / (double) frameSize);
    }

    return frames;
}
} // namespace

int TableFactory::getNumFactoryTables()
{
    return 120;
}

// The browser's grouping, one entry per factory table.
juce::StringArray TableFactory::getFactoryTableCategories()
{
    return { "Analog", "Digital", "Harsh", "Harsh", "Vocal", "Spectral", "Analog", "Analog",
             "Basic", "Basic", "Analog", "Vocal", "Spectral", "Spectral", "Spectral", "Digital",
             // v1.1
             "Analog", "Analog", "Analog", "Analog",
             "Digital", "Digital", "Digital", "Digital",
             "Vocal", "Vocal", "Vocal",
             "Spectral", "Spectral", "Spectral", "Spectral",
             "Harsh", "Harsh", "Harsh", "Harsh",
             "Organic", "Organic", "Organic",
             "Chaos", "Chaos",
             // v1.3 (M10)
             "Basic", "Basic", "Basic", "Basic", "Basic", "Basic",
             "Analog", "Analog", "Analog", "Analog", "Analog", "Analog", "Analog", "Analog", "Analog", "Analog",
             "Digital", "Digital", "Digital", "Digital", "Digital", "Digital", "Digital", "Digital", "Digital", "Digital",
             "Vocal", "Vocal", "Vocal", "Vocal", "Vocal", "Vocal", "Vocal",
             "Spectral", "Spectral", "Spectral", "Spectral", "Spectral", "Spectral", "Spectral",
             "Harsh", "Harsh", "Harsh", "Harsh", "Harsh", "Harsh", "Harsh",
             "Organic", "Organic", "Organic", "Organic", "Organic", "Organic", "Organic", "Organic",
             "Chaos", "Chaos", "Chaos", "Chaos", "Chaos", "Chaos",
             "Keys", "Keys", "Keys", "Keys", "Keys", "Keys", "Keys",
             "Bass", "Bass", "Bass", "Bass", "Bass", "Bass", "Bass",
             "Spectral", "Organic", "Spectral", "Analog", "Vocal" };
}

juce::StringArray TableFactory::getCategoryOrder()
{
    return { "Basic", "Analog", "Digital", "Vocal", "Spectral", "Harsh", "Organic", "Chaos", "Keys", "Bass" };
}

juce::StringArray TableFactory::getFactoryTableNames()
{
    return { "Basic", "HardSync", "Wavefold", "FM Metal",
             "Formant", "Comb", "PWM", "DriveSaw",
             "Sine", "Triangle", "Analog",
             "Vowel", "Glass", "Fractal", "Riser", "Digital",
             // v1.1 (appended: saved patches store the index)
             "Soft Saw", "Square Sweep", "Saw Octaves", "Organ",
             "Harmonic Walk", "Odd To Even", "Bitwise", "Stepped",
             "Choir", "Throat", "Talkbox",
             "Comb Sweep", "Spectral Tilt", "Partial Cloud", "Fibonacci Bell",
             "Chebyshev", "Hard Clip", "Rectify", "Bitcrush",
             "Pluck Position", "Breath", "Wood",
             "Logistic", "Lorenz",
             // v1.3 (M10, appended before the User tables; old patches move theirs up)
             "Sine To Saw", "Tri To Square", "Pulse Sweep", "Harmonic Stack", "Soft Square", "Saw To Square",
             "Ladder Saw", "Ladder Square", "Acid Ramp", "Skew Triangle", "Rounded PWM", "Soft Sync",
             "RC Saw", "Analog Pulse", "Resonant Tri", "Octave Pulse",
             "FM 1:1", "FM 1:2", "FM 1:3", "FM 2:1", "FM Stack", "CZ Saw", "CZ Square", "CZ Reso", "Wave Scan", "Bit Ramp",
             "Vowels Male", "Vowels Female", "Vowels Child", "Whisper", "Nasal", "Yeah", "Diphthong",
             "Prime Partials", "Odd Tilt", "Moving Comb", "Octave Partials", "Formant Ladder", "Harmonic Bands", "Mirror Spectrum",
             "Fold Sweep", "Fold Bias", "Square Sync", "Ring Sine", "Driven Tri", "Wavecrush", "Grind",
             "Pluck Stiff", "Bowed", "Reed", "Brass", "Flute", "Marimba", "Bell", "Hollow Wood",
             "Henon", "Random Walk", "Rossler", "Shaped Noise", "Tent Map", "Duffing",
             "Tine", "Reed Piano", "Jazz Organ", "Full Organ", "Clav", "Harpsichord", "Toy Piano",
             "Warm Sub", "Reese", "Growl", "Wobble", "808", "Neuro", "Deep Pulse",
             "Glass Harmonics", "Air Pad", "Shimmer Stack", "Warm Pad", "Pad Choir" };
}

namespace
{
// v1.1 tables: remove each frame's DC and bring every frame to the same
// peak, so sweeping FRAME changes the tone, not the volume.
std::vector<std::vector<float>> tidy (std::vector<std::vector<float>> frames)
{
    for (auto& frame : frames)
    {
        auto mean = 0.0;

        for (auto value : frame)
            mean += value;

        mean /= (double) juce::jmax ((size_t) 1, frame.size());
        auto peak = 0.0f;

        for (auto& value : frame)
        {
            value -= (float) mean;
            peak = juce::jmax (peak, std::abs (value));
        }

        if (peak > 1.0e-6f)
            for (auto& value : frame)
                value *= 0.9f / peak;
    }

    return frames;
}

// Additive table from a per-frame harmonic amplitude function: each frame
// is one inverse FFT of its sine partials (the same wave as summing them).
struct NoPhases { double operator() (int) const { return 0.0; } };

// phaseFor (n) offsets partial n by that fraction of a cycle.
template <typename AmplitudeFunction, typename PhaseFunction = NoPhases>
std::vector<std::vector<float>> additiveTable (AmplitudeFunction&& amplitudeFor, int partials = additivePartials,
                                               PhaseFunction phaseFor = {})
{
    using Complex = std::complex<float>;
    juce::dsp::FFT fft ((int) std::log2 ((double) frameSize));
    std::vector<Complex> spectrum ((size_t) frameSize), time ((size_t) frameSize);
    std::vector<std::vector<float>> frames ((size_t) numFrames, std::vector<float> ((size_t) frameSize, 0.0f));
    partials = juce::jmin (partials, frameSize / 2 - 1);

    for (int frame = 0; frame < numFrames; ++frame)
    {
        const auto t = (double) frame / (double) (numFrames - 1);
        std::fill (spectrum.begin(), spectrum.end(), Complex());

        // a sin (2 pi n i / N) = a / 2j (e^+ - e^-): bins n and N - n, scaled
        // by N for the inverse transform's 1 / N.
        for (int n = 1; n <= partials; ++n)
        {
            const auto a = (float) amplitudeFor (t, n) * (float) frameSize * 0.5f;
            const auto phase = (float) (juce::MathConstants<double>::twoPi * phaseFor (n));
            const auto c = std::polar (a, phase); // a e^(j phase) / j = -j a e^(j phase)
            spectrum[(size_t) n] = Complex (c.imag(), -c.real());
            spectrum[(size_t) (frameSize - n)] = std::conj (spectrum[(size_t) n]);
        }

        fft.perform (spectrum.data(), time.data(), true);
        auto& target = frames[(size_t) frame];
        for (int i = 0; i < frameSize; ++i)
            target[(size_t) i] = time[(size_t) i].real();
    }

    return frames;
}

double formantWeight (double frequency, double centre, double width, double gain)
{
    const auto d = (frequency - centre) / width;
    return gain * std::exp (-0.5 * d * d);
}

std::vector<std::vector<float>> generateV11 (int tableIndex)
{
    switch (tableIndex)
    {
        case 16: // Soft Saw: a saw from dark to bright
            return additiveTable ([] (double t, int n) { return std::exp (-(double) n / (3.0 + t * 90.0)) / (double) n; });

        case 17: // Square Sweep: odd harmonics opening up
            return additiveTable ([] (double t, int n)
            {
                return n % 2 == 1 ? std::exp (-(double) n / (2.0 + t * 70.0)) / (double) n : 0.0;
            });

        case 18: // Saw Octaves: a saw with its upper octaves fading in
            return generateTable ([] (double t, double phase)
            {
                return saw (phase) + (float) t * 0.7f * saw (phase * 2.0) + (float) (t * t) * 0.5f * saw (phase * 4.0);
            });

        case 19: // Organ: five drawbar registrations
        {
            static const double bars[5][9] {
                { 8, 8, 8, 0, 0, 0, 0, 0, 0 }, { 8, 8, 8, 8, 0, 0, 0, 0, 0 }, { 8, 0, 8, 6, 0, 4, 0, 0, 3 },
                { 6, 8, 8, 8, 8, 6, 4, 3, 2 }, { 8, 8, 8, 8, 8, 8, 8, 8, 8 } };
            static const int harmonics[9] { 1, 3, 2, 4, 6, 8, 10, 12, 16 }; // 16', 5 1/3', 8', 4' ...
            return additiveTable ([] (double t, int n)
            {
                const auto scaled = t * 4.0;
                const auto a = juce::jlimit (0, 4, (int) scaled);
                const auto b = juce::jmin (4, a + 1);
                const auto frac = scaled - (double) a;
                auto sum = 0.0;

                for (int bar = 0; bar < 9; ++bar)
                    if (harmonics[bar] == n)
                        sum += (bars[a][bar] + (bars[b][bar] - bars[a][bar]) * frac) / 8.0;

                return sum;
            }, 16);
        }

        case 20: // Harmonic Walk: one partial climbing from 1 to 32
            return additiveTable ([] (double t, int n)
            {
                const auto position = 1.0 + t * 31.0;
                return juce::jmax (0.0, 1.0 - std::abs ((double) n - position));
            }, 32);

        case 21: // Odd To Even: odd harmonics hand over to even ones
            return additiveTable ([] (double t, int n)
            {
                return (n % 2 == 1 ? 1.0 - t : t + (n == 1 ? 0.35 : 0.0)) / (double) n;
            });

        case 22: // Bitwise: XOR patterns of an 8-bit ramp
            return generateTable ([] (double t, double phase)
            {
                const auto step = (int) (phase * 256.0) & 255;
                const auto key = 1 + (int) (t * 62.0);
                const auto value = (step ^ (step * key >> 3)) & 255;
                return (float) value / 127.5f - 1.0f;
            });

        case 23: // Stepped: a sine in fewer and fewer steps
            return generateTable ([] (double t, double phase)
            {
                const auto steps = std::exp2 (6.0 - t * 5.0); // 64 down to 2
                return sine (std::floor (phase * steps) / steps);
            });

        case 24: // Choir: "oo" to "ah" with a soft fourth formant
        {
            struct Vowel { double f1, f2, f3, f4; };
            static const Vowel vowels[3] { { 300, 870, 2240, 3300 }, { 450, 800, 2830, 3500 }, { 730, 1090, 2440, 3400 } };
            return additiveTable ([] (double t, int n)
            {
                const auto scaled = t * 2.0;
                const auto a = juce::jlimit (0, 2, (int) scaled);
                const auto b = juce::jmin (2, a + 1);
                const auto frac = scaled - (double) a;
                const auto mix = [&] (double Vowel::* field) { return vowels[a].*field + (vowels[b].*field - vowels[a].*field) * frac; };
                const auto frequency = 110.0 * (double) n;
                return formantWeight (frequency, mix (&Vowel::f1), 80.0, 1.0) + formantWeight (frequency, mix (&Vowel::f2), 100.0, 0.5)
                       + formantWeight (frequency, mix (&Vowel::f3), 150.0, 0.25) + formantWeight (frequency, mix (&Vowel::f4), 200.0, 0.12);
            });
        }

        case 25: // Throat: one narrow resonance sweeping up
            return additiveTable ([] (double t, int n)
            {
                return formantWeight (110.0 * (double) n, 180.0 + t * t * 2600.0, 45.0 + t * 60.0, 1.0) + 0.03 / (double) n;
            });

        case 26: // Talkbox: sharp I -> A -> U formants
        {
            static const double f1s[3] { 270, 730, 300 }, f2s[3] { 2290, 1090, 870 };
            return additiveTable ([] (double t, int n)
            {
                const auto scaled = t * 2.0;
                const auto a = juce::jlimit (0, 2, (int) scaled);
                const auto b = juce::jmin (2, a + 1);
                const auto frac = scaled - (double) a;
                const auto f1 = f1s[a] + (f1s[b] - f1s[a]) * frac;
                const auto f2 = f2s[a] + (f2s[b] - f2s[a]) * frac;
                const auto frequency = 110.0 * (double) n;
                return formantWeight (frequency, f1, 45.0, 1.0) + formantWeight (frequency, f2, 60.0, 0.8) + 0.08 / (double) n;
            });
        }

        case 27: // Comb Sweep: a saw with a moving comb of notches
            return additiveTable ([] (double t, int n)
            {
                return (0.5 + 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) n * (0.02 + t * 0.2))) / (double) n;
            });

        case 28: // Spectral Tilt: from dark to all partials equal
            return additiveTable ([] (double t, int n) { return std::pow ((double) n, -(2.0 - t * 1.6)); }, 96);

        case 29: // Partial Cloud: sparse partials that trade places
        {
            juce::Random random (2718);
            std::array<std::array<float, 97>, 5> sets {};

            for (auto& set : sets)
                for (int n = 1; n <= 96; ++n)
                    set[(size_t) n] = random.nextFloat() < 0.18f ? random.nextFloat() / std::sqrt ((float) n) : 0.0f;

            sets[0][1] = sets[1][1] = sets[2][1] = sets[3][1] = sets[4][1] = 1.0f;

            return additiveTable ([sets] (double t, int n)
            {
                const auto scaled = t * 4.0;
                const auto a = juce::jlimit (0, 4, (int) scaled);
                const auto b = juce::jmin (4, a + 1);
                const auto frac = (float) (scaled - (double) a);
                return (double) (sets[(size_t) a][(size_t) n] + (sets[(size_t) b][(size_t) n] - sets[(size_t) a][(size_t) n]) * frac);
            }, 96);
        }

        case 30: // Fibonacci Bell: only Fibonacci partials, brightening
            return additiveTable ([] (double t, int n)
            {
                static const int fib[] { 1, 2, 3, 5, 8, 13, 21, 34, 55, 89 };

                for (int i = 0; i < 10; ++i)
                    if (fib[i] == n)
                        return std::pow (0.35 + t * 0.55, (double) i);

                return 0.0;
            }, 96);

        case 31: // Chebyshev: T1 up to T12 of a sine
            return generateTable ([] (double t, double phase)
            {
                const auto order = 1.0 + t * 11.0;
                const auto x = std::sin (juce::MathConstants<double>::twoPi * phase);
                return (float) std::cos (order * std::acos (juce::jlimit (-1.0, 1.0, x)));
            });

        case 32: // Hard Clip: a sine driven into a wall
            return generateTable ([] (double t, double phase)
            {
                return juce::jlimit (-1.0f, 1.0f, sine (phase) * (float) (1.0 + t * t * 30.0));
            });

        case 33: // Rectify: half to full-wave rectified sine
            return generateTable ([] (double t, double phase)
            {
                const auto s = sine (phase);
                return s > 0.0f ? s : s * (float) (1.0 - 2.0 * t);
            });

        case 34: // Bitcrush: a sine in fewer amplitude levels
            return generateTable ([] (double t, double phase)
            {
                const auto levels = std::exp2 (7.0 - t * 6.0);
                return (float) (std::round (std::sin (juce::MathConstants<double>::twoPi * phase) * levels) / levels);
            });

        case 35: // Pluck Position: a string plucked from the end to the middle
            return additiveTable ([] (double t, int n)
            {
                const auto position = 0.03 + t * 0.47;
                return std::abs (std::sin (juce::MathConstants<double>::pi * (double) n * position)) / (double) (n * n) * 6.0;
            }, 96);

        case 36: // Breath: harmonics melting into air
        {
            juce::Random random (31415);
            std::array<float, 161> wobble {};

            for (auto& value : wobble)
                value = random.nextFloat();

            return additiveTable ([wobble] (double t, int n)
            {
                const auto harmonic = std::exp (-(double) n / 6.0) / (double) n;
                const auto air = (0.02 + t * 0.25) * wobble[(size_t) n] / std::sqrt ((double) n);
                return harmonic * (1.0 - t * 0.6) + air * (n > 6 ? 1.0 : 0.3);
            });
        }

        case 37: // Wood: odd partials through body resonances
            return additiveTable ([] (double t, int n)
            {
                const auto frequency = 110.0 * (double) n;
                const auto body = formantWeight (frequency, 250.0 + t * 150.0, 80.0, 1.0)
                                  + formantWeight (frequency, 620.0 + t * 300.0, 140.0, 0.7)
                                  + formantWeight (frequency, 1150.0 + t * 500.0, 220.0, 0.45);
                return (n % 2 == 1 ? 1.0 : 0.35) * (body + 0.05) / std::sqrt ((double) n);
            });

        case 38: // Logistic: the logistic map from order to chaos
            return generateTable ([] (double t, double phase)
            {
                // 32 smoothed steps per cycle from x -> r x (1 - x).
                const auto r = 3.4 + t * 0.6;
                std::array<double, 33> values {};
                auto x = 0.4;

                for (int i = 0; i < 200; ++i)
                    x = r * x * (1.0 - x);

                for (auto& value : values)
                {
                    x = r * x * (1.0 - x);
                    value = x * 2.0 - 1.0;
                }

                values[32] = values[0];
                const auto position = phase * 32.0;
                const auto index = juce::jlimit (0, 31, (int) position);
                const auto frac = position - (double) index;
                const auto smooth = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * frac);
                return (float) (values[(size_t) index] + (values[(size_t) index + 1] - values[(size_t) index]) * smooth);
            });

        case 39: // Lorenz: one lap of the Lorenz attractor
            return generateTable ([] (double t, double phase)
            {
                // Integrate a fixed span, then read it as one cycle (crossfaded
                // at the seam so it loops cleanly).
                static std::vector<std::vector<float>> traces;

                if (traces.empty())
                {
                    traces.resize (64);

                    for (int frame = 0; frame < 64; ++frame)
                    {
                        const auto rho = 24.0 + (double) frame / 63.0 * 14.0;
                        auto x = 1.0, y = 1.0, z = 1.0;
                        auto& trace = traces[(size_t) frame];
                        trace.resize (2048);
                        const auto dt = 0.0015;

                        for (int i = 0; i < 3000; ++i) // settle onto the attractor
                        {
                            const auto dx = 10.0 * (y - x), dy = x * (rho - z) - y, dz = x * y - 8.0 / 3.0 * z;
                            x += dx * dt; y += dy * dt; z += dz * dt;
                        }

                        for (int i = 0; i < 2048; ++i)
                        {
                            for (int k = 0; k < 2; ++k)
                            {
                                const auto dx = 10.0 * (y - x), dy = x * (rho - z) - y, dz = x * y - 8.0 / 3.0 * z;
                                x += dx * dt; y += dy * dt; z += dz * dt;
                            }

                            trace[(size_t) i] = (float) (x / 20.0);
                        }

                        // Fade the last 10% into the first sample so the cycle loops cleanly.
                        for (int i = 0; i < 205; ++i)
                        {
                            const auto keep = (float) i / 205.0f; // 0 at the very end
                            auto& tail = trace[(size_t) (2047 - i)];
                            tail = tail * keep + trace[0] * (1.0f - keep);
                        }
                    }
                }

                const auto frame = juce::jlimit (0, 63, (int) std::round (t * 63.0));
                return traces[(size_t) frame][(size_t) juce::jlimit (0, 2047, (int) (phase * 2048.0))];
            });

        default:
            return {};
    }
}
} // namespace

namespace
{
// v1.3 (M10) tables ----------------------------------------------------------

// Interpolates keyframes k[0..count-1] at t in [0, 1].
template <typename Array>
double keyframe (const Array& k, int count, double t)
{
    const auto scaled = juce::jlimit (0.0, 1.0, t) * (count - 1);
    const auto a = juce::jlimit (0, count - 1, (int) scaled);
    const auto b = juce::jmin (count - 1, a + 1);
    return k[a] + (k[b] - k[a]) * (scaled - a);
}

// A resonant 2-pole low-pass's magnitude at harmonic n (cutoff c in
// harmonics, resonance q), squared for 4 poles.
double resonantLp (double n, double c, double q)
{
    const auto x = n / c;
    const auto h = 1.0 / std::sqrt ((1.0 - x * x) * (1.0 - x * x) + (x / q) * (x / q));
    return h * h;
}

double sawAmp (int n) { return 1.0 / n; }
double squareAmp (int n) { return n % 2 == 1 ? 1.0 / n : 0.0; }
double triAmp (int n) { return n % 2 == 1 ? 1.0 / ((double) n * n) : 0.0; }

// Three-formant vowel weight for harmonic n of a fundamental f0.
double vowelWeight (double f0, int n, double f1, double f2, double f3, double width)
{
    const auto f = f0 * n;
    return formantWeight (f, f1, width, 1.0) + formantWeight (f, f2, width * 1.3, 0.6) + formantWeight (f, f3, width * 1.8, 0.3)
           + 0.01 / n;
}

std::vector<std::vector<float>> vowelTable (double f0, const double (&formants)[5][3], double width)
{
    return additiveTable ([f0, &formants, width] (double t, int n)
    {
        const auto scaled = t * 4.0;
        const auto a = juce::jlimit (0, 4, (int) scaled);
        const auto b = juce::jmin (4, a + 1);
        const auto frac = scaled - a;
        const auto mix = [&] (int i) { return formants[a][i] + (formants[b][i] - formants[a][i]) * frac; };
        return vowelWeight (f0, n, mix (0), mix (1), mix (2), width);
    }, 96);
}

// A 2-d chaotic map or flow read as one cycle, crossfaded at the seam.
template <typename Step>
std::vector<std::vector<float>> traceTable (Step&& stepForFrame)
{
    std::vector<std::vector<float>> frames ((size_t) numFrames, std::vector<float> ((size_t) frameSize));
    for (int frame = 0; frame < numFrames; ++frame)
    {
        auto& trace = frames[(size_t) frame];
        stepForFrame ((double) frame / (numFrames - 1), trace);
        auto peak = 1.0e-9f;
        for (auto v : trace)
            peak = juce::jmax (peak, std::abs (v));
        for (auto& v : trace)
            v /= peak;
        for (int i = 0; i < 205; ++i)
        {
            const auto keep = (float) i / 205.0f;
            auto& tail = trace[(size_t) (frameSize - 1 - i)];
            tail = tail * keep + trace[0] * (1.0f - keep);
        }
    }
    return frames;
}

// A trace sampled into frameSize points by linear interpolation of count points.
void resampleInto (const std::vector<double>& points, std::vector<float>& trace)
{
    for (int i = 0; i < frameSize; ++i)
    {
        const auto position = (double) i / frameSize * (double) (points.size() - 1);
        const auto index = juce::jlimit (0, (int) points.size() - 2, (int) position);
        const auto frac = position - index;
        const auto smooth = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * frac);
        trace[(size_t) i] = (float) (points[(size_t) index] + (points[(size_t) index + 1] - points[(size_t) index]) * smooth);
    }
}

std::vector<std::vector<float>> drawbarTable (const double (&bars)[3][9])
{
    static const int harmonics[9] { 1, 3, 2, 4, 6, 8, 10, 12, 16 };
    return additiveTable ([&bars] (double t, int n)
    {
        auto sum = 0.0;
        for (int bar = 0; bar < 9; ++bar)
            if (harmonics[bar] == n)
            {
                const double k[3] { bars[0][bar], bars[1][bar], bars[2][bar] };
                sum += keyframe (k, 3, t) / 8.0;
            }
        return sum;
    }, 16);
}

std::vector<std::vector<float>> generateV13 (int tableIndex)
{
    constexpr auto pi = juce::MathConstants<double>::pi;
    const auto tanh = [] (double x) { return std::tanh (x); };

    switch (tableIndex)
    {
        // Basic
        case 40: // Sine To Saw
            return additiveTable ([] (double t, int n) { return n == 1 ? 1.0 : std::pow (t, 0.5 + 0.1 * n) / n; });
        case 41: // Tri To Square
            return additiveTable ([] (double t, int n) { return n % 2 == 1 ? (1.0 - t) / ((double) n * n) * 0.81 + t / n : 0.0; });
        case 42: // Pulse Sweep: 50 % down to 3 %
            return additiveTable ([] (double t, int n)
            {
                const auto width = 0.5 - t * 0.47;
                return std::sin (pi * n * width) / n;
            }, additivePartials, [] (int) { return 0.25; }); // a pulse's partials are cosines
        case 43: // Harmonic Stack: a sine gaining partials 2..16 one at a time
            return additiveTable ([] (double t, int n)
            {
                const auto count = 1.0 + t * 15.0;
                return juce::jlimit (0.0, 1.0, count - (n - 1)) / std::sqrt ((double) n);
            }, 16);
        case 44: // Soft Square: a sine into tanh
            return generateTable ([tanh] (double t, double phase)
            {
                const auto drive = 1.0 + t * t * 12.0;
                return (float) (tanh (drive * std::sin (2.0 * pi * phase)) / tanh (drive));
            });
        case 45: // Saw To Square
            return additiveTable ([] (double t, int n) { return (n % 2 == 1 ? 1.0 : 1.0 - t) / n; });

        // Analog
        case 46: // Ladder Saw: a resonant sweep up a saw
            return additiveTable ([] (double t, int n) { return sawAmp (n) * resonantLp (n, 1.5 * std::pow (60.0, t), 2.5); });
        case 47: // Ladder Square
            return additiveTable ([] (double t, int n) { return squareAmp (n) * resonantLp (n, 1.5 * std::pow (60.0, t), 2.5); });
        case 48: // Acid Ramp: a squelchy, high-resonance sweep
            return additiveTable ([] (double t, int n) { return sawAmp (n) * resonantLp (n, 2.0 * std::pow (30.0, t), 7.0); });
        case 49: // Skew Triangle: triangle to ramp
            return generateTable ([] (double t, double phase)
            {
                const auto peak = 0.5 + t * 0.49;
                const auto p = phase - std::floor (phase);
                const auto v = p < peak ? p / peak : 1.0 - (p - peak) / (1.0 - peak);
                return (float) (2.0 * v - 1.0);
            });
        case 50: // Rounded PWM: a pulse with soft edges, 50 % to 10 %
            return generateTable ([tanh] (double t, double phase)
            {
                const auto width = 0.5 - t * 0.4;
                const auto p = phase - std::floor (phase);
                return (float) (tanh (30.0 * (std::sin (2.0 * pi * p) - std::cos (pi * width))) );
            });
        case 51: // Soft Sync: a synced saw windowed so the reset doesn't click
            return generateTable ([] (double t, double phase)
            {
                const auto ratio = 1.0 + t * 5.0;
                const auto window = std::sin (pi * phase);
                return (float) (saw (phase * ratio) * window);
            });
        case 52: // RC Saw: a saw through a one-pole low-pass opening up
            return additiveTable ([] (double t, int n)
            {
                const auto c = 1.0 + std::pow (80.0, t);
                return sawAmp (n) / std::sqrt (1.0 + (n / c) * (n / c));
            });
        case 53: // Analog Pulse: widths 50 % to 5 %, rounded
            return additiveTable ([] (double t, int n)
            {
                const auto width = 0.5 - t * 0.45;
                return std::sin (pi * n * width) / n * std::exp (-(double) n / 60.0);
            }, additivePartials, [] (int) { return 0.25; });
        case 54: // Resonant Tri
            return additiveTable ([] (double t, int n) { return (triAmp (n) + 0.02 / n) * resonantLp (n, 1.2 * std::pow (40.0, t), 4.0); });
        case 55: // Octave Pulse: a pulse and its octave
            return generateTable ([] (double t, double phase)
            {
                return (float) (0.6 * pulse (phase, 0.5) + 0.4 * t * pulse (phase * 2.0, 0.3));
            });

        // Digital
        case 56: case 57: case 58: case 59: // FM 1:1, 1:2, 1:3, 2:1: the index from 0 to 8
        {
            static const double carriers[4] { 1, 1, 1, 2 }, modulators[4] { 1, 2, 3, 1 };
            const auto c = carriers[tableIndex - 56], m = modulators[tableIndex - 56];
            return generateTable ([c, m] (double t, double phase)
            {
                const auto index = t * t * 8.0;
                return (float) std::sin (2.0 * pi * c * phase + index * std::sin (2.0 * pi * m * phase));
            });
        }
        case 60: // FM Stack: three operators in a line
            return generateTable ([] (double t, double phase)
            {
                const auto i1 = t * 4.0, i2 = t * t * 3.0;
                return (float) std::sin (2.0 * pi * phase + i1 * std::sin (4.0 * pi * phase + i2 * std::sin (6.0 * pi * phase)));
            });
        case 61: // CZ Saw: phase distortion, sine to saw
            return generateTable ([] (double t, double phase)
            {
                const auto knee = 0.5 - t * 0.49;
                const auto p = phase - std::floor (phase);
                const auto warped = p < knee ? 0.5 * p / knee : 0.5 + 0.5 * (p - knee) / (1.0 - knee);
                return (float) std::cos (2.0 * pi * warped);
            });
        case 62: // CZ Square
            return generateTable ([] (double t, double phase)
            {
                const auto knee = 0.25 - t * 0.24;
                const auto p = std::fmod (phase * 2.0, 1.0);
                const auto half = phase - std::floor (phase) < 0.5;
                const auto flat = p < knee ? p / knee * 0.5 : (p < 0.5 ? 0.5 : (p < 0.5 + knee ? 0.5 + (p - 0.5) / knee * 0.5 : 1.0));
                return (float) (half ? std::cos (pi * flat) : -std::cos (pi * flat));
            });
        case 63: // CZ Reso: a windowed sine at a rising multiple (the CZ resonance)
            return generateTable ([] (double t, double phase)
            {
                const auto p = phase - std::floor (phase);
                const auto k = 1.0 + t * 15.0;
                return (float) ((1.0 - p) * std::sin (2.0 * pi * k * p));
            });
        case 64: // Wave Scan: eight hard-switched waves, PPG style
            return generateTable ([] (double t, double phase)
            {
                const auto wave = juce::jlimit (0, 7, (int) (t * 8.0));
                const auto p = phase - std::floor (phase);
                switch (wave)
                {
                    case 0: return sine (p);
                    case 1: return (float) (sine (p) + 0.5 * sine (p * 3.0));
                    case 2: return saw (p);
                    case 3: return pulse (p, 0.25);
                    case 4: return (float) (sine (p) * sine (p * 5.0));
                    case 5: return (float) std::sin (2.0 * pi * p * p * 6.0);
                    case 6: return (float) (saw (p * 2.0) * 0.6 + pulse (p, 0.5) * 0.4);
                    default: return (float) std::sin (2.0 * pi * p + 3.0 * std::sin (6.0 * pi * p));
                }
            });
        case 65: // Bit Ramp: a ramp losing bits and samples
            return generateTable ([] (double t, double phase)
            {
                const auto levels = std::exp2 (8.0 - t * 6.0), steps = std::exp2 (9.0 - t * 5.0);
                const auto p = std::floor (phase * steps) / steps;
                return (float) (std::round ((2.0 * p - 1.0) * levels) / levels);
            });

        // Vocal
        case 66: { static const double v[5][3] { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
                   return vowelTable (110.0, v, 90.0); }
        case 67: { static const double v[5][3] { { 850, 1220, 2810 }, { 610, 2330, 2990 }, { 310, 2790, 3310 }, { 590, 920, 2710 }, { 370, 950, 2670 } };
                   return vowelTable (220.0, v, 120.0); }
        case 68: { static const double v[5][3] { { 1030, 1370, 3170 }, { 690, 2610, 3570 }, { 370, 3200, 3730 }, { 680, 1060, 3180 }, { 430, 1170, 3260 } };
                   return vowelTable (300.0, v, 150.0); }
        case 69: // Whisper: formants over a ragged spectrum
        {
            juce::Random random (6901);
            std::array<float, 97> jitter {};
            for (auto& j : jitter)
                j = 0.3f + random.nextFloat();
            return additiveTable ([jitter] (double t, int n)
            {
                const double f1s[3] { 400, 700, 300 }, f2s[3] { 1600, 1100, 2200 };
                return (vowelWeight (110.0, n, keyframe (f1s, 3, t), keyframe (f2s, 3, t), 2600.0, 160.0) * 0.6 + 0.08 / std::sqrt ((double) n))
                       * jitter[(size_t) n];
            }, 96);
        }
        case 70: // Nasal: a formant pair with an anti-resonance
            return additiveTable ([] (double t, int n)
            {
                const auto f = 110.0 * n;
                const auto notch = 1.0 - 0.9 * formantWeight (f, 700.0 + t * 500.0, 90.0, 1.0);
                return (formantWeight (f, 280.0, 60.0, 1.0) + formantWeight (f, 1100.0 + t * 1200.0, 150.0, 0.5) + 0.03 / n) * notch;
            }, 96);
        case 71: // Yeah: "ee" opening to "ah"
            return additiveTable ([] (double t, int n)
            {
                return vowelWeight (110.0, n, 270.0 + t * 460.0, 2290.0 - t * 1200.0, 3010.0 - t * 570.0, 90.0);
            }, 96);
        case 72: // Diphthong: "ai" to "oo"
            return additiveTable ([] (double t, int n)
            {
                const double f1s[3] { 730, 400, 300 }, f2s[3] { 1090, 2000, 870 };
                return vowelWeight (110.0, n, keyframe (f1s, 3, t), keyframe (f2s, 3, t), 2400.0, 90.0);
            }, 96);

        // Spectral
        case 73: // Prime Partials: primes only, opening up
            return additiveTable ([] (double t, int n)
            {
                if (n < 2 && n != 1)
                    return 0.0;
                auto prime = n == 1 || n == 2;
                if (n > 2)
                {
                    prime = n % 2 != 0;
                    for (int d = 3; prime && d * d <= n; d += 2)
                        prime = n % d != 0;
                }
                return prime ? std::exp (-(double) n / (4.0 + t * 60.0)) / std::sqrt ((double) n) : 0.0;
            }, 128);
        case 74: // Odd Tilt: odd partials, dark to flat
            return additiveTable ([] (double t, int n) { return n % 2 == 1 ? std::pow ((double) n, -(2.0 - t * 1.7)) : 0.0; }, 128);
        case 75: // Moving Comb: a comb of peaks sliding up
            return additiveTable ([] (double t, int n)
            {
                return (0.15 + 0.85 * std::pow (0.5 + 0.5 * std::cos (2.0 * pi * (n / 6.0 - t)), 4.0)) / std::sqrt ((double) n);
            }, 96);
        case 76: // Octave Partials: 1, 2, 4, 8, ... brightening
            return additiveTable ([] (double t, int n)
            {
                if ((n & (n - 1)) != 0)
                    return 0.0;
                return std::pow (0.3 + t * 0.65, std::log2 ((double) n));
            }, 128);
        case 77: // Formant Ladder: a wide band climbing the harmonics
            return additiveTable ([] (double t, int n)
            {
                const auto centre = std::pow (64.0, t) * 1.5;
                const auto d = std::log2 (n / centre);
                return std::exp (-2.0 * d * d) + 0.02 / n;
            }, 128);
        case 78: // Harmonic Bands: blocks of four partials taking turns
            return additiveTable ([] (double t, int n)
            {
                const auto band = (n - 1) / 4;
                const auto phase = t * 8.0 - band;
                return std::max (0.05, std::cos (pi * 0.5 * juce::jlimit (-1.0, 1.0, phase))) / std::sqrt ((double) n);
            }, 64);
        case 79: // Mirror Spectrum: from a falling to a rising spectrum
            return additiveTable ([] (double t, int n)
            {
                const auto rising = (double) n / 48.0;
                return ((1.0 - t) / n + t * rising * rising) * (n <= 48 ? 1.0 : 0.0);
            }, 48);

        // Harsh
        case 80: // Fold Sweep
            return generateTable ([] (double t, double phase)
            {
                return foldTriangle ((float) (std::sin (2.0 * pi * phase) * (1.0 + t * 7.0)));
            });
        case 81: // Fold Bias: asymmetric folds
            return generateTable ([] (double t, double phase)
            {
                return foldTriangle ((float) (std::sin (2.0 * pi * phase) * (1.0 + t * 5.0) + t * 1.3));
            });
        case 82: // Square Sync
            return generateTable ([] (double t, double phase)
            {
                return pulse (phase * (1.0 + t * 7.0), 0.5);
            });
        case 83: // Ring Sine: a sine ringing against a rising partner
            return generateTable ([] (double t, double phase)
            {
                const auto k = 1.0 + std::round (t * 15.0);
                return (float) (std::sin (2.0 * pi * phase) * std::sin (2.0 * pi * k * phase) * 0.7 + std::sin (2.0 * pi * phase) * 0.3);
            });
        case 84: // Driven Tri
            return generateTable ([tanh] (double t, double phase)
            {
                const auto p = phase - std::floor (phase);
                const auto tri = p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p;
                const auto drive = 1.0 + t * t * 20.0;
                return (float) (tanh (drive * tri) / tanh (drive));
            });
        case 85: // Wavecrush: a saw held in fewer and fewer steps
            return generateTable ([] (double t, double phase)
            {
                const auto steps = std::exp2 (7.0 - t * 5.0);
                return saw (std::floor (phase * steps) / steps + 0.5 / steps);
            });
        case 86: // Grind: a sine phase-modulated by a crushed saw
            return generateTable ([] (double t, double phase)
            {
                const auto steps = std::exp2 (6.0 - t * 3.0);
                const auto crushed = std::floor (saw (phase * 3.0) * steps) / steps;
                return (float) std::sin (2.0 * pi * phase + t * 6.0 * crushed);
            });

        // Organic
        case 87: // Pluck Stiff: a plucked stiff string, bright to mellow
            return additiveTable ([] (double t, int n)
            {
                const auto position = 0.08 + t * 0.3;
                return std::abs (std::sin (pi * n * position)) / (n * n) * std::exp (-(double) n * t / 12.0) * 8.0;
            }, 96);
        case 88: // Bowed: a Helmholtz saw, softer corners with less pressure
            return additiveTable ([] (double t, int n)
            {
                return sawAmp (n) * std::exp (-(double) n / (6.0 + t * 50.0)) * (1.0 + 0.3 * std::sin (n * 0.9));
            });
        case 89: // Reed: odd partials, brightening with breath
            return additiveTable ([] (double t, int n)
            {
                return (n % 2 == 1 ? 1.0 : 0.08 + t * 0.2) * std::exp (-(double) n / (3.0 + t * 20.0)) / std::sqrt ((double) n);
            });
        case 90: // Brass: the brighter the louder
            return additiveTable ([] (double t, int n)
            {
                const auto centre = 2.0 + t * 10.0;
                return std::exp (-std::pow ((n - 1) / centre, 1.5)) / std::sqrt ((double) n);
            });
        case 91: // Flute: a sine with a little 2nd, 3rd and breath
        {
            juce::Random random (9101);
            std::array<float, 161> air {};
            for (auto& a : air)
                a = random.nextFloat();
            return additiveTable ([air] (double t, int n)
            {
                const double harm[4] { 0.0, 1.0, 0.25 + t * 0.2, 0.1 + t * 0.15 };
                return (n <= 3 ? harm[n] : 0.02 * t * air[(size_t) n] / std::sqrt ((double) n));
            });
        }
        case 92: // Marimba: the tuned bar's 1, 4, 10 partials
            return additiveTable ([] (double t, int n)
            {
                return n == 1 ? 1.0 : n == 4 ? 0.2 + t * 0.5 : n == 10 ? 0.05 + t * 0.3 : n == 7 ? 0.03 * t : 0.0;
            }, 16);
        case 93: // Bell: minor-third bell partials to the nearest harmonic
            return additiveTable ([] (double t, int n)
            {
                switch (n)
                {
                    case 1: return 0.6;
                    case 2: return 1.0;
                    case 3: return 0.3 + t * 0.4;  // tierce (2.4)
                    case 4: return 0.5;            // nominal (4.0)
                    case 5: return 0.2 + t * 0.3;
                    case 8: return 0.1 + t * 0.3;
                    case 11: return 0.05 + t * 0.2;
                    default: return 0.0;
                }
            }, 16);
        case 94: // Hollow Wood: even partials suppressed, body resonances
            return additiveTable ([] (double t, int n)
            {
                const auto f = 110.0 * n;
                return (n % 2 == 1 ? 1.0 : 0.1) * (formantWeight (f, 400.0 + t * 300.0, 120.0, 1.0) + formantWeight (f, 1500.0, 300.0, 0.3) + 0.04)
                       / std::sqrt ((double) n);
            });

        // Chaos
        case 95: // Henon
            return traceTable ([] (double t, std::vector<float>& trace)
            {
                const auto a = 1.05 + t * 0.35;
                auto x = 0.1, y = 0.1;
                std::vector<double> points;
                for (int i = 0; i < 100 + 64; ++i)
                {
                    const auto nx = 1.0 - a * x * x + y;
                    y = 0.3 * x;
                    x = std::isfinite (nx) && std::abs (nx) < 10.0 ? nx : 0.1;
                    if (i >= 100)
                        points.push_back (x);
                }
                resampleInto (points, trace);
            });
        case 96: // Random Walk: smooth random curves, rougher across the table
            return traceTable ([] (double t, std::vector<float>& trace)
            {
                juce::Random random (9601);
                const auto count = 8 + (int) (t * 56.0);
                std::vector<double> points;
                auto x = 0.0;
                for (int i = 0; i < count; ++i)
                {
                    x = juce::jlimit (-1.0, 1.0, x + (random.nextDouble() * 2.0 - 1.0) * 0.6);
                    points.push_back (x);
                }
                points.push_back (points.front());
                resampleInto (points, trace);
            });
        case 97: // Rossler
            return traceTable ([] (double t, std::vector<float>& trace)
            {
                const auto c = 4.0 + t * 5.0;
                auto x = 1.0, y = 1.0, z = 0.0;
                const auto dt = 0.01;
                for (int i = 0; i < 20000; ++i)
                {
                    const auto dx = -y - z, dy = x + 0.2 * y, dz = 0.2 + z * (x - c);
                    x += dx * dt; y += dy * dt; z += dz * dt;
                }
                for (auto& v : trace)
                {
                    for (int k = 0; k < 3; ++k)
                    {
                        const auto dx = -y - z, dy = x + 0.2 * y, dz = 0.2 + z * (x - c);
                        x += dx * dt; y += dy * dt; z += dz * dt;
                    }
                    v = (float) x;
                }
            });
        case 98: // Shaped Noise: random partials with random phases, tilting
        {
            std::array<double, 65> phases {}, amps {};
            juce::Random random (9801);
            for (int n = 1; n <= 64; ++n)
            {
                phases[(size_t) n] = random.nextDouble();
                amps[(size_t) n] = 0.2 + random.nextDouble();
            }
            return additiveTable ([amps] (double t, int n) { return amps[(size_t) n] * std::pow ((double) n, -(1.6 - t * 1.2)); }, 64,
                                  [phases] (int n) { return phases[(size_t) n]; });
        }
        case 99: // Tent Map
            return traceTable ([] (double t, std::vector<float>& trace)
            {
                const auto mu = 1.2 + t * 0.79;
                auto x = 0.3;
                std::vector<double> points;
                for (int i = 0; i < 100 + 48; ++i)
                {
                    x = x < 0.5 ? mu * x : mu * (1.0 - x);
                    if (i >= 100)
                        points.push_back (x * 2.0 - 1.0);
                }
                resampleInto (points, trace);
            });
        case 100: // Duffing
            return traceTable ([] (double t, std::vector<float>& trace)
            {
                const auto drive = 0.2 + t * 0.4;
                auto x = 0.5, v = 0.0, time = 0.0;
                const auto dt = 0.01;
                const auto step = [&]
                {
                    const auto a = -0.25 * v + x - x * x * x + drive * std::cos (1.2 * time);
                    v += a * dt; x += v * dt; time += dt;
                };
                for (int i = 0; i < 10000; ++i)
                    step();
                for (auto& value : trace)
                {
                    for (int k = 0; k < 3; ++k)
                        step();
                    value = (float) x;
                }
            });

        // Keys
        case 101: // Tine: a sine, its bell partial and some bark
            return additiveTable ([] (double t, int n)
            {
                return n == 1 ? 1.0 : n == 2 ? 0.15 + t * 0.4 : n == 3 ? 0.05 + t * 0.3 : n == 7 ? 0.08 + t * 0.35 : n == 14 ? 0.03 * t : 0.0;
            }, 16);
        case 102: // Reed Piano: an asymmetric, reedy wave
            return generateTable ([tanh] (double t, double phase)
            {
                const auto s = std::sin (2.0 * pi * phase);
                const auto drive = 1.0 + t * 4.0;
                return (float) (tanh (drive * s + 0.4 * t) - tanh (0.4 * t));
            });
        case 103: { static const double b[3][9] { { 8, 8, 8, 0, 0, 0, 0, 0, 0 }, { 8, 8, 8, 4, 0, 0, 0, 0, 0 }, { 8, 8, 8, 6, 0, 0, 0, 0, 4 } };
                    return drawbarTable (b); } // Jazz Organ
        case 104: { static const double b[3][9] { { 8, 6, 8, 8, 4, 4, 0, 0, 0 }, { 8, 8, 8, 8, 8, 6, 4, 2, 2 }, { 8, 8, 8, 8, 8, 8, 8, 8, 8 } };
                    return drawbarTable (b); } // Full Organ
        case 105: // Clav: a narrow pulse plucked near the end
            return additiveTable ([] (double t, int n)
            {
                return std::sin (pi * n * (0.1 + t * 0.15)) / n * std::abs (std::sin (pi * n * 0.07)) * 6.0;
            }, 96, [] (int) { return 0.25; });
        case 106: // Harpsichord: a bright pluck near the bridge
            return additiveTable ([] (double t, int n)
            {
                return std::abs (std::sin (pi * n * (0.04 + t * 0.08))) / std::pow ((double) n, 1.2) * 4.0;
            });
        case 107: // Toy Piano: a clangy rod
            return additiveTable ([] (double t, int n)
            {
                return n == 1 ? 1.0 : n == 3 ? 0.3 + t * 0.4 : n == 6 ? 0.4 : n == 11 ? 0.1 + t * 0.3 : n == 17 ? 0.08 * t : 0.0;
            }, 20);

        // Bass
        case 108: // Warm Sub: a sine gaining its 2nd and 3rd
            return additiveTable ([] (double t, int n) { return n == 1 ? 1.0 : n == 2 ? t * 0.4 : n == 3 ? t * t * 0.2 : 0.0; }, 4);
        case 109: // Reese: a saw with a notch moving through it
            return additiveTable ([] (double t, int n)
            {
                return sawAmp (n) * (1.0 - 0.95 * std::exp (-std::pow ((n - (2.0 + t * 12.0)) / 1.5, 2.0)));
            });
        case 110: // Growl: a vowel-ish resonance low in the spectrum
            return additiveTable ([] (double t, int n)
            {
                return sawAmp (n) * 0.3 + formantWeight (55.0 * n, 300.0 + t * 700.0, 70.0, 1.0) / std::sqrt ((double) n);
            }, 96);
        case 111: // Wobble: a square through a resonant low-pass, closed to open
            return additiveTable ([] (double t, int n) { return squareAmp (n) * resonantLp (n, 1.2 + t * 14.0, 5.0); }, 64);
        case 112: // 808: a sine driven harder and off-centre (even harmonics, like a clipped kick)
            return generateTable ([tanh] (double t, double phase)
            {
                const auto drive = 1.0 + t * 6.0, bias = 0.15 + t * 0.5;
                return (float) (tanh (drive * std::sin (2.0 * pi * phase) + bias) - tanh (bias));
            });
        case 113: // Neuro: FM into a fold
            return generateTable ([] (double t, double phase)
            {
                const auto fm = std::sin (2.0 * pi * phase + t * 3.0 * std::sin (4.0 * pi * phase));
                return foldTriangle ((float) (fm * (1.0 + t * 3.0)));
            });
        case 114: // Deep Pulse: a rounded pulse with a strong fundamental
            return additiveTable ([] (double t, int n)
            {
                return (n == 1 ? 1.5 : 1.0) * std::sin (pi * n * (0.5 - t * 0.35)) / n * std::exp (-(double) n / 20.0);
            }, 64, [] (int) { return 0.25; });

        // v1.3 extras
        case 115: // Glass Harmonics: high partials only
            return additiveTable ([] (double t, int n)
            {
                return n == 1 ? 0.5 : std::exp (-std::pow ((n - (8.0 + t * 40.0)) / 5.0, 2.0)) / std::sqrt ((double) n) * 3.0;
            }, 96);
        case 116: // Air Pad: soft harmonics and air
        {
            juce::Random random (11601);
            std::array<float, 161> air {};
            for (auto& a : air)
                a = random.nextFloat();
            return additiveTable ([air] (double t, int n)
            {
                return std::exp (-(double) n / 4.0) / n + (0.01 + t * 0.06) * air[(size_t) n] / std::sqrt ((double) n);
            });
        }
        case 117: // Shimmer Stack: octaves and fifths
            return additiveTable ([] (double t, int n)
            {
                const auto octave = (n & (n - 1)) == 0;
                const auto fifth = n % 3 == 0 && ((n / 3) & (n / 3 - 1)) == 0;
                return (octave ? 1.0 : fifth ? 0.3 + t * 0.6 : 0.0) * std::pow (0.8 + t * 0.15, std::log2 ((double) n));
            }, 96);
        case 118: // Warm Pad: a soft saw with a low-mid body
            return additiveTable ([] (double t, int n)
            {
                return sawAmp (n) * std::exp (-(double) n / (6.0 + t * 20.0)) * (1.0 + 0.8 * formantWeight (110.0 * n, 500.0, 200.0, 1.0));
            });
        case 119: // Pad Choir: soft "oo" to "ah"
            return additiveTable ([] (double t, int n)
            {
                return vowelWeight (110.0, n, 330.0 + t * 400.0, 900.0 + t * 200.0, 2400.0, 110.0) * std::exp (-(double) n / 30.0);
            }, 96);
        default:
            return {};
    }
}
} // namespace

std::vector<std::vector<float>> TableFactory::generate (int tableIndex)
{
    if (tableIndex >= 40)
        return tidy (generateV13 (tableIndex));
    if (tableIndex >= 16)
        return tidy (generateV11 (tableIndex));

    switch (tableIndex)
    {
        case 0: // (the same partial sums as ever, built by inverse FFT)
            return additiveTable ([] (double t, int n)
            {
                const auto fade = std::pow (1.0 - t, 2.0);
                return (n % 2 == 0 ? fade : 1.0) / (double) n;
            }, 96);

        case 1:
            return generateTable ([] (double t, double phase)
            {
                return saw (phase * (1.0 + t * 7.0));
            });

        case 2:
            return generateTable ([] (double t, double phase)
            {
                return foldTriangle (sine (phase) * (float) (1.0 + t * 7.0));
            });

        case 3:
            return generateTable ([] (double t, double phase)
            {
                const auto index = t * 8.0;
                return (float) std::sin (twoPi * phase + index * std::sin (twoPi * 3.0 * phase));
            });

        case 4:
            return additiveTable ([] (double t, int n)
            {
                const auto center1 = 3.0 + t * 40.0;
                const auto center2 = 6.0 + t * 90.0;
                const auto width1 = 0.35 * center1 + 1.0;
                const auto width2 = 0.30 * center2 + 1.0;
                const auto distance1 = ((double) n - center1) / width1;
                const auto distance2 = ((double) n - center2) / width2;
                const auto body = 0.15 / (double) n;
                return (body + std::exp (-0.5 * distance1 * distance1) + 0.7 * std::exp (-0.5 * distance2 * distance2)) / (double) n;
            });

        case 5:
            return additiveTable ([] (double t, int n)
            {
                const auto combFrequency = 0.08 + t * 2.5;
                return (0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * (double) n * combFrequency)) / (double) n;
            });

        case 6:
            return generateTable ([] (double t, double phase)
            {
                return pulse (phase, 0.5 - t * 0.44);
            });

        case 7:
            return generateTable ([] (double t, double phase)
            {
                return (float) std::tanh (saw (phase) * (1.0 + t * 14.0));
            });

        case 8:
            return generateTable ([] (double, double phase)
            {
                return sine (phase);
            });

        case 9:
            return additiveTable ([] (double t, int n)
            {
                if (n % 2 == 0)
                    return 0.0;
                const auto rolloff = 1.0 + t * 2.0;
                const auto sign = ((n - 1) / 2) % 2 == 0 ? 1.0 : -1.0;
                return 2.0 * sign / (double) (n * n) * std::exp (-(double) n / (60.0 / rolloff));
            }, 96);

        case 10:
            return additiveTable ([] (double t, int n)
            {
                const auto rolloff = 40.0 + t * 80.0;
                return (1.0 / (double) n) * std::exp (-juce::jmax (0.0, (double) n - rolloff) / 25.0);
            }, 128);

        case 11:
        {
            // Vowel: morphs A -> E -> I -> O -> U through three formants.
            struct Formant { double f1, f2, f3; };
            static const Formant vowels[] { { 730.0, 1090.0, 2440.0 }, { 530.0, 1840.0, 2480.0 },
                                            { 270.0, 2290.0, 3010.0 }, { 570.0, 840.0, 2410.0 },
                                            { 300.0, 870.0, 2240.0 } };
            return additiveTable ([] (double t, int n)
            {
                const auto scaled = juce::jlimit (0.0, 4.0, t * 4.0);
                const auto vowelIndex = juce::jlimit (0, 3, (int) scaled);
                const auto frac = scaled - (double) vowelIndex;
                const auto& a = vowels[vowelIndex];
                const auto& b = vowels[juce::jmin (4, vowelIndex + 1)];

                const auto f1 = a.f1 + (b.f1 - a.f1) * frac;
                const auto f2 = a.f2 + (b.f2 - a.f2) * frac;
                const auto f3 = a.f3 + (b.f3 - a.f3) * frac;
                constexpr auto f0 = 110.0;
                const auto frequency = f0 * (double) n;
                const auto weight = [&] (double formant, double width, double gain)
                {
                    const auto d = (frequency - formant) / width;
                    return gain * std::exp (-0.5 * d * d);
                };
                return (weight (f1, 90.0, 1.0) + weight (f2, 110.0, 0.55) + weight (f3, 140.0, 0.3)) + 0.06 / std::sqrt ((double) n);
            });
        }

        case 12:
            return generateTable ([] (double t, double phase)
            {
                // Glass: inharmonic two-operator FM, bell-like partials.
                const auto index = 0.4 + t * 7.0;

                return (float) (0.72 * std::sin (twoPi * phase + index * std::sin (twoPi * 3.5 * phase))
                                + 0.28 * std::sin (twoPi * 2.0 * phase + index * 0.5 * std::sin (twoPi * 7.03 * phase)));
            });

        case 13:
        {
            // Fractal: self-similar harmonic stack whose spectrum gets rougher.
            return additiveTable ([] (double t, int n)
            {
                const auto power = 0.75 + t * 0.85;
                const auto roughness = t * 2.6;
                const auto modulation = 0.55 + 0.45 * std::sin (std::pow ((double) n, 0.6) * 5.0 + roughness * std::log ((double) n + 1.0));
                return std::pow ((double) n, -power) * modulation;
            });
        }

        case 14:
        {
            // Riser: a resonant peak sweeping upward with growing high end.
            return additiveTable ([] (double t, int n)
            {
                const auto peak = 4.0 + t * t * 140.0;
                const auto width = 6.0 + t * 34.0;
                const auto d = ((double) n - peak) / width;
                return std::exp (-0.5 * d * d) * 1.15 + (0.05 + t * 0.55) / std::sqrt ((double) n);
            });
        }

        case 15:
            return generateTable ([] (double t, double phase)
            {
                // Digital: phase distortion (Casio CZ style) sweeping saw -> sine.
                const auto bend = juce::jmax (0.02, 0.5 - t * 0.47);
                auto p = phase - std::floor (phase);

                const auto warped = p < bend ? 0.5 * p / bend
                                             : 0.5 + 0.5 * (p - bend) / (1.0 - bend);

                return sine (warped);
            });
    }

    return generateTable ([] (double, double phase) { return sine (phase); });
}
