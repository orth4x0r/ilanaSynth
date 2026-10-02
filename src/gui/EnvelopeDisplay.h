#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <tuple>
#include <vector>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "ParamInfo.h"
#include "AnimationUtils.h"

class EnvelopeDisplay : public juce::Component,
                        public juce::SettableTooltipClient,
                        public IlanaAnim::PageAnimated,
                        private IlanaAnim::FrameTimer
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
                    "double-click a handle to reset it.\nTime runs on a square-root scale: the ticks give the time since "
                    "the note started, and after the sustain since the key was let go (+). The dot is the last note "
                    "played.");
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

        const auto path = curvePath (geo);

        paintTimeTicks (g, geo);

        // DAHDSR: the delay and hold stretches get a faint band and a label.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        for (const auto& [from, to, name] : { std::tuple<float, float, const char*> { geo.x0, geo.xStart, "DELAY" },
                                              std::tuple<float, float, const char*> { geo.xA, geo.xH, "HOLD" } })
        {
            if (to - from < 2.0f)
                continue;

            const auto band = juce::Rectangle<float> (from, geo.yTop, to - from, geo.yBottom - geo.yTop);
            g.setColour (curveColour.withAlpha (0.06f));
            g.fillRect (band);
            g.setColour (curveColour.withAlpha (0.55f));
            g.drawText (name, band.withTrimmedTop (band.getHeight() - 16.0f).toNearestInt(), juce::Justification::centred);
        }

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

        // The last played note's place on the curve.
        if (const auto playhead = playheadPoint (geo); playhead.has_value())
        {
            // In the curve's colour with a white rim, so it doesn't read as
            // one of the white stage handles.
            g.setColour (curveColour.withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (*playhead));
            g.setColour (curveColour.interpolatedWith (juce::Colours::white, 0.15f));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (*playhead));
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (*playhead), 1.4f);
        }
        else if (const auto monitor = readMonitor(); monitor > 0.002f)
        {
            // No stage to place it by: a short tick at the edge for the level.
            const auto monitorY = geo.yBottom - monitor * (geo.yBottom - geo.yTop);
            g.setColour (curveColour.withAlpha (0.7f));
            g.fillRect (juce::Rectangle<float> (8.0f, 2.0f).withPosition (geo.plot.getRight() - 8.0f, monitorY - 1.0f));
        }

        if (readout.isNotEmpty())
        {
            const auto readoutBounds = juce::Rectangle<float> (geo.plot.getX() + 4.0f, geo.plot.getY() + 2.0f, 220.0f, 16.0f);

            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (readoutBounds, 4.0f);
            g.setColour (curveColour.withAlpha (0.95f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (readout, readoutBounds.toNearestInt(), juce::Justification::centredLeft);
        }
    }

private:
    struct Geometry
    {
        juce::Rectangle<float> plot;
        // Note start, attack start (after the delay), peak, hold end, decay
        // end, sustain end, release end.
        float x0 = 0.0f, xStart = 0.0f, xA = 0.0f, xH = 0.0f, xD = 0.0f, xS = 0.0f, xR = 0.0f;
        float yTop = 0.0f, yBottom = 0.0f, ySustain = 0.0f;
        float exponent = 1.0f;
        float scale = 1.0f;
    };

    static juce::Path curvePath (const Geometry& geo)
    {
        juce::Path path;
        path.startNewSubPath (geo.x0, geo.yBottom);
        path.lineTo (geo.xStart, geo.yBottom);
        addSegment (path, geo.xStart, geo.yBottom, geo.xA, geo.yTop, true, geo.exponent);
        path.lineTo (geo.xH, geo.yTop);
        addSegment (path, geo.xH, geo.yTop, geo.xD, geo.ySustain, false, geo.exponent);
        path.lineTo (geo.xS, geo.ySustain);
        addSegment (path, geo.xS, geo.ySustain, geo.xR, geo.yBottom, false, geo.exponent);
        return path;
    }

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
        const auto total = timeUnits (readSeconds ("delay")) + timeUnits (readSeconds ("attack"))
                           + timeUnits (readSeconds ("hold")) + timeUnits (readSeconds ("decay"))
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
        geo.xStart = geo.x0 + scale * timeUnits (readSeconds ("delay"));
        geo.xA = geo.xStart + scale * timeUnits (readSeconds ("attack"));
        geo.xH = geo.xA + scale * timeUnits (readSeconds ("hold"));
        geo.xD = geo.xH + scale * timeUnits (readSeconds ("decay"));
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
            juce::Point<float> { (geo.xStart + geo.xA) * 0.5f,
                                 juce::jmap (std::pow (0.5f, geo.exponent), geo.yBottom, geo.yTop) },
            juce::Point<float> { (geo.xH + geo.xD) * 0.5f,
                                 geo.yBottom - (readValue ("sustain")
                                                + (1.0f - readValue ("sustain")) * std::pow (0.5f, geo.exponent))
                                                    * geo.plot.getHeight() },
            juce::Point<float> { (geo.xS + geo.xR) * 0.5f,
                                 geo.yBottom - readValue ("sustain") * std::pow (0.5f, geo.exponent)
                                                   * geo.plot.getHeight() }
        };
    }

public:
    // Where the dot for the last played note sits: on the curve at its
    // stage and progress (in the release, at its actual level, since a
    // note let go early releases from below the sustain). None while idle.
    std::optional<juce::Point<float>> playheadPoint() const { return playheadPoint (layoutGeometry()); }

private:
    std::optional<juce::Point<float>> playheadPoint (const Geometry& geo) const
    {
        const auto position = processorRef.getEnvMonitorPosition (envelopeIndex());

        // Idle, or an envelope that isn't running (it waits at the start).
        if (position <= 0.0f)
            return std::nullopt;

        const auto stage = juce::jlimit (0, 5, (int) position);
        const auto u = juce::jlimit (0.0f, 1.0f, position - (float) stage);
        const auto height = geo.yBottom - geo.yTop;

        switch (stage)
        {
            case 0: return juce::Point<float> (geo.x0 + u * (geo.xStart - geo.x0), geo.yBottom);
            case 1: return juce::Point<float> (geo.xStart + u * (geo.xA - geo.xStart), geo.yBottom - std::pow (u, geo.exponent) * height);
            case 2: return juce::Point<float> (geo.xA + u * (geo.xH - geo.xA), geo.yTop);
            case 3: return juce::Point<float> (geo.xH + u * (geo.xD - geo.xH),
                                               geo.yTop + (geo.ySustain - geo.yTop) * (1.0f - std::pow (1.0f - u, geo.exponent)));
            case 4: return juce::Point<float> (geo.xD + 0.25f * (geo.xS - geo.xD), geo.ySustain);
            default: return juce::Point<float> (geo.xS + u * (geo.xR - geo.xS),
                                                geo.yBottom - juce::jlimit (0.0f, 1.0f, readMonitor()) * height);
        }
    }

    // ENV 1-16 (amp, filter, filter 2, mod, ENV 5, ENV 6-16) for this graph.
    int envelopeIndex() const
    {
        if (paramPrefix.startsWith ("env"))
            return paramPrefix.substring (3).getIntValue() - 1;

        return paramPrefix == "fe" ? 1 : paramPrefix == "f2e" ? 2 : paramPrefix == "me" ? 3 : paramPrefix == "e4" ? 4 : 0;
    }

    // Faint time lines on the square-root axis. Inside a stage time runs
    // evenly, so a tick lands where the note is that long after its start
    // (through delay, attack, hold and decay) or, past the sustain, after
    // the key was let go. Ticks at 10 ms, 100 ms, 1 s and 10 s are labelled;
    // the 2s and 5s between them are short marks at the base.
    void paintTimeTicks (juce::Graphics& g, const Geometry& geo) const
    {
        const float spans[] { readSeconds ("delay"), readSeconds ("attack"), readSeconds ("hold"), readSeconds ("decay") };
        const float edges[] { geo.x0, geo.xStart, geo.xA, geo.xH, geo.xD };

        const auto noteX = [&] (float seconds) -> float
        {
            auto start = 0.0f;

            for (int stage = 0; stage < 4; ++stage)
            {
                if (spans[stage] > 0.0f && seconds < start + spans[stage])
                    return edges[stage] + (seconds - start) / spans[stage] * (edges[stage + 1] - edges[stage]);

                start += juce::jmax (0.0f, spans[stage]);
            }

            return -1.0f;
        };
        const auto release = readSeconds ("release");
        const auto releaseX = [&] (float seconds)
        {
            return release > 0.0f && seconds < release ? geo.xS + seconds / release * (geo.xR - geo.xS) : -1.0f;
        };

        const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny);
        g.setFont (font);
        std::vector<juce::Range<float>> labelled;

        for (const auto inRelease : { false, true })
        {
            // Longest first, so where ticks crowd the bigger time keeps its label.
            for (const auto seconds : { 20.0f, 10.0f, 5.0f, 2.0f, 1.0f, 0.5f, 0.2f, 0.1f, 0.05f, 0.02f, 0.01f, 0.005f, 0.002f })
            {
                const auto x = inRelease ? releaseX (seconds) : noteX (seconds);

                if (x < 0.0f)
                    continue;

                const auto decade = std::abs (std::log10 (seconds) - std::round (std::log10 (seconds))) < 1.0e-3f && seconds >= 0.01f;

                if (! decade)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.12f));
                    g.fillRect (juce::Rectangle<float> (1.0f, 4.0f).withPosition (x, geo.yBottom - 4.0f));
                    continue;
                }

                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillRect (juce::Rectangle<float> (1.0f, geo.yBottom - geo.yTop).withPosition (x, geo.yTop));

                const auto text = (inRelease ? "+" : "") + (seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms"
                                                                          : juce::String (juce::roundToInt (seconds)) + " s");
                const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 4.0f;
                const auto span = juce::Range<float> (x + 3.0f, x + 3.0f + width);

                if (span.getEnd() > geo.plot.getRight() + 10.0f
                    || std::any_of (labelled.begin(), labelled.end(), [span] (juce::Range<float> other) { return other.expanded (4.0f).intersects (span); }))
                    continue;

                labelled.push_back (span);
                g.setColour (IlanaTheme::Ui::text3);
                g.drawText (text, juce::Rectangle<float> (span.getStart(), geo.yBottom - 13.0f, width, 12.0f), juce::Justification::centredLeft);
            }
        }
    }

    float readMonitor() const
    {
        if (paramPrefix.startsWith ("env"))
            return processorRef.getEnvMonitorExtra (paramPrefix.substring (3).getIntValue() - 6);

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
        appear = juce::jmin (1.0f, appear + 0.12f * frameTicks());

        if (isShowing() && (appear < 1.0f || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)
                                                                ^ IlanaAnim::phaseSignature (processorRef.getEnvMonitorPosition (envelopeIndex()), 31))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

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

        const auto path = curvePath (geo);

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
                processorRef.beginEdit (dragParameter->getName (64));
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

        processorRef.endEdit();
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
                setSeconds (*dragParameter, unitsToSeconds ((event.position.x - geo.xStart) / geo.scale));
                break;

            case 1:
                setSeconds (*dragParameter, unitsToSeconds ((event.position.x - geo.xH) / geo.scale));
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
            processorRef.performEdit ("Reset " + parameter->getName (64), [parameter]
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
                parameter->endChangeGesture();
            });
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
