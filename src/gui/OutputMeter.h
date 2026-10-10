#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// A stereo peak meter for the master output, named OUT: bars fall
// smoothly, a peak line holds for a moment, ticks mark 0, -12 and -24 dB, and
// the clip light beside the name lights red after a clip until clicked. The tooltip
// gives the loudest peak since the last click.
class OutputMeter : public juce::Component,
                    public juce::TooltipClient,
                    private IlanaAnim::FrameTimer
{
public:
    explicit OutputMeter (IlanaSynthAudioProcessor& processor) : processorRef (processor)
    {
        startTimerHz (30);
    }

    juce::String getTooltip() override
    {
        const auto db = juce::Decibels::gainToDecibels (loudest, -100.0f);
        return juce::String ("Output level (peak, ticks at 0, -12 and -24 dB).\n")
               + (loudest < 1.0e-5f ? juce::String ("Silent since the last reset.")
                                    : "Loudest peak: " + juce::String (db, 1) + " dB" + (clipped ? " (clipped)." : "."))
               + "\nA red light at the right of the meter shows if the output went over 0 dB; click to reset it.";
    }

    bool isClipLit() const { return clipped; }

    void paint (juce::Graphics& g) override
    {
        // The dock's meter (shell mockup): a 110 x 30 well holding two thin
        // bars, left over right, with the 0, -12 and -24 dB ticks across
        // them. The clip light is a red dot at the well's right end that
        // only shows after a clip; the loudest peak is in the tooltip.
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 8.0f);
        const auto inner = bounds.reduced (6.0f, 0.0f).withTrimmedRight (clipped ? 10.0f : 0.0f);
        const auto barHeight = 6.0f;
        const auto barsTop = bounds.getY() + 6.0f;

        if (clipped)
        {
            const auto light = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ bounds.getRight() - 9.0f, bounds.getCentreY() });
            g.setColour (juce::Colour (0xffff4f5e));
            g.fillEllipse (light);
            g.setColour (juce::Colour (0xffff4f5e).brighter (0.4f));
            g.drawEllipse (light.reduced (0.5f), 1.0f);
        }

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto bar = juce::Rectangle<float> (inner.getX(), barsTop + (float) channel * (barHeight + 4.0f),
                                                     inner.getWidth(), barHeight);
            // The empty track, so the scale reads at rest (A16-7).
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (bar, 3.0f);
            const auto level = proportion (levels[(size_t) channel]);
            const auto filled = bar.withWidth (bar.getWidth() * level);

            juce::ColourGradient gradient (juce::Colour (0xff4fd1a5), bar.getX(), bar.getY(),
                                           juce::Colour (0xffff4f5e), bar.getRight(), bar.getY(), false);
            gradient.addColour (proportion (juce::Decibels::decibelsToGain (-6.0f)), juce::Colour (0xffffd447));
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (filled, 3.0f);

            const auto hold = proportion (holds[(size_t) channel]);

            if (hold > 0.01f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.fillRect (bar.withX (bar.getX() + bar.getWidth() * hold - 1.0f).withWidth (1.0f));
            }
        }

        // dB ticks across both bars, named below (-24, -12, 0): the scale
        // shows at rest too.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        for (const auto db : { 0.0f, -12.0f, -24.0f })
        {
            const auto x = inner.getX() + inner.getWidth() * proportion (juce::Decibels::decibelsToGain (db));
            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.fillRect (juce::Rectangle<float> (x - 0.5f, barsTop, 1.0f, barHeight * 2.0f + 3.0f));
            const auto name = db == 0.0f ? juce::String ("0") : juce::String ((int) db);
            g.setColour (IlanaTheme::Ui::text3);
            IlanaTheme::drawFitted (g, name, juce::Rectangle<float> (28.0f, 11.0f).withCentre ({ x, barsTop + barHeight * 2.0f + 3.0f + 7.0f }).getSmallestIntegerContainer(),
                                    juce::Justification::centred, 1);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        clipped = false;
        loudest = 0.0f;
        repaint();
    }

private:
    // -48 dB .. +3 dB onto 0..1.
    static float proportion (float gain)
    {
        const auto db = juce::Decibels::gainToDecibels (gain, -60.0f);
        return juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 51.0f);
    }

    void timerCallback() override
    {
        if (! IlanaAnim::showing (*this))
            return;

        auto changed = false;

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto peak = processorRef.takeOutputPeak (channel);
            auto& level = levels[(size_t) channel];
            auto& hold = holds[(size_t) channel];
            auto& holdSeconds = holdTimes[(size_t) channel];
            const auto before = level;

            // Rise at once, fall about 20 dB per second.
            level = juce::jmax (peak, IlanaAnim::decay (level, 0.925f, frameTicks()));

            if (level < 0.0005f)
                level = 0.0f;

            if (peak >= hold)
            {
                hold = peak;
                holdSeconds = 1.2f;
            }
            else if ((holdSeconds -= frameSeconds()) <= 0.0f)
            {
                hold = IlanaAnim::decay (hold, 0.8f, frameTicks());
            }

            if (peak > 1.0f && ! clipped)
            {
                clipped = true;
                changed = true;
            }

            loudest = juce::jmax (loudest, peak);
            changed = changed || std::abs (level - before) > 1.0e-4f || hold > 0.0005f;
        }

        // (A latched clip light needs no repaint of its own: it was drawn
        // when it lit, and repainted every frame it idled at 60 fps.)
        if (changed)
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    std::array<float, 2> levels {}, holds {};
    std::array<float, 2> holdTimes {};
    float loudest = 0.0f;
    bool clipped = false;
};
