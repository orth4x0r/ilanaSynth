#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"

class StepEditor : public juce::Component,
                   public juce::SettableTooltipClient,
                   public IlanaAnim::PageAnimated,
                   private juce::Timer
{
public:
    StepEditor (IlanaSynthAudioProcessor& processor, int lfoIndex, juce::Colour stepColourIn = IlanaTheme::accent(),
                bool followsThemeIn = false)
        : processorRef (processor),
          index (lfoIndex),
          stepColour (stepColourIn),
          followsTheme (followsThemeIn)
    {
        setTooltip ("Drag to draw the 16 step values");
        startTimerHz (24);
    }

    void lookAndFeelChanged() override
    {
        if (followsTheme)
            stepColour = IlanaTheme::accent();
    }

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

    int getLfoIndex() const { return index; }

    void setLfoIndex (int lfoIndex)
    {
        index = juce::jlimit (0, IlanaSynthAudioProcessor::numLfos - 1, lfoIndex);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        g.setOpacity (juce::jlimit (0.0f, 1.0f, appear));
        g.addTransform (juce::AffineTransform::translation (0.0f, (1.0f - juce::jlimit (0.0f, 1.0f, appear)) * 10.0f));

        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto plot = bounds.reduced (8.0f, 10.0f);
        const auto centreY = plot.getCentreY();
        const auto halfHeight = plot.getHeight() * 0.44f;
        const auto stepWidth = plot.getWidth() / 16.0f;

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre ({ plot.getCentreX(), centreY }));

        for (int step = 0; step < 16; ++step)
        {
            const auto value = readStep (step);
            const auto x = plot.getX() + (float) step * stepWidth;
            const auto y = centreY - value * halfHeight;

            g.setColour (stepColour.withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (x + 1.0f, juce::jmin (y, centreY), juce::jmax (1.0f, stepWidth - 2.0f),
                                                std::abs (y - centreY)));

            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (x + stepWidth - 1.0f, plot.getY(), 1.0f, plot.getHeight()));
        }
    }

private:
    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f);
        repaint();
    }

    float readStep (int step) const
    {
        const auto id = "lfo" + juce::String (index + 1) + "_step" + juce::String (step + 1);

        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    void setFromPosition (juce::Point<float> position)
    {
        const auto plot = getLocalBounds().toFloat().reduced (8.0f, 10.0f);

        if (plot.getWidth() <= 1.0f)
            return;

        const auto step = juce::jlimit (0, 15, (int) ((position.x - plot.getX()) / plot.getWidth() * 16.0f));
        const auto value = juce::jlimit (-1.0f, 1.0f, 1.0f - 2.0f * (position.y - plot.getY()) / plot.getHeight());
        const auto id = "lfo" + juce::String (index + 1) + "_step" + juce::String (step + 1);

        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));

        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override { setFromPosition (event.position); }
    void mouseDrag (const juce::MouseEvent& event) override { setFromPosition (event.position); }

    IlanaSynthAudioProcessor& processorRef;
    int index = 0;
    juce::Colour stepColour;
    bool followsTheme = false;
    float appear = 1.0f;
};

class MsegEditor : public juce::Component,
                   public juce::SettableTooltipClient,
                   public IlanaAnim::PageAnimated,
                   private juce::Timer
{
public:
    explicit MsegEditor (IlanaSynthAudioProcessor& processor)
        : processorRef (processor)
    {
        setTooltip ("Drag the points: horizontal = time, vertical = level");
        startTimerHz (24);
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

        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto plot = bounds.reduced (10.0f, 12.0f);
        const auto centreY = plot.getCentreY();
        const auto halfHeight = plot.getHeight() * 0.46f;

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre ({ plot.getCentreX(), centreY }));

        auto totalTime = 0.0f;

        for (int i = 0; i < 4; ++i)
            totalTime += readTime (i);

        juce::Path path;
        auto cumulative = 0.0f;
        auto started = false;

        for (int i = 0; i < 4; ++i)
        {
            const auto x = plot.getX() + (cumulative / totalTime) * plot.getWidth();
            const auto y = centreY - readLevel (i) * halfHeight;

            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }

            cumulative += readTime (i);
        }

        const auto endX = plot.getX() + plot.getWidth();

        if (isLooping())
            path.lineTo (endX, centreY - readLevel (0) * halfHeight);
        else
            path.lineTo (endX, centreY - readLevel (3) * halfHeight);

        g.setColour (juce::Colour (0xff6fe3c1));
        g.strokePath (path, juce::PathStrokeType (1.8f));

        cumulative = 0.0f;

        for (int i = 0; i < 4; ++i)
        {
            const auto x = plot.getX() + (cumulative / totalTime) * plot.getWidth();
            const auto y = centreY - readLevel (i) * halfHeight;

            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y }));

            cumulative += readTime (i);
        }
    }

private:
    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f);
        repaint();
    }

    bool isLooping() const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("mseg_loop"))
            return value->load() > 0.5f;

        return true;
    }

    float readLevel (int point) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("mseg_level" + juce::String (point + 1)))
            return value->load();

        return 0.0f;
    }

    float readTime (int point) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("mseg_time" + juce::String (point + 1)))
            return juce::jmax (0.01f, value->load());

        return 0.25f;
    }

    void setParameter (const juce::String& id, float plainValue)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }

    int findHandle (juce::Point<float> position) const
    {
        const auto plot = getLocalBounds().toFloat().reduced (10.0f, 12.0f);
        const auto centreY = plot.getCentreY();
        const auto halfHeight = plot.getHeight() * 0.46f;

        auto totalTime = 0.0f;

        for (int i = 0; i < 4; ++i)
            totalTime += readTime (i);

        auto cumulative = 0.0f;

        for (int i = 0; i < 4; ++i)
        {
            const auto x = plot.getX() + (cumulative / totalTime) * plot.getWidth();
            const auto y = centreY - readLevel (i) * halfHeight;

            if (juce::Point<float> (x, y).getDistanceFrom (position) < 14.0f)
                return i;

            cumulative += readTime (i);
        }

        return -1;
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragHandle = findHandle (event.position);
        lastMousePosition = event.position;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragHandle < 0)
            return;

        const auto delta = event.position - lastMousePosition;
        lastMousePosition = event.position;

        const auto plot = getLocalBounds().toFloat().reduced (10.0f, 12.0f);
        const auto level = juce::jlimit (-1.0f, 1.0f,
                                         1.0f - 2.0f * (event.position.y - plot.getY()) / plot.getHeight());

        setParameter ("mseg_level" + juce::String (dragHandle + 1), level);

        const auto currentTime = readTime (dragHandle);
        setParameter ("mseg_time" + juce::String (dragHandle + 1),
                      juce::jlimit (0.05f, 1.0f, currentTime + delta.x * 0.004f));
    }

    IlanaSynthAudioProcessor& processorRef;
    int dragHandle = -1;
    juce::Point<float> lastMousePosition;
    float appear = 1.0f;
};
