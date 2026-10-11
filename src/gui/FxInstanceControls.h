#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

#include "../PluginProcessor.h"

// An effect instance's own settings on its own card (roadmap step 17
// follow-up). The type's parameters hold the settings of the one card in
// front; every other instance keeps its own copy in the FX patch
// (FxPatch.h). So that such a card has real knobs, menus and switches like
// any other, its controls are the usual ParamControls classes attached to a
// SHADOW of the effect's parameters: a small parameter set of its own
// (same IDs, ranges, choices and value text) on a processor that plays
// nothing. A change to a shadow parameter goes into the instance's copy
// (setFxInstanceValue, one undo step per gesture); refresh() brings changes
// from elsewhere (undo, a preset, a swap) back onto the shadow.
namespace FxInstance
{
// A parameter that keeps its own value and answers everything else (range,
// steps, value text) from the real one.
class ShadowParameter : public juce::RangedAudioParameter
{
public:
    explicit ShadowParameter (const juce::RangedAudioParameter& realToUse)
        : juce::RangedAudioParameter (juce::ParameterID { realToUse.getParameterID(), 1 }, realToUse.getName (64),
                                      juce::AudioProcessorParameterWithIDAttributes().withLabel (realToUse.getLabel())),
          real (realToUse),
          value (realToUse.getDefaultValue())
    {
    }

    float getValue() const override { return value; }
    void setValue (float newValue) override { value = newValue; }
    float getDefaultValue() const override { return real.getDefaultValue(); }
    int getNumSteps() const override { return real.getNumSteps(); }
    bool isDiscrete() const override { return real.isDiscrete(); }
    bool isBoolean() const override { return real.isBoolean(); }
    juce::String getText (float normalised, int length) const override { return real.getText (normalised, length); }
    float getValueForText (const juce::String& text) const override { return real.getValueForText (text); }
    juce::StringArray getAllValueStrings() const override { return real.getAllValueStrings(); }
    const juce::NormalisableRange<float>& getNormalisableRange() const override { return real.getNormalisableRange(); }

private:
    const juce::RangedAudioParameter& real;
    float value;
};

// Holds the shadow parameters (an AudioProcessorValueTreeState needs a
// processor); never prepared or played.
class ShadowProcessor : public juce::AudioProcessor
{
public:
    const juce::String getName() const override { return "FX instance"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

// One instance's shadow: its parameters and the link to the processor.
class Shadow : private juce::AudioProcessorParameter::Listener
{
public:
    Shadow (IlanaSynthAudioProcessor& processorToUse, int slotToUse, int typeToUse, const std::vector<juce::String>& ids)
        : processorRef (processorToUse), slot (slotToUse), type (typeToUse)
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (const auto& id : ids)
            if (auto* real = processorRef.apvts.getParameter (id))
            {
                // A menu's choices come from an AudioParameterChoice (ComboControl).
                if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (real))
                    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, choice->getName (64), choice->choices,
                                                                             juce::roundToInt (choice->convertFrom0to1 (static_cast<juce::AudioProcessorParameter*> (choice)->getDefaultValue()))));
                else
                    layout.add (std::make_unique<ShadowParameter> (*real));
            }
        state = std::make_unique<juce::AudioProcessorValueTreeState> (holder, nullptr, "FxInstance", std::move (layout));

        for (const auto& id : ids)
            if (auto* parameter = state->getParameter (id))
            {
                parameters.push_back ({ id, parameter });
                parameter->addListener (this);
            }
        refresh();
    }

    ~Shadow() override
    {
        for (auto& entry : parameters)
            entry.parameter->removeListener (this);
        if (gestures > 0)
            processorRef.endEdit();
    }

    juce::AudioProcessorValueTreeState& getState() { return *state; }
    int getSlot() const { return slot; }
    int getType() const { return type; }

    // The instance's copy onto the shadow (changes made elsewhere).
    void refresh() { refresh (processorRef.getFxPatch()); }

    void refresh (const FxPatchData::Patch& patch)
    {
        const auto& values = patch.slots[(size_t) slot].values;
        const juce::ScopedValueSetter<bool> quiet (syncing, true);
        for (auto& entry : parameters)
            for (const auto& [id, value] : values)
                if (id == entry.id)
                {
                    const auto normalised = entry.parameter->convertTo0to1 (value);
                    if (std::abs (entry.parameter->getValue() - normalised) > 1.0e-6f)
                        entry.parameter->setValueNotifyingHost (normalised);
                    break;
                }
    }

private:
    struct Entry
    {
        juce::String id;
        juce::RangedAudioParameter* parameter = nullptr;
    };

    void parameterValueChanged (int index, float) override
    {
        if (syncing)
            return;
        for (auto& entry : parameters)
            if (entry.parameter->getParameterIndex() == index)
            {
                const auto value = entry.parameter->convertFrom0to1 (entry.parameter->getValue());
                // A click on a menu or a switch is one step without a gesture.
                if (gestures == 0)
                    processorRef.performEdit ("Change " + entry.parameter->getName (64), [this, &entry, value]
                                              { processorRef.setFxInstanceValue (slot, entry.id, value); });
                else
                    processorRef.setFxInstanceValue (slot, entry.id, value);
                return;
            }
    }

    void parameterGestureChanged (int index, bool starting) override
    {
        for (auto& entry : parameters)
            if (entry.parameter->getParameterIndex() == index)
            {
                if (starting)
                {
                    if (gestures++ == 0)
                        processorRef.beginEdit ("Change " + entry.parameter->getName (64));
                }
                else if (gestures > 0 && --gestures == 0)
                    processorRef.endEdit();
                return;
            }
    }

    IlanaSynthAudioProcessor& processorRef;
    int slot, type;
    ShadowProcessor holder;
    std::unique_ptr<juce::AudioProcessorValueTreeState> state;
    std::vector<Entry> parameters;
    bool syncing = false;
    int gestures = 0;
};
} // namespace FxInstance
