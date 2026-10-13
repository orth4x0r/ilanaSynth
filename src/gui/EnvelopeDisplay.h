#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <tuple>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/TensionAdsr.h"
#include "IlanaLookAndFeel.h"
#include "ParamInfo.h"
#include "AnimationUtils.h"
#include "DisplayStyle.h"
#include "FmOperatorInfo.h"

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
        setTooltip ("Drag a handle to set its stage (it follows the mouse).\n"
                    "Drag the dot on the attack, decay or release to curve that segment alone (CURVE curves all three).\n"
                    "Double-click a handle or a dot to reset it. Esc cancels a drag.\n"
                    "Each stage gets room by the square root of its time; the ruler under the graph gives the time since "
                    "the note started, up to KEY UP (the release's own time is on its knob). The dot is the last note played.");
        setWantsKeyboardFocus (true);
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
        IlanaAnim::countPaint ("env");
        const auto bounds = getLocalBounds().toFloat();

        g.setOpacity (juce::jlimit (0.0f, 1.0f, appear));
        g.addTransform (juce::AffineTransform::translation (0.0f, (1.0f - juce::jlimit (0.0f, 1.0f, appear)) * 10.0f));

        if (appear < 0.999f)
        {
            g.setColour (curveColour.withAlpha (0.12f * (1.0f - appear)));
            g.fillRoundedRectangle (bounds.expanded (4.0f), 8.0f);
        }

        // The well frames the plot only; the time ruler sits under it,
        // outside the frame (UI review 7, V7-23).
        IlanaTheme::paintWell (g, bounds.withTrimmedBottom (slim ? 0.0f : rulerHeight + 2.0f), 6.0f);

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
            // At the top of its stretch, clear of the ruler below the plot;
            // only where it fits (a sliver of delay is just the band).
            if (to - from >= (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 6.0f)
                g.drawText (name, band.withTrimmedTop (10.0f).withHeight (12.0f).toNearestInt(), juce::Justification::centred);
        }

        auto filled = path;
        filled.lineTo (geo.x0, geo.yBottom);
        filled.closeSubPath();

        g.setColour (curveColour.withAlpha (0.12f));
        g.fillPath (filled);

        IlanaTrace::paintTrace (g, path, curveColour);

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

        // Curve dots sit on the middle of each curved segment, one per
        // segment; kept subtle so they read as part of the curve until
        // hovered.
        const auto tensionPoints = tensionHandlePositions (geo);

        for (int segment = 0; segment < 3; ++segment)
        {
            const auto& point = tensionPoints[(size_t) segment];
            const auto tensionActive = hoverHandle == firstCurveHandle + segment || dragHandle == firstCurveHandle + segment;
            g.setColour (juce::Colours::black.withAlpha (tensionActive ? 0.6f : 0.4f));
            g.fillEllipse (juce::Rectangle<float> (tensionActive ? 9.0f : 6.0f, tensionActive ? 9.0f : 6.0f)
                               .withCentre (point));
            g.setColour (curveColour.withAlpha (tensionActive ? 1.0f : 0.5f));
            g.fillEllipse (juce::Rectangle<float> (tensionActive ? 6.0f : 3.5f, tensionActive ? 6.0f : 3.5f)
                               .withCentre (point));
        }

        paintTrail (g, geo);

        // The last played note's place on the curve.
        if (const auto playhead = playheadPoint (geo); playhead.has_value() || dotFade > 0.0f)
        {
            if (playhead.has_value())
                lastDot = *playhead;

            const auto fade = playhead.has_value() ? 1.0f : dotFade;
            const auto at = lastDot;

            // A soft halo, a ring in the curve's colour with a white rim,
            // and one ripple when a note starts.
            juce::ColourGradient halo (curveColour.withAlpha (0.45f * fade), at.x, at.y,
                                       curveColour.withAlpha (0.0f), at.x + 15.0f, at.y, true);
            g.setGradientFill (halo);
            g.fillEllipse (juce::Rectangle<float> (30.0f, 30.0f).withCentre (at));
            g.setColour (curveColour.interpolatedWith (juce::Colours::white, 0.15f).withAlpha (fade));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (at));
            g.setColour (juce::Colours::white.withAlpha (0.9f * fade));
            g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (at), 1.4f);

            if (const auto age = (juce::Time::getMillisecondCounterHiRes() - pingMs) / 450.0; age >= 0.0 && age < 1.0)
            {
                const auto size = 9.0f + 22.0f * (float) (1.0 - (1.0 - age) * (1.0 - age));
                g.setColour (curveColour.withAlpha (0.55f * (1.0f - (float) age)));
                g.drawEllipse (juce::Rectangle<float> (size, size).withCentre (at), 1.2f);
            }
        }
        else if (const auto monitor = readMonitor(); monitor > 0.002f)
        {
            // No stage to place it by: a short tick at the edge for the level.
            const auto monitorY = geo.yBottom - monitor * (geo.yBottom - geo.yTop);
            g.setColour (curveColour.withAlpha (0.7f));
            g.fillRect (juce::Rectangle<float> (8.0f, 2.0f).withPosition (geo.plot.getRight() - 8.0f, monitorY - 1.0f));
        }

        // The value being dragged, beside the handle (left of it near the
        // right edge, below it near the top), inside the plot.
        if (readout.isNotEmpty() && dragHandle >= 0)
        {
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
            const auto anchor = handlePosition (geo, dragHandle);
            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, readout) + 12.0f;
            auto box = juce::Rectangle<float> (width, 16.0f).withPosition (anchor.x + 10.0f, anchor.y - 24.0f);
            if (box.getRight() > geo.plot.getRight())
                box.setX (anchor.x - 10.0f - width);
            if (box.getY() < geo.plot.getY())
                box.setY (anchor.y + 10.0f);
            box = box.constrainedWithin (getLocalBounds().toFloat().reduced (2.0f));

            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (curveColour.withAlpha (0.95f));
            g.setFont (font);
            g.drawText (readout, box.toNearestInt(), juce::Justification::centred);
        }
    }

    // The plot inside the well: the time ruler sits below the well (UI
    // review 6, V6-17; review 7, V7-23), so tick labels never meet the
    // curve, its handles or DELAY, and read as an axis.
    static constexpr float rulerHeight = 14.0f;

    bool slim = false;

public:
    // PLAY's graph is a slim strip across the card: no time ruler under it.
    void setSlim (bool shouldBeSlim) { slim = shouldBeSlim; repaint(); }

    juce::Rectangle<float> getPlotArea() const
    {
        if (slim)
            return getLocalBounds().toFloat().reduced (12.0f, 9.0f);
        return getLocalBounds().toFloat().withTrimmedTop (14.0f).withTrimmedBottom (rulerHeight + 2.0f + 8.0f).reduced (12.0f, 0.0f);
    }

    // Where a handle is drawn (0-3 the stages, 4-6 the curve dots), and the
    // readout while one is dragged; the UI test reaches these.
    juce::Point<float> getHandlePosition (int handle) const { return handlePosition (layoutGeometry(), handle); }
    const juce::String& getReadout() const { return readout; }
    static constexpr int firstCurveHandle = 4;

private:
    struct Geometry
    {
        juce::Rectangle<float> plot;
        // Note start, attack start (after the delay), peak, hold end, decay
        // end, sustain end, release end.
        float x0 = 0.0f, xStart = 0.0f, xA = 0.0f, xH = 0.0f, xD = 0.0f, xS = 0.0f, xR = 0.0f;
        float yTop = 0.0f, yBottom = 0.0f, ySustain = 0.0f;
        // The attack's, decay's and release's tension rates (TensionCurve).
        std::array<double, 3> rates { 0.0, 0.0, 0.0 };
        float scale = 1.0f;
    };

    static juce::Path curvePath (const Geometry& geo)
    {
        juce::Path path;
        path.startNewSubPath (geo.x0, geo.yBottom);
        path.lineTo (geo.xStart, geo.yBottom);
        addSegment (path, geo.xStart, geo.yBottom, geo.xA, geo.yTop, true, geo.rates[0]);
        path.lineTo (geo.xH, geo.yTop);
        addSegment (path, geo.xH, geo.yTop, geo.xD, geo.ySustain, false, geo.rates[1]);
        path.lineTo (geo.xS, geo.ySustain);
        addSegment (path, geo.xS, geo.ySustain, geo.xR, geo.yBottom, false, geo.rates[2]);
        return path;
    }

    static void addSegment (juce::Path& path, float x1, float y1, float x2, float y2, bool rising, double rate)
    {
        // A step about every 2 px, so the dot (which sits on the exact
        // curve) never leaves the drawn line on a strong bend.
        const auto steps = juce::jlimit (24, 160, (int) (juce::jmax (std::abs (x2 - x1), std::abs (y2 - y1)) / 2.0f));

        for (int i = 1; i <= steps; ++i)
        {
            const auto u = (float) i / (float) steps;
            const auto value = (float) TensionCurve::shape (rising ? u : 1.0f - u, rate);
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
        geo.plot = getPlotArea();

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
        for (int segment = 0; segment < 3; ++segment)
            geo.rates[(size_t) segment] = TensionCurve::rateFor (segmentCurve (segment));
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
                                 juce::jmap ((float) TensionCurve::shape (0.5, geo.rates[0]), geo.yBottom, geo.yTop) },
            juce::Point<float> { (geo.xH + geo.xD) * 0.5f,
                                 geo.yBottom - (readValue ("sustain")
                                                + (1.0f - readValue ("sustain")) * (float) TensionCurve::shape (0.5, geo.rates[1]))
                                                    * geo.plot.getHeight() },
            juce::Point<float> { (geo.xS + geo.xR) * 0.5f,
                                 geo.yBottom - readValue ("sustain") * (float) TensionCurve::shape (0.5, geo.rates[2])
                                                   * geo.plot.getHeight() }
        };
    }

    juce::Point<float> handlePosition (const Geometry& geo, int handle) const
    {
        if (handle >= firstCurveHandle)
            return tensionHandlePositions (geo)[(size_t) juce::jlimit (0, 2, handle - firstCurveHandle)];
        return stageHandles (geo)[(size_t) juce::jlimit (0, 3, handle)];
    }

    // The curved segment (4 attack, 5 decay, 6 release) under an x, or -1
    // over the flat delay, hold and sustain.
    static int segmentAt (const Geometry& geo, float x)
    {
        if (x >= geo.xStart && x <= geo.xA)
            return firstCurveHandle;
        if (x >= geo.xH && x <= geo.xD)
            return firstCurveHandle + 1;
        if (x >= geo.xS && x <= geo.xR)
            return firstCurveHandle + 2;
        return -1;
    }

public:
    // Whether the trail or the dot still needs frames (false: nothing to animate).
    bool trailIsLit() const { return trailAlive; }

    // How far the dot is from the drawn line, in pixels (-1: no dot). The
    // dot is meant to ride the line exactly.
    float playheadOffLine() const
    {
        const auto geo = layoutGeometry();
        const auto dot = playheadPoint (geo);

        if (! dot.has_value())
            return -1.0f;

        juce::Point<float> nearest;
        curvePath (geo).getNearestPoint (*dot, nearest);
        return dot->getDistanceFrom (nearest);
    }

    // Where the dot for the last played note sits: on the curve at its
    // stage and progress (in the release, at its actual level, since a
    // note let go early releases from below the sustain). None while idle.
    std::optional<juce::Point<float>> playheadPoint() const { return playheadPoint (layoutGeometry()); }

private:
    std::optional<juce::Point<float>> playheadPoint (const Geometry& geo) const
    {
        const auto position = shownPosition();

        // Idle, or an envelope that isn't running (it waits at the start).
        if (position <= 0.0f)
            return std::nullopt;

        return pointAt (geo, position, processorRef.getEnvMonitorStageStart (envelopeIndex()));
    }

    float shownPosition() const
    {
        const auto position = positionSmoother.get (processorRef.getEnvMonitorPosition (envelopeIndex()),
                                                    processorRef.getEnvMonitorSeconds (envelopeIndex()),
                                                    processorRef.getNoteSerial());
        updateTrail (position);
        return position;
    }

    // The point on the drawn line at position `p` (stage + progress; 4 is
    // the sustain, whose dot slides in along the line). The attack and the
    // release ride the drawn curve by the level they really have: a
    // retrigger's attack starts partway up, a release from below the
    // sustain starts partway down, and the dot goes to where on the line
    // that level sits, so it never leaves the line.
    juce::Point<float> pointAt (const Geometry& geo, float p, float from) const
    {
        const auto stage = juce::jlimit (0, 5, (int) p);
        const auto u = juce::jlimit (0.0f, 1.0f, p - (float) stage);
        const auto height = geo.yBottom - geo.yTop;
        from = juce::jlimit (0.0f, 1.0f, from);

        switch (stage)
        {
            case 0: return { geo.x0 + u * (geo.xStart - geo.x0), geo.yBottom };
            case 1:
            {
                const auto level = from + (1.0f - from) * (float) TensionCurve::shape (u, geo.rates[0]);
                const auto along = from > 0.0f ? (float) TensionCurve::inverse (level, geo.rates[0]) : u;
                return { geo.xStart + along * (geo.xA - geo.xStart), geo.yBottom - level * height };
            }
            case 2: return { geo.xA + u * (geo.xH - geo.xA), geo.yTop };
            case 3: return { geo.xH + u * (geo.xD - geo.xH),
                             geo.yTop + (geo.ySustain - geo.yTop) * (1.0f - (float) TensionCurve::shape (1.0f - u, geo.rates[1])) };
            case 4: return { geo.xD + u * (geo.xS - geo.xD), geo.ySustain };
            default:
            {
                // (Let go above the sustain, the dot takes the drawn
                // release from the top, as the line has no part above it.)
                const auto sustain = (geo.yBottom - geo.ySustain) / juce::jmax (1.0f, height);
                const auto below = from < sustain - 1.0e-4f && sustain > 1.0e-4f;
                const auto level = (below ? from : sustain) * (float) TensionCurve::shape (1.0f - u, geo.rates[2]);
                const auto along = below ? 1.0f - (float) TensionCurve::inverse (juce::jlimit (0.0f, 1.0f, level / sustain), geo.rates[2]) : u;
                return { geo.xS + along * (geo.xR - geo.xS), geo.yBottom - level * height };
            }
        }
    }

    // The glowing trail the dot leaves on the line: a brightness for each
    // slice of the curve (a stage and its progress), lit as the dot
    // passes and fading in about half a second, as in Vital.
    static constexpr int trailBinsPerStage = 48, trailBins = 6 * trailBinsPerStage;

    void updateTrail (float position) const
    {
        if (position <= 0.0f && ! trailAlive)
        {
            trailHead = -1.0f;
            trailMs = 0.0;
            return;
        }

        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto dt = trailMs > 0.0 ? juce::jlimit (0.0, 0.1, (now - trailMs) / 1000.0) : 0.0;
        trailMs = now;
        const auto fade = (float) std::exp (-dt / 0.4);
        auto alive = false;

        for (auto& value : trail)
        {
            value *= fade;
            if (value < 0.03f)
                value = 0.0f;
            alive = alive || value > 0.0f;
        }

        if (position > 0.0f)
        {
            const auto bin = [] (float p) { return juce::jlimit (0, trailBins - 1, (int) (p * (float) trailBinsPerStage)); };
            const auto from = trailHead >= 0.0f && position >= trailHead - 1.0e-4f ? bin (trailHead) : bin (position);

            for (int i = from; i <= bin (position); ++i)
                trail[(size_t) i] = 1.0f;

            // A new note rings once from the dot.
            if (trailHead < 0.0f || position < trailHead - 0.05f)
                pingMs = now;

            trailHead = position;
            alive = true;
            dotFade = 1.0f;
        }
        else
        {
            trailHead = -1.0f;
            dotFade = juce::jmax (0.0f, dotFade - (float) dt / 0.25f);
        }

        trailAlive = alive || dotFade > 0.0f || now - pingMs < 450.0;
    }

    void paintTrail (juce::Graphics& g, const Geometry& geo) const
    {
        if (! trailAlive)
            return;

        const auto from = juce::jlimit (0.0f, 1.0f, processorRef.getEnvMonitorStageStart (envelopeIndex()));
        const auto step = 1.0f / (float) trailBinsPerStage;
        const auto bottom = geo.yBottom;
        auto previous = pointAt (geo, 0.0001f, from);
        // The wash under the line fades in over the last 24 px before the
        // dot, so it has no hard front edge.
        const auto headX = trailHead > 0.0f ? pointAt (geo, trailHead, from).x : 1.0e9f;

        for (int i = 0; i < trailBins; ++i)
        {
            const auto next = pointAt (geo, juce::jmin (5.999f, (float) (i + 1) * step), from);
            const auto lit = trail[(size_t) i];

            if (lit > 0.0f && (std::abs (next.x - previous.x) > 0.01f || std::abs (next.y - previous.y) > 0.01f))
            {
                // The area under the line brightens, then the line itself:
                // a wide soft stroke and a bright core.
                juce::Path wash;
                wash.addQuadrilateral (previous.x, previous.y, next.x, next.y, next.x, bottom, previous.x, bottom);
                const auto front = juce::jlimit (0.0f, 1.0f, (headX - next.x) / 24.0f);
                g.setGradientFill (juce::ColourGradient (curveColour.withAlpha (0.3f * lit * front), 0.0f, juce::jmin (previous.y, next.y),
                                                         curveColour.withAlpha (0.02f * lit * front), 0.0f, bottom, false));
                g.fillPath (wash);
                g.setColour (curveColour.withAlpha (0.22f * lit));
                g.drawLine (previous.x, previous.y, next.x, next.y, 5.0f);
                g.setColour (curveColour.interpolatedWith (juce::Colours::white, 0.45f).withAlpha (0.9f * lit));
                g.drawLine (previous.x, previous.y, next.x, next.y, 2.2f);
            }

            previous = next;
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

        if (slim)
            return;

        const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny);
        g.setFont (font);
        std::vector<juce::Range<float>> labelled;
        const auto rulerY = (float) getHeight() - rulerHeight;

        // KEY UP, where the release starts: a dashed line through the plot
        // and its name on the ruler, so the release's "+" times read as
        // after it (UI review 7, V7-23 / S7-28).
        {
            const float dashes[] { 3.0f, 3.0f };
            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.drawDashedLine ({ geo.xS, geo.yTop, geo.xS, geo.yBottom }, dashes, 2, 1.0f);
            const auto boldFont = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (boldFont, "KEY UP") + 4.0f;
            const auto left = juce::jlimit (geo.plot.getX() - 8.0f, geo.plot.getRight() + 8.0f - width, geo.xS - width * 0.5f);
            labelled.push_back ({ left, left + width });
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (boldFont);
            g.drawText ("KEY UP", juce::Rectangle<float> (left, rulerY + 2.0f, width, 12.0f), juce::Justification::centred);
            g.setFont (font);
        }

        // One scale on the ruler (UI review 8, S8-31): the time since the
        // note started, up to KEY UP. The release, timed from the key, has
        // no ticks of its own ("+100 ms" read as a second axis).
        for (const auto inRelease : { false })
        {
            // The decades first (10 ms, 100 ms, 1 s, 10 s keep their labels),
            // then the in-between times where they fit; longest first within each.
            for (const auto pass : { 0, 1 })
            for (const auto seconds : { 20.0f, 10.0f, 5.0f, 2.0f, 1.0f, 0.5f, 0.2f, 0.1f, 0.05f, 0.02f, 0.01f, 0.005f, 0.002f })
            {
                const auto x = inRelease ? releaseX (seconds) : noteX (seconds);

                if (x < 0.0f)
                    continue;

                const auto decade = std::abs (std::log10 (seconds) - std::round (std::log10 (seconds))) < 1.0e-3f && seconds >= 0.01f;

                if ((pass == 0) != decade)
                    continue;

                // The in-between times (20 ms, 50 ms, 200 ms, 0.5 s, 2 s, 5 s) are
                // labelled too where there is room, so a long stage reads more
                // than two ticks (V10-19); where there isn't, a short mark.
                const auto text = (inRelease ? "+" : "") + (seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms"
                                                                          : juce::String (juce::roundToInt (seconds)) + " s");
                const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 4.0f;
                const auto span = juce::Range<float> (x + 3.0f, x + 3.0f + width);
                const auto fits = span.getEnd() <= geo.plot.getRight() + 10.0f
                                  && ! std::any_of (labelled.begin(), labelled.end(),
                                                    [span] (juce::Range<float> other) { return other.expanded (4.0f).intersects (span); });

                if (! decade && (! fits || seconds < 0.02f))
                {
                    g.setColour (juce::Colours::white.withAlpha (0.12f));
                    g.fillRect (juce::Rectangle<float> (1.0f, 3.0f).withPosition (x, rulerY - 1.0f));
                    continue;
                }

                if (decade)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.06f));
                    g.fillRect (juce::Rectangle<float> (1.0f, geo.yBottom - geo.yTop).withPosition (x, geo.yTop));
                }

                if (! fits)
                    continue;

                labelled.push_back (span);
                g.setColour (IlanaTheme::Ui::text3);
                g.fillRect (juce::Rectangle<float> (1.0f, 4.0f).withPosition (x, rulerY - 1.0f));
                g.drawText (text, juce::Rectangle<float> (span.getStart(), rulerY + 2.0f, width, 12.0f), juce::Justification::centredLeft);
            }
        }
    }

    float readMonitor() const
    {
        return levelSmoother.get (readRawMonitor());
    }

    float readRawMonitor() const
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
        // The amp envelope on a DX7 voice shapes nothing: drawn faint, as
        // its knobs are, on PLAY and MOD alike (UI review 7, V7-24).
        if (paramPrefix == "amp")
            if (const auto alpha = FmOperatorInfo::ampEnvelopeInUse (processorRef) ? 1.0f : 0.4f; getAlpha() != alpha)
                setAlpha (alpha);

        appear = juce::jmin (1.0f, appear + 0.12f * frameTicks());

        const auto position = shownPosition();

        if (IlanaAnim::showing (*this) && (appear < 1.0f || trailAlive || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)
                                                                ^ IlanaAnim::phaseSignature (position, 31))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;
    IlanaAnim::EnvTracker positionSmoother;
    mutable std::array<float, trailBins> trail {};
    mutable float trailHead = -1.0f, dotFade = 0.0f;
    mutable double trailMs = 0.0, pingMs = -1.0e9;
    mutable bool trailAlive = false;
    mutable juce::Point<float> lastDot;
    IlanaAnim::BlockSmoother levelSmoother; // monitors glided between audio blocks

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

    // A segment's bend (0 attack, 1 decay, 2 release): CURVE plus its own,
    // clamped as the envelope does.
    float segmentCurve (int segment) const
    {
        const char* const suffixes[] { "acurve", "dcurve", "rcurve" };
        return juce::jlimit (-1.0f, 1.0f, readCurve() + readValue (suffixes[juce::jlimit (0, 2, segment)]));
    }

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
            case 4: return "acurve";
            case 5: return "dcurve";
            case 6: return "rcurve";
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

        const auto tensionPoints = tensionHandlePositions (geo);
        for (int segment = 0; segment < 3; ++segment)
            if (tensionPoints[(size_t) segment].getDistanceFrom (position) < 12.0f)
                return firstCurveHandle + segment;

        return -1;
    }

    // The curved segment whose line is near `position` (dragging the line
    // bends it, as its dot does), or -1.
    int curveSegmentNear (juce::Point<float> position) const
    {
        const auto geo = layoutGeometry();

        const auto path = curvePath (geo);

        juce::Point<float> nearest;
        const auto distance = path.getNearestPoint (position, nearest);

        return distance < 10.0f ? segmentAt (geo, nearest.x) : -1;
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        auto hovered = findHandle (event.position);

        if (hovered < 0)
            hovered = curveSegmentNear (event.position);

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

        if (dragHandle < 0)
            dragHandle = curveSegmentNear (event.position);

        lastMousePosition = event.position;
        frozenScale = fitScale (getPlotArea());
        cancelled = false;

        // The handle keeps its place under the press: the pointer may be up
        // to the hit radius off its centre, and the drag moves the handle by
        // the pointer's movement, not to the pointer (no jump on first click).
        if (dragHandle >= 0)
            dragPoint = handlePosition (layoutGeometry(), dragHandle);

        if (dragHandle >= 0)
        {
            dragParameter = parameterFor (suffixForHandle (dragHandle));

            if (dragParameter != nullptr)
            {
                dragNormalised = dragParameter->getValue();
                startNormalised = dragNormalised;
                processorRef.beginEdit (dragParameter->getName (64));
                dragParameter->beginChangeGesture();

                // The decay handle also sets sustain.
                sustainGesture = dragHandle == 1 ? parameterFor ("sustain") : nullptr;

                if (sustainGesture != nullptr)
                {
                    startSustain = sustainGesture->getValue();
                    sustainGesture->beginChangeGesture();
                }
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

    // Esc during a drag puts the stage (and the sustain a decay drag moves) back.
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() != juce::KeyPress::escapeKey || dragParameter == nullptr || cancelled)
            return false;

        dragParameter->setValueNotifyingHost (startNormalised);

        if (sustainGesture != nullptr)
            sustainGesture->setValueNotifyingHost (startSustain);

        cancelled = true;
        readout.clear();
        repaint();
        return true;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragHandle < 0 || dragParameter == nullptr || cancelled)
            return;

        const auto geo = layoutGeometry();

        // Shift (or Ctrl / Cmd) moves the handle 10x finer than the pointer.
        {
            const auto fine = event.mods.isShiftDown() || event.mods.isCommandDown() ? 0.1f : 1.0f;
            dragPoint += (event.position - lastMousePosition) * fine;
            dragPoint.x = juce::jmax (dragPoint.x, geo.x0);
            dragPoint.y = juce::jlimit (geo.yTop, geo.yBottom, dragPoint.y);
        }

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
                setSeconds (*dragParameter, unitsToSeconds (juce::jmax (0.0f, dragPoint.x - geo.xStart) / geo.scale));
                break;

            case 1:
                setSeconds (*dragParameter, unitsToSeconds (juce::jmax (0.0f, dragPoint.x - geo.xH) / geo.scale));
                setSustainFromY (dragPoint.y);
                break;

            case 2:
                setSustainFromY (dragPoint.y);
                break;

            case 3:
                setSeconds (*dragParameter, unitsToSeconds (juce::jmax (0.0f, dragPoint.x - geo.xS) / geo.scale));
                break;

            default:
            {
                // A segment's curve: its dot follows the mouse up and down
                // (up bows the segment up). Where the segment is too flat to
                // place the dot by, the drag moves it by distance instead.
                const auto segment = dragHandle - firstCurveHandle;
                const auto sustain = readValue ("sustain");
                const auto height = geo.plot.getHeight();
                const auto level = (geo.yBottom - dragPoint.y) / juce::jmax (1.0f, height);
                const auto low = segment == 0 ? 0.0f : segment == 1 ? sustain : 0.0f;
                const auto high = segment == 0 ? 1.0f : segment == 1 ? 1.0f : sustain;

                if ((high - low) * height >= 8.0f)
                {
                    // The dot is the segment at its middle: low + span * shape (0.5).
                    const auto fraction = (level - low) / (high - low);
                    const auto bend = (float) (-TensionCurve::rateForMidHeight (fraction) / TensionCurve::range);
                    const auto range = dragParameter->getNormalisableRange();
                    dragNormalised = dragParameter->convertTo0to1 (juce::jlimit (range.start, range.end, bend - readCurve()));
                }
                else
                {
                    const auto delta = (event.position.y - lastMousePosition.y) * (event.mods.isShiftDown() || event.mods.isCommandDown() ? 0.1f : 1.0f);
                    dragNormalised = juce::jlimit (0.0f, 1.0f, dragNormalised - delta * 0.004f);
                }

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

        if (handle < 0)
            handle = curveSegmentNear (event.position);

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

        // A segment's curve reads as the bend it plays (CURVE plus its own),
        // in the words the knob uses.
        if (dragHandle >= firstCurveHandle)
        {
            const char* const names[] { "ATTACK", "DECAY", "RELEASE" };
            const auto segment = juce::jlimit (0, 2, dragHandle - firstCurveHandle);
            const auto bend = segmentCurve (segment);
            readout = juce::String (names[segment]) + " CURVE  " + (bend >= 0.005f ? "+" : "") + juce::String (bend, 2);
            return;
        }

        const auto id = paramPrefix + "_" + suffixForHandle (dragHandle);
        const auto value = dragParameter->convertFrom0to1 (dragParameter->getValue());
        readout = juce::String (suffixForHandle (dragHandle)).toUpperCase() + "  " + describeValue (id, value);
        // The decay handle sets the sustain too.
        if (dragHandle == 1)
            readout << "  SUSTAIN " << describeValue (paramPrefix + "_sustain", readValue ("sustain"));
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
    float startNormalised = 0.0f, startSustain = 0.0f; // at the press, for Esc
    bool cancelled = false;
    float frozenScale = 1.0f;
    juce::Point<float> lastMousePosition, dragPoint;
    juce::String readout;
};
