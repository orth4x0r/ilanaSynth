#include "TableFactory.h"

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

float additive (double phase, const float* amplitudes, int count)
{
    const auto angle = twoPi * phase;
    const auto cosStep = (float) std::cos (angle);
    const auto sinStep = (float) std::sin (angle);

    auto sinN = sinStep;
    auto cosN = cosStep;
    auto sum = 0.0f;

    for (int n = 1; n <= count; ++n)
    {
        sum += amplitudes[n] * sinN;

        const auto nextSin = sinN * cosStep + cosN * sinStep;
        cosN = cosN * cosStep - sinN * sinStep;
        sinN = nextSin;
    }

    return sum;
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
    return 40;
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
             "Chaos", "Chaos" };
}

juce::StringArray TableFactory::getCategoryOrder()
{
    return { "Basic", "Analog", "Digital", "Vocal", "Spectral", "Harsh", "Organic", "Chaos" };
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
             "Logistic", "Lorenz" };
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

// Additive table from a per-frame harmonic amplitude function.
template <typename AmplitudeFunction>
std::vector<std::vector<float>> additiveTable (AmplitudeFunction&& amplitudeFor, int partials = additivePartials)
{
    std::vector<float> amplitudes ((size_t) partials + 1, 0.0f);
    auto lastT = -1.0;

    return generateTable ([&] (double t, double phase)
    {
        if (t != lastT)
        {
            lastT = t;

            for (int n = 1; n <= partials; ++n)
                amplitudes[(size_t) n] = (float) amplitudeFor (t, n);
        }

        return additive (phase, amplitudes.data(), partials);
    });
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

std::vector<std::vector<float>> TableFactory::generate (int tableIndex)
{
    if (tableIndex >= 16)
        return tidy (generateV11 (tableIndex));

    switch (tableIndex)
    {
        case 0:
            return generateTable ([] (double t, double phase)
            {
                const auto fade = (float) std::pow (1.0 - t, 2.0);
                auto sum = 0.0f;

                for (int n = 1; n <= 96; ++n)
                {
                    const auto gain = (n % 2 == 0 ? fade : 1.0f) / (float) n;
                    sum += gain * sine (phase * n);
                }

                return sum;
            });

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
        {
            std::vector<float> amplitudes ((size_t) additivePartials + 1, 0.0f);

            return generateTable ([&amplitudes] (double t, double phase)
            {
                const auto center1 = 3.0 + t * 40.0;
                const auto center2 = 6.0 + t * 90.0;
                const auto width1 = 0.35 * center1 + 1.0;
                const auto width2 = 0.30 * center2 + 1.0;

                for (int n = 1; n <= additivePartials; ++n)
                {
                    const auto distance1 = ((double) n - center1) / width1;
                    const auto distance2 = ((double) n - center2) / width2;
                    const auto body = 0.15 / (double) n;

                    amplitudes[(size_t) n] = (float) (body
                                                      + std::exp (-0.5 * distance1 * distance1)
                                                      + 0.7 * std::exp (-0.5 * distance2 * distance2)) / (float) n;
                }

                amplitudes[0] = 0.0f;
                return additive (phase, amplitudes.data(), additivePartials);
            });
        }

        case 5:
        {
            std::vector<float> amplitudes ((size_t) additivePartials + 1, 0.0f);

            return generateTable ([&amplitudes] (double t, double phase)
            {
                const auto combFrequency = 0.08 + t * 2.5;

                for (int n = 1; n <= additivePartials; ++n)
                {
                    const auto comb = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * (double) n * combFrequency);
                    amplitudes[(size_t) n] = (float) comb / (float) n;
                }

                amplitudes[0] = 0.0f;
                return additive (phase, amplitudes.data(), additivePartials);
            });
        }

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
            return generateTable ([] (double t, double phase)
            {
                auto sum = 0.0f;
                const auto rolloff = 1.0f + (float) t * 2.0f;

                for (int n = 1; n <= 96; n += 2)
                {
                    const auto sign = ((n - 1) / 2) % 2 == 0 ? 1.0f : -1.0f;
                    const auto gain = sign / (float) (n * n);
                    sum += gain * sine (phase * n) * std::exp (-(float) n / (60.0f / rolloff));
                }

                return sum * 2.0f;
            });

        case 10:
            return generateTable ([] (double t, double phase)
            {
                auto sum = 0.0f;
                const auto rolloff = (float) (40.0 + t * 80.0);

                for (int n = 1; n <= 128; ++n)
                {
                    const auto gain = (1.0f / (float) n) * std::exp (-juce::jmax (0.0f, (float) n - rolloff) / 25.0f);
                    sum += gain * sine (phase * n);
                }

                return sum;
            });

        case 11:
        {
            // Vowel: morphs A -> E -> I -> O -> U through three formants.
            struct Formant { double f1, f2, f3; };
            static const Formant vowels[] { { 730.0, 1090.0, 2440.0 }, { 530.0, 1840.0, 2480.0 },
                                            { 270.0, 2290.0, 3010.0 }, { 570.0, 840.0, 2410.0 },
                                            { 300.0, 870.0, 2240.0 } };
            std::vector<float> amplitudes ((size_t) additivePartials + 1, 0.0f);

            return generateTable ([&amplitudes] (double t, double phase)
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

                for (int n = 1; n <= additivePartials; ++n)
                {
                    const auto frequency = f0 * (double) n;
                    const auto weight = [&] (double formant, double width, double gain)
                    {
                        const auto d = (frequency - formant) / width;
                        return gain * std::exp (-0.5 * d * d);
                    };

                    amplitudes[(size_t) n] = (float) ((weight (f1, 90.0, 1.0)
                                                       + weight (f2, 110.0, 0.55)
                                                       + weight (f3, 140.0, 0.3)) + 0.06 / std::sqrt ((double) n));
                }

                amplitudes[0] = 0.0f;
                return additive (phase, amplitudes.data(), additivePartials);
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
            std::vector<float> amplitudes ((size_t) additivePartials + 1, 0.0f);

            return generateTable ([&amplitudes] (double t, double phase)
            {
                const auto power = 0.75 + t * 0.85;
                const auto roughness = t * 2.6;

                for (int n = 1; n <= additivePartials; ++n)
                {
                    const auto modulation = 0.55 + 0.45 * std::sin (std::pow ((double) n, 0.6) * 5.0
                                                                    + roughness * std::log ((double) n + 1.0));
                    amplitudes[(size_t) n] = (float) (std::pow ((double) n, -power) * modulation);
                }

                amplitudes[0] = 0.0f;
                return additive (phase, amplitudes.data(), additivePartials);
            });
        }

        case 14:
        {
            // Riser: a resonant peak sweeping upward with growing high end.
            std::vector<float> amplitudes ((size_t) additivePartials + 1, 0.0f);

            return generateTable ([&amplitudes] (double t, double phase)
            {
                const auto peak = 4.0 + t * t * 140.0;
                const auto width = 6.0 + t * 34.0;

                for (int n = 1; n <= additivePartials; ++n)
                {
                    const auto d = ((double) n - peak) / width;
                    const auto bump = std::exp (-0.5 * d * d);
                    amplitudes[(size_t) n] = (float) (bump * 1.15 + (0.05 + t * 0.55) / std::sqrt ((double) n));
                }

                amplitudes[0] = 0.0f;
                return additive (phase, amplitudes.data(), additivePartials);
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
