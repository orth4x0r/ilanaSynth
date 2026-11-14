#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <optional>

// A slider tied to a parameter, as juce's SliderAttachment ties it, except
// that while the user is moving the slider (and for a moment after) the
// parameter does not move the slider back.
//
// Why: a VST3 host sends the plugin's own edits back to it through the audio
// callback, a block or more late (Ableton Live does on every drag). JUCE sets
// the parameter to that echoed, older value, and juce's attachment drew it:
// mid-drag the knob jumped back to where it was a frame ago, then forward on
// the next mouse move ("it regresses to the previous frame"). Here the slider
// keeps the user's value while they hold it; once they have let go for
// holdMs, it takes the parameter's current value (automation, a preset, undo
// still show as before).
class HeldSliderAttachment : private juce::Slider::Listener,
                             private juce::Timer
{
public:
    // How long after the user's last move the slider still ignores the
    // parameter: longer than a host's echo takes (a few blocks).
    static constexpr double holdMs = 250.0;

    HeldSliderAttachment (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID, juce::Slider& sliderIn)
        : HeldSliderAttachment (*state.getParameter (parameterID), sliderIn)
    {
    }

    HeldSliderAttachment (juce::RangedAudioParameter& param, juce::Slider& sliderIn)
        : slider (sliderIn),
          parameter (param),
          attachment (param, [this] (float value) { parameterMoved (value); }, nullptr)
    {
        slider.valueFromTextFunction = [&param] (const juce::String& text) { return (double) param.convertFrom0to1 (param.getValueForText (text)); };
        slider.textFromValueFunction = [&param] (double value) { return param.getText (param.convertTo0to1 ((float) value), 0); };
        slider.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));

        // (The parameter's own range, mapping and snapping, as juce's attachment sets them.)
        auto range = param.getNormalisableRange();
        auto from0to1 = [range] (double start, double end, double normalised) mutable
        {
            range.start = (float) start;
            range.end = (float) end;
            return (double) range.convertFrom0to1 ((float) normalised);
        };
        auto to0to1 = [range] (double start, double end, double mapped) mutable
        {
            range.start = (float) start;
            range.end = (float) end;
            return (double) range.convertTo0to1 ((float) mapped);
        };
        auto snap = [range] (double start, double end, double mapped) mutable
        {
            range.start = (float) start;
            range.end = (float) end;
            return (double) range.snapToLegalValue ((float) mapped);
        };
        juce::NormalisableRange<double> sliderRange { (double) range.start, (double) range.end,
                                                      std::move (from0to1), std::move (to0to1), std::move (snap) };
        sliderRange.interval = range.interval;
        sliderRange.skew = range.skew;
        sliderRange.symmetricSkew = range.symmetricSkew;
        slider.setNormalisableRange (sliderRange);

        attachment.sendInitialUpdate();
        slider.valueChanged();
        slider.addListener (this);
        registry().emplace (&parameter, this);
    }

    ~HeldSliderAttachment() override
    {
        stopTimer();
        slider.removeListener (this);
        auto& all = registry();
        for (auto it = all.begin(); it != all.end(); ++it)
            if (it->second == this)
            {
                all.erase (it);
                break;
            }
    }

    // The value a knob the user is moving shows (plain units), or nothing when
    // no knob of this parameter is held. A display drawn from the parameter
    // (the filter graph) reads this first, as Vital's graph reads its
    // sliders: the host's late echo moves the parameter back a frame, never
    // the knob, so the picture follows the hand. Message thread only.
    static std::optional<float> heldValue (const juce::AudioProcessorParameter& param)
    {
        const auto range = registry().equal_range (&param);
        for (auto it = range.first; it != range.second; ++it)
            if (it->second->isHeld())
                return (float) it->second->slider.getValue();
        return std::nullopt;
    }

    // True while the user holds the slider or moved it within holdMs.
    bool isHeld() const
    {
        return slider.isMouseButtonDown() || juce::Time::getMillisecondCounterHiRes() - lastUserMove < holdMs;
    }

private:
    void parameterMoved (float value)
    {
        if (isHeld())
        {
            // (Caught up with once the user lets go.)
            if (! isTimerRunning())
                startTimerHz (30);
            return;
        }

        show (value);
    }

    void show (float value)
    {
        const juce::ScopedValueSetter<bool> svs (ignoreCallbacks, true);
        slider.setValue (value, juce::sendNotificationSync);
    }

    void timerCallback() override
    {
        if (isHeld())
            return;

        stopTimer();
        // The parameter as it is now, not whatever stale value woke the timer.
        const auto value = parameter.convertFrom0to1 (parameter.getValue());
        if (std::abs (value - (float) slider.getValue()) > 1.0e-6f)
            show (value);
    }

    void sliderValueChanged (juce::Slider*) override
    {
        if (ignoreCallbacks)
            return;

        lastUserMove = juce::Time::getMillisecondCounterHiRes();
        attachment.setValueAsPartOfGesture ((float) slider.getValue());
    }

    void sliderDragStarted (juce::Slider*) override { attachment.beginGesture(); }
    void sliderDragEnded (juce::Slider*) override { attachment.endGesture(); }

    static std::multimap<const juce::AudioProcessorParameter*, HeldSliderAttachment*>& registry()
    {
        static std::multimap<const juce::AudioProcessorParameter*, HeldSliderAttachment*> all;
        return all;
    }

    juce::Slider& slider;
    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    bool ignoreCallbacks = false;
    double lastUserMove = -1.0e9;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeldSliderAttachment)
};
