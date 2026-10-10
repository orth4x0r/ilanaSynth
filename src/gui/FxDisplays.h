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
    enum class Kind { none, transfer, airwindowsTransfer, dynamics, delay, reverb, vowel, comb, chorus, phaser, vocoder, airwindowsImpulse, airwindowsResponse, freeze, picture };

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
            case 12: return Kind::freeze;
            case 37: case 39: case 40: return Kind::airwindowsTransfer; // AW Dynamics, Console, Lo-Fi: what it does to a sine
            case 38: return Kind::airwindowsResponse; // AW EQ
            case 34: case 35: return Kind::airwindowsImpulse; // AW Reverb (spaces), AW Delay (echo)
            // The effects that were a sentence in a well: a small picture each (N16-3).
            case 8: case 10: case 11: case 14: case 15: case 17: case 18: case 19: case 22:
            case 23: case 24: case 25: case 26: case 28: case 36: case 41: return Kind::picture;
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
            case Kind::airwindowsResponse: paintResponse (g); break;
            case Kind::freeze:             paintFreeze (g); break;
            case Kind::picture:            paintPicture (g); break;
            case Kind::none:               break;
        }
        flushTicks (g);
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
                {
                    const auto pushed = x * juce::jlimit (1.0f, 20.0f, param ("fx_drive_amount"));
                    const auto driven = param ("fx_drive_type") > 0.5f ? juce::jlimit (-1.0f, 1.0f, pushed) : std::tanh (pushed);
                    y = x + (driven - x) * param ("fx_drive_mix");
                }
                const auto fold = param ("fx_fold");
                if (fold > 0.001f && param ("fx_fold_type") > 0.5f)
                {
                    y = std::sin (y * (1.0f + fold * 5.0f) * juce::MathConstants<float>::halfPi) / (1.0f + fold * 1.5f);
                }
                else if (fold > 0.001f)
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

    // The curve fills its picture, as the design draws it (a row's picture is wide and low).
    juce::Rectangle<float> squarePlot() const { return plotArea(); }

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
        paintInputDot (g, plot, transferAt (inputLevel));

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
        kind = category == "Space" || category == "Delay" ? Kind::airwindowsImpulse
               : category == "EQ & Filter" ? Kind::airwindowsResponse
                                           : Kind::airwindowsTransfer;
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
            paintInputDot (g, plot, measuredAt (inputLevel));
        }

        paintCaption (g, "TRANSFER");
    }

    // ---- The live input on a transfer curve (step 14) ----
    // The slot's input level (smoothed RMS, as a sine's peak) drawn as a dot
    // on the curve: where the signal sits on the shaper right now. Nothing
    // while the slot is silent.
    float inputLevel = 0.0f;

    int slotIndexOfType() const
    {
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            if ((int) param ("fx_slot" + juce::String (slot)) == type)
                return slot - 1;
        return -1;
    }

    // The measured Airwindows curve's output at x, interpolated (0 when empty).
    float measuredAt (float x) const
    {
        for (size_t i = 1; i < measured.size(); ++i)
        {
            const auto a = measured[i - 1], b = measured[i];
            if ((x >= a.x && x <= b.x) || (x <= a.x && x >= b.x))
                return std::abs (b.x - a.x) < 1.0e-6f ? a.y : a.y + (b.y - a.y) * (x - a.x) / (b.x - a.x);
        }
        return x;
    }

    void paintInputDot (juce::Graphics& g, juce::Rectangle<float> plot, float y) const
    {
        if (inputLevel < 0.004f)
            return;

        const juce::Point<float> centre (plot.getX() + (inputLevel + 1.0f) * 0.5f * plot.getWidth(),
                                         plot.getCentreY() - juce::jlimit (-1.1f, 1.1f, y) * 0.5f * plot.getHeight());
        g.setColour (colour.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (centre));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (centre));
    }

    bool updateInputLevel (bool smooth)
    {
        const auto slot = slotIndexOfType();
        const auto target = slot >= 0 ? juce::jlimit (0.0f, 1.0f, processorRef.getFxSlotInLevel (slot) * juce::MathConstants<float>::sqrt2) : 0.0f;
        const auto next = smooth ? inputLevel + (target - inputLevel) * 0.4f : target;
        const auto moved = std::abs (next - inputLevel) > 0.002f;
        inputLevel = next < 0.002f && target < 0.002f ? 0.0f : next;
        return moved;
    }

public:
    // For the UI test: the dot's level (0 = no dot).
    float getInputDotLevel() const { return inputLevel; }
    void refreshNow() { refresh (false); }

private:
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

    // A tiny axis label: grey, tiny type, never louder than the grid it names. It is queued and
    // drawn after the picture (flushTicks) on a small plate in the well's colour, so a trace,
    // fill or bar never runs through it.
    void paintTick (juce::Graphics&, const juce::String& text, juce::Rectangle<float> box, juce::Justification justification, float alpha = 0.55f) const
    {
        pendingTicks.push_back ({ text, box, justification, alpha });
    }

    void flushTicks (juce::Graphics& g) const
    {
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny);
        for (const auto& tick : pendingTicks)
        {
            const auto room = tick.box.toNearestInt();
            const auto width = juce::jmin ((float) room.getWidth(), juce::GlyphArrangement::getStringWidth (font, tick.text) + 1.0f);
            auto plate = juce::Rectangle<float> (width, juce::jmin (tick.box.getHeight(), font.getHeight() + 1.0f));
            plate.setCentre (tick.box.getCentre());
            if (tick.justification.testFlags (juce::Justification::left))
                plate.setX (tick.box.getX());
            else if (tick.justification.testFlags (juce::Justification::right))
                plate.setRight (tick.box.getRight());
            g.setColour (IlanaTheme::Ui::well.withAlpha (0.82f));
            g.fillRoundedRectangle (plate.expanded (2.0f, 0.0f), 2.0f);
            g.setColour (IlanaTheme::Ui::text3.withAlpha (tick.alpha));
            g.setFont (font);
            IlanaTheme::drawFitted (g, tick.text, room, tick.justification, 1);
        }
        pendingTicks.clear();
    }

    struct PendingTick { juce::String text; juce::Rectangle<float> box; juce::Justification justification; float alpha; };
    mutable std::vector<PendingTick> pendingTicks;

    static juce::String dbText (float db, bool signedValue = false)
    {
        const auto minus = juce::String::fromUTF8 ("\xe2\x88\x92");
        const auto value = juce::String (std::abs (db), std::abs (db) < 10.0f ? 1 : 0);
        return (db < -0.05f ? minus : (signedValue && db > 0.05f ? juce::String ("+") : juce::String())) + value;
    }

    void paintDynamics (juce::Graphics& g)
    {
        auto area = plotArea();
        const auto meters = area.removeFromRight (type == 20 ? 34.0f : 14.0f);
        area.removeFromRight (6.0f);
        // The curve is a picture of its own shape, not a line stretched over
        // the whole card: it keeps a 2.2 : 1 plot and the spare width carries
        // the settings as labelled readings (UI review 15, V15-2).
        const auto plotWidth = juce::jmin (area.getWidth(), juce::jmax (180.0f, area.getHeight() * 2.2f));
        const auto plot = area.withWidth (plotWidth);
        auto readout = area.withTrimmedLeft (plotWidth + 18.0f);

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

        // The dB scale: the same on both axes (input across, output up).
        if (plot.getHeight() > 34.0f)
            for (const auto db : { -48.0f, -24.0f, 0.0f })
            {
                const auto name = db < -0.5f ? juce::String::fromUTF8 ("\xe2\x88\x92") + juce::String ((int) -db) : juce::String ("0");
                paintTick (g, name, juce::Rectangle<float> (toX (db) + 3.0f, plot.getBottom() - 10.0f, 22.0f, 10.0f), juce::Justification::centredLeft);
                if (db > -40.0f)
                    paintTick (g, name, juce::Rectangle<float> (plot.getX() + 2.0f, toY (db) + 1.0f, 22.0f, 10.0f), juce::Justification::centredLeft);
            }

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

        // Where the signal sits on the curve now.
        if (inputLevel >= 0.004f)
        {
            const auto inDb = juce::jlimit (floorDb, topDb, juce::Decibels::gainToDecibels (inputLevel, floorDb));
            const juce::Point<float> centre (toX (inDb), toY (dynamicsOutDb (inDb)));
            g.setColour (colour.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (centre));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (centre));
        }

        if (type == 20)
            paintOttMeters (g, meters);
        else
            paintGrMeter (g, meters, meterGr[0]);

        // The settings, named, beside the curve.
        if (readout.getWidth() >= 110.0f)
        {
            std::vector<std::pair<juce::String, juce::String>> rows;
            if (type == 4)
            {
                rows.push_back ({ "THRESHOLD", dbText (param ("fx_comp_threshold")) + " dB" });
                rows.push_back ({ "RATIO", juce::String (param ("fx_comp_ratio"), 1) + " : 1" });
                rows.push_back ({ "MAKEUP", dbText (param ("fx_comp_makeup"), true) + " dB" });
            }
            else if (type == 21)
            {
                rows.push_back ({ "CEILING", dbText (param ("fx_limit_ceiling")) + " dB" });
            }
            else
            {
                rows.push_back ({ "AMOUNT", juce::String (juce::roundToInt (param ("fx_ott_amount") * 100.0f)) + "%" });
                static const char* const bandNames[] { "LOW", "MID", "HIGH" };
                for (size_t band = 0; band < 3; ++band)
                    rows.push_back ({ bandNames[band], dbText (meterGr[band], true) + " dB" });
            }
            if (type != 20)
                rows.push_back ({ "REDUCTION", dbText (meterGr[0]) + " dB" });

            // (A low picture shows the first readings that fit, a line each, never overlapped.)
            rows.resize ((size_t) juce::jlimit (1, (int) rows.size(), (int) (readout.getHeight() / 12.0f)));
            const auto rowHeight = juce::jmin (16.0f, readout.getHeight() / (float) rows.size());
            auto column = readout.withHeight (rowHeight * (float) rows.size()).withCentre (readout.getCentre()).withWidth (juce::jmin (readout.getWidth(), 190.0f));
            for (const auto& row : rows)
            {
                auto line = column.removeFromTop (rowHeight);
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                IlanaTheme::drawFitted (g, row.first, line.removeFromLeft (84.0f).toNearestInt(), juce::Justification::centredLeft, 1);
                g.setColour (IlanaTheme::Ui::text2);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                IlanaTheme::drawFitted (g, row.second, line.toNearestInt(), juce::Justification::centredLeft, 1);
            }
        }

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

    // ---- Airwindows EQs and filters: the frequency response of the algorithm itself ----
    std::vector<float> responseDb;

    void measureResponse()
    {
        responseDb.clear();
        juce::String prefix;
        const auto index = airwindowsSource (prefix);
        if (index < 0)
            return;

        const auto& info = airwindows::registry()[(size_t) index];
        auto run = info.create();
        if (run == nullptr)
            return;

        constexpr double rate = 44100.0;
        constexpr int order = 13, size = 1 << order;
        run->prepare (rate);
        for (int k = 0; k < info.numKnobs; ++k)
            run->setParam (info.knobs[k].parameter, info.knobs[k].toPlugin (param (prefix + "_p" + juce::String (k + 1))));

        // The impulse response (the module's own mix applied), one FFT of it.
        std::vector<float> left ((size_t) size, 0.0f), right ((size_t) size, 0.0f);
        left[0] = right[0] = 0.5f;
        run->process (left.data(), right.data(), size);
        const auto mix = juce::jlimit (0.0f, 1.0f, param (prefix + "_mix"));
        std::vector<std::complex<float>> in ((size_t) size), out ((size_t) size);
        for (int i = 0; i < size; ++i)
        {
            const auto wet = std::isfinite (left[(size_t) i]) ? left[(size_t) i] : 0.0f;
            in[(size_t) i] = { (wet * mix + (i == 0 ? 0.5f * (1.0f - mix) : 0.0f)) * 2.0f, 0.0f };
        }
        juce::dsp::FFT (order).perform (in.data(), out.data(), false);

        constexpr int points = 160;
        responseDb.resize (points);
        for (int i = 0; i < points; ++i)
        {
            const auto hz = 20.0 * std::pow (1000.0, (double) i / (points - 1)); // 20 Hz to 20 kHz
            const auto bin = juce::jlimit (1, size / 2 - 1, juce::roundToInt (hz / rate * size));
            responseDb[(size_t) i] = juce::Decibels::gainToDecibels (std::abs (out[(size_t) bin]), -60.0f);
        }
    }

    void paintResponse (juce::Graphics& g)
    {
        const auto plot = plotArea();
        constexpr float range = 24.0f;
        const auto toY = [&] (float db) { return plot.getCentreY() - juce::jlimit (-range, range, db) / range * 0.5f * plot.getHeight(); };
        const auto toX = [&] (float hz) { return plot.getX() + std::log10 (hz / 20.0f) / 3.0f * plot.getWidth(); };

        // The grid is the display: decades across, +-12 dB and 0 up, each
        // labelled, so a flat response still reads as "flat at 0 dB" (N16-10).
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (toX (hz), plot.getY()));
        for (const auto db : { -12.0f, 12.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), toY (db)));
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getCentreY()));

        if (plot.getHeight() > 30.0f)
        {
            for (const auto& tick : { std::pair<float, const char*> { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
                paintTick (g, tick.second, juce::Rectangle<float> (toX (tick.first) + 2.0f, plot.getBottom() - 10.0f, 26.0f, 10.0f), juce::Justification::centredLeft);
            paintTick (g, "0 dB", juce::Rectangle<float> (plot.getX() + 2.0f, plot.getCentreY() - 11.0f, 30.0f, 10.0f), juce::Justification::centredLeft);
            if (plot.getHeight() > 56.0f)
            {
                paintTick (g, "+12", juce::Rectangle<float> (plot.getX() + 2.0f, toY (12.0f) + 1.0f, 24.0f, 10.0f), juce::Justification::centredLeft);
                paintTick (g, juce::String::fromUTF8 ("\xe2\x88\x92") + "12", juce::Rectangle<float> (plot.getX() + 2.0f, toY (-12.0f) - 11.0f, 24.0f, 10.0f), juce::Justification::centredLeft);
            }
        }

        auto lo = 0.0f, hi = 0.0f;
        if (! responseDb.empty())
        {
            juce::Path curve;
            for (size_t i = 0; i < responseDb.size(); ++i)
            {
                lo = juce::jmin (lo, responseDb[i]);
                hi = juce::jmax (hi, responseDb[i]);
                const juce::Point<float> point (plot.getX() + plot.getWidth() * (float) i / (float) (responseDb.size() - 1), toY (responseDb[i]));
                if (i == 0)
                    curve.startNewSubPath (point);
                else
                    curve.lineTo (point);
            }
            strokeCurve (g, curve, plot, plot.getCentreY());
        }

        // The extremes, so a flat line says how flat.
        const auto flat = hi - lo < 0.3f;
        const auto text = flat ? juce::String ("FLAT") : "+" + juce::String (hi, 1) + " / " + dbText (lo) + " dB";
        paintCaption (g, "RESPONSE", text);
    }

    // ---- FREEZE: the spectrum it holds (or, with HOLD off, the one passing) ----
    std::array<float, SpectralFreeze::numBands> freezeBands {};

    void paintFreeze (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto count = (int) freezeBands.size();
        const auto width = plot.getWidth() / (float) count;
        const auto held = param ("fx_freeze_on") > 0.5f;
        auto any = false;

        for (int b = 0; b < count; ++b)
        {
            const auto bar = juce::Rectangle<float> (plot.getX() + width * (float) b, plot.getY(), width, plot.getHeight()).reduced (0.5f, 0.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (bar);
            const auto level = freezeBands[(size_t) b];
            if (level > 0.01f)
            {
                any = true;
                g.setColour (colour.withAlpha (held ? 0.85f : 0.45f));
                g.fillRect (bar.withTop (bar.getBottom() - bar.getHeight() * level));
            }
        }

        if (! any)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText ("play a note to see its spectrum", plot, juce::Justification::centred);
        }

        paintCaption (g, held ? "HELD SPECTRUM" : "SPECTRUM", held ? "HOLD" : "NOT HELD");
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

        constexpr double rate = 44100.0;
        constexpr int columns = 220;

        // One run of the algorithm over `seconds`: its impulse response as
        // the loudest level in each of the picture's columns.
        const auto measure = [&] (float seconds)
        {
            auto run = info.create();
            if (run == nullptr)
                return false;

            impulseSeconds = seconds;
            run->prepare (rate);
            for (int k = 0; k < info.numKnobs; ++k)
                run->setParam (info.knobs[k].parameter, info.knobs[k].toPlugin (param (prefix + "_p" + juce::String (k + 1))));

            const auto total = (int) (rate * seconds);
            std::vector<float> left ((size_t) total, 0.0f), right ((size_t) total, 0.0f);
            left[0] = right[0] = 0.8f;
            run->process (left.data(), right.data(), total);

            impulseEnvelope.assign (columns, 0.0f);
            for (int i = 0; i < total; ++i)
            {
                auto& cell = impulseEnvelope[(size_t) juce::jmin (columns - 1, i * columns / total)];
                const auto v = std::abs (std::isfinite (left[(size_t) i]) ? left[(size_t) i] : 0.0f)
                               + std::abs (std::isfinite (right[(size_t) i]) ? right[(size_t) i] : 0.0f);
                cell = juce::jmax (cell, v * 0.5f);
            }
            return true;
        };

        if (! measure (isSpace() ? 4.0f : 2.5f))
            return;

        // An echo that is over early gets a picture of its own length, not a
        // few lines in the first tenth of a long one (I14-2).
        if (! isSpace())
        {
            auto peak = 1.0e-6f;
            for (const auto v : impulseEnvelope)
                peak = juce::jmax (peak, v);
            auto last = 0;
            for (int i = 0; i < columns; ++i)
                if (impulseEnvelope[(size_t) i] > peak * 0.01f)
                    last = i;
            const auto lastSeconds = (float) (last + 1) / (float) columns * impulseSeconds;
            if (lastSeconds < 0.6f * impulseSeconds)
                measure (juce::jlimit (0.1f, 2.5f, lastSeconds * 1.3f));
        }
    }

    void paintImpulse (juce::Graphics& g)
    {
        const auto plot = plotArea();
        auto step = 1.0f;
        for (const auto candidate : { 0.005f, 0.01f, 0.02f, 0.05f, 0.1f, 0.25f, 0.5f, 1.0f })
            if (impulseSeconds / candidate <= 6.0f)
            {
                step = candidate;
                break;
            }
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (auto t = step; t < impulseSeconds - step * 0.1f; t += step)
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (plot.getX() + t / impulseSeconds * plot.getWidth(), plot.getY()));
        for (const auto db : { -12.0f, -24.0f, -36.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getBottom() - (db + 48.0f) / 48.0f * plot.getHeight()));

        // The scales: seconds along the foot, dB up the side.
        if (plot.getHeight() > 30.0f)
        {
            for (auto t = step; t < impulseSeconds - step * 0.4f; t += step)
            {
                const auto label = impulseSeconds < 1.0f ? juce::String (juce::roundToInt (t * 1000.0f)) + " ms" : juce::String (t, 1) + " s";
                paintTick (g, label, juce::Rectangle<float> (plot.getX() + t / impulseSeconds * plot.getWidth() + 2.0f, plot.getBottom() - 10.0f, 40.0f, 10.0f), juce::Justification::centredLeft);
            }
            paintTick (g, "0 dB", juce::Rectangle<float> (plot.getX() + 2.0f, plot.getY(), 30.0f, 10.0f), juce::Justification::centredLeft);
            paintTick (g, juce::String::fromUTF8 ("\xe2\x88\x92") + "24", juce::Rectangle<float> (plot.getX() + 2.0f, plot.getCentreY() - 5.0f, 30.0f, 10.0f), juce::Justification::centredLeft);
        }

        auto peak = 1.0e-6f;
        for (const auto v : impulseEnvelope)
            peak = juce::jmax (peak, v);

        // A lone spike at the start is the dry hit with nothing after it:
        // say so instead of drawing one static line (UI review 13, I13-3).
        auto after = 0.0f;
        for (size_t i = 4; i < impulseEnvelope.size(); ++i)
            after = juce::jmax (after, impulseEnvelope[i]);
        const auto hasTail = after > peak * 0.003f;

        if (! impulseEnvelope.empty() && peak > 1.0e-5f && hasTail)
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
            // The dry hit alone, where the echoes would start: one bar at 0 s.
            g.setColour (colour.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (3.0f, plot.getHeight()).withPosition (plot.getX(), plot.getY()));
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.drawText (isSpace() ? "no tail at these settings" : "no echo within " + juce::String (impulseSeconds, 1) + " s at these settings",
                        plot.withTrimmedLeft (12.0f), juce::Justification::centred);
        }

        paintCaption (g, isSpace() ? "IMPULSE" : "ECHOES", impulseSeconds < 1.0f ? juce::String (juce::roundToInt (impulseSeconds * 1000.0f)) + " ms" : juce::String (impulseSeconds, 1) + " s");
    }

    // ---- Pictures for the effects that had only a sentence (UI review 16, N16-3) ----
    // Each one is drawn from the effect's own settings, in the style of the
    // graphs above: a plot with a quiet grid, the effect's curve in its
    // colour, a caption and a reading. The sentence on what the effect does
    // moved to the card's title tooltip.
    static constexpr float pi = juce::MathConstants<float>::pi;

    juce::String choiceText (const char* id) const
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (id)))
            return choice->getCurrentChoiceName();
        return {};
    }

    static float hash01 (int n)
    {
        auto x = (juce::uint32) (n * 7919 + 17);
        x = (x ^ (x >> 13)) * 1274126177u;
        return (float) ((x ^ (x >> 16)) & 0xffffu) / 65535.0f;
    }

    static juce::String signedText (float value, int decimals, const char* unit)
    {
        return (value > 0.0f ? "+" : (value < 0.0f ? juce::String::fromUTF8 ("\xe2\x88\x92") : juce::String())) + juce::String (std::abs (value), decimals) + unit;
    }

    void paintLaneLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> lane) const
    {
        paintTick (g, text, lane.withWidth (28.0f).withHeight (10.0f), juce::Justification::centredLeft, 0.8f);
    }

    // A sweep of a delay against time: the chorus family's picture.
    void paintSweep (juce::Graphics& g, juce::Rectangle<float> plot, float rate, float centreMs, float swingMs, float topMs,
                     std::initializer_list<float> phases, float seconds) const
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        const auto step = topMs > 12.0f ? 5.0f : 1.0f;
        for (auto ms = step; ms < topMs; ms += step)
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getBottom() - ms / topMs * plot.getHeight()));
        if (plot.getHeight() > 30.0f)
            for (auto ms = step * 2.0f; ms < topMs; ms += step * 2.0f)
                paintTick (g, juce::String ((int) ms) + " ms", juce::Rectangle<float> (plot.getX() + 2.0f, plot.getBottom() - ms / topMs * plot.getHeight() - 10.0f, 34.0f, 10.0f), juce::Justification::centredLeft);

        auto alpha = 1.0f;
        for (const auto phase : phases)
        {
            juce::Path sweep;
            const auto steps = juce::jmax (32, (int) plot.getWidth());
            for (int i = 0; i <= steps; ++i)
            {
                const auto t = seconds * (float) i / (float) steps;
                const auto ms = juce::jlimit (0.0f, topMs, centreMs + swingMs * std::sin (2.0f * pi * rate * t + phase));
                const juce::Point<float> point (plot.getX() + plot.getWidth() * (float) i / (float) steps, plot.getBottom() - ms / topMs * plot.getHeight());
                if (i == 0)
                    sweep.startNewSubPath (point);
                else
                    sweep.lineTo (point);
            }
            g.setColour (colour.withAlpha (alpha));
            g.strokePath (sweep, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            alpha *= 0.55f;
        }
    }

    void paintHaas (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto delay = param ("fx_haas_delay"), mix = param ("fx_haas_mix");
        const auto left = plot.withHeight (plot.getHeight() * 0.5f - 2.0f);
        const auto right = plot.withTrimmedTop (plot.getHeight() * 0.5f + 2.0f);
        const auto x0 = plot.getX() + 26.0f, span = plot.getWidth() - 34.0f;
        const auto toX = [=] (float ms) { return x0 + ms / 40.0f * span; };

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto ms : { 10.0f, 20.0f, 30.0f, 40.0f })
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (toX (ms), plot.getY()));
        for (const auto lane : { left, right })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), lane.getBottom()));
        paintLaneLabel (g, "L", left);
        paintLaneLabel (g, "R", right);
        if (plot.getHeight() > 30.0f)
            for (const auto ms : { 10.0f, 20.0f, 30.0f })
                paintTick (g, juce::String ((int) ms) + " ms", juce::Rectangle<float> (toX (ms) + 2.0f, plot.getBottom() - 10.0f, 34.0f, 10.0f), juce::Justification::centredLeft);

        const auto pulse = [&] (juce::Rectangle<float> lane, float ms, float level, float alpha)
        {
            const auto height = (lane.getHeight() - 4.0f) * level;
            g.setColour (colour.withAlpha (alpha));
            g.fillRoundedRectangle (juce::Rectangle<float> (3.0f, juce::jmax (2.0f, height)).withBottomY (lane.getBottom() - 1.0f).withX (toX (ms) - 1.5f), 1.5f);
        };
        pulse (left, 0.0f, 1.0f, 1.0f);
        pulse (right, 0.0f, 1.0f - mix, 0.5f);
        pulse (right, delay, 0.35f + 0.65f * mix, 1.0f);

        // The gap between the two ears.
        const auto y = right.getY() - 2.0f;
        g.setColour (colour.withAlpha (0.6f));
        g.drawLine (toX (0.0f), y, toX (delay), y, 1.0f);
        paintCaption (g, "EAR TO EAR", juce::String (delay, 1) + " ms");
    }

    void paintStutter (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto reverse = param ("fx_stutter_reverse") > 0.5f;
        const auto pitch = param ("fx_stutter_pitch");
        const auto slices = 4;
        const auto width = plot.getWidth() / (float) slices;
        for (int s = 0; s < slices; ++s)
        {
            const auto cell = juce::Rectangle<float> (plot.getX() + width * (float) s, plot.getY(), width, plot.getHeight()).reduced (2.0f, 0.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (cell, 3.0f);
            const auto flipped = reverse && (s % 2 == 1);
            juce::Path wave;
            const auto steps = juce::jmax (16, (int) cell.getWidth() / 2);
            const auto lane = plot.getHeight() > 30.0f ? cell.withTrimmedTop (12.0f) : cell;
            for (int i = 0; i <= steps; ++i)
            {
                const auto t = (float) i / (float) steps;
                const auto u = flipped ? 1.0f - t : t;
                const auto level = (0.25f + 0.75f * u) * std::sin (2.0f * pi * (3.0f + std::abs (pitch) / 12.0f) * u) * 0.42f + 0.0f;
                const juce::Point<float> point (lane.getX() + t * lane.getWidth(), lane.getCentreY() - level * lane.getHeight());
                if (i == 0)
                    wave.startNewSubPath (point);
                else
                    wave.lineTo (point);
            }
            g.setColour (colour.withAlpha (s == 0 ? 0.45f : 1.0f));
            g.strokePath (wave, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (plot.getHeight() > 30.0f)
                paintTick (g, s == 0 ? "SLICE" : (flipped ? "REVERSED" : "REPEAT"), cell.withTrimmedLeft (3.0f).withHeight (10.0f), juce::Justification::centredLeft);
        }
        paintCaption (g, "REPEATS", choiceText ("fx_stutter_div") + (pitch != 0.0f ? "  " + signedText (pitch, 0, " st") : juce::String()));
    }

    void paintSmear (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto size = param ("fx_smear_size"), density = param ("fx_smear_density"), mix = param ("fx_smear_mix");
        const auto x0 = plot.getX() + 6.0f;
        const auto span = (plot.getWidth() - 14.0f) * juce::jlimit (0.15f, 1.0f, size / 300.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getBottom() - 1.0f));
        // The hit going in.
        g.setColour (IlanaTheme::Ui::text3.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (2.0f, plot.getHeight() - 2.0f).withPosition (x0 - 1.0f, plot.getY()));
        // The cloud of grains it becomes: more of them with DENSITY, lower with the tail.
        const auto grains = juce::jlimit (3, 60, (int) (density * 1.6f));
        g.setColour (colour.withAlpha (0.35f + 0.6f * mix));
        for (int i = 0; i < grains; ++i)
        {
            const auto t = (float) (i + 1) / (float) grains;
            const auto envelope = std::exp (-2.6f * t) * (0.45f + 0.55f * hash01 (i));
            const auto h = juce::jmax (2.0f, (plot.getHeight() - 4.0f) * envelope * (0.55f + 0.45f * mix));
            g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, h).withBottomY (plot.getBottom() - 1.0f).withX (x0 + 5.0f + t * span), 1.0f);
        }
        paintTick (g, "IN", juce::Rectangle<float> (x0 + 3.0f, plot.getY(), 20.0f, 10.0f), juce::Justification::centredLeft, 0.8f);
        paintCaption (g, "HIT BLURRED", juce::String (juce::roundToInt (size)) + " ms " + juce::String::fromUTF8 ("\xc2\xb7") + " " + juce::String (juce::roundToInt (density)) + " grains");
    }

    void paintTapeStop (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto stop = param ("fx_tape_stop_time"), mix = param ("fx_tape_stop_mix");
        const auto armed = param ("fx_tape_stop_trigger") > 0.5f;
        constexpr float window = 4.0f;
        const auto toX = [&] (float s) { return plot.getX() + 12.0f + s / window * (plot.getWidth() - 14.0f); };
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto s : { 1.0f, 2.0f, 3.0f })
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (toX (s), plot.getY()));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getBottom() - 1.0f));
        if (plot.getHeight() > 30.0f)
        {
            for (const auto s : { 1.0f, 2.0f, 3.0f })
                paintTick (g, juce::String ((int) s) + " s", juce::Rectangle<float> (toX (s) + 2.0f, plot.getBottom() - 11.0f, 24.0f, 10.0f), juce::Justification::centredLeft);
            paintTick (g, "100%", juce::Rectangle<float> (plot.getX() + 2.0f, plot.getY(), 26.0f, 10.0f), juce::Justification::centredLeft);
            paintTick (g, "0", juce::Rectangle<float> (plot.getX() + 2.0f, plot.getBottom() - 11.0f, 10.0f, 10.0f), juce::Justification::centredLeft);
        }
        // The tape's speed: full, a ramp down over the stop time, and standstill.
        juce::Path speed;
        const auto top = plot.getY() + 3.0f, bottom = plot.getBottom() - 2.0f;
        speed.startNewSubPath (plot.getX() + 12.0f, top);
        speed.lineTo (toX (0.4f), top);
        speed.lineTo (toX (juce::jmin (window, 0.4f + stop)), bottom);
        speed.lineTo (plot.getRight() - 2.0f, bottom);
        strokeCurve (g, speed, plot, bottom);
        paintCaption (g, "TAPE SPEED", juce::String (stop, 2) + " s " + juce::String::fromUTF8 ("\xc2\xb7") + " " + juce::String (juce::roundToInt (mix * 100.0f)) + "%" + (armed ? "  STOPPED" : juce::String()));
    }

    void paintTilt (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto tilt = juce::jlimit (-1.0f, 1.0f, param ("fx_tilt")), level = param ("fx_tilt_level");
        constexpr double low = 20.0, high = 20000.0;
        constexpr float range = 18.0f;
        const auto lowGain = (double) std::pow (10.0f, (-tilt * 12.0f + level) / 20.0f);
        const auto highGain = (double) std::pow (10.0f, (tilt * 12.0f + level) / 20.0f);
        paintFrequencyGrid (g, plot, low, high);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto db : { -12.0f, 12.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getCentreY() - db / range * 0.5f * plot.getHeight() * 1.0f));
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getCentreY()));
        if (plot.getHeight() > 30.0f)
        {
            for (const auto& tick : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
                paintTick (g, tick.second, juce::Rectangle<float> (logX (plot, tick.first, low, high) + 2.0f, plot.getBottom() - 10.0f, 24.0f, 10.0f), juce::Justification::centredLeft);
            paintTick (g, "0 dB", juce::Rectangle<float> (plot.getX() + 2.0f, plot.getCentreY() - 11.0f, 30.0f, 10.0f), juce::Justification::centredLeft);
        }
        // The pivot, 700 Hz.
        g.setColour (colour.withAlpha (0.3f));
        const auto pivot = logX (plot, 700.0, low, high);
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (pivot, plot.getY()));
        const auto curve = responsePath (plot, low, high, -range, range, [&] (double f)
        {
            const std::complex<double> s (0.0, f / 700.0);
            return lowGain / (1.0 + s) + highGain * s / (1.0 + s);
        });
        strokeCurve (g, curve, plot, plot.getCentreY());
        paintCaption (g, "SEE-SAW AT 700 Hz", signedText (-tilt * 12.0f, 1, " dB low") + "  " + signedText (tilt * 12.0f, 1, " dB high"));
    }

    void paintUtility (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto gainDb = param ("fx_util_gain");
        const auto mono = param ("fx_util_mono") > 0.5f, invert = param ("fx_util_invert") > 0.5f;
        const auto gain = juce::jlimit (0.0f, 1.0f, juce::Decibels::decibelsToGain (gainDb) * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getCentreY()));

        const auto draw = [&] (float amount, juce::Colour c, float thickness)
        {
            juce::Path wave;
            const auto steps = juce::jmax (32, (int) plot.getWidth());
            for (int i = 0; i <= steps; ++i)
            {
                const auto t = (float) i / (float) steps;
                const auto y = juce::jlimit (-0.5f, 0.5f, amount * 0.5f * std::sin (2.0f * pi * 3.0f * t));
                const juce::Point<float> point (plot.getX() + t * plot.getWidth(), plot.getCentreY() - y * (plot.getHeight() - 2.0f));
                if (i == 0)
                    wave.startNewSubPath (point);
                else
                    wave.lineTo (point);
            }
            g.setColour (c);
            g.strokePath (wave, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        draw (1.0f, IlanaTheme::Ui::text3.withAlpha (0.55f), 1.0f);
        draw ((invert ? -1.0f : 1.0f) * gain * 2.0f, colour, 1.6f);

        // The two switches, lit while on.
        // (On the caption's line, left of the gain reading.)
        auto chips = getLocalBounds().toFloat().reduced (8.0f, 0.0f).removeFromTop (16.0f).withTrimmedTop (2.0f).withTrimmedBottom (1.0f);
        chips.removeFromRight (juce::GlyphArrangement::getStringWidth (IlanaTheme::font (IlanaTheme::TextSize::tiny, true), signedText (gainDb, 1, " dB")) + 12.0f);
        for (const auto& [name, on] : { std::pair<const char*, bool> { "MONO", mono }, { "INVERT", invert } })
        {
            const auto chip = chips.removeFromRight (juce::GlyphArrangement::getStringWidth (IlanaTheme::font (IlanaTheme::TextSize::tiny, true), name) + 12.0f);
            chips.removeFromRight (4.0f);
            g.setColour (on ? colour.withAlpha (0.25f) : juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (chip, 5.0f);
            g.setColour (on ? colour : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (name, chip, juce::Justification::centred);
        }
        paintCaption (g, "IN (GREY) / OUT", signedText (gainDb, 1, " dB"));
    }

    void paintWidener (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto width = param ("fx_width"), mix = param ("fx_width_mix");
        const auto effective = 1.0f + (width - 1.0f) * mix;
        // MID and SIDE as bars against the 100 % mark: widening is the side's level.
        auto bars = plot.withTrimmedLeft (44.0f).withTrimmedRight (6.0f);
        const auto rowHeight = bars.getHeight() / 2.0f;
        const auto toX = [&] (float v) { return bars.getX() + v / 2.0f * bars.getWidth(); };
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto v : { 0.5f, 1.0f, 1.5f })
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (toX (v), plot.getY()));
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (toX (1.0f), plot.getY()));
        int row = 0;
        for (const auto& [name, value] : { std::pair<const char*, float> { "MID", 1.0f }, { "SIDE", effective } })
        {
            const auto lane = juce::Rectangle<float> (plot.getX(), plot.getY() + rowHeight * (float) row, plot.getWidth(), rowHeight).reduced (0.0f, 3.0f);
            paintTick (g, name, lane.withWidth (40.0f), juce::Justification::centredLeft, 0.9f);
            g.setColour (row == 0 ? IlanaTheme::Ui::text3.withAlpha (0.6f) : colour);
            g.fillRoundedRectangle (juce::Rectangle<float> (toX (value) - bars.getX(), lane.getHeight()).withPosition (bars.getX(), lane.getY()), 3.0f);
            if (lane.getHeight() > 12.0f)
                paintTick (g, juce::String (juce::roundToInt (value * 100.0f)) + "%", juce::Rectangle<float> (toX (value) + 4.0f, lane.getY(), 36.0f, lane.getHeight()), juce::Justification::centredLeft, 0.9f);
            ++row;
        }
        paintCaption (g, "MID / SIDE", "WIDTH " + juce::String (juce::roundToInt (width * 100.0f)) + "%");
    }

    // The Airwindows stereo module: the stereo field itself, a cloud of
    // points spreading from L to R (no parameter of its own to read).
    void paintStereoField (juce::Graphics& g)
    {
        const auto plot = plotArea();
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (plot.getCentreX(), plot.getY()));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getCentreY()));
        paintTick (g, "L", plot.withWidth (12.0f).withHeight (10.0f), juce::Justification::centredLeft, 0.8f);
        paintTick (g, "R", plot.withTrimmedLeft (plot.getWidth() - 12.0f).withHeight (10.0f), juce::Justification::centredRight, 0.8f);
        g.setColour (colour);
        for (int i = 0; i < 90; ++i)
        {
            const auto a = hash01 (i) * 2.0f * pi, r = std::sqrt (hash01 (i + 200));
            const auto x = plot.getCentreX() + std::cos (a) * r * plot.getWidth() * 0.42f;
            const auto y = plot.getCentreY() - std::sin (a) * r * plot.getHeight() * 0.42f;
            g.setColour (colour.withAlpha (0.35f + 0.5f * (1.0f - r)));
            g.fillEllipse (juce::Rectangle<float> (2.5f, 2.5f).withCentre ({ x, y }));
        }
        paintCaption (g, "STEREO FIELD");
    }

    void paintFlanger (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto rate = juce::jlimit (0.05f, 8.0f, param ("fx_flanger_rate")), depth = juce::jlimit (0.0f, 1.0f, param ("fx_flanger_depth"));
        const auto feedback = param ("fx_flanger_feedback");
        paintSweep (g, plot, rate, 2.5f, 2.5f * depth * 0.8f, 5.0f, { 0.0f }, juce::jmax (0.5f, 2.0f / rate));
        paintCaption (g, "DELAY SWEEP", "2.5 ms " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb1 ")) + juce::String (2.0f * depth, 1) + " ms " + juce::String::fromUTF8 ("\xc2\xb7") + " FB " + juce::String (juce::roundToInt (feedback * 100.0f)) + "%");
    }

    void paintDimension (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto rate = juce::jlimit (0.05f, 4.0f, param ("fx_dim_rate")), depth = juce::jlimit (0.0f, 1.0f, param ("fx_dim_depth"));
        // The two ears: 12 ms and 19 ms, swept against each other.
        paintSweep (g, plot, rate, 12.0f, 12.0f * 0.25f * depth, 25.0f, { 0.0f }, juce::jmax (0.5f, 2.0f / rate));
        paintSweep (g, plot, rate, 19.0f, 19.0f * 0.25f * depth, 25.0f, { 2.1f }, juce::jmax (0.5f, 2.0f / rate));
        paintCaption (g, "L / R DELAY SWEEP", juce::String (rate, 2) + " Hz");
    }

    void paintEnsemble (juce::Graphics& g)
    {
        const auto plot = plotArea();
        paintSweep (g, plot, 1.0f, 7.0f, 4.0f, 20.0f, { 0.0f, 2.1f, 4.2f }, 2.0f);
        paintCaption (g, "VOICE SWEEPS");
    }

    void paintTremolo (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto rate = juce::jlimit (0.05f, 20.0f, param ("fx_trem_rate")), depth = juce::jlimit (0.0f, 1.0f, param ("fx_trem_depth"));
        const auto shape = (int) param ("fx_trem_shape");
        const auto seconds = juce::jmax (0.5f, 2.0f / rate);
        const auto lfo = [&] (float t)
        {
            const auto phase = t * rate - std::floor (t * rate);
            switch (shape)
            {
                case 1:  return std::abs (2.0f * phase - 1.0f);
                case 2:  return phase;
                case 3:  return 1.0f - phase;
                case 4:  return phase < 0.5f ? 1.0f : 0.0f;
                case 5:  return hash01 ((int) std::floor (t * rate * 4.0f));
                default: return 0.5f + 0.5f * std::sin (2.0f * pi * phase);
            }
        };
        // (1 at the top of the LFO, 0 at the bottom; the gain dips by DEPTH.)
        const auto gainAt = [&] (float t) { return 1.0f - depth * (1.0f - juce::jlimit (0.0f, 1.0f, lfo (t))); };

        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getCentreY()));
        const auto steps = juce::jmax (64, (int) plot.getWidth());
        const auto carrier = 5.0f * juce::jmax (4.0f, seconds * rate * 2.0f);
        juce::Path envelope;
        for (int i = 0; i <= steps; ++i)
        {
            const auto t = seconds * (float) i / (float) steps;
            const auto x = plot.getX() + plot.getWidth() * (float) i / (float) steps;
            const auto amplitude = gainAt (t) * 0.5f * (plot.getHeight() - 2.0f);
            g.setColour (colour.withAlpha (0.35f));
            const auto wave = std::sin (2.0f * pi * carrier * (float) i / (float) steps);
            g.drawLine (x, plot.getCentreY(), x, plot.getCentreY() - wave * amplitude, 1.0f);
            const juce::Point<float> top (x, plot.getCentreY() - amplitude);
            if (i == 0)
                envelope.startNewSubPath (top);
            else
                envelope.lineTo (top);
        }
        g.setColour (colour);
        g.strokePath (envelope, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        paintCaption (g, "LEVEL OVER TIME", juce::String (rate, 1) + " Hz " + juce::String::fromUTF8 ("\xc2\xb7") + " " + choiceText ("fx_trem_shape").toUpperCase());
    }

    void paintFreqShift (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto shift = param ("fx_shifter_shift");
        constexpr float top = 3000.0f;
        const auto toX = [&] (float hz) { return plot.getX() + 4.0f + hz / top * (plot.getWidth() - 8.0f); };
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto hz : { 1000.0f, 2000.0f })
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (toX (hz), plot.getY()));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getBottom() - 1.0f));
        if (plot.getHeight() > 30.0f)
            for (const auto& tick : { std::pair<float, const char*> { 1000.0f, "1 kHz" }, { 2000.0f, "2 kHz" } })
                paintTick (g, tick.second, juce::Rectangle<float> (toX (tick.first) + 2.0f, plot.getBottom() - 11.0f, 34.0f, 10.0f), juce::Justification::centredLeft);
        // A harmonic series, and the same lines moved by the same number of Hz (no longer in tune with each other).
        for (int k = 1; k <= 12; ++k)
        {
            const auto level = 1.0f / (float) std::pow ((float) k, 0.7f);
            const auto bar = [&] (float hz, juce::Colour c)
            {
                const auto f = std::abs (hz);
                if (f > top)
                    return;
                g.setColour (c);
                g.fillRect (juce::Rectangle<float> (2.0f, (plot.getHeight() - 14.0f) * level).withBottomY (plot.getBottom() - 1.0f).withX (toX (f) - 1.0f));
            };
            bar ((float) k * 220.0f, IlanaTheme::Ui::text3.withAlpha (0.5f));
            bar ((float) k * 220.0f + shift, colour);
        }
        paintCaption (g, "PARTIALS (GREY) MOVED", signedText (shift, 0, " Hz"));
    }

    void paintRingMod (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto carrier = param ("fx_ring_freq");
        constexpr double low = 20.0, high = 20000.0;
        paintFrequencyGrid (g, plot, low, high);
        if (plot.getHeight() > 30.0f)
            for (const auto& tick : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
                paintTick (g, tick.second, juce::Rectangle<float> (logX (plot, tick.first, low, high) + 2.0f, plot.getBottom() - 10.0f, 24.0f, 10.0f), juce::Justification::centredLeft);
        const auto bar = [&] (double hz, juce::Colour c, float level)
        {
            if (hz < low || hz > high)
                return;
            g.setColour (c);
            g.fillRect (juce::Rectangle<float> (2.0f, (plot.getHeight() - 14.0f) * level).withBottomY (plot.getBottom() - 11.0f).withX (logX (plot, hz, low, high) - 1.0f));
        };
        const float dashes[] { 3.0f, 3.0f };
        g.setColour (IlanaTheme::Ui::text3.withAlpha (0.7f));
        g.drawDashedLine ({ logX (plot, carrier, low, high), plot.getY(), logX (plot, carrier, low, high), plot.getBottom() - 11.0f }, dashes, 2, 1.0f);
        bar (440.0, IlanaTheme::Ui::text3.withAlpha (0.6f), 1.0f);
        bar (std::abs (440.0 - (double) carrier), colour, 0.8f);
        bar (440.0 + (double) carrier, colour, 0.8f);
        paintTick (g, "IN", juce::Rectangle<float> (logX (plot, 440.0, low, high) + 3.0f, plot.getY() + 2.0f, 20.0f, 10.0f), juce::Justification::centredLeft, 0.9f);
        {
            const auto x = logX (plot, carrier, low, high);
            if (x - plot.getX() > 56.0f)
                paintTick (g, "CARRIER", juce::Rectangle<float> (x - 50.0f, plot.getY() + 2.0f, 46.0f, 10.0f), juce::Justification::centredRight, 0.9f);
            else
                paintTick (g, "CARRIER", juce::Rectangle<float> (x + 3.0f, plot.getY() + 12.0f, 46.0f, 10.0f), juce::Justification::centredLeft, 0.9f);
        }
        paintCaption (g, "SIDEBANDS OF A 440 Hz NOTE", juce::String (juce::roundToInt (carrier)) + " Hz");
    }

    void paintOctaver (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto mix = param ("fx_octaver_mix");
        const auto upper = plot.withHeight (plot.getHeight() * 0.5f - 1.0f), lower = plot.withTrimmedTop (plot.getHeight() * 0.5f + 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getCentreY()));
        const auto draw = [&] (juce::Rectangle<float> lane, float cycles, float amount, juce::Colour c)
        {
            juce::Path wave;
            const auto steps = juce::jmax (32, (int) lane.getWidth());
            for (int i = 0; i <= steps; ++i)
            {
                const auto t = (float) i / (float) steps;
                const juce::Point<float> point (lane.getX() + t * lane.getWidth(), lane.getCentreY() - std::sin (2.0f * pi * cycles * t) * amount * lane.getHeight() * 0.45f);
                if (i == 0)
                    wave.startNewSubPath (point);
                else
                    wave.lineTo (point);
            }
            g.setColour (c);
            g.strokePath (wave, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        draw (upper, 5.0f, 1.0f, IlanaTheme::Ui::text3.withAlpha (0.8f));
        draw (lower, 2.5f, 0.35f + 0.65f * mix, colour);
        paintLaneLabel (g, "IN", upper);
        paintLaneLabel (g, juce::String::fromUTF8 ("\xe2\x88\x92") + "1 OCT", lower.translated (0.0f, 0.0f));
        paintCaption (g, "ONE OCTAVE DOWN", juce::String (juce::roundToInt (mix * 100.0f)) + "% MIX");
    }

    void paintFeedbackLoop (juce::Graphics& g)
    {
        const auto plot = plotArea();
        const auto amount = param ("fx_feedback_amount"), delay = juce::jmax (1.0f, param ("fx_feedback_delay"));
        constexpr float window = 250.0f;
        const auto toX = [&] (float ms) { return plot.getX() + 6.0f + ms / window * (plot.getWidth() - 12.0f); };
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (const auto ms : { 50.0f, 100.0f, 150.0f, 200.0f })
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (toX (ms), plot.getY()));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), plot.getBottom() - 1.0f));
        if (plot.getHeight() > 30.0f)
            for (const auto ms : { 100.0f, 200.0f })
                paintTick (g, juce::String ((int) ms) + " ms", juce::Rectangle<float> (toX (ms) + 2.0f, plot.getBottom() - 11.0f, 34.0f, 10.0f), juce::Justification::centredLeft);
        // Each trip round the loop comes back AMOUNT as loud.
        auto level = 1.0f;
        for (int i = 0; i < 80 && toX ((float) i * delay) < plot.getRight(); ++i)
        {
            g.setColour (colour.withAlpha (i == 0 ? 0.5f : 1.0f));
            const auto h = juce::jmax (1.0f, (plot.getHeight() - 14.0f) * level);
            g.fillRect (juce::Rectangle<float> (2.0f, h).withBottomY (plot.getBottom() - 1.0f).withX (toX ((float) i * delay) - 1.0f));
            level *= amount;
        }
        paintCaption (g, "ROUND THE LOOP", juce::String (juce::roundToInt (amount * 100.0f)) + "% " + juce::String::fromUTF8 ("\xc2\xb7") + " " + juce::String (juce::roundToInt (delay)) + " ms");
    }

    void paintPicture (juce::Graphics& g)
    {
        switch (type)
        {
            case 8:  paintHaas (g); break;
            case 10: paintStutter (g); break;
            case 11: paintSmear (g); break;
            case 14: paintFlanger (g); break;
            case 15: paintDimension (g); break;
            case 17: paintTapeStop (g); break;
            case 18: paintTilt (g); break;
            case 19: paintUtility (g); break;
            case 22: paintWidener (g); break;
            case 23: paintTremolo (g); break;
            case 24: paintFreqShift (g); break;
            case 25: paintRingMod (g); break;
            case 26: paintOctaver (g); break;
            case 28: paintFeedbackLoop (g); break;
            case 36: paintEnsemble (g); break;
            case 41: paintStereoField (g); break;
            default: break;
        }
    }

    // The settings each picture is drawn from (for the poll's signature).
    static std::vector<juce::String> pictureIds (int fxType)
    {
        switch (fxType)
        {
            case 8:  return { "fx_haas_delay", "fx_haas_mix" };
            case 10: return { "fx_stutter_div", "fx_stutter_reverse", "fx_stutter_pitch" };
            case 11: return { "fx_smear_size", "fx_smear_density", "fx_smear_mix" };
            case 14: return { "fx_flanger_rate", "fx_flanger_depth", "fx_flanger_feedback" };
            case 15: return { "fx_dim_rate", "fx_dim_depth" };
            case 17: return { "fx_tape_stop_time", "fx_tape_stop_mix", "fx_tape_stop_trigger" };
            case 18: return { "fx_tilt", "fx_tilt_level" };
            case 19: return { "fx_util_gain", "fx_util_mono", "fx_util_invert" };
            case 22: return { "fx_width", "fx_width_mix" };
            case 23: return { "fx_trem_rate", "fx_trem_depth", "fx_trem_shape" };
            case 24: return { "fx_shifter_shift" };
            case 25: return { "fx_ring_freq" };
            case 26: return { "fx_octaver_mix" };
            case 28: return { "fx_feedback_amount", "fx_feedback_delay" };
            default: return {};
        }
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
                                                 "fx_drive_mix", "fx_fold", "fx_drive_type", "fx_fold_type", "fx_crush_bits", "fx_crush_mix", "fx_crush_down" };
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
            case Kind::airwindowsResponse:
            case Kind::airwindowsTransfer:
                if (juce::String prefix; airwindowsSource (prefix) >= 0)
                {
                    mixIn (param (prefix + "_algo"));
                    mixIn (param (prefix + "_mix"));
                    for (int k = 1; k <= airwindows::Module::numKnobs; ++k)
                        mixIn (param (prefix + "_p" + juce::String (k)));
                }
                break;
            case Kind::freeze: mixIn (param ("fx_freeze_on")); break;
            case Kind::picture: for (const auto& id : pictureIds (type)) mixIn (param (id)); break;
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
            if (kind == Kind::airwindowsResponse)
                measureResponse();
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

        // FREEZE's spectrum, held while HOLD is on: bars that rise toward the
        // new level and fall away.
        if (kind == Kind::freeze)
        {
            std::array<float, SpectralFreeze::numBands> now {};
            processorRef.getFreezeBands (now);
            auto moving = false;
            for (size_t b = 0; b < freezeBands.size(); ++b)
            {
                // 0 to 1 over 80 dB below the full-scale magnitude of a window of this size.
                const auto target = now[b] > 1.0e-6f ? juce::jlimit (0.0f, 1.0f, 1.0f + 20.0f * std::log10 (now[b] / 512.0f) / 80.0f) : 0.0f;
                const auto next = smooth ? freezeBands[b] + (target - freezeBands[b]) * 0.5f : target;
                moving = moving || std::abs (next - freezeBands[b]) > 2.0e-3f;
                freezeBands[b] = next;
            }
            dirty = dirty || moving;
        }

        if (kind == Kind::transfer || kind == Kind::dynamics || (kind == Kind::airwindowsTransfer && ! measured.empty()))
            dirty = updateInputLevel (smooth) || dirty;

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
