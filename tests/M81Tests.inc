// M8.1: the chaos and physics modulators, checked against their equations.
// Included by TableTest.cpp (uses its check() and setParam()).

namespace M81
{
constexpr double sr = 48000.0;

LfoSimSettings settingsFor (int shape)
{
    LfoSimSettings s;
    s.shape = shape;
    const auto& info = LfoSimInfo::get (shape);
    for (int i = 0; i < LfoSimInfo::numParams; ++i)
        s.p[(size_t) i] = info.params[(size_t) i].defaultValue;
    return s;
}

// Normalised knob value for a physical value on an expAround() knob.
float expKnob (double centre, double octaves, double value)
{
    return (float) (0.5 + std::log2 (value / centre) / (2.0 * octaves));
}

// Runs a simulation at RATE `rate` for `seconds`, calling f (time, a, b).
template <typename F>
void run (LfoSim& sim, const LfoSimSettings& s, double rate, double seconds, F&& f)
{
    sim.sampleRate = sr;
    const auto samples = (int) (seconds * sr);
    for (int i = 0; i < samples; ++i)
    {
        float a = 0.0f, b = 0.0f;
        sim.next (s, rate / sr, a, b);
        f ((double) (i + 1) / sr, a, b);
    }
}

// Times of upward zero crossings of a (linear interpolation).
std::vector<double> upCrossings (const std::vector<float>& values, double dt)
{
    std::vector<double> times;
    for (size_t i = 1; i < values.size(); ++i)
        if (values[i - 1] < 0.0f && values[i] >= 0.0f)
            times.push_back (dt * ((double) (i - 1) + (double) (-values[i - 1] / (values[i] - values[i - 1]))));
    return times;
}

double meanPeriod (const std::vector<double>& crossings)
{
    if (crossings.size() < 3)
        return 0.0;
    return (crossings.back() - crossings.front()) / (double) (crossings.size() - 1);
}

void bounceTests()
{
    auto s = settingsFor (LfoSimShapes::Bounce);
    s.p[3] = 0.0f; // no air drag: the analytic series applies
    const auto g = LfoSimInfo::value (LfoSimShapes::Bounce, 0, s.p[0]);
    const auto h = LfoSimInfo::value (LfoSimShapes::Bounce, 1, s.p[1]);
    const auto e = LfoSimInfo::value (LfoSimShapes::Bounce, 2, s.p[2]);
    check (std::abs (g - 9.81) < 1.0e-9 && std::abs (h - 1.0) < 1.0e-9, "Bounce defaults: g 9.81 m/s2, drop 1 m");

    LfoSim sim;
    sim.sampleRate = sr;
    sim.reset (s, 1);
    std::vector<double> impactTimes;
    std::vector<float> pulses, heights;
    auto lastCount = 0, sampleCount = 0;
    auto apex = -1.0f, previous = 1.0f;
    auto rising = false;
    run (sim, s, 1.0, 6.0, [&] (double, float a, float b)
    {
        ++sampleCount;
        if (sim.getImpactCount() != lastCount)
        {
            lastCount = sim.getImpactCount();
            impactTimes.push_back (sim.getLastImpactTime());
        }
        pulses.push_back (b);
        if (a > previous) rising = true;
        else if (a < previous && rising) { heights.push_back (0.5f * (previous + 1.0f)); rising = false; }
        previous = a;
        juce::ignoreUnused (apex);
    });

    // t1 = sqrt(2h/g); each hop after n impacts lasts 2 e^n v0 / g.
    const auto v0 = std::sqrt (2.0 * g * h);
    auto expected = std::sqrt (2.0 * h / g);
    auto worst = 0.0;
    for (size_t n = 0; n < juce::jmin ((size_t) 6, impactTimes.size()); ++n)
    {
        worst = juce::jmax (worst, std::abs (impactTimes[n] - expected));
        expected += 2.0 * std::pow (e, (double) n + 1.0) * v0 / g;
    }
    check (impactTimes.size() >= 6 && worst < 1.0e-4,
           "Bounce impact times match t1 = sqrt(2h/g), hops 2 e^n v0/g (" + juce::String ((int) impactTimes.size())
               + " impacts, worst error " + juce::String (worst * 1000.0, 4) + " ms)");

    auto heightError = 0.0;
    for (size_t n = 0; n < juce::jmin ((size_t) 4, heights.size()); ++n)
        heightError = juce::jmax (heightError, std::abs ((double) heights[n] - std::pow (e, 2.0 * ((double) n + 1.0))));
    check (heights.size() >= 4 && heightError < 0.01, "Bounce apex heights follow e^2n h (worst error "
                                                          + juce::String (heightError, 4) + ")");

    const auto firstPulse = *std::max_element (pulses.begin(), pulses.begin() + (int) (0.5 * sr));
    check (std::abs (firstPulse - 1.0f) < 0.02f, "Bounce IMPACT output is 1 at the first impact (" + juce::String (firstPulse, 3) + ")");

    float a = 0.0f, b = 0.0f;
    sim.next (s, 1.0 / sr, a, b);
    check (a == -1.0f, "Bounce settles and stays down (loop off)");

    sim.trigger (s, 2);
    sim.next (s, 1.0 / sr, a, b);
    check (a > 0.99f, "a trigger drops the ball again from the top");

    // Loop on: it starts again once settled.
    s.loop = true;
    sim.reset (s, 3);
    auto late = -1.0f;
    run (sim, s, 1.0, 8.0, [&] (double t, float out, float) { if (t > 6.0) late = juce::jmax (late, out); });
    check (late > 0.5f, "Bounce with LOOP starts again once settled");

    // RATE is the time scale: 2 Hz halves the first fall.
    s.loop = false;
    sim.reset (s, 4);
    lastCount = 0;
    auto first = 0.0;
    run (sim, s, 2.0, 1.0, [&] (double t, float, float) { if (first == 0.0 && sim.getImpactCount() > 0) first = t; });
    check (std::abs (first - 0.5 * std::sqrt (2.0 * h / g)) < 0.002, "RATE 2 Hz runs Bounce at twice real time");

    // Air drag: the first fall is slower than without.
    s.p[3] = 0.8f;
    sim.reset (s, 5);
    first = 0.0;
    run (sim, s, 1.0, 2.0, [&] (double t, float, float) { if (first == 0.0 && sim.getImpactCount() > 0) first = t; });
    check (first > std::sqrt (2.0 * h / g) + 0.005, "air DRAG slows the fall");
}

void pendulumTests()
{
    auto s = settingsFor (LfoSimShapes::Pendulum);
    s.p[2] = 0.0f;                     // no damping
    s.p[3] = (float) (5.0 / 179.0);    // 5 degrees: small-angle period

    for (const auto length : { 1.0, 4.0, 0.25 })
    {
        s.p[0] = expKnob (1.0, 3.0, length);
        LfoSim sim;
        sim.reset (s, 1);
        std::vector<float> values;
        run (sim, s, 1.0, 12.0, [&] (double, float a, float) { values.push_back (a); });
        const auto period = meanPeriod (upCrossings (values, 1.0 / sr));
        const auto theory = juce::MathConstants<double>::twoPi * std::sqrt (length / 9.81)
                            * (1.0 + std::pow (juce::degreesToRadians (5.0), 2.0) / 16.0);
        check (std::abs (period / theory - 1.0) < 0.002,
               "Pendulum " + juce::String (length, 2) + " m: period " + juce::String (period, 4) + " s, theory "
                   + juce::String (theory, 4) + " s");
    }

    // Large swing: 90 degrees is 1.18034 times the small-angle period.
    s.p[0] = 0.5f;
    s.p[3] = (float) (90.0 / 179.0);
    LfoSim sim;
    sim.reset (s, 1);
    std::vector<float> values;
    run (sim, s, 1.0, 12.0, [&] (double, float a, float) { values.push_back (a); });
    const auto period = meanPeriod (upCrossings (values, 1.0 / sr));
    const auto theory = juce::MathConstants<double>::twoPi * std::sqrt (1.0 / 9.81) * 1.18034;
    check (std::abs (period / theory - 1.0) < 0.003, "Pendulum at 90 degrees: period " + juce::String (period, 4)
                                                         + " s against the elliptic-integral " + juce::String (theory, 4) + " s");

    // Damping: the swing decays; GRAVITY scales the period by 1/sqrt(g).
    s.p[2] = 0.6f;
    sim.reset (s, 1);
    auto late = 0.0f;
    run (sim, s, 1.0, 30.0, [&] (double t, float a, float) { if (t > 29.0) late = juce::jmax (late, std::abs (a)); });
    check (late < 0.2f, "Pendulum DAMP settles the swing (" + juce::String (late, 3) + " after 30 s)");
}

void springTests()
{
    auto s = settingsFor (LfoSimShapes::Spring);
    s.p[2] = 0.0f; // undamped
    LfoSim sim;
    sim.reset (s, 1);
    std::vector<float> values;
    run (sim, s, 1.0, 10.0, [&] (double, float a, float) { values.push_back (a); });
    const auto period = meanPeriod (upCrossings (values, 1.0 / sr));
    check (std::abs (period - 1.0) < 0.001, "Spring 1 kg, 4 pi^2 N/m rings at 1 Hz (" + juce::String (period, 5) + " s)");

    // Damping ratio: successive peaks fall by exp(-2 pi zeta / sqrt(1 - zeta^2)).
    s.p[2] = (float) std::sqrt (0.05);
    sim.reset (s, 1);
    std::vector<float> trace, peaks;
    run (sim, s, 1.0, 5.0, [&] (double, float a, float) { trace.push_back (a); });
    // One peak per period (1 s): the largest value in each second.
    for (size_t second = 0; second + 1 <= trace.size() / (size_t) sr; ++second)
        peaks.push_back (*std::max_element (trace.begin() + (long) (second * (size_t) sr),
                                            trace.begin() + (long) ((second + 1) * (size_t) sr)));
    const auto expected = std::exp (-juce::MathConstants<double>::twoPi * 0.05 / std::sqrt (1.0 - 0.0025));
    const auto measured = peaks.size() >= 3 ? (double) peaks[2] / (double) peaks[1] : 0.0;
    check (std::abs (measured - expected) < 0.01, "Spring damping ratio 0.05: peak ratio " + juce::String (measured, 4)
                                                      + ", theory " + juce::String (expected, 4));

    auto last = 1.0f;
    run (sim, s, 1.0, 40.0, [&] (double, float a, float) { last = a; });
    check (std::abs (last) < 0.01f, "Spring settles at its REST point and stays (loop off)");
    sim.trigger (s, 1);
    float a = 0.0f, b = 0.0f;
    sim.next (s, 1.0 / sr, a, b);
    check (std::abs (std::abs (a) - 1.0f) < 0.01f, "a trigger plucks the spring again");
}

void frictionTests()
{
    const auto s = settingsFor (LfoSimShapes::Friction);
    LfoSim sim;
    sim.reset (s, 1);
    std::vector<double> slips;
    auto count = 0;
    auto highest = -2.0f;
    run (sim, s, 1.0, 30.0, [&] (double t, float a, float)
    {
        highest = juce::jmax (highest, a);
        if (sim.getImpactCount() != count) { count = sim.getImpactCount(); slips.push_back (t); }
    });

    // The first slip comes when the spring force reaches static friction:
    // stretch mu_s m g / k, reached at SPEED after stretch / speed seconds.
    const auto speed = LfoSimInfo::value (LfoSimShapes::Friction, 0, s.p[0]);
    const auto muS = LfoSimInfo::value (LfoSimShapes::Friction, 1, s.p[1]);
    const auto k = LfoSimInfo::value (LfoSimShapes::Friction, 3, s.p[3]);
    const auto m = LfoSimInfo::value (LfoSimShapes::Friction, 4, s.p[4]);
    const auto firstSlip = muS * m * 9.81 / k / speed;
    check (! slips.empty() && std::abs (slips.front() - firstSlip) < 0.002,
           "Friction slips when the spring beats static friction (" + juce::String (slips.empty() ? 0.0 : slips.front(), 4)
               + " s, theory " + juce::String (firstSlip, 4) + " s)");

    auto steady = slips.size() >= 6;
    for (size_t i = 3; i < slips.size(); ++i)
        steady = steady && std::abs ((slips[i] - slips[i - 1]) - (slips[2] - slips[1])) < 0.01;
    check (steady, "Friction settles into a regular stick-slip cycle (" + juce::String ((int) slips.size()) + " slips)");
    check (highest > 0.95f, "Friction STRETCH output fills the range (peak " + juce::String (highest, 3) + ")");
}

double lorenzLyapunov()
{
    // Benettin's method: two trajectories 1e-8 apart, renormalised every
    // 0.1 time units, after 50 units of settling.
    double a[3] { 1.0, 1.0, 20.0 };
    const auto f = [] (const double* y, double* d, double) { LfoSim::lorenz (y, d, 10.0, 28.0, 8.0 / 3.0); };
    for (int i = 0; i < 50000; ++i)
        LfoSim::rk4<3> (a, 0.001, f);
    double b[3] { a[0] + 1.0e-8, a[1], a[2] };
    auto sum = 0.0;
    constexpr int intervals = 3000;
    for (int n = 0; n < intervals; ++n)
    {
        for (int i = 0; i < 100; ++i)
        {
            LfoSim::rk4<3> (a, 0.001, f);
            LfoSim::rk4<3> (b, 0.001, f);
        }
        const auto dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
        const auto d = std::sqrt (dx * dx + dy * dy + dz * dz);
        sum += std::log (d / 1.0e-8);
        for (int i = 0; i < 3; ++i)
            b[i] = a[i] + (b[i] - a[i]) * 1.0e-8 / d;
    }
    return sum / (intervals * 0.1);
}

// Fraction of samples pinned at +-1 and the range covered.
struct Coverage { float low = 1.0f, high = -1.0f; double pinned = 0.0; bool finite = true; };

Coverage coverage (const LfoSimSettings& s, double rate, double seconds, bool outputB = false)
{
    LfoSim sim;
    sim.reset (s, 11);
    Coverage c;
    auto count = 0;
    run (sim, s, rate, seconds, [&] (double, float a, float b)
    {
        const auto v = outputB ? b : a;
        c.finite = c.finite && std::isfinite (v);
        c.low = juce::jmin (c.low, v);
        c.high = juce::jmax (c.high, v);
        if (std::abs (v) >= 1.0f) ++count;
    });
    c.pinned = (double) count / (seconds * sr);
    return c;
}

void chaosTests()
{
    const auto lyapunov = lorenzLyapunov();
    check (std::abs (lyapunov - 0.906) < 0.05, "Lorenz's largest Lyapunov exponent is about 0.9 (" + juce::String (lyapunov, 3) + ")");

    for (const auto shape : { LfoSimShapes::Lorenz, LfoSimShapes::Rossler, LfoSimShapes::Duffing, LfoSimShapes::Henon,
                              LfoSimShapes::Logistic, LfoSimShapes::DoublePendulum })
    {
        auto s = settingsFor (shape);
        if (shape == LfoSimShapes::Logistic)
            s.p[0] = 0.9f; // r = 3.88: chaos (r = 4 piles up at the ends)
        for (int axis = 0; axis < (LfoSimInfo::get (shape).usesAxis ? 3 : 1); ++axis)
        {
            s.axis = axis;
            const auto c = coverage (s, LfoSimShapes::isMap (shape) ? 40.0 : 1.0, 60.0);
            check (c.finite && c.pinned < 0.002 && c.high - c.low > 0.9f,
                   juce::String (LfoSimInfo::get (shape).name) + " axis " + juce::String (axis) + " fills -1..1 without clipping (range "
                       + juce::String (c.low, 2) + ".." + juce::String (c.high, 2) + ", pinned "
                       + juce::String (c.pinned * 100.0, 3) + "%)");
        }
    }

    // Logistic map: period 2 at r = 3.2, period 4 at r = 3.5.
    for (const auto [r, period] : { std::pair<double, int> { 3.2, 2 }, std::pair<double, int> { 3.5, 4 } })
    {
        auto s = settingsFor (LfoSimShapes::Logistic);
        s.p[0] = (float) ((r - 2.8) / 1.2);
        LfoSim sim;
        sim.reset (s, 1);
        std::vector<float> values;
        // RATE 100 Hz: one step every 480 samples; sample the middle of each.
        auto n = 0;
        run (sim, s, 100.0, 3.0, [&] (double, float a, float) { if (++n % 480 == 240) values.push_back (a); });
        auto periodic = values.size() > 200;
        for (size_t i = 200; i + (size_t) period < values.size(); ++i)
            periodic = periodic && std::abs (values[i] - values[i + (size_t) period]) < 1.0e-4f;
        auto notShorter = values.size() > 200 && std::abs (values[200] - values[200 + (size_t) period / 2]) > 1.0e-3f;
        check (periodic && notShorter, "Logistic map at r = " + juce::String (r, 1) + " settles into period " + juce::String (period));
    }

    // Henon at a = 1.4, b = 0.3 stays on the attractor.
    {
        auto s = settingsFor (LfoSimShapes::Henon);
        const auto a = LfoSimInfo::value (LfoSimShapes::Henon, 0, s.p[0]), b = LfoSimInfo::value (LfoSimShapes::Henon, 1, s.p[1]);
        check (std::abs (a - 1.4) < 1.0e-6 && std::abs (b - 0.3) < 1.0e-6, "Henon defaults are the classic a = 1.4, b = 0.3");
    }

    // Double pendulum: energy is conserved (RK4, no damping).
    {
        double y[4] { 2.0, 2.5, 0.0, 0.0 };
        const auto energy = [] (const double* s)
        {
            const auto v1sq = s[2] * s[2];
            const auto v2sq = s[2] * s[2] + s[3] * s[3] + 2.0 * s[2] * s[3] * std::cos (s[0] - s[1]);
            return 0.5 * v1sq + 0.5 * v2sq - 9.81 * (2.0 * std::cos (s[0]) + std::cos (s[1]));
        };
        const auto before = energy (y);
        for (int i = 0; i < 20000; ++i)
            LfoSim::rk4<4> (y, 0.0005, [] (const double* s, double* d, double) { LfoSim::doublePendulum (s, d, 1.0, 1.0, 1.0, 1.0, 9.81); });
        const auto drift = std::abs (energy (y) - before) / std::abs (before);
        check (drift < 1.0e-4, "Double pendulum conserves energy over 10 s (drift " + juce::String (drift * 100.0, 5) + "%)");
    }

    // Duffing walks from a steady swing into chaos as DRIVE rises: at low
    // drive the orbit repeats every drive period.
    {
        auto s = settingsFor (LfoSimShapes::Duffing);
        s.p[0] = (float) (0.2 / 0.8); // gamma 0.2: period 1
        LfoSim sim;
        sim.reset (s, 1);
        std::vector<float> strobe;
        auto n = 0;
        run (sim, s, 10.0, 20.0, [&] (double, float a, float) { if (++n % 4800 == 0) strobe.push_back (a); });
        auto spread = 0.0f;
        for (size_t i = strobe.size() / 2; i < strobe.size(); ++i)
            spread = juce::jmax (spread, std::abs (strobe[i] - strobe.back()));
        s.p[0] = 0.625f; // gamma 0.5: chaos
        sim.reset (s, 1);
        strobe.clear();
        n = 0;
        run (sim, s, 10.0, 20.0, [&] (double, float a, float) { if (++n % 4800 == 0) strobe.push_back (a); });
        auto chaosSpread = 0.0f;
        for (size_t i = strobe.size() / 2; i < strobe.size(); ++i)
            chaosSpread = juce::jmax (chaosSpread, std::abs (strobe[i] - strobe.back()));
        check (spread < 0.01f && chaosSpread > 0.2f, "Duffing: drive 0.2 repeats every cycle (" + juce::String (spread, 4)
                                                          + "), drive 0.5 never does (" + juce::String (chaosSpread, 3) + ")");
    }
}

void randomTests()
{
    for (const auto shape : { LfoSimShapes::RandomHold, LfoSimShapes::SineRandom, LfoSimShapes::Perlin, LfoSimShapes::DrunkWalk })
    {
        auto s = settingsFor (shape);
        s.seed = 42;
        LfoSim one, two;
        one.reset (s, 1);
        two.reset (s, 999);
        std::vector<float> first, second;
        run (one, s, 5.0, 4.0, [&] (double, float a, float) { first.push_back (a); });
        run (two, s, 5.0, 4.0, [&] (double, float a, float) { second.push_back (a); });
        check (first == second, juce::String (LfoSimInfo::get (shape).name) + ": the same SEED gives the same sequence on every voice");

        s.seed = 0;
        one.reset (s, 1);
        two.reset (s, 2);
        first.clear(); second.clear();
        run (one, s, 5.0, 4.0, [&] (double, float a, float) { first.push_back (a); });
        run (two, s, 5.0, 4.0, [&] (double, float a, float) { second.push_back (a); });
        check (first != second, juce::String (LfoSimInfo::get (shape).name) + ": SEED 0 differs from voice to voice");

        auto maxStep = 0.0f, stereoGap = 0.0f;
        auto previous = std::numeric_limits<float>::quiet_NaN();
        s.stereo = 0.0f;
        one.reset (s, 3);
        auto same = true;
        run (one, s, 5.0, 2.0, [&] (double, float a, float b) { same = same && a == b; });
        s.stereo = 1.0f;
        one.reset (s, 3);
        run (one, s, 5.0, 4.0, [&] (double, float a, float b)
        {
            if (! std::isnan (previous))
                maxStep = juce::jmax (maxStep, std::abs (a - previous));
            previous = a;
            stereoGap = juce::jmax (stereoGap, std::abs (a - b));
        });
        check (same && stereoGap > 0.2f, juce::String (LfoSimInfo::get (shape).name) + ": STEREO 0 keeps B on A, STEREO 1 departs ("
                                             + juce::String (stereoGap, 2) + ")");
        if (shape != LfoSimShapes::RandomHold)
            check (maxStep < 0.01f, juce::String (LfoSimInfo::get (shape).name) + " is smooth at 5 Hz (largest step "
                                        + juce::String (maxStep, 4) + ")");
    }

    // SMOOTH turns S&H into a glide.
    {
        auto s = settingsFor (LfoSimShapes::RandomHold);
        LfoSim sim;
        sim.reset (s, 7);
        LfoSmoother smoother;
        const auto coefficient = LfoSmoother::coefficientFor (0.3f, 5.0, sr);
        auto rawStep = 0.0f, smoothStep = 0.0f, rawPrev = 0.0f, smoothPrev = 0.0f;
        run (sim, s, 5.0, 4.0, [&] (double t, float a, float b)
        {
            const auto raw = a;
            smoother.process (a, b, coefficient);
            if (t > 0.01)
            {
                rawStep = juce::jmax (rawStep, std::abs (raw - rawPrev));
                smoothStep = juce::jmax (smoothStep, std::abs (a - smoothPrev));
            }
            rawPrev = raw;
            smoothPrev = a;
        });
        check (rawStep > 0.2f && smoothStep < 0.01f, "SMOOTH glides S&H (largest step " + juce::String (rawStep, 3) + " -> "
                                                        + juce::String (smoothStep, 4) + ")");
    }
}

void stabilityTests()
{
    juce::Random random (81);
    auto allFinite = true;
    auto worst = juce::String();
    for (int shape = LfoSimShapes::RandomHold; shape < LfoSimShapes::End; ++shape)
    {
        for (int trial = 0; trial < 12; ++trial)
        {
            LfoSimSettings s;
            s.shape = shape;
            for (auto& p : s.p)
                p = trial == 0 ? 0.0f : trial == 1 ? 1.0f : random.nextFloat();
            s.axis = trial % 4;
            s.loop = trial % 2 == 0;
            s.stereo = random.nextFloat();
            const auto rate = trial < 4 ? (trial % 2 == 0 ? 0.01 : 200.0) : 0.01 * std::pow (20000.0, (double) random.nextFloat());
            LfoSim sim;
            sim.reset (s, (std::uint32_t) trial);
            auto ok = true;
            run (sim, s, rate, 3.0, [&] (double, float a, float b)
            {
                ok = ok && std::isfinite (a) && std::isfinite (b) && std::abs (a) <= 1.0f && std::abs (b) <= 1.0f;
            });
            if (! ok)
                worst = juce::String (LfoSimInfo::get (shape).name) + " trial " + juce::String (trial);
            allFinite = allFinite && ok;
        }
    }
    check (allFinite, "no NaN or runaway in any simulated shape at any setting or rate" + (worst.isEmpty() ? juce::String() : " (" + worst + ")"));
}

float renderPeak (IlanaSynthAudioProcessor& processor, int blocks, bool notes = true)
{
    juce::AudioBuffer<float> buffer (2, 512);
    auto peak = 0.0f;
    for (int block = 0; block < blocks; ++block)
    {
        juce::MidiBuffer midi;
        if (notes && block == 0)
            for (int note : { 48, 55, 60, 64, 67, 72, 76, 79 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto v = buffer.getSample (ch, i);
                if (! std::isfinite (v))
                    return 1.0e9f;
                peak = juce::jmax (peak, std::abs (v));
            }
    }
    return peak;
}

void processorTests()
{
    // Output B as a mod source, synth-wide and per voice.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, "lfo1_shape", (float) LfoSimShapes::Bounce);
        setParam (processor, "lfo1_trigger", 0.0f);
        setParam (processor, "mod1_src", (float) Mod::Source::Lfo1B);
        setParam (processor, "mod1_dst", (float) Mod::Destination::Filter1Cutoff);
        setParam (processor, "mod1_amt", 0.5f);
        setParam (processor, "lfo1_rate", 1.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        auto impact = 0.0f;
        for (int block = 0; block < 94; ++block) // about one second
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            impact = juce::jmax (impact, processor.getLfoLiveValueB (0));
        }
        check (impact > 0.01f, "LFO 1 B carries Bounce's impacts to the matrix (" + juce::String (impact, 3) + ")");
    }

    // Every LFO chaotic, per voice, at extreme rates: stable, and the CPU
    // cost is reported.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        const int shapes[] { LfoSimShapes::Lorenz, LfoSimShapes::Rossler, LfoSimShapes::Duffing, LfoSimShapes::DoublePendulum,
                             LfoSimShapes::Bounce, LfoSimShapes::Pendulum, LfoSimShapes::Spring, LfoSimShapes::Friction };
        for (int lfo = 1; lfo <= 16; ++lfo)
        {
            const auto prefix = "lfo" + juce::String (lfo);
            setParam (processor, prefix + "_shape", (float) shapes[(lfo - 1) % 8]);
            setParam (processor, prefix + "_retrig", 1.0f);
            setParam (processor, prefix + "_rate", lfo % 2 == 0 ? 40.0f : 0.05f);
            setParam (processor, prefix + "_smooth", 0.2f);
            setParam (processor, prefix + "_p1", 1.0f);
            setParam (processor, "mod" + juce::String (lfo) + "_src", (float) Mod::lfoSourceFor (lfo - 1));
            setParam (processor, "mod" + juce::String (lfo) + "_dst", (float) Mod::Destination::Filter1Cutoff);
            setParam (processor, "mod" + juce::String (lfo) + "_amt", 0.05f);
            setParam (processor, "mod" + juce::String (16 + lfo) + "_src", (float) Mod::lfoBSourceFor (lfo - 1));
            setParam (processor, "mod" + juce::String (16 + lfo) + "_dst", (float) Mod::Destination::Pan);
            setParam (processor, "mod" + juce::String (16 + lfo) + "_amt", 0.05f);
        }
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto peak = renderPeak (processor, 188); // 2 s
        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        check (peak < 100.0f, "16 chaotic LFOs per voice (A and B routed) stay stable (peak " + juce::String (peak, 2) + ")");
        std::cout << "INFO: 8 voices x 16 chaotic LFOs: " << juce::String (elapsed / 2.0 * 100.0, 1) << "% of real time" << std::endl;
        checkTiming (elapsed < 2.0, "16 chaotic LFOs per voice render faster than real time");
    }

    // Beat trigger: a free-running Spring is plucked on each beat.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, "lfo1_shape", (float) LfoSimShapes::Spring);
        setParam (processor, "lfo1_trigger", 2.0f);
        setParam (processor, "lfo1_div", 2.0f); // 1/4
        setParam (processor, "lfo1_p3", 0.8f);  // heavy damping: settles within a beat
        setParam (processor, "mod1_src", (float) Mod::Source::Lfo1);
        setParam (processor, "mod1_dst", (float) Mod::Destination::Pan);
        setParam (processor, "mod1_amt", 0.1f);
        juce::AudioBuffer<float> buffer (2, 512);
        auto plucks = 0;
        auto previous = 0.0f;
        for (int block = 0; block < 94 * 4; ++block)
        {
            juce::MidiBuffer midi;
            buffer.clear();
            processor.processBlock (buffer, midi);
            const auto value = std::abs (processor.getLfoLiveValue (0));
            if (value > 0.5f && previous < 0.5f)
                ++plucks;
            previous = value;
        }
        // 120 BPM by default: 8 beats in 4 s.
        check (plucks >= 6 && plucks <= 9, "BEAT trigger plucks the spring on every beat (" + juce::String (plucks) + " in 4 s)");
    }

    // Classic shapes are untouched by the new controls at their defaults,
    // and SMOOTH glides a classic square.
    {
        IlanaSynthAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, "lfo1_shape", 4.0f); // square
        setParam (processor, "lfo1_rate", 2.0f);
        setParam (processor, "lfo1_smooth", 0.25f);
        setParam (processor, "mod1_src", (float) Mod::Source::Lfo1);
        setParam (processor, "mod1_dst", (float) Mod::Destination::Pan);
        setParam (processor, "mod1_amt", 0.1f);
        juce::AudioBuffer<float> buffer (2, 64);
        auto previous = processor.getLfoLiveValue (0);
        auto largest = 0.0f;
        for (int block = 0; block < 3000; ++block)
        {
            juce::MidiBuffer midi;
            buffer.clear();
            processor.processBlock (buffer, midi);
            const auto value = processor.getLfoLiveValue (0);
            if (block > 10)
                largest = juce::jmax (largest, std::abs (value - previous));
            previous = value;
        }
        check (largest < 0.2f, "SMOOTH glides the classic square (largest step per 64 samples " + juce::String (largest, 3) + ")");
    }
}
} // namespace M81

void runM81ModulatorTests()
{
    M81::bounceTests();
    M81::pendulumTests();
    M81::springTests();
    M81::frictionTests();
    M81::chaosTests();
    M81::randomTests();
    M81::stabilityTests();
    M81::processorTests();
}
