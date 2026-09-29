#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// A slim stereo peak meter for the master output: bars fall smoothly, a
// peak line holds for a moment, and the top lights red after a clip until
// clicked.
class OutputMeter : public juce::Component,
                    public juce::SettableTooltipClient,
                    private IlanaAnim::FrameTimer
{
public:
    explicit OutputMeter (IlanaSynthAudioProcessor& processor) : processorRef (processor)
    {
        setTooltip ("Output level (peak). The top lights red if the output went over 0 dB; click to reset it.");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 3.0f);

        auto inner = bounds.reduced (2.0f, 3.0f);
        const auto clipZone = inner.removeFromTop (3.0f);
        inner.removeFromTop (1.0f);
        const auto barWidth = (inner.getWidth() - 1.0f) * 0.5f;

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto bar = juce::Rectangle<float> (inner.getX() + (float) channel * (barWidth + 1.0f), inner.getY(),
                                                     barWidth, inner.getHeight());
            const auto level = proportion (levels[(size_t) channel]);
            const auto filled = bar.withTrimmedTop (bar.getHeight() * (1.0f - level));

            juce::ColourGradient gradient (juce::Colour (0xffff4f5e), bar.getX(), bar.getY(),
                                           juce::Colour (0xff4fd1a5), bar.getX(), bar.getBottom(), false);
            gradient.addColour (1.0 - proportion (juce::Decibels::decibelsToGain (-6.0f)), juce::Colour (0xffffd447));
            g.setGradientFill (gradient);
            g.fillRect (filled);

            const auto hold = proportion (holds[(size_t) channel]);

            if (hold > 0.01f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.fillRect (bar.withY (bar.getBottom() - bar.getHeight() * hold).withHeight (1.0f));
            }
        }

        g.setColour (clipped ? juce::Colour (0xffff4f5e) : juce::Colours::white.withAlpha (0.08f));
        g.fillRect (clipZone);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        clipped = false;
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
        if (! isShowing())
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

            clipped = clipped || peak > 1.0f;
            changed = changed || std::abs (level - before) > 1.0e-4f || hold > 0.0005f;
        }

        if (changed || clipped)
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    std::array<float, 2> levels {}, holds {};
    std::array<float, 2> holdTimes {};
    bool clipped = false;
};
