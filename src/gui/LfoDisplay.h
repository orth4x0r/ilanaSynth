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
        setTooltip ("LFO shape\nDrag any wave (Sine, Triangle, Saw...) to turn it into an MSEG with the same points and edit it.  "
                    "Draw: drag to draw.  Steps: drag across the steps to set them.  MSEG: click to add points, drag them, "
                    "drag the dot on a line to curve it, double-click to delete.  GRID (top right) sets the snap; "
                    "right-click for shapes.  The value shows while you drag; the ruler under the graph is one cycle.");

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
        IlanaAnim::countPaint ("lfo");
        const auto bounds = getLocalBounds().toFloat();

        g.setOpacity (juce::jlimit (0.0f, 1.0f, appear));
        g.addTransform (juce::AffineTransform::translation (0.0f, (1.0f - juce::jlimit (0.0f, 1.0f, appear)) * 10.0f));

        if (appear < 0.999f)
        {
            g.setColour (traceColour.withAlpha (0.12f * (1.0f - appear)));
            g.fillRoundedRectangle (bounds.expanded (4.0f), 8.0f);
        }

        IlanaTheme::paintWell (g, wellArea(), 6.0f);

        if (const auto simShape = (int) readParam ("_shape"); LfoSimShapes::isSim (simShape))
        {
            simPreview.paint (g, bounds.reduced (10.0f, 8.0f), processorRef.readLfoSimSettings (index), traceColour);
            return;
        }

        if ((int) readParam ("_shape") == LfoShapes::Steps)
        {
            paintSteps (g, wellArea());
            return;
        }

        const auto plot = plotArea();
        if (! LfoShapes::isPhysics ((int) readParam ("_shape")))
            paintTimeRuler (g, plot);
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

        auto phase = (double) phaseSmoother.get (processorRef.getLfoPhase (index), true);
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

        // The value scale and the live reading (N16-4: one line in a big well):
        // +1 / 0 / -1 against the centre line, and where the dot is now.
        if (plot.getHeight() > 120.0f)
        {
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            for (const auto [level, name] : { std::pair<float, const char*> { 1.0f, "+1" }, { 0.0f, "0" }, { -1.0f, "-1" } })
            {
                const auto y = centreY - level * halfHeight;
                if (level != 0.0f)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.05f));
                    g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre ({ plot.getCentreX(), y }));
                }
                g.setColour (IlanaTheme::Ui::text3.withAlpha (0.8f));
                g.drawText (name, juce::Rectangle<float> (plot.getX() - 8.0f, y - 12.0f, 24.0f, 11.0f), juce::Justification::centredLeft);
            }
            const auto readout = juce::String ("PHASE ") + juce::String (phase - std::floor (phase), 2) + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  VALUE "))
                                 + (value >= 0.0f ? "+" : "") + juce::String (value, 2);
            g.setColour (traceColour.interpolatedWith (juce::Colours::white, 0.35f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            IlanaTheme::drawFitted (g, readout, juce::Rectangle<float> (wellArea().getX() + 12.0f, wellArea().getY() + 4.0f, 190.0f, 12.0f).toNearestInt(),
                                    juce::Justification::centredLeft, 1);
        }

        if (shape == IlanaSynthAudioProcessor::curveShape)
        {
            paintCurveHandles (g, plot, centreY, halfHeight);

            // The dragged point's place, or the segment's curve, beside it.
            if (dragPoint >= 0 && dragPoint < (int) curve.points.size() && isMouseButtonDown())
            {
                const auto& point = curve.points[(size_t) dragPoint];
                paintReadout (g, pointToScreen (point), "POINT  " + juce::String (juce::roundToInt (point.x * 100.0f)) + "%  "
                                                            + (point.y >= 0.0f ? "+" : "") + juce::String (point.y, 2), plot);
            }
            else if (dragTension >= 0 && dragTension + 1 < (int) curve.points.size() && isMouseButtonDown())
            {
                const auto tension = curve.points[(size_t) dragTension].tension;
                paintReadout (g, tensionHandle (dragTension), "CURVE  " + juce::String (tension >= 0.0f ? "+" : "") + juce::String (tension, 2), plot);
            }
        }

        // The snap grid's own control, on the graph wherever a drag makes
        // (or will make) a curve.
        if (shape == IlanaSynthAudioProcessor::curveShape || convertsToCurve (shape))
        {
            // A menu, not a label: the pill ends in a chevron (V10-20).
            const auto chip = gridChipBounds();
            IlanaTheme::paintPill (g, chip.withTrimmedRight (6.0f), gridDivisions > 0 ? "GRID " + juce::String (gridDivisions) : juce::String ("GRID OFF"),
                                   traceColour, shape == IlanaSynthAudioProcessor::curveShape && gridDivisions > 0,
                                   chip.contains (getMouseXYRelative().toFloat()) ? 1.0f : 0.0f);
            juce::Path chevron;
            const auto at = juce::Point<float> (chip.getRight() - 9.0f, chip.getCentreY());
            chevron.startNewSubPath (at.x - 3.0f, at.y - 1.5f);
            chevron.lineTo (at.x, at.y + 1.5f);
            chevron.lineTo (at.x + 3.0f, at.y - 1.5f);
            g.setColour (IlanaTheme::Ui::text2);
            g.strokePath (chevron, juce::PathStrokeType (1.3f));
        }

        // What a drag does on a preset wave, and what just happened after one.
        juce::String hint;
        auto hintAlpha = 0.0f;

        if (convertHint > 0.0f)
        {
            hint = "now an MSEG with the same points (undo puts the " + convertedFrom + " back)";
            hintAlpha = juce::jmin (1.0f, convertHint * 2.0f);
        }
        else if (convertsToCurve (shape) && isMouseOver())
        {
            hint = "drag to edit: the wave becomes an MSEG";
            hintAlpha = 0.8f;
        }

        if (hint.isNotEmpty())
        {
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, hint) + 16.0f;
            const auto box = juce::Rectangle<float> (width, 16.0f).withCentre ({ bounds.getCentreX(), bounds.getY() + 12.0f });
            g.setColour (juce::Colours::black.withAlpha (0.55f * hintAlpha));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (traceColour.interpolatedWith (juce::Colours::white, 0.3f).withAlpha (hintAlpha));
            g.setFont (font);
            g.drawText (hint, box, juce::Justification::centred);
        }
    }

    // Preset waves turn into a Curve on the first drag. Draw and Steps edit
    // in place already, and the simulated shapes aren't a drawable cycle.
    static bool convertsToCurve (int shape)
    {
        return shape >= 0 && shape < LfoShapes::Count && shape != LfoShapes::Draw && shape != LfoShapes::Steps
               && shape != LfoShapes::Curve;
    }

    // The points a preset wave becomes as a Curve: the matching curve
    // preset where there is one, otherwise the picture on screen sampled.
    LfoCurve curveFromShape (int shape)
    {
        switch (shape)
        {
            case LfoShapes::Sine:     return LfoCurve::preset (0);
            case LfoShapes::Triangle: return LfoCurve::preset (1);
            case LfoShapes::SawUp:    return LfoCurve::preset (2);
            case LfoShapes::SawDown:  return LfoCurve::preset (3);
            case LfoShapes::Square:   return LfoCurve::preset (4);
            default: break;
        }

        LfoCurve curve;
        curve.points.clear();

        if (shape == LfoShapes::SampleHold)
        {
            // The eight held values the graph shows, as flat stairs.
            for (int step = 0; step < 8; ++step)
            {
                const auto value = sampleHoldPreview[(size_t) step];
                curve.points.push_back ({ (float) step / 8.0f, value, 0.0f });
                curve.points.push_back ({ (float) (step + 1) / 8.0f - 0.001f, value, 0.0f });
            }
            curve.points.push_back ({ 1.0f, sampleHoldPreview[0], 0.0f });
        }
        else
        {
            // The random and physics pictures, 16 points across the cycle
            // (the physics trace's first cycle).
            constexpr int points = 16;
            const auto physics = LfoShapes::isPhysics (shape);
            const std::vector<float> trace = physics ? physicsTrace (shape, juce::jmax (48, (int) plotArea().getWidth()))
                                                     : std::vector<float> (1, 0.0f);

            for (int i = 0; i <= points; ++i)
            {
                const auto phase = (double) i / (double) points;
                const auto value = physics ? trace[(size_t) juce::jlimit (0, (int) trace.size() - 1,
                                                                          (int) (phase / (double) physicsCycles * (double) (trace.size() - 1)))]
                                           : lfoShapeValue (shape, juce::jmin (0.9999, phase));
                curve.points.push_back ({ (float) phase, juce::jlimit (-1.0f, 1.0f, value), 0.0f });
            }
        }

        curve.sanitise();
        return curve;
    }

    // Sets this LFO's SHAPE (inside whatever undo step is open).
    void setShape (int shape)
    {
        if (auto* parameter = processorRef.apvts.getParameter ("lfo" + juce::String (index + 1) + "_shape"))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) shape));
            parameter->endChangeGesture();
        }
    }

    // A preset wave becomes a Curve seeded with `seed`, and the graph says so.
    void convertToCurve (const LfoCurve& seed)
    {
        const auto shape = (int) readParam ("_shape");
        const juce::StringArray names { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" };
        convertedFrom = juce::isPositiveAndBelow (shape, names.size()) ? names[shape] : juce::String ("shape");
        processorRef.setLfoCurve (index, seed);
        setShape (IlanaSynthAudioProcessor::curveShape);
        curve = processorRef.getLfoCurve (index);
        convertHint = 1.6f;
        repaint();
    }

    juce::Rectangle<float> gridChipBounds() const
    {
        return getLocalBounds().toFloat().removeFromTop (22.0f).removeFromRight (84.0f).reduced (6.0f, 3.0f);
    }

    void showGridMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Snap to grid");

        for (const auto divisions : { 0, 4, 8, 16, 32 })
            menu.addItem (200 + divisions, divisions == 0 ? juce::String ("Off") : juce::String (divisions) + " columns",
                          true, gridDivisions == divisions);

        juce::Component::SafePointer<LfoDisplay> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safeThis] (int result)
        {
            if (safeThis != nullptr && result >= 200)
            {
                safeThis->gridDivisions = result - 200;
                safeThis->repaint();
            }
        });
    }

public:
    // For the UI test: the snap grid's column count (0 = off).
    int getGridDivisions() const { return gridDivisions; }
    void setGridDivisions (int divisions) { gridDivisions = divisions; repaint(); }

private:
    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto shape = (int) readParam ("_shape");
        pendingConvert = false;
        pendingOutputDrag = -1;

        // A simulated shape's output tags drag its outputs onto knobs.
        if (LfoSimShapes::isSim (shape))
        {
            pendingOutputDrag = outputTagAt (event.position);
            return;
        }

        if ((shape == IlanaSynthAudioProcessor::curveShape || convertsToCurve (shape)) && gridChipBounds().contains (event.position))
        {
            showGridMenu();
            return;
        }

        if (convertsToCurve (shape))
        {
            if (event.mods.isPopupMenu())
            {
                showCurveMenu (true);
                return;
            }

            // The first drag converts; a plain click changes nothing.
            processorRef.beginEdit (editName (IlanaSynthAudioProcessor::curveShape));
            pendingConvert = true;
            downPosition = event.position;
            return;
        }

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
        if (pendingOutputDrag >= 0)
        {
            if (event.getDistanceFromDragStart() >= 4)
                if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this); container != nullptr && ! container->isDragAndDropActive())
                {
                    const auto source = pendingOutputDrag == 1 ? Mod::lfoBSourceFor (index) : Mod::lfoSourceFor (index);
                    pendingOutputDrag = -1;
                    container->startDragging ("modsource:" + juce::String ((int) source), this);
                }
            return;
        }

        if (pendingConvert)
        {
            // Seeded with the wave's points, then this drag edits the curve
            // from where it started: grabbing a corner moves it.
            pendingConvert = false;
            convertToCurve (curveFromShape ((int) readParam ("_shape")));
            curvePress (downPosition);
        }

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

    // The well frames the graph; the ruler (one cycle's time, or the step
    // numbers) sits under it, outside the frame, as on the envelope graphs
    // (UI review 7, S7-26 / I7-29). A simulated shape draws its own scope
    // in the whole area.
    static constexpr float rulerHeight = 14.0f;

    juce::Rectangle<float> wellArea() const
    {
        const auto bounds = getLocalBounds().toFloat();
        return LfoSimShapes::isSim ((int) readParam ("_shape")) ? bounds : bounds.withTrimmedBottom (rulerHeight + 2.0f);
    }

    juce::Rectangle<float> plotArea() const { return wellArea().reduced (10.0f, 14.0f); }

    // One cycle's ruler under the graph: its time at RATE in Hz, or note
    // values while SYNC is on, at the quarters.
    void paintTimeRuler (juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        const auto rulerY = (float) getHeight() - rulerHeight;
        const auto synced = readParam ("_sync") > 0.5f;
        juce::StringArray labels;

        if (synced)
        {
            const auto* division = processorRef.apvts.getParameter ("lfo" + juce::String (index + 1) + "_div");
            const auto name = division != nullptr ? division->getCurrentValueAsText() : juce::String ("1/4");
            const auto denominator = name.fromFirstOccurrenceOf ("/", false, false).getIntValue();
            const auto plain = name.startsWith ("1/") && denominator > 0 && name.containsOnly ("0123456789/");
            for (int quarter = 0; quarter <= 4; ++quarter)
            {
                if (quarter == 0)
                    labels.add ("0");
                else if (quarter == 4)
                    labels.add (name);
                else if (plain)
                {
                    // quarter / (4 * denominator), reduced.
                    auto top = quarter, bottom = 4 * denominator;
                    while (top % 2 == 0 && bottom % 2 == 0)
                        top /= 2, bottom /= 2;
                    labels.add (juce::String (top) + "/" + juce::String (bottom));
                }
                else
                    labels.add (quarter == 2 ? juce::String (juce::CharPointer_UTF8 ("\xc2\xbd")) : juce::String());
            }
        }
        else
        {
            const auto rate = juce::jmax (0.001f, readParam ("_rate"));
            for (int quarter = 0; quarter <= 4; ++quarter)
            {
                const auto seconds = (double) quarter * 0.25 / (double) rate;
                labels.add (quarter == 0 ? juce::String ("0 ms")
                                         : seconds < 1.0 ? juce::String (juce::roundToInt (seconds * 1000.0)) + " ms"
                                                         : juce::String (seconds, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " s");
            }
        }

        const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny);
        g.setFont (font);
        auto lastRight = -1.0e9f;
        for (int quarter = 0; quarter <= 4; ++quarter)
        {
            const auto x = plot.getX() + plot.getWidth() * (float) quarter / 4.0f;
            g.setColour (juce::Colours::white.withAlpha (quarter == 0 || quarter == 4 ? 0.0f : 0.04f));
            g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (x, plot.getY()));
            const auto& text = labels[quarter];
            if (text.isEmpty())
                continue;
            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 4.0f;
            const auto left = juce::jlimit (0.0f, (float) getWidth() - width, x - width * 0.5f);
            if (left < lastRight + 6.0f)
                continue;
            lastRight = left + width;
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillRect (juce::Rectangle<float> (1.0f, 3.0f).withPosition (x, rulerY - 1.0f));
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText (text, juce::Rectangle<float> (left, rulerY + 2.0f, width, 12.0f), juce::Justification::centred);
        }
    }

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
            showCurveMenu (false);
            return;
        }

        curvePress (event.position);
    }

    // Grabs the point or bend handle under `position`, or adds a point there.
    void curvePress (juce::Point<float> position)
    {
        curve = processorRef.getLfoCurve (index);
        dragPoint = hitPoint (position);
        dragTension = dragPoint < 0 ? hitTension (position) : -1;

        if (dragPoint < 0 && dragTension < 0)
        {
            // Empty space: add a point there and start dragging it.
            const auto plot = plotArea();
            LfoCurve::Point point;
            point.x = snapX (juce::jlimit (0.001f, 0.999f, (position.x - plot.getX()) / plot.getWidth()));
            point.y = snapY (valueFromY (position.y, plot));

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

    // The shape menu. On a preset wave (`converting`) every edit first
    // turns it into a Curve with the wave's points, in the same undo step.
    void showCurveMenu (bool converting)
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

        if (converting)
            menu.addItem (400, "Edit as an MSEG (same points)");

        menu.addSubMenu ("Load shape", shapes);
        menu.addSubMenu ("Snap to grid", grid);
        menu.addSeparator();
        menu.addItem (300, "Flip vertically");
        menu.addItem (301, "Reverse");
        menu.addSeparator();
        menu.addItem (401, "Use 16 steps (Steps shape)");

        juce::Component::SafePointer<LfoDisplay> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safeThis, converting] (int result)
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

                                if (result == 401)
                                {
                                    self.processorRef.performEdit ("LFO " + juce::String (self.index + 1) + " shape",
                                                                   [&self] { self.setShape (LfoShapes::Steps); });
                                    self.repaint();
                                    return;
                                }

                                self.processorRef.beginEdit (self.editName (IlanaSynthAudioProcessor::curveShape));

                                if (converting)
                                    self.convertToCurve (self.curveFromShape ((int) self.readParam ("_shape")));

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
        g.drawText ("click: add   drag: move   dot on a line: curve   double-click: delete   right-click: shapes",
                    wellArea().toNearestInt().reduced (10, 2).removeFromBottom (12), juce::Justification::centredLeft);
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
        const auto plot = plotArea();

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
        const auto plot = plotArea();

        if (plot.getWidth() <= 1.0f)
            return;

        const auto step = juce::jlimit (0, 15, (int) ((position.x - plot.getX()) / plot.getWidth() * 16.0f));
        draggedStep = step;

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

    // Steps (UI review 6, V6-20 / I6-20: the LFO's own step editor, no
    // second copy on another page): sixteen bars from the centre line, the
    // one playing lit, numbered every four, the dragged one's value beside it.
    void paintSteps (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        const auto plot = bounds.reduced (10.0f, 14.0f);
        const auto centreY = plot.getCentreY();
        const auto halfHeight = plot.getHeight() * 0.42f;
        const auto stepWidth = plot.getWidth() / 16.0f;
        const auto playing = juce::jlimit (0, 15, (int) (phaseSmoother.get (processorRef.getLfoPhase (index), true) * 16.0f));
        // The step under the mouse lights as well, as an arp lane's does.
        const auto mouse = getMouseXYRelative().toFloat();
        const auto hovered = isMouseOver() && plot.getWidth() > 1.0f
                                 ? juce::jlimit (0, 15, (int) ((mouse.x - plot.getX()) / plot.getWidth() * 16.0f)) : -1;

        g.setColour (juce::Colours::white.withAlpha (0.04f));
        for (const auto level : { -1.0f, -0.5f, 0.5f, 1.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), centreY - level * halfHeight));
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre ({ plot.getCentreX(), centreY }));

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));

        for (int step = 0; step < 16; ++step)
        {
            const auto value = readParam (("_step" + juce::String (step + 1)).toRawUTF8());
            const auto x = plot.getX() + (float) step * stepWidth;
            const auto y = centreY - value * halfHeight;
            const auto lit = step == playing || step == draggedStep || step == hovered;

            g.setColour (traceColour.withAlpha (lit ? 0.95f : 0.6f));
            g.fillRect (juce::Rectangle<float> (x + 1.5f, juce::jmin (y, centreY), juce::jmax (1.0f, stepWidth - 3.0f),
                                                juce::jmax (1.5f, std::abs (y - centreY))));

            if (step > 0)
            {
                g.setColour (juce::Colours::white.withAlpha (step % 4 == 0 ? 0.14f : 0.05f));
                g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withPosition (x, plot.getY()));
            }

            // Every step numbered on the ruler under the graph, as the arp's
            // lanes are (UI review 7, I7-29); every fourth where they crowd.
            if (stepWidth >= 16.0f || step % 4 == 0)
            {
                g.setColour (step % 4 == 0 || lit ? IlanaTheme::Ui::text2 : IlanaTheme::Ui::text3);
                g.drawText (juce::String (step + 1), juce::Rectangle<float> (x, (float) getHeight() - rulerHeight + 2.0f, stepWidth, 12.0f),
                            juce::Justification::centred);
            }
        }

        if (draggedStep >= 0)
        {
            const auto value = readParam (("_step" + juce::String (draggedStep + 1)).toRawUTF8());
            paintReadout (g, { plot.getX() + ((float) draggedStep + 0.5f) * stepWidth, centreY - value * halfHeight },
                          "STEP " + juce::String (draggedStep + 1) + "  " + (value >= 0.0f ? "+" : "") + juce::String (value, 2), plot);
        }
    }

    // A value shown beside what is being dragged (every curve editor does
    // this the same way).
    void paintReadout (juce::Graphics& g, juce::Point<float> anchor, const juce::String& text, juce::Rectangle<float> area) const
    {
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
        const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 14.0f;
        auto box = juce::Rectangle<float> (width, 17.0f).withCentre ({ anchor.x, anchor.y - 18.0f });
        if (box.getY() < area.getY())
            box.setY (anchor.y + 10.0f);
        box.setX (juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - width), box.getX()));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (traceColour.interpolatedWith (juce::Colours::white, 0.3f));
        g.setFont (font);
        g.drawText (text, box, juce::Justification::centred);
    }

    // Which output tag (0 A, 1 B) of a simulated shape's scope is under
    // `position`, or -1.
    int outputTagAt (juce::Point<float> position) const
    {
        const auto settings = processorRef.readLfoSimSettings (index);
        const auto& info = LfoSimInfo::get (settings.shape);
        const auto scope = LfoSimPreview::scopeArea (getLocalBounds().toFloat().reduced (10.0f, 8.0f), settings.shape);

        for (const auto outputB : { false, true })
            if (LfoSimPreview::outputTagBounds (scope, LfoSimPreview::outputTagText (info, settings.axis, outputB), outputB).contains (position))
                return outputB ? 1 : 0;

        return -1;
    }

public:
    // For the UI test: where a simulated shape's output tag sits (0 A, 1 B).
    juce::Rectangle<float> getOutputTagBounds (int output) const
    {
        const auto settings = processorRef.readLfoSimSettings (index);
        const auto scope = LfoSimPreview::scopeArea (getLocalBounds().toFloat().reduced (10.0f, 8.0f), settings.shape);
        return LfoSimPreview::outputTagBounds (scope, LfoSimPreview::outputTagText (LfoSimInfo::get (settings.shape), settings.axis, output == 1),
                                               output == 1);
    }

private:
    static float valueFromY (float y, juce::Rectangle<float> plot)
    {
        const auto halfHeight = juce::jmax (1.0f, plot.getHeight() * 0.42f);
        return juce::jlimit (-1.0f, 1.0f, (plot.getCentreY() - y) / halfHeight);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        pendingConvert = false;
        pendingOutputDrag = -1;
        draggedStep = -1;

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
        const auto hinting = convertHint > 0.0f;
        convertHint = juce::jmax (0.0f, convertHint - 0.033f * frameTicks());

        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto elapsed = juce::jlimit (0.0, 0.2, (now - lastTimerMs) / 1000.0);
        lastTimerMs = now;
        const auto simulating = isVisible() && LfoSimShapes::isSim ((int) readParam ("_shape"));
        if (simulating)
            simPreview.advance (processorRef.readLfoSimSettings (index), currentRate(), elapsed);

        if (IlanaAnim::showing (*this) && (simulating || hinting || appear < 1.0f || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this) ^ IlanaAnim::phaseSignature (phaseSmoother.get (processorRef.getLfoPhase (index), true), index))))
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
    bool pendingConvert = false;       // a press on a preset wave, converted on the first drag
    int pendingOutputDrag = -1;        // a press on a simulated shape's output tag (0 A, 1 B)
    int draggedStep = -1;              // the step being set, for its readout
    juce::Point<float> downPosition;
    float convertHint = 0.0f;          // the "now a Curve" note, fading
    juce::String convertedFrom;
    std::array<float, 16> sampleHoldPreview {};
    static constexpr int physicsCycles = 3;
    std::vector<float> physicsValues;
    int traceShape = -1;
    float traceA = -1.0f, traceB = -1.0f;
    double lastPhase = 0.0;
    int cycleCount = 0;
    LfoSimPreview simPreview;
    IlanaAnim::BlockSmoother phaseSmoother; // the published phase, glided between audio blocks
    double lastTimerMs = juce::Time::getMillisecondCounterHiRes();
};
