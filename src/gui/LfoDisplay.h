#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>

#include "../PluginProcessor.h"
#include "../dsp/LfoShape.h"
#include "IlanaLookAndFeel.h"

class LfoDisplay : public juce::Component,
                   public juce::SettableTooltipClient,
                   public IlanaAnim::PageAnimated,
                   private juce::Timer
{
public:
    LfoDisplay (IlanaSynthAudioProcessor& processor, int lfoIndex, juce::Colour traceColourIn = IlanaTheme::accent(),
                bool followsThemeIn = false)
        : processorRef (processor),
          index (lfoIndex),
          traceColour (traceColourIn),
          followsTheme (followsThemeIn)
    {
        setTooltip ("Drag to draw when Shape is Draw, or to set steps when Shape is Steps");

        juce::Random random (lfoIndex * 1234 + 7);

        for (auto& value : sampleHoldPreview)
            value = random.nextFloat() * 2.0f - 1.0f;

        startTimerHz (30);
    }

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        g.setOpacity (juce::jlimit (0.0f, 1.0f, appear));
        g.addTransform (juce::AffineTransform::translation (0.0f, (1.0f - juce::jlimit (0.0f, 1.0f, appear)) * 10.0f));

        if (appear < 0.999f)
        {
            g.setColour (traceColour.withAlpha (0.12f * (1.0f - appear)));
            g.fillRoundedRectangle (bounds.expanded (4.0f), 8.0f);
        }

        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto plot = bounds.reduced (10.0f, 14.0f);
        const auto centreY = plot.getCentreY();
        const auto halfHeight = plot.getHeight() * 0.42f;
        const auto shape = (int) readParam ("_shape");

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre ({ plot.getCentreX(), centreY }));

        juce::Path path;

        const auto isSampleHold = shape == 5;
        const auto isDraw = shape == 6;
        const auto isSteps = shape == 7;
        const auto cycles = 1.0;
        constexpr int stepsPerCycle = 8;

        std::array<float, IlanaSynthAudioProcessor::lfoDrawSteps> custom {};
        std::array<float, 16> steps {};

        if (isDraw)
            for (int i = 0; i < IlanaSynthAudioProcessor::lfoDrawSteps; ++i)
                custom[(size_t) i] = processorRef.getLfoCustomPoint (index, i);

        if (isSteps)
            for (int i = 0; i < 16; ++i)
                if (const auto* value = processorRef.apvts.getRawParameterValue (
                        "lfo" + juce::String (index + 1) + "_step" + juce::String (i + 1)))
                    steps[(size_t) i] = value->load();

        if (isSampleHold)
        {
            bool started = false;

            for (int x = 0; x < (int) plot.getWidth(); ++x)
            {
                const auto phase = (double) x / (double) plot.getWidth() * cycles;
                const auto step = juce::jlimit (0, stepsPerCycle * 2 - 1, (int) (phase * (double) stepsPerCycle));
                const auto value = sampleHoldPreview[(size_t) step];
                const auto y = centreY - value * halfHeight;
                const auto px = plot.getX() + (float) x;

                if (! started)
                {
                    path.startNewSubPath (px, y);
                    started = true;
                }
                else
                {
                    const auto previous = path.getCurrentPosition();

                    if (std::abs (previous.y - y) > 0.5f)
                    {
                        path.lineTo (px, previous.y);
                        path.lineTo (px, y);
                    }
                    else
                    {
                        path.lineTo (px, y);
                    }
                }
            }
        }
        else if (isSteps)
        {
            bool started = false;

            for (int x = 0; x < (int) plot.getWidth(); ++x)
            {
                const auto phase = (double) x / (double) plot.getWidth();
                const auto step = juce::jlimit (0, 15, (int) (phase * 16.0));
                const auto value = steps[(size_t) step];
                const auto y = centreY - value * halfHeight;
                const auto px = plot.getX() + (float) x;

                if (! started)
                {
                    path.startNewSubPath (px, y);
                    started = true;
                }
                else
                {
                    const auto previous = path.getCurrentPosition();
                    path.lineTo (px, previous.y);
                    path.lineTo (px, y);
                }
            }
        }
        else
        {
            for (int x = 0; x <= (int) plot.getWidth(); ++x)
            {
                const auto phase = (double) x / (double) plot.getWidth() * cycles;
                const auto value = isDraw ? interpolateCustom (custom, phase) : lfoShapeValue (shape, phase);
                const auto y = centreY - value * halfHeight;
                const auto px = plot.getX() + (float) x;

                if (x == 0)
                    path.startNewSubPath (px, y);
                else
                    path.lineTo (px, y);
            }
        }

        g.setColour (traceColour);
        g.strokePath (path, juce::PathStrokeType (1.6f));

        const auto phase = (double) processorRef.getLfoPhase (index);
        auto value = 0.0f;

        if (isSampleHold)
        {
            const auto step = juce::jlimit (0, stepsPerCycle * 2 - 1, (int) (phase * 2.0 * (double) stepsPerCycle));
            value = sampleHoldPreview[(size_t) step];
        }
        else if (isSteps)
        {
            const auto step = juce::jlimit (0, 15, (int) (phase * 16.0));
            value = steps[(size_t) step];
        }
        else if (isDraw)
        {
            value = interpolateCustom (custom, phase);
        }
        else
        {
            value = lfoShapeValue (shape, phase);
        }

        const auto dotX = plot.getX() + (float) phase * plot.getWidth() / (float) cycles;
        const auto dotY = centreY - value * halfHeight;

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ dotX, dotY }));
    }

private:
    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto shape = (int) readParam ("_shape");

        if (shape == 6)
            setCustomPoint (event.position);
        else if (shape == 7)
            setStepPoint (event.position);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto shape = (int) readParam ("_shape");

        if (shape == 6)
            setCustomPoint (event.position);
        else if (shape == 7)
            setStepPoint (event.position);
    }

    void lookAndFeelChanged() override
    {
        if (followsTheme)
            traceColour = IlanaTheme::accent();
    }

private:
    static float interpolateCustom (const std::array<float, IlanaSynthAudioProcessor::lfoDrawSteps>& table, double phase)
    {
        const auto position = phase * (double) IlanaSynthAudioProcessor::lfoDrawSteps;
        const auto index = (int) position % IlanaSynthAudioProcessor::lfoDrawSteps;
        const auto next = (index + 1) % IlanaSynthAudioProcessor::lfoDrawSteps;
        const auto frac = (float) (position - std::floor (position));

        return table[(size_t) index] + (table[(size_t) next] - table[(size_t) index]) * frac;
    }

    void setCustomPoint (juce::Point<float> position)
    {
        const auto plot = getLocalBounds().toFloat().reduced (10.0f, 14.0f);

        if (plot.getWidth() <= 1.0f)
            return;

        const auto step = juce::jlimit (0, IlanaSynthAudioProcessor::lfoDrawSteps - 1,
                                        (int) ((position.x - plot.getX()) / plot.getWidth()
                                               * (float) IlanaSynthAudioProcessor::lfoDrawSteps));
        const auto value = valueFromY (position.y, plot);

        processorRef.setLfoCustomPoint (index, step, value);
        repaint();
    }

    void setStepPoint (juce::Point<float> position)
    {
        const auto plot = getLocalBounds().toFloat().reduced (10.0f, 14.0f);

        if (plot.getWidth() <= 1.0f)
            return;

        const auto step = juce::jlimit (0, 15, (int) ((position.x - plot.getX()) / plot.getWidth() * 16.0f));

        if (auto* parameter = processorRef.apvts.getParameter ("lfo" + juce::String (index + 1)
                                                              + "_step" + juce::String (step + 1)))
        {
            if (gestureParameter != parameter)
            {
                if (gestureParameter != nullptr)
                    gestureParameter->endChangeGesture();

                gestureParameter = parameter;
                gestureParameter->beginChangeGesture();
            }

            parameter->setValueNotifyingHost (parameter->convertTo0to1 (valueFromY (position.y, plot)));
        }

        repaint();
    }

    static float valueFromY (float y, juce::Rectangle<float> plot)
    {
        const auto halfHeight = juce::jmax (1.0f, plot.getHeight() * 0.42f);
        return juce::jlimit (-1.0f, 1.0f, (plot.getCentreY() - y) / halfHeight);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (gestureParameter != nullptr)
        {
            gestureParameter->endChangeGesture();
            gestureParameter = nullptr;
        }
    }

    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f);
        repaint();
    }

    float readParam (const char* suffix) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (index + 1) + suffix))
            return value->load();

        return 0.0f;
    }

    IlanaSynthAudioProcessor& processorRef;
    int index = 0;
    juce::Colour traceColour;
    bool followsTheme = false;
    juce::RangedAudioParameter* gestureParameter = nullptr;
    float appear = 1.0f;
    std::array<float, 16> sampleHoldPreview {};
};
