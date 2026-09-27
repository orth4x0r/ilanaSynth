#pragma once

// M8.1: real chaos and physics modulators.
//
// The shapes appended after the M2 set (LfoShapes::RandomHold onwards) are
// simulations with named parameters: a random family (S&H, sine
// interpolated, Perlin, drunk walk), chaotic systems solved with RK4
// (Lorenz, Rossler, Duffing, double pendulum) or stepped maps (logistic,
// Henon), and physics objects (bounce, pendulum, spring, stick-slip
// friction). Each one has two outputs, A and B.
//
// The M2 shapes keep their indices and code (LfoShape.h) as the "classic"
// versions, so old patches render exactly as before.

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace LfoSimShapes
{
// Appended after LfoShapes::Friction (15). Indices are stored in patches.
enum
{
    RandomHold = 16, SineRandom, Perlin, DrunkWalk,
    Lorenz, Rossler, Duffing, Logistic, Henon, DoublePendulum,
    Bounce, Pendulum, Spring, Friction,
    End
};

inline bool isSim (int shape) { return shape >= RandomHold && shape < End; }
inline bool isRandom (int shape) { return shape >= RandomHold && shape <= DrunkWalk; }
inline bool isMap (int shape) { return shape == Logistic || shape == Henon; }
inline bool isChaos (int shape) { return shape >= Lorenz && shape <= DoublePendulum; }
inline bool isPhysics (int shape) { return shape >= Bounce && shape <= Friction; }
// Solved as differential equations (control rate, RK4 or exact steps).
inline bool isOde (int shape) { return isSim (shape) && ! isRandom (shape) && ! isMap (shape); }
// Phase-driven: one new value per cycle of RATE.
inline bool isStepped (int shape) { return isRandom (shape) || isMap (shape); }
} // namespace LfoSimShapes

// Names, units and defaults of the six shape parameters (lfoN_p1..p6),
// which are stored normalised (0..1) and mapped per shape.
namespace LfoSimInfo
{
constexpr int numParams = 6;

struct Param
{
    const char* name = nullptr; // nullptr: unused by this shape
    float defaultValue = 0.5f;  // normalised
};

struct Shape
{
    const char* name;
    std::array<Param, numParams> params;
    const char* outA;
    const char* outB;
    bool usesAxis, usesLoop, usesStereo, usesSeed;
};

inline const Shape& get (int shape)
{
    using P = Param;
    static const Shape shapes[] {
        { "Random S&H", { P { nullptr }, P {}, P {}, P {}, P {}, P {} }, "VALUE", "STEREO", false, false, true, true },
        { "Sine Random", { P { nullptr }, P {}, P {}, P {}, P {}, P {} }, "VALUE", "STEREO", false, false, true, true },
        { "Perlin", { P { "OCTAVES", 0.43f }, P { "ROUGH", 0.5f }, P {}, P {}, P {}, P {} }, "VALUE", "STEREO", false, false, true, true },
        { "Drunk Walk", { P { "STEP", 0.3f }, P {}, P {}, P {}, P {}, P {} }, "VALUE", "STEREO", false, false, true, true },
        { "Lorenz", { P { "SIGMA", 0.5f }, P { "RHO", 0.5f }, P { "BETA", 0.5f }, P {}, P {}, P {} }, "X", "NEXT AXIS", true, false, false, true },
        { "Rossler", { P { "A", 0.5f }, P { "B", 0.5f }, P { "C", 0.5f }, P {}, P {}, P {} }, "X", "NEXT AXIS", true, false, false, true },
        { "Duffing", { P { "DRIVE", 0.625f }, P { "DAMP", 0.5f }, P { "FREQ", 0.5f }, P {}, P {}, P {} }, "POSITION", "VELOCITY", true, false, false, true },
        { "Logistic Map", { P { "R", 0.5f }, P {}, P {}, P {}, P {}, P {} }, "X", "PREVIOUS", false, false, false, true },
        { "Henon Map", { P { "A", 0.5f }, P { "B", 0.5f }, P {}, P {}, P {}, P {} }, "X", "Y", false, false, false, true },
        { "Double Pendulum", { P { "LENGTH 1", 0.5f }, P { "LENGTH 2", 0.5f }, P { "MASS 1", 0.5f }, P { "MASS 2", 0.5f },
                               P { "ANGLE 1", 0.75f }, P { "ANGLE 2", 0.9f } }, "TIP X", "TIP Y", false, false, false, false },
        { "Bounce", { P { "GRAVITY", 0.5f }, P { "HEIGHT", 0.5f }, P { "ELASTIC", 0.72f }, P { "DRAG", 0.0f }, P {}, P {} },
          "HEIGHT", "IMPACT", false, true, false, false },
        { "Pendulum", { P { "LENGTH", 0.5f }, P { "GRAVITY", 0.5f }, P { "DAMP", 0.25f }, P { "ANGLE", 0.33f },
                        P { "DRIVE", 0.0f }, P { "DRIVE FREQ", 0.5f } }, "SWING", "SPEED", false, true, false, false },
        { "Spring", { P { "MASS", 0.5f }, P { "STIFFNESS", 0.5f }, P { "DAMP", 0.3f }, P { "REST", 0.5f }, P {}, P {} },
          "POSITION", "VELOCITY", false, true, false, false },
        { "Friction", { P { "SPEED", 0.5f }, P { "STATIC", 0.5f }, P { "SLIDING", 0.5f }, P { "STIFFNESS", 0.5f },
                        P { "MASS", 0.5f }, P {} }, "STRETCH", "SLIP", false, false, false, false },
    };

    return shapes[juce::jlimit (0, LfoSimShapes::End - LfoSimShapes::RandomHold - 1, shape - LfoSimShapes::RandomHold)];
}

// Exponential knob around a centre value: 0.5 = centre, each end = centre
// times or divided by 2^octaves.
inline double expAround (double centre, double octaves, float p)
{
    return centre * std::exp2 (((double) p - 0.5) * 2.0 * octaves);
}

constexpr double earthGravity = 9.81;

// The physical value of parameter `index` (0..5) of `shape` at normalised p.
inline double value (int shape, int index, float p)
{
    using namespace LfoSimShapes;
    const auto x = (double) juce::jlimit (0.0f, 1.0f, p);

    switch (shape)
    {
        case Perlin:   return index == 0 ? 1.0 + std::floor (x * 7.0 + 0.5) : 0.25 + 0.5 * x; // octaves, persistence
        case DrunkWalk: return 0.02 + 0.98 * x;                                            // largest step (of the full range)
        case Lorenz:   return index == 0 ? 20.0 * x : index == 1 ? 56.0 * x : (16.0 / 3.0) * x;
        case Rossler:  return index == 0 ? 0.4 * x : index == 1 ? 0.4 * x : 1.0 + 9.4 * x;
        case Duffing:  return index == 0 ? 0.8 * x : index == 1 ? 0.6 * x : 0.4 + 1.6 * x; // gamma, delta, omega
        case Logistic: return 2.8 + 1.2 * x;                                                // r
        case Henon:    return index == 0 ? 1.0 + 0.8 * x : 0.6 * x;                        // a, b
        case DoublePendulum:
            return index < 4 ? expAround (1.0, 2.0, p)                                       // m or kg
                             : x * juce::MathConstants<double>::pi;                          // start angles
        case Bounce:
            switch (index)
            {
                case 0: return expAround (earthGravity, 2.0, p); // m/s^2
                case 1: return expAround (1.0, 2.0, p);          // drop height, m
                case 2: return 0.98 * x;                          // restitution
                default: return 3.0 * x * x;                      // air drag, 1/s
            }
        case Pendulum:
            switch (index)
            {
                case 0: return expAround (1.0, 3.0, p);          // length, m
                case 1: return expAround (earthGravity, 2.0, p); // gravity
                case 2: return 2.0 * x * x * x;                   // damping, 1/s
                case 3: return x * juce::degreesToRadians (179.0); // start angle
                case 4: return 1.5 * x;                           // drive, in units of g/L
                default: return expAround (1.0, 2.0, p);          // drive frequency / natural frequency
            }
        case Spring:
            switch (index)
            {
                case 0: return expAround (1.0, 3.0, p);                                          // kg
                case 1: return expAround (4.0 * juce::MathConstants<double>::pi * juce::MathConstants<double>::pi, 4.0, p); // N/m
                case 2: return x * x;                                                             // damping ratio
                default: return 2.0 * x - 1.0;                                                    // rest point
            }
        case Friction:
            switch (index)
            {
                case 0: return expAround (0.5, 3.0, p);  // drag speed, m/s
                case 1: return 0.1 + 0.9 * x;            // static coefficient
                case 2: return 0.05 + 0.9 * x;           // sliding / static
                case 3: return expAround (10.0, 3.0, p); // spring, N/m
                default: return expAround (1.0, 3.0, p); // kg
            }
        default: return x;
    }
}

inline juce::String text (int shape, int index, float p)
{
    using namespace LfoSimShapes;
    const auto v = value (shape, index, p);
    const auto num = [] (double n, int places) { return juce::String (n, places); };

    switch (shape)
    {
        case Perlin:    return index == 0 ? juce::String ((int) v) : num (v, 2);
        case DrunkWalk: return juce::String (juce::roundToInt (v * 100.0)) + "%";
        case Lorenz: case Rossler: case Henon: return num (v, 2);
        case Duffing:   return index == 2 ? num (v, 2) + " rad/s" : num (v, 2);
        case Logistic:  return "r " + num (v, 3);
        case DoublePendulum: return index < 2 ? num (v, 2) + " m" : index < 4 ? num (v, 2) + " kg"
                                                                                : juce::String (juce::roundToInt (juce::radiansToDegrees (v))) + juce::String::fromUTF8 ("\xc2\xb0");
        case Bounce:
            return index == 0 ? num (v, 2) + " m/s2" : index == 1 ? num (v, 2) + " m" : index == 2 ? num (v, 2)
                                                                                                    : num (v, 2) + " /s";
        case Pendulum:
            return index == 0 ? num (v, 2) + " m" : index == 1 ? num (v, 2) + " m/s2" : index == 2 ? num (v, 3) + " /s"
                 : index == 3 ? juce::String (juce::roundToInt (juce::radiansToDegrees (v))) + juce::String::fromUTF8 ("\xc2\xb0")
                 : index == 4 ? num (v, 2) + " g" : num (v, 2) + "x";
        case Spring:
            return index == 0 ? num (v, 2) + " kg" : index == 1 ? num (v, 1) + " N/m" : index == 2 ? num (v, 3) : num (v, 2);
        case Friction:
            return index == 0 ? num (v, 2) + " m/s" : index == 3 ? num (v, 1) + " N/m" : index == 4 ? num (v, 2) + " kg" : num (v, 2);
        default: return num (v, 2);
    }
}

// Model time per cycle of RATE for the chaotic systems (the physics
// objects run in seconds: RATE 1 Hz is real time).
inline double timePerCycle (int shape, double omega = 1.2)
{
    using namespace LfoSimShapes;
    switch (shape)
    {
        case Lorenz:  return 1.2;
        case Rossler: return 6.0;
        case Duffing: return juce::MathConstants<double>::twoPi / juce::jmax (0.05, omega);
        default:      return 1.0;
    }
}
} // namespace LfoSimInfo

// Per-block settings for one simulated LFO.
struct LfoSimSettings
{
    int shape = LfoSimShapes::RandomHold;
    std::array<float, LfoSimInfo::numParams> p { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    int axis = 0;          // 0 X, 1 Y, 2 Z, 3 mix
    bool loop = false;     // physics: start again once settled
    int seed = 0;          // 0: free (differs per voice); else a repeatable sequence
    float stereo = 0.0f;   // random family: how far B departs from A
};

// One-pole glide, used as the SMOOTH control on every LFO shape.
struct LfoSmoother
{
    float a = 0.0f, b = 0.0f;
    bool primed = false;

    void reset() { primed = false; }

    // coefficient: 1 - exp(-1 / (glide seconds * sample rate)), 1 = off
    static float coefficientFor (float smooth, double rateHz, double sampleRate)
    {
        if (smooth <= 0.0f)
            return 1.0f;
        const auto seconds = juce::jmin (4.0, (double) smooth / juce::jmax (0.01, rateHz));
        return (float) (1.0 - std::exp (-1.0 / juce::jmax (1.0, seconds * sampleRate)));
    }

    void process (float& valueA, float& valueB, float coefficient)
    {
        if (! primed)
        {
            a = valueA;
            b = valueB;
            primed = true;
        }
        a += (valueA - a) * coefficient;
        b += (valueB - b) * coefficient;
        valueA = a;
        valueB = b;
    }
};

class LfoSim
{
public:
    // Control rate for the differential equations: a new state every
    // `tick` samples, linearly interpolated in between.
    static constexpr int tick = 4;

    double sampleRate = 48000.0;

    // A fresh start (a new note on a per-voice LFO, or a new shape).
    // `freeSeed` seeds the generator when the settings' seed is 0.
    void reset (const LfoSimSettings& s, std::uint32_t freeSeed)
    {
        shape = s.shape;
        rng = s.seed > 0 ? hash ((std::uint32_t) s.seed * 2654435761u + 12345u) : hash (freeSeed + 0x9e3779b9u);
        if (rng == 0)
            rng = 0x1234567u;
        phaseAccumulator = 0.0;
        cycleCount = 0;
        tickCounter = 0;
        modelTime = 0.0;
        pulse = 0.0;
        startObject (s);
        computeOutputs (s);
        previousOut = out;
        smoother.reset();
    }

    // A trigger (note, beat, Generative step or the manual button): the
    // physics objects start again (the ball drops, the spring is plucked),
    // seeded sequences restart. `strength` scales a pendulum kick.
    void trigger (const LfoSimSettings& s, std::uint32_t freeSeed, bool kick = false, float strength = 1.0f)
    {
        using namespace LfoSimShapes;
        if (kick && s.shape == Pendulum)
        {
            const auto omega0 = std::sqrt (LfoSimInfo::value (Pendulum, 1, s.p[1]) / LfoSimInfo::value (Pendulum, 0, s.p[0]));
            st[1] += (double) strength * omega0;
            return;
        }
        if (isPhysics (s.shape) || s.seed > 0)
            reset (s, freeSeed);
    }

    // One sample. `cyclesPerSample` is RATE / sample rate. Returns A and B.
    void next (const LfoSimSettings& s, double cyclesPerSample, float& outA, float& outB)
    {
        using namespace LfoSimShapes;
        if (s.shape != shape)
            reset (s, rng);

        if (isStepped (shape))
        {
            phaseAccumulator += cyclesPerSample;
            while (phaseAccumulator >= 1.0)
            {
                phaseAccumulator -= 1.0;
                stepCycle (s);
            }
            computeOutputs (s);
            selectOutputs (s, out, outA, outB);
        }
        else
        {
            if (tickCounter == 0)
            {
                previousOut = out;
                advanceModel (s, cyclesPerSample * (double) tick);
                computeOutputs (s);
            }
            const auto t = (float) (tickCounter + 1) / (float) tick;
            std::array<float, 3> mixed;
            for (size_t i = 0; i < 3; ++i)
                mixed[i] = previousOut[i] + (out[i] - previousOut[i]) * t;

            // Impacts and slips are sharp: they decay per sample, in real time.
            if (shape == Bounce || shape == Friction)
            {
                mixed[1] = (float) pulse;
                pulse *= pulseDecay();
            }
            selectOutputs (s, mixed, outA, outB);
            tickCounter = (tickCounter + 1) % tick;
        }

        if (! std::isfinite (outA)) outA = 0.0f;
        if (! std::isfinite (outB)) outB = 0.0f;
        outA = juce::jlimit (-1.0f, 1.0f, outA);
        outB = juce::jlimit (-1.0f, 1.0f, outB);
    }

    // For the display and tests.
    const std::array<double, 4>& state() const { return st; }
    int getShape() const { return shape; }
    double getModelTime() const { return modelTime; }
    int getImpactCount() const { return impacts; }
    double getLastImpactTime() const { return lastImpactTime; }

    // --- The systems, public so tests can check them against theory ---
    static void lorenz (const double* y, double* d, double sigma, double rho, double beta)
    {
        d[0] = sigma * (y[1] - y[0]);
        d[1] = y[0] * (rho - y[2]) - y[1];
        d[2] = y[0] * y[1] - beta * y[2];
    }

    static void rossler (const double* y, double* d, double a, double b, double c)
    {
        d[0] = -y[1] - y[2];
        d[1] = y[0] + a * y[1];
        d[2] = b + y[2] * (y[0] - c);
    }

    // State: theta1, theta2, omega1, omega2.
    static void doublePendulum (const double* y, double* d, double l1, double l2, double m1, double m2, double g)
    {
        const auto delta = y[0] - y[1];
        const auto den = 2.0 * m1 + m2 - m2 * std::cos (2.0 * delta);
        d[0] = y[2];
        d[1] = y[3];
        d[2] = (-g * (2.0 * m1 + m2) * std::sin (y[0]) - m2 * g * std::sin (y[0] - 2.0 * y[1])
                - 2.0 * std::sin (delta) * m2 * (y[3] * y[3] * l2 + y[2] * y[2] * l1 * std::cos (delta)))
               / (l1 * den);
        d[3] = (2.0 * std::sin (delta) * (y[2] * y[2] * l1 * (m1 + m2) + g * (m1 + m2) * std::cos (y[0])
                                          + y[3] * y[3] * l2 * m2 * std::cos (delta)))
               / (l2 * den);
    }

    template <int N, typename F>
    static void rk4 (double* y, double h, F&& f)
    {
        double k1[N], k2[N], k3[N], k4[N], t[N];
        f (y, k1, 0.0);
        for (int i = 0; i < N; ++i) t[i] = y[i] + 0.5 * h * k1[i];
        f (t, k2, 0.5 * h);
        for (int i = 0; i < N; ++i) t[i] = y[i] + 0.5 * h * k2[i];
        f (t, k3, 0.5 * h);
        for (int i = 0; i < N; ++i) t[i] = y[i] + h * k3[i];
        f (t, k4, h);
        for (int i = 0; i < N; ++i)
            y[i] += h / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }

    LfoSmoother smoother;

private:
    int shape = -1;
    std::uint32_t rng = 1;
    std::array<double, 4> st {};  // the model's state
    std::array<float, 3> out {}, previousOut {};
    double phaseAccumulator = 0.0, modelTime = 0.0, pulse = 0.0;
    long long cycleCount = 0;
    int tickCounter = 0;
    // Random family: the values either side of the current cycle, for A and
    // for the stereo partner.
    float holdA0 = 0.0f, holdA1 = 0.0f, holdB0 = 0.0f, holdB1 = 0.0f;
    bool stuck = true, resting = false;
    int impacts = 0;
    double lastImpactTime = 0.0, firstSpeed = 1.0;

    static std::uint32_t hash (std::uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352du;
        x ^= x >> 15; x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    float nextRandom() // -1..1
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (float) ((double) rng / 4294967295.0 * 2.0 - 1.0);
    }

    double pulseDecay() const { return std::exp (-1.0 / (0.008 * sampleRate)); }

    double param (const LfoSimSettings& s, int index) const { return LfoSimInfo::value (shape, index, s.p[(size_t) index]); }

    void startObject (const LfoSimSettings& s)
    {
        using namespace LfoSimShapes;
        st = {};
        stuck = true;
        resting = false;
        impacts = 0;
        lastImpactTime = 0.0;

        switch (shape)
        {
            case RandomHold: case SineRandom: case DrunkWalk:
                holdA0 = holdA1 = nextRandom();
                holdB0 = holdB1 = nextRandom();
                if (shape == DrunkWalk)
                    holdB0 = holdB1 = holdA0;
                break;
            case Perlin:
                st[0] = (double) (rng % 10007u); // lattice offset: the seed picks the landscape
                break;
            case Lorenz:
                st = { 1.0 + 0.5 * (double) nextRandom(), 1.0 + 0.5 * (double) nextRandom(), 20.0 + (double) nextRandom(), 0.0 };
                break;
            case Rossler:
                st = { 1.0 + (double) nextRandom(), (double) nextRandom(), 0.1, 0.0 };
                break;
            case Duffing:
                st = { 1.0 + 0.1 * (double) nextRandom(), 0.0, 0.0, 0.0 }; // x, v, drive phase
                break;
            case Logistic:
                st = { 0.3 + 0.2 * (0.5 + 0.5 * (double) nextRandom()), 0.3, 0.0, 0.0 };
                break;
            case Henon:
                st = { 0.1 * (double) nextRandom(), 0.1 * (double) nextRandom(), 0.0, 0.0 };
                break;
            case DoublePendulum:
                st = { param (s, 4), param (s, 5), 0.0, 0.0 };
                break;
            case Bounce:
                st = { param (s, 1), 0.0, 0.0, 0.0 };
                firstSpeed = std::sqrt (2.0 * param (s, 0) * param (s, 1));
                break;
            case Pendulum:
                st = { param (s, 3), 0.0, 0.0, 0.0 };
                break;
            case Spring:
            {
                const auto rest = param (s, 3);
                st = { rest >= 0.0 ? -1.0 : 1.0, 0.0, 0.0, 0.0 };
                break;
            }
            case Friction:
                st = { 0.0, 0.0, 0.0, 0.0 }; // stretch, block velocity
                break;
            default: break;
        }
    }

    // Once per cycle of RATE (the random family and the maps).
    void stepCycle (const LfoSimSettings& s)
    {
        using namespace LfoSimShapes;
        ++cycleCount;

        switch (shape)
        {
            case RandomHold: case SineRandom:
                holdA0 = holdA1;
                holdA1 = nextRandom();
                holdB0 = holdB1;
                holdB1 = nextRandom();
                break;
            case DrunkWalk:
            {
                const auto step = (float) param (s, 0);
                const auto walk = [this, step] (float from)
                {
                    auto next = from + nextRandom() * step;
                    if (next > 1.0f) next = 2.0f - next;
                    if (next < -1.0f) next = -2.0f - next;
                    return juce::jlimit (-1.0f, 1.0f, next);
                };
                holdA0 = holdA1;
                holdA1 = walk (holdA1);
                holdB0 = holdB1;
                holdB1 = walk (holdB1);
                break;
            }
            case Logistic:
            {
                const auto r = param (s, 0);
                st[1] = st[0];
                st[0] = r * st[0] * (1.0 - st[0]);
                // 0 and 1 are fixed points the orbit can land on in floating point.
                if (! std::isfinite (st[0]) || st[0] <= 1.0e-9 || st[0] >= 1.0 - 1.0e-9)
                    st[0] = 0.25 + 0.5 * (0.5 + 0.5 * (double) nextRandom());
                break;
            }
            case Henon:
            {
                const auto a = param (s, 0), b = param (s, 1);
                const auto x = 1.0 - a * st[0] * st[0] + st[1];
                st[1] = b * st[0];
                st[0] = x;
                if (! std::isfinite (x) || std::abs (x) > 100.0)
                    st = { 0.1 * (double) nextRandom(), 0.0, 0.0, 0.0 };
                break;
            }
            default: break;
        }
    }

    static float perlinGradient (std::int64_t cell, std::uint32_t salt)
    {
        auto h = hash ((std::uint32_t) cell * 0x27d4eb2du ^ salt);
        return (float) ((double) h / 4294967295.0 * 2.0 - 1.0);
    }

    static float perlin1 (double position, std::uint32_t salt)
    {
        const auto cell = (std::int64_t) std::floor (position);
        const auto t = (float) (position - (double) cell);
        const auto g0 = perlinGradient (cell, salt) * t;
        const auto g1 = perlinGradient (cell + 1, salt) * (t - 1.0f);
        const auto fade = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        return 2.0f * (g0 + (g1 - g0) * fade); // about -1..1
    }

    float perlin (const LfoSimSettings& s, double position, std::uint32_t salt) const
    {
        const auto octaves = (int) param (s, 0);
        const auto persistence = param (s, 1);
        auto total = 0.0, amplitude = 1.0, norm = 0.0, frequency = 1.0;
        for (int o = 0; o < octaves; ++o)
        {
            total += amplitude * (double) perlin1 (position * frequency + (double) o * 31.7, salt + (std::uint32_t) o * 977u);
            norm += amplitude;
            amplitude *= persistence;
            frequency *= 2.0;
        }
        return (float) (1.3 * total / juce::jmax (1.0e-6, norm));
    }

    void computeOutputs (const LfoSimSettings& s)
    {
        using namespace LfoSimShapes;
        const auto t = (float) phaseAccumulator;

        switch (shape)
        {
            case RandomHold:
                out = { holdA1, holdB1, -holdA1 };
                break;
            case SineRandom: case DrunkWalk:
            {
                const auto w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t);
                out = { holdA0 + (holdA1 - holdA0) * w, holdB0 + (holdB1 - holdB0) * w, 0.0f };
                out[2] = -out[0];
                break;
            }
            case Perlin:
            {
                const auto position = (double) cycleCount + (double) t;
                const auto salt = (std::uint32_t) st[0];
                out = { perlin (s, position, salt), perlin (s, position + 517.3, salt ^ 0x5bd1e995u), 0.0f };
                out[2] = -out[0];
                break;
            }
            case Lorenz:
            {
                const auto rho = param (s, 1), beta = param (s, 2);
                const auto scale = rho > 1.0 ? std::sqrt (beta * (rho - 1.0)) : 1.0;
                out = { (float) (st[0] / (2.4 * juce::jmax (0.5, scale))), (float) (st[1] / (3.2 * juce::jmax (0.5, scale))),
                        (float) ((st[2] - (rho - 1.0)) / juce::jmax (1.0, 0.9 * (rho - 1.0))) };
                break;
            }
            case Rossler:
            {
                const auto c = param (s, 2);
                const auto scale = 1.8 * c + 2.0;
                out = { (float) (st[0] / scale), (float) (st[1] / scale), (float) (st[2] / (2.0 * c) - 1.0) };
                break;
            }
            case Duffing:
                out = { (float) (st[0] / 1.6), (float) (st[1] / 1.2), (float) std::cos (st[2]) };
                break;
            case Logistic:
                out = { (float) (2.0 * st[0] - 1.0), (float) (2.0 * st[1] - 1.0), 0.0f };
                break;
            case Henon:
                out = { (float) (st[0] / 1.3), (float) (st[1] / (1.3 * juce::jmax (0.05, param (s, 1)))), 0.0f };
                break;
            case DoublePendulum:
            {
                const auto l1 = param (s, 0), l2 = param (s, 1), total = l1 + l2;
                const auto x = l1 * std::sin (st[0]) + l2 * std::sin (st[1]);
                const auto y = -l1 * std::cos (st[0]) - l2 * std::cos (st[1]);
                out = { (float) (x / total), (float) (y / total), (float) std::sin (st[0]) };
                break;
            }
            case Bounce:
                out = { (float) (2.0 * st[0] / juce::jmax (1.0e-6, param (s, 1)) - 1.0), 0.0f,
                        (float) (st[1] / juce::jmax (1.0e-6, firstSpeed)) };
                break;
            case Pendulum:
            {
                const auto omega0 = std::sqrt (param (s, 1) / param (s, 0));
                out = { (float) std::sin (st[0]), (float) (st[1] / (2.0 * omega0)), (float) -std::cos (st[0]) };
                break;
            }
            case Spring:
            {
                const auto omega0 = std::sqrt (param (s, 1) / param (s, 0));
                out = { (float) st[0], (float) (st[1] / omega0), (float) -st[0] };
                break;
            }
            case Friction:
            {
                const auto m = param (s, 4), k = param (s, 3), muS = param (s, 1), muK = muS * param (s, 2);
                const auto high = muS * m * LfoSimInfo::earthGravity / k;
                const auto low = (2.0 * muK - muS) * m * LfoSimInfo::earthGravity / k;
                out = { (float) (2.0 * (st[0] - low) / juce::jmax (1.0e-9, high - low) - 1.0), 0.0f,
                        (float) (st[1] / juce::jmax (1.0e-6, 2.0 * param (s, 0))) };
                break;
            }
            default: out = {}; break;
        }
    }

    void selectOutputs (const LfoSimSettings& s, const std::array<float, 3>& o, float& a, float& b) const
    {
        using namespace LfoSimShapes;
        if (isRandom (shape))
        {
            a = o[0];
            b = o[0] + (o[1] - o[0]) * s.stereo;
            return;
        }
        const auto& info = LfoSimInfo::get (shape);
        const auto axis = info.usesAxis ? juce::jlimit (0, 3, s.axis) : 0;
        if (axis == 3)
        {
            a = 0.7071f * (o[0] + o[2]);
            b = o[1];
            return;
        }
        a = o[(size_t) axis];
        b = o[(size_t) (axis + 1) % 3];
    }

    // Advances the model by `cycles` of RATE.
    void advanceModel (const LfoSimSettings& s, double cycles)
    {
        using namespace LfoSimShapes;
        if (cycles <= 0.0)
            return;

        auto guard = [this, &s]
        {
            for (auto v : st)
                if (! std::isfinite (v) || std::abs (v) > 1.0e5)
                {
                    startObject (s);
                    return;
                }
        };

        switch (shape)
        {
            case Lorenz:
            {
                const auto sigma = param (s, 0), rho = param (s, 1), beta = param (s, 2);
                integrate<3> (cycles * LfoSimInfo::timePerCycle (shape), 0.004,
                              [=] (const double* y, double* d, double) { lorenz (y, d, sigma, rho, beta); });
                break;
            }
            case Rossler:
            {
                const auto a = param (s, 0), b = param (s, 1), c = param (s, 2);
                integrate<3> (cycles * LfoSimInfo::timePerCycle (shape), 0.02,
                              [=] (const double* y, double* d, double) { rossler (y, d, a, b, c); });
                break;
            }
            case Duffing:
            {
                // x'' + delta x' - x + x^3 = gamma cos(phi), phi' = omega
                const auto gamma = param (s, 0), delta = param (s, 1), omega = param (s, 2);
                integrate<3> (cycles * LfoSimInfo::timePerCycle (shape, omega), 0.02,
                              [=] (const double* y, double* d, double)
                              {
                                  d[0] = y[1];
                                  d[1] = -delta * y[1] + y[0] - y[0] * y[0] * y[0] + gamma * std::cos (y[2]);
                                  d[2] = omega;
                              });
                st[2] = std::fmod (st[2], juce::MathConstants<double>::twoPi);
                break;
            }
            case DoublePendulum:
            {
                const auto l1 = param (s, 0), l2 = param (s, 1), m1 = param (s, 2), m2 = param (s, 3);
                const auto omega = std::sqrt (LfoSimInfo::earthGravity / juce::jmin (l1, l2));
                integrate<4> (cycles, 0.02 / omega,
                              [=] (const double* y, double* d, double) { doublePendulum (y, d, l1, l2, m1, m2, LfoSimInfo::earthGravity); });
                break;
            }
            case Bounce: advanceBounce (s, cycles); break;
            case Pendulum:
            {
                const auto length = param (s, 0), g = param (s, 1), damp = param (s, 2), drive = param (s, 4);
                const auto omega0 = std::sqrt (g / length);
                const auto driveOmega = omega0 * param (s, 5);
                integrate<3> (cycles, 0.03 / omega0,
                              [=] (const double* y, double* d, double)
                              {
                                  d[0] = y[1];
                                  d[1] = -omega0 * omega0 * (std::sin (y[0]) - drive * std::cos (y[2])) - damp * y[1];
                                  d[2] = driveOmega;
                              });
                st[2] = std::fmod (st[2], juce::MathConstants<double>::twoPi);
                const auto energy = 0.5 * st[1] * st[1] + omega0 * omega0 * (1.0 - std::cos (st[0]));
                if (s.loop && drive == 0.0 && energy < 1.0e-4 * omega0 * omega0)
                    st[0] = param (s, 3), st[1] = 0.0;
                break;
            }
            case Spring:
            {
                const auto m = param (s, 0), k = param (s, 1), zeta = param (s, 2), rest = param (s, 3);
                const auto omega0 = std::sqrt (k / m);
                integrate<2> (cycles, 0.03 / omega0,
                              [=] (const double* y, double* d, double)
                              {
                                  d[0] = y[1];
                                  d[1] = -omega0 * omega0 * (y[0] - rest) - 2.0 * zeta * omega0 * y[1];
                              });
                const auto offset = st[0] - rest;
                if (s.loop && 0.5 * st[1] * st[1] + 0.5 * omega0 * omega0 * offset * offset < 1.0e-5 * omega0 * omega0)
                    st[0] = rest >= 0.0 ? -1.0 : 1.0, st[1] = 0.0;
                break;
            }
            case Friction: advanceFriction (s, cycles); break;
            default: break;
        }

        modelTime += cycles * (shape == Lorenz || shape == Rossler || shape == Duffing
                                   ? LfoSimInfo::timePerCycle (shape, param (s, 2)) : 1.0);
        guard();
    }

    template <int N, typename F>
    void integrate (double duration, double maxStep, F&& f)
    {
        const auto steps = juce::jlimit (1, 256, (int) std::ceil (duration / maxStep));
        const auto h = duration / (double) steps;
        for (int i = 0; i < steps; ++i)
            rk4<N> (st.data(), h, f);
    }

    // Exact flight under gravity and linear air drag; each impact is found
    // inside the step and the ball leaves at `elasticity` of its speed.
    void advanceBounce (const LfoSimSettings& s, double duration)
    {
        const auto g = param (s, 0), height = param (s, 1), e = param (s, 2), k = param (s, 3);
        firstSpeed = std::sqrt (2.0 * g * height);

        const auto flight = [g, k] (double y, double v, double t, double& yOut, double& vOut)
        {
            if (k < 1.0e-9)
            {
                yOut = y + v * t - 0.5 * g * t * t;
                vOut = v - g * t;
                return;
            }
            const auto terminal = g / k;
            const auto decay = std::exp (-k * t);
            vOut = (v + terminal) * decay - terminal;
            yOut = y + (v + terminal) * (1.0 - decay) / k - terminal * t;
        };

        auto remaining = duration;
        for (int guardCount = 0; guardCount < 8 && remaining > 0.0; ++guardCount)
        {
            if (resting)
            {
                st[0] = st[1] = 0.0;
                if (s.loop)
                {
                    startObject (s);
                    continue;
                }
                break;
            }

            double y1, v1;
            flight (st[0], st[1], remaining, y1, v1);
            if (y1 > 0.0)
            {
                st[0] = y1;
                st[1] = v1;
                modelTimeInStep += remaining;
                break;
            }

            // Find the impact: bisection on the flight's height.
            auto lo = 0.0, hi = remaining;
            for (int i = 0; i < 48; ++i)
            {
                const auto mid = 0.5 * (lo + hi);
                double ym, vm;
                flight (st[0], st[1], mid, ym, vm);
                (ym > 0.0 ? lo : hi) = mid;
            }
            double yi, vi;
            flight (st[0], st[1], hi, yi, vi);
            juce::ignoreUnused (yi);
            remaining -= hi;
            ++impacts;
            lastImpactTime = modelTime + modelTimeInStep + hi;
            modelTimeInStep += hi;
            pulse = juce::jmax (pulse, juce::jlimit (0.0, 1.0, -vi / juce::jmax (1.0e-9, firstSpeed)));

            const auto rebound = -vi * e;
            st[0] = 0.0;
            st[1] = rebound;
            // Settled: the next hop would last under a millisecond.
            if (rebound < 0.02 * firstSpeed || 2.0 * rebound / g < 1.0e-3)
                resting = true;
        }
        modelTimeInStep = 0.0;
    }

    double modelTimeInStep = 0.0;

    // Stick-slip: a block on a spring whose far end is dragged at SPEED.
    // It sticks until the spring beats static friction, then slides against
    // sliding friction until it stops relative to the surface.
    void advanceFriction (const LfoSimSettings& s, double duration)
    {
        const auto speed = param (s, 0), muS = param (s, 1), muK = muS * param (s, 2);
        const auto k = param (s, 3), m = param (s, 4), g = LfoSimInfo::earthGravity;
        const auto staticLimit = muS * m * g;
        const auto omega0 = std::sqrt (k / m);
        const auto h = juce::jmin (duration, 0.02 / omega0);
        auto remaining = duration;

        while (remaining > 1.0e-12)
        {
            const auto step = juce::jmin (h, remaining);
            remaining -= step;

            if (stuck)
            {
                // The driver stretches the spring at a steady rate.
                const auto toSlip = (staticLimit / k - st[0]) / speed;
                if (toSlip > step)
                {
                    st[0] += speed * step;
                    st[1] = 0.0;
                    continue;
                }
                st[0] = staticLimit / k;
                remaining += step - juce::jmax (0.0, toSlip);
                stuck = false;
                ++impacts;
                lastImpactTime = modelTime;
                pulse = 1.0;
                continue;
            }

            // Sliding: x' = v (block), stretch' = speed - v,
            // m v' = k stretch - muK m g sign(v).
            const auto before = st[1];
            rk4<2> (st.data(), step, [=] (const double* y, double* d, double)
            {
                d[0] = speed - y[1];
                d[1] = (k * y[0] - muK * m * g * (y[1] >= 0.0 ? 1.0 : -1.0)) / m;
            });
            if (before > 0.0 && st[1] <= 0.0 && std::abs (k * st[0]) <= staticLimit)
            {
                stuck = true;
                st[1] = 0.0;
            }
        }
    }
};
