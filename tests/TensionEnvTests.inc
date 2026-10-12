// The tension envelope (TensionAdsr, roadmap step 46): the curve follows the
// closed form (e^(k q) - 1) / (e^k - 1) sample by sample, the straight line
// (CURVE 0) is exact, the control-rate path (skip) lands where the
// per-sample path does, and note off, retrigger and a changed length never
// jump. Included by TableTest.cpp; ILANA_TENSIONENV_TEST=1 runs only these.

namespace TensionEnvLab
{
constexpr double rate = 48000.0;

inline TensionAdsr make (const TensionAdsr::Parameters& p)
{
    TensionAdsr env;
    env.setSampleRate (rate);
    env.setParameters (p);
    return env;
}

inline double closedAttack (const TensionAdsr::Parameters& p, int n)
{
    const auto length = std::max (1.0, (double) p.attack * rate);
    return TensionCurve::shape ((double) n / length, TensionCurve::rateFor ((double) p.curve + p.attackCurve));
}
} // namespace TensionEnvLab

void runTensionEnvTests()
{
    using namespace TensionEnvLab;
    std::cout << "--- tension envelope ---" << std::endl;

    // 1. Straight line is exact, curved stages follow the closed form.
    const float curves[] { 0.0f, 0.35f, -0.35f, 1.0f, -1.0f };
    for (const auto curve : curves)
    {
        TensionAdsr::Parameters p;
        p.attack = 0.05f; p.decay = 0.2f; p.sustain = 0.4f; p.release = 0.3f; p.curve = curve;
        p.attackCurve = 0.2f; p.releaseCurve = -0.25f; p.hold = 0.01f; p.delay = 0.005f;
        auto env = make (p);
        env.noteOn();

        const auto delaySamples = (int) std::ceil (p.delay * rate);
        auto worst = 0.0;
        auto attackLen = (int) (p.attack * rate);
        for (int i = 0; i < delaySamples + attackLen; ++i)
        {
            const auto value = (double) env.getNextSample();
            const auto n = i - delaySamples + 1; // the first attack sample is position 1
            if (i >= delaySamples)
                worst = std::max (worst, std::abs (value - closedAttack (p, n)));
        }
        check (worst < 2.0e-5, "attack follows the closed form (curve " + juce::String (curve, 2)
                                   + ", worst error " + juce::String (worst, 8) + ")");

        // Whole cycle is bounded, finite and ends idle.
        auto bounded = true;
        for (int i = 0; i < (int) (rate * 0.5); ++i)
        {
            const auto v = env.getNextSample();
            bounded = bounded && std::isfinite (v) && v >= -1.0e-6f && v <= 1.0f + 1.0e-6f;
        }
        env.noteOff();
        for (int i = 0; i < (int) (rate * 0.4); ++i)
        {
            const auto v = env.getNextSample();
            bounded = bounded && std::isfinite (v) && v >= -1.0e-6f && v <= 1.0f + 1.0e-6f;
        }
        check (bounded, "levels stay in 0..1 (curve " + juce::String (curve, 2) + ")");
        check (! env.isActive() && env.getNextSample() == 0.0f, "the envelope ends idle (curve " + juce::String (curve, 2) + ")");
    }

    // 2. A linear stage is the exact straight line of the old envelope.
    {
        TensionAdsr::Parameters p;
        p.attack = 0.01f; p.decay = 0.02f; p.sustain = 0.5f; p.release = 0.03f; p.curve = 0.0f;
        auto env = make (p);
        env.noteOn();
        auto exact = true;
        const auto attackLen = (double) p.attack * rate;
        for (int n = 1; n < (int) attackLen; ++n)
            exact = exact && env.getNextSample() == (float) ((double) n / attackLen);
        check (exact, "a straight attack is exactly position / length");
    }

    // 3. skip() (control rate) lands where per-sample processing does.
    for (const auto curve : curves)
    {
        TensionAdsr::Parameters p;
        p.attack = 0.03f; p.decay = 0.15f; p.sustain = 0.3f; p.release = 0.2f; p.curve = curve;
        auto full = make (p), sparse = make (p);
        full.noteOn(); sparse.noteOn();
        auto worst = 0.0;
        for (int i = 0; i < (int) (rate * 0.5); ++i)
        {
            const auto a = full.getNextSample();
            if (i % 16 == 15)
                worst = std::max (worst, (double) std::abs (a - sparse.getNextSample()));
            else
                sparse.skip();
        }
        // Mid-stage note off after skips: release starts from the right level.
        full.noteOff(); sparse.noteOff();
        for (int i = 0; i < (int) (rate * 0.25); ++i)
        {
            const auto a = full.getNextSample();
            if (i % 16 == 15)
                worst = std::max (worst, (double) std::abs (a - sparse.getNextSample()));
            else
                sparse.skip();
        }
        check (worst < 3.0e-5, "skip() and per-sample agree (curve " + juce::String (curve, 2)
                                   + ", worst " + juce::String (worst, 8) + ")");
    }

    // 4. A hard-bent attack has no jump at its start, and note off / retrigger
    //    in the middle of a stage continue from where the level is.
    {
        TensionAdsr::Parameters p;
        p.attack = 0.05f; p.decay = 0.2f; p.sustain = 0.5f; p.release = 0.2f; p.curve = 1.0f;
        auto env = make (p);
        env.noteOn();
        const auto first = env.getNextSample();
        check (first < 0.02f, "the first sample of a hard-bent attack is small (" + juce::String (first, 4) + ")");

        auto last = first;
        auto biggest = 0.0f;
        for (int i = 0; i < (int) (rate * 0.02); ++i)
        {
            const auto v = env.getNextSample();
            biggest = std::max (biggest, std::abs (v - last));
            last = v;
        }
        check (biggest < 0.01f, "attack steps stay small (" + juce::String (biggest, 5) + ")");

        env.noteOff();
        const auto afterOff = env.getNextSample();
        check (std::abs (afterOff - last) < 0.01f, "note off mid-attack releases from the current level ("
                                                       + juce::String (last, 4) + " -> " + juce::String (afterOff, 4) + ")");
        for (int i = 0; i < 2000; ++i)
            last = env.getNextSample();
        env.retrigger();
        const auto afterRetrigger = env.getNextSample();
        check (std::abs (afterRetrigger - last) < 0.01f, "retrigger continues from the current level ("
                                                             + juce::String (last, 4) + " -> " + juce::String (afterRetrigger, 4) + ")");
    }

    // 5. Changing a stage's time or curve while it runs does not jump or
    //    leave 0..1: the curve is re-found from the position.
    {
        TensionAdsr::Parameters p;
        p.attack = 0.2f; p.decay = 0.2f; p.sustain = 0.5f; p.release = 0.2f; p.curve = -0.6f;
        auto env = make (p);
        env.noteOn();
        auto last = 0.0f, biggest = 0.0f;
        auto finite = true;
        for (int i = 0; i < (int) (rate * 0.15); ++i)
        {
            if (i == 2000) { p.attack = 0.3f; env.setParameters (p); }
            if (i == 4000) { p.curve = 0.4f; env.setParameters (p); }
            const auto v = env.getNextSample();
            finite = finite && std::isfinite (v) && v >= 0.0f && v <= 1.0f;
            if (i > 0 && i != 2000 && i != 4000)
                biggest = std::max (biggest, std::abs (v - last));
            last = v;
        }
        check (finite, "a changed length or curve stays in 0..1");
        check (biggest < 0.01f, "a changed length or curve moves smoothly between the changes (" + juce::String (biggest, 5) + ")");
    }

    // 6. The curve helpers invert each other and match the knob's reach.
    {
        auto worst = 0.0;
        for (const auto k : { -5.0, -2.5, -0.3, 0.3, 1.7, 5.0 })
            for (const auto q : { 0.05, 0.3, 0.5, 0.8, 0.97 })
                worst = std::max (worst, std::abs (TensionCurve::inverse (TensionCurve::shape (q, k), k) - q));
        check (worst < 1.0e-9, "TensionCurve::inverse undoes shape (" + juce::String (worst, 12) + ")");

        auto midWorst = 0.0;
        for (const auto k : { -4.0, -1.0, 0.7, 3.0 })
            midWorst = std::max (midWorst, std::abs (TensionCurve::rateForMidHeight (TensionCurve::shape (0.5, k)) - k));
        check (midWorst < 1.0e-9, "rateForMidHeight finds the rate from the middle dot (" + juce::String (midWorst, 12) + ")");
        check (TensionCurve::shape (0.0, 3.0) == 0.0 && std::abs (TensionCurve::shape (1.0, 3.0) - 1.0) < 1.0e-12
                   && TensionCurve::shape (0.4, 0.0) == 0.4,
               "the curve passes through 0 and 1; zero tension is the line");
    }
}
