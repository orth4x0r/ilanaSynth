#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <complex>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/airwindows/Categories.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// The small picture on an FX card, one per effect family, drawn from the
// module's parameters (and its live gain reduction where the processor
// measures one): a transfer curve for drive / amp / crush and the Airwindows
// saturators, in-out curves with GR meters for comp / OTT / limiter, echoes
// on a beat grid for the delay, a decay curve for the reverb, and (UI review
// 6) the vowel's formants, the comb's teeth, the chorus's delay sweep, the
// phaser's notches, the vocoder's bands and the Airwindows echoes' and
// spaces' impulse responses. The formulas mirror src/processor/Effects.cpp
// (and JUCE's chorus and phaser); the display never changes sound.
class FxDisplay : public juce::Component,
                  public juce::SettableTooltipClient,
                  private IlanaAnim::FrameTimer
{
public:
    enum class Kind { none, transfer, airwindowsTransfer, dynamics, delay, reverb, vowel, comb, chorus, phaser, vocoder, airwindowsImpulse };

    explicit FxDisplay (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setInterceptsMouseClicks (false, false);
        startPollingHz (30);
    }

    // Which FX type the card holds (types as in the fx_slotN choice).
    void setType (int newType, juce::Colour newColour)
    {
        colour = newColour;

        if (newType == type)
            return;

        type = newType;
        kind = kindFor (type);
        lastSignature = 0;
        allInOneKind();
        repaint();
    }

    static Kind kindFor (int fxType)
    {
        switch (fxType)
        {
            case 1: case 2: case 3: return Kind::transfer;
            case 4: case 20: case 21: return Kind::dynamics;
            case 9: return Kind::delay;
            case 13: return Kind::reverb;
            case 32: case 33: return Kind::airwindowsTransfer; // AW Tape, AW Saturation
            case 30: return Kind::airwindowsTransfer; // the all-in-one module (an echo or space: its impulse, below)
            case 27: return Kind::vowel;
            case 5: return Kind::comb;
            case 7: return Kind::chorus;
            case 6: return Kind::phaser;
            case 31: return Kind::vocoder;
            case 34: case 35: return Kind::airwindowsImpulse; // AW Reverb (spaces), AW Delay (echo)
            default: return Kind::none;
        }
    }

    static bool hasDisplay (int fxType) { return kindFor (fxType) != Kind::none; }

    void paint (juce::Graphics& g) override
    {
        // Offscreen (snapshots, tests) the poll doesn't run: catch up here.
        if (! isShowing())
            refresh (false);

        IlanaTheme::paintWell (g, getLocalBounds().toFloat(), 6.0f);

        switch (kind)
        {
            case Kind::transfer:           paintTransfer (g); break;
            case Kind::airwindowsTransfer: paintAirwindowsTransfer (g); break;
            case Kind::dynamics:           paintDynamics (g); break;
            case Kind::delay:              paintDelay (g); break;
            case Kind::reverb:             paintReverb (g); break;
            case Kind::vowel:              paintVowel (g); break;
            case Kind::comb:               paintComb (g); break;
            case Kind::chorus:             paintChorus (g); break;
            case Kind::phaser:             paintPhaser (g); break;
            case Kind::vocoder:            paintVocoder (g); break;
            case Kind::airwindowsImpulse:  paintImpulse (g); break;
            case Kind::none:               break;
        }
    }

private:
    IlanaSynthAudioProcessor& processorRef;
    int type = -1;
    Kind kind = Kind::none;
    juce::Colour colour { IlanaTheme::accent() };
    juce::uint64 lastSignature = 0;
    std::array<float, 3> meterGr {}; // smoothed gain reduction (dB, <= 0), or OTT band gains
    bool metersMoving = false;

    float param (const char* id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();
        return 0.0f;
    }

    float param (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();
        return 0.0f;
    }

    // ---- Shared drawing ----
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (8.0f, 7.0f).withTrimmedTop (11.0f); }

    void paintCaption (juce::Graphics& g, const juce::String& left, const juce::String& right = {}) const
    {
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        auto line = getLocalBounds().reduced (8, 0).removeFromTop (16).withTrimmedTop (3);
        // The right-hand reading keeps its room; a left one too long for
        // what's left is left out rather than cut short or run into it.
        if (right.isNotEmpty())
        {
            g.drawText (right, line, juce::Justification::centredRight);
            line.removeFromRight (juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), right) + 8);
        }
        g.drawText (IlanaTheme::fittedHint (left, g.getCurrentFont(), (float) line.getWidth()), line, juce::Justification::centredLeft, false);
    }

    void strokeCurve (juce::Graphics& g, const juce::Path& curve, juce::Rectangle<float> plot, float baselineY) const
    {
        auto fill = curve;
        fill.lineTo (curve.getCurrentPosition().x, baselineY);
        fill.lineTo (plot.getX(), baselineY);
        fill.closeSubPath();
        g.setColour (colour.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ---- Transfer curves: output against input, -1..1 both ways ----
    float transferAt (float x) const
    {
        switch (type)
        {
            case 1: // Amp: the shaper before the tone stack, times LEVEL
            {
                auto value = x * param ("fx_amp_drive");
                switch (juce::jlimit (0, 2, (int) param ("fx_amp_mode")))
                {
                    case 1:  value = std::tanh (value * 1.6f); break;
                    case 2:  value = value / (1.0f + std::abs (value) * 0.6f); break;
                    default: value = std::tanh (value + 0.15f) - 0.1489f; break;
                }
                return value * param ("fx_amp_level");
            }
            case 2: // Drive: tanh blended by MIX, then the folder
            {
                auto y = x;
                if (param ("fx_drive_on") > 0.5f)
                    y = x + (std::tanh (x * juce::jlimit (1.0f, 20.0f, param ("fx_drive_amount"))) - x) * param ("fx_drive_mix");
                const auto fold = param ("fx_fold");
                if (fold > 0.001f)
                {
                    auto v = std::fmod (y * (1.0f + fold * 5.0f) + 1.0f, 4.0f);
                    if (v < 0.0f)
                        v += 4.0f;
                    y = (v <= 2.0f ? v - 1.0f : 3.0f - v) / (1.0f + fold * 1.5f);
                }
                return y;
            }
            case 3: // Crush: the bit depth's steps, blended by MIX
            {
                const auto levels = std::pow (2.0f, juce::jlimit (1.0f, 16.0f, param ("fx_crush_bits"))) - 1.0f;
                return x + (std::round (x * levels) / levels - x) * param ("fx_crush_mix");
            }
            default: return x;
        }
    }

    void paintTransferGrid (juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getCentreY()));
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (plot.getCentreX(), plot.getY()));
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        const float dashes[] { 3.0f, 3.0f };
        g.drawDashedLine ({ plot.getBottomLeft(), plot.getTopRight() }, dashes, 2, 1.0f);
    }

    juce::Rectangle<float> squarePlot() const
    {
        auto plot = plotArea();
        const auto side = juce::jmin (plot.getWidth(), plot.getHeight());
        return plot.withSizeKeepingCentre (side, side);
    }

    void paintTransfer (juce::Graphics& g)
    {
        const auto plot = squarePlot();
        paintTransferGrid (g, plot);

        juce::Path curve;
        const auto steps = juce::jmax (32, (int) plot.getWidth() * 2);
        for (int i = 0; i <= steps; ++i)
        {
            const auto x = -1.0f + 2.0f * (float) i / (float) steps;
            const auto y = juce::jlimit (-1.1f, 1.1f, transferAt (x));
            const juce::Point<float> point (plot.getX() + (x + 1.0f) * 0.5f * plot.getWidth(),
                                            plot.getCentreY() - y * 0.5f * plot.getHeight());
            if (i == 0)
                curve.startNewSubPath (point);
            else
                curve.lineTo (point);
        }

        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::String right;
        if (type == 3)
            right = "RATE / " + juce::String (juce::jlimit (1, 64, (int) param ("fx_crush_down")));
        paintCaption (g, type == 1 ? "SHAPER" : "TRANSFER", right);
    }

    // ---- Airwindows saturators: the chosen algorithm run on one sine ----
    std::unique_ptr<airwindows::Algorithm> algorithm;
    int algorithmIndex = -1;
    std::vector<juce::Point<float>> measured;

    // The Airwindows module's parameter prefix and chosen algorithm (its
    // registry index), or -1: a category module, or the all-in-one module.
    int airwindowsSource (juce::String& prefix) const
    {
        if (type == 30)
        {
            prefix = "fx_aw";
            return juce::jlimit (0, airwindows::count() - 1, (int) param ("fx_aw_algo"));
        }
        const auto category = airwindows::categoryForFxType (type);
        if (category < 0)
            return -1;
        const auto& module = airwindows::categoryModules()[(size_t) category];
        prefix = juce::String ("fx_") + module.id;
        const auto choice = juce::jlimit (0, (int) module.algorithms.size() - 1, (int) param (prefix + "_algo"));
        return module.algorithms[(size_t) choice];
    }

    // The all-in-one module draws as the family of its algorithm: an echo
    // or a space as its impulse, anything else as its transfer curve
    // (I7-28: it was the only card without a picture).
    void allInOneKind()
    {
        if (type != 30)
            return;
        juce::String prefix;
        const auto index = airwindowsSource (prefix);
        const juce::String category (index >= 0 ? airwindows::registry()[(size_t) index].category : "");
        kind = category == "Space" || category == "Delay" ? Kind::airwindowsImpulse : Kind::airwindowsTransfer;
    }

    // An impulse's length: a space's tail is longer than an echo's.
    bool isSpace() const
    {
        juce::String prefix;
        const auto index = airwindowsSource (prefix);
        return index >= 0 && juce::String (airwindows::registry()[(size_t) index].category) == "Space";
    }

    void measureAirwindows()
    {
        juce::String prefix;
        const auto index = airwindowsSource (prefix);
        if (index < 0)
            return;

        const auto& info = airwindows::registry()[(size_t) index];

        if (index != algorithmIndex || algorithm == nullptr)
        {
            algorithm = info.create();
            algorithmIndex = index;
        }

        constexpr double rate = 48000.0;
        algorithm->prepare (rate);
        for (int k = 0; k < info.numKnobs; ++k)
            algorithm->setParam (info.knobs[k].parameter, info.knobs[k].toPlugin (param (prefix + "_p" + juce::String (k + 1))));

        // 50 Hz at full scale: 0.2 s to settle, then one cycle measured.
        constexpr int period = 960, total = period * 10;
        std::vector<float> left ((size_t) total), right ((size_t) total);
        for (int i = 0; i < total; ++i)
            left[(size_t) i] = right[(size_t) i] = std::sin (juce::MathConstants<float>::twoPi * (float) i / (float) period);
        const auto input = left;
        algorithm->process (left.data(), right.data(), total);

        const auto mix = juce::jlimit (0.0f, 1.0f, param (prefix + "_mix"));
        measured.clear();
        for (int i = total - period; i < total; i += 4)
        {
            const auto x = input[(size_t) i];
            auto y = left[(size_t) i];
            if (! std::isfinite (y))
                y = 0.0f;
            measured.push_back ({ x, x + (y - x) * mix });
        }
    }

    void paintAirwindowsTransfer (juce::Graphics& g)
    {
        const auto plot = squarePlot();
        paintTransferGrid (g, plot);

        if (! measured.empty())
        {
            juce::Path curve;
            for (size_t i = 0; i < measured.size(); ++i)
            {
                const juce::Point<float> point (plot.getX() + (measured[i].x + 1.0f) * 0.5f * plot.getWidth(),
                                                plot.getCentreY() - juce::jlimit (-1.1f, 1.1f, measured[i].y) * 0.5f * plot.getHeight());
                if (i == 0)
                    curve.startNewSubPath (point);
                else
                    curve.lineTo (point);
            }
            g.setColour (colour);
            g.strokePath (curve, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        paintCaption (g, "TRANSFER");
    }

    // ---- Dynamics: static in/out curve (dB), live gain reduction ----
    static constexpr float floorDb = -60.0f, topDb = 6.0f;

    float dynamicsOutDb (float inDb) const
    {
        const auto x = juce::Decibels::decibelsToGain (inDb);

        switch (type)
        {
            case 4:
            {
                const auto threshold = juce::Decibels::decibelsToGain (param ("fx_comp_threshold"));
                const auto ratio = juce::jmax (1.0f, param ("fx_comp_ratio"));
                const auto gain = x > threshold ? std::pow (x / threshold, 1.0f / ratio - 1.0f) : 1.0f;
                const auto wet = x * gain * juce::Decibels::decibelsToGain (param ("fx_comp_makeup"));
                return juce::Decibels::gainToDecibels (x + (wet - x) * param ("fx_comp_mix"), -100.0f);
            }
            case 20:
            {
                const auto gain = juce::jlimit (0.25f, 4.0f, std::pow (x + 0.001f, -0.6f * juce::jlimit (0.0f, 1.0f, param ("fx_ott_amount"))));
                return juce::Decibels::gainToDecibels (x + (x * gain - x) * param ("fx_ott_mix"), -100.0f);
            }
            case 21:
                return juce::jmin (inDb, juce::jlimit (-24.0f, 0.0f, param ("fx_limit_ceiling")));
            default:
                return inDb;
        }
    }

    void paintDynamics (juce::Graphics& g)
    {
        auto area = plotArea();
        const auto meters = area.removeFromRight (type == 20 ? 34.0f : 14.0f);
        area.removeFromRight (6.0f);
        const auto plot = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight() * 1.5f), area.getHeight());

        const auto toX = [plot] (float db) { return plot.getX() + (db - floorDb) / (topDb - floorDb) * plot.getWidth(); };
        const auto toY = [plot] (float db) { return plot.getBottom() - (juce::jlimit (floorDb, topDb, db) - floorDb) / (topDb - floorDb) * plot.getHeight(); };

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto db : { -48.0f, -36.0f, -24.0f, -12.0f, 0.0f })
        {
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), toY (db)));
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (toX (db), plot.getY()));
        }
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        const float dashes[] { 3.0f, 3.0f };
        g.drawDashedLine ({ toX (floorDb), toY (floorDb), toX (topDb), toY (topDb) }, dashes, 2, 1.0f);

        juce::Path curve;
        for (int i = 0; i <= 66; ++i)
        {
            const auto inDb = floorDb + (topDb - floorDb) * (float) i / 66.0f;
            const juce::Point<float> point (toX (inDb), toY (dynamicsOutDb (inDb)));
            if (i == 0)
                curve.startNewSubPath (point);
            else
                curve.lineTo (point);
        }
        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // The threshold or ceiling, as a quiet marker.
        if (type == 4 || type == 21)
        {
            const auto db = type == 4 ? param ("fx_comp_threshold") : param ("fx_limit_ceiling");
            g.setColour (colour.withAlpha (0.35f));
            if (type == 4)
                g.drawDashedLine ({ toX (db), plot.getY(), toX (db), plot.getBottom() }, dashes, 2, 1.0f);
            else
                g.drawDashedLine ({ plot.getX(), toY (db), plot.getRight(), toY (db) }, dashes, 2, 1.0f);
        }

        if (type == 20)
            paintOttMeters (g, meters);
        else
            paintGrMeter (g, meters, meterGr[0]);

        paintCaption (g, "IN / OUT", type == 20 ? "BANDS" : "GR " + juce::String (meterGr[0], 1) + " dB");
    }

    // One downward bar from the top: 0 to -24 dB of gain reduction.
    void paintGrMeter (juce::Graphics& g, juce::Rectangle<float> bar, float grDb) const
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (bar, 2.0f);
        const auto amount = juce::jlimit (0.0f, 1.0f, -grDb / 24.0f);
        if (amount > 0.002f)
        {
            g.setColour (colour.withAlpha (0.85f));
            g.fillRoundedRectangle (bar.withHeight (bar.getHeight() * amount), 2.0f);
        }
    }

    // LOW / MID / HIGH: each band's gain, up (boost) or down (cut) from the
    // centre line, -12..+12 dB.
    void paintOttMeters (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const auto width = area.getWidth() / 3.0f;
        for (int band = 0; band < 3; ++band)
        {
            const auto bar = area.withX (area.getX() + width * (float) band).withWidth (width).reduced (1.5f, 0.0f);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRoundedRectangle (bar, 2.0f);
            const auto db = juce::jlimit (-12.0f, 12.0f, meterGr[(size_t) band]);
            const auto height = std::abs (db) / 12.0f * bar.getHeight() * 0.5f;
            g.setColour (colour.withAlpha (0.85f));
            if (db > 0.0f)
                g.fillRect (bar.withY (bar.getCentreY() - height).withHeight (height));
            else
                g.fillRect (bar.withY (bar.getCentreY()).withHeight (height));
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRect (bar.withY (bar.getCentreY()).withHeight (1.0f));
        }
    }

    void updateMeters (bool smooth)
    {
        std::array<float, 3> target {};
        const auto live = processorRef.getOutputPeak() > 1.0e-5f;

        if (type == 4)
            target[0] = juce::Decibels::gainToDecibels (processorRef.getCompGainReduction(), -60.0f);
        else if (type == 21)
            target[0] = juce::Decibels::gainToDecibels (processorRef.getLimiterGainReduction(), -60.0f);
        else if (type == 20)
            for (int band = 0; band < 3; ++band)
                target[(size_t) band] = juce::Decibels::gainToDecibels (processorRef.getOttBandGain (band), -60.0f);

        if (! live)
            target = {};

        metersMoving = false;
        for (size_t i = 0; i < 3; ++i)
        {
            const auto next = smooth ? meterGr[i] + (target[i] - meterGr[i]) * 0.35f : target[i];
            metersMoving = metersMoving || std::abs (next - meterGr[i]) > 0.05f;
            meterGr[i] = std::abs (next) < 0.05f && std::abs (target[i]) < 0.05f ? 0.0f : next;
        }
    }

    // ---- Delay: echoes on a beat grid ----
    struct Echo
    {
        float beats, level;
        int row; // 0 left, 1 right, 2 both
        bool tap;
    };

    std::vector<Echo> delayEchoes (float& beatsPerEcho, juce::String& caption) const
    {
        const auto bpm = juce::jmax (20.0, processorRef.getCurrentBpm());
        const auto msToBeats = [bpm] (float ms) { return (float) (ms * 0.001 * bpm / 60.0); };
        static const double divisionBeats[] { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.75, 0.375 };
        const auto synced = param ("fx_delay_sync") > 0.5f;
        auto left = msToBeats (param ("fx_delay_time"));
        auto right = msToBeats (param ("fx_delay_time_r"));

        if (synced)
        {
            const auto division = juce::jlimit (0, 10, (int) param ("fx_delay_div"));
            left = right = (float) divisionBeats[division];
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter ("fx_delay_div")))
                caption = choice->getCurrentChoiceName();
        }
        else
            caption = juce::String (juce::roundToInt (param ("fx_delay_time"))) + " ms";

        beatsPerEcho = left;
        const auto feedback = juce::jlimit (0.0f, 0.95f, param ("fx_delay_feedback"));
        const auto pingPong = param ("fx_delay_pingpong") > 0.5f;
        const auto split = ! pingPong && std::abs (left - right) > 1.0e-4f;
        std::vector<Echo> echoes;

        for (int k = 1; k <= 24; ++k)
        {
            const auto level = std::pow (feedback, (float) (k - 1));
            if (level < 0.02f)
                break;
            if (pingPong)
                echoes.push_back ({ left * (float) k, level, (k % 2 == 1) ? 0 : 1, false });
            else if (split)
            {
                echoes.push_back ({ left * (float) k, level, 0, false });
                echoes.push_back ({ right * (float) k, level, 1, false });
            }
            else
                echoes.push_back ({ left * (float) k, level, 2, false });
        }

        // The extra taps (src/processor/Effects.cpp's patterns), off the left time.
        if (param ("fx_taps_on") > 0.5f)
        {
            static constexpr float tapTimes[6][4] { { 0.25f, 0.5f, 0, 0 }, { 0.25f, 0.375f, 0.75f, 0 }, { 0.375f, 0.75f, 0, 0 },
                                                    { 1.0f, 2.0f, 3.0f, 0 }, { 0.125f, 0.25f, 0.375f, 0.5f }, { 1.5f, 1.0f, 0.5f, 0 } };
            static constexpr float tapGains[6][4] { { 0.7f, 0.5f, 0, 0 }, { 0.6f, 0.5f, 0.4f, 0 }, { 0.6f, 0.45f, 0, 0 },
                                                    { 0.5f, 0.35f, 0.25f, 0 }, { 0.6f, 0.55f, 0.5f, 0.45f }, { 0.4f, 0.5f, 0.6f, 0 } };
            const auto pattern = juce::jlimit (0, 6, (int) param ("fx_taps_pattern"));
            const auto mix = param ("fx_taps_mix");

            if (pattern == 6)
            {
                for (int step = 0; step < 16; ++step)
                    if (const auto gain = param ("fx_taps_step" + juce::String (step + 1)); gain > 0.001f)
                        echoes.push_back ({ left * (float) (step + 1) * 0.25f, gain * mix, 2, true });
            }
            else
                for (int tap = 0; tap < 4; ++tap)
                    if (tapTimes[pattern][tap] > 0.0f)
                        echoes.push_back ({ left * tapTimes[pattern][tap], tapGains[pattern][tap] * mix, 2, true });
        }

        return echoes;
    }

    void paintDelay (juce::Graphics& g)
    {
        float beatsPerEcho = 1.0f;
        juce::String caption;
        const auto echoes = delayEchoes (beatsPerEcho, caption);
        const auto plot = plotArea();

        // One bar of 4/4, or two when the echoes are long.
        auto lastBeat = 0.0f;
        for (const auto& echo : echoes)
            if (echo.level > 0.08f)
                lastBeat = juce::jmax (lastBeat, echo.beats);
        const auto span = lastBeat > 4.0f || beatsPerEcho > 1.5f ? 8.0f : 4.0f;
        const auto toX = [plot, span] (float beats) { return plot.getX() + beats / span * plot.getWidth(); };

        for (int sixteenth = 0; sixteenth <= (int) span * 4; ++sixteenth)
        {
            const auto onBeat = sixteenth % 4 == 0;
            g.setColour (juce::Colours::white.withAlpha (sixteenth % 16 == 0 ? 0.16f : (onBeat ? 0.09f : 0.035f)));
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (toX ((float) sixteenth * 0.25f), plot.getY()));
        }

        // The dry hit at 0, then the echoes; L above the centre line, R below.
        const auto mid = plot.getCentreY();
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), mid));
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRect (juce::Rectangle<float> (2.0f, plot.getHeight() * 0.9f).withCentre ({ toX (0.0f) + 1.0f, mid }));

        const auto half = plot.getHeight() * 0.5f - 1.0f;
        for (const auto& echo : echoes)
        {
            if (echo.beats > span)
                continue;
            const auto x = toX (echo.beats);
            const auto h = juce::jmax (2.0f, half * echo.level);
            const auto c = echo.tap ? juce::Colours::white.withAlpha (0.55f) : colour.withAlpha (0.35f + 0.6f * echo.level);
            g.setColour (c);
            const auto w = echo.tap ? 1.5f : 3.0f;
            if (echo.row == 0 || echo.row == 2)
                g.fillRoundedRectangle (juce::Rectangle<float> (w, h).withPosition (x - w * 0.5f, mid - h), 1.0f);
            if (echo.row == 1 || echo.row == 2)
                g.fillRoundedRectangle (juce::Rectangle<float> (w, h).withPosition (x - w * 0.5f, mid + 1.0f), 1.0f);
        }

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.drawText ("L", plot.withTrimmedLeft (4.0f).withHeight (12.0f), juce::Justification::centredLeft);
        g.drawText ("R", plot.withTrimmedLeft (4.0f).withTop (plot.getBottom() - 12.0f), juce::Justification::centredLeft);
        paintCaption (g, "ECHOES  " + caption, juce::String ((int) span / 4) + (span > 4.0f ? " BARS" : " BAR"));
    }

    // ---- Reverb: the tail's decay over time, lows and highs ----
    void paintReverb (juce::Graphics& g)
    {
        const auto reverbType = juce::jlimit (0, 6, (int) param ("fx_reverb_type"));
        const auto plot = plotArea();

        if (reverbType == 6)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText ("Impulse response: the loaded file's own decay", plot, juce::Justification::centred);
            paintCaption (g, "DECAY");
            return;
        }

        // juce::Reverb (Freeverb): comb feedback from the room size, a
        // one-pole damping filter in each comb's loop (~31 ms on average).
        auto size = param ("fx_reverb_size");
        auto damping = param ("fx_reverb_damping");
        switch (reverbType)
        {
            case 1: size = size * 0.5f + 0.5f; damping *= 0.8f; break;
            case 2: damping *= 0.35f; break;
            case 4: damping *= 0.25f; break;
            case 5: size = size * 0.4f + 0.6f; break;
            default: break;
        }
        const auto feedback = juce::jlimit (0.0f, 0.999f, size * 0.28f + 0.7f);
        const auto d1 = damping * 0.4f;
        const auto highFeedback = feedback * (1.0f - d1) / (1.0f + d1);
        constexpr auto loopSeconds = 0.0312f;
        const auto rt60 = [] (float gain) { return 60.0f / juce::jmax (0.01f, -20.0f * std::log10 (gain)) * loopSeconds; };
        const auto lowTime = rt60 (feedback);
        const auto highTime = rt60 (highFeedback);
        const auto gated = reverbType == 5;
        const auto shownTime = gated ? 0.6f : juce::jmax (0.5f, lowTime * 1.1f);

        const auto toX = [plot, shownTime] (float seconds) { return plot.getX() + seconds / shownTime * plot.getWidth(); };
        const auto toY = [plot] (float db) { return plot.getY() + juce::jlimit (0.0f, 1.0f, -db / 60.0f) * plot.getHeight(); };

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto db : { -20.0f, -40.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), toY (db)));
        const auto tick = shownTime > 4.0f ? 1.0f : (shownTime > 1.5f ? 0.5f : 0.1f);
        for (auto t = tick; t < shownTime; t += tick)
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (toX (t), plot.getY()));

        const auto curveFor = [&] (float decaySeconds)
        {
            juce::Path curve;
            for (int i = 0; i <= 80; ++i)
            {
                const auto t = shownTime * (float) i / 80.0f;
                auto db = -60.0f * t / decaySeconds;
                if (gated) // the gate closes ~60 ms after the input stops
                    db += juce::Decibels::gainToDecibels (std::exp (-t / 0.06f), -100.0f);
                const juce::Point<float> point (toX (t), toY (db));
                if (i == 0)
                    curve.startNewSubPath (point);
                else
                    curve.lineTo (point);
            }
            return curve;
        };

        g.setColour (colour.withAlpha (0.4f));
        g.strokePath (curveFor (highTime), juce::PathStrokeType (1.0f));
        strokeCurve (g, curveFor (lowTime), plot, plot.getBottom());

        // A legend in the top right, off the curves (UI review 6, V6-25).
        {
            auto legend = juce::Rectangle<float> (plot.getRight() - 64.0f, plot.getY() + 2.0f, 62.0f, 22.0f);
            g.setColour (IlanaTheme::Ui::well.withAlpha (0.85f));
            g.fillRoundedRectangle (legend, 3.0f);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            for (const auto& [highs, text] : { std::pair<bool, const char*> { false, "LOWS" }, { true, "HIGHS" } })
            {
                auto line = legend.removeFromTop (11.0f);
                const auto sample = line.removeFromLeft (16.0f).reduced (3.0f, 0.0f);
                g.setColour (highs ? colour.withAlpha (0.4f) : colour);
                g.fillRect (sample.withSizeKeepingCentre (sample.getWidth(), highs ? 1.0f : 1.6f));
                g.setColour (IlanaTheme::Ui::text3);
                g.drawText (text, line, juce::Justification::centredLeft);
            }
        }
        const auto seconds = gated ? juce::String ("gated") : "RT60 " + juce::String (lowTime, lowTime < 10.0f ? 1 : 0) + " s";
        paintCaption (g, "DECAY", seconds);
    }

    // ---- Shared: a response over frequency, in dB ----
    static float logX (juce::Rectangle<float> plot, double frequency, double low, double high)
    {
        return plot.getX() + (float) (std::log (frequency / low) / std::log (high / low)) * plot.getWidth();
    }

    void paintFrequencyGrid (juce::Graphics& g, juce::Rectangle<float> plot, double low, double high) const
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto frequency : { 100.0, 1000.0, 10000.0 })
            if (frequency > low && frequency < high)
                g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (logX (plot, frequency, low, high), plot.getY()));
    }

    // The magnitude of `response` from low to high Hz, dB in [floor, top].
    template <typename Response>
    juce::Path responsePath (juce::Rectangle<float> plot, double low, double high, float floor, float top, Response response) const
    {
        juce::Path path;
        const auto steps = juce::jmax (32, (int) plot.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const auto frequency = low * std::pow (high / low, (double) i / (double) steps);
            const auto db = juce::jlimit (floor, top, (float) (20.0 * std::log10 (juce::jmax (1.0e-6, std::abs (response (frequency))))));
            const juce::Point<float> point (plot.getX() + plot.getWidth() * (float) i / (float) steps,
                                            plot.getBottom() - (db - floor) / (top - floor) * plot.getHeight());
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        return path;
    }

    // ---- Vowel: the three formant band-passes (src/processor/Effects.cpp) ----
    void paintVowel (juce::Graphics& g)
    {
        static const float formant1[5] { 800.0f, 400.0f, 350.0f, 450.0f, 325.0f };
        static const float formant2[5] { 1150.0f, 1600.0f, 1700.0f, 800.0f, 700.0f };
        const auto morph = juce::jlimit (0.0f, 1.0f, param ("fx_vowel_morph"));
        const auto mix = param ("fx_vowel_mix");
        const auto position = morph * 4.0f;
        const auto index = juce::jlimit (0, 3, (int) position);
        const auto frac = position - (float) index;
        const auto f1 = (double) (formant1[index] + (formant1[index + 1] - formant1[index]) * frac);
        const auto f2 = (double) (formant2[index] + (formant2[index + 1] - formant2[index]) * frac);
        const auto f3 = f2 * 2.4;
        const auto bandPass = [] (double frequency, double centre, double resonance)
        {
            const std::complex<double> s (0.0, frequency / centre);
            const auto k = 2.0 - 2.0 * resonance;
            return s / (s * s + k * s + 1.0);
        };

        const auto plot = plotArea();
        constexpr double low = 100.0, high = 8000.0;
        paintFrequencyGrid (g, plot, low, high);
        const auto curve = responsePath (plot, low, high, -24.0f, 12.0f, [&] (double f)
        {
            const auto wet = bandPass (f, f1, 0.82) + 0.7 * bandPass (f, f2, 0.82) + 0.35 * bandPass (f, f3, 0.8);
            return 1.0 + (wet - 1.0) * (double) mix;
        });
        strokeCurve (g, curve, plot, plot.getBottom());

        // The formants' places, named.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        for (const auto& [frequency, name] : { std::pair<double, const char*> { f1, "F1" }, { f2, "F2" }, { f3, "F3" } })
        {
            const auto x = logX (plot, frequency, low, high);
            g.setColour (colour.withAlpha (0.3f));
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (x, plot.getY()));
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText (name, juce::Rectangle<float> (x + 2.0f, plot.getBottom() - 11.0f, 20.0f, 10.0f), juce::Justification::centredLeft);
        }

        static const char* const vowels[] { "A", "E", "I", "O", "U" };
        const auto nearest = juce::jlimit (0, 4, juce::roundToInt (position));
        paintCaption (g, "FORMANTS", juce::String ("VOWEL  ") + vowels[nearest]);
    }

    // ---- Comb: x + MIX * the fed-back delay (src/processor/Effects.cpp) ----
    // On a linear axis (the first eight teeth), where they sit evenly.
    void paintComb (juce::Graphics& g)
    {
        const auto frequency = (double) juce::jlimit (20.0f, 2000.0f, param ("fx_comb_freq"));
        const auto feedback = (double) juce::jlimit (0.0f, 0.97f, param ("fx_comb_feedback"));
        const auto mix = (double) param ("fx_comb_mix");
        const auto plot = plotArea();
        constexpr int teeth = 8;
        constexpr float floor = -24.0f, top = 18.0f;
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int tooth = 1; tooth < teeth; ++tooth)
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (plot.getX() + plot.getWidth() * (float) tooth / (float) teeth, plot.getY()));

        juce::Path curve;
        const auto steps = juce::jmax (64, (int) plot.getWidth() * 4);
        for (int i = 0; i <= steps; ++i)
        {
            const auto f = frequency * teeth * (double) i / (double) steps;
            const auto delayed = std::polar (1.0, -juce::MathConstants<double>::twoPi * f / frequency);
            const auto response = 1.0 + mix * delayed / (1.0 - feedback * delayed);
            const auto db = juce::jlimit (floor, top, (float) (20.0 * std::log10 (juce::jmax (1.0e-6, std::abs (response)))));
            const juce::Point<float> point (plot.getX() + plot.getWidth() * (float) i / (float) steps,
                                            plot.getBottom() - (db - floor) / (top - floor) * plot.getHeight());
            if (i == 0)
                curve.startNewSubPath (point);
            else
                curve.lineTo (point);
        }
        strokeCurve (g, curve, plot, plot.getBottom());
        paintCaption (g, "TEETH", "every " + juce::String (juce::roundToInt (frequency)) + " Hz");
    }

    // ---- Chorus: JUCE's delay sweep, 7 ms +- 10 ms x DEPTH at RATE ----
    void paintChorus (juce::Graphics& g)
    {
        const auto rate = juce::jmax (0.01f, param ("fx_chorus_rate"));
        const auto depth = juce::jlimit (0.0f, 1.0f, param ("fx_chorus_depth"));
        const auto plot = plotArea();
        // Two cycles (at least half a second), delay 0..20 ms up the side.
        const auto seconds = juce::jmax (0.5f, 2.0f / rate);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto ms : { 5.0f, 10.0f, 15.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getBottom() - ms / 20.0f * plot.getHeight()));

        juce::Path sweep;
        const auto steps = juce::jmax (32, (int) plot.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const auto t = seconds * (float) i / (float) steps;
            const auto ms = juce::jmax (1.0f, 7.0f + 10.0f * depth * std::sin (juce::MathConstants<float>::twoPi * rate * t));
            const juce::Point<float> point (plot.getX() + plot.getWidth() * (float) i / (float) steps,
                                            plot.getBottom() - juce::jlimit (0.0f, 20.0f, ms) / 20.0f * plot.getHeight());
            if (i == 0)
                sweep.startNewSubPath (point);
            else
                sweep.lineTo (point);
        }
        g.setColour (colour);
        g.strokePath (sweep, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        paintCaption (g, "DELAY SWEEP", "7 ms " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb1 ")) + juce::String (10.0f * depth, 1) + " ms");
    }

    // ---- Phaser: JUCE's six all-passes, at the sweep's ends and middle ----
    void paintPhaser (juce::Graphics& g)
    {
        const auto depth = juce::jlimit (0.0f, 1.0f, param ("fx_phaser_depth"));
        const auto feedback = (double) param ("fx_phaser_feedback");
        const auto mix = (double) param ("fx_phaser_mix");
        const auto plot = plotArea();
        constexpr double low = 20.0, high = 20000.0;
        const auto centre = std::log10 (800.0 / 20.0) / 3.0;
        const auto cutoffAt = [&] (double lfo) { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, centre + (double) depth * 0.5 * lfo)); };
        const auto responseAt = [&] (double cutoff)
        {
            return [cutoff, feedback, mix] (double f)
            {
                const std::complex<double> s (0.0, f / cutoff);
                const auto allPass = std::pow ((1.0 - s) / (1.0 + s), 6.0);
                return (1.0 - mix) + mix * allPass / (1.0 + feedback * allPass);
            };
        };

        paintFrequencyGrid (g, plot, low, high);
        // The band the notches sweep through, then its ends faint.
        const auto from = logX (plot, cutoffAt (-1.0), low, high), to = logX (plot, cutoffAt (1.0), low, high);
        g.setColour (colour.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (from, plot.getY(), juce::jmax (1.0f, to - from), plot.getHeight()));
        g.setColour (colour.withAlpha (0.3f));
        for (const auto lfo : { -1.0, 1.0 })
            g.strokePath (responsePath (plot, low, high, -30.0f, 12.0f, responseAt (cutoffAt (lfo))), juce::PathStrokeType (1.0f));
        g.setColour (colour);
        g.strokePath (responsePath (plot, low, high, -30.0f, 12.0f, responseAt (cutoffAt (0.0))),
                      juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        paintCaption (g, "NOTCHES", juce::String (param ("fx_phaser_rate"), 2) + " Hz");
    }

    // ---- Vocoder: each band's level now (the modulator's envelope) ----
    std::array<float, 24> bandLevels {};

    void paintVocoder (juce::Graphics& g)
    {
        const auto count = juce::jlimit (4, 24, (int) param ("fx_voc_bands"));
        const auto plot = plotArea();
        const auto width = plot.getWidth() / (float) count;
        auto peak = 1.0e-4f;
        for (int b = 0; b < count; ++b)
            peak = juce::jmax (peak, bandLevels[(size_t) b]);

        for (int b = 0; b < count; ++b)
        {
            const auto bar = juce::Rectangle<float> (plot.getX() + width * (float) b, plot.getY(), width, plot.getHeight()).reduced (1.0f, 0.0f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (bar, 1.5f);
            const auto level = juce::jlimit (0.0f, 1.0f, bandLevels[(size_t) b] / juce::jmax (0.05f, peak));
            if (level > 0.01f)
            {
                g.setColour (colour.withAlpha (0.4f + 0.55f * level));
                g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * level), 1.5f);
            }
        }

        // The modulator as the MODULATOR menu names it, or the formant shift
        // when there is one (both don't fit beside the band count).
        static const char* const sources[] { "INPUT, ELSE TALK", "INPUT", "TALK" };
        const auto formant = param ("fx_voc_formant");
        paintCaption (g, juce::String (count) + " BANDS",
                      std::abs (formant) < 0.05f ? juce::String (sources[juce::jlimit (0, 2, (int) param ("fx_voc_source"))])
                                                 : "FORMANT " + juce::String (formant > 0.0f ? "+" : "") + juce::String (formant, 1) + " st");
    }

    // ---- Airwindows echoes and spaces: the algorithm's own impulse response ----
    std::vector<float> impulseEnvelope;
    float impulseSeconds = 2.0f;

    void measureImpulse()
    {
        impulseEnvelope.clear();
        juce::String prefix;
        const auto index = airwindowsSource (prefix);
        if (index < 0)
            return;

        const auto& info = airwindows::registry()[(size_t) index];
        auto run = info.create();
        if (run == nullptr)
            return;

        constexpr double rate = 44100.0;
        impulseSeconds = isSpace() ? 4.0f : 2.5f;
        run->prepare (rate);
        for (int k = 0; k < info.numKnobs; ++k)
            run->setParam (info.knobs[k].parameter, info.knobs[k].toPlugin (param (prefix + "_p" + juce::String (k + 1))));

        const auto total = (int) (rate * impulseSeconds);
        std::vector<float> left ((size_t) total, 0.0f), right ((size_t) total, 0.0f);
        left[0] = right[0] = 0.8f;
        run->process (left.data(), right.data(), total);

        constexpr int columns = 220;
        impulseEnvelope.assign (columns, 0.0f);
        for (int i = 0; i < total; ++i)
        {
            auto& cell = impulseEnvelope[(size_t) juce::jmin (columns - 1, i * columns / total)];
            const auto v = std::abs (std::isfinite (left[(size_t) i]) ? left[(size_t) i] : 0.0f)
                           + std::abs (std::isfinite (right[(size_t) i]) ? right[(size_t) i] : 0.0f);
            cell = juce::jmax (cell, v * 0.5f);
        }
    }

    void paintImpulse (juce::Graphics& g)
    {
        const auto plot = plotArea();
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (auto t = 0.5f; t < impulseSeconds; t += 0.5f)
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (plot.getX() + t / impulseSeconds * plot.getWidth(), plot.getY()));

        auto peak = 1.0e-6f;
        for (const auto v : impulseEnvelope)
            peak = juce::jmax (peak, v);

        if (! impulseEnvelope.empty() && peak > 1.0e-5f)
        {
            // Levels in dB over 48 dB below the loudest moment, as bars.
            const auto width = plot.getWidth() / (float) impulseEnvelope.size();
            g.setColour (colour);
            for (size_t i = 0; i < impulseEnvelope.size(); ++i)
            {
                const auto db = juce::Decibels::gainToDecibels (impulseEnvelope[i] / peak, -60.0f);
                const auto height = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f) * plot.getHeight();
                if (height > 0.5f)
                    g.fillRect (juce::Rectangle<float> (plot.getX() + width * (float) i, plot.getBottom() - height, juce::jmax (1.0f, width - 0.5f), height));
            }
        }
        else
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.drawText ("no tail at these settings", plot, juce::Justification::centred);
        }

        paintCaption (g, isSpace() ? "IMPULSE" : "ECHOES", juce::String (impulseSeconds, 1) + " s");
    }

    // ---- Polling ----
    juce::uint64 signature() const
    {
        juce::uint64 hash = (juce::uint64) type * 1000003u;
        const auto mixIn = [&hash] (float value)
        {
            juce::uint32 bits;
            std::memcpy (&bits, &value, sizeof (bits));
            hash = (hash ^ bits) * 1099511628211ull;
        };

        static const char* const transferIds[] { "fx_amp_drive", "fx_amp_mode", "fx_amp_level", "fx_drive_on", "fx_drive_amount",
                                                 "fx_drive_mix", "fx_fold", "fx_crush_bits", "fx_crush_mix", "fx_crush_down" };
        static const char* const dynamicsIds[] { "fx_comp_threshold", "fx_comp_ratio", "fx_comp_makeup", "fx_comp_mix",
                                                 "fx_ott_amount", "fx_ott_mix", "fx_limit_ceiling" };
        static const char* const delayIds[] { "fx_delay_time", "fx_delay_time_r", "fx_delay_sync", "fx_delay_div", "fx_delay_feedback",
                                              "fx_delay_pingpong", "fx_taps_on", "fx_taps_pattern", "fx_taps_mix" };
        static const char* const reverbIds[] { "fx_reverb_type", "fx_reverb_size", "fx_reverb_damping" };
        static const char* const vowelIds[] { "fx_vowel_morph", "fx_vowel_mix" };
        static const char* const combIds[] { "fx_comb_freq", "fx_comb_feedback", "fx_comb_mix" };
        static const char* const chorusIds[] { "fx_chorus_rate", "fx_chorus_depth" };
        static const char* const phaserIds[] { "fx_phaser_rate", "fx_phaser_depth", "fx_phaser_feedback", "fx_phaser_mix" };
        static const char* const vocoderIds[] { "fx_voc_bands", "fx_voc_source", "fx_voc_formant" };

        switch (kind)
        {
            case Kind::transfer: for (auto* id : transferIds) mixIn (param (id)); break;
            case Kind::dynamics: for (auto* id : dynamicsIds) mixIn (param (id)); break;
            case Kind::delay:
                for (auto* id : delayIds) mixIn (param (id));
                for (int step = 1; step <= 16; ++step) mixIn (param ("fx_taps_step" + juce::String (step)));
                mixIn ((float) processorRef.getCurrentBpm());
                break;
            case Kind::reverb: for (auto* id : reverbIds) mixIn (param (id)); break;
            case Kind::vowel: for (auto* id : vowelIds) mixIn (param (id)); break;
            case Kind::comb: for (auto* id : combIds) mixIn (param (id)); break;
            case Kind::chorus: for (auto* id : chorusIds) mixIn (param (id)); break;
            case Kind::phaser: for (auto* id : phaserIds) mixIn (param (id)); break;
            case Kind::vocoder: for (auto* id : vocoderIds) mixIn (param (id)); break;
            case Kind::airwindowsImpulse:
            case Kind::airwindowsTransfer:
                if (juce::String prefix; airwindowsSource (prefix) >= 0)
                {
                    mixIn (param (prefix + "_algo"));
                    mixIn (param (prefix + "_mix"));
                    for (int k = 1; k <= airwindows::Module::numKnobs; ++k)
                        mixIn (param (prefix + "_p" + juce::String (k)));
                }
                break;
            case Kind::none: break;
        }
        return hash;
    }

    // Re-reads the parameters (and meters); true when the picture changed.
    bool refresh (bool smooth)
    {
        if (kind == Kind::none)
            return false;

        auto dirty = false;
        if (const auto now = signature(); now != lastSignature)
        {
            lastSignature = now;
            allInOneKind();
            if (kind == Kind::airwindowsTransfer)
                measureAirwindows();
            if (kind == Kind::airwindowsImpulse)
                measureImpulse();
            dirty = true;
        }

        // The vocoder's bands follow the modulator (smoothed, falling back
        // to rest once nothing plays).
        if (kind == Kind::vocoder)
        {
            const auto live = processorRef.getOutputPeak() > 1.0e-5f;
            auto moving = false;
            for (size_t b = 0; b < bandLevels.size(); ++b)
            {
                const auto target = live ? processorRef.getVocoderBandLevel ((int) b) : 0.0f;
                const auto next = smooth ? bandLevels[b] + (target - bandLevels[b]) * 0.4f : target;
                moving = moving || std::abs (next - bandLevels[b]) > 1.0e-4f;
                bandLevels[b] = std::abs (next) < 1.0e-5f ? 0.0f : next;
            }
            dirty = dirty || moving;
        }

        if (kind == Kind::dynamics)
        {
            const auto before = meterGr;
            updateMeters (smooth);
            dirty = dirty || metersMoving || before != meterGr;
        }

        return dirty;
    }

    void timerCallback() override
    {
        if (isShowing() && refresh (true))
            repaint();
    }
};
