// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

#include "../FxDisplays.h"

namespace
{
class TapGrid : public juce::Component,
                private IlanaAnim::FrameTimer
{
public:
    explicit TapGrid (IlanaSynthAudioProcessor& p)
        : processorRef (p)
    {
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, area, 6.0f);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("CUSTOM TAP GRID", getLocalBounds().removeFromTop (14).reduced (9, 0),
                    juce::Justification::centredLeft);

        auto bars = getBarBounds().toFloat();
        const auto columnWidth = bars.getWidth() / 16.0f;

        for (int step = 0; step < 16; ++step)
        {
            auto cell = bars.withWidth (columnWidth).reduced (1.5f, 0.0f);
            bars.removeFromLeft (columnWidth);

            const auto value = getStep (step);

            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (cell, 2.0f);

            if (value > 0.001f)
            {
                g.setColour (IlanaTheme::accent().withAlpha (step == hoverStep ? 0.95f : 0.65f));
                g.fillRoundedRectangle (cell.withTop (cell.getBottom() - cell.getHeight() * value), 2.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override { setFromMouse (event); }
    void mouseDrag (const juce::MouseEvent& event) override { setFromMouse (event); }
    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto step = stepAt (event.getPosition());

        if (step != hoverStep)
        {
            hoverStep = step;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverStep != -1)
        {
            hoverStep = -1;
            repaint();
        }
    }

private:
    juce::Rectangle<int> getBarBounds() const
    {
        return getLocalBounds().reduced (8, 9).withTrimmedTop (12);
    }

    int stepAt (juce::Point<int> position) const
    {
        const auto bars = getBarBounds();
        const auto columnWidth = (float) bars.getWidth() / 16.0f;

        return juce::jlimit (0, 15, (int) ((float) (position.x - bars.getX()) / columnWidth));
    }

    float getStep (int index) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("fx_taps_step" + juce::String (index + 1)))
            return value->load();

        return 0.0f;
    }

    void setFromMouse (const juce::MouseEvent& event)
    {
        const auto bars = getBarBounds();
        const auto index = stepAt (event.getPosition());
        const auto value = juce::jlimit (0.0f, 1.0f,
                                         1.0f - (float) (event.getPosition().y - bars.getY()) / (float) juce::jmax (1, bars.getHeight()));

        if (auto* parameter = processorRef.apvts.getParameter ("fx_taps_step" + juce::String (index + 1)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter ("fx_taps_pattern")))
            if (choice->getIndex() != 6)
                choice->setValueNotifyingHost (choice->convertTo0to1 (6.0f));

        hoverStep = index;
        repaint();
    }

    void timerCallback() override
    {
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    int hoverStep = -1;
};

// Each effect family has its own colour: drive/distortion warm, modulation
// blue-violet, time and space green-cyan, dynamics teal, filters/EQ pink.
inline juce::Colour fxColour (int type)
{
    // One colour per category, as the library groups them (it read as
    // confetti when colours followed the individual effects).
    switch (type)
    {
        // (The Airwindows category modules, 32-41, wear their function's.)
        case 13: case 9: case 15: case 11: case 12: case 8: case 22: case 34: case 35: case 41:
            return juce::Colour (0xff5cc4e8); // space: reverb, delay, dimension, smear, freeze, haas, widener, AW reverb/delay/stereo
        case 2: case 1: case 3: case 26: case 28: case 30: case 32: case 33: case 39: case 40:
            return juce::Colour (0xffff8a5c); // drive: drive, amp, crush, octaver, feedback, airwindows, AW tape/saturation/console/lo-fi
        case 7: case 6: case 14: case 23: case 24: case 25: case 27: case 5: case 31: case 36:
            return juce::Colour (0xff9a8cff); // motion: chorus .. comb, vocoder, AW modulation
        case 16: case 10: case 17:
            return juce::Colour (0xff7ad98e); // rhythm: trance gate, stutter, tape stop
        case 29: case 18: case 4: case 20: case 21: case 19: case 37: case 38:
            return juce::Colour (0xffe0c35c); // tone & level: eq, tilt, comp, ott, limiter, utility, AW dynamics/EQ
        default:
            return IlanaTheme::Ui::text3;
    }
}

// Trance gate steps: bar height is each step's level, the playing step
// lights up. Editing a built-in pattern copies it into Custom first.
class GateGrid : public juce::Component,
                 public juce::SettableTooltipClient,
                 private IlanaAnim::FrameTimer
{
public:
    explicit GateGrid (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("Drag up/down to set each step's level. Right-click a step to toggle it.");
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintWell (g, getLocalBounds().toFloat(), 6.0f);
        const auto colour = fxColour (16);
        const auto steps = numSteps();
        const auto playing = processorRef.getGateDisplayStep();
        auto bars = getBarBounds().toFloat();
        const auto columnWidth = bars.getWidth() / 16.0f;

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("STEPS", getLocalBounds().removeFromTop (14).reduced (9, 0), juce::Justification::centredLeft);

        for (int step = 0; step < 16; ++step)
        {
            const auto cell = bars.withWidth (columnWidth).reduced (1.5f, 0.0f);
            bars.removeFromLeft (columnWidth);
            const auto active = step < steps;
            const auto value = level (step);

            g.setColour (juce::Colours::white.withAlpha (active ? (step % 4 == 0 ? 0.11f : 0.07f) : 0.02f));
            g.fillRoundedRectangle (cell, 2.0f);

            if (active && value > 0.001f)
            {
                g.setColour (colour.withAlpha (step == playing ? 1.0f : (step == hoverStep ? 0.85f : 0.6f)));
                g.fillRoundedRectangle (cell.withTop (cell.getBottom() - cell.getHeight() * value), 2.0f);
            }

            if (active && step == playing)
            {
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.fillRect (cell.withHeight (2.0f).withY (cell.getBottom() + 2.0f));
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
        {
            const auto step = stepAt (event.getPosition());
            setLevel (step, level (step) > 0.5f ? 0.0f : 1.0f);
            return;
        }

        setFromMouse (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! event.mods.isPopupMenu())
            setFromMouse (event);
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto step = stepAt (event.getPosition());

        if (step != hoverStep)
        {
            hoverStep = step;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverStep = -1;
        repaint();
    }

private:
    juce::Rectangle<int> getBarBounds() const { return getLocalBounds().reduced (8, 10).withTrimmedTop (10); }

    int stepAt (juce::Point<int> position) const
    {
        const auto bars = getBarBounds();
        return juce::jlimit (0, 15, (int) ((float) (position.x - bars.getX()) / ((float) bars.getWidth() / 16.0f)));
    }

    int numSteps() const { return (int) processorRef.apvts.getRawParameterValue ("fx_gate_steps")->load(); }
    int pattern() const { return (int) processorRef.apvts.getRawParameterValue ("fx_gate_pattern")->load(); }

    float level (int step) const
    {
        if (pattern() == 8)
            return processorRef.apvts.getRawParameterValue ("fx_gate_step" + juce::String (step + 1))->load();

        return IlanaSynthAudioProcessor::gatePatternLevel (pattern(), step);
    }

    void setLevel (int step, float value)
    {
        // First edit of a built-in pattern: copy it into Custom.
        if (pattern() != 8)
        {
            for (int i = 0; i < 16; ++i)
                if (auto* parameter = processorRef.apvts.getParameter ("fx_gate_step" + juce::String (i + 1)))
                    parameter->setValueNotifyingHost (IlanaSynthAudioProcessor::gatePatternLevel (pattern(), i));

            if (auto* choice = processorRef.apvts.getParameter ("fx_gate_pattern"))
                choice->setValueNotifyingHost (choice->convertTo0to1 (8.0f));
        }

        if (auto* parameter = processorRef.apvts.getParameter ("fx_gate_step" + juce::String (step + 1)))
            parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));

        repaint();
    }

    void setFromMouse (const juce::MouseEvent& event)
    {
        const auto bars = getBarBounds();
        auto value = 1.0f - (float) (event.getPosition().y - bars.getY()) / (float) juce::jmax (1, bars.getHeight());

        // Snap near the ends so full and closed steps are easy to hit.
        value = value > 0.92f ? 1.0f : (value < 0.08f ? 0.0f : value);
        hoverStep = stepAt (event.getPosition());
        setLevel (hoverStep, value);
    }

    IlanaAnim::ChangeGate changeGate;

    void timerCallback() override
    {
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    int hoverStep = -1;
};

// An FX card's title as its type menu: the tag, the name and a chevron;
// a click opens the list of effects to swap this slot's module for.
class FxTypeButton : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    FxTypeButton()
    {
        setTooltip ("Change this slot's effect");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void()> onClick;

    void setTitle (const juce::String& newTitle, juce::Colour newColour)
    {
        if (newTitle == title && newColour == colour)
            return;
        title = newTitle;
        colour = newColour;
        repaint();
    }

    // Wide enough for the tag, the name and the chevron.
    int preferredWidth() const { return IlanaTheme::cardTitleWidth (title) + 4; }

    void paint (juce::Graphics& g) override
    {
        if (isMouseOver())
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().withTrimmedLeft (-4.0f), 5.0f);
        }

        IlanaTheme::paintCardTitle (g, getLocalBounds(), title, colour);

        // The chevron after the name.
        const auto x = (float) IlanaTheme::cardTitleWidth (title) - 12.0f;
        const auto y = (float) getHeight() * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (x, y - 2.0f);
        chevron.lineTo (x + 4.0f, y + 2.0f);
        chevron.lineTo (x + 8.0f, y - 2.0f);
        g.setColour (IlanaTheme::Ui::text2.withAlpha (isMouseOver() ? 1.0f : 0.7f));
        g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (onClick != nullptr && getLocalBounds().contains (event.getPosition()))
            onClick();
    }

private:
    juce::String title;
    juce::Colour colour;
};

// Scrolling content for the FX stack: paints each module's panel and hands
// clicks back to the page.
class FxStackContent : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> painter;
    std::function<void (juce::Point<int>)> onClick;

    void paint (juce::Graphics& g) override
    {
        if (painter != nullptr)
            painter (g);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (onClick != nullptr)
            onClick (event.getPosition());
    }
};

// An FX slot's on switch for modules without an on parameter of their own:
// on means playing, off bypasses the slot. Drawn like a ToggleControl switch
// (13 px of label space above the pill), so every module's header matches.
class SlotSwitch : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    SlotSwitch (IlanaSynthAudioProcessor& p, int slotIndex)
        : parameter (p.apvts.getParameter ("fx_slot" + juce::String (slotIndex + 1) + "_bypass"))
    {
        setTooltip ("On\nSwitch this effect off to bypass its slot.");
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintSwitch (g, getLocalBounds().withTrimmedTop (13).toFloat(), isOn() ? 1.0f : 0.0f,
                                 IlanaTheme::accent(), isMouseOver() ? 1.0f : 0.0f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (isOn() ? 1.0f : 0.0f);
        parameter->endChangeGesture();
        repaint();
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    bool isOn() const { return parameter == nullptr || parameter->getValue() < 0.5f; }

    void timerCallback() override
    {
        if (isShowing() && isOn() != shownOn)
        {
            shownOn = isOn();
            repaint();
        }
    }

    juce::RangedAudioParameter* parameter = nullptr;
    bool shownOn = true;
};
} // namespace
