#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "ParamInfo.h"

class EnvelopeDisplay : public juce::Component,
                        public juce::SettableTooltipClient,
                        public IlanaAnim::PageAnimated,
                        private juce::Timer
{
public:
    EnvelopeDisplay (IlanaSynthAudioProcessor& processor, juce::String prefix,
                     juce::Colour curveColourIn = IlanaTheme::accent(),
                     bool followsThemeIn = false)
        : processorRef (processor),
          paramPrefix (std::move (prefix)),
          curveColour (curveColourIn),
          followsTheme (followsThemeIn)
    {
        setTooltip ("Drag a handle to set its stage (it follows the mouse), drag the curve to bend the tension, "
                    "double-click a handle to reset it");
        startTimerHz (30);
    }

    void lookAndFeelChanged() override
    {
        if (followsTheme)
            curveColour = IlanaTheme::accent();
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
            g.setColour (curveColour.withAlpha (0.12f * (1.0f - appear)));
            g.fillRoundedRectangle (bounds.expanded (4.0f), 8.0f);
        }

        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto geo = layoutGeometry();

        juce::Path path;
        path.startNewSubPath (geo.x0, geo.yBottom);
        addSegment (path, geo.x0, geo.yBottom, geo.xA, geo.yTop, true, geo.exponent);
        addSegment (path, geo.xA, geo.yTop, geo.xD, geo.ySustain, false, geo.exponent);
        path.lineTo (geo.xS, geo.ySustain);
        addSegment (path, geo.xS, geo.ySustain, geo.xR, geo.yBottom, false, geo.exponent);

        auto filled = path;
        filled.lineTo (geo.x0, geo.yBottom);
        filled.closeSubPath();

        g.setColour (curveColour.withAlpha (0.12f));
        g.fillPath (filled);

        g.setColour (curveColour);
        g.strokePath (path, juce::PathStrokeType (1.8f));

        // Stage handles.
        const auto handles = stageHandles (geo);

        for (int i = 0; i < 4; ++i)
        {
            const auto hovered = hoverHandle == i;

            if (hovered)
            {
                g.setColour (curveColour.withAlpha (0.35f));
                g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (handles[i]));
            }

            g.setColour (juce::Colours::white.withAlpha (hovered ? 1.0f : 0.85f));
            g.fillEllipse (juce::Rectangle<float> (hovered ? 10.0f : 8.0f, hovered ? 10.0f : 8.0f)
                               .withCentre (handles[i]));
        }

        // Tension handles sit on the middle of each curved segment; kept
        // subtle so they read as part of the curve until hovered.
        const auto tensionPoints = tensionHandlePositions (geo);
        const auto tensionActive = hoverHandle == 4 || dragHandle == 4;

        for (const auto& point : tensionPoints)
        {
            g.setColour (juce::Colours::black.withAlpha (tensionActive ? 0.6f : 0.4f));
            g.fillEllipse (juce::Rectangle<float> (tensionActive ? 9.0f : 6.0f, tensionActive ? 9.0f : 6.0f)
                               .withCentre (point));
            g.setColour (curveColour.withAlpha (tensionActive ? 1.0f : 0.5f));
            g.fillEllipse (juce::Rectangle<float> (tensionActive ? 6.0f : 3.5f, tensionActive ? 6.0f : 3.5f)
                               .withCentre (point));
        }

        const auto monitor = readMonitor();

        if (monitor > 0.002f)
        {
            const auto monitorY = geo.yBottom - monitor * (geo.yBottom - geo.yTop);

            g.setColour (curveColour.withAlpha (0.5f));
            g.fillRect (juce::Rectangle<float> (geo.plot.getWidth(), 1.2f)
                            .withCentre ({ geo.plot.getCentreX(), monitorY }));
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ geo.plot.getRight(), monitorY }));
        }

        if (readout.isNotEmpty())
        {
            const auto readoutBounds = juce::Rectangle<float> (geo.plot.getX() + 4.0f, geo.plot.getY() + 2.0f, 220.0f, 16.0f);

            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (readoutBounds, 4.0f);
            g.setColour (curveColour.withAlpha (0.95f));
            g.setFont (IlanaTheme::font (11.5f, true));
            g.drawText (readout, readoutBounds.toNearestInt(), juce::Justification::centredLeft);
        }
    }

private:
    struct Geometry
    {
        juce::Rectangle<float> plot;
        float x0 = 0.0f, xA = 0.0f, xD = 0.0f, xS = 0.0f, xR = 0.0f;
        float yTop = 0.0f, yBottom = 0.0f, ySustain = 0.0f;
        float exponent = 1.0f;
        float scale = 1.0f;
    };

    static void addSegment (juce::Path& path, float x1, float y1, float x2, float y2, bool rising, float exponent)
    {
        constexpr int steps = 20;

        for (int i = 1; i <= steps; ++i)
        {
            const auto u = (float) i / (float) steps;
            const auto value = rising ? std::pow (u, exponent) : std::pow (1.0f - u, exponent);
            const auto x = x1 + (x2 - x1) * u;
            const auto y = y1 + (y2 - y1) * (rising ? value : (1.0f - value));

            path.lineTo (x, y);
        }
    }

    // Time runs on a square-root scale so short and long stages both read.
    // Each stage's width depends only on its own time, and the overall zoom
    // is frozen while dragging, so a handle stays exactly under the mouse.
    static constexpr float holdUnits = 0.55f;

    static float timeUnits (float seconds) { return std::sqrt (juce::jmax (0.0f, seconds)); }
    static float unitsToSeconds (float units) { return units * units; }

    float fitScale (const juce::Rectangle<float>& plot) const
    {
        const auto total = timeUnits (readSeconds ("attack")) + timeUnits (readSeconds ("decay"))
                           + holdUnits + timeUnits (readSeconds ("release"));

        // A little headroom so the release end is never pinned to the edge,
        // and a minimum span so tiny envelopes don't zoom in absurdly.
        return plot.getWidth() / juce::jmax (1.6f, total * 1.15f);
    }

    Geometry layoutGeometry() const
    {
        Geometry geo;
        geo.plot = getLocalBounds().toFloat().reduced (12.0f, 14.0f);

        const auto scale = dragHandle >= 0 && dragHandle < 4 ? frozenScale : fitScale (geo.plot);
        const auto sustain = readValue ("sustain");

        geo.x0 = geo.plot.getX();
        geo.xA = geo.x0 + scale * timeUnits (readSeconds ("attack"));
        geo.xD = geo.xA + scale * timeUnits (readSeconds ("decay"));
        geo.xS = geo.xD + scale * holdUnits;
        geo.xR = geo.xS + scale * timeUnits (readSeconds ("release"));
        geo.yTop = geo.plot.getY();
        geo.yBottom = geo.plot.getBottom();
        geo.ySustain = geo.yBottom - sustain * geo.plot.getHeight();
        geo.exponent = std::exp2 (-readCurve() * 2.0f);
        geo.scale = scale;

        return geo;
    }

    static std::array<juce::Point<float>, 4> stageHandles (const Geometry& geo)
    {
        return { juce::Point<float> { geo.xA, geo.yTop },
                 juce::Point<float> { geo.xD, geo.ySustain },
                 juce::Point<float> { (geo.xD + geo.xS) * 0.5f, geo.ySustain },
                 juce::Point<float> { geo.xR, geo.yBottom } };
    }

    std::array<juce::Point<float>, 3> tensionHandlePositions (const Geometry& geo) const
    {
        return {
            juce::Point<float> { (geo.x0 + geo.xA) * 0.5f,
                                 juce::jmap (std::pow (0.5f, geo.exponent), geo.yBottom, geo.yTop) },
            juce::Point<float> { (geo.xA + geo.xD) * 0.5f,
                                 geo.yBottom - (readValue ("sustain")
                                                + (1.0f - readValue ("sustain")) * std::pow (0.5f, geo.exponent))
                                                    * geo.plot.getHeight() },
            juce::Point<float> { (geo.xS + geo.xR) * 0.5f,
                                 geo.yBottom - readValue ("sustain") * std::pow (0.5f, geo.exponent)
                                                   * geo.plot.getHeight() }
        };
    }

    float readMonitor() const
    {
        if (paramPrefix == "fe")
            return processorRef.getEnvMonitorFilter();

        if (paramPrefix == "f2e")
            return processorRef.getEnvMonitorFilter2();

        if (paramPrefix == "me")
            return processorRef.getEnvMonitorMod();

        if (paramPrefix == "e4")
            return processorRef.getEnvMonitorEnv4();

        return processorRef.getEnvMonitorAmp();
    }

    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f);
        repaint();
    }

    float readSeconds (const char* suffix) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (paramPrefix + "_" + suffix))
            return value->load();

        return 0.0f;
    }

    float readValue (const char* suffix) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (paramPrefix + "_" + suffix))
            return value->load();

        return 0.0f;
    }

    float readCurve() const { return readValue ("curve"); }

    juce::RangedAudioParameter* parameterFor (const char* suffix) const
    {
        return dynamic_cast<juce::RangedAudioParameter*> (processorRef.apvts.getParameter (paramPrefix + "_" + suffix));
    }

    static const char* suffixForHandle (int handle)
    {
        switch (handle)
        {
            case 0: return "attack";
            case 1: return "decay";
            case 2: return "sustain";
            case 3: return "release";
            default: return "curve";
        }
    }

    int findHandle (juce::Point<float> position) const
    {
        const auto geo = layoutGeometry();
        const auto handles = stageHandles (geo);

        // Closest handle wins, so stacked handles (tiny times) stay reachable.
        auto best = -1;
        auto bestDistance = 14.0f;

        for (int i = 0; i < 4; ++i)
        {
            const auto distance = handles[(size_t) i].getDistanceFrom (position);

            if (distance < bestDistance)
            {
                best = i;
                bestDistance = distance;
            }
        }

        if (best >= 0)
            return best;

        for (const auto& tensionPoint : tensionHandlePositions (geo))
            if (tensionPoint.getDistanceFrom (position) < 12.0f)
                return 4;

        return -1;
    }

    bool isNearCurve (juce::Point<float> position) const
    {
        const auto geo = layoutGeometry();

        juce::Path path;
        path.startNewSubPath (geo.x0, geo.yBottom);
        addSegment (path, geo.x0, geo.yBottom, geo.xA, geo.yTop, true, geo.exponent);
        addSegment (path, geo.xA, geo.yTop, geo.xD, geo.ySustain, false, geo.exponent);
        path.lineTo (geo.xS, geo.ySustain);
        addSegment (path, geo.xS, geo.ySustain, geo.xR, geo.yBottom, false, geo.exponent);

        juce::Point<float> nearest;
        const auto distance = path.getNearestPoint (position, nearest);

        return distance < 10.0f;
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        auto hovered = findHandle (event.position);

        if (hovered < 0 && isNearCurve (event.position))
            hovered = 4;

        if (hovered != hoverHandle)
        {
            hoverHandle = hovered;
            repaint();
        }

        setMouseCursor (hovered >= 0 ? juce::MouseCursor::PointingHandCursor
                                     : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverHandle != -1)
        {
            hoverHandle = -1;
            repaint();
        }

        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragHandle = findHandle (event.position);

        if (dragHandle < 0 && isNearCurve (event.position))
            dragHandle = 4;

        lastMousePosition = event.position;
        frozenScale = fitScale (getLocalBounds().toFloat().reduced (12.0f, 14.0f));

        if (dragHandle >= 0)
        {
            dragParameter = parameterFor (suffixForHandle (dragHandle));

            if (dragParameter != nullptr)
            {
                dragNormalised = dragParameter->getValue();
                dragParameter->beginChangeGesture();

                // The decay handle also sets sustain.
                sustainGesture = dragHandle == 1 ? parameterFor ("sustain") : nullptr;

                if (sustainGesture != nullptr)
                    sustainGesture->beginChangeGesture();
                updateReadout();
            }
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragParameter != nullptr)
        {
            dragParameter->endChangeGesture();
            dragParameter = nullptr;
        }

        if (sustainGesture != nullptr)
        {
            sustainGesture->endChangeGesture();
            sustainGesture = nullptr;
        }

        dragHandle = -1;
        readout.clear();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragHandle < 0 || dragParameter == nullptr)
            return;

        const auto geo = layoutGeometry();
        const auto setSeconds = [] (juce::RangedAudioParameter& parameter, float seconds)
        {
            const auto range = parameter.getNormalisableRange();
            parameter.setValueNotifyingHost (parameter.convertTo0to1 (juce::jlimit (range.start, range.end, seconds)));
        };

        const auto setSustainFromY = [this, &geo] (float y)
        {
            if (auto* sustainParameter = parameterFor ("sustain"))
                sustainParameter->setValueNotifyingHost (
                    juce::jlimit (0.0f, 1.0f, (geo.yBottom - y) / juce::jmax (1.0f, geo.plot.getHeight())));
        };

        // Stage handles map the mouse straight to a time: the distance from
        // where the stage starts, back through the square-root scale.
        switch (dragHandle)
        {
            case 0:
                setSeconds (*dragParameter, unitsToSeconds ((event.position.x - geo.x0) / geo.scale));
                break;

            case 1:
                setSeconds (*dragParameter, unitsToSeconds ((event.position.x - geo.xA) / geo.scale));
                setSustainFromY (event.position.y);
                break;

            case 2:
                setSustainFromY (event.position.y);
                break;

            case 3:
                setSeconds (*dragParameter, unitsToSeconds ((event.position.x - geo.xS) / geo.scale));
                break;

            default:
            {
                // Tension: drag up for a snappier, more exponential curve.
                const auto delta = event.position.y - lastMousePosition.y;
                dragNormalised = juce::jlimit (0.0f, 1.0f, dragNormalised - delta * 0.004f);
                dragParameter->setValueNotifyingHost (dragNormalised);
                break;
            }
        }

        lastMousePosition = event.position;
        updateReadout();
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        auto handle = findHandle (event.position);

        if (handle < 0 && isNearCurve (event.position))
            handle = 4;

        if (handle < 0)
            return;

        if (auto* parameter = parameterFor (suffixForHandle (handle)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
    }

    void updateReadout()
    {
        if (dragParameter == nullptr)
        {
            readout.clear();
            return;
        }

        const auto id = paramPrefix + "_" + suffixForHandle (dragHandle);
        const auto value = dragParameter->convertFrom0to1 (dragParameter->getValue());
        const auto label = dragHandle == 4 ? juce::String ("TENSION")
                                           : juce::String (suffixForHandle (dragHandle)).toUpperCase();

        readout = label + "  " + describeValue (id, value);
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::String paramPrefix;
    juce::Colour curveColour;
    bool followsTheme = false;
    float appear = 1.0f;
    int dragHandle = -1;
    int hoverHandle = -1;
    juce::RangedAudioParameter* dragParameter = nullptr;
    juce::RangedAudioParameter* sustainGesture = nullptr;
    float dragNormalised = 0.5f;
    float frozenScale = 1.0f;
    juce::Point<float> lastMousePosition;
    juce::String readout;
};
