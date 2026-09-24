#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"

namespace MatrixMenus
{
// Fills a combo box with every destination, grouped into sub-menus so the
// list of ~180 targets stays navigable. Item IDs are index + 1, which is
// what ComboBoxAttachment expects.
inline void fillDestinations (juce::ComboBox& combo)
{
    using D = Mod::Destination;
    const auto names = Mod::getDestinationNames();
    auto* root = combo.getRootMenu();
    root->clear();

    const auto item = [&names] (juce::PopupMenu& menu, int index)
    {
        menu.addItem (index + 1, names[index]);
    };

    const auto group = [&] (const juce::String& title, std::initializer_list<D> list)
    {
        juce::PopupMenu menu;

        for (const auto destination : list)
            item (menu, (int) destination);

        root->addSubMenu (title, menu);
    };

    item (*root, 0);
    root->addSeparator();

    group ("Oscillator 1", { D::Osc1Pitch, D::Osc1Frame, D::Osc1Level, D::Osc1Pan, D::Osc1Detune, D::Osc1Spread,
                             D::Osc1Blend, D::Osc1Warp, D::Osc1SampleStart, D::Osc1SampleEnd });
    group ("Oscillator 2", { D::Osc2Pitch, D::Osc2Frame, D::Osc2Level, D::Osc2Pan, D::Osc2Detune, D::Osc2Spread,
                             D::Osc2Blend, D::Osc2Warp, D::Osc2SampleStart, D::Osc2SampleEnd });
    group ("Oscillator 3", { D::SubPitch, D::SubFrame, D::SubLevel, D::SubPan, D::SubDetune, D::SubSpread,
                             D::SubBlend, D::SubWarp, D::SubSampleStart, D::SubSampleEnd, D::NoiseLevel });
    group ("Filters", { D::Filter1Cutoff, D::Filter1Reso, D::Filter1Drive, D::Filter1Env, D::Filter1Fm, D::Filter1Morph,
                        D::Filter2Cutoff, D::Filter2Reso, D::Filter2Drive, D::Filter2Env, D::Filter2Fm, D::Filter2Morph });
    group ("Voice", { D::AmpLevel, D::Pan, D::FmAmount, D::FmFeedback, D::RingMod, D::Drift,
                      D::ResAmount, D::ResDecay, D::ResOffset });
    group ("Envelopes", { D::AmpAttack, D::AmpDecay, D::AmpSustain, D::AmpRelease,
                          D::FeAttack, D::FeDecay, D::FeSustain, D::FeRelease,
                          D::MeAttack, D::MeDecay, D::MeSustain, D::MeRelease,
                          D::F2eAttack, D::F2eDecay, D::F2eSustain, D::F2eRelease,
                          D::E4Attack, D::E4Decay, D::E4Sustain, D::E4Release });
    group ("LFOs & MSEG", { D::Lfo1Rate, D::Lfo2Rate, D::Lfo3Rate, D::Lfo4Rate, D::MsegRate });

    juce::PopupMenu effects, global;

    for (const auto destination : { D::FxDriveAmount, D::FxCrushMix, D::FxCombFreq, D::FxPhaserRate, D::FxChorusDepth,
                                    D::FxDelayMix, D::FxDelayFeedback, D::FxSmearMix, D::FxFreezeMix, D::FxReverbMix,
                                    D::FxReverbSize })
        item (effects, (int) destination);

    const auto& params = Mod::getParamDestinations();

    for (int i = 0; i < (int) params.size(); ++i)
    {
        const juce::String id (params[(size_t) i].id);
        item (id.startsWith ("fx_") ? effects : global, Mod::numExplicitDestinations + i);
    }

    root->addSubMenu ("Effects", effects);
    root->addSubMenu ("Global", global);
}
} // namespace MatrixMenus

// Curve control: drag up or down to bend the response, double-click to
// straighten it. Draws the resulting transfer curve.
class CurveControl : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    CurveControl (juce::RangedAudioParameter& parameterIn)
        : parameter (parameterIn),
          attachment (parameterIn, [this] (float) { repaint(); }, nullptr)
    {
        setTooltip ("Curve\nDrag up or down to bend how the source maps to the amount.  Double-click to reset.");
        attachment.sendInitialUpdate();
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        IlanaTheme::paintWell (g, bounds, 4.0f);

        const auto plot = bounds.reduced (5.0f, 4.0f);
        const auto curve = parameter.convertFrom0to1 (parameter.getValue());
        const auto exponent = std::exp2 (curve * 3.0f);

        juce::Path path;

        for (int i = 0; i <= 24; ++i)
        {
            const auto x = (float) i / 24.0f;
            const auto y = std::pow (x, exponent);
            const juce::Point<float> point (plot.getX() + x * plot.getWidth(), plot.getBottom() - y * plot.getHeight());

            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }

        g.setColour (colour.withAlpha (isMouseOver() ? 1.0f : 0.8f));
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragStart = parameter.convertFrom0to1 (parameter.getValue());
        attachment.beginGesture();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto value = juce::jlimit (-1.0f, 1.0f, dragStart - (float) event.getDistanceFromDragStartY() * 0.01f);
        attachment.setValueAsPartOfGesture (value);
    }

    void mouseUp (const juce::MouseEvent&) override { attachment.endGesture(); }

    void mouseDoubleClick (const juce::MouseEvent&) override { attachment.setValueAsCompleteGesture (0.0f); }

private:
    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    juce::Colour colour = IlanaTheme::accent();
    float dragStart = 0.0f;
};

// Binds a choice parameter to a combo box by item ID (ID = choice + 1)
// rather than by position, so the items can be grouped into sub-menus in
// any order. JUCE's ComboBoxAttachment matches by position.
class IdComboAttachment
{
public:
    IdComboAttachment (juce::RangedAudioParameter& parameterIn, juce::ComboBox& comboIn)
        : parameter (parameterIn),
          combo (comboIn),
          attachment (parameterIn, [this] (float value)
                      {
                          combo.setSelectedId (juce::roundToInt (value) + 1, juce::dontSendNotification);
                      },
                      nullptr)
    {
        combo.onChange = [this]
        {
            if (combo.getSelectedId() > 0)
                attachment.setValueAsCompleteGesture ((float) (combo.getSelectedId() - 1));
        };

        attachment.sendInitialUpdate();
    }

private:
    juce::RangedAudioParameter& parameter;
    juce::ComboBox& combo;
    juce::ParameterAttachment attachment;
};

// One row of the modulation matrix, bound to one slot's parameters.
class MatrixRow : public juce::Component
{
public:
    void setMacroNames (const juce::StringArray& names)
    {
        for (int m = 0; m < names.size(); ++m)
        {
            const auto base = "Macro " + juce::String (m + 1);
            const auto text = names[m] == base ? base : base + " (" + names[m] + ")";
            const auto itemId = (int) Mod::Source::Macro1 + m + 1;

            for (auto* box : { &source, &via })
            {
                // getSelectedId() matches on the item text too, so read it
                // before renaming.
                const auto wasSelected = box->getSelectedId() == itemId;
                box->changeItemText (itemId, text);

                if (wasSelected)
                    box->setSelectedId (itemId, juce::dontSendNotification);
            }
        }
    }

    MatrixRow (IlanaSynthAudioProcessor& p, int slotIndexIn)
        : processorRef (p),
          slotIndex (slotIndexIn),
          curve (*p.apvts.getParameter (p.getModSlotParamId (slotIndexIn, "curve")))
    {
        const auto id = [this] (const char* field) { return processorRef.getModSlotParamId (slotIndex, field); };

        source.addItemList (Mod::getSourceNames(), 1);
        via.addItemList (Mod::getSourceNames(), 1);
        via.setTextWhenNothingSelected ("-");
        polarity.addItemList ({ "Natural", "Unipolar", "Bipolar" }, 1);
        MatrixMenus::fillDestinations (destination);

        source.setTooltip ("Source\nWhat moves the destination.");
        via.setTooltip ("Via\nA second source that scales this routing: e.g. the mod wheel fading an LFO in.  "
                        "None leaves the amount as set.");
        polarity.setTooltip ("Polarity\nNatural uses the source's own range.  Unipolar only pushes one way.  "
                             "Bipolar swings either side of the knob.");
        destination.setTooltip ("Destination\nWhat gets modulated.");

        amount.setSliderStyle (juce::Slider::LinearHorizontal);
        amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
        amount.setTooltip ("Amount\nHow far the destination moves.  Negative values invert.  Double-click to zero.");
        amount.setDoubleClickReturnValue (true, 0.0);

        bypass.setClickingTogglesState (true);
        bypass.setTooltip ("Switch this routing on or off without losing its settings.");

        remove.setButtonText ("x");
        remove.setTooltip ("Remove this routing");
        remove.onClick = [this] { processorRef.clearModSlot (slotIndex); };

        for (auto* component : std::initializer_list<juce::Component*> { &bypass, &source, &via, &amount, &curve,
                                                                          &polarity, &destination, &remove })
            addAndMakeVisible (component);

        auto& state = p.apvts;
        sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id ("src"), source);
        viaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id ("aux"), via);
        polarityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id ("pol"), polarity);
        destinationAttachment = std::make_unique<IdComboAttachment> (*state.getParameter (id ("dst")), destination);
        amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id ("amt"), amount);
        bypassAttachment = std::make_unique<ReverseButtonAttachment> (*state.getParameter (id ("byp")), bypass);
    }

    int getSlotIndex() const { return slotIndex; }

    void refresh()
    {
        const auto slot = processorRef.readModSlot (slotIndex);
        const auto colour = slot.source != Mod::Source::None ? modSourceColour ((int) slot.source) : IlanaTheme::accent();

        if (colour != lastColour)
        {
            lastColour = colour;
            amount.setColour (juce::Slider::rotarySliderFillColourId, colour);
            curve.setColour (colour);
        }

        meterValue = slot.source != Mod::Source::None ? processorRef.getSourceDisplayValue ((int) slot.source) : 0.0f;
        active = slot.isActive();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.0f, 2.0f);

        g.setColour (active ? lastColour.withAlpha (0.08f) : juce::Colours::white.withAlpha (0.025f));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (active ? lastColour.withAlpha (0.35f) : juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

        g.setColour (active ? lastColour : juce::Colours::white.withAlpha (0.4f));
        g.setFont (IlanaTheme::font (11.5f, true));
        g.drawText (juce::String (slotIndex + 1), juce::Rectangle<int> (4, 0, 22, getHeight()), juce::Justification::centred);

        // Live source meter, bipolar around the middle.
        const auto meter = juce::Rectangle<float> (meterX, 7.0f, 6.0f, (float) getHeight() - 14.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (meter, 2.0f);

        const auto half = meter.getHeight() * 0.5f;
        const auto bar = juce::jlimit (0.0f, half, std::abs (meterValue) * half);
        g.setColour (lastColour.withAlpha (active ? 0.9f : 0.35f));
        g.fillRect (meterValue >= 0.0f ? juce::Rectangle<float> (meter.getX(), meter.getCentreY() - bar, meter.getWidth(), bar)
                                       : juce::Rectangle<float> (meter.getX(), meter.getCentreY(), meter.getWidth(), bar));

        // Arrow between the amount and the destination.
        const auto arrowX = (float) destination.getX() - 11.0f;
        juce::Path arrow;
        arrow.addTriangle (arrowX, (float) getHeight() * 0.5f - 4.0f, arrowX, (float) getHeight() * 0.5f + 4.0f,
                           arrowX + 6.0f, (float) getHeight() * 0.5f);
        g.setColour (lastColour.withAlpha (active ? 0.8f : 0.3f));
        g.fillPath (arrow);
    }

    // Column layout shared with the header labels.
    struct Columns
    {
        static constexpr int number = 28, bypass = 24, meter = 12, source = 142, via = 116, amount = 214,
                             curve = 54, polarity = 96, destination = 220, remove = 26, gap = 6;
    };

    void resized() override
    {
        auto area = getLocalBounds().reduced (0, 5);
        area.removeFromLeft (Columns::number);
        bypass.setBounds (area.removeFromLeft (Columns::bypass).withSizeKeepingCentre (18, 18));
        area.removeFromLeft (Columns::gap);
        meterX = (float) area.getX() + 2.0f;
        area.removeFromLeft (Columns::meter + Columns::gap);
        source.setBounds (area.removeFromLeft (Columns::source));
        area.removeFromLeft (Columns::gap);
        via.setBounds (area.removeFromLeft (Columns::via));
        area.removeFromLeft (Columns::gap * 2);
        amount.setBounds (area.removeFromLeft (Columns::amount));
        area.removeFromLeft (Columns::gap);
        curve.setBounds (area.removeFromLeft (Columns::curve));
        area.removeFromLeft (Columns::gap);
        polarity.setBounds (area.removeFromLeft (Columns::polarity));
        area.removeFromLeft (Columns::gap * 3);
        destination.setBounds (area.removeFromLeft (Columns::destination));
        area.removeFromLeft (Columns::gap);
        remove.setBounds (area.removeFromLeft (Columns::remove));
    }

private:
    // The bypass parameter is "off = active", but the button reads as a
    // power switch: lit means the routing is on.
    struct ReverseButtonAttachment
    {
        ReverseButtonAttachment (juce::RangedAudioParameter& parameterIn, juce::Button& buttonIn)
            : button (buttonIn),
              attachment (parameterIn, [this] (float value) { button.setToggleState (value < 0.5f, juce::dontSendNotification); },
                          nullptr)
        {
            button.onClick = [this] { attachment.setValueAsCompleteGesture (button.getToggleState() ? 0.0f : 1.0f); };
            attachment.sendInitialUpdate();
        }

        juce::Button& button;
        juce::ParameterAttachment attachment;
    };

    IlanaSynthAudioProcessor& processorRef;
    int slotIndex;
    juce::TextButton bypass, remove;
    juce::ComboBox source, via, polarity, destination;
    juce::Slider amount;
    CurveControl curve;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment, viaAttachment,
        polarityAttachment;
    std::unique_ptr<IdComboAttachment> destinationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<ReverseButtonAttachment> bypassAttachment;
    juce::Colour lastColour = IlanaTheme::accent();
    float meterValue = 0.0f;
    float meterX = 0.0f;
    bool active = false;
};
