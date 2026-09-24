#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "ParamInfo.h"

struct ModRingConfig
{
    Mod::Destination destination = Mod::Destination::None;
    float scale = 1.0f;
};

inline ModRingConfig modRingConfigFor (const juce::String& parameterID)
{
    if (parameterID == "osc1_frame") return { Mod::Destination::Osc1Frame, 1.0f };
    if (parameterID == "osc1_level") return { Mod::Destination::Osc1Level, 1.0f };
    if (parameterID == "osc1_semi" || parameterID == "osc1_fine") return { Mod::Destination::Osc1Pitch, 1.0f };
    if (parameterID == "osc1_pan") return { Mod::Destination::Pan, 0.5f };
    if (parameterID == "osc2_frame") return { Mod::Destination::Osc2Frame, 1.0f };
    if (parameterID == "osc2_level") return { Mod::Destination::Osc2Level, 1.0f };
    if (parameterID == "osc2_semi" || parameterID == "osc2_fine") return { Mod::Destination::Osc2Pitch, 1.0f };
    if (parameterID == "osc2_pan") return { Mod::Destination::Pan, 0.5f };
    if (parameterID == "sub_level") return { Mod::Destination::SubLevel, 1.0f };
    if (parameterID == "sub_semi" || parameterID == "sub_fine") return { Mod::Destination::SubPitch, 1.0f };
    if (parameterID == "sub_frame") return { Mod::Destination::SubFrame, 1.0f };
    if (parameterID == "osc1_sample_start") return { Mod::Destination::Osc1SampleStart, 0.5f };
    if (parameterID == "osc1_sample_end") return { Mod::Destination::Osc1SampleEnd, 0.5f };
    if (parameterID == "osc2_sample_start") return { Mod::Destination::Osc2SampleStart, 0.5f };
    if (parameterID == "osc2_sample_end") return { Mod::Destination::Osc2SampleEnd, 0.5f };
    if (parameterID == "sub_sample_start") return { Mod::Destination::SubSampleStart, 0.5f };
    if (parameterID == "sub_sample_end") return { Mod::Destination::SubSampleEnd, 0.5f };
    if (parameterID == "noise_level") return { Mod::Destination::NoiseLevel, 1.0f };
    if (parameterID == "f1_cutoff") return { Mod::Destination::Filter1Cutoff, 0.5f };
    if (parameterID == "f1_reso") return { Mod::Destination::Filter1Reso, 1.0f };
    if (parameterID == "f2_cutoff") return { Mod::Destination::Filter2Cutoff, 0.5f };
    if (parameterID == "f2_reso") return { Mod::Destination::Filter2Reso, 1.0f };
    if (parameterID == "lfo1_rate") return { Mod::Destination::Lfo1Rate, 0.3f };
    if (parameterID == "lfo2_rate") return { Mod::Destination::Lfo2Rate, 0.3f };

    return {};
}

inline juce::Colour modSourceColour (int sourceIndex)
{
    switch (sourceIndex)
    {
        case 1:  return juce::Colour (0xffff8a3b);
        case 2:  return juce::Colour (0xff35c8ff);
        case 3:  return juce::Colour (0xff8fff3b);
        case 4:  return juce::Colour (0xffff4fd8);
        case 5:  return juce::Colour (0xff5b8cff);
        case 6:
        case 7:
        case 8:  return juce::Colour (0xffbbbbbb);
        case 9:
        case 10:
        case 11: return juce::Colour (0xffb28aff);
        case 12:
        case 13:
        case 14:
        case 15: return juce::Colour (0xffffd447);
        case 16: return juce::Colour (0xffbbbbbb);
        case 17: return juce::Colour (0xff6fe3c1);
        case 18: return juce::Colour (0xffffd447);
        case 19: return juce::Colour (0xffb28aff);
        case 20: return juce::Colour (0xff6fe3c1);
        case 21: return juce::Colour (0xffe3a56f);
        default: return IlanaTheme::accent();
    }
}

inline juce::String& knobClipboard()
{
    static juce::String value;
    return value;
}

class KnobControl : public juce::Component,
                    public juce::DragAndDropTarget,
                    public juce::SettableTooltipClient,
                    public IlanaAnim::PageAnimated,
                    private juce::Timer
{
public:
    KnobControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                 const juce::String& labelText, juce::Colour accent = IlanaTheme::accent(),
                 bool followsThemeIn = true)
        : ringConfig (modRingConfigFor (parameterID)),
          parameterId (parameterID),
          knobAccent (accent),
          followsTheme (followsThemeIn)
    {
        processorRef = dynamic_cast<IlanaSynthAudioProcessor*> (&state.processor);

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        addAndMakeVisible (slider);
        slider.addMouseListener (this, false);

        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (IlanaTheme::font (12.0f));
        label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterID, slider);

        parameter = state.getParameter (parameterID);

        if (parameter != nullptr)
        {
            slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

            const auto description = describeParameter (parameterID);
            const auto tooltip = parameter->getName (64) + (description.isNotEmpty() ? "\n" + description : "");
            slider.setTooltip (tooltip);
            setTooltip (tooltip);
        }

        lastSliderValue = slider.getValue();
        startTimerHz (30);
    }

    void lookAndFeelChanged() override
    {
        if (followsTheme)
            slider.setColour (juce::Slider::rotarySliderFillColourId, IlanaTheme::accent());
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
            showModMenu();
    }

    juce::Slider& getSlider() { return slider; }

    void mouseEnter (const juce::MouseEvent&) override
    {
        hover = true;
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover = false;
        repaint();
    }

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

    void paint (juce::Graphics& g) override
    {
        const auto modActive = processorRef != nullptr
                               && ringConfig.destination != Mod::Destination::None
                               && std::abs (processorRef->getModDisplay (ringConfig.destination)) > 0.001f;
        const auto glowColour = modActive ? modSourceColour (dominantModSource()) : IlanaTheme::accent();
        const auto glowIntensity = juce::jmax (glow * 0.09f, activity * 0.2f);

        if (glowIntensity > 0.005f)
        {
            const auto centre = rotaryArea().getCentre();
            const auto knobRadius = knobRadiusFor (knobBounds);

            for (int ring = 2; ring >= 1; --ring)
            {
                const auto radius = knobRadius + (float) ring * 3.5f;
                g.setColour (glowColour.withAlpha (glowIntensity * (ring == 2 ? 0.4f : 0.7f)));
                g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
            }
        }

        if (dragHover)
        {
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.fillRoundedRectangle (knobBounds.toFloat().reduced (2.0f), 6.0f);
        }

        if (processorRef == nullptr || ringConfig.destination == Mod::Destination::None)
            return;

        const auto mod = processorRef->getModDisplay (ringConfig.destination);

        if (std::abs (mod) < 0.001f)
            return;

        const auto centre = rotaryArea().getCentre();
        const auto radius = knobRadiusFor (knobBounds);

        if (radius < 8.0f)
            return;

        const auto lineWidth = 2.0f;
        const auto arcRadius = radius - lineWidth * 0.5f;
        const auto startAngle = juce::MathConstants<float>::pi * 1.2f;
        const auto endAngle = juce::MathConstants<float>::pi * 2.8f;

        const auto baseNorm = (float) juce::jlimit (0.0, 1.0, slider.valueToProportionOfLength (slider.getValue()));
        const auto displayNorm = juce::jlimit (0.0f, 1.0f, baseNorm + mod * ringConfig.scale);

        if (std::abs (displayNorm - baseNorm) < 0.001f)
            return;

        const auto angleA = startAngle + baseNorm * (endAngle - startAngle);
        const auto angleB = startAngle + displayNorm * (endAngle - startAngle);

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                           juce::jmin (angleA, angleB), juce::jmax (angleA, angleB), true);

        g.setColour (modSourceColour (dominantModSource()).withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (13));
        knobBounds = area;
        slider.setBounds (area);
    }

    bool isInterestedInDragSource (const SourceDetails& details) override
    {
        return processorRef != nullptr
               && ringConfig.destination != Mod::Destination::None
               && details.description.toString().startsWith ("modsource:");
    }

    void itemDragEnter (const SourceDetails&) override
    {
        dragHover = true;
        repaint();
    }

    void itemDragExit (const SourceDetails&) override
    {
        dragHover = false;
        repaint();
    }

    void itemDropped (const SourceDetails& details) override
    {
        dragHover = false;

        if (processorRef == nullptr)
            return;

        const auto sourceIndex = details.description.toString()
                                     .fromFirstOccurrenceOf ("modsource:", false, false)
                                     .getIntValue();

        processorRef->assignModSlot (sourceIndex, ringConfig.destination, 0.35f);
        repaint();
    }

private:
    static float knobRadiusFor (juce::Rectangle<int> bounds)
    {
        auto area = bounds.toFloat();
        area.setHeight (juce::jmax (8.0f, area.getHeight() - 16.0f));
        return juce::jlimit (14.0f, 30.0f,
                             juce::jmin (area.getWidth(), area.getHeight()) * 0.5f);
    }

    // The rotary is drawn above the value text box, so glow and mod ring must
    // use the same area or they appear off-centre.
    juce::Rectangle<float> rotaryArea() const
    {
        auto area = knobBounds.toFloat();
        area.setHeight (juce::jmax (8.0f, area.getHeight() - 16.0f));
        return area;
    }

    int dominantModSource() const
    {
        if (processorRef == nullptr || ringConfig.destination == Mod::Destination::None)
            return 0;

        auto dominantSource = 0;
        auto dominantDepth = 0.0f;

        for (int i = 1; i <= Mod::maxSlots; ++i)
        {
            const auto prefix = "mod" + juce::String (i);

            const auto* destinationValue = processorRef->apvts.getRawParameterValue (prefix + "_dst");
            const auto* sourceValue = processorRef->apvts.getRawParameterValue (prefix + "_src");
            const auto* depthValue = processorRef->apvts.getRawParameterValue (prefix + "_amt");

            if (destinationValue == nullptr || sourceValue == nullptr || depthValue == nullptr)
                continue;

            if ((Mod::Destination) (int) destinationValue->load() != ringConfig.destination)
                continue;

            const auto depth = std::abs (depthValue->load());
            const auto source = (int) sourceValue->load();

            if (source != 0 && depth > dominantDepth)
            {
                dominantDepth = depth;
                dominantSource = source;
            }
        }

        return dominantSource;
    }

    void showModMenu()
    {
        if (processorRef == nullptr)
            return;

        juce::PopupMenu menu;

        if (ringConfig.destination != Mod::Destination::None)
        {
            const auto sources = Mod::getSourceNames();
            juce::PopupMenu sourceMenu;

            for (int i = 1; i < sources.size(); ++i)
                sourceMenu.addItem (i + 1, sources[i]);

            menu.addSubMenu ("Modulate with", sourceMenu);
            menu.addItem (1000, "Clear modulation to this target");
            menu.addSeparator();
        }

        if (parameter != nullptr)
            menu.addItem (2000, "Reset to default");

        menu.addSeparator();
        menu.addItem (3000, "Copy value");

        if (knobClipboard().isNotEmpty())
            menu.addItem (3001, "Paste value");

        if (parameterId.startsWith ("macro"))
        {
            const auto macroIndex = parameterId.getTrailingIntValue() - 1;

            menu.addSeparator();
            menu.addItem (4000, "MIDI Learn  (CC " + juce::String (processorRef->getMacroCc (macroIndex)) + ")");
        }

        juce::Component::SafePointer<KnobControl> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safeThis] (int result)
                            {
                                if (safeThis == nullptr || result == 0)
                                    return;

                                if (result == 1000)
                                    safeThis->processorRef->clearModSlotsForTarget (safeThis->ringConfig.destination);
                                else if (result == 2000)
                                {
                                    if (safeThis->parameter != nullptr)
                                        safeThis->parameter->setValueNotifyingHost (safeThis->parameter->getDefaultValue());
                                }
                                else if (result == 3000)
                                {
                                    knobClipboard() = juce::String (safeThis->slider.getValue(), 6);
                                }
                                else if (result == 3001)
                                {
                                    const auto value = knobClipboard().getDoubleValue();
                                    safeThis->slider.setValue (value, juce::sendNotificationSync);
                                }
                                else if (result == 4000)
                                {
                                    safeThis->processorRef->startMacroLearn (
                                        safeThis->parameterId.getTrailingIntValue() - 1);
                                }
                                else
                                {
                                    safeThis->processorRef->assignModSlot (result - 1, safeThis->ringConfig.destination, 0.35f);
                                }
                            });
    }

    void timerCallback() override
    {
        const auto targetGlow = hover ? 1.0f : 0.0f;
        glow += (targetGlow - glow) * 0.22f;
        appear = juce::jmin (1.0f, appear + 0.12f);
        slider.setAlpha (appear);
        label.setAlpha (appear);

        const auto value = slider.getValue();

        if (std::abs (value - lastSliderValue) > 1.0e-6)
        {
            lastSliderValue = value;
            activity = 1.0f;
        }

        activity *= 0.88f;

        if (processorRef == nullptr || ringConfig.destination == Mod::Destination::None)
        {
            if (glow > 0.01f || activity > 0.01f || appear < 0.999f || isMouseOver())
                repaint();

            return;
        }

        const auto modValue = processorRef->getModDisplay (ringConfig.destination);

        if (std::abs (modValue - lastModValue) > 0.002f)
        {
            lastModValue = modValue;
            repaint();
        }
        else if (glow > 0.01f || activity > 0.01f || appear < 0.999f || isMouseOver())
        {
            repaint();
        }
    }

    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    IlanaSynthAudioProcessor* processorRef = nullptr;
    juce::RangedAudioParameter* parameter = nullptr;
    ModRingConfig ringConfig;
    juce::String parameterId;
    juce::Colour knobAccent;
    bool followsTheme = false;
    juce::Rectangle<int> knobBounds;
    float lastModValue = 0.0f;
    float glow = 0.0f;
    float activity = 0.0f;
    double lastSliderValue = 0.0;
    bool dragHover = false;
    bool hover = false;
    float appear = 1.0f;
};

class ComboControl : public juce::Component,
                     public juce::SettableTooltipClient,
                     public IlanaAnim::PageAnimated,
                     private juce::Timer
{
public:
    ComboControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                  const juce::String& labelText)
    {
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (IlanaTheme::font (12.0f));
        label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
        addAndMakeVisible (label);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (parameterID)))
            combo.addItemList (choice->getAllValueStrings(), 1);

        addAndMakeVisible (combo);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, parameterID, combo);

        if (auto* parameter = state.getParameter (parameterID))
        {
            const auto description = describeParameter (parameterID);
            const auto tooltip = parameter->getName (64) + (description.isNotEmpty() ? "\n" + description : "");
            combo.setTooltip (tooltip);
            setTooltip (tooltip);
        }

        startTimerHz (30);
    }

    juce::ComboBox& getComboBox() { return combo; }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (13));
        combo.setBounds (area.removeFromTop (24));
    }

    void paint (juce::Graphics& g) override
    {
        if (hover > 0.01f)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.2f * hover));
            g.fillRoundedRectangle (combo.getBounds().toFloat().expanded (2.0f), 5.0f);
        }
    }

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

private:
    void timerCallback() override
    {
        hover = IlanaAnim::approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.22f);
        appear = juce::jmin (1.0f, appear + 0.12f);

        combo.setAlpha (appear);
        label.setAlpha (appear);

        const auto scale = 1.0f + 0.05f * hover;
        combo.setTransform (juce::AffineTransform::scale (scale, scale,
                                                          (float) combo.getX() + (float) combo.getWidth() * 0.5f,
                                                          (float) combo.getY() + (float) combo.getHeight() * 0.5f));

        if (hover > 0.01f || appear < 0.999f)
            repaint();
    }

    juce::ComboBox combo;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    float appear = 1.0f;
    float hover = 0.0f;
};

class ToggleControl : public juce::Component,
                      public juce::SettableTooltipClient,
                      public IlanaAnim::PageAnimated,
                      private juce::Timer
{
public:
    ToggleControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                   const juce::String& labelText)
    {
        button.setButtonText (labelText);
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonOnColourId, IlanaTheme::accent().withAlpha (0.85f));
        addAndMakeVisible (button);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, parameterID, button);

        if (auto* parameter = state.getParameter (parameterID))
        {
            const auto description = describeParameter (parameterID);
            const auto tooltip = parameter->getName (64) + (description.isNotEmpty() ? "\n" + description : "");
            button.setTooltip (tooltip);
            setTooltip (tooltip);
        }

        startTimerHz (30);
    }

    juce::TextButton& getButton() { return button; }

    void paint (juce::Graphics& g) override
    {
        const auto on = button.getToggleState();
        const auto pulse = 0.6f + 0.4f * std::sin (pulsePhase);
        const auto ledY = juce::jmin (button.getBottom() + 6.0f, (float) getHeight() - 8.0f);

        if (hover > 0.01f)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.18f * hover));
            g.fillRoundedRectangle (button.getBounds().toFloat().expanded (2.0f), 5.0f);
        }

        if (on)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.12f * pulse));
            g.fillEllipse ((float) button.getX() + 2.0f, ledY - 3.0f, 12.0f, 12.0f);
        }

        g.setColour (on ? IlanaTheme::accent().withAlpha (0.5f + 0.5f * pulse)
                        : juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse ((float) button.getX() + 5.0f, ledY, 6.0f, 6.0f);

        // Bezel and glass highlight on the LED.
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse ((float) button.getX() + 4.5f, ledY - 0.5f, 7.0f, 7.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.18f));
        g.fillEllipse ((float) button.getX() + 6.0f, ledY + 1.0f, 2.2f, 2.2f);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (13);
        button.setBounds (area.removeFromTop (juce::jmin (24, juce::jmax (16, area.getHeight()))));
    }

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

private:
    void timerCallback() override
    {
        pulsePhase += 0.16f;
        hover = IlanaAnim::approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.22f);
        appear = juce::jmin (1.0f, appear + 0.12f);

        const auto scale = 1.0f + 0.05f * hover;
        button.setTransform (juce::AffineTransform::scale (scale, scale,
                                                           (float) button.getX() + (float) button.getWidth() * 0.5f,
                                                           (float) button.getY() + (float) button.getHeight() * 0.5f));
        button.setAlpha (appear);

        if (button.getToggleState() || hover > 0.01f || appear < 0.999f || isMouseOver())
            repaint();
    }

    juce::TextButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    float pulsePhase = 0.0f;
    float appear = 1.0f;
    float hover = 0.0f;
};

class ValueSliderControl : public juce::Component
{
public:
    ValueSliderControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 16);
        slider.setColour (juce::Slider::rotarySliderFillColourId, IlanaTheme::accent());
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterID, slider);

        if (auto* parameter = state.getParameter (parameterID))
        {
            slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

            const auto description = describeParameter (parameterID);
            slider.setTooltip (parameter->getName (64) + (description.isNotEmpty() ? "\n" + description : ""));
        }
    }

    void resized() override { slider.setBounds (getLocalBounds()); }

    juce::Slider& getSlider() { return slider; }

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

inline void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items)
{
    if (items.empty())
        return;

    const auto width = area.getWidth() / (int) items.size();

    for (auto* item : items)
        item->setBounds (area.removeFromLeft (width).reduced (3));
}
