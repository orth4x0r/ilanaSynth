#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../dsp/Generative.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// M7.1: the Generative card's displays.

namespace GenerativeWidgets
{
inline float read (const IlanaSynthAudioProcessor& processor, const juce::String& id)
{
    if (const auto* value = processor.apvts.getRawParameterValue (id))
        return value->load();

    return 0.0f;
}

// Sets a parameter from the editor as one undoable gesture.
inline void write (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
    {
        const auto normalised = parameter->convertTo0to1 (value);

        if (std::abs (normalised - parameter->getValue()) < 1.0e-6f)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (normalised);
        parameter->endChangeGesture();
    }
}
} // namespace GenerativeWidgets

// The Euclidean rhythm as a ring: one dot per step, hits lit, the step
// playing now ringed, and E(hits, steps) in the middle. Drag up or down on
// the ring to change the hits; drag sideways to rotate.
class EuclidDisplay : public juce::Component,
                      public juce::SettableTooltipClient,
                      private IlanaAnim::FrameTimer
{
public:
    EuclidDisplay (IlanaSynthAudioProcessor& processor, juce::Colour colourIn)
        : processorRef (processor), colour (colourIn)
    {
        setTooltip ("Euclidean rhythm. Drag up or down to change the hits, sideways to rotate.");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        using GenerativeWidgets::read;
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto on = read (processorRef, "euc_on") > 0.5f;
        const auto steps = juce::jlimit (2, 32, juce::roundToInt (read (processorRef, "euc_steps")));
        const auto hits = juce::jlimit (0, 32, juce::roundToInt (read (processorRef, "euc_hits")));
        const auto rotate = juce::jlimit (0, 31, juce::roundToInt (read (processorRef, "euc_rotate")));
        const auto current = processorRef.getEuclidDisplayStep();

        // The ring on the left, a flat strip of the same steps on the right.
        auto area = bounds.reduced (8.0f, 6.0f);
        const auto ringArea = area.removeFromLeft (juce::jmin (area.getHeight(), area.getWidth() * 0.42f));
        area.removeFromLeft (12.0f);

        const auto centre = ringArea.getCentre();
        const auto radius = ringArea.getHeight() * 0.5f - 7.0f;
        const auto dot = juce::jlimit (3.0f, 7.0f, radius * 6.0f / (float) steps);

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.0f);

        // The polygon joining the hits.
        juce::Path polygon;
        auto first = true;

        for (int step = 0; step < steps; ++step)
        {
            if (! euclidHit (step, hits, steps, rotate))
                continue;

            const auto point = pointOnRing (centre, radius, step, steps);

            if (first)
                polygon.startNewSubPath (point);
            else
                polygon.lineTo (point);

            first = false;
        }

        if (! first)
        {
            polygon.closeSubPath();
            g.setColour (colour.withAlpha (on ? 0.12f : 0.05f));
            g.fillPath (polygon);
            g.setColour (colour.withAlpha (on ? 0.5f : 0.2f));
            g.strokePath (polygon, juce::PathStrokeType (1.2f));
        }

        for (int step = 0; step < steps; ++step)
        {
            const auto point = pointOnRing (centre, radius, step, steps);
            const auto hit = euclidHit (step, hits, steps, rotate);
            const auto playing = on && step == current;
            const auto circle = juce::Rectangle<float> (dot * 2.0f, dot * 2.0f).withCentre (point);

            if (playing)
            {
                g.setColour (colour.withAlpha (hit ? 0.4f : 0.2f));
                g.fillEllipse (circle.expanded (dot * 0.9f));
            }

            g.setColour (hit ? colour.withAlpha (on ? 1.0f : 0.45f) : juce::Colours::white.withAlpha (0.12f));
            g.fillEllipse (circle);

            if (step == 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.drawEllipse (circle.expanded (2.0f), 1.0f);
            }
        }

        g.setColour (juce::Colours::white.withAlpha (on ? 0.85f : 0.4f));
        g.setFont (IlanaTheme::font (juce::jlimit (11.0f, 15.0f, radius * 0.32f), true));
        g.drawText ("E(" + juce::String (hits) + "," + juce::String (steps) + ")",
                    juce::Rectangle<float> (radius * 1.6f, 18.0f).withCentre (centre.translated (0.0f, -4.0f)),
                    juce::Justification::centred);

        if (rotate != 0)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.drawText ("+" + juce::String (rotate), juce::Rectangle<float> (radius, 14.0f).withCentre (centre.translated (0.0f, 11.0f)),
                        juce::Justification::centred);
        }

        // The strip: every step in a row, hits tall.
        const auto strip = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 46.0f));
        const auto width = strip.getWidth() / (float) steps;

        for (int step = 0; step < steps; ++step)
        {
            const auto cell = juce::Rectangle<float> (strip.getX() + (float) step * width, strip.getY(), width, strip.getHeight())
                                  .reduced (juce::jmin (1.5f, width * 0.15f), 0.0f);
            const auto hit = euclidHit (step, hits, steps, rotate);
            const auto playing = on && step == current;
            const auto bar = hit ? cell : cell.withTrimmedTop (cell.getHeight() * 0.8f);

            g.setColour (hit ? colour.withAlpha ((on ? 0.75f : 0.35f) + (playing ? 0.25f : 0.0f))
                             : juce::Colours::white.withAlpha (playing ? 0.3f : 0.1f));
            g.fillRoundedRectangle (bar, juce::jmin (2.0f, width * 0.3f));
        }

        if (! on)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("EUCLID OFF", bounds.reduced (8.0f, 5.0f), juce::Justification::topRight);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        processorRef.getUndoManager().beginNewTransaction ("Euclid");
        dragHits = juce::roundToInt (GenerativeWidgets::read (processorRef, "euc_hits"));
        dragRotate = juce::roundToInt (GenerativeWidgets::read (processorRef, "euc_rotate"));
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto steps = juce::jlimit (2, 32, juce::roundToInt (GenerativeWidgets::read (processorRef, "euc_steps")));
        const auto hits = juce::jlimit (0, steps, dragHits - event.getDistanceFromDragStartY() / 8);
        const auto rotate = ((dragRotate + event.getDistanceFromDragStartX() / 12) % steps + steps) % steps;
        GenerativeWidgets::write (processorRef, "euc_hits", (float) hits);
        GenerativeWidgets::write (processorRef, "euc_rotate", (float) juce::jmin (31, rotate));
    }

private:
    static juce::Point<float> pointOnRing (juce::Point<float> centre, float radius, int step, int steps)
    {
        const auto angle = juce::MathConstants<float>::twoPi * (float) step / (float) steps - juce::MathConstants<float>::halfPi;
        return centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * radius;
    }

    void timerCallback() override
    {
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    int dragHits = 0, dragRotate = 0;
};

// The probability sequencer's 16 steps in three lanes: CHANCE (bars),
// RANGE (how far above the key a step may land) and RATCHET (1-4 dots).
// Drag in a lane to draw values across steps; double-click resets a step.
class ProbSeqEditor : public juce::Component,
                      public juce::SettableTooltipClient,
                      private IlanaAnim::FrameTimer
{
public:
    ProbSeqEditor (IlanaSynthAudioProcessor& processor, juce::Colour colourIn)
        : processorRef (processor), colour (colourIn)
    {
        setTooltip ("Probability sequencer. Drag in CHANCE and RANGE to draw; click RATCHET to cycle 1-4 repeats. "
                    "Double-click a step to reset it.");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        using GenerativeWidgets::read;
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto on = read (processorRef, "pseq_on") > 0.5f;
        const auto length = juce::jlimit (1, 16, juce::roundToInt (read (processorRef, "pseq_length")));
        const auto current = on ? processorRef.getEngineDisplayStep() : -1;
        const auto playingStep = current >= 0 ? current % length : -1;

        const auto lanes = laneBounds();
        const char* const names[] { "CHANCE", "RANGE", "RATCHET" };

        for (int lane = 0; lane < 3; ++lane)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (names[lane], labelArea (lanes[(size_t) lane]), juce::Justification::centredLeft);
        }

        for (int step = 0; step < 16; ++step)
        {
            const auto active = step < length;
            const auto alpha = (active ? 1.0f : 0.25f) * (on ? 1.0f : 0.55f);
            const auto n = juce::String (step + 1);
            const auto playing = step == playingStep;

            if (playing)
            {
                g.setColour (colour.withAlpha (0.12f));
                g.fillRect (columnFor (step, lanes[0].getUnion (lanes[2])));
            }

            // Chance: a bar from the bottom of its lane.
            {
                const auto cell = columnFor (step, lanes[0]).reduced (1.5f, 0.0f);
                const auto chance = juce::jlimit (0.0f, 1.0f, read (processorRef, "pseq_chance" + n));
                g.setColour (juce::Colours::white.withAlpha (0.05f * alpha));
                g.fillRoundedRectangle (cell, 2.0f);
                g.setColour (colour.withAlpha ((playing ? 1.0f : 0.8f) * alpha));
                g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - chance)), 2.0f);
            }

            // Range: a thinner bar, with the semitones printed when set.
            {
                const auto cell = columnFor (step, lanes[1]).reduced (1.5f, 0.0f);
                const auto range = juce::jlimit (0, 24, juce::roundToInt (read (processorRef, "pseq_range" + n)));
                g.setColour (juce::Colours::white.withAlpha (0.05f * alpha));
                g.fillRoundedRectangle (cell, 2.0f);

                if (range > 0)
                {
                    g.setColour (colour.withMultipliedSaturation (0.6f).withAlpha (0.55f * alpha));
                    g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - (float) range / 24.0f)), 2.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.8f * alpha));
                    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                    g.drawText (juce::String (range), cell.toNearestInt(), juce::Justification::centredTop);
                }
            }

            // Ratchet: 1-4 dots.
            {
                const auto cell = columnFor (step, lanes[2]);
                const auto ratchet = juce::jlimit (1, 4, juce::roundToInt (read (processorRef, "pseq_ratchet" + n)));
                const auto size = juce::jmin (4.0f, cell.getWidth() * 0.22f);

                for (int r = 0; r < ratchet; ++r)
                {
                    const auto x = cell.getCentreX() + ((float) r - (float) (ratchet - 1) * 0.5f) * (size + 1.5f);
                    g.setColour (colour.withAlpha ((ratchet > 1 ? 0.9f : 0.35f) * alpha));
                    g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ x, cell.getCentreY() }));
                }
            }
        }

        if (! on)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("SEQ OFF", bounds.reduced (8.0f, 4.0f), juce::Justification::topRight);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        processorRef.getUndoManager().beginNewTransaction ("Probability sequencer");
        dragLane = laneAt (event.position);

        if (dragLane == 2)
        {
            if (const auto step = stepAt (event.position.x); step >= 0)
            {
                const auto id = "pseq_ratchet" + juce::String (step + 1);
                const auto ratchet = juce::roundToInt (GenerativeWidgets::read (processorRef, id));
                GenerativeWidgets::write (processorRef, id, (float) (ratchet % 4 + 1));
            }

            return;
        }

        drawAt (event.position);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragLane == 0 || dragLane == 1)
            drawAt (event.position);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (const auto step = stepAt (event.position.x); step >= 0)
        {
            const auto n = juce::String (step + 1);
            GenerativeWidgets::write (processorRef, "pseq_chance" + n, 1.0f);
            GenerativeWidgets::write (processorRef, "pseq_range" + n, 0.0f);
            GenerativeWidgets::write (processorRef, "pseq_ratchet" + n, 1.0f);
        }
    }

private:
    static constexpr float labelWidth = 50.0f;

    std::array<juce::Rectangle<float>, 3> laneBounds() const
    {
        auto area = getLocalBounds().toFloat().reduced (6.0f, 5.0f);
        const auto height = area.getHeight();
        std::array<juce::Rectangle<float>, 3> lanes;
        lanes[0] = area.removeFromTop (height * 0.5f).withTrimmedBottom (3.0f);
        lanes[1] = area.removeFromTop (height * 0.3f).withTrimmedBottom (3.0f);
        lanes[2] = area;
        return lanes;
    }

    static juce::Rectangle<float> labelArea (juce::Rectangle<float> lane) { return lane.withWidth (labelWidth); }

    juce::Rectangle<float> columnFor (int step, juce::Rectangle<float> lane) const
    {
        const auto grid = lane.withTrimmedLeft (labelWidth);
        const auto width = grid.getWidth() / 16.0f;
        return { grid.getX() + (float) step * width, lane.getY(), width, lane.getHeight() };
    }

    int stepAt (float x) const
    {
        const auto grid = laneBounds()[0].withTrimmedLeft (labelWidth);

        if (x < grid.getX() || x > grid.getRight())
            return -1;

        return juce::jlimit (0, 15, (int) ((x - grid.getX()) / (grid.getWidth() / 16.0f)));
    }

    int laneAt (juce::Point<float> position) const
    {
        const auto lanes = laneBounds();

        for (int lane = 0; lane < 3; ++lane)
            if (position.y <= lanes[(size_t) lane].getBottom() + 1.5f)
                return lane;

        return 2;
    }

    void drawAt (juce::Point<float> position)
    {
        const auto step = stepAt (position.x);

        if (step < 0 || dragLane < 0 || dragLane > 1)
            return;

        const auto lane = laneBounds()[(size_t) dragLane];
        const auto value = juce::jlimit (0.0f, 1.0f, (lane.getBottom() - position.y) / juce::jmax (1.0f, lane.getHeight()));
        const auto n = juce::String (step + 1);

        if (dragLane == 0)
            GenerativeWidgets::write (processorRef, "pseq_chance" + n, value);
        else
            GenerativeWidgets::write (processorRef, "pseq_range" + n, (float) juce::roundToInt (value * 24.0f));
    }

    void timerCallback() override
    {
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    int dragLane = -1;
};
