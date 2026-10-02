#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../dsp/LfoShape.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

class StepEditor : public juce::Component,
                   public juce::SettableTooltipClient,
                   public IlanaAnim::PageAnimated,
                   private IlanaAnim::FrameTimer
{
public:
    StepEditor (IlanaSynthAudioProcessor& processor, int lfoIndex, juce::Colour stepColourIn = IlanaTheme::accent(),
                bool followsThemeIn = false)
        : processorRef (processor),
          index (lfoIndex),
          stepColour (IlanaSynthAudioProcessor::lfoColour (lfoIndex)),
          followsTheme (followsThemeIn)
    {
        // Drawn in its LFO's colour (as on its chip and card), whatever
        // colour the page asked for.
        juce::ignoreUnused (stepColourIn);
        setTooltip ("Drag to draw the 16 step values");
        startTimerHz (24);
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
        stepColour = IlanaSynthAudioProcessor::lfoColour (index);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        g.setOpacity (juce::jlimit (0.0f, 1.0f, appear));
        g.addTransform (juce::AffineTransform::translation (0.0f, (1.0f - juce::jlimit (0.0f, 1.0f, appear)) * 10.0f));

        IlanaTheme::paintWell (g, bounds, 6.0f);

        auto plot = bounds.reduced (8.0f, 10.0f);
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
        appear = juce::jmin (1.0f, appear + 0.12f * frameTicks());

        if (isShowing() && (appear < 1.0f || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

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

    void mouseDown (const juce::MouseEvent& event) override
    {
        processorRef.beginEdit ("LFO " + juce::String (index + 1) + " steps");
        setFromPosition (event.position);
    }

    void mouseDrag (const juce::MouseEvent& event) override { setFromPosition (event.position); }
    void mouseUp (const juce::MouseEvent&) override { processorRef.endEdit(); }

    IlanaSynthAudioProcessor& processorRef;
    int index = 0;
    juce::Colour stepColour;
    bool followsTheme = false;
    float appear = 1.0f;
};

class MsegEditor : public juce::Component,
                   public juce::SettableTooltipClient,
                   public IlanaAnim::PageAnimated,
                   private IlanaAnim::FrameTimer
{
public:
    explicit MsegEditor (IlanaSynthAudioProcessor& processor)
        : processorRef (processor)
    {
        setTooltip ("MSEG\nDrag the points: horizontal = time, vertical = level. The labels give the time into one "
                    "cycle at the current RATE; the line and dot show where it is now.");
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

        paintTimeGrid (g, plot, centreY, halfHeight);

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

        g.setColour (juce::Colour (0xffe0e6f0)); // the MSEG's source colour
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

        // The playhead: a faint line where the MSEG is in its cycle and a
        // dot riding the curve there, as on the LFO graph.
        const auto phase = juce::jlimit (0.0f, 1.0f, processorRef.getMsegPhase());
        const auto playX = plot.getX() + phase * plot.getWidth();
        g.setColour (juce::Colour (0xffe0e6f0).withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (playX, plot.getY()));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ playX, centreY - valueAt (phase) * halfHeight }));
    }

private:
    // The level the MSEG plays at `phase` (0..1), as Mseg::valueAt does.
    float valueAt (float phase) const
    {
        auto total = 0.0f;

        for (int i = 0; i < 4; ++i)
            total += readTime (i);

        auto cumulative = 0.0f;

        for (int i = 0; i < 4; ++i)
        {
            const auto duration = readTime (i) / total;

            if (phase < cumulative + duration || i == 3)
            {
                const auto local = juce::jlimit (0.0f, 1.0f, duration > 0.0001f ? (phase - cumulative) / duration : 0.0f);
                const auto to = i < 3 ? readLevel (i + 1) : (isLooping() ? readLevel (0) : readLevel (3));
                return readLevel (i) + (to - readLevel (i)) * local;
            }

            cumulative += duration;
        }

        return readLevel (3);
    }

    // Faint level lines at +-0.5 and +-1, and time lines with labels: one
    // cycle lasts 1 / RATE seconds (before modulation).
    void paintTimeGrid (juce::Graphics& g, juce::Rectangle<float> plot, float centreY, float halfHeight) const
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));

        for (const auto level : { -1.0f, -0.5f, 0.5f, 1.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), centreY - level * halfHeight));

        const auto* rate = processorRef.apvts.getRawParameterValue ("mseg_rate");
        const auto cycle = 1.0 / juce::jmax (0.001, rate != nullptr ? (double) rate->load() : 1.0);
        auto step = 0.001;

        for (const auto candidate : { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0 })
        {
            step = candidate;

            if (cycle / candidate <= 8.0)
                break;
        }

        const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny);
        g.setFont (font);
        auto lastLabelRight = -1.0e9f;

        for (int tick = 0; (double) tick * step <= cycle + 1.0e-9; ++tick)
        {
            const auto seconds = (double) tick * step;
            const auto x = plot.getX() + (float) (seconds / cycle) * plot.getWidth();
            g.setColour (juce::Colours::white.withAlpha (tick == 0 ? 0.0f : 0.05f));
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (x, plot.getY()));

            const auto text = seconds < 1.0 ? juce::String (juce::roundToInt (seconds * 1000.0)) + " ms"
                                            : juce::String (seconds, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " s";
            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 4.0f;
            auto labelX = x + 3.0f;

            if (labelX + width > plot.getRight())
                labelX = x - width - 3.0f;

            if (labelX < lastLabelRight + 6.0f)
                continue;

            lastLabelRight = labelX + width;
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText (text, juce::Rectangle<float> (labelX, plot.getBottom() - 12.0f, width, 12.0f), juce::Justification::centredLeft);
        }
    }

    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f * frameTicks());

        if (isShowing() && (appear < 1.0f || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)
                                                                ^ IlanaAnim::phaseSignature (processorRef.getMsegPhase(), 77))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

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

        if (dragHandle >= 0)
            processorRef.beginEdit ("MSEG point " + juce::String (dragHandle + 1));
    }

    void mouseUp (const juce::MouseEvent&) override { processorRef.endEdit(); }

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
