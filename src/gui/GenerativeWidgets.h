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

// Why a pattern display steps back (its engine is off...), on a plate centred
// in `area`: the same words in the same place on every engine's tab.
inline void paintOffPlate (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text)
{
    const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
    const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 24.0f;
    const auto plate = juce::Rectangle<float> (juce::jmin (width, area.getWidth()), 22.0f).withCentre (area.getCentre());
    g.setColour (IlanaTheme::Ui::well.withAlpha (0.92f));
    g.fillRoundedRectangle (plate, 11.0f);
    g.setColour (IlanaTheme::Ui::line);
    g.drawRoundedRectangle (plate.reduced (0.5f), 11.0f, 1.0f);
    g.setColour (IlanaTheme::Ui::text2);
    g.setFont (font);
    g.drawText (text, plate.toNearestInt(), juce::Justification::centred, true);
}
} // namespace GenerativeWidgets

// An on switch for a part whose off is a choice (review 6, I6-30):
// GENERATE's SNAP TO KEY (SCALE Off) and STRUM (DIRECTION Off). Off sets
// the choice to its first entry; on brings back the last other entry (or
// `firstOn`). One undo step per click; the menu stays the way to pick.
class ChoiceSwitch : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
{
public:
    ChoiceSwitch (IlanaSynthAudioProcessor& processor, const juce::String& parameterId, int firstOn, const juce::String& what)
        : processorRef (processor), id (parameterId), remembered (firstOn), name (what)
    {
        button.getProperties().set ("switch", true);
        button.onClick = [this] { toggle(); };
        button.setTooltip (what + " on / off");
        setTooltip (button.getTooltip());
        addAndMakeVisible (button);
        sync();
        startTimerHz (10);
    }

    bool isOn() const { return current() > 0; }

    juce::TextButton& getButton() { return button; }

    void toggle()
    {
        const auto now = current();

        if (now > 0)
            remembered = now;

        processorRef.performEdit (name + (now > 0 ? " off" : " on"),
                                  [this, now] { GenerativeWidgets::write (processorRef, id, now > 0 ? 0.0f : (float) remembered); });
        sync();
    }

    // Laid out like a ToggleControl's bare switch: 13 px of label space,
    // then the pill.
    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (13);
        button.setBounds (area.removeFromTop (juce::jmin (24, juce::jmax (16, area.getHeight()))));
    }

private:
    int current() const { return juce::roundToInt (GenerativeWidgets::read (processorRef, id)); }

    void sync()
    {
        const auto now = current();

        if (now > 0)
            remembered = now;

        const auto amount = now > 0 ? 1.0f : 0.0f;

        if ((float) button.getProperties()["switchAmount"] != amount || ! button.getProperties().contains ("switchAmount"))
        {
            button.getProperties().set ("switchAmount", amount);
            button.repaint();
        }
    }

    void timerCallback() override { sync(); }

    IlanaSynthAudioProcessor& processorRef;
    juce::String id;
    int remembered = 1;
    juce::String name;
    juce::TextButton button;
};

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

        // The strip: every step in a row, hits tall, numbered under it
        // (every fourth) when there is room.
        const auto strip = area.withSizeKeepingCentre (area.getWidth(), juce::jlimit (juce::jmin (area.getHeight(), 46.0f),
                                                                                     juce::jmax (0.0f, area.getHeight() - 22.0f),
                                                                                     area.getHeight() * 0.42f));
        const auto width = strip.getWidth() / (float) steps;

        if (strip.getBottom() + 16.0f <= area.getBottom())
        {
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));

            for (int step = 0; step < steps; step += 4)
            {
                g.setColour (step == current && on ? colour : IlanaTheme::Ui::text3);
                g.drawText (juce::String (step + 1),
                            juce::Rectangle<float> (strip.getX() + (float) step * width, strip.getBottom() + 3.0f, juce::jmax (width, 24.0f), 13.0f)
                                .toNearestInt(),
                            juce::Justification::centredLeft);
            }
        }

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
            GenerativeWidgets::paintOffPlate (g, strip.withHeight (22.0f).withY (bounds.getY() + 8.0f),
                                              "EUCLID OFF: the rhythm plays once it is on");
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
        // RANGE's numbers are semitones above the held note, printed inside
        // their own lane.
        const char* const names[] { "CHANCE", "RANGE  +st", "RATCHET" };

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
                    g.setColour (juce::Colours::white.withAlpha (0.85f * alpha));
                    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                    g.drawText ("+" + juce::String (range), cell.toNearestInt(), juce::Justification::centred);
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
            GenerativeWidgets::paintOffPlate (g, lanes[0].getUnion (lanes[2]).withTrimmedLeft (labelWidth),
                                              "PROB SEQ OFF: the steps play once it is on");
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
    static constexpr float labelWidth = 66.0f;

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

// The arp's step lanes (review 6, S6-23): VELOCITY, GATE (a share of the
// GATE knob; 0 rests the step, 200 % ties it into the next) and PITCH
// (semitones) for up to 16 steps, over a numbered step ruler. Click a step
// number to set how many steps loop; drag in a lane to draw across steps;
// double-click a step to reset it. The held notes' order (MODE, OCTAVES)
// runs on by itself; the lanes shape each step it lands on. While the arp
// is off the lanes stay editable but step back.
class ArpLanesEditor : public juce::Component,
                       public juce::SettableTooltipClient,
                       private IlanaAnim::FrameTimer
{
public:
    enum Lane { velocity, gate, pitch, numLanes };

    ArpLanesEditor (IlanaSynthAudioProcessor& processor, juce::Colour colourIn)
        : processorRef (processor), colour (colourIn)
    {
        setTooltip ("Arp step lanes. Drag in VELOCITY, GATE and PITCH to draw; double-click a step to reset it. "
                    "Click a step number to set how many steps loop.");
        startTimerHz (30);
    }

    static juce::String idFor (int lane, int step)
    {
        const auto n = juce::String (step + 1);
        return lane == velocity ? "arp_vel" + n : lane == gate ? "arp_len" + n : "arp_pitch" + n;
    }

    // Each lane's value as 0..1 of its height, and back (PITCH is
    // bipolar: 0.5 is no transpose).
    static float toUnit (int lane, float value)
    {
        return lane == velocity ? (value - 1.0f) / 126.0f : lane == gate ? value / 2.0f : (value + 12.0f) / 24.0f;
    }

    static float fromUnit (int lane, float unit)
    {
        unit = juce::jlimit (0.0f, 1.0f, unit);
        return lane == velocity ? (float) juce::roundToInt (1.0f + unit * 126.0f)
             : lane == gate     ? std::round (unit * 40.0f) / 20.0f // 5 % steps
                                : (float) juce::roundToInt (unit * 24.0f - 12.0f);
    }

    static float defaultFor (int lane) { return lane == velocity ? 100.0f : lane == gate ? 1.0f : 0.0f; }

    static juce::String textFor (int lane, float value)
    {
        if (lane == velocity)
            return juce::String (juce::roundToInt (value));

        if (lane == gate)
            return value < 0.005f ? juce::String ("rest") : juce::String (juce::roundToInt (value * 100.0f)) + "%";

        const auto semitones = juce::roundToInt (value);
        return semitones > 0 ? "+" + juce::String (semitones) : juce::String (semitones);
    }

    void paint (juce::Graphics& g) override
    {
        using GenerativeWidgets::read;
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto on = read (processorRef, "arp_on") > 0.5f;
        const auto replaced = read (processorRef, "pseq_on") > 0.5f;
        const auto steps = numSteps();
        const auto engineStep = processorRef.getEngineDisplayStep();
        const auto playingStep = on && ! replaced && engineStep >= 0 ? engineStep % steps : -1;
        // Off (or played over by PROB SEQ): the lanes step back.
        const auto alpha = on && ! replaced ? 1.0f : 0.32f;
        const auto ruler = rulerBounds();
        const auto lanes = laneBounds();
        const char* const names[] { "VELOCITY", "GATE", "PITCH  st" };

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));

        for (int lane = 0; lane < numLanes; ++lane)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText (names[lane], lanes[(size_t) lane].withWidth (labelWidth).toNearestInt(), juce::Justification::centredLeft);
        }

        g.setColour (IlanaTheme::Ui::text3);
        g.drawText ("STEP", ruler.withWidth (labelWidth).toNearestInt(), juce::Justification::centredLeft);

        for (int step = 0; step < 16; ++step)
        {
            const auto active = step < steps;
            const auto cellAlpha = alpha * (active ? 1.0f : 0.3f);
            const auto playing = step == playingStep;
            const auto column = columnFor (step, ruler.getUnion (lanes[2]));

            if (playing)
            {
                g.setColour (colour.withAlpha (0.14f));
                g.fillRect (column);
            }

            // The step ruler: numbers in their own strip (never behind the
            // bars), every fourth stronger, the loop's end marked.
            {
                const auto cell = columnFor (step, ruler);
                g.setColour (step == steps - 1 ? colour.withAlpha (alpha) : juce::Colours::white.withAlpha ((step % 4 == 0 ? 0.6f : 0.32f) * (active ? 1.0f : 0.5f)));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, step % 4 == 0 || step == steps - 1));
                g.drawText (juce::String (step + 1), cell.toNearestInt(), juce::Justification::centred);

                if (step == steps - 1)
                {
                    g.setColour (colour.withAlpha (0.8f * alpha));
                    g.fillRect (cell.getRight() - 1.0f, cell.getY() + 2.0f, 2.0f, lanes[2].getBottom() - cell.getY() - 2.0f);
                }
            }

            for (int lane = 0; lane < numLanes; ++lane)
            {
                const auto cell = columnFor (step, lanes[(size_t) lane]).reduced (1.5f, 0.0f);
                const auto value = read (processorRef, idFor (lane, step));
                const auto unit = juce::jlimit (0.0f, 1.0f, toUnit (lane, value));
                const auto edited = std::abs (value - defaultFor (lane)) > 1.0e-3f;

                g.setColour (juce::Colours::white.withAlpha (0.045f * (active ? 1.0f : 0.6f)));
                g.fillRoundedRectangle (cell, 2.0f);
                const auto fill = colour.withAlpha ((playing ? 1.0f : 0.78f) * cellAlpha);

                if (lane == pitch)
                {
                    // From the centre line, up or down.
                    const auto mid = cell.getCentreY();
                    const auto y = cell.getBottom() - unit * cell.getHeight();
                    g.setColour (juce::Colours::white.withAlpha (0.12f * cellAlpha));
                    g.fillRect (cell.getX(), mid - 0.5f, cell.getWidth(), 1.0f);
                    if (edited)
                    {
                        g.setColour (fill);
                        g.fillRoundedRectangle (juce::Rectangle<float> (cell.getX(), juce::jmin (mid, y), cell.getWidth(),
                                                                        juce::jmax (2.0f, std::abs (y - mid))), 2.0f);
                    }
                }
                else if (lane == gate && value < 0.005f)
                {
                    // A rest: a small cross where the bar would be.
                    const auto mark = juce::Rectangle<float> (7.0f, 7.0f).withCentre (cell.getCentre());
                    g.setColour (juce::Colours::white.withAlpha (0.45f * cellAlpha));
                    g.drawLine ({ mark.getTopLeft(), mark.getBottomRight() }, 1.3f);
                    g.drawLine ({ mark.getBottomLeft(), mark.getTopRight() }, 1.3f);
                }
                else
                {
                    g.setColour (fill);
                    g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - unit)), 2.0f);

                    // GATE's 100 % line: where the GATE knob's length is.
                    if (lane == gate)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.22f * cellAlpha));
                        g.fillRect (cell.getX(), cell.getCentreY() - 0.5f, cell.getWidth(), 1.0f);
                    }
                }

                // A value moved off its default, or being drawn, reads out.
                const auto dragging = lane == dragLane && step == dragStep;

                if ((edited && lane == pitch) || dragging)
                {
                    g.setColour (juce::Colours::white.withAlpha ((dragging ? 1.0f : 0.85f) * (active ? 1.0f : 0.5f)));
                    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                    g.drawText (textFor (lane, value), cell.toNearestInt(), juce::Justification::centred);
                }
            }
        }

        // Why the lanes step back, on a plate over them.
        if (! on || replaced)
            GenerativeWidgets::paintOffPlate (g, lanes[0].getUnion (lanes[2]).withTrimmedLeft (labelWidth),
                                              replaced && on ? "PROB SEQ is playing instead of the ARP"
                                                             : "ARP OFF: the lanes play once it is on");
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragLane = laneAt (event.position);
        dragStep = -1;

        // A step number sets the loop's length.
        if (dragLane < 0)
        {
            if (const auto step = stepAt (event.position.x); step >= 0)
                processorRef.performEdit ("Arp steps", [this, step] { GenerativeWidgets::write (processorRef, "arp_steps", (float) (step + 1)); });

            return;
        }

        processorRef.beginEdit (juce::String ("Arp ") + (dragLane == velocity ? "velocity" : dragLane == gate ? "gate" : "transpose"));
        editOpen = true;
        lastPosition = event.position;
        drawAt (event.position, event.position);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! editOpen)
            return;

        drawAt (lastPosition, event.position);
        lastPosition = event.position;
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (editOpen)
            processorRef.endEdit();

        editOpen = false;
        dragLane = dragStep = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto step = stepAt (event.position.x);

        if (step < 0 || laneAt (event.position) < 0)
            return;

        processorRef.performEdit ("Reset arp step", [this, step]
        {
            for (int lane = 0; lane < numLanes; ++lane)
                GenerativeWidgets::write (processorRef, idFor (lane, step), defaultFor (lane));
        });
    }

    // The UI test reaches the cells through these.
    juce::Point<float> cellCentre (int lane, int step) const
    {
        return columnFor (step, lane < 0 ? rulerBounds() : laneBounds()[(size_t) lane]).getCentre();
    }

    juce::Rectangle<float> laneArea (int lane) const { return laneBounds()[(size_t) juce::jlimit (0, 2, lane)]; }

private:
    static constexpr float labelWidth = 66.0f;
    static constexpr float rulerHeight = 15.0f;

    int numSteps() const { return juce::jlimit (1, 16, juce::roundToInt (GenerativeWidgets::read (processorRef, "arp_steps"))); }

    juce::Rectangle<float> rulerBounds() const
    {
        return getLocalBounds().toFloat().reduced (6.0f, 4.0f).removeFromTop (rulerHeight);
    }

    std::array<juce::Rectangle<float>, numLanes> laneBounds() const
    {
        auto area = getLocalBounds().toFloat().reduced (6.0f, 4.0f);
        area.removeFromTop (rulerHeight + 2.0f);
        const auto height = area.getHeight();
        std::array<juce::Rectangle<float>, numLanes> lanes;
        lanes[0] = area.removeFromTop (height * 0.4f).withTrimmedBottom (4.0f);
        lanes[1] = area.removeFromTop (height * 0.3f).withTrimmedBottom (4.0f);
        lanes[2] = area;
        return lanes;
    }

    static juce::Rectangle<float> columnFor (int step, juce::Rectangle<float> lane)
    {
        const auto grid = lane.withTrimmedLeft (labelWidth);
        const auto width = grid.getWidth() / 16.0f;
        return { grid.getX() + (float) step * width, lane.getY(), width, lane.getHeight() };
    }

    int stepAt (float x) const
    {
        const auto grid = rulerBounds().withTrimmedLeft (labelWidth);

        if (x < grid.getX() || x > grid.getRight())
            return -1;

        return juce::jlimit (0, 15, (int) ((x - grid.getX()) / (grid.getWidth() / 16.0f)));
    }

    // -1 for the ruler (and the label column), else the lane under y.
    int laneAt (juce::Point<float> position) const
    {
        if (position.y < rulerBounds().getBottom() + 1.0f || position.x < rulerBounds().getX() + labelWidth)
            return -1;

        const auto lanes = laneBounds();

        for (int lane = 0; lane < numLanes; ++lane)
            if (position.y <= lanes[(size_t) lane].getBottom() + 2.0f)
                return lane;

        return pitch;
    }

    // Draws the dragged lane between two points, so a fast stroke leaves
    // no step out.
    void drawAt (juce::Point<float> from, juce::Point<float> to)
    {
        if (dragLane < 0)
            return;

        const auto lane = laneBounds()[(size_t) dragLane];
        const auto first = stepAt (juce::jlimit (lane.getX() + labelWidth, lane.getRight(), juce::jmin (from.x, to.x)));
        const auto last = stepAt (juce::jlimit (lane.getX() + labelWidth, lane.getRight(), juce::jmax (from.x, to.x)));

        for (auto step = first; step >= 0 && step <= last; ++step)
        {
            const auto centreX = columnFor (step, lane).getCentreX();
            const auto t = std::abs (to.x - from.x) < 1.0f ? 1.0f : juce::jlimit (0.0f, 1.0f, (centreX - from.x) / (to.x - from.x));
            const auto y = from.y + (to.y - from.y) * t;
            const auto unit = (lane.getBottom() - y) / juce::jmax (1.0f, lane.getHeight());
            GenerativeWidgets::write (processorRef, idFor (dragLane, step), fromUnit (dragLane, unit));
        }

        dragStep = stepAt (juce::jlimit (lane.getX() + labelWidth, lane.getRight(), to.x));
        repaint();
    }

    void timerCallback() override
    {
        const auto signature = processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)
                             ^ ((juce::uint64) (processorRef.getEngineDisplayStep() + 1) << 44);

        if (isShowing() && changeGate.check (signature))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;
    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    int dragLane = -1, dragStep = -1;
    bool editOpen = false;
    juce::Point<float> lastPosition;
};

// The note path at a glance (review 6, I6-15): which note stages are on and
// in what order the played keys pass through them, KEYS > SNAP > STRUM >
// SPRAY > the pattern engine > CLIP > VOICES, with how they combine (PROB
// SEQ playing instead of the ARP, EUCLID resting its steps, the pattern
// transposing the clip). A stage of the PATTERN card is a chip that opens
// its tab.
class NoteChainView : public juce::Component,
                      public juce::SettableTooltipClient,
                      private IlanaAnim::FrameTimer
{
public:
    explicit NoteChainView (IlanaSynthAudioProcessor& processor, juce::Colour colourIn)
        : processorRef (processor), colour (colourIn)
    {
        startTimerHz (8);
    }

    // The engine tab a chip opens (0 ARP, 1 EUCLID, 2 PROB SEQ, 3 CLIP).
    std::function<void (int)> onOpenEngine;

    struct Stage
    {
        juce::String name;
        int engine = -1;        // the PATTERN tab it opens, or -1
        bool waiting = false;   // on, but another stage plays instead
        bool quiet = false;     // KEYS and VOICES: the ends, not stages
    };

    // The stages that act now, in order, and one line on how they combine.
    std::vector<Stage> stages (juce::String& note) const
    {
        const auto read = [this] (const char* id) { return GenerativeWidgets::read (processorRef, id); };
        const auto arpOn = read ("arp_on") > 0.5f, seqOn = read ("pseq_on") > 0.5f, clipOn = read ("clip_on") > 0.5f;
        const auto euclidOn = read ("euc_on") > 0.5f;
        const auto euclidTarget = juce::roundToInt (read ("euc_target"));
        const auto euclidNotes = euclidOn && euclidTarget == 0;
        const auto scaleOn = juce::roundToInt (read ("gen_scale")) > 0;
        const auto clipHost = juce::roundToInt (read ("clip_mode")) == 1;

        std::vector<Stage> list;
        list.push_back ({ "KEYS", -1, false, true });

        if (scaleOn && read ("gen_snap") > 0.5f)
            list.push_back ({ "SNAP TO KEY" });
        if (juce::roundToInt (read ("spray_strum")) > 0)
            list.push_back ({ "STRUM" });
        if (read ("spray_on") > 0.5f)
            list.push_back ({ "SPRAY" });

        if (seqOn && arpOn)
            list.push_back ({ "ARP", 0, true });
        if (seqOn)
            list.push_back ({ "PROB SEQ", 2 });
        else if (arpOn)
            list.push_back ({ "ARP", 0 });
        if (euclidNotes)
            list.push_back ({ "EUCLID", 1 });
        if (clipOn)
            list.push_back ({ clipHost ? "CLIP + HOST" : "CLIP", 3 });

        list.push_back ({ "VOICES", -1, false, true });

        if (euclidOn && euclidTarget != 0)
            list.push_back ({ euclidTarget == 1 ? "EUCLID: STRINGS" : "EUCLID: GATE", 1 });

        const auto engine = seqOn ? juce::String ("PROB SEQ") : arpOn ? juce::String ("ARP") : juce::String();

        if (seqOn && arpOn)
            note = "PROB SEQ plays instead of the ARP";
        else if (euclidNotes && engine.isNotEmpty())
            note = "EUCLID rests " + engine + "'s steps between its hits";
        else if (euclidNotes)
            note = "EUCLID plays the held chord on each hit";
        else if (clipOn && ! clipHost && engine.isNotEmpty())
            note = engine + "'s notes transpose the clip";
        else if (clipOn && ! clipHost)
            note = "a held key plays the clip (C3 as written)";
        else if (clipOn)
            note = "the clip plays with the host; keys play over it";
        else if (euclidOn && euclidTarget == 1)
            note = "EUCLID re-strikes the Physical strings";
        else if (euclidOn)
            note = "EUCLID drives the Trance Gate effect";
        else if (engine.isEmpty())
            note = "no pattern engine on: keys play the voices";
        else if (seqOn)
            note = "hold keys: each step rolls its chance";
        else
            note = "hold keys: the ARP plays them in MODE's order";

        return list;
    }

    void paint (juce::Graphics& g) override
    {
        juce::String note;
        const auto list = stages (note);
        chips.clear();
        const auto font = IlanaTheme::pillFont();
        const auto height = juce::jmin (18.0f, (float) getHeight() - 4.0f);
        auto x = 0.0f;
        const auto centreY = (float) getHeight() * 0.5f;
        const auto mouse = getMouseXYRelative().toFloat();

        for (size_t i = 0; i < list.size(); ++i)
        {
            const auto& stage = list[i];

            // The arrow before every stage after the first; an engine
            // switched to Strings / Gate hangs off VOICES with a plus.
            if (i > 0)
            {
                const auto side = stage.name.startsWith ("EUCLID:");
                const auto arrow = side ? juce::String ("+") : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xba"));
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                g.drawText (arrow, juce::Rectangle<float> (x, 0.0f, 14.0f, (float) getHeight()).toNearestInt(), juce::Justification::centred);
                x += 14.0f;
            }

            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, stage.name) + 4.0f;

            if (x + width > (float) getWidth())
                break;

            const auto chip = juce::Rectangle<float> (x, centreY - height * 0.5f, width, height);
            g.setFont (font);

            if (stage.quiet)
            {
                g.setColour (IlanaTheme::Ui::text2);
                g.drawText (stage.name, chip.toNearestInt(), juce::Justification::centred);
            }
            else
            {
                // A stage reads as a word in the accent (the tabs are the
                // pills): bold, underlined while the pointer is on one that
                // opens a tab; one waiting for another is grey, struck through.
                const auto hovered = stage.engine >= 0 && isMouseOver() && chip.contains (mouse);
                g.setColour (stage.waiting ? IlanaTheme::Ui::text3 : colour.interpolatedWith (juce::Colours::white, hovered ? 0.45f : 0.2f));
                g.drawText (stage.name, chip.toNearestInt(), juce::Justification::centred);
                const auto textWidth = (float) juce::GlyphArrangement::getStringWidthInt (font, stage.name);
                const auto line = juce::Rectangle<float> (textWidth, 1.0f).withCentre ({ chip.getCentreX(), centreY });

                if (stage.waiting)
                    g.fillRect (line);
                else if (hovered)
                    g.fillRect (line.withY (centreY + 7.0f));
            }

            chips.push_back ({ chip, stage.engine });
            x += width;
        }

        if (note.isNotEmpty() && x + 40.0f < (float) getWidth())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText (note, juce::Rectangle<float> (x + 10.0f, 0.0f, (float) getWidth() - x - 10.0f, (float) getHeight()).toNearestInt(),
                        juce::Justification::centredLeft, true);
        }

        // The whole path in words, for the tooltip.
        juce::StringArray names;
        for (const auto& stage : list)
            names.add (stage.waiting ? "(" + stage.name + ", waiting)" : stage.name);
        setTooltip ("The note path: " + names.joinIntoString (" > ") + (note.isNotEmpty() ? ".\n" + note + "." : ".")
                    + "\nClick a pattern engine to open its tab.");
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        for (const auto& [chip, engine] : chips)
            if (engine >= 0 && chip.contains (event.position) && onOpenEngine != nullptr)
            {
                onOpenEngine (engine);
                return;
            }
    }

    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    void timerCallback() override
    {
        if (isShowing() && changeGate.check (processorRef.getUiEpoch()))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;
    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    std::vector<std::pair<juce::Rectangle<float>, int>> chips;
};
