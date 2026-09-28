#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

#include "../PluginProcessor.h"
#include "../dsp/Modulation.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"

namespace XtraDisplayHelpers
{
inline void drawWells (juce::Graphics& g, juce::Rectangle<float> bounds, float appear)
{
    g.setOpacity (juce::jlimit (0.0f, 1.0f, appear));
    g.addTransform (juce::AffineTransform::translation (0.0f, (1.0f - juce::jlimit (0.0f, 1.0f, appear)) * 10.0f));

    IlanaTheme::paintWell (g, bounds, 6.0f);
}

inline float readParam (IlanaSynthAudioProcessor& processor, const char* id)
{
    if (const auto* value = processor.apvts.getRawParameterValue (id))
        return value->load();

    return 0.0f;
}
} // namespace XtraDisplayHelpers

class VoiceDisplay : public juce::Component,
                     public IlanaAnim::PageAnimated,
                     private juce::Timer
{
public:
    explicit VoiceDisplay (IlanaSynthAudioProcessor& processor)
        : processorRef (processor)
    {
        startTimerHz (30);
    }

    void replayAppear() override { appear = 0.0f; }

    void paint (juce::Graphics& g) override
    {
        using namespace XtraDisplayHelpers;
        const auto bounds = getLocalBounds().toFloat();
        drawWells (g, bounds, appear);

        const auto plot = bounds.reduced (12.0f, 14.0f);
        const auto laneHeight = plot.getHeight() / 4.0f;

        const char* const prefixes[] { "osc1", "osc2", "sub" };
        const juce::Colour laneColours[] { IlanaTheme::accent(), juce::Colour (0xff5b8cff), juce::Colour (0xffffd447) };

        for (int lane = 0; lane < 3; ++lane)
        {
            auto laneArea = plot.withHeight (laneHeight).translated (0.0f, laneHeight * (float) lane);
            const auto y = laneArea.getCentreY();
            const auto prefix = juce::String (prefixes[lane]);

            const auto unison = juce::jlimit (1, 8, (int) readParam (processorRef, (prefix + "_unison").toRawUTF8()));
            const auto spread = readParam (processorRef, (prefix + "_spread").toRawUTF8());
            const auto detune = readParam (processorRef, (prefix + "_detune").toRawUTF8());
            const auto spreadWidth = laneArea.getWidth() * 0.34f * (0.2f + 0.8f * spread);

            // Lane card with a tinted edge.
            g.setColour (juce::Colours::white.withAlpha (0.03f));
            g.fillRoundedRectangle (laneArea.reduced (2.0f, 1.0f), 5.0f);
            g.setColour (laneColours[lane].withAlpha (0.12f));
            g.fillRect (juce::Rectangle<float> (2.0f, laneArea.getHeight() - 8.0f)
                            .withX (laneArea.getX() + 2.0f)
                            .withY (laneArea.getY() + 4.0f));

            const auto header = laneArea.removeFromTop (14.0f);

            g.setColour (laneColours[lane].withAlpha (0.55f));
            g.setFont (IlanaTheme::font (11.0f, true));
            g.drawText ("OSC " + juce::String (lane + 1), header.toNearestInt(),
                        juce::Justification::centredLeft);

            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.setFont (IlanaTheme::font (10.5f));
            g.drawText (juce::String (unison) + (unison == 1 ? " VOICE   -   " : " VOICES   -   ") + juce::String (juce::roundToInt (detune)) + " ct   -   "
                            + juce::String (juce::roundToInt (spread * 100.0f)) + "% WIDTH",
                        header.toNearestInt(), juce::Justification::centredRight);

            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRect (juce::Rectangle<float> (laneArea.getWidth(), 1.0f).withCentre ({ laneArea.getCentreX(), y }));

            // Spread bracket.
            g.setColour (laneColours[lane].withAlpha (0.18f));
            g.fillRect (juce::Rectangle<float> (spreadWidth * 2.0f, 1.5f)
                            .withCentre ({ plot.getCentreX(), y + 9.0f }));

            for (int u = 0; u < unison; ++u)
            {
                const auto offset = unison > 1 ? ((float) u / (float) (unison - 1) * 2.0f - 1.0f) : 0.0f;
                const auto x = plot.getCentreX() + offset * spreadWidth;
                const auto size = 4.0f + juce::jmin (6.0f, detune * 0.2f);
                const juce::Point<float> dot (x, y);

                g.setColour (laneColours[lane].withAlpha (0.16f));
                g.fillEllipse (juce::Rectangle<float> (size + 6.0f, size + 6.0f).withCentre (dot));
                g.setColour (laneColours[lane].withAlpha (0.45f + 0.5f * (1.0f - std::abs (offset))));
                g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (dot));
            }
        }

        // Global row: voice spread width + live voice count.
        const auto globalY = plot.getBottom() - 16.0f;
        const auto voiceSpread = readParam (processorRef, "voice_spread");
        auto widthBar = juce::Rectangle<float> (plot.getX(), globalY - 3.0f, plot.getWidth() * 0.62f, 6.0f);

        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRoundedRectangle (widthBar, 3.0f);
        g.setColour (IlanaTheme::accent().withAlpha (0.8f));
        g.fillRoundedRectangle (widthBar.withWidth (widthBar.getWidth() * juce::jlimit (0.0f, 1.0f, voiceSpread)), 3.0f);

        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText ("VOICE SPREAD   -   " + juce::String (processorRef.getActiveVoiceCount()) + " / 16 VOICES",
                    widthBar.withY (globalY - 18.0f).withWidth (220.0f).toNearestInt(),
                    juce::Justification::centredLeft);
    }

private:
    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f);
        repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    float appear = 1.0f;
};

class CrossModDisplay : public juce::Component,
                        public IlanaAnim::PageAnimated,
                        private juce::Timer
{
public:
    CrossModDisplay (IlanaSynthAudioProcessor& processor, juce::Colour unitColourIn)
        : processorRef (processor),
          unitColour (unitColourIn)
    {
        startTimerHz (30);
    }

    void replayAppear() override { appear = 0.0f; }

    void paint (juce::Graphics& g) override
    {
        using namespace XtraDisplayHelpers;
        const auto bounds = getLocalBounds().toFloat();
        drawWells (g, bounds, appear);

        const auto plot = bounds.reduced (16.0f, 16.0f);
        const auto fm = readParam (processorRef, "fm_amount");
        const auto feedback = readParam (processorRef, "fm_feedback");
        const auto ring = readParam (processorRef, "ring_mod");
        const auto sync = readParam (processorRef, "hard_sync") > 0.5f;
        const auto drift = readParam (processorRef, "drift");

        const auto nodeSize = 54.0f;
        const auto osc1Centre = juce::Point<float> (plot.getCentreX() - 110.0f, plot.getCentreY());
        const auto osc2Centre = juce::Point<float> (plot.getRight() - 40.0f - nodeSize * 0.5f, plot.getCentreY());

        // Ring mod ring around OSC 1.
        const auto ringRadius = nodeSize * 0.5f + 18.0f;
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawEllipse (juce::Rectangle<float> (ringRadius * 2.0f, ringRadius * 2.0f).withCentre (osc1Centre), 1.0f);

        if (ring > 0.001f)
        {
            g.setColour (juce::Colour (0xffe3a56f).withAlpha (0.35f + 0.5f * ring));
            g.drawEllipse (juce::Rectangle<float> (ringRadius * 2.0f, ringRadius * 2.0f).withCentre (osc1Centre),
                           1.5f + ring * 4.0f);
        }

        const auto smallReadout = [&g] (const juce::String& text, juce::Rectangle<float> area, juce::Colour colour)
        {
            g.setColour (colour.withAlpha (0.5f));
            g.setFont (IlanaTheme::font (10.5f, true));
            g.drawText (text, area.toNearestInt(), juce::Justification::centred);
        };

        // FM connection: thickness and glow follow amount.
        const auto lineWidth = 1.5f + fm * 10.0f;
        const auto lineX1 = osc2Centre.x - nodeSize * 0.5f;
        const auto lineX2 = osc1Centre.x + nodeSize * 0.5f;

        g.setColour (juce::Colour (0xff5b8cff).withAlpha (0.1f + 0.3f * fm));
        g.drawLine (lineX1, osc2Centre.y, lineX2, osc2Centre.y, lineWidth + 5.0f);

        g.setColour (juce::Colour (0xff5b8cff).withAlpha (0.25f + 0.6f * fm));
        g.drawLine (lineX1, osc2Centre.y, lineX2, osc2Centre.y, lineWidth);

        // Dashed energy flow along the FM line.
        if (fm > 0.01f)
        {
            const auto phase = std::fmod ((float) juce::Time::getMillisecondCounterHiRes() * 0.0006f, 1.0f);

            for (int dash = 0; dash < 6; ++dash)
            {
                const auto t = std::fmod (phase + (float) dash / 6.0f, 1.0f);
                const auto x = juce::jmap (t, lineX1, lineX2);
                const auto alpha = (0.2f + 0.7f * std::sin (t * juce::MathConstants<float>::pi))
                                   * juce::jmin (1.0f, fm * 2.0f);

                g.setColour (juce::Colours::white.withAlpha (alpha));
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ x, osc2Centre.y }));
            }
        }

        // Self-feedback loop around OSC 1.
        if (feedback > 0.001f)
        {
            juce::Path loop;
            loop.addCentredArc (osc1Centre.x, osc1Centre.y - 4.0f, nodeSize * 0.7f, nodeSize * 0.7f, 0.0f,
                                juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 1.8f, true);
            g.setColour (unitColour.withAlpha (0.3f + 0.6f * feedback));
            g.strokePath (loop, juce::PathStrokeType (1.5f + feedback * 3.0f));
        }

        drawNode (g, osc2Centre, nodeSize, juce::Colour (0xff5b8cff), "OSC 2", sync);
        drawNode (g, osc1Centre, nodeSize, IlanaTheme::accent(), "OSC 1", false);

        smallReadout ("FM " + juce::String (juce::roundToInt (fm * 100.0f)) + "%",
                      { lineX1, osc2Centre.y + nodeSize * 0.5f + 2.0f, lineX2 - lineX1, 12.0f },
                      juce::Colour (0xff5b8cff));
        smallReadout ("RING " + juce::String (juce::roundToInt (ring * 100.0f)) + "%",
                      { osc1Centre.x - ringRadius, osc1Centre.y + ringRadius + 2.0f, ringRadius * 2.0f, 12.0f },
                      juce::Colour (0xffe3a56f));
        smallReadout ("FEEDBACK " + juce::String (juce::roundToInt (feedback * 100.0f)) + "%",
                      { osc1Centre.x - ringRadius, osc1Centre.y - ringRadius - 14.0f, ringRadius * 2.0f, 12.0f },
                      unitColour);

        // Drift: a wandering pitch line along the bottom.
        auto driftArea = plot;
        driftArea = driftArea.removeFromBottom (34.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (driftArea, 4.0f);

        juce::Path driftPath;
        const auto time = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;

        for (int x = 0; x <= (int) driftArea.getWidth(); ++x)
        {
            const auto t = (float) x / juce::jmax (1.0f, driftArea.getWidth());
            const auto wobble = std::sin (t * 9.0f + time * 1.7f) * 0.5f + std::sin (t * 23.0f + time * 0.9f) * 0.25f;
            const auto y = driftArea.getCentreY() - wobble * drift * driftArea.getHeight() * 0.42f;

            if (x == 0)
                driftPath.startNewSubPath (driftArea.getX(), y);
            else
                driftPath.lineTo (driftArea.getX() + (float) x, y);
        }

        g.setColour (unitColour.withAlpha (0.4f + 0.5f * juce::jmin (1.0f, drift * 2.0f)));
        g.strokePath (driftPath, juce::PathStrokeType (1.4f));

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText ("DRIFT " + juce::String (juce::roundToInt (drift * 100.0f)) + "%",
                    driftArea.withX (driftArea.getX() + 6.0f).toNearestInt(), juce::Justification::centredLeft);
    }

private:
    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f);
        repaint();
    }

    static void drawNode (juce::Graphics& g, juce::Point<float> centre, float size, juce::Colour colour,
                          const juce::String& label, bool syncPulse)
    {
        juce::ColourGradient nodeFill (colour.withAlpha (0.35f), centre.x - size * 0.3f, centre.y - size * 0.3f,
                                       colour.withAlpha (0.08f), centre.x + size * 0.3f, centre.y + size * 0.3f, true);
        g.setGradientFill (nodeFill);
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (centre));

        g.setColour (colour.withAlpha (0.9f));
        g.drawEllipse (juce::Rectangle<float> (size, size).withCentre (centre), 1.6f);

        g.setColour (colour.withAlpha (0.12f));
        g.drawEllipse (juce::Rectangle<float> (size + 6.0f, size + 6.0f).withCentre (centre), 1.0f);

        if (syncPulse)
        {
            const auto phase = std::fmod ((float) juce::Time::getMillisecondCounterHiRes() * 0.001f, 1.0f);
            g.setColour (colour.withAlpha (1.0f - phase));
            g.drawEllipse (juce::Rectangle<float> (size, size).withCentre (centre).expanded (phase * 14.0f), 1.5f);
        }

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (IlanaTheme::font (11.5f, true));
        g.drawText (label, juce::Rectangle<float> (size, size).withCentre (centre).toNearestInt(),
                    juce::Justification::centred);
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::Colour unitColour;
    float appear = 1.0f;
};

class ArpDisplay : public juce::Component,
                   public IlanaAnim::PageAnimated,
                   private juce::Timer
{
public:
    ArpDisplay (IlanaSynthAudioProcessor& processor, juce::Colour unitColourIn)
        : processorRef (processor),
          unitColour (unitColourIn)
    {
        startTimerHz (30);
    }

    void replayAppear() override { appear = 0.0f; }

    void paint (juce::Graphics& g) override
    {
        using namespace XtraDisplayHelpers;
        const auto bounds = getLocalBounds().toFloat();
        drawWells (g, bounds, appear);

        const auto plot = bounds.reduced (12.0f, 14.0f);
        const auto on = readParam (processorRef, "arp_on") > 0.5f;
        const auto mode = (int) readParam (processorRef, "arp_mode");
        const auto octaves = juce::jlimit (1, 4, (int) readParam (processorRef, "arp_octaves"));
        const auto gate = readParam (processorRef, "arp_gate");

        constexpr int steps = 16;
        const auto stepWidth = plot.getWidth() / (float) steps;
        const auto centreY = plot.getCentreY();
        const auto halfHeight = plot.getHeight() * 0.38f;

        const auto activeStep = juce::jlimit (0, steps - 1, (int) (arpPhase * (float) steps));

        // Octave lanes.
        const auto laneCount = juce::jlimit (1, 4, octaves);

        for (int oct = 0; oct < laneCount; ++oct)
        {
            const auto y = centreY + halfHeight - (float) oct * 0.18f * halfHeight * 0.5f;

            g.setColour (juce::Colours::white.withAlpha (oct == 0 ? 0.08f : 0.05f));
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withX (plot.getX()).withCentre ({ plot.getCentreX(), y }));

            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.setFont (IlanaTheme::font (9.5f));
            g.drawText ("OCT " + juce::String (oct + 1),
                        juce::Rectangle<float> (plot.getRight() - 40.0f, y - 11.0f, 40.0f, 10.0f).toNearestInt(),
                        juce::Justification::centredRight);
        }

        for (int step = 0; step < steps; ++step)
        {
            const auto x = plot.getX() + (float) step * stepWidth;
            const auto cell = juce::Rectangle<float> (x + 1.5f, plot.getY(), stepWidth - 3.0f, plot.getHeight());

            g.setColour (juce::Colours::white.withAlpha (step == activeStep && on ? 0.12f : 0.035f));
            g.fillRoundedRectangle (cell, 3.0f);

            if (step % 4 == 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.08f));
                g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withX (x));
            }

            // Pattern shape from the mode.
            auto position = 0.0f;
            const auto last = (float) (steps - 1);

            switch (mode)
            {
                case 0: position = (float) step / last; break;                                // Up
                case 1: position = 1.0f - (float) step / last; break;                         // Down
                case 2:                                                                       // UpDown
                    position = (float) (step < steps / 2 ? step : steps - 1 - step) / (float) (steps / 2 - 1);
                    break;
                case 4:                                                                       // DownUp
                    position = 1.0f - (float) (step < steps / 2 ? step : steps - 1 - step) / (float) (steps / 2 - 1);
                    break;
                case 5:                                                                       // Converge
                    position = (step % 2 == 0 ? (float) (step / 2) : (float) (steps - 1 - step / 2)) / last;
                    break;
                case 6:                                                                       // Walk
                    position = (float) ((step * step * 3 + step * 5) % steps) / last;
                    break;
                case 3: position = (float) ((step * 7) % steps) / last; break;                // Random
                default:                                                                      // Chord
                    position = step % 3 == 0 ? 1.0f : (step % 3 == 1 ? 0.72f : 0.46f);
                    break;
            }

            position = juce::jlimit (0.0f, 1.0f, position);

            const auto octaveOffset = (float) ((step / juce::jmax (1, steps / octaves)) % octaves) * 0.18f;
            const auto y = centreY + halfHeight - juce::jlimit (0.0f, 1.0f, position) * halfHeight * 0.8f
                           - octaveOffset * halfHeight * 0.5f;
            const auto barHeight = juce::jmax (3.0f, halfHeight * 0.8f * gate);

            if (step == activeStep && on)
            {
                g.setColour (unitColour.withAlpha (0.2f));
                g.fillRoundedRectangle (juce::Rectangle<float> (x + 1.5f, y - barHeight * 0.5f - 3.0f,
                                                                stepWidth - 3.0f, barHeight + 6.0f),
                                        3.0f);
            }

            juce::ColourGradient barGradient (unitColour.withAlpha (step == activeStep && on ? 1.0f : 0.55f),
                                              x + stepWidth * 0.5f, y - barHeight * 0.5f,
                                              unitColour.withAlpha (step == activeStep && on ? 0.45f : 0.16f),
                                              x + stepWidth * 0.5f, y + barHeight * 0.5f, false);
            g.setGradientFill (barGradient);
            g.fillRoundedRectangle (juce::Rectangle<float> (x + 3.0f, y - barHeight * 0.5f, stepWidth - 6.0f, barHeight),
                                    2.0f);
        }

        if (on)
        {
            const auto x = plot.getX() + (float) activeStep * stepWidth;
            g.setColour (unitColour.withAlpha (0.9f));
            g.fillRect (juce::Rectangle<float> (2.0f, plot.getHeight()).withX (x));

            g.setColour (unitColour.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (stepWidth, plot.getHeight()).withX (x));
        }

        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.setFont (IlanaTheme::font (9.5f));

        for (int step = 0; step < steps; step += 4)
            g.drawText (juce::String (step + 1),
                        juce::Rectangle<float> (plot.getX() + (float) step * stepWidth + 2.0f,
                                                plot.getBottom() - 11.0f, stepWidth, 10.0f).toNearestInt(),
                        juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText (on ? "RUNNING   -   GATE " + juce::String (juce::roundToInt (gate * 100.0f)) + "%"
                       : "ARP OFF",
                    plot.withHeight (14.0f).toNearestInt(),
                    juce::Justification::topRight);
    }

private:
    void timerCallback() override
    {
        const auto on = XtraDisplayHelpers::readParam (processorRef, "arp_on") > 0.5f;
        const auto rate = on ? processorRef.getArpStepRateHz() : 0.0f;

        // One bar per arp step.
        arpPhase += rate / (30.0f * (float) numSteps);
        arpPhase -= std::floor (arpPhase);

        appear = juce::jmin (1.0f, appear + 0.12f);
        repaint();
    }

    static constexpr int numSteps = 16;

    IlanaSynthAudioProcessor& processorRef;
    float arpPhase = 0.0f;
    juce::Colour unitColour;
    float appear = 1.0f;
};

class ResonatorDisplay : public juce::Component,
                         public IlanaAnim::PageAnimated,
                         private juce::Timer
{
public:
    ResonatorDisplay (IlanaSynthAudioProcessor& processor, juce::Colour unitColourIn)
        : processorRef (processor),
          unitColour (unitColourIn)
    {
        startTimerHz (30);
    }

    void replayAppear() override { appear = 0.0f; }

    void paint (juce::Graphics& g) override
    {
        using namespace XtraDisplayHelpers;
        const auto bounds = getLocalBounds().toFloat();
        drawWells (g, bounds, appear);

        const auto plot = bounds.reduced (12.0f, 14.0f);
        const auto on = readParam (processorRef, "res_on") > 0.5f;
        const auto amount = readParam (processorRef, "res_amount");
        const auto decay = readParam (processorRef, "res_decay");
        const auto offset = readParam (processorRef, "res_offset");

        // Log-frequency grid, same mapping as the filter display.
        g.setColour (juce::Colours::white.withAlpha (0.07f));

        for (const auto frequency : { 100.0, 1000.0, 10000.0 })
        {
            const auto x = plot.getX()
                           + (float) (std::log (frequency / 20.0) / std::log (1000.0)) * plot.getWidth();
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withX (x));
        }

        // Resonant peaks: harmonic series of the tuned comb.
        constexpr auto noteHz = 220.0;
        const auto f0 = noteHz * std::pow (2.0, (double) offset / 12.0);

        // Harmonic ticks along the bottom.
        for (int harmonic = 1; harmonic <= 12; ++harmonic)
        {
            const auto peak = f0 * (double) harmonic;

            if (peak > 20000.0)
                break;

            const auto x = plot.getX()
                           + (float) (std::log (peak / 20.0) / std::log (1000.0)) * plot.getWidth();

            g.setColour (unitColour.withAlpha (harmonic == 1 ? 0.55f : 0.22f));
            g.fillRect (juce::Rectangle<float> (1.0f, harmonic == 1 ? 9.0f : 5.0f)
                            .withX (x)
                            .withBottom (plot.getBottom()));
        }

        g.setColour (juce::Colours::white.withAlpha (0.42f));
        g.setFont (IlanaTheme::font (10.5f));
        g.drawText ("AMOUNT " + juce::String (juce::roundToInt (amount * 100.0f)) + "%   DECAY "
                        + juce::String (juce::roundToInt (decay * 100.0f)) + "%   OFFSET "
                        + juce::String (juce::roundToInt (offset)) + " st",
                    plot.withHeight (14.0f).toNearestInt(), juce::Justification::topLeft);

        g.setColour (unitColour.withAlpha (on ? 0.55f + 0.4f * amount : 0.15f));

        juce::Path response;

        const auto width = juce::jmax (2.0f, plot.getWidth());
        auto started = false;

        for (int x = 0; x <= (int) width; ++x)
        {
            const auto proportion = (float) x / width;
            const auto frequency = 20.0 * std::pow (1000.0, (double) proportion);
            auto magnitude = 0.0;

            for (int harmonic = 1; harmonic <= 24; ++harmonic)
            {
                const auto peak = f0 * (double) harmonic;
                const auto distance = std::log2 (frequency / peak);
                const auto peakWidth = 0.35 - decay * 0.28;
                magnitude += std::exp (-std::pow (distance / juce::jmax (0.03, peakWidth), 2.0)) / (double) harmonic;
            }

            magnitude = juce::jlimit (0.0, 1.0, magnitude * (0.25 + amount * 0.75));
            const auto y = plot.getBottom() - (float) magnitude * plot.getHeight() * (on ? 0.9f : 0.15f);

            if (! started)
            {
                response.startNewSubPath (plot.getX() + (float) x, y);
                started = true;
            }
            else
            {
                response.lineTo (plot.getX() + (float) x, y);
            }
        }

        juce::Path filled = response;
        filled.lineTo (plot.getRight(), plot.getBottom());
        filled.lineTo (plot.getX(), plot.getBottom());
        filled.closeSubPath();

        juce::ColourGradient fill (unitColour.withAlpha (on ? 0.4f : 0.08f), 0.0f, plot.getY(),
                                   unitColour.withAlpha (0.02f), 0.0f, plot.getBottom(), false);
        g.setGradientFill (fill);
        g.fillPath (filled);

        g.setColour (unitColour.withAlpha (on ? 0.2f : 0.06f));
        g.strokePath (response, juce::PathStrokeType (4.0f));

        g.setColour (unitColour.withAlpha (on ? 0.95f : 0.2f));
        g.strokePath (response, juce::PathStrokeType (1.6f));

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText (on ? ("TUNED " + juce::String (f0, 1) + " Hz") : "RESONATOR OFF",
                    plot.withHeight (14.0f).toNearestInt(), juce::Justification::topRight);
    }

private:
    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f);
        repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::Colour unitColour;
    float appear = 1.0f;
};
