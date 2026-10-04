#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "ModNames.h"
#include "ParamControls.h"
#include "RemapEditor.h"

namespace MatrixMenus
{
// Fills a combo box with every destination, grouped into sub-menus so the
// list of ~400 targets stays navigable. Item IDs are index + 1, which is
// what ComboBoxAttachment expects.
inline void fillDestinations (juce::ComboBox& combo)
{
    using D = Mod::Destination;
    auto* root = combo.getRootMenu();
    root->clear();

    // Each written as the page labels it, module first ("Filter 1 › Cutoff").
    const auto item = [] (juce::PopupMenu& menu, int index)
    {
        menu.addItem (index + 1, ModNames::destination (index));
    };

    const auto fill = [&] (juce::PopupMenu& menu, std::initializer_list<D> list)
    {
        for (const auto destination : list)
            item (menu, (int) destination);
    };

    const auto group = [&] (const juce::String& title, std::initializer_list<D> list)
    {
        juce::PopupMenu menu;
        fill (menu, list);
        root->addSubMenu (title, menu);
    };

    item (*root, 0);
    root->addSeparator();

    // Oscillator and FM menus also collect the parameter destinations that
    // belong to them (appended after M4), so they're filled before adding.
    std::array<juce::PopupMenu, 6> oscillators;
    juce::PopupMenu fm;
    fill (oscillators[0], { D::Osc1Pitch, D::Osc1Frame, D::Osc1Level, D::Osc1Pan, D::Osc1Detune, D::Osc1Spread,
                            D::Osc1Blend, D::Osc1Warp, D::Osc1SampleStart, D::Osc1SampleEnd });
    fill (oscillators[1], { D::Osc2Pitch, D::Osc2Frame, D::Osc2Level, D::Osc2Pan, D::Osc2Detune, D::Osc2Spread,
                            D::Osc2Blend, D::Osc2Warp, D::Osc2SampleStart, D::Osc2SampleEnd });
    fill (oscillators[2], { D::SubPitch, D::SubFrame, D::SubLevel, D::SubPan, D::SubDetune, D::SubSpread,
                            D::SubBlend, D::SubWarp, D::SubSampleStart, D::SubSampleEnd, D::NoiseLevel });
    fill (oscillators[3], { D::Osc4Pitch, D::Osc4Frame, D::Osc4Level, D::Osc4Pan, D::Osc4Detune,
                            D::Osc4Spread, D::Osc4Blend, D::Osc4Warp, D::Osc4SampleStart, D::Osc4SampleEnd });
    fill (oscillators[4], { D::Osc5Pitch, D::Osc5Frame, D::Osc5Level, D::Osc5Pan, D::Osc5Detune,
                            D::Osc5Spread, D::Osc5Blend, D::Osc5Warp, D::Osc5SampleStart, D::Osc5SampleEnd });
    fill (oscillators[5], { D::Osc6Pitch, D::Osc6Frame, D::Osc6Level, D::Osc6Pan, D::Osc6Detune,
                            D::Osc6Spread, D::Osc6Blend, D::Osc6Warp, D::Osc6SampleStart, D::Osc6SampleEnd });
    fill (fm, { D::FmAmount, D::Fm1to2, D::Fm1to3, D::Fm2to3, D::Fm3to1, D::Fm3to2,
                D::FmFeedback, D::Fm2Feedback, D::Fm3Feedback });

    juce::PopupMenu effects, global, keys;

    for (const auto destination : { D::FxDriveAmount, D::FxCrushMix, D::FxCombFreq, D::FxPhaserRate, D::FxChorusDepth,
                                    D::FxDelayMix, D::FxDelayFeedback, D::FxSmearMix, D::FxFreezeMix, D::FxReverbMix,
                                    D::FxReverbSize })
        item (effects, (int) destination);

    const auto& params = Mod::getParamDestinations();
    const char* const prefixes[] { "osc1_", "osc2_", "sub_", "osc4_", "osc5_", "osc6_" };

    for (int i = 0; i < (int) params.size(); ++i)
    {
        const juce::String id (params[(size_t) i].id);
        const auto destination = Mod::paramDestinationFor (i);

        if (i < Mod::numLegacyParamDestinations)
        {
            item (id.startsWith ("fx_") ? effects : global, destination);
            continue;
        }

        if (id.startsWith ("fm_"))
        {
            item (fm, destination);
            continue;
        }

        // Operator and PD settings sit with their oscillator; the M4
        // physical and keys settings keep their own menu.
        auto placed = false;
        for (int osc = 0; osc < 6 && ! placed; ++osc)
            if (id.startsWith (prefixes[osc])
                && (id.endsWith ("_warp2_amt") || id.endsWith ("_pd_env_amt") || id.endsWith ("_key_level")))
            {
                item (oscillators[(size_t) osc], destination);
                placed = true;
            }

        if (! placed)
            item (keys, destination);
    }

    for (int osc = 0; osc < 6; ++osc)
        root->addSubMenu ("OSC " + juce::String (osc + 1), oscillators[(size_t) osc]);

    group ("Filters", { D::Filter1Cutoff, D::Filter1Reso, D::Filter1Drive, D::Filter1Env, D::Filter1Fm, D::Filter1Morph,
                        D::Filter2Cutoff, D::Filter2Reso, D::Filter2Drive, D::Filter2Env, D::Filter2Fm, D::Filter2Morph });
    group ("Voice", { D::AmpLevel, D::Pan, D::RingMod, D::Drift, D::ResAmount, D::ResDecay, D::ResOffset });
    root->addSubMenu ("FM", fm);
    group ("Envelopes", { D::AmpAttack, D::AmpDecay, D::AmpSustain, D::AmpRelease,
                          D::FeAttack, D::FeDecay, D::FeSustain, D::FeRelease,
                          D::MeAttack, D::MeDecay, D::MeSustain, D::MeRelease,
                          D::F2eAttack, D::F2eDecay, D::F2eSustain, D::F2eRelease,
                          D::E4Attack, D::E4Decay, D::E4Sustain, D::E4Release });
    group ("LFOs & MSEG", { D::Lfo1Rate, D::Lfo2Rate, D::Lfo3Rate, D::Lfo4Rate, D::Lfo5Rate, D::Lfo6Rate,
                            D::Lfo7Rate, D::Lfo8Rate, D::Lfo9Rate, D::Lfo10Rate, D::Lfo11Rate, D::Lfo12Rate,
                            D::Lfo13Rate, D::Lfo14Rate, D::Lfo15Rate, D::Lfo16Rate, D::MsegRate });

    root->addSubMenu ("Effects", effects);
    root->addSubMenu ("Global", global);
    root->addSubMenu ("Physical & Keys", keys);
}

// Fills a combo box with every source, grouped as the knobs' menus group
// them, by their one name. Item IDs are index + 1 (bind with IdComboAttachment).
inline void fillSources (juce::ComboBox& combo, const IlanaSynthAudioProcessor& processor, bool withNone)
{
    auto* root = combo.getRootMenu();
    root->clear();
    ModNames::fillSourceMenu (*root, &processor, nullptr, withNone);
}
} // namespace MatrixMenus

// Curve control: drag up or down to bend the response; click to draw the
// slot's remap curve (docked under the row by the matrix, else a pop-up).
// Draws the resulting transfer curve, the bend and the remap together.
class CurveControl : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    CurveControl (juce::RangedAudioParameter& parameterIn, IlanaSynthAudioProcessor& p, int slotIndexIn)
        : parameter (parameterIn),
          attachment (parameterIn, [this] (float) { repaint(); }, nullptr),
          processorRef (p),
          slotIndex (slotIndexIn)
    {
        setTooltip ("Curve\nDrag up or down to curve how the source maps to the amount.  "
                    "Click to draw a remap curve.  Double-click to straighten it.");
        attachment.sendInitialUpdate();
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    // The remap is not a parameter: the row polls it.
    void refresh()
    {
        const auto epoch = processorRef.getDataEpoch();
        if (epoch != lastEpoch)
        {
            lastEpoch = epoch;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        IlanaTheme::paintWell (g, bounds, 4.0f);

        const auto plot = bounds.reduced (5.0f, 4.0f);
        const auto curve = parameter.convertFrom0to1 (parameter.getValue());
        const auto exponent = std::exp2 (curve * 3.0f);
        const auto* remap = processorRef.readModSlot (slotIndex).remap;

        juce::Path path;

        for (int i = 0; i <= 48; ++i)
        {
            const auto x = (float) i / 48.0f;
            auto y = std::pow (x, exponent);
            if (remap != nullptr)
            {
                const auto position = y * (float) Mod::remapSize;
                const auto index = juce::jmin ((int) position, Mod::remapSize - 1);
                y = 0.5f * (remap[index] + (remap[index + 1] - remap[index]) * (position - (float) index) + 1.0f);
            }
            const juce::Point<float> point (plot.getX() + x * plot.getWidth(), plot.getBottom() - y * plot.getHeight());

            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }

        g.setColour (colour.withAlpha (isMouseOver() ? 1.0f : 0.8f));
        g.strokePath (path, juce::PathStrokeType (1.5f));

        // A drawn remap is marked in the corner.
        if (remap != nullptr)
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ bounds.getRight() - 5.0f, bounds.getY() + 5.0f }));
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragStart = parameter.convertFrom0to1 (parameter.getValue());
        dragging = false;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! dragging)
        {
            if (event.getDistanceFromDragStart() < 3)
                return;
            dragging = true;
            attachment.beginGesture();
        }

        const auto perPixel = event.mods.isShiftDown() ? 0.002f : 0.01f;   // shift = fine
        const auto value = juce::jlimit (-1.0f, 1.0f, dragStart - (float) event.getDistanceFromDragStartY() * perPixel);
        attachment.setValueAsPartOfGesture (value);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (dragging)
        {
            attachment.endGesture();
            dragging = false;
        }
        else if (event.mouseWasClicked() && event.getNumberOfClicks() == 1)
        {
            // Wait out a double-click before opening the editor.
            juce::Component::SafePointer<CurveControl> safeThis (this);
            juce::Timer::callAfterDelay (juce::MouseEvent::getDoubleClickTimeout() + 20, [safeThis]
            {
                if (safeThis != nullptr && ! safeThis->doubleClicked)
                    safeThis->openRemapEditor();
                if (safeThis != nullptr)
                    safeThis->doubleClicked = false;
            });
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        doubleClicked = true;
        attachment.setValueAsCompleteGesture (0.0f);
    }

    int getSlotIndex() const { return slotIndex; }

    // Set by the matrix to open the editor under the row instead of in a
    // call-out over the other rows.
    std::function<void (int slot)> onOpenRemap;

    void openRemapEditor()
    {
        if (onOpenRemap != nullptr)
        {
            onOpenRemap (slotIndex);
            return;
        }

        auto editor = std::make_unique<RemapEditor> (processorRef, slotIndex, colour);
        auto* parent = getTopLevelComponent();
        juce::CallOutBox::launchAsynchronously (std::move (editor),
                                                parent != nullptr ? parent->getLocalArea (this, getLocalBounds())
                                                                  : getScreenBounds(),
                                                parent);
    }

private:
    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    IlanaSynthAudioProcessor& processorRef;
    int slotIndex;
    juce::Colour colour = IlanaTheme::accent();
    float dragStart = 0.0f;
    bool dragging = false, doubleClicked = false;
    unsigned lastEpoch = 0;
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

// Polarity as two segments, UNI and BI (Serum 2's toggle). The parameter's
// Auto (choice 0) follows the source's own range: the segment it gives is
// outlined rather than filled. Click a segment to fix the polarity; click
// the fixed one again, or double-click, to go back to Auto.
class PolarityToggle : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    explicit PolarityToggle (juce::RangedAudioParameter& parameterIn)
        : attachment (parameterIn, [this] (float value) { choice = juce::roundToInt (value); updateTooltip(); repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    // What Auto gives the row's source (an LFO swings both ways).
    void setAutoBipolar (bool bipolar)
    {
        if (autoBipolar != bipolar)
        {
            autoBipolar = bipolar;
            updateTooltip();
            repaint();
        }
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    int getChoice() const { return choice; }
    bool isEffectivelyBipolar() const { return choice == 2 || (choice == 0 && autoBipolar); }

    // Clicking a segment (0 UNI, 1 BI); public for the tests.
    void clickSegment (int segment)
    {
        const auto wanted = segment == 0 ? 1 : 2;
        attachment.setValueAsCompleteGesture ((float) (choice == wanted ? 0 : wanted));
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f, 2.5f);
        IlanaTheme::paintWell (g, bounds, 5.0f);
        const auto lit = isEffectivelyBipolar() ? 1 : 0;

        for (int segment = 0; segment < 2; ++segment)
        {
            const auto area = segmentBounds (segment);

            if (segment == lit)
            {
                if (choice == 0)
                {
                    // Auto: outlined in the source's colour.
                    g.setColour (colour.withAlpha (0.85f));
                    g.drawRoundedRectangle (area.reduced (1.0f), 4.0f, 1.2f);
                }
                else
                {
                    g.setColour (colour.withAlpha (0.85f));
                    g.fillRoundedRectangle (area.reduced (1.0f), 4.0f);
                }
            }

            g.setColour (segment == lit ? (choice == 0 ? IlanaTheme::Ui::text : IlanaTheme::Ui::bg) : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (segment == 0 ? "UNI" : "BI", area, juce::Justification::centred);
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu() || event.mouseWasDraggedSinceMouseDown() || event.getNumberOfClicks() > 1)
            return;

        clickSegment (event.position.x < (float) getWidth() * 0.5f ? 0 : 1);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override { attachment.setValueAsCompleteGesture (0.0f); }

private:
    juce::Rectangle<float> segmentBounds (int segment) const
    {
        auto area = getLocalBounds().toFloat().reduced (2.0f, 4.0f);
        return segment == 0 ? area.removeFromLeft (area.getWidth() * 0.5f) : area.withTrimmedLeft (area.getWidth() * 0.5f);
    }

    void updateTooltip()
    {
        const auto range = isEffectivelyBipolar() ? juce::String ("Bipolar: swings either side of the knob's value")
                                                  : juce::String ("Unipolar: only pushes the knob one way");
        setTooltip ("Polarity\n" + range + (choice == 0 ? juce::String ("  (auto: the source's own range).") : juce::String (".")) +
                    "\nClick UNI or BI to fix it; click the fixed one again, or double-click, for auto.");
    }

    juce::ParameterAttachment attachment;
    int choice = 0;
    bool autoBipolar = false;
    juce::Colour colour = IlanaTheme::accent();
};

// One row of the modulation matrix, bound to one slot's parameters.
class MatrixRow : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    void setMacroNames (const juce::StringArray& names)
    {
        for (int m = 0; m < names.size(); ++m)
        {
            const auto base = "Macro " + juce::String (m + 1);
            const auto text = names[m] == base ? base : base + " (" + names[m] + ")";
            const auto itemId = (int) Mod::macroSourceFor (m) + 1;

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

    // An LFO or envelope outside its pool is greyed out in SOURCE and VIA
    // (kept in the lists, whose positions the attachments use), unless this
    // row already plays it. The "+" on the LFO and envelope pools adds one.
    void setSourcesInPatch (const std::vector<bool>& inPatch)
    {
        for (auto* box : { &source, &via })
        {
            const auto selected = box->getSelectedId();

            for (int item = 2; item <= (int) inPatch.size(); ++item)
                box->setItemEnabled (item, inPatch[(size_t) item - 1] || item == selected);
        }
    }

    bool isSourceItemEnabled (int itemId) const { return source.isItemEnabled (itemId); }

    MatrixRow (IlanaSynthAudioProcessor& p, int slotIndexIn)
        : processorRef (p),
          slotIndex (slotIndexIn),
          curve (*p.apvts.getParameter (p.getModSlotParamId (slotIndexIn, "curve")), p, slotIndexIn),
          polarity (*p.apvts.getParameter (p.getModSlotParamId (slotIndexIn, "pol")))
    {
        const auto id = [this] (const char* field) { return processorRef.getModSlotParamId (slotIndex, field); };

        // Sources by their one name, grouped as everywhere else.
        MatrixMenus::fillSources (source, p, true);
        MatrixMenus::fillSources (via, p, true);
        via.setTextWhenNothingSelected ("none");
        MatrixMenus::fillDestinations (destination);

        source.setTooltip ("Source\nWhat moves the destination.");
        via.setTooltip ("Via (aux)\nA second source that scales this routing: e.g. the mod wheel fading an LFO in.  "
                        "None leaves the amount as set.");

        // Until a via source is set, VIA reads "Aux: none" and opens the
        // same list.
        viaButton.setButtonText ("Aux: none");
        viaButton.setTooltip ("Via (aux)\nScale this routing by a second source (the mod wheel fading an LFO in, say).");
        viaButton.onClick = [this]
        {
            via.getRootMenu()->showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&viaButton),
                                              [safeThis = juce::Component::SafePointer<MatrixRow> (this)] (int result)
                                              {
                                                  if (safeThis != nullptr && result > 0)
                                                      safeThis->via.setSelectedId (result, juce::sendNotificationSync);
                                              });
        };
        destination.setTooltip ("Destination\nWhat gets modulated.");

        amount.setSliderStyle (juce::Slider::LinearHorizontal);
        amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
        amount.setTooltip ("Amount\nHow far the destination moves.  Negative values invert.  Double-click to zero.  "
                           "The bar under it shows what the routing adds right now.");
        amount.setDoubleClickReturnValue (true, 0.0);

        bypass.setClickingTogglesState (true);
        bypass.getProperties().set ("switch", true);
        bypass.setTooltip ("Switch this routing on or off without losing its settings.");

        remove.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
        remove.setTooltip ("Remove this routing");
        remove.onClick = [this] { processorRef.performEdit ("Remove modulation", [this] { processorRef.clearModSlot (slotIndex); }); };

        for (auto* component : std::initializer_list<juce::Component*> { &bypass, &source, &via, &viaButton, &amount, &curve,
                                                                          &polarity, &destination, &remove })
            addAndMakeVisible (component);

        auto& state = p.apvts;
        sourceAttachment = std::make_unique<IdComboAttachment> (*state.getParameter (id ("src")), source);
        viaAttachment = std::make_unique<IdComboAttachment> (*state.getParameter (id ("aux")), via);
        destinationAttachment = std::make_unique<IdComboAttachment> (*state.getParameter (id ("dst")), destination);
        amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id ("amt"), amount);
        bypassAttachment = std::make_unique<ReverseButtonAttachment> (*state.getParameter (id ("byp")), bypass);
    }

    int getSlotIndex() const { return slotIndex; }
    int getDisplayNumber() const { return displayNumber; }
    bool isDuplicate() const { return duplicateOf.isNotEmpty(); }
    CurveControl& getCurve() { return curve; }
    PolarityToggle& getPolarity() { return polarity; }
    juce::ComboBox& getSourceBox() { return source; }
    juce::ComboBox& getDestinationBox() { return destination; }
    // Why the routing does nothing now (its module is off), or empty.
    const juce::String& getIdleReason() const { return idleReason; }
    float getLiveValue() const { return liveValue; }

    // The "!" of a repeated routing: the page offers to merge it.
    std::function<void (int slot)> onDuplicateClicked;

    // The row whose remap the dock shows is outlined.
    void setSelected (bool shouldBeSelected)
    {
        if (selected != shouldBeSelected)
        {
            selected = shouldBeSelected;
            repaint();
        }
    }

    // The row's number in the list (1..n as shown; the slot stays in the
    // tooltip) and the other rows with the same source and destination.
    void setDisplayNumber (int number, const juce::String& duplicateRows)
    {
        if (number == displayNumber && duplicateRows == duplicateOf)
            return;

        displayNumber = number;
        duplicateOf = duplicateRows;
        updateTooltip();
        repaint();
    }

    // VIA takes its full width only while some row uses it.
    void setViaExpanded (bool shouldExpand)
    {
        if (viaExpanded != shouldExpand)
        {
            viaExpanded = shouldExpand;
            resized();
        }
    }

    void refresh()
    {
        const auto slot = processorRef.readModSlot (slotIndex);
        const auto hasVia = slot.aux != Mod::Source::None;

        via.setVisible (hasVia);
        viaButton.setVisible (! hasVia);
        polarity.setAutoBipolar (Mod::isBipolarSource (slot.source));

        const auto colour = slot.source != Mod::Source::None ? modSourceColour ((int) slot.source) : IlanaTheme::accent();

        if (colour != lastColour)
        {
            lastColour = colour;
            amount.setColour (juce::Slider::rotarySliderFillColourId, colour);
            curve.setColour (colour);
            polarity.setColour (colour);
        }

        curve.refresh();
        liveValue = slot.isActive() ? Mod::shape (slot, processorRef.getSourceDisplayValue ((int) slot.source)) * slot.depth : 0.0f;
        active = slot.isActive();

        // A routing into a module that is off can't be heard: the row dims
        // and says why (UI review 6, S6-15 / V5-20).
        const auto idle = slot.source != Mod::Source::None ? ModNames::whyDestinationIsIdle (processorRef, slot.destination) : juce::String();

        if (idle != idleReason)
        {
            idleReason = idle;
            destination.setTooltip ("Destination\nWhat gets modulated."
                                    + (idle.isNotEmpty() ? "\nNo effect now: " + idle + "." : juce::String()));
            updateTooltip();

            for (auto* component : std::initializer_list<juce::Component*> { &source, &via, &viaButton, &amount, &curve, &polarity, &destination })
                component->setAlpha (idle.isNotEmpty() ? IlanaTheme::dimmedAlpha : 1.0f);
        }

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.0f, 2.0f);
        const auto idle = idleReason.isNotEmpty();

        // Flat row; the source's colour marks its left edge.
        g.setColour (IlanaTheme::Ui::panel.interpolatedWith (lastColour, active && ! idle ? 0.04f : 0.0f));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (selected ? lastColour.withAlpha (0.8f) : IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, selected ? 1.5f : 1.0f);
        g.setColour (lastColour.withAlpha (active && ! idle ? 0.9f : 0.3f));
        g.fillRoundedRectangle (bounds.withWidth (3.0f).reduced (0.0f, 6.0f).translated (1.0f, 0.0f), 1.5f);

        // The row number; a second routing of the same source to the same
        // destination is marked in amber, and clicking it offers a merge.
        const auto numberArea = numberBounds();

        if (isDuplicate())
        {
            g.setColour (amber().withAlpha (numberHover ? 0.4f : 0.22f));
            g.fillRoundedRectangle (numberArea.toFloat().reduced (1.0f, 5.0f), 4.0f);
            g.setColour (amber());
        }
        else
        {
            g.setColour (active ? lastColour : IlanaTheme::Ui::text3);
        }

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        g.drawText (juce::String (displayNumber) + (isDuplicate() ? "!" : ""), numberArea, juce::Justification::centred);

        // Arrow between the polarity and the destination; amber with a
        // module that is off.
        const auto arrowX = (float) destination.getX() - 11.0f;
        juce::Path arrow;
        arrow.addTriangle (arrowX, (float) getHeight() * 0.5f - 4.0f, arrowX, (float) getHeight() * 0.5f + 4.0f,
                           arrowX + 6.0f, (float) getHeight() * 0.5f);
        g.setColour (idle ? amber().withAlpha (0.9f) : lastColour.withAlpha (active ? 0.8f : 0.3f));
        g.fillPath (arrow);
    }

    // The live bar: what the routing adds right now, drawn in the amount
    // slider's track from zero, as wide as the track and brighter than the
    // amount's own fill, ending in a white tick (Vital's matrix shows the
    // same; UI review 6, V6-22).
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! active || std::abs (liveValue) < 0.002f)
            return;

        const auto track = amountTrack();
        const auto zeroX = track.getCentreX();
        const auto liveX = zeroX + juce::jlimit (-1.0f, 1.0f, liveValue) * track.getWidth() * 0.5f;
        const auto bar = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zeroX, liveX), track.getY() - 1.0f,
                                                                    juce::jmax (zeroX, liveX), track.getBottom() + 1.0f);
        const auto idle = idleReason.isNotEmpty();
        g.setColour (IlanaTheme::Ui::bg.withAlpha (0.6f));
        g.fillRoundedRectangle (bar.expanded (0.0f, 1.0f), 3.0f);
        g.setColour (lastColour.interpolatedWith (juce::Colours::white, 0.3f).withAlpha (idle ? 0.35f : 0.95f));
        g.fillRoundedRectangle (bar, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (idle ? 0.4f : 0.95f));
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, bar.getHeight() + 6.0f).withCentre ({ liveX, bar.getCentreY() }), 1.0f);
    }

    void mouseMove (const juce::MouseEvent& event) override { setNumberHover (isDuplicate() && numberBounds().contains (event.getPosition())); }
    void mouseExit (const juce::MouseEvent&) override { setNumberHover (false); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (isDuplicate() && numberBounds().contains (event.getPosition()) && ! event.mouseWasDraggedSinceMouseDown()
            && onDuplicateClicked != nullptr)
            onDuplicateClicked (slotIndex);
    }

    // Column layout shared with the header labels. VIA is narrow ("Aux:
    // none") until some row uses it.
    struct Columns
    {
        static constexpr int number = 30, bypass = 34, source = 150, viaWide = 120, viaNarrow = 80,
                             amount = 190, curve = 50, polarity = 76, destination = 244, remove = 24, gap = 6;
        static int via (bool expanded) { return expanded ? viaWide : viaNarrow; }
    };

    static constexpr int rowHeight = 28;

    void resized() override
    {
        auto area = getLocalBounds().reduced (0, 3);
        area.removeFromLeft (Columns::number);
        bypass.setBounds (area.removeFromLeft (Columns::bypass).withSizeKeepingCentre (32, 18));
        area.removeFromLeft (Columns::gap * 2);
        source.setBounds (area.removeFromLeft (Columns::source));
        area.removeFromLeft (Columns::gap);
        const auto viaArea = area.removeFromLeft (Columns::via (viaExpanded));
        via.setBounds (viaArea);
        viaButton.setBounds (viaArea.withWidth (Columns::viaNarrow));
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

    static juce::Colour amber() { return juce::Colour (0xffffb020); }

private:
    juce::Rectangle<int> numberBounds() const { return { 4, 0, 24, getHeight() }; }

    // The amount slider's track, in row coordinates.
    juce::Rectangle<float> amountTrack()
    {
        const auto layout = amount.getLookAndFeel().getSliderLayout (amount);
        return layout.sliderBounds.toFloat().translated ((float) amount.getX(), (float) amount.getY())
            .withSizeKeepingCentre ((float) layout.sliderBounds.getWidth(), 6.0f);
    }

    void setNumberHover (bool hover)
    {
        if (hover != numberHover)
        {
            numberHover = hover;
            setMouseCursor (hover ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void updateTooltip()
    {
        setTooltip ("Slot " + juce::String (slotIndex + 1)
                    + (duplicateOf.isNotEmpty() ? "\nSame source and destination as row " + duplicateOf
                                                      + ": the two add up.  Click the ! to merge them into one row."
                                                : juce::String())
                    + (idleReason.isNotEmpty() ? "\nNo effect now: " + idleReason + "." : juce::String()));
    }

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
    juce::TextButton bypass, remove, viaButton;
    juce::ComboBox source, via, destination;
    juce::Slider amount;
    CurveControl curve;
    PolarityToggle polarity;
    std::unique_ptr<IdComboAttachment> sourceAttachment, viaAttachment, destinationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<ReverseButtonAttachment> bypassAttachment;
    juce::Colour lastColour = IlanaTheme::accent();
    juce::String idleReason;
    float liveValue = 0.0f;
    bool active = false;
    bool viaExpanded = false;
    bool selected = false;
    bool numberHover = false;
    int displayNumber = 0;
    juce::String duplicateOf;
};
