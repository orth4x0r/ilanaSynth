#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

// The tension curve of the attack, decay and release (roadmap step 46,
// replacing the old progress^exponent bend). A stage's CURVE (-1..1, as
// the knob and graph show it) becomes a rate k = -CURVE * range, and the
// curve between 0 and 1 is (e^(k q) - 1) / (e^k - 1): the shape of an
// analog envelope's RC charge. k = 0 is the straight line (exactly as
// before). A rising k bends the stage late (a negative CURVE), a falling k
// early. Unlike a power curve its slope is finite at both ends (no jump
// at the start of a hard-bent attack), and it can be followed one
// multiply per sample (see TensionAdsr) instead of a pow() per sample.
// The graph (EnvelopeDisplay) draws with the same functions.
namespace TensionCurve
{
constexpr double range = 5.0;

inline double rateFor (double curve) { return -juce::jlimit (-1.0, 1.0, curve) * range; }

// q in 0..1 along the stage, the stage's rate k.
inline double shape (double q, double k)
{
    return k == 0.0 ? q : std::expm1 (k * q) / std::expm1 (k);
}

// The q that gives this height (0..1) under rate k; the inverse of shape,
// for dragging a curve handle (k = 0: the height itself).
inline double inverse (double height, double k)
{
    return k == 0.0 ? height : std::log1p (height * std::expm1 (k)) / k;
}

// The rate whose curve passes through `height` at the stage's middle
// (clamped to the +-range the knob reaches).
inline double rateForMidHeight (double height)
{
    const auto low = shape (0.5, range), high = shape (0.5, -range);
    height = juce::jlimit (low, high, height);
    return std::abs (height - 0.5) < 1.0e-9 ? 0.0 : 2.0 * std::log (1.0 / height - 1.0);
}
} // namespace TensionCurve

class TensionAdsr
{
public:
    struct Parameters
    {
        float attack = 0.005f;
        float decay = 0.3f;
        float sustain = 0.8f;
        float release = 0.25f;
        float curve = 0.0f;
        // M5 DAHDSR: a wait before the attack and a hold at the peak. Both
        // default to zero, which skips their stages entirely.
        float delay = 0.0f;
        float hold = 0.0f;
        // UI review 6 (V5-18): each curved stage's own bend, added to CURVE
        // (and clamped like it). All zero bends the three alike, as before.
        float attackCurve = 0.0f;
        float decayCurve = 0.0f;
        float releaseCurve = 0.0f;
    };

    void setSampleRate (double newSampleRate) { sampleRate = juce::jmax (1.0, newSampleRate); }

    void setParameters (const Parameters& newParameters)
    {
        params = newParameters;
        params.attack = juce::jmax (0.0005f, params.attack);
        params.decay = juce::jmax (0.0005f, params.decay);
        params.release = juce::jmax (0.0005f, params.release);
        params.sustain = juce::jlimit (0.0f, 1.0f, params.sustain);
        params.curve = juce::jlimit (-1.0f, 1.0f, params.curve);
        params.delay = juce::jmax (0.0f, params.delay);
        params.hold = juce::jmax (0.0f, params.hold);

        const auto rate = [this] (float offset)
        {
            return TensionCurve::rateFor ((double) params.curve + (double) offset);
        };
        const auto newAttack = rate (params.attackCurve);
        const auto newDecay = rate (params.decayCurve);
        const auto newRelease = rate (params.releaseCurve);
        // A stage that changes length or rate while it runs re-finds its
        // running power (the next sample does).
        if (newAttack != attackRate || newDecay != decayRate || newRelease != releaseRate
            || params.attack != lastAttack || params.decay != lastDecay || params.release != lastRelease)
            powerValid = false;
        attackRate = newAttack;
        decayRate = newDecay;
        releaseRate = newRelease;
        lastAttack = params.attack;
        lastDecay = params.decay;
        lastRelease = params.release;
        attackScale = scaleFor (attackRate);
        decayScale = scaleFor (decayRate);
        releaseScale = scaleFor (releaseRate);
    }

    void reset()
    {
        enter (Stage::Idle);
        currentValue = 0.0f;
        stale = false;
    }

    // Starts in the delay stage, which hands over to the attack on the first
    // sample when there is no delay (the voice may set the parameters only
    // after the note starts).
    void noteOn()
    {
        enter (Stage::Delay);
        currentValue = 0.0f;
        attackStart = 0.0f;
        stale = false;
    }

    // Mono retrigger: restart the attack from wherever the envelope is now,
    // so a new note doesn't click by snapping to zero first.
    void retrigger()
    {
        catchUp();
        attackStart = stage == Stage::Idle ? 0.0f : currentValue;
        enter (Stage::Attack);
    }

    void noteOff()
    {
        if (stage == Stage::Idle)
            return;

        catchUp();
        enter (Stage::Release);
        releaseStart = currentValue;
    }

    bool isActive() const { return stage != Stage::Idle; }

    // Where the envelope is, for the graph's playhead: the stage (0 delay,
    // 1 attack, 2 hold, 3 decay, 4 sustain, 5 release) plus the fraction
    // of it done, or -1 while idle. Read only; changes nothing.
    float getDisplayPosition() const
    {
        const auto fraction = [this] (float seconds)
        {
            return (float) juce::jlimit (0.0, 0.999, position / juce::jmax (1.0, (double) seconds * sampleRate));
        };

        switch (stage)
        {
            case Stage::Delay:   return 0.0f + fraction (params.delay);
            case Stage::Attack:  return 1.0f + fraction (params.attack);
            case Stage::Hold:    return 2.0f + fraction (params.hold);
            case Stage::Decay:   return 3.0f + fraction (params.decay);
            case Stage::Sustain: return 4.0f;
            case Stage::Release: return 5.0f + fraction (params.release);
            case Stage::Idle:
            default:             return -1.0f;
        }
    }
    // The stage lengths as the note runs them (delay, attack, hold, decay,
    // release; key scaling and modulation are in the parameters), for the
    // playhead's clock.
    std::array<float, 5> getDisplaySeconds() const
    {
        return { params.delay, params.attack, params.hold, params.decay, params.release };
    }
    // Where the running attack or release began (the level a retrigger or an
    // early key up left it at), 0 in the other stages.
    float getDisplayStageStart() const
    {
        return stage == Stage::Attack ? attackStart : stage == Stage::Release ? releaseStart : 0.0f;
    }
    float getCurrentValue() const { return currentValue; }

    // One sample on, like getNextSample, without working out the curve: for
    // an envelope only read every few samples (the voice's control rate).
    // The next getNextSample computes the value where the envelope is then;
    // a note off or retrigger in between first catches the value up.
    void skip()
    {
        switch (stage)
        {
            case Stage::Idle:
            case Stage::Sustain:
                return;

            case Stage::Delay:
                if (position < (double) params.delay * sampleRate)
                {
                    position += 1.0;
                    return;
                }
                enter (Stage::Attack);
                [[fallthrough]];

            case Stage::Attack:
                position += 1.0;
                if (position >= juce::jmax (1.0, (double) params.attack * sampleRate))
                {
                    currentValue = 1.0f;
                    enter (params.hold > 0.0f ? Stage::Hold : Stage::Decay);
                    stale = false;
                    return;
                }
                break;

            case Stage::Hold:
                position += 1.0;
                currentValue = 1.0f;
                stale = false;
                if (position >= (double) params.hold * sampleRate)
                    enter (Stage::Decay);
                return;

            case Stage::Decay:
                position += 1.0;
                if (position >= juce::jmax (1.0, (double) params.decay * sampleRate))
                {
                    currentValue = params.sustain;
                    enter (Stage::Sustain);
                    stale = false;
                    return;
                }
                break;

            case Stage::Release:
                position += 1.0;
                if (position >= juce::jmax (1.0, (double) params.release * sampleRate))
                {
                    currentValue = 0.0f;
                    enter (Stage::Idle);
                    stale = false;
                    return;
                }
                break;
        }
        stale = true;
        powerValid = false;
    }

    float getNextSample()
    {
        stale = false;
        // Held notes spend most of their life here.
        if (stage == Stage::Sustain)
            return currentValue = params.sustain;
        if (stage == Stage::Idle)
            return currentValue = 0.0f;

        if (stage == Stage::Delay)
        {
            if (position < (double) params.delay * sampleRate)
            {
                position += 1.0;
                currentValue = 0.0f;
                return currentValue;
            }

            enter (Stage::Attack);
        }

        switch (stage)
        {
            case Stage::Hold:
            {
                position += 1.0;
                currentValue = 1.0f;

                if (position >= (double) params.hold * sampleRate)
                    enter (Stage::Decay);

                break;
            }

            case Stage::Attack:
            {
                position += 1.0;
                const auto length = juce::jmax (1.0, (double) params.attack * sampleRate);

                if (position >= length)
                {
                    currentValue = 1.0f;
                    enter (params.hold > 0.0f ? Stage::Hold : Stage::Decay);
                }
                else
                {
                    currentValue = attackStart + (1.0f - attackStart) * (float) levelAt (position / length, length, attackRate, attackScale, true);
                }

                break;
            }

            case Stage::Decay:
            {
                position += 1.0;
                const auto length = juce::jmax (1.0, (double) params.decay * sampleRate);

                if (position >= length)
                {
                    currentValue = params.sustain;
                    enter (Stage::Sustain);
                }
                else
                {
                    currentValue = params.sustain
                                   + (1.0f - params.sustain) * (float) levelAt (1.0 - position / length, length, decayRate, decayScale, false);
                }

                break;
            }

            case Stage::Sustain:
                currentValue = params.sustain;
                break;

            case Stage::Release:
            {
                position += 1.0;
                const auto length = juce::jmax (1.0, (double) params.release * sampleRate);

                if (position >= length)
                {
                    currentValue = 0.0f;
                    enter (Stage::Idle);
                }
                else
                {
                    currentValue = releaseStart * (float) levelAt (1.0 - position / length, length, releaseRate, releaseScale, false);
                }

                break;
            }

            case Stage::Delay:
            case Stage::Idle:
            default:
                currentValue = 0.0f;
                break;
        }

        return currentValue;
    }

private:
    enum class Stage
    {
        Idle,
        Delay,
        Hold,
        Attack,
        Decay,
        Sustain,
        Release
    };

    void enter (Stage next)
    {
        stage = next;
        position = 0.0;
        powerValid = false;
    }

    static double scaleFor (double rate) { return rate == 0.0 ? 0.0 : 1.0 / std::expm1 (rate); }

    // The stage's curve at q (0..1 along its direction: rising for the
    // attack, 1 - progress for decay and release). Straight when the rate is
    // zero; otherwise (e^(k q) - 1) / (e^k - 1), where e^(k q) is carried
    // from sample to sample by one multiply (the stage moves 1 / length in
    // q per sample) and found afresh at the start of a stage, after a skip
    // or a parameter change. The product runs in double over at most a few
    // hundred thousand samples (rounding stays near 1e-11).
    double levelAt (double q, double length, double rate, double scale, bool rising)
    {
        if (rate == 0.0)
            return q;

        if (powerValid)
            power *= rising ? step : 1.0 / step;
        else
        {
            power = std::exp (rate * q);
            step = std::exp (rate / length);
            powerValid = true;
        }

        return (power - 1.0) * scale;
    }

    // The value at the current position after skips (Attack, Decay or
    // Release: the stages skip leaves stale).
    void catchUp()
    {
        if (! stale)
            return;
        stale = false;
        if (stage == Stage::Attack)
        {
            const auto length = juce::jmax (1.0, (double) params.attack * sampleRate);
            currentValue = attackStart + (1.0f - attackStart) * (float) TensionCurve::shape (position / length, attackRate);
        }
        else if (stage == Stage::Decay)
        {
            const auto length = juce::jmax (1.0, (double) params.decay * sampleRate);
            currentValue = params.sustain + (1.0f - params.sustain) * (float) TensionCurve::shape (1.0 - position / length, decayRate);
        }
        else if (stage == Stage::Release)
        {
            const auto length = juce::jmax (1.0, (double) params.release * sampleRate);
            currentValue = releaseStart * (float) TensionCurve::shape (1.0 - position / length, releaseRate);
        }
    }

    bool stale = false;
    bool powerValid = false;
    double power = 1.0, step = 1.0;

    Parameters params;
    Stage stage = Stage::Idle;
    double sampleRate = 44100.0;
    double position = 0.0;
    double attackRate = 0.0, decayRate = 0.0, releaseRate = 0.0;
    double attackScale = 0.0, decayScale = 0.0, releaseScale = 0.0;
    float lastAttack = 0.0f, lastDecay = 0.0f, lastRelease = 0.0f;
    float currentValue = 0.0f;
    float releaseStart = 0.0f;
    float attackStart = 0.0f;
};
