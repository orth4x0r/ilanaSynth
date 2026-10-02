#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../dsp/airwindows/Categories.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// The small picture on an FX card, one per effect family, drawn from the
// module's parameters (and its live gain reduction where the processor
// measures one): a transfer curve for drive / amp / crush and the Airwindows
// saturators, in-out curves with GR meters for comp / OTT / limiter, echoes
// on a beat grid for the delay and a decay curve for the reverb. The
// formulas mirror src/processor/Effects.cpp; the display never changes sound.
class FxDisplay : public juce::Component,
                  public juce::SettableTooltipClient,
                  private IlanaAnim::FrameTimer
{
public:
    enum class Kind { none, transfer, airwindowsTransfer, dynamics, delay, reverb };

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
        const auto line = getLocalBounds().reduced (8, 0).removeFromTop (16).withTrimmedTop (3);
        g.drawText (left, line, juce::Justification::centredLeft);
        if (right.isNotEmpty())
            g.drawText (right, line, juce::Justification::centredRight);
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

    void measureAirwindows()
    {
        const auto category = airwindows::categoryForFxType (type);
        if (category < 0)
            return;

        const auto& module = airwindows::categoryModules()[(size_t) category];
        const juce::String prefix = juce::String ("fx_") + module.id;
        const auto choice = juce::jlimit (0, (int) module.algorithms.size() - 1, (int) param (prefix + "_algo"));
        const auto index = module.algorithms[(size_t) choice];
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

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.drawText ("HIGHS", juce::Rectangle<float> (toX (juce::jmin (highTime, shownTime) * 0.45f), toY (-60.0f * 0.45f * juce::jmin (highTime, shownTime) / highTime) - 2.0f, 40.0f, 12.0f),
                    juce::Justification::centredLeft);
        const auto seconds = gated ? juce::String ("gated") : "RT60 " + juce::String (lowTime, lowTime < 10.0f ? 1 : 0) + " s";
        paintCaption (g, "DECAY", seconds);
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
            case Kind::airwindowsTransfer:
                if (const auto category = airwindows::categoryForFxType (type); category >= 0)
                {
                    const juce::String prefix = juce::String ("fx_") + airwindows::categoryModules()[(size_t) category].id;
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
            if (kind == Kind::airwindowsTransfer)
                measureAirwindows();
            dirty = true;
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
