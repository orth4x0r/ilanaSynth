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

// Drawing in a pattern engine that is off switches it on, in the same undo
// step as the first stroke (review 9, S9-3: one click on the tab, not a
// tab and a switch).
inline void switchOnIfOff (IlanaSynthAudioProcessor& processor, const juce::String& id);

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

inline void switchOnIfOff (IlanaSynthAudioProcessor& processor, const juce::String& id)
{
    if (read (processor, id) < 0.5f)
        write (processor, id, 1.0f);
}

// The tools on a step lane's right-click (UI review 31, item 32): what one
// does to a 16-step lane every time, in one undo step each. They work on the
// loop's steps (Reset on all 16), in the parameter's own 0..1, so a lane
// copied from VEL pastes onto PITCH or CHANCE by shape.
enum class LaneTool { reset = 1, randomise, shiftLeft, shiftRight, rampUp, rampDown, copy, paste };

struct LaneClipboard
{
    std::array<float, 16> units {};
    bool full = false;
};

// Process-wide on purpose (a clipboard carries between instances).
inline LaneClipboard& laneClipboard()
{
    static LaneClipboard clipboard;
    return clipboard;
}

// Its own generator: a lane's dice never move a patch's random sequences.
inline juce::Random& laneRandom()
{
    static juce::Random random;
    return random;
}

inline void applyLaneTool (IlanaSynthAudioProcessor& processor, const std::function<juce::String (int)>& idFor, int loopLength,
                           LaneTool tool, const juce::String& laneName)
{
    std::array<juce::RangedAudioParameter*, 16> parameters {};
    for (int step = 0; step < 16; ++step)
        parameters[(size_t) step] = processor.apvts.getParameter (idFor (step));

    const auto length = juce::jlimit (1, 16, loopLength);
    std::array<float, 16> units {};
    for (int step = 0; step < 16; ++step)
        units[(size_t) step] = parameters[(size_t) step] != nullptr ? parameters[(size_t) step]->getValue() : 0.0f;

    if (tool == LaneTool::copy)
    {
        laneClipboard().units = units;
        laneClipboard().full = true;
        return;
    }

    auto next = units;
    switch (tool)
    {
        case LaneTool::reset:
            for (int step = 0; step < 16; ++step)
                if (parameters[(size_t) step] != nullptr)
                    next[(size_t) step] = parameters[(size_t) step]->getDefaultValue();
            break;
        case LaneTool::randomise:
            for (int step = 0; step < length; ++step)
                next[(size_t) step] = laneRandom().nextFloat();
            break;
        case LaneTool::shiftLeft:
            for (int step = 0; step < length; ++step)
                next[(size_t) step] = units[(size_t) ((step + 1) % length)];
            break;
        case LaneTool::shiftRight:
            for (int step = 0; step < length; ++step)
                next[(size_t) step] = units[(size_t) ((step + length - 1) % length)];
            break;
        case LaneTool::rampUp:
        case LaneTool::rampDown:
            for (int step = 0; step < length; ++step)
            {
                const auto t = length > 1 ? (float) step / (float) (length - 1) : 1.0f;
                next[(size_t) step] = tool == LaneTool::rampUp ? t : 1.0f - t;
            }
            break;
        case LaneTool::paste:
            if (! laneClipboard().full)
                return;
            next = laneClipboard().units;
            break;
        case LaneTool::copy:
            break;
    }

    const char* const names[] { "", "Reset", "Randomise", "Shift left", "Shift right", "Ramp up", "Ramp down", "Copy", "Paste" };
    processor.performEdit (juce::String (names[(int) tool]) + " " + laneName + " lane", [&parameters, &next, &units]
    {
        for (size_t step = 0; step < 16; ++step)
            if (auto* parameter = parameters[step]; parameter != nullptr && std::abs (next[step] - units[step]) > 1.0e-6f)
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, next[step]));
                parameter->endChangeGesture();
            }
    });
}

// The UI test's hook: while set, a lane's menu is only noted (its name), not shown.
inline juce::String*& laneMenuProbe()
{
    static juce::String* probe = nullptr;
    return probe;
}

// The lane's menu, at the pointer.
inline void showLaneMenu (IlanaSynthAudioProcessor& processor, juce::Component& lanes, std::function<juce::String (int)> idFor,
                          int loopLength, const juce::String& laneName)
{
    if (auto* probe = laneMenuProbe())
    {
        *probe = laneName;
        return;
    }

    juce::PopupMenu menu;
    menu.addSectionHeader (laneName);
    menu.addItem ((int) LaneTool::reset, "Reset");
    menu.addItem ((int) LaneTool::randomise, "Randomise");
    menu.addItem ((int) LaneTool::shiftLeft, "Shift left");
    menu.addItem ((int) LaneTool::shiftRight, "Shift right");
    menu.addItem ((int) LaneTool::rampUp, "Ramp up");
    menu.addItem ((int) LaneTool::rampDown, "Ramp down");
    menu.addSeparator();
    menu.addItem ((int) LaneTool::copy, "Copy");
    menu.addItem ((int) LaneTool::paste, "Paste", laneClipboard().full);
    juce::Component::SafePointer<juce::Component> safe (&lanes);
    menu.showMenuAsync (IlanaTheme::menuOptions (&lanes).withMousePosition(),
                        [&processor, safe, idFor, loopLength, laneName] (int result)
                        {
                            if (result <= 0 || safe == nullptr)
                                return;
                            applyLaneTool (processor, idFor, loopLength, (LaneTool) result, laneName);
                            safe->repaint();
                        });
}
} // namespace GenerativeWidgets

// SEQ's fixed colours (UI review 31, item 40): each lane its own, used for
// its name, bars and the knobs that belong to it, and each GENERATE box its
// own, as the FX rows have theirs. Taken from the section colour swatches
// (SectionColours::swatches), not a new palette; the skin's accent stays for
// what is lit or playing.
namespace SeqColours
{
inline juce::Colour swatch (int index) { return juce::Colour (SectionColours::swatches[(size_t) index]); }
inline juce::Colour velocity()   { return swatch (10); } // copper
inline juce::Colour stepLength() { return swatch (1); }  // gold
inline juce::Colour pitch()      { return swatch (4); }  // ice
inline juce::Colour euclid()     { return swatch (3); }  // green
inline juce::Colour chance()     { return swatch (2); }  // lime
inline juce::Colour range()      { return swatch (6); }  // periwinkle
inline juce::Colour ratchet()    { return swatch (8); }  // violet
inline juce::Colour snap()       { return swatch (5); }  // blue
inline juce::Colour strum()      { return swatch (7); }  // purple
inline juce::Colour spray()      { return swatch (9); }  // rose
} // namespace SeqColours

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
    // The choice a click on the switch brings back (the last one that was on).
    int getRemembered() const { return remembered; }

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

        // The button's own state: the look-and-feel's shared animator
        // slides the switch to it (it used to jump).
        if (button.getToggleState() != (now > 0))
        {
            button.setToggleState (now > 0, juce::dontSendNotification);
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

        // A plate under the ring (the corners of its square are not left
        // bare, N16-8 leftover): a soft radial wash, and a short
        // tick inside every step.
        {
            const auto plate = ringArea.reduced (0.0f, 0.0f);
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (on ? 0.10f : 0.04f), centre,
                                                     juce::Colours::white.withAlpha (0.02f), centre.translated (plate.getWidth() * 0.72f, 0.0f), true));
            g.fillRoundedRectangle (plate, 8.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawRoundedRectangle (plate.reduced (0.5f), 8.0f, 1.0f);

            for (int step = 0; step < steps; ++step)
                g.drawLine (juce::Line<float> (pointOnRing (centre, radius * 0.8f, step, steps), pointOnRing (centre, radius, step, steps)), 0.8f);
        }

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
        g.setFont (IlanaTheme::font (juce::jlimit (IlanaTheme::TextSize::tiny, 16.5f, radius * 0.35f), true));
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

        // The strip: every step in a row, hits tall, under the same step
        // ruler as the arp's and PROB SEQ's (review 8, I8-23): numbers over
        // their steps, every fourth stronger (every fourth only when the
        // steps are narrow), the step playing in the colour.
        // (The gap row below it (N16-8) is part of the block, so the block stands in the well's middle.)
        // (In a tall pane the bars take its height: the step numbers above them at the pane's top, the gaps under
        // them at its foot, no empty band above or below; they used half of it, UI review 31, item 22.)
        constexpr float numbersAbove = 17.0f, gapsBelow = 62.0f;
        const auto strip = area.getHeight() >= 200.0f
                               ? area.withTrimmedTop (numbersAbove).withTrimmedBottom (gapsBelow)
                               : area.withSizeKeepingCentre (area.getWidth(), juce::jlimit (juce::jmin (area.getHeight(), 46.0f),
                                                                                            juce::jmax (0.0f, area.getHeight() - 22.0f),
                                                                                            area.getHeight() * 0.5f));
        const auto width = strip.getWidth() / (float) steps;

        if (strip.getY() - 17.0f >= area.getY())
        {
            const auto every = width >= 16.0f ? 1 : 4;

            for (int step = 0; step < steps; step += every)
            {
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, step % 4 == 0));
                g.setColour (step == current && on ? colour : juce::Colours::white.withAlpha (step % 4 == 0 ? 0.6f : 0.32f));
                g.drawText (juce::String (step + 1),
                            juce::Rectangle<float> (strip.getX() + (float) step * width, strip.getY() - 17.0f, width, 15.0f).toNearestInt(),
                            every == 1 ? juce::Justification::centred : juce::Justification::centredLeft, false);
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

        // The rhythm as its gaps (N16-8): under the strip, a bracket from each
        // hit to the next, wrapping round at the end, with the distance in
        // steps: 7 hits in 16 reads 2 3 2 2 3 2 2.
        if (hits > 0 && hits <= steps)
        {
            std::vector<int> hitSteps;

            for (int step = 0; step < steps; ++step)
                if (euclidHit (step, hits, steps, rotate))
                    hitSteps.push_back (step);

            const auto top = strip.getBottom() + 14.0f;

            if (! hitSteps.empty() && top + 30.0f <= area.getBottom() + 40.0f)
            {
                const auto labelFont = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
                g.setFont (labelFont);
                g.setColour (IlanaTheme::Ui::text3);
                g.drawText ("GAP BETWEEN HITS, IN STEPS", juce::Rectangle<float> (strip.getX(), top - 2.0f, strip.getWidth(), 14.0f).toNearestInt(),
                            juce::Justification::centredLeft, false);
                const auto y = top + 28.0f;

                for (size_t k = 0; k < hitSteps.size(); ++k)
                {
                    const auto from = hitSteps[k];
                    const auto next = k + 1 < hitSteps.size() ? hitSteps[k + 1] : hitSteps.front() + steps;
                    const auto gap = next - from;
                    const auto x0 = strip.getX() + ((float) from + 0.5f) * width;
                    // (The last gap runs on past the strip's end, round to the first hit: it stops at the edge.)
                    const auto x1 = juce::jmin (strip.getRight() - width * 0.5f, strip.getX() + ((float) next + 0.5f) * width);
                    const auto wraps = next >= steps;

                    g.setColour (colour.withAlpha (on ? 0.7f : 0.3f));
                    g.fillRect (x0 - 1.0f, y - 5.0f, 2.0f, 10.0f);
                    g.setColour (colour.withAlpha (on ? 0.4f : 0.18f));
                    g.fillRect (x0, y - 0.5f, x1 - x0, 1.0f);

                    if (! wraps)
                    {
                        g.setColour (colour.withAlpha (on ? 0.7f : 0.3f));
                        g.fillRect (x1 - 1.0f, y - 5.0f, 2.0f, 10.0f);
                    }

                    if (x1 - x0 >= 14.0f)
                    {
                        g.setColour (juce::Colours::white.withAlpha (on ? 0.85f : 0.45f));
                        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                        g.drawText (juce::String (gap), juce::Rectangle<float> (x0, y + 4.0f, x1 - x0, 16.0f).toNearestInt(),
                                    juce::Justification::centred, false);
                    }
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        processorRef.getUndoManager().beginNewTransaction ("Euclid");
        GenerativeWidgets::switchOnIfOff (processorRef, "euc_on");
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
        if (IlanaAnim::showing (*this) && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    int dragHits = 0, dragRotate = 0;
};

// The probability sequencer's 16 steps in three lanes: CHANCE (bars),
// RANGE (how far above the key a step may land) and RATCHET (1-4 dots).
// Drag in a lane to draw values across steps (Shift-drag draws a straight
// line); double-click resets a step; right-click a lane for its tools.
class ProbSeqEditor : public juce::Component,
                      public juce::SettableTooltipClient,
                      private IlanaAnim::FrameTimer
{
public:
    ProbSeqEditor (IlanaSynthAudioProcessor& processor, juce::Colour colourIn)
        : processorRef (processor), colour (colourIn)
    {
        setTooltip ("Probability sequencer. Drag in CHANCE and RANGE to draw (Shift-drag for a straight line); click RATCHET to "
                    "cycle 1-4 repeats. Double-click a step to reset it. Click a step number to set how many steps loop. "
                    "Right-click a lane for Reset, Randomise, Shift, Ramp, Copy and Paste.");
        startTimerHz (30);
    }

    enum Lane { chance, range, ratchet, numLanes };

    static juce::String idFor (int lane, int step)
    {
        const auto n = juce::String (step + 1);
        return lane == chance ? "pseq_chance" + n : lane == range ? "pseq_range" + n : "pseq_ratchet" + n;
    }

    // Each lane its own colour (UI review 31, item 40).
    static juce::Colour laneColour (int lane)
    {
        return lane == chance ? SeqColours::chance() : lane == range ? SeqColours::range() : SeqColours::ratchet();
    }

    void paint (juce::Graphics& g) override
    {
        using GenerativeWidgets::read;
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto on = read (processorRef, "pseq_on") > 0.5f;
        const auto length = loopLength();
        const auto current = on ? processorRef.getEngineDisplayStep() : -1;
        const auto playingStep = current >= 0 ? current % length : -1;
        const auto mouse = getMouseXYRelative().toFloat();
        const auto hoverLane = isMouseOver() && dragLane < 0 ? laneAt (mouse) : -1;
        const auto hoverStep = hoverLane >= 0 ? stepAt (mouse.x) : -1;

        const auto lanes = laneBounds();
        // RANGE's numbers are semitones above the held note.
        const char* const names[] { "CHANCE", "RANGE  +st", "RATCHET" };

        for (int lane = 0; lane < numLanes; ++lane)
        {
            g.setColour (laneColour (lane).withAlpha (on ? 0.9f : 0.6f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (names[lane], labelArea (lanes[(size_t) lane]), juce::Justification::centredLeft);
        }

        // A value off its default, under the pointer or being drawn reads out in
        // the strip under its bars, as the ARP's lanes do (never on a bar: UI
        // review 31, item 3).
        const auto readout = [&] (juce::Rectangle<float> full, juce::Rectangle<float> bars, const juce::String& text,
                                  bool lit, float alpha)
        {
            const auto strip = bars.getHeight() < full.getHeight() ? full.withTrimmedTop (bars.getHeight())
                                                                   : full.withTrimmedTop (full.getHeight() - 15.0f);
            g.setColour (juce::Colours::white.withAlpha ((lit ? 1.0f : 0.85f) * juce::jmax (alpha, 0.5f)));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (text, strip.toNearestInt(), juce::Justification::centred, false);
        };

        for (int step = 0; step < 16; ++step)
        {
            const auto active = step < length;
            const auto alpha = (active ? 1.0f : 0.25f) * (on ? 1.0f : 0.55f);
            const auto playing = step == playingStep;

            if (playing)
            {
                g.setColour (colour.withAlpha (0.12f));
                g.fillRect (columnFor (step, lanes[0].getUnion (lanes[2])));
            }

            // Chance: a bar from the bottom of its lane.
            {
                const auto full = columnFor (step, lanes[chance]).reduced (1.5f, 0.0f);
                const auto cell = barsOf (full);
                const auto value = juce::jlimit (0.0f, 1.0f, read (processorRef, idFor (chance, step)));
                const auto hovered = hoverLane == chance && hoverStep == step;
                const auto dragging = dragLane == chance && dragStep == step;
                g.setColour (juce::Colours::white.withAlpha ((hovered ? 0.1f : 0.05f) * alpha));
                g.fillRoundedRectangle (full, 2.0f);
                g.setColour (laneColour (chance).withAlpha ((playing ? 1.0f : 0.8f) * alpha));
                g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - value)), 2.0f);

                if (value < 0.995f || hovered || dragging)
                    readout (full, cell, juce::String (juce::roundToInt (value * 100.0f)) + "%", hovered || dragging, alpha);
            }

            // Range: a bar, its semitones in the strip under it.
            {
                const auto full = columnFor (step, lanes[range]).reduced (1.5f, 0.0f);
                const auto cell = barsOf (full);
                const auto value = juce::jlimit (0, 24, juce::roundToInt (read (processorRef, idFor (range, step))));
                const auto hovered = hoverLane == range && hoverStep == step;
                const auto dragging = dragLane == range && dragStep == step;
                g.setColour (juce::Colours::white.withAlpha ((hovered ? 0.1f : 0.05f) * alpha));
                g.fillRoundedRectangle (full, 2.0f);

                if (value > 0)
                {
                    g.setColour (laneColour (range).withAlpha ((playing ? 1.0f : 0.8f) * alpha));
                    g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - (float) value / 24.0f)), 2.0f);
                }

                if (value > 0 || hovered || dragging)
                    readout (full, cell, value > 0 ? "+" + juce::String (value) : juce::String ("0"), hovered || dragging, alpha);
            }

            // Ratchet: 1-4 ticks stacked from the lane's foot (more ticks, more repeats).
            {
                const auto full = columnFor (step, lanes[ratchet]).reduced (cellInset, 0.0f);
                const auto value = juce::jlimit (1, 4, juce::roundToInt (read (processorRef, idFor (ratchet, step))));
                const auto hovered = hoverLane == ratchet && hoverStep == step;
                g.setColour (juce::Colours::white.withAlpha ((hovered ? 0.1f : 0.05f) * alpha));
                g.fillRoundedRectangle (full, 2.0f);

                const auto tickHeight = 4.0f, tickGap = 2.0f;
                const auto tickArea = full.reduced (full.getWidth() * 0.18f, tickInset);

                for (int r = 0; r < value; ++r)
                {
                    g.setColour (laneColour (ratchet).withAlpha ((value > 1 ? 0.9f : 0.45f) * (playing ? 1.0f : 0.85f) * alpha));
                    g.fillRoundedRectangle ({ tickArea.getX(), tickArea.getBottom() - (float) (r + 1) * tickHeight - (float) r * tickGap,
                                              tickArea.getWidth(), tickHeight }, 1.5f);
                }
            }
        }

        // The step ruler, as the arp's (review 7, I7-29): every fourth
        // number stronger, the last step played in the colour. While off the
        // lanes only dim (the PATTERN header says why).
        {
            const auto ruler = rulerBounds();
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("STEP", labelArea (ruler).toNearestInt(), juce::Justification::centredLeft);

            for (int step = 0; step < 16; ++step)
            {
                const auto active = step < length;
                g.setColour (step == length - 1 ? colour.withAlpha (on ? 1.0f : 0.5f)
                                                : juce::Colours::white.withAlpha ((step % 4 == 0 ? 0.6f : 0.32f) * (active ? 1.0f : 0.5f)));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, step % 4 == 0 || step == length - 1));
                g.drawText (juce::String (step + 1), columnFor (step, ruler).toNearestInt(), juce::Justification::centred);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        // Right-click: the lane's tools, never a stroke (UI review 31, item 32).
        if (event.mods.isPopupMenu())
        {
            if (const auto lane = laneAt (event.position); lane >= 0)
                showLaneMenu (lane);
            return;
        }

        processorRef.getUndoManager().beginNewTransaction ("Probability sequencer");
        dragLane = laneAt (event.position);
        dragStep = -1;

        // (A click on a lane's name draws nothing.)
        if (dragLane >= 0 && stepAt (event.position.x) < 0)
        {
            dragLane = -1;
            return;
        }

        if (dragLane >= 0)
            GenerativeWidgets::switchOnIfOff (processorRef, "pseq_on");

        // A step number sets how many steps loop, as on the arp's ruler.
        if (dragLane < 0)
        {
            if (const auto step = stepAt (event.position.x); step >= 0)
                GenerativeWidgets::write (processorRef, "pseq_length", (float) (step + 1));

            return;
        }

        if (dragLane == ratchet)
        {
            if (const auto step = stepAt (event.position.x); step >= 0)
            {
                const auto id = idFor (ratchet, step);
                const auto value = juce::roundToInt (GenerativeWidgets::read (processorRef, id));
                GenerativeWidgets::write (processorRef, id, (float) (value % 4 + 1));
            }

            dragLane = -1;
            return;
        }

        // Shift-drag draws a straight line from here (the steps it leaves go back as they were).
        lineMode = event.mods.isShiftDown();
        lineStart = event.position;
        for (int step = 0; step < 16; ++step)
            before[(size_t) step] = GenerativeWidgets::read (processorRef, idFor (dragLane, step));
        drawBetween (event.position, event.position);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragLane != chance && dragLane != range)
            return;

        if (lineMode)
        {
            restoreOutside (lineStart.x, event.position.x);
            drawBetween (lineStart, event.position);
        }
        else
        {
            drawBetween (lastPosition, event.position);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragLane = dragStep = -1;
        lineMode = false;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (laneAt (event.position) < 0)
            return;

        if (const auto step = stepAt (event.position.x); step >= 0)
        {
            processorRef.performEdit ("Reset prob step", [this, step]
            {
                GenerativeWidgets::write (processorRef, idFor (chance, step), 1.0f);
                GenerativeWidgets::write (processorRef, idFor (range, step), 0.0f);
                GenerativeWidgets::write (processorRef, idFor (ratchet, step), 1.0f);
            });
        }
    }

    // A lane's tool, as its right-click menu runs it (the UI test calls it too).
    void applyLaneTool (int lane, GenerativeWidgets::LaneTool tool)
    {
        const auto which = juce::jlimit (0, (int) numLanes - 1, lane);
        GenerativeWidgets::applyLaneTool (processorRef, [which] (int step) { return idFor (which, step); }, loopLength(), tool, laneName (which));
        repaint();
    }

    // The UI test reaches the lanes through these.
    juce::Point<float> cellCentre (int lane, int step) const
    {
        return columnFor (step, lane < 0 ? rulerBounds() : laneBounds()[(size_t) juce::jlimit (0, 2, lane)]).getCentre();
    }
    juce::Rectangle<float> laneArea (int lane) const { return laneBounds()[(size_t) juce::jlimit (0, 2, lane)]; }
    juce::Rectangle<float> barsArea (int lane) const { return barsOf (laneArea (lane)); }

private:
    static constexpr float labelWidth = 66.0f;
    static constexpr float rulerHeight = 15.0f, readoutHeight = 14.0f, ratchetHeight = 28.0f,
                                cellInset = 1.5f, tickInset = 3.0f;

    static juce::String laneName (int lane) { return lane == chance ? "CHANCE" : lane == range ? "RANGE" : "RATCHET"; }

    int loopLength() const { return juce::jlimit (1, 16, juce::roundToInt (GenerativeWidgets::read (processorRef, "pseq_length"))); }

    void showLaneMenu (int lane)
    {
        GenerativeWidgets::showLaneMenu (processorRef, *this, [lane] (int step) { return idFor (lane, step); }, loopLength(), laneName (lane));
    }

    juce::Rectangle<float> rulerBounds() const
    {
        return getLocalBounds().toFloat().reduced (6.0f, 4.0f).removeFromTop (rulerHeight);
    }

    std::array<juce::Rectangle<float>, 3> laneBounds() const
    {
        auto area = getLocalBounds().toFloat().reduced (6.0f, 4.0f);
        area.removeFromTop (rulerHeight + 2.0f);
        std::array<juce::Rectangle<float>, 3> lanes;
        // RATCHET is a compact lane of stacked ticks; the height it leaves goes to CHANCE and RANGE (5 : 3).
        lanes[2] = area.removeFromBottom (ratchetHeight);
        const auto rest = area.getHeight();
        lanes[0] = area.removeFromTop (rest * 0.625f).withTrimmedBottom (3.0f);
        lanes[1] = area.withTrimmedBottom (3.0f);
        return lanes;
    }

    // The bars' part of CHANCE and RANGE: the readout strip at their foot, as the arp's lanes have.
    static juce::Rectangle<float> barsOf (juce::Rectangle<float> lane)
    {
        return lane.getHeight() >= 36.0f ? lane.withTrimmedBottom (readoutHeight) : lane;
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

        if (position.y < rulerBounds().getBottom())
            return -1; // the ruler only reads

        for (int lane = 0; lane < 3; ++lane)
            if (position.y <= lanes[(size_t) lane].getBottom() + 1.5f)
                return lane;

        return 2;
    }

    // The value at `y` in the dragged lane.
    float valueAt (float y) const
    {
        const auto bars = barsOf (laneBounds()[(size_t) dragLane]);
        const auto unit = juce::jlimit (0.0f, 1.0f, (bars.getBottom() - y) / juce::jmax (1.0f, bars.getHeight()));
        return dragLane == chance ? unit : (float) juce::roundToInt (unit * 24.0f);
    }

    // Draws the dragged lane between two points, so a fast stroke leaves no step out.
    void drawBetween (juce::Point<float> from, juce::Point<float> to)
    {
        if (dragLane != chance && dragLane != range)
            return;

        const auto grid = laneBounds()[0].withTrimmedLeft (labelWidth);
        const auto first = stepAt (juce::jlimit (grid.getX(), grid.getRight(), juce::jmin (from.x, to.x)));
        const auto last = stepAt (juce::jlimit (grid.getX(), grid.getRight(), juce::jmax (from.x, to.x)));

        for (auto step = first; step >= 0 && step <= last; ++step)
        {
            const auto centreX = columnFor (step, laneBounds()[0]).getCentreX();
            const auto t = std::abs (to.x - from.x) < 1.0f ? 1.0f : juce::jlimit (0.0f, 1.0f, (centreX - from.x) / (to.x - from.x));
            GenerativeWidgets::write (processorRef, idFor (dragLane, step), valueAt (from.y + (to.y - from.y) * t));
        }

        dragStep = stepAt (juce::jlimit (grid.getX(), grid.getRight(), to.x));
        lastPosition = to;
        repaint();
    }

    // A straight line's steps outside [a, b] go back to what they were.
    void restoreOutside (float a, float b)
    {
        const auto grid = laneBounds()[0].withTrimmedLeft (labelWidth);
        const auto first = stepAt (juce::jlimit (grid.getX(), grid.getRight(), juce::jmin (a, b)));
        const auto last = stepAt (juce::jlimit (grid.getX(), grid.getRight(), juce::jmax (a, b)));
        for (int step = 0; step < 16; ++step)
            if (step < first || step > last)
                GenerativeWidgets::write (processorRef, idFor (dragLane, step), before[(size_t) step]);
    }

    void timerCallback() override
    {
        if (IlanaAnim::showing (*this) && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    int dragLane = -1, dragStep = -1;
    bool lineMode = false;
    juce::Point<float> lineStart, lastPosition;
    std::array<float, 16> before {};
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
        setTooltip (juce::String::fromUTF8 ("Arp step lanes. Drag in VEL (velocity), STEP LEN (times the GATE knob's length; 0 rests the step) and PITCH to draw "
                                            "(Shift-drag for a straight line); drawing switches the arp on. Double-click a step to reset it. Click a step number "
                                            "to set how many steps loop. Right-click a lane for Reset, Randomise, Shift, Ramp, Copy and Paste."));
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

        // STEP LEN scales the GATE knob's length (review 9, I9-13: the lane is
        // not a second GATE), in the ratio style.
        if (lane == gate)
            return value < 0.005f ? juce::String ("rest") : juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + juce::String (value, 2);

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
        // Off (or played over by PROB SEQ): the lanes step back, with no
        // plate over them (review 7, S7-19), still readable, and an OFF
        // badge in the ruler's corner (review 8, I8-24, S8-32); the PATTERN
        // header says why.
        const auto alpha = on && ! replaced ? 1.0f : 0.45f;
        const auto ruler = rulerBounds();
        const auto lanes = laneBounds();
        // The lane names as the clip roll's (VEL) and GENERATE's (VEL RND):
        // one word for one thing (review 8, I8-23).
        const juce::String names[] { "VEL", "STEP LEN", "PITCH (st)" };
        const auto mouse = getMouseXYRelative().toFloat();
        const auto hoverLane = isMouseOver() && dragLane < 0 ? laneAt (mouse) : -1;
        const auto hoverStep = hoverLane >= 0 ? stepAt (mouse.x) : -1;

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));

        for (int lane = 0; lane < numLanes; ++lane)
        {
            // The name in the lane's colour, its scale at the right of the
            // label column: top, centre (GATE's 100 %, PITCH's 0) and foot.
            const auto area = lanes[(size_t) lane];
            g.setColour (laneColour (lane).withAlpha (0.55f + 0.35f * alpha));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            // (The name rides in the upper half, between the scale's top and middle marks, which the name's width would touch.)
            const auto nameRow = barsOf (area).getHeight() >= 44.0f
                                     ? juce::Rectangle<float> (area.getX(), area.getY(), (float) labelWidth, barsOf (area).getHeight() * 0.5f)
                                     : area.withWidth (labelWidth);
            g.drawText (names[lane], nameRow.toNearestInt(), juce::Justification::centredLeft);

            const auto ticks = area.withX (area.getX() + labelWidth - 30.0f).withWidth (26.0f);
            const auto tick = [&g, &ticks] (const juce::String& text, float y)
            {
                g.drawText (text, juce::Rectangle<float> (ticks.getX(), y - 6.0f, ticks.getWidth(), 12.0f).toNearestInt(),
                            juce::Justification::centredRight, false);
            };
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));

            if (area.getHeight() >= 30.0f)
            {
                const auto bars = barsOf (area);
                const auto top = bars.getY() + 5.0f, bottom = bars.getBottom() - 5.0f;
                if (lane == velocity)
                {
                    tick ("127", top);
                    tick ("1", bottom);
                }
                else if (lane == gate)
                {
                    tick (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + "2", top);
                    if (area.getHeight() >= 44.0f)
                        tick (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + "1", bars.getCentreY());
                    tick ("0", bottom);
                }
                else
                {
                    tick ("+12", top);
                    if (area.getHeight() >= 44.0f)
                        tick ("0", bars.getCentreY());
                    tick (juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + "12", bottom);
                }
            }

            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        }

        // PITCH's 0 st line runs the whole lane, over the cells' gaps.
        {
            const auto lane = barsOf (lanes[(size_t) pitch]).withTrimmedLeft (labelWidth);
            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.fillRect (lane.getX(), lane.getCentreY() - 0.5f, lane.getWidth(), 1.0f);
        }

        // (While off, the corner says so instead: "ARP OFF", not a bare OFF
        // beside STEP; drawing in the lanes switches it on. Review 9, S9-16.)
        if (on)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText ("STEP", ruler.withWidth (labelWidth).toNearestInt(), juce::Justification::centredLeft);
        }

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
                const auto full = columnFor (step, lanes[(size_t) lane]).reduced (1.5f, 0.0f);
                const auto cell = barsOf (full);
                const auto value = read (processorRef, idFor (lane, step));
                const auto unit = juce::jlimit (0.0f, 1.0f, toUnit (lane, value));
                const auto edited = std::abs (value - defaultFor (lane)) > 1.0e-3f;

                const auto hovered = lane == hoverLane && step == hoverStep;
                g.setColour (juce::Colours::white.withAlpha ((hovered ? 0.1f : on ? 0.045f : 0.02f) * (active ? 1.0f : 0.6f)));
                g.fillRoundedRectangle (full, 2.0f);
                // Off, the bars also lose most of their colour, so they read
                // as stored, not playing (I8-24), while the values stay clear.
                // (A lane's bars are the engine's own look: while it is off they
                // are plain grey, not the lane's colour, so a stored pattern
                // never reads as a playing one: S10-13.)
                const auto fill = (on ? laneColour (lane) : IlanaTheme::Ui::text3.withAlpha (0.8f))
                                      .withAlpha ((playing || hovered ? 1.0f : 0.78f) * cellAlpha);

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
                    }                    else if (! on)
                    {
                        // The pattern the arp would play, dimmed: no transpose.
                        g.setColour (IlanaTheme::Ui::text3.withAlpha (0.6f * cellAlpha));
                        g.fillRoundedRectangle (juce::Rectangle<float> (cell.getX() + 2.0f, mid - 2.0f, cell.getWidth() - 4.0f, 4.0f), 2.0f);
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
                else if (! on && ! edited)
                {
                    // Off and never drawn in: the pattern the arp would play
                    // (every step at its default), as a thin tick at the bar's
                    // height rather than a filled bar, so the lane shows the
                    // pattern but never reads as in use or as a grey wall
                    // (V11-16, V15-1, review 18).
                    g.setColour (IlanaTheme::Ui::text3.withAlpha (0.55f * cellAlpha));
                    g.fillRoundedRectangle (juce::Rectangle<float> (cell.getX() + 3.0f, cell.getBottom() - unit * cell.getHeight() - 1.5f,
                                                                    cell.getWidth() - 6.0f, 3.0f), 1.5f);

                    if (lane == gate)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.18f * cellAlpha));
                        g.fillRect (cell.getX(), cell.getCentreY() - 0.5f, cell.getWidth(), 1.0f);
                    }
                }
                else
                {
                    g.setColour (on ? fill : fill.withMultipliedAlpha (0.35f));
                    g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - unit)), 2.0f);

                    // GATE's 100 % line: where the GATE knob's length is.
                    if (lane == gate)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.22f * cellAlpha));
                        g.fillRect (cell.getX(), cell.getCentreY() - 0.5f, cell.getWidth(), 1.0f);
                    }
                }

                // A value moved off its default, under the pointer, or being
                // drawn, reads out, in every lane the same way (review 8,
                // S8-15; review 9, S9-13): in the readout row at the foot of
                // its lane, never over a bar. (A lane too short for the row
                // prints at its foot, as before.)
                const auto dragging = lane == dragLane && step == dragStep;

                if (edited || dragging || hovered)
                {
                    const auto strip = cell.getHeight() < full.getHeight() ? full.withTrimmedTop (cell.getHeight())
                                                                           : full.withTrimmedTop (full.getHeight() - 15.0f);
                    g.setColour (juce::Colours::white.withAlpha ((dragging || hovered ? 1.0f : 0.85f) * (active ? 1.0f : 0.5f)
                                                                 * juce::jmax (alpha, 0.7f)));
                    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                    g.drawText (textFor (lane, value), strip.toNearestInt(), juce::Justification::centred, false);
                }
            }
        }

        if (! on)
        {
            // (The centre pill says it; a corner badge and a header note
            // said the same a second and third time, review 21.)
            // The way on, where the eye is: drawing a step switches the ARP on
            // (UI review 13, S13-11), as the VECTOR pad says what switches it.
            const auto font = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            const juce::String text ("DRAW A STEP TO TURN THE ARP ON");
            const auto width = juce::GlyphArrangement::getStringWidth (font, text) + 36.0f;
            const auto pill = juce::Rectangle<float> (width, 30.0f).withCentre (getLocalBounds().toFloat().getCentre());
            g.setColour (IlanaTheme::Ui::panel.withAlpha (0.92f));
            g.fillRoundedRectangle (pill, 15.0f);
            g.setColour (IlanaTheme::accent().withAlpha (0.6f));
            g.drawRoundedRectangle (pill.reduced (0.5f), 15.0f, 1.0f);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (font);
            g.drawText (text, pill, juce::Justification::centred, false);
        }
    }

    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    // Each lane its own fixed colour for its name, bars and knobs (UI review 31,
    // item 40; they were three shades of the accent).
    juce::Colour laneColour (int lane) const
    {
        return lane == velocity ? SeqColours::velocity() : lane == gate ? SeqColours::stepLength() : SeqColours::pitch();
    }

    // A lane's tool, as its right-click menu runs it (the UI test calls it too).
    void applyLaneTool (int lane, GenerativeWidgets::LaneTool tool)
    {
        const auto which = juce::jlimit (0, (int) numLanes - 1, lane);
        GenerativeWidgets::applyLaneTool (processorRef, [which] (int step) { return idFor (which, step); }, numSteps(), tool, laneName (which));
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        // Right-click on a lane (its name or its bars): the lane's tools, never a
        // stroke (UI review 31, item 32).
        if (event.mods.isPopupMenu())
        {
            if (const auto lane = laneAtY (event.position.y); lane >= 0)
                GenerativeWidgets::showLaneMenu (processorRef, *this, [lane] (int step) { return idFor (lane, step); }, numSteps(), laneName (lane));
            return;
        }

        dragLane = laneAt (event.position);
        dragStep = -1;

        // A step number sets the loop's length.
        if (dragLane < 0)
        {
            if (const auto step = stepAt (event.position.x); step >= 0)
                processorRef.performEdit ("Arp steps", [this, step] { GenerativeWidgets::write (processorRef, "arp_steps", (float) (step + 1)); });

            return;
        }

        processorRef.beginEdit (juce::String ("Arp ") + (dragLane == velocity ? "velocity" : dragLane == gate ? "step length" : "transpose"));
        editOpen = true;
        GenerativeWidgets::switchOnIfOff (processorRef, "arp_on");
        lastPosition = lineStart = event.position;
        // Shift-drag draws a straight line from here; the steps it leaves go back as they were.
        lineMode = event.mods.isShiftDown();
        for (int step = 0; step < 16; ++step)
            before[(size_t) step] = GenerativeWidgets::read (processorRef, idFor (dragLane, step));
        drawAt (event.position, event.position);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! editOpen)
            return;

        if (lineMode)
        {
            const auto lane = laneBounds()[(size_t) dragLane];
            const auto first = stepAt (juce::jlimit (lane.getX() + labelWidth, lane.getRight(), juce::jmin (lineStart.x, event.position.x)));
            const auto last = stepAt (juce::jlimit (lane.getX() + labelWidth, lane.getRight(), juce::jmax (lineStart.x, event.position.x)));
            for (int step = 0; step < 16; ++step)
                if (step < first || step > last)
                    GenerativeWidgets::write (processorRef, idFor (dragLane, step), before[(size_t) step]);
            drawAt (lineStart, event.position);
        }
        else
        {
            drawAt (lastPosition, event.position);
        }
        lastPosition = event.position;
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (editOpen)
            processorRef.endEdit();

        editOpen = false;
        lineMode = false;
        dragLane = dragStep = -1;
        repaint();
    }

    ~ArpLanesEditor() override
    {
        if (editOpen)
        {
            processorRef.endEdit();
            editOpen = false;
        }
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
    static constexpr float labelWidth = 66.0f; // the name, then the scale (PROB SEQ's gutter: step 1 stays put)
    static constexpr float rulerHeight = 15.0f, readoutHeight = 14.0f;

    int numSteps() const { return juce::jlimit (1, 16, juce::roundToInt (GenerativeWidgets::read (processorRef, "arp_steps"))); }

    static juce::String laneName (int lane) { return lane == velocity ? "VEL" : lane == gate ? "STEP LEN" : "PITCH"; }

    // The lane at `y`, its name column included (-1 on the ruler).
    int laneAtY (float y) const
    {
        if (y < rulerBounds().getBottom() + 1.0f)
            return -1;

        const auto lanes = laneBounds();
        for (int lane = 0; lane < numLanes; ++lane)
            if (y <= lanes[(size_t) lane].getBottom() + 2.0f)
                return lane;

        return pitch;
    }

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

    // The bars' part of a lane: a readout row at its foot, the same place in
    // every lane (review 9, S9-13), when the lane is tall enough for both.
    static juce::Rectangle<float> barsOf (juce::Rectangle<float> lane)
    {
        return lane.getHeight() >= 36.0f ? lane.withTrimmedBottom (readoutHeight) : lane;
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
        const auto bars = barsOf (lane);
        const auto first = stepAt (juce::jlimit (lane.getX() + labelWidth, lane.getRight(), juce::jmin (from.x, to.x)));
        const auto last = stepAt (juce::jlimit (lane.getX() + labelWidth, lane.getRight(), juce::jmax (from.x, to.x)));

        for (auto step = first; step >= 0 && step <= last; ++step)
        {
            const auto centreX = columnFor (step, lane).getCentreX();
            const auto t = std::abs (to.x - from.x) < 1.0f ? 1.0f : juce::jlimit (0.0f, 1.0f, (centreX - from.x) / (to.x - from.x));
            const auto y = from.y + (to.y - from.y) * t;
            const auto unit = (bars.getBottom() - y) / juce::jmax (1.0f, bars.getHeight());
            GenerativeWidgets::write (processorRef, idFor (dragLane, step), fromUnit (dragLane, unit));
        }

        dragStep = stepAt (juce::jlimit (lane.getX() + labelWidth, lane.getRight(), to.x));
        repaint();
    }

    void timerCallback() override
    {
        const auto signature = processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)
                             ^ ((juce::uint64) (processorRef.getEngineDisplayStep() + 1) << 44);

        if (IlanaAnim::showing (*this) && changeGate.check (signature))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;
    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    int dragLane = -1, dragStep = -1;
    bool editOpen = false, lineMode = false;
    juce::Point<float> lastPosition, lineStart;
    std::array<float, 16> before {};
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

    // The tab shown below: while its engine is off, the note says so (the
    // displays only dim: review 7, S7-19, no plate over their data).
    void setShownEngine (int engine)
    {
        if (engine != shownEngine)
        {
            shownEngine = engine;
            repaint();
        }
    }

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

        // The engines in their tab order, ARP, EUCLID, PROB SEQ, CLIP, each
        // opening its tab (review 12, S12-4).
        if (arpOn)
            list.push_back ({ "ARP", 0, seqOn });
        if (euclidNotes)
            list.push_back ({ "EUCLID", 1 });
        if (seqOn)
            list.push_back ({ "PROB SEQ", 2 });
        if (clipOn)
            list.push_back ({ clipHost ? "CLIP + HOST" : "CLIP", 3 });

        list.push_back ({ "VOICES", -1, false, true });

        if (euclidOn && euclidTarget != 0)
            list.push_back ({ euclidTarget == 1 ? "EUCLID: STRINGS" : "EUCLID: GATE", 1 });

        const auto engine = seqOn ? juce::String ("PROB SEQ") : arpOn ? juce::String ("ARP") : juce::String();
        const bool engineOn[] { arpOn, euclidOn, seqOn, clipOn };
        const char* const offNotes[] { "",
                                       "EUCLID is off: drag to switch it on.",
                                       "PROB SEQ is off: draw to switch it on.",
                                       "CLIP is off: switch it on in its tab." };

        // The order follows the chain: how the stages combine, the last
        // stage's part last (review 7, S7-37).
        if (juce::isPositiveAndBelow (shownEngine, 4) && ! engineOn[shownEngine])
            note = offNotes[shownEngine];
        else if (seqOn && arpOn)
            note = "PROB SEQ replaces the ARP.";
        else if (euclidNotes && engine.isNotEmpty() && clipOn && ! clipHost)
            note = "EUCLID gates; steps transpose CLIP.";
        else if (euclidNotes && clipOn && ! clipHost)
            note = "EUCLID's hits trigger the clip.";
        else if (euclidNotes && engine.isNotEmpty())
            note = "EUCLID rests " + engine + "'s steps.";
        else if (euclidNotes)
            note = "EUCLID plays the held chord.";
        else if (clipOn && ! clipHost && engine.isNotEmpty())
            note = engine + " transposes the clip.";
        else if (clipOn && ! clipHost)
            note = "A held key plays the clip.";
        else if (clipOn)
            note = "The clip plays with the host.";
        else if (euclidOn && euclidTarget == 1)
            note = "EUCLID re-strikes the strings.";
        else if (euclidOn)
            note = "EUCLID drives the Trance Gate.";
        else if (engine.isEmpty())
            note = "No pattern engine is on.";
        else if (seqOn)
            note = "Each step rolls its chance.";
        else
            note = "The ARP plays held keys.";

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
        if (IlanaAnim::showing (*this) && changeGate.check (processorRef.getUiEpoch()))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;
    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    int shownEngine = -1;
    std::vector<std::pair<juce::Rectangle<float>, int>> chips;
};
