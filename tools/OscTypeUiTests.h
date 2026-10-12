// The FM / DX7 oscillator type (2026-10-06, ilana: "operator mode on the
// wavetable should be a separate DX7 or FM mode"): a Wavetable card has no
// operator controls, an FM / DX7 card has them, the type menu and the add
// rows offer it, and the FM page offers the switch. Included by Snapshot.cpp
// after its helpers; runOscTypeTests runs from runUiTests. (No popup menu is
// opened: a native menu window crashes the xvfb harness. The type menu is
// driven by its arrow keys, which go through the same path.)
#pragma once

void runOscTypeTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto loadNamed = [&processor] (const juce::String& name)
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (name)));
        settle (500);
    };
    const auto setParam = [&processor] (const juce::String& id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };
    const auto read = [&processor] (const juce::String& id)
    {
        const auto* value = processor.apvts.getRawParameterValue (id);
        return value != nullptr ? juce::roundToInt (value->load()) : -1;
    };
    const auto inCard = [&editor] (juce::Component* c, int osc)
    {
        return c != nullptr && visibleInTree (c) && c->getWidth() > 0
               && editor.getOscCardBounds (osc).contains (editor.getLocalArea (c, c->getLocalBounds()).getCentre());
    };
    // A control of the card bound to a parameter (its tooltip starts with the parameter's name).
    const auto comboIn = [&] (const juce::String& id, int osc) -> ComboControl*
    {
        const auto name = processor.apvts.getParameter (id)->getName (64);
        std::vector<ComboControl*> combos;
        findAll<ComboControl> (editor, combos);
        for (auto* combo : combos)
            if (inCard (combo, osc) && combo->getComboBox().getTooltip().startsWith (name + "\n"))
                return combo;
        return nullptr;
    };
    const auto knobIn = [&] (const juce::String& id, int osc) -> KnobControl*
    {
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (editor, knobs);
        for (auto* knob : knobs)
            if (knob->getParameterId() == id && inCard (knob, osc))
                return knob;
        return nullptr;
    };
    const auto envGraphIn = [&] (int osc)
    {
        std::vector<OperatorEnvDisplay*> graphs;
        findAll<OperatorEnvDisplay> (editor, graphs);
        for (auto* graph : graphs)
            if (inCard (graph, osc))
                return true;
        return false;
    };
    const auto press = [] (ComboControl* combo, const juce::KeyPress& key)
    {
        if (combo != nullptr)
            combo->getComboBox().keyPressed (key);
        settle (400);
    };
    const auto addButton = [&editor] (const juce::String& text) -> DashedAddButton*
    {
        std::vector<DashedAddButton*> buttons;
        findAll<DashedAddButton> (editor, buttons);
        for (auto* button : buttons)
            if (visibleInTree (button) && button->getWidth() > 0 && button->getButtonText() == text)
                return button;
        return nullptr;
    };

    // A Wavetable: TUNING offers only Semitones, ENVELOPE no OP ENV, and the
    // card has none of the operator's controls.
    loadNamed ("Neuro Wobble");
    editor.showPage ("OSC");
    settle (500);
    {
        auto* mode = comboIn ("osc1_mode", 0);
        auto* tune = comboIn ("osc1_tune", 0);
        auto* envelope = comboIn ("osc1_amp_env", 0);
        juce::ComboBox fallback;
        auto& modeBox = mode != nullptr ? mode->getComboBox() : fallback;
        expect (mode != nullptr && modeBox.getText() == "Wavetable" && modeBox.getNumItems() == 6 && modeBox.getItemText (5) == "FM / DX7"
                    && modeBox.getItemId (5) == 6,
                "OSC: the type menu names FM / DX7, appended (index 5)");
        expect (tune != nullptr && tune->getComboBox().isItemEnabled (1) && ! tune->getComboBox().isItemEnabled (2)
                    && ! tune->getComboBox().isItemEnabled (3),
                "OSC: a Wavetable's TUNING offers Semitones only (Ratio and Fixed Hz are FM / DX7's)");
        expect (envelope != nullptr && envelope->getComboBox().indexOfItemId (OperatorEg::envelopeChoice + 1) < 0
                    && envelope->getComboBox().indexOfItemId (17) >= 0,
                "OSC: a Wavetable's ENVELOPE has no OP ENV");
        expect (knobIn ("osc1_ratio", 0) == nullptr && knobIn ("osc1_fixed_hz", 0) == nullptr && knobIn ("osc1_eg_out", 0) == nullptr
                    && comboIn ("osc1_fb_type", 0) == nullptr && ! envGraphIn (0) && knobIn ("osc1_frame", 0) != nullptr,
                "OSC: a Wavetable card has no RATIO, FIXED, OUTPUT, FB TYPE or OP ENV graph");

        // The arrow key steps the menu's order: Wavetable, FM / DX7, ...
        press (mode, juce::KeyPress (juce::KeyPress::downKey));
        expect (read ("osc1_mode") == 5, "OSC: the type menu's next type after Wavetable is FM / DX7 (" + juce::String (read ("osc1_mode")) + ")");
        tune = comboIn ("osc1_tune", 0);
        envelope = comboIn ("osc1_amp_env", 0);
        expect (tune != nullptr && tune->getComboBox().isItemEnabled (2) && tune->getComboBox().isItemEnabled (3)
                    && envelope != nullptr && envelope->getComboBox().indexOfItemId (OperatorEg::envelopeChoice + 1) >= 0
                    && comboIn ("osc1_fb_type", 0) != nullptr && knobIn ("fm_feedback", 0) != nullptr && knobIn ("osc1_frame", 0) == nullptr,
                "OSC: an FM / DX7 card has the operator's TUNING, OP ENV, FEEDBACK and FB TYPE, and no FRAME");
        processor.getUndoManager().undo();
        settle (400);
        expect (read ("osc1_mode") == 0, "OSC: undo takes the type back to Wavetable");

        // An operator by ratio on the OP ENV: its card draws the envelope and
        // its RATIO and OUTPUT; back to Wavetable it plays in semitones on the Amp Env.
        press (mode, juce::KeyPress (juce::KeyPress::downKey));
        setParam ("osc1_tune", (float) OscTuning::Ratio);
        setParam ("osc1_amp_env", (float) OperatorEg::envelopeChoice);
        settle (500);
        expect (knobIn ("osc1_ratio", 0) != nullptr && knobIn ("osc1_eg_out", 0) != nullptr && envGraphIn (0),
                "OSC: an FM / DX7 card on the OP ENV shows RATIO, OUTPUT and the envelope");
        press (comboIn ("osc1_mode", 0), juce::KeyPress (juce::KeyPress::upKey));
        expect (read ("osc1_mode") == 0 && read ("osc1_tune") == OscTuning::Semitones && read ("osc1_amp_env") == 0,
                "OSC: FM / DX7 back to Wavetable drops the ratio and the OP ENV");
        processor.getUndoManager().undo();
        settle (400);
        expect (read ("osc1_mode") == 5 && read ("osc1_tune") == OscTuning::Ratio && read ("osc1_amp_env") == OperatorEg::envelopeChoice,
                "OSC: undo brings the operator back in one step");
    }

    // The add row: a Wavetable, or at its right end an FM / DX7 operator.
    loadNamed ("Neuro Wobble");
    editor.showPage ("OSC");
    settle (500);
    if (auto* add = addButton ("+  ADD FM / DX7"); add != nullptr)
    {
        auto next = -1;
        for (int osc = 0; osc < OscillatorIds::count && next < 0; ++osc)
            if (! processor.isOscillatorShown (osc))
                next = osc;
        add->triggerClick();
        settle (600);
        const juce::String prefix (OscillatorIds::prefixes[(size_t) juce::jmax (0, next)]);
        expect (next >= 0 && processor.isOscillatorShown (next) && read (prefix + "_mode") == 5 && read (prefix + "_tune") == OscTuning::Ratio
                    && read (prefix + "_amp_env") == OperatorEg::envelopeChoice && envGraphIn (next),
                "OSC: + ADD FM / DX7 adds a sine operator by ratio on its OP ENV, drawn as one");
        processor.getUndoManager().undo();
        settle (400);
        // (Undo puts the parameters back; the card stays revealed, as for + ADD OSC.)
        expect (next >= 0 && read (prefix + "_on") == 0 && read (prefix + "_mode") != 5, "OSC: undo takes the added operator back");
    }
    else
        expect (false, "OSC: the add row offers + ADD FM / DX7");

    editor.showPage ("MAIN");
    settle (400);
    expect (addButton ("+  ADD FM / DX7") != nullptr, "PLAY: the add row offers + ADD FM / DX7");

    // FM: another type's card offers the switch; an operator's doesn't.
    editor.showOperatorEnvelope (0); // (the FM page on OSC 1)
    settle (500);
    {
        const auto makeShown = [&editor]
        {
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (editor, buttons);
            for (auto* button : buttons)
                if (visibleInTree (button) && button->getWidth() > 0 && button->getButtonText() == "SWITCH TO FM / DX7")
                    return button;
            return (juce::TextButton*) nullptr;
        };
        auto* make = makeShown();
        expect (make != nullptr, "FM: a Wavetable's card offers SWITCH TO FM / DX7");
        if (make != nullptr)
        {
            const auto before = read ("osc1_mode");
            make->triggerClick();
            settle (500);
            expect (before == 0 && read ("osc1_mode") == 5 && makeShown() == nullptr,
                    "FM: SWITCH TO FM / DX7 makes OSC 1 an operator (and goes)");
        }
    }

    // A DX7 voice: every operator is FM / DX7, from the factory and from an
    // old patch.
    loadNamed ("E.PIANO 1 (ROM1A)");
    {
        auto all = true;
        for (const auto* prefix : OscillatorIds::prefixes)
            all = all && read (juce::String (prefix) + "_mode") == 5;
        expect (all, "a DX7 voice's six operators are FM / DX7");
    }

    loadNamed ("Neuro Wobble");
    editor.showPage ("MAIN");
    settle (300);
}
