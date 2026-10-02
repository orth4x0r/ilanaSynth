#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/LfoShape.h"
#include "IlanaLookAndFeel.h"
#include "LfoSimView.h"
#include "AnimationUtils.h"

class LfoDisplay : public juce::Component,
                   public juce::SettableTooltipClient,
                   public IlanaAnim::PageAnimated,
                   private IlanaAnim::FrameTimer
{
public:
    LfoDisplay (IlanaSynthAudioProcessor& processor, int lfoIndex, juce::Colour traceColourIn = IlanaTheme::accent(),
                bool followsThemeIn = false)
        : processorRef (processor),
          index (lfoIndex),
          traceColour (traceColourIn),
          followsTheme (followsThemeIn)
    {
        setTooltip ("LFO shape\nDraw: drag to draw.  Steps: drag to set steps.  Curve: click to add points, drag them, "
                    "drag the dot on a line to bend it, double-click to delete, right-click for shapes and grid.");

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

        if (const auto simShape = (int) readParam ("_shape"); LfoSimShapes::isSim (simShape))
        {
            simPreview.paint (g, bounds.reduced (10.0f, 8.0f), processorRef.readLfoSimSettings (index), traceColour);
            return;
        }

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
        else if (shape == IlanaSynthAudioProcessor::curveShape)
        {
            curve = processorRef.getLfoCurve (index);
            paintCurveGrid (g, plot, centreY, halfHeight);

            for (int x = 0; x <= (int) plot.getWidth(); ++x)
            {
                const auto phase = (double) x / (double) plot.getWidth();
                const auto y = centreY - curve.valueAt (juce::jmin (0.99999, phase)) * halfHeight;
                const auto px = plot.getX() + (float) x;

                if (x == 0)
                    path.startNewSubPath (px, y);
                else
                    path.lineTo (px, y);
            }
        }
        else if (LfoShapes::isPhysics (shape))
        {
            // The real motion for the current PHYSICS A / B, several cycles long
            // since it doesn't repeat each cycle.
            const auto& trace = physicsTrace (shape, (int) plot.getWidth());

            for (int x = 0; x < (int) trace.size(); ++x)
            {
                const auto y = centreY - trace[(size_t) x] * halfHeight;
                const auto px = plot.getX() + (float) x;

                if (x == 0)
                    path.startNewSubPath (px, y);
                else
                    path.lineTo (px, y);
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

        auto phase = (double) processorRef.getLfoPhase (index);
        auto value = 0.0f;

        // Count whole cycles so the physics dot can walk the longer trace.
        if (phase < lastPhase)
            cycleCount = (cycleCount + 1) % physicsCycles;
        lastPhase = phase;

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
        else if (shape == IlanaSynthAudioProcessor::curveShape)
        {
            value = curve.valueAt (phase);
        }
        else if (LfoShapes::isPhysics (shape))
        {
            // The dot rides the trace; the shape doesn't follow the phase, so
            // the live value would wander off it.
            const auto& trace = physicsTrace (shape, (int) plot.getWidth());
            const auto position = ((double) cycleCount + phase) / (double) physicsCycles;
            const auto x = juce::jlimit (0, (int) trace.size() - 1, (int) (position * (double) (trace.size() - 1)));
            value = trace[(size_t) x];
            phase = position;
        }
        else if (LfoShapes::isStateful (shape))
        {
            // Random and chaos shapes show a representative picture; the dot
            // follows it (an unrouted LFO 5-16 has no live value to show).
            value = lfoShapeValue (shape, phase);
        }
        else
        {
            value = lfoShapeValue (shape, phase);
        }

        const auto dotX = plot.getX() + (float) phase * plot.getWidth() / (float) cycles;
        const auto dotY = centreY - value * halfHeight;

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ dotX, dotY }));

        if (shape == IlanaSynthAudioProcessor::curveShape)
            paintCurveHandles (g, plot, centreY, halfHeight);
    }

private:
    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto shape = (int) readParam ("_shape");

        // One undo step per stroke (the curve's right-click menu makes its own).
        if (shape == 6 || shape == 7 || (shape == IlanaSynthAudioProcessor::curveShape && ! event.mods.isPopupMenu()))
            processorRef.beginEdit (editName (shape));

        if (shape == IlanaSynthAudioProcessor::curveShape)
            curveMouseDown (event);
        else if (shape == 6)
            setCustomPoint (event.position);
        else if (shape == 7)
            setStepPoint (event.position);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto shape = (int) readParam ("_shape");

        if (shape == IlanaSynthAudioProcessor::curveShape)
            curveMouseDrag (event);
        else if (shape == 6)
            setCustomPoint (event.position);
        else if (shape == 7)
            setStepPoint (event.position);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if ((int) readParam ("_shape") != IlanaSynthAudioProcessor::curveShape)
            return;

        processorRef.performEdit (editName (IlanaSynthAudioProcessor::curveShape), [this, &event]
        {
            curve = processorRef.getLfoCurve (index);
            const auto hit = hitPoint (event.position);

            // Double-click removes a point (the ends stay) or straightens a segment.
            if (hit > 0 && hit < (int) curve.points.size() - 1)
            {
                curve.points.erase (curve.points.begin() + hit);
                commitCurve();
            }
            else if (const auto segment = hitTension (event.position); segment >= 0)
            {
                curve.points[(size_t) segment].tension = 0.0f;
                commitCurve();
            }
        });
    }

    // The undo step's name for an edit of this LFO's shape.
    juce::String editName (int shape) const
    {
        return "LFO " + juce::String (index + 1) + (shape == IlanaSynthAudioProcessor::curveShape ? " curve"
                                                    : shape == 6 ? " drawing" : " steps");
    }

    void lookAndFeelChanged() override
    {
        if (followsTheme)
            traceColour = IlanaTheme::accent();
    }

private:
    // ---- Curve shape editing --------------------------------------------

    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (10.0f, 14.0f); }

    juce::Point<float> pointToScreen (const LfoCurve::Point& point) const
    {
        const auto plot = plotArea();
        return { plot.getX() + point.x * plot.getWidth(), plot.getCentreY() - point.y * plot.getHeight() * 0.42f };
    }

    // The little dot halfway along a segment that bends it.
    juce::Point<float> tensionHandle (int segment) const
    {
        const auto& a = curve.points[(size_t) segment];
        const auto& b = curve.points[(size_t) segment + 1];
        const auto plot = plotArea();
        const auto midX = 0.5f * (a.x + b.x);
        const auto y = a.y + (b.y - a.y) * LfoCurve::bend (0.5f, a.tension);
        return { plot.getX() + midX * plot.getWidth(), plot.getCentreY() - y * plot.getHeight() * 0.42f };
    }

    int hitPoint (juce::Point<float> position) const
    {
        for (int i = 0; i < (int) curve.points.size(); ++i)
            if (pointToScreen (curve.points[(size_t) i]).getDistanceFrom (position) < 8.0f)
                return i;

        return -1;
    }

    int hitTension (juce::Point<float> position) const
    {
        for (int i = 0; i + 1 < (int) curve.points.size(); ++i)
            if (curve.points[(size_t) i + 1].x - curve.points[(size_t) i].x > 0.02f
                && tensionHandle (i).getDistanceFrom (position) < 7.0f)
                return i;

        return -1;
    }

    float snapX (float x) const
    {
        return gridDivisions > 0 ? std::round (x * (float) gridDivisions) / (float) gridDivisions : x;
    }

    float snapY (float y) const
    {
        return gridDivisions > 0 ? std::round (y * 4.0f) / 4.0f : y;
    }

    void curveMouseDown (const juce::MouseEvent& event)
    {
        curve = processorRef.getLfoCurve (index);
        dragPoint = -1;
        dragTension = -1;

        if (event.mods.isPopupMenu())
        {
            showCurveMenu();
            return;
        }

        dragPoint = hitPoint (event.position);

        if (dragPoint < 0)
            dragTension = hitTension (event.position);

        if (dragPoint < 0 && dragTension < 0)
        {
            // Empty space: add a point there and start dragging it.
            const auto plot = plotArea();
            LfoCurve::Point point;
            point.x = snapX (juce::jlimit (0.001f, 0.999f, (event.position.x - plot.getX()) / plot.getWidth()));
            point.y = snapY (valueFromY (event.position.y, plot));

            auto insertAt = curve.points.begin();

            while (insertAt != curve.points.end() && insertAt->x <= point.x)
                ++insertAt;

            dragPoint = (int) std::distance (curve.points.begin(), curve.points.insert (insertAt, point));
            commitCurve();
        }

        if (dragTension >= 0)
            dragStartTension = curve.points[(size_t) dragTension].tension;
    }

    void curveMouseDrag (const juce::MouseEvent& event)
    {
        const auto plot = plotArea();

        if (dragPoint >= 0 && dragPoint < (int) curve.points.size())
        {
            auto& point = curve.points[(size_t) dragPoint];
            const auto last = (int) curve.points.size() - 1;

            if (dragPoint > 0 && dragPoint < last)
            {
                const auto low = curve.points[(size_t) dragPoint - 1].x;
                const auto high = curve.points[(size_t) dragPoint + 1].x;
                point.x = juce::jlimit (low, high, snapX ((event.position.x - plot.getX()) / plot.getWidth()));
            }

            point.y = snapY (valueFromY (event.position.y, plot));
            commitCurve();
        }
        else if (dragTension >= 0)
        {
            // Dragging towards the segment's end makes it arrive late.
            const auto& a = curve.points[(size_t) dragTension];
            const auto& b = curve.points[(size_t) dragTension + 1];
            const auto direction = b.y >= a.y ? 1.0f : -1.0f;
            const auto delta = (float) event.getDistanceFromDragStartY() * 0.012f * direction;
            curve.points[(size_t) dragTension].tension = juce::jlimit (-1.0f, 1.0f, dragStartTension + delta);
            commitCurve();
        }
    }

    void commitCurve()
    {
        processorRef.setLfoCurve (index, curve);
        curve = processorRef.getLfoCurve (index);
        repaint();
    }

    void showCurveMenu()
    {
        juce::PopupMenu shapes;
        const auto names = LfoCurve::getPresetNames();

        for (int i = 0; i < names.size(); ++i)
            shapes.addItem (100 + i, names[i]);

        juce::PopupMenu grid;

        for (const auto divisions : { 0, 4, 8, 16, 32 })
            grid.addItem (200 + divisions, divisions == 0 ? juce::String ("Off") : juce::String (divisions),
                          true, gridDivisions == divisions);

        juce::PopupMenu menu;
        menu.addSubMenu ("Load shape", shapes);
        menu.addSubMenu ("Snap to grid", grid);
        menu.addSeparator();
        menu.addItem (300, "Flip vertically");
        menu.addItem (301, "Reverse");

        juce::Component::SafePointer<LfoDisplay> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safeThis] (int result)
                            {
                                if (safeThis == nullptr || result == 0)
                                    return;

                                auto& self = *safeThis;

                                if (result >= 200 && result < 300)
                                {
                                    self.gridDivisions = result - 200;
                                    self.repaint();
                                    return;
                                }

                                self.processorRef.beginEdit (self.editName (IlanaSynthAudioProcessor::curveShape));
                                self.curve = self.processorRef.getLfoCurve (self.index);

                                if (result >= 100 && result < 200)
                                    self.curve = LfoCurve::preset (result - 100);
                                else if (result == 300)
                                    for (auto& point : self.curve.points)
                                        point.y = -point.y;
                                else if (result == 301)
                                {
                                    for (auto& point : self.curve.points)
                                    {
                                        point.x = 1.0f - point.x;
                                        point.tension = -point.tension;
                                    }

                                    std::reverse (self.curve.points.begin(), self.curve.points.end());

                                    // Tension belongs to the segment's start point,
                                    // which moved one place along.
                                    for (size_t i = 0; i + 1 < self.curve.points.size(); ++i)
                                        self.curve.points[i].tension = self.curve.points[i + 1].tension;
                                }

                                self.commitCurve();
                                self.processorRef.endEdit();
                            });
    }

    void paintCurveGrid (juce::Graphics& g, juce::Rectangle<float> plot, float centreY, float halfHeight) const
    {
        if (gridDivisions <= 0)
            return;

        g.setColour (juce::Colours::white.withAlpha (0.05f));

        for (int i = 1; i < gridDivisions; ++i)
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (plot.getX() + plot.getWidth() * (float) i / (float) gridDivisions,
                                                                                     plot.getY()));

        for (const auto level : { -1.0f, -0.5f, 0.5f, 1.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), centreY - level * halfHeight));
    }

    void paintCurveHandles (juce::Graphics& g, juce::Rectangle<float>, float, float) const
    {
        for (int i = 0; i + 1 < (int) curve.points.size(); ++i)
        {
            if (curve.points[(size_t) i + 1].x - curve.points[(size_t) i].x <= 0.02f)
                continue;

            g.setColour (traceColour.withAlpha (0.8f));
            g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (tensionHandle (i)), 1.2f);
        }

        for (const auto& point : curve.points)
        {
            const auto centre = pointToScreen (point);
            g.setColour (traceColour.withAlpha (0.35f));
            g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (centre));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (centre));
        }

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.drawText ("click: add   drag: move   dot on a line: bend   double-click: delete   right-click: shapes / grid",
                    getLocalBounds().reduced (10, 2).removeFromBottom (12), juce::Justification::centredLeft);
    }

    // Simulated physics output across physicsCycles cycles, one value per
    // pixel, for the current shape and PHYSICS A / B. Rebuilt on change.
    const std::vector<float>& physicsTrace (int shape, int width)
    {
        const auto a = readParam ("_phys_a");
        const auto b = readParam ("_phys_b");
        width = juce::jmax (2, width);

        if (shape != traceShape || a != traceA || b != traceB || width != (int) physicsValues.size())
        {
            traceShape = shape;
            traceA = a;
            traceB = b;
            physicsValues.assign ((size_t) width, 0.0f);

            // The physics run in cycles, so any rate gives the same picture.
            constexpr int stepsPerCycle = 2048;
            const auto totalSteps = stepsPerCycle * physicsCycles;
            LfoChaos state;
            state.resetPhysics (shape, a);
            state.physicsOut = juce::jlimit (-1.0f, 1.0f, state.physicsOut);

            for (int step = 0, x = 0; step <= totalSteps && x < width; ++step)
            {
                if (step * (width - 1) >= x * totalSteps)
                    physicsValues[(size_t) x++] = state.physicsOut;

                state.advancePhysics (shape, 1.0 / stepsPerCycle, a, b);
            }
        }

        return physicsValues;
    }

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

        processorRef.endEdit();
    }

    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.12f * frameTicks());

        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto elapsed = juce::jlimit (0.0, 0.2, (now - lastTimerMs) / 1000.0);
        lastTimerMs = now;
        const auto simulating = isVisible() && LfoSimShapes::isSim ((int) readParam ("_shape"));
        if (simulating)
            simPreview.advance (processorRef.readLfoSimSettings (index), currentRate(), elapsed);

        if (isShowing() && (simulating || appear < 1.0f || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this) ^ IlanaAnim::phaseSignature (processorRef.getLfoPhase (index), index))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    // RATE in Hz, following SYNC at the host tempo.
    double currentRate() const
    {
        if (readParam ("_sync") > 0.5f)
        {
            static const double beats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.75, 0.375 };
            return (processorRef.getCurrentBpm() / 60.0) / beats[juce::jlimit (0, 10, (int) readParam ("_div"))];
        }
        return (double) readParam ("_rate");
    }

public:
    void triggerPreview() { simPreview.trigger (processorRef.readLfoSimSettings (index)); }

private:
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
    LfoCurve curve;
    int dragPoint = -1;
    int dragTension = -1;
    float dragStartTension = 0.0f;
    int gridDivisions = 8;
    std::array<float, 16> sampleHoldPreview {};
    static constexpr int physicsCycles = 3;
    std::vector<float> physicsValues;
    int traceShape = -1;
    float traceA = -1.0f, traceB = -1.0f;
    double lastPhase = 0.0;
    int cycleCount = 0;
    LfoSimPreview simPreview;
    double lastTimerMs = juce::Time::getMillisecondCounterHiRes();
};
