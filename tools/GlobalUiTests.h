// UI review 8, package R6 (global consistency, header, browser, SEQ): checks
// for its fixes. Included by Snapshot.cpp after its helpers (findAll,
// findChild, settle, expect, visibleInTree); runGlobalReview8Tests runs from
// runUiTests.
#pragma once

#include <set>

// The source tree, for the checks that read it. __FILE__ is relative to the
// build folder when ccache's base_dir rewrites it ("../tools/..."), so it is
// also tried from the executable's build folder (build/X_artefacts/Release).
inline juce::File findSourceTree()
{
    const juce::String here (__FILE__);
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    for (const auto& base : { juce::File::getCurrentWorkingDirectory(), exe.getParentDirectory().getParentDirectory().getParentDirectory() })
    {
        const auto file = juce::File::isAbsolutePath (here) ? juce::File (here) : base.getChildFile (here);
        if (const auto src = file.getParentDirectory().getSiblingFile ("src"); src.getChildFile ("gui").isDirectory())
            return src;
    }
    return {};
}

void runGlobalReview8Tests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    const auto loadNamed = [&processor] (const juce::String& name)
    {
        processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf (name)));
        settle (300);
    };
    const auto setParam = [&processor] (const juce::String& id, float plain)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
    };
    const auto boundsInEditor = [&editor] (juce::Component* component)
    {
        return component != nullptr && component->getParentComponent() != nullptr
                   ? editor.getLocalArea (component->getParentComponent(), component->getBounds())
                   : juce::Rectangle<int>();
    };

    // V8-12, I8-25: text is never squeezed sideways. The one fitting rule
    // (IlanaTheme::drawFitted) shrinks a line to the floor, then cuts it; the
    // probe records both. Every page, on a wavetable patch, the piano and a
    // DX7 voice, at 100 % and at 75 %: nothing is cut.
    {
        // The rule itself: a line that fits draws as it is; one a little
        // long shrinks (no cut); one far too long is cut, never condensed.
        juce::Image image (juce::Image::ARGB, 400, 40, true);
        juce::Graphics g (image);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        const auto width = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "STRING COUPLING");
        auto& probe = IlanaTheme::textFitProbe();
        probe = {};
        probe.armed = true;
        IlanaTheme::drawFitted (g, "STRING COUPLING", juce::Rectangle<int> (0, 0, (int) std::ceil (width) + 1, 20), juce::Justification::centred);
        const auto fitted = probe.shrunk.isEmpty() && probe.cut.isEmpty();
        IlanaTheme::drawFitted (g, "STRING COUPLING", juce::Rectangle<int> (0, 0, (int) (width * 0.9f), 20), juce::Justification::centred);
        const auto shrunk = probe.shrunk.contains ("STRING COUPLING") && probe.cut.isEmpty();
        IlanaTheme::drawFitted (g, "STRING COUPLING", juce::Rectangle<int> (0, 0, (int) (width * 0.5f), 20), juce::Justification::centred);
        const auto cut = probe.cut.contains ("STRING COUPLING");
        // Given two lines and the height for them, a long line wraps at its
        // own size rather than shrinking or tightening onto one (the FM
        // matrix's note).
        probe = {};
        probe.armed = true;
        IlanaTheme::drawFitted (g, "STRING COUPLING STRING COUPLING", juce::Rectangle<int> (0, 0, (int) (width * 1.2f), 40),
                                juce::Justification::topLeft, 2);
        const auto wraps = probe.shrunk.isEmpty() && probe.cut.isEmpty();
        probe = {};
        expect (fitted && shrunk && cut && wraps && juce::approximatelyEqual (g.getCurrentFont().getHorizontalScale(), 1.0f),
                "drawFitted draws a fitting line as it is, shrinks a slightly long one, cuts a far too long one and wraps one given two lines, never condensed");

        // No other path to JUCE's condensing fitted text in the sources.
        const auto sources = findSourceTree();
        juce::StringArray offenders;
        if (! sources.isDirectory())
            offenders.add ("(the source tree wasn't found)");
        else
            for (const auto& file : sources.findChildFiles (juce::File::findFiles, true, "*.h;*.cpp"))
            {
                if (file.getFileName() == "IlanaLookAndFeel.h" || file.getFullPathName().contains ("thirdparty"))
                    continue;
                if (file.loadFileAsString().contains ("drawFittedText"))
                    offenders.add (file.getFileName());
            }
        expect (offenders.isEmpty(), "every fitted text goes through IlanaTheme::drawFitted (no drawFittedText elsewhere"
                                         + (offenders.isEmpty() ? juce::String (")") : ": " + offenders.joinIntoString (", ") + ")"));

        // No "OFF" spelled out away from a switch (I8-20): the FM matrix's
        // headings and the SIGNAL FLOW's stubs are dimmed instead.
        const auto fmPage = sources.getChildFile ("gui/pages/FmInputPages.h").loadFileAsString();
        const auto flow = sources.getChildFile ("gui/FilterWidgets.h").loadFileAsString();
        expect (fmPage.isNotEmpty() && ! fmPage.contains ("name + \": OFF\"") && flow.isNotEmpty() && ! flow.contains ("drawText (\"OFF\""),
                "the FM matrix's headings and the SIGNAL FLOW's stubs don't spell OFF out (dimmed instead)");

        auto* top = editor.getTopLevelComponent();
        const auto before = top->getBounds();
        juce::StringArray cutText;
        std::set<juce::String> seen;
        auto painted = 0, shrunkCount = 0;

        for (const auto* preset : { "Neuro Wobble", "Felt Hammer Board", "E.PIANO 1 (ROM1A)" })
        {
            loadNamed (preset);

            for (const auto small : { false, true })
            {
                if (small)
                    top->setSize (795, 540);
                else
                    top->setBounds (before);
                settle (300);

                for (const auto& page : editor.getPageIds())
                {
                    editor.showPage (page);
                    settle (200);
                    probe = {};
                    probe.armed = true;
                    editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
                    probe.armed = false;
                    ++painted;
                    shrunkCount += probe.shrunk.size();
                    for (const auto& text : probe.cutDetails)
                        if (seen.insert ((small ? "75 " : "") + page + text.upToLastOccurrenceOf (": needs", false, false)).second)
                            cutText.add (juce::String (preset) + (small ? " 75% " : " ") + page + ": '" + text + "'");
                }
            }
        }

        probe = {};
        top->setBounds (before);
        editor.showPage ("MAIN");
        settle (300);
        std::cout << "  (text fitting: " << painted << " pages painted, " << shrunkCount << " lines shrunk to fit)" << std::endl;
        expect (cutText.isEmpty() && painted >= 30, "no text is squeezed or cut on any page at 100 % or 75 % ("
                                                        + juce::String (painted) + " pages)"
                                                        + (cutText.isEmpty() ? juce::String() : ": " + cutText.joinIntoString (", ")));
    }

    // I8-32: a card without a family colour has no grey tag.
    expect (! IlanaTheme::hasFamilyColour (IlanaTheme::Ui::text2) && ! IlanaTheme::hasFamilyColour (juce::Colour (0xffe0e6f0))
                && IlanaTheme::hasFamilyColour (IlanaTheme::accent()),
            "card titles tag only a family colour (no grey dot on OUTPUT or SIGNAL FLOW)");

    // V8-36: a tab's own pages are picked right after the tabs, not beside
    // SCOPE at the far right.
    {
        loadNamed ("Neuro Wobble");
        editor.showPage ("MAIN");
        settle (300);
        std::vector<SectionSwitcher*> switchers;
        findAll<SectionSwitcher> (editor, switchers);
        SectionSwitcher* shown = nullptr;
        for (auto* candidate : switchers)
            if (visibleInTree (candidate) && ! candidate->getBounds().isEmpty())
                shown = candidate;
        juce::TextButton* scope = nullptr;
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (editor, buttons);
        for (auto* button : buttons)
            if (button->getButtonText() == "SCOPE")
                scope = button;
        const auto switchBounds = boundsInEditor (shown);
        const auto scopeBounds = boundsInEditor (scope);
        expect (shown != nullptr && scope != nullptr && switchBounds.getRight() + 60 < scopeBounds.getX() && switchBounds.getX() < editor.getWidth() / 2,
                "PLAY's OVERVIEW / VECTOR switch follows the tabs, apart from SCOPE (" + switchBounds.toString() + ", SCOPE "
                    + scopeBounds.toString() + ")");
    }

    // S8-37, V8-30: the OUT meter keeps clear of the resize grip's corner.
    {
        auto* meter = findChild<OutputMeter> (editor);
        const auto meterBounds = boundsInEditor (meter);
        expect (meter != nullptr && meterBounds.getRight() <= editor.getWidth() - 26,
                "the OUT meter keeps clear of the window's corner (" + meterBounds.toString() + " in " + juce::String (editor.getWidth()) + ")");
    }

    // I8-21, I8-20: OSC's shared tabs light their dot from a switch only
    // (VOICE and ACOUSTIC KEYS have none); an off oscillator's tab says
    // nothing in words (its dot is out).
    {
        editor.showPage ("OSC");
        settle (300);
        std::vector<StateTabs*> rows;
        findAll<StateTabs> (editor, rows);
        auto voiceDot = true, keysDot = true, subDotFollowsSwitch = false, offWord = false;
        setParam ("subosc_on", 0.0f);
        setParam ("noise_level", 0.5f);
        setParam ("osc2_on", 0.0f);
        settle (400);
        for (auto* tabs : rows)
            for (int i = 0; i < tabs->getNumItems(); ++i)
            {
                const auto& item = tabs->getItem (i);
                if (item.name == "VOICE")
                    voiceDot = item.dot;
                if (item.name == "SOUNDBOARD")
                    keysDot = item.dot;
                if (item.name == "SUB + NOISE")
                    subDotFollowsSwitch = item.dot && ! item.lit;
                if (item.name == "OSC 2")
                    offWord = item.state.containsIgnoreCase ("off");
            }
        expect (! voiceDot && ! keysDot && subDotFollowsSwitch && ! offWord,
                "OSC's tabs: no dot on VOICE or SOUNDBOARD, SUB + NOISE's dot follows its switch, no OFF in an off oscillator's tab");
        setParam ("osc2_on", 1.0f);
        loadNamed ("Neuro Wobble");
    }

    // S8-23: dialog buttons are upper case, as the rest of the UI's.
    {
        ConfirmOverlay overlay;
        ConfirmOverlay::Choices choices;
        choices.confirmText = "Load anyway";
        choices.alternativeText = "Save and load";
        choices.onAlternative = [] {};
        overlay.ask ("Test", "Message", choices, [] (bool, bool) {});
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (overlay, buttons);
        juce::StringArray texts;
        for (auto* button : buttons)
            texts.add (button->getButtonText());
        std::vector<juce::ToggleButton*> ticks;
        findAll<juce::ToggleButton> (overlay, ticks);
        expect (texts.contains ("LOAD ANYWAY") && texts.contains ("SAVE AND LOAD") && texts.contains ("CANCEL")
                    && ! ticks.empty() && ticks.front()->getButtonText() == "Don't show this again",
                "the confirm's buttons are upper case and its tick reads as the tour's (" + texts.joinIntoString (", ") + ")");
        overlay.finish (false);
    }

    // SEQ (I8-23, S8-33, V8-22): every engine's row starts RATE, STEPS,
    // GATE, menus and knobs on one label line; the arp's step lane is
    // GATE ×; the lanes read their edited values; a greyed FIT says why.
    {
        editor.showPage ("ARP/SEQ");
        settle (400);
        const auto shownControl = [&editor, &processor] (const juce::String& id) -> juce::Component*
        {
            auto* parameter = processor.apvts.getParameter (id);
            std::vector<juce::Component*> all;
            findAll<juce::Component> (editor, all);
            for (auto* component : all)
            {
                if (! visibleInTree (component) || component->getWidth() <= 0)
                    continue;
                if (auto* knob = dynamic_cast<KnobControl*> (component); knob != nullptr && knob->getParameterId() == id)
                    return knob;
                if (auto* combo = dynamic_cast<ComboControl*> (component); combo != nullptr && parameter != nullptr
                    && combo->getTooltip().startsWith (parameter->getName (64)))
                    return combo;
            }
            return nullptr;
        };
        auto* tabs = findChild<CardTabs> (editor);
        juce::StringArray rows;
        auto rowsOk = tabs != nullptr;
        const std::vector<std::pair<int, std::vector<juce::String>>> engines {
            { 0, { "arp_div", "arp_steps", "arp_gate", "arp_mode", "arp_octaves", "arp_chance" } },
            { 1, { "euc_div", "euc_steps", "euc_gate", "euc_target", "euc_hits", "euc_rotate" } },
            { 2, { "pseq_div", "pseq_length", "pseq_gate" } } };
        for (const auto& [engine, ids] : engines)
        {
            if (tabs == nullptr)
                break;
            tabs->setSelected (engine, true);
            settle (250);
            auto previousX = -1;
            auto labelTop = -1;
            for (const auto& id : ids)
            {
                const auto bounds = boundsInEditor (shownControl (id));
                const auto inOrder = ! bounds.isEmpty() && bounds.getX() > previousX;
                const auto level = labelTop < 0 || std::abs (bounds.getY() - labelTop) <= 1;
                if (labelTop < 0)
                    labelTop = bounds.getY();
                previousX = bounds.getX();
                if (! inOrder || ! level)
                {
                    rowsOk = false;
                    rows.add (id + " " + bounds.toString());
                }
            }
        }
        if (tabs != nullptr)
            tabs->setSelected (0, true);
        settle (200);
        expect (rowsOk, "ARP, EUCLID and PROB SEQ rows start RATE, STEPS, GATE, with menus and knobs on one label line"
                            + (rows.isEmpty() ? juce::String() : " (" + rows.joinIntoString (", ") + ")"));

        expect (ArpLanesEditor::textFor (ArpLanesEditor::gate, 1.5f) == juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + "1.50"
                    && ArpLanesEditor::textFor (ArpLanesEditor::gate, 0.0f) == "rest",
                "the arp's GATE lane reads as a multiple of the GATE knob (" + ArpLanesEditor::textFor (ArpLanesEditor::gate, 1.5f) + ")");

        ClipEditor roll (processor, juce::Colours::orange);
        roll.setSize (400, 200);
        ClipZoomControl zoom (roll, "ZOOM");
        zoom.setSize (160, 37);
        std::vector<juce::TextButton*> zoomButtons;
        findAll<juce::TextButton> (zoom, zoomButtons);
        juce::String fitTip;
        for (auto* button : zoomButtons)
            if (button->getButtonText() == "FIT")
                fitTip = button->getTooltip();
        expect (roll.isDrawMode() && fitTip.contains ("already"),
                "a new clip roll opens in DRAW, and a greyed FIT says the whole clip is shown already");
    }

    // S8-27, V8-25, S8-28: the DX7 chip counts the same with or without a
    // bank picked, and the banks keep to the DX7 chip's line.
    {
        editor.showPage ("MAIN");
        settle (200);
        auto* panel = findChild<PresetPanel> (editor);
        if (auto* display = findChild<PresetDisplay> (editor); display != nullptr && display->onClick != nullptr && (panel == nullptr || ! panel->isOpen()))
        {
            display->onClick();
            settle (400);
            panel = findChild<PresetPanel> (editor);
        }
        expect (panel != nullptr && panel->isOpen(), "the browser opens for the DX7 chip checks");

        if (panel != nullptr && panel->isOpen())
        {
            panel->selectFilter ("");
            settle (100);
            if (! panel->getChipKeys().contains ("bank:ROM1A"))
                panel->clickChip ("pack:dx7");
            settle (100);
            const auto plain = panel->getChipLabel ("pack:dx7");
            panel->clickChip ("bank:ROM1A");
            settle (100);
            const auto banked = panel->getChipLabel ("pack:dx7");
            // The banks are one BANK chip (review 9, S9-8), on the DX7
            // chip's line, not nine chips over up to four rows.
            const auto dx7Line = panel->getChipBounds ("pack:dx7").getY();
            const auto bankChip = panel->getChipBounds ("*bank");
            expect (plain.isNotEmpty() && plain == banked && ! bankChip.isEmpty() && bankChip.getY() == dx7Line
                        && panel->getChipBounds ("bank:DEXED01").isEmpty(),
                    "the DX7 chip counts the same with a bank picked ('" + plain + "', '" + banked + "'), and the banks are one BANK chip on its line ("
                        + panel->getChipBounds ("pack:dx7").toString() + ", " + bankChip.toString() + " in "
                        + juce::String (panel->getChipRowWidth()) + " px)");
            panel->clickChip ("bank:ROM1A");
            panel->clickChip ("pack:dx7");
            settle (100);
        }

        if (auto* display = findChild<PresetDisplay> (editor); panel != nullptr && panel->isOpen() && display != nullptr && display->onClick != nullptr)
        {
            display->onClick();
            settle (300);
        }
    }
}
