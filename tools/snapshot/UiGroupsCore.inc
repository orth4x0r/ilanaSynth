// ---------------------------------------------------------------------------
// UI tests: a registry of named groups.
//   ilanaSnapshot --uitest                 every group in order, on one shared editor
//   ilanaSnapshot --uitest --list          the group ids
//   ilanaSnapshot --uitest --only A,B      just those groups, each on a fresh editor (--chain: one editor for all)
// ILANA_UITEST_ONLY=A,B does the same (the old single ids T1, T2, U1, R6, PLAY, PG, HR,
// WF, OS, OR, FC, FL, FF, DD, R31C, STRIPS still work; OSC is the six groups that
// make up the OSC page checks).
// ---------------------------------------------------------------------------
struct UiCtx
{
    IlanaSynthAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    IlanaSynthAudioProcessorEditor* pages = nullptr;
    juce::StringArray names;
    int neuroWobble = -1;
    bool askedBefore = true;

    UiCtx()
    {
        processor.prepareToPlay (48000.0, 512);
        names = processor.getFactoryPresetNames();
        neuroWobble = names.indexOf ("Neuro Wobble");
        processor.loadFactoryPreset (neuroWobble);
        openEditor();
    }

    void openEditor()
    {
        editor.reset (processor.createEditor());
        editor->setSize (1060, 720);
        settle (400);

        if (auto* tutorial = findChild<TutorialOverlay> (*editor))
            tutorial->setVisible (false);

        pages = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
        // Loads don't ask over edited patches except in the test that checks it
        // (the user's own choice is put back at the end).
        askedBefore = pages->asksBeforeReplacingEdits();
        pages->setAsksBeforeReplacingEdits (false);
    }

    void closeEditor()
    {
        if (pages != nullptr)
            pages->setAsksBeforeReplacingEdits (askedBefore);

        pages = nullptr;
        editor.reset();
    }
};

// Undo checks start from an empty history, with every parameter change
// already in the tree (it otherwise catches up on a timer).
void uiClearHistory (UiCtx& ctx)
{
    ctx.processor.apvts.copyState();
    ctx.processor.getUndoManager().clearUndoHistory();
}

juce::StringArray uiUndoSteps (UiCtx& ctx)
{
    ctx.processor.apvts.copyState();
    return ctx.processor.getUndoManager().getUndoDescriptions();
}

// An oscillator card's switch on the page shown (its tooltip names the
// parameter).
ToggleControl* uiToggleFor (UiCtx& ctx, const juce::String& id)
{
    auto* parameter = ctx.processor.apvts.getParameter (id);
    std::vector<ToggleControl*> toggles;
    findAll<ToggleControl> (*ctx.editor, toggles);
    for (auto* toggle : toggles)
        if (parameter != nullptr && visibleInTree (toggle) && toggle->getWidth() > 0
            && toggle->getButton().getTooltip().startsWith (parameter->getName (64)))
            return toggle;
    return nullptr;
}

// A click on that card's title line: a card folded to fit opens (UI review 4, V13).
bool uiClickCardTitle (UiCtx& ctx, const juce::String& onId, bool rightClick)
{
    auto* toggle = uiToggleFor (ctx, onId);
    auto* card = toggle != nullptr ? toggle->getParentComponent() : nullptr;
    if (card == nullptr)
        return false;
    const juce::Point<float> position (40.0f, (float) toggle->getBounds().getCentreY());
    const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(), position,
                                  rightClick ? juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier | juce::ModifierKeys::popupMenuClickModifier)
                                             : juce::ModifierKeys(),
                                  1.0f, 0.0f, 0.0f, 0.0f, 0.0f, card, card, juce::Time::getCurrentTime(),
                                  position, juce::Time::getCurrentTime(), 1, false);
    card->mouseUp (event);
    settle (200);
    return true;
}

// OSC's tab for an oscillator, clicked (UI review 6: one oscillator at a time on OSC).
bool uiSelectOscTab (UiCtx& ctx, int osc)
{
    std::vector<StateTabs*> rows;
    findAll<StateTabs> (*ctx.editor, rows);
    for (auto* tabs : rows)
        for (int i = 0; i < tabs->getNumItems(); ++i)
            if (tabs->getItem (i).name == "OSC " + juce::String (osc + 1) && tabs->onSelect != nullptr
                && (dynamic_cast<OscPicker*> (tabs) == nullptr || tabs->getName() == "OSC tabs"))
            {
                tabs->setSelected (i);
                tabs->onSelect (i);
                settle (200);
                return true;
            }
    return false;
}

// Plain-value parameter access.
void uiSetParam (UiCtx& ctx, const juce::String& id, float plain)
{
    if (auto* parameter = ctx.processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
}

float uiReadParam (UiCtx& ctx, const juce::String& id)
{
    const auto* value = ctx.processor.apvts.getRawParameterValue (id);
    return value != nullptr ? value->load() : 0.0f;
}

juce::Component* uiOpenSourcePopover (UiCtx& ctx, Mod::Source source) { return openSourcePopover (*ctx.pages, source); }
void uiCloseSourcePopover (UiCtx& ctx) { closeSourcePopover (*ctx.pages); }

// PLAY's BALANCE slider (it shows while the filters run in parallel): found by its tooltip.
bool uiBalanceSliderShown (juce::Component& root)
{
    for (auto* child : root.getChildren())
    {
        if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (child); tip != nullptr && visibleInTree (child) && child->getWidth() > 0
                                                                           && tip->getTooltip().startsWith ("BALANCE\n"))
            return true;
        if (uiBalanceSliderShown (*child))
            return true;
    }
    return false;
}

// What every group body starts from: the names the old single function had in scope.
#define UI_GROUP_PRELUDE \
    [[maybe_unused]] auto& processor = ctx.processor; \
    [[maybe_unused]] auto& editor = ctx.editor; \
    [[maybe_unused]] auto* pages = ctx.pages; \
    [[maybe_unused]] const auto& names = ctx.names; \
    [[maybe_unused]] const auto neuroWobble = ctx.neuroWobble; \
    [[maybe_unused]] const auto clearHistory = [&ctx] { uiClearHistory (ctx); }; \
    [[maybe_unused]] const auto undoSteps = [&ctx] { return uiUndoSteps (ctx); }; \
    [[maybe_unused]] const auto toggleFor = [&ctx] (const juce::String& id) { return uiToggleFor (ctx, id); }; \
    [[maybe_unused]] const auto clickCardTitle = [&ctx] (const juce::String& id, bool rightClick = false) { return uiClickCardTitle (ctx, id, rightClick); }; \
    [[maybe_unused]] const auto selectOscTab = [&ctx] (int osc) { return uiSelectOscTab (ctx, osc); }; \
    [[maybe_unused]] const auto setParam = [&ctx] (const juce::String& id, float plain) { uiSetParam (ctx, id, plain); }; \
    [[maybe_unused]] const auto readParam = [&ctx] (const juce::String& id) { return uiReadParam (ctx, id); };


void uiGroup_BASICS (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    expect (pages != nullptr && pages->getPageIds()[0] == "MAIN", "MAIN is the first page");

    // The animation clock's cheap "is it on screen" (IlanaAnim::showing)
    // answers as Component::isShowing does, without asking the window system
    // on every frame: a hidden branch or an editor with no window is not shown.
    {
        juce::Component parent, child;
        parent.addAndMakeVisible (child);
        parent.setVisible (false);
        expect (! IlanaAnim::showing (child) && ! child.isShowing(), "a child of a hidden component is not showing");
        parent.setVisible (true);
        expect (IlanaAnim::showing (child) == child.isShowing(), "showing() agrees with isShowing() for a component with no window");
    }
}

void uiGroup_TABS (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    expect (findChild<juce::TabbedComponent> (*editor) != nullptr
                && findChild<juce::TabbedComponent> (*editor)->getNumTabs() == (IlanaSynthAudioProcessor::isEffectBuild ? 7 : 6),
            "six tabs (PLAY, MATRIX, FM, SEQ, FX, PATCH; the INPUT tab on the effect build)");


    // Pages inside a tab: MATRIX is its own tab, and the scope opens over any page.
    pages->showPage ("MATRIX");
    settle (200);
    expect (pages->getCurrentPageId() == "MATRIX" && pages->getCurrentPage() != nullptr && visibleInTree (pages->getCurrentPage()),
            "MATRIX opens in its tab");
    // The OSC, FILTER and ENV / LFO pages are gone (PLAY and the source pop-ups hold them): their old ids,
    // and STEPS's, open PLAY.
    auto oldIdsOpenPlay = true;
    for (const auto* oldId : { "STEPS", "OSC", "FILTER", "ENV/LFO" })
    {
        pages->showPage ("MATRIX");
        pages->showPage (oldId);
        settle (200);
        oldIdsOpenPlay = oldIdsOpenPlay && pages->getCurrentPageId() == "MAIN" && ! pages->getPageIds().contains (oldId);
    }
    expect (oldIdsOpenPlay, "no page has the old STEPS, OSC, FILTER or ENV/LFO id; each opens PLAY");
    pages->showPage ("MATRIX");
    settle (200);
    {
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*editor, buttons);
        juce::TextButton* scopeButton = nullptr;
        for (auto* b : buttons)
            if (b->getButtonText() == "SCOPE" && b->getTooltip().startsWith ("Scope"))
                scopeButton = scopeButton == nullptr ? b : scopeButton;
        std::vector<SectionSwitcher*> switchers;
        findAll<SectionSwitcher> (*editor, switchers);
        auto shownSwitchers = 0;
        for (auto* sw : switchers)
            shownSwitchers += visibleInTree (sw) && sw->getWidth() > 40 ? 1 : 0;
        expect (scopeButton != nullptr && visibleInTree (scopeButton) && scopeButton->getWidth() > 20,
                "the SCOPE button is in the tab row (" + (scopeButton != nullptr ? scopeButton->getBounds().toString() : juce::String ("none")) + ")");
        juce::String where;
        for (auto* sw : switchers)
            if (sw->isVisible())
                where << sw->getBounds().toString() << " z" << sw->getParentComponent()->getIndexOfChildComponent (sw)
                      << "/" << sw->getParentComponent()->getNumChildComponents() << " alpha " << sw->getAlpha();
        expect (shownSwitchers == 0, "MATRIX has one page, so no page switch shows (" + where + ")");
    }
    pages->setScopeOpen (true);
    settle (250);
    expect (pages->isScopeOpen(), "the scope panel opens");
    pages->setScopeOpen (false);
    expect (! pages->isScopeOpen(), "the scope panel closes");
}

void uiGroup_KEYS (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // KEYS opens the keyboard and gives the page less room; COMPARE flips
    // to B and lights; the page switch changes the page.
    {
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*editor, buttons);
        juce::TextButton* keys = nullptr;
        juce::TextButton* compare = nullptr;
        for (auto* b : buttons)
        {
            keys = b->getButtonText() == "KEYBOARD" ? b : keys;
            compare = dynamic_cast<ABButton*> (b) != nullptr ? b : compare;
        }

        auto* tabsComponent = findChild<juce::TabbedComponent> (*editor);
        auto* keyboard = findChild<KeyboardStrip> (*editor);
        expect (keys != nullptr && keyboard != nullptr && ! visibleInTree (keyboard), "the keyboard starts hidden");

        if (keys != nullptr && keyboard != nullptr && tabsComponent != nullptr)
        {
            // The keyboard only adds height below the synth: the window grows
            // by the strip and every existing panel keeps its size and place.
            const auto windowBefore = editor->getBounds();
            const auto pageBounds = tabsComponent->getBounds();
            std::vector<juce::Rectangle<int>> panelsBefore;
            std::vector<juce::Component*> panels;
            for (auto* b : buttons)
            {
                if (! b->isShowing())
                    continue; // (the hidden dialogs centre in the window, so they would move)
                panels.push_back (b);
                panelsBefore.push_back (b->getScreenBounds() - editor->getScreenPosition());
            }

            keys->triggerClick();
            settle (150);
            expect (visibleInTree (keyboard), "KEYS shows the keyboard");
            expect (editor->getWidth() == windowBefore.getWidth() && editor->getHeight() > windowBefore.getHeight(),
                    "KEYS grows the window (" + juce::String (windowBefore.getHeight()) + " to " + juce::String (editor->getHeight()) + " px)");
            expect (tabsComponent->getBounds() == pageBounds, "KEYS leaves the page area where it was and the same size");
            auto panelsStay = true;
            for (size_t i = 0; i < panels.size(); ++i)
                panelsStay = panelsStay && (panels[i]->getScreenBounds() - editor->getScreenPosition()) == panelsBefore[i];
            expect (panelsStay, "KEYS leaves the header and tab buttons where they were");
            expect (keyboard->getScreenBounds().getY() >= tabsComponent->getScreenBounds().getBottom()
                        && keyboard->getScreenBounds().getBottom() <= editor->getScreenBounds().getBottom(),
                    "the keyboard sits below the pages inside the taller window");

            keys->triggerClick();
            settle (150);
            expect (! visibleInTree (keyboard) && editor->getBounds() == windowBefore && tabsComponent->getBounds() == pageBounds,
                    "KEYS hides it again and the window returns to its size");
        }

        // Flair 2026-11-01: the keyboard lights for every note the synth
        // plays (the host's too), as hard as it was hit, and lets go with it.
        {
            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            const auto epoch = processor.getUiNoteEpoch();
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
            processor.processBlock (buffer, midi);
            expect (processor.getUiNoteVelocity (64) == 100 && processor.getUiNoteEpoch() != epoch,
                    "a host note shows on the keyboard with its velocity (" + juce::String (processor.getUiNoteVelocity (64)) + ")");
            midi.clear();
            midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            expect (processor.getUiNoteVelocity (64) == 0, "the key goes dark when the host lets the note go");
        }

        expect (compare != nullptr && compare->getWidth() <= 64, "the header's compare is a compact A|B");

        if (compare != nullptr)
        {
            compare->triggerClick();
            settle (150);
            expect (compare->getButtonText() == "B" && compare->getToggleState(), "A|B flips to B and lights");
            compare->triggerClick();
            settle (150);
            expect (compare->getButtonText() == "A" && ! compare->getToggleState(), "A|B flips back to A");

            // UI review 31, item 24: copy one side to the other. B gets A's
            // cutoff while A plays; then B is changed, and "Copy B to A"
            // from A brings B's sound into the side playing.
            if (auto* cutoff = processor.apvts.getParameter ("f1_cutoff"))
            {
                const auto original = cutoff->getValue();
                pages->copyAB (true); // A to B
                cutoff->setValueNotifyingHost (0.2f);
                compare->triggerClick(); // to B: the copy of A, not the 0.2
                settle (150);
                const auto bHas = cutoff->getValue();
                cutoff->setValueNotifyingHost (0.7f);
                compare->triggerClick(); // back to A (0.2)
                settle (150);
                pages->copyAB (false); // B (0.7) to A, which plays
                settle (150);
                const auto aHas = cutoff->getValue();
                expect (std::abs (bHas - original) < 0.001f && std::abs (aHas - 0.7f) < 0.001f && pages->isShowingA(),
                        "A|B's copy: A to B keeps B as A was, B to A brings B's sound into A (" + juce::String (bHas, 3) + ", "
                            + juce::String (aHas, 3) + ")");
                cutoff->setValueNotifyingHost (original);
            }
        }

        // UI review 31, item 23: Ctrl+F the browser, Ctrl+B A/B, Ctrl+L the
        // favourite, Ctrl+N a new patch (asking over an edited one), Ctrl+R a
        // random patch; none while a text field has the keys.
        {
            const auto ctrl = [] (int key) { return juce::KeyPress (key, juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::commandModifier, 0); };
            const auto wasA = pages->isShowingA();
            const auto b = pages->keyPressed (ctrl ('B'));
            settle (150);
            const auto flipped = pages->isShowingA() != wasA;
            pages->keyPressed (ctrl ('B'));
            settle (150);
            auto* display = findChild<PresetDisplay> (*editor);
            const auto favBefore = display != nullptr && display->getTooltip().isNotEmpty();
            juce::ignoreUnused (favBefore);
            const auto l = pages->keyPressed (ctrl ('L'));
            const auto l2 = pages->keyPressed (ctrl ('L')); // (and back)
            auto* panel = findChild<PresetPanel> (*editor);
            const auto f = pages->keyPressed (ctrl ('F'));
            settle (200);
            panel = findChild<PresetPanel> (*editor);
            const auto opened = panel != nullptr && panel->isOpen();
            pages->keyPressed (ctrl ('F'));
            settle (200);
            expect (b && flipped && l && l2 && f && opened, "Ctrl+B flips A/B, Ctrl+L favourites, Ctrl+F opens the browser");

            if (auto* cutoff = processor.apvts.getParameter ("f1_cutoff"))
            {
                const auto presetName = processor.getCurrentPresetName();
                const auto asked = pages->asksBeforeReplacingEdits();
                pages->setAsksBeforeReplacingEdits (true); // (the settings may say "don't ask again")
                const auto original = cutoff->getValue();
                cutoff->setValueNotifyingHost (original > 0.5f ? 0.1f : 0.9f); // an edited patch
                settle (400);
                auto* confirm = findChild<ConfirmOverlay> (*editor);
                const auto n = pages->keyPressed (ctrl ('N'));
                settle (150);
                const auto askedN = confirm != nullptr && confirm->isAsking();
                if (askedN)
                    confirm->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
                settle (150);
                const auto r = pages->keyPressed (ctrl ('R'));
                settle (150);
                const auto askedR = confirm != nullptr && confirm->isAsking();
                if (askedR)
                    confirm->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
                settle (150);
                expect (n && askedN && r && askedR, "Ctrl+N and Ctrl+R ask first over an edited patch (edited "
                                                        + juce::String (pages->isPatchEdited() ? "yes" : "no") + ")");
                pages->setAsksBeforeReplacingEdits (asked);
                // (Whatever happened, the tests after this go on with the same preset.)
                if (processor.getCurrentPresetName() != presetName)
                    processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (presetName));
                else
                    cutoff->setValueNotifyingHost (original);
                settle (300);
            }

            // A text field keeps Ctrl+F: the editor leaves it.
            {
                juce::TextEditor field;
                pages->addAndMakeVisible (field);
                field.setBounds (0, 0, 50, 20);
                field.grabKeyboardFocus();
                const auto taken = juce::Component::getCurrentlyFocusedComponent() == &field && pages->keyPressed (ctrl ('F'));
                pages->removeChildComponent (&field);
                expect (! taken, "Ctrl+F is left to a text field that has the keys");
            }
        }

        // UI review 31, item 35: the FX chain's chips in the tab row, one per
        // effect in chain order; a click bypasses (one undo step).
        if (auto* chips = findChild<FxChainChips> (*editor))
        {
            // (An effect in an empty slot for the test, taken out after.)
            auto spare = -1;
            for (int slot = IlanaSynthAudioProcessor::numFxSlots - 1; slot >= 0 && spare < 0; --slot)
                if (processor.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1))->load() < 0.5f)
                    spare = slot;
            auto* spareType = spare >= 0 ? processor.apvts.getParameter ("fx_slot" + juce::String (spare + 1)) : nullptr;
            if (spareType != nullptr)
                spareType->setValueNotifyingHost (spareType->convertTo0to1 (13.0f)); // Reverb
            settle (300);
            auto loaded = 0, firstSlot = -1;
            for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
                if (processor.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1))->load() > 0.5f)
                {
                    ++loaded;
                    firstSlot = firstSlot < 0 ? slot : firstSlot;
                }
            auto inOrder = true;
            auto previous = -1;
            for (const auto& box : chips->getChipBoxes())
            {
                inOrder = inOrder && box.first > previous;
                previous = box.first;
            }
            expect (chips->getNumChips() == loaded && inOrder, "one FX chip per effect, in chain order ("
                                                                   + juce::String (chips->getNumChips()) + " of " + juce::String (loaded) + ")");
            if (firstSlot >= 0)
            {
                auto* bypass = processor.apvts.getRawParameterValue ("fx_slot" + juce::String (firstSlot + 1) + "_bypass");
                const auto before = bypass->load();
                chips->toggleBypass (firstSlot);
                const auto after = bypass->load();
                processor.getUndoManager().undo();
                expect (after != before && bypass->load() == before, "an FX chip's click bypasses its effect, one undo step");
            }
            auto& hover = pages->getHoverLine();
            const auto chipsArea = chips->getBounds();
            expect (chips->isVisible() && ! hover.getBounds().intersects (chipsArea), "the hover line never covers the FX chips");
            if (spareType != nullptr)
                spareType->setValueNotifyingHost (0.0f);
            settle (300);
        }
        else
        {
            expect (false, "the FX chain's chips are in the tab row");
        }

        // UI review 6, header and global: VOICES as "3/32", a tuning
        // indicator while a scale retunes, MASTER at 0 dB on the factory
        // presets (their level in the hidden trim), the hover line.
        {
            expect (pages->getVoicesText().matchesWildcard ("*/*", true)
                        && pages->getVoicesText().fromFirstOccurrenceOf ("/", false, false).getIntValue() >= 1,
                    "VOICES reads as a count of the most (" + pages->getVoicesText() + ")");

            settle (400); // (the A|B swaps above land first: they restore the tuning too)
            expect (pages->getTuningIndicatorText().isEmpty(), "no tuning indicator in 12-TET");
            juce::String error;
            const auto loaded = processor.loadTuningScale ("! test.scl\nQuarter tones\n 2\n!\n 150.0\n 2/1\n", error);
            settle (600); // (the header refreshes on a 250 ms timer)
            expect (loaded && pages->getTuningIndicatorText().startsWith ("TUNING: "),
                    "a Scala scale shows the tuning indicator (" + pages->getTuningIndicatorText() + error + ", on "
                        + juce::String (processor.apvts.getRawParameterValue ("tuning_on")->load()) + ", scale "
                        + processor.getTuningState().getDescription() + ")");
            // (UI review 31, item 21: it shows beside the category in the preset box.)
            if (auto* display = findChild<PresetDisplay> (*editor))
                expect (display->getTuning().isNotEmpty() && display->getTuning() == pages->getTuningIndicatorText().fromFirstOccurrenceOf ("TUNING: ", false, false),
                        "the tuning shows beside the preset's category (" + display->getTuning() + ")");
            if (auto* tuningOn = processor.apvts.getParameter ("tuning_on"))
                tuningOn->setValueNotifyingHost (0.0f);
            settle (600);
            expect (pages->getTuningIndicatorText().isEmpty(), "switching the tuning off hides the indicator ("
                                                                   + pages->getTuningIndicatorText() + ", on "
                                                                   + juce::String (processor.apvts.getRawParameterValue ("tuning_on")->load()) + ")");

            const auto factoryNames = processor.getFactoryPresetNames();
            const auto presetBefore = factoryNames.indexOf (processor.getCurrentPresetName());
            auto atZero = 0, checked = 0;
            juce::String offender;
            for (int index = 0; index < factoryNames.size(); index += 7)
            {
                processor.loadFactoryPreset (index);
                const auto master = processor.apvts.getRawParameterValue ("master")->load();
                ++checked;
                if (std::abs (master) < 0.01f)
                    ++atZero;
                else if (offender.isEmpty())
                    offender = factoryNames[index] + " " + juce::String (master, 2);
            }
            processor.loadFactoryPreset (0);
            const auto initTrim = processor.apvts.getRawParameterValue ("output_trim")->load();
            processor.loadFactoryPreset (juce::jmax (0, presetBefore));
            settle (100);
            expect (atZero == checked && std::abs (initTrim + 5.1f) < 0.05f,
                    "MASTER reads 0 dB on the factory presets, the level in the hidden trim (" + juce::String (atZero) + "/"
                        + juce::String (checked) + offender + ", Init trim " + juce::String (initTrim, 1) + " dB)");
            const auto* trimParam = dynamic_cast<juce::AudioProcessorParameterWithID*> (processor.apvts.getParameter ("output_trim"));
            expect (trimParam != nullptr && ! trimParam->isAutomatable(), "the preset trim is hidden from automation");

            // UI review 7 (I7-23): MASTER's hover names the preset's own
            // level, and the preset menu resets it (one undo step).
            {
                processor.loadFactoryPreset (0);
                settle (150);
                const auto tip = pages->getMasterTooltip();
                const auto shown = pages->presetLevelText();
                pages->resetPresetLevel();
                settle (150);
                const auto reset = processor.apvts.getRawParameterValue ("output_trim")->load();
                const auto tipAfter = pages->getMasterTooltip();
                processor.getUndoManager().undo();
                const auto undone = processor.apvts.getRawParameterValue ("output_trim")->load();
                expect (shown == "-5.1 dB" && tip.contains ("preset's own level: -5.1 dB") && std::abs (reset) < 0.01f
                            && tipAfter.contains ("level: 0.0 dB") && std::abs (undone + 5.1f) < 0.05f,
                        "MASTER's hover names the preset level, and its reset is one undo step (" + shown + ", reset " + juce::String (reset, 2)
                            + ", undone " + juce::String (undone, 2) + ", " + tipAfter.fromLastOccurrenceOf ("  ", false, false) + ")");
                processor.loadFactoryPreset (juce::jmax (0, presetBefore));
                settle (100);
            }

            auto& hover = pages->getHoverLine();
            hover.restOn (compare);
            expect (compare == nullptr || (hover.isShowingLine() && hover.getShownTitle().isNotEmpty()),
                    "the hover line shows while the mouse rests on a control (" + hover.getShownTitle() + ")");
            hover.restOn (nullptr);
            bool clicksHere = true, clicksBelow = true;
            hover.getInterceptsMouseClicks (clicksHere, clicksBelow);
            expect (! hover.isShowingLine() && ! clicksHere && ! clicksBelow,
                    "the hover line is gone when the mouse leaves, and never takes a click");
        }

        // MATRIX is a tab of its own (2026-10-10): no switcher shows on it.
        pages->showPage ("MATRIX");
        settle (150);
        std::vector<SectionSwitcher*> switchers;
        findAll<SectionSwitcher> (*editor, switchers);
        auto anySwitcher = false;
        for (auto* sw : switchers)
            anySwitcher = anySwitcher || visibleInTree (sw);
        expect (! anySwitcher, "MATRIX has one page, so no page switch shows");
        pages->showPage ("MATRIX");
        settle (200);
        expect (pages->getCurrentPageId() == "MATRIX", "MATRIX opens in its own tab");
    }
}

void uiGroup_MATRIX_ROWS (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Matrix shows the preset's routing with the right destination text.
    pages->showPage ("MATRIX");
    settle (300);

    std::vector<MatrixRow*> rows;
    findAll<MatrixRow> (*editor, rows);
    MatrixRow* firstRow = nullptr;

    for (auto* row : rows)
        if (row->isVisible() && row->getSlotIndex() == 0)
            firstRow = row;

    expect (firstRow != nullptr, "matrix shows slot 1 for a preset that uses it");

    if (firstRow != nullptr)
    {
        std::vector<juce::ComboBox*> combos;
        findAll<juce::ComboBox> (*firstRow, combos);

        auto showsCutoff = false;

        for (auto* combo : combos)
            showsCutoff = showsCutoff || combo->getText() == ModNames::destination ((int) Mod::Destination::Filter1Cutoff);

        expect (showsCutoff, "matrix row shows the preset's destination as Filter 1 > Cutoff");

        for (auto* combo : combos)
        {
            if (combo->getText() == ModNames::destination ((int) Mod::Destination::Filter1Cutoff))
            {
                combo->setSelectedId ((int) Mod::Destination::Osc2Warp + 1, juce::sendNotificationSync);
                settle (50);
                expect (processor.readModSlot (0).destination == (int) Mod::Destination::Osc2Warp,
                        "picking a grouped destination writes the right index");
            }
        }
    }
}

void uiGroup_DROP_SOURCE (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Dropping a source on a knob routes it, and the knob grows a dot.
    // (BODY folds while its coupling is off: V9-4, so it is switched on.)
    if (auto* resOn = processor.apvts.getParameter ("res_on"))
        resOn->setValueNotifyingHost (1.0f);
    pages->showPage ("MAIN");
    settle (300);

    std::vector<KnobControl*> knobs;
    findAll<KnobControl> (*editor, knobs);
    KnobControl* reso = nullptr;

    for (auto* knob : knobs)
        if (knob->getParameterId() == "f1_reso" && visibleInTree (knob))
            reso = knob;

    expect (reso != nullptr, "filter page has a resonance knob");
    for (const auto* id : { "body_material", "body_size", "body_coupling" })
    {
        const auto found = std::any_of (knobs.begin(), knobs.end(), [id] (const KnobControl* knob)
        {
            return knob->getParameterId() == id && visibleInTree (knob) && knob->getWidth() > 30;
        });
        expect (found, juce::String ("BODY card shows ") + id);
    }

    if (reso != nullptr)
    {
        juce::DragAndDropTarget::SourceDetails details ("modsource:" + juce::String ((int) Mod::Source::Lfo2), nullptr, {});
        reso->itemDropped (details);
        settle (100);

        auto routed = -1;

        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            const auto slot = processor.readModSlot (i);

            if (slot.source == Mod::Source::Lfo2 && slot.destination == (int) Mod::Destination::Filter1Reso)
                routed = i;
        }

        expect (routed >= 0 && reso->getNumRoutings() == 1, "dropping LFO 2 on RESO routes it and shows one dot");

        // (PLAY's knobs hand their strip to the page that hosts it, so it is not found among the knob's children.)
        std::vector<ModDotStrip*> strips { &reso->getDotStrip() };

        if (! strips.empty() && strips[0]->onDepthChange != nullptr && routed >= 0)
        {
            strips[0]->onDepthChange (routed, -0.6f);
            expect (std::abs (processor.readModSlot (routed).depth + 0.6f) < 1.0e-3f, "dragging the dot sets the depth");

            // UI review 4 (V2): a double-click zeroes the depth (as knobs and
            // the source card do), keeps the routing, is one named undo
            // step, and undo brings the depth back.
            // (A knob too narrow for badges beside its rings leaves them
            // to the rings, which take the same double-click.)
            reso->syncRoutings();
            const auto onRing = strips[0]->getNumShown() == 0;
            auto& strip = onRing ? reso->getRingOverlay() : static_cast<juce::Component&> (*strips[0]);
            const auto at = onRing ? strip.getLocalPoint (reso, reso->getRingPoint (0, 0.5f))
                                   : juce::Point<float> ((float) strip.getWidth() * 0.5f, (float) ModDotStrip::dotPitch * 0.5f);
            const juce::MouseEvent click (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(), 1.0f, 0.0f,
                                          0.0f, 0.0f, 0.0f, &strip, &strip, juce::Time::getCurrentTime(), at,
                                          juce::Time::getCurrentTime(), 2, false);
            clearHistory();
            strip.mouseDoubleClick (click);
            settle (100);
            const auto zeroed = processor.readModSlot (routed).source == Mod::Source::Lfo2
                                && std::abs (processor.readModSlot (routed).depth) < 1.0e-4f;
            const auto steps = undoSteps();
            processor.getUndoManager().undo();
            settle (100);
            expect (zeroed && steps.size() == 1 && steps[0] == "Zero LFO 2 depth"
                        && processor.readModSlot (routed).source == Mod::Source::Lfo2
                        && std::abs (processor.readModSlot (routed).depth + 0.6f) < 1.0e-3f,
                    "double-clicking a depth dot zeroes it, keeps the routing, is one undo step ('" + steps.joinIntoString ("', '")
                        + "') and undo restores the depth");

            // Removal and bypass are on the dot's right-click menu.
            expect (strips[0]->onRemove != nullptr && strips[0]->onBypass != nullptr, "a depth dot offers Remove and Bypass");
            if (strips[0]->onBypass != nullptr)
            {
                strips[0]->onBypass (routed, true);
                const auto bypassed = processor.readModSlot (routed).bypass;
                strips[0]->onBypass (routed, false);
                expect (bypassed && ! processor.readModSlot (routed).bypass, "the dot's Bypass switches the routing off and on");
            }

            // Dropping the same source again is refused: it points at the
            // existing routing instead of adding a second.
            reso->itemDropped (details);
            settle (100);
            auto copies = 0;
            for (int i = 0; i < Mod::maxSlots; ++i)
            {
                const auto slot = processor.readModSlot (i);
                copies += slot.source == Mod::Source::Lfo2 && slot.destination == (int) Mod::Destination::Filter1Reso ? 1 : 0;
            }
            expect (copies == 1 && reso->getNumRoutings() == 1,
                    "dropping a source on a knob it already drives adds no second routing (" + juce::String (copies) + ")");
            reso->closeModCard();
        }
        else
        {
            expect (false, "knob has a dot strip");
        }

        // An effect knob is a drop target too (plain-parameter destination).
        pages->showPage ("FX");
        processor.assignFxSlot (1, 20); // OTT
        settle (400);
        knobs.clear();
        findAll<KnobControl> (*editor, knobs);
        KnobControl* ottAmount = nullptr;

        for (auto* knob : knobs)
            if (knob->getParameterId() == "fx_ott_amount" && visibleInTree (knob))
                ottAmount = knob;

        expect (ottAmount != nullptr && ottAmount->isInterestedInDragSource (details), "OTT amount accepts modulation");
    }

    // (The old MODULATION card tests are retired: PLAY now carries its envelope and LFO carousels, covered in PlayPageUiTests.)
}

void uiGroup_MACRO_NAMES (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Legacy presets get named default macros, and the matrix shows the names.
    {
        pages->showPage ("MATRIX");
        settle (300);
        auto showsMacroName = false;
        std::vector<MatrixRow*> rows;
        findAll<MatrixRow> (*editor, rows);

        for (auto* row : rows)
        {
            std::vector<juce::ComboBox*> combos;
            findAll<juce::ComboBox> (*row, combos);

            for (auto* combo : combos)
                showsMacroName = showsMacroName || combo->getText() == "TONE";
        }

        expect (showsMacroName, "matrix source shows the macro's name alone ('TONE', review 9 I9-8)");
    }
}

void uiGroup_ENVLFO_PAGES (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // Every envelope has its pop-up editor, and each one shows its own controls.
    pages->showPage ("MAIN");
    settle (300);

    {
        const auto attackKnobs = [&] (juce::Component& root, bool& modAttackShown, bool& ampAttackShown)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (root, knobs);
            modAttackShown = ampAttackShown = false;

            for (auto* knob : knobs)
            {
                modAttackShown = modAttackShown || (knob->getParameterId() == "me_attack" && visibleInTree (knob));
                ampAttackShown = ampAttackShown || (knob->getParameterId() == "amp_attack" && visibleInTree (knob));
            }
        };
        bool modAttackShown = false, ampAttackShown = false, modOnAmp = false, ampOnAmp = false;

        if (auto* popover = uiOpenSourcePopover (ctx, Mod::Source::ModEnv))
            attackKnobs (*popover, modAttackShown, ampAttackShown);

        if (auto* popover = uiOpenSourcePopover (ctx, Mod::Source::AmpEnv))
            attackKnobs (*popover, modOnAmp, ampOnAmp);

        uiCloseSourcePopover (ctx);
        expect (modAttackShown && ! ampAttackShown && ! modOnAmp && ampOnAmp,
                "the MOD envelope's pop-up shows the mod envelope, the AMP envelope's the amp envelope");
    }

    // Envelope graph: a dragged handle lands where the mouse is.
    pages->showPage ("MAIN");
    settle (200);

    if (auto* page = pages->getCurrentPage())
    {
        std::vector<EnvelopeDisplay*> displays;
        findAll<EnvelopeDisplay> (*page, displays);
        EnvelopeDisplay* amp = nullptr;

        for (auto* display : displays)
            if (visibleInTree (display))
                amp = display;

        const auto set = [&processor] (const char* id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        const auto get = [&processor] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };

        if (amp != nullptr)
        {
            set ("amp_attack", 0.25f);
            set ("amp_decay", 0.3f);
            set ("amp_sustain", 0.5f);
            set ("amp_release", 0.5f);

            // Mirror of the display's geometry.
            const auto plot = amp->getPlotArea();
            const auto total = std::sqrt (0.25f) + std::sqrt (0.3f) + 0.55f + std::sqrt (0.5f);
            const auto scale = plot.getWidth() / juce::jmax (1.6f, total * 1.15f);
            const auto xA = plot.getX() + scale * std::sqrt (0.25f);
            const auto xS = xA + scale * std::sqrt (0.3f) + scale * 0.55f;
            const auto xR = xS + scale * std::sqrt (0.5f);

            auto source = juce::Desktop::getInstance().getMainMouseSource();
            const auto drag = [&] (juce::Point<float> from, juce::Point<float> to)
            {
                const auto now = juce::Time::getCurrentTime();
                const auto make = [&] (juce::Point<float> at, bool dragged)
                {
                    return juce::MouseEvent (source, at, juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                             amp, amp, now, from, now, 1, dragged);
                };

                auto& component = static_cast<juce::Component&> (*amp);
                component.mouseDown (make (from, false));
                component.mouseDrag (make (to, true));
                component.mouseUp (make (to, true));
            };

            drag ({ xA, plot.getY() }, { xA + 30.0f, plot.getY() });
            const auto expectedAttack = std::pow ((xA + 30.0f - plot.getX()) / scale, 2.0f);
            expect (std::abs (get ("amp_attack") - expectedAttack) < expectedAttack * 0.03f,
                    "attack handle follows the mouse (" + juce::String (get ("amp_attack"), 3) + " s, expected "
                        + juce::String (expectedAttack, 3) + ")");

            // Re-measure with the new attack, then drag the release end.
            const auto total2 = std::sqrt (get ("amp_attack")) + std::sqrt (0.3f) + 0.55f + std::sqrt (0.5f);
            const auto scale2 = plot.getWidth() / juce::jmax (1.6f, total2 * 1.15f);
            const auto xS2 = plot.getX() + scale2 * (std::sqrt (get ("amp_attack")) + std::sqrt (0.3f) + 0.55f);
            const auto xR2 = xS2 + scale2 * std::sqrt (0.5f);
            drag ({ xR2, plot.getBottom() }, { xR2 - 25.0f, plot.getBottom() });
            const auto expectedRelease = std::pow ((xR2 - 25.0f - xS2) / scale2, 2.0f);
            expect (std::abs (get ("amp_release") - expectedRelease) < expectedRelease * 0.03f,
                    "release handle follows the mouse (" + juce::String (get ("amp_release"), 3) + " s, expected "
                        + juce::String (expectedRelease, 3) + ")");
            juce::ignoreUnused (xR);

            // Drag feel (ilana: "adjusting adsrs via the graph feels weird"): the first click does not
            // jump the handle to the pointer, Shift is 10x finer, dragging left past the stage's start
            // does not turn into a long time, a double-click resets.
            {
                set ("amp_attack", 0.25f);
                set ("amp_decay", 0.3f);
                set ("amp_sustain", 0.5f);
                set ("amp_release", 0.5f);
                const auto gesture = [&] (juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods, int clicks = 1)
                {
                    const auto now = juce::Time::getCurrentTime();
                    const auto make = [&] (juce::Point<float> at, bool dragged)
                    {
                        return juce::MouseEvent (source, at, mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                 amp, amp, now, from, now, clicks, dragged);
                    };
                    auto& component = static_cast<juce::Component&> (*amp);
                    component.mouseDown (make (from, false));
                    component.mouseDrag (make (to, true));
                    component.mouseUp (make (to, true));
                };
                const auto attackHandle = amp->getHandlePosition (0);

                gesture (attackHandle + juce::Point<float> (6.0f, 0.0f), attackHandle + juce::Point<float> (6.0f, 0.0f), {});
                expect (std::abs (get ("amp_attack") - 0.25f) < 0.003f,
                        "a press 6 px off the attack handle does not move it (" + juce::String (get ("amp_attack"), 4) + " s)");

                const auto before = get ("amp_attack");
                gesture (attackHandle, attackHandle + juce::Point<float> (20.0f, 0.0f), {});
                const auto coarse = get ("amp_attack") - before;
                set ("amp_attack", before);
                gesture (attackHandle, attackHandle + juce::Point<float> (20.0f, 0.0f), juce::ModifierKeys::shiftModifier);
                const auto fine = get ("amp_attack") - before;
                expect (coarse > 0.0f && fine > 0.0f && fine < coarse * 0.2f,
                        "Shift drags the handle about 10x finer (" + juce::String (fine, 4) + " vs " + juce::String (coarse, 4) + ")");

                set ("amp_attack", 0.25f);
                gesture (attackHandle, attackHandle - juce::Point<float> (400.0f, 0.0f), {});
                expect (get ("amp_attack") < 0.01f,
                        "dragging left of the stage's start gives the shortest time, not a long one (" + juce::String (get ("amp_attack"), 3) + " s)");

                set ("amp_attack", 0.25f);
                set ("amp_attack", 0.6f);
                gesture (amp->getHandlePosition (0), amp->getHandlePosition (0), {}, 2);
                static_cast<juce::Component&> (*amp).mouseDoubleClick (juce::MouseEvent (source, amp->getHandlePosition (0), {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                                                         amp, amp, juce::Time::getCurrentTime(), amp->getHandlePosition (0), juce::Time::getCurrentTime(), 2, false));
                if (auto* parameter = processor.apvts.getParameter ("amp_attack"))
                    expect (std::abs (get ("amp_attack") - parameter->convertFrom0to1 (parameter->getDefaultValue())) < 0.002f,
                            "double-clicking the attack handle resets it");

                // A knob: 20 px up moves it 10% of its range, Shift 1%, sideways nothing, Ctrl-click resets.
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*page, knobs);
                KnobControl* knob = nullptr;
                for (auto* candidate : knobs)
                    if (visibleInTree (candidate) && candidate->getSlider().getInterval() == 0.0 && candidate->getSlider().isEnabled())
                    {
                        knob = candidate;
                        break;
                    }
                if (knob != nullptr)
                {
                    auto& slider = knob->getSlider();
                    slider.setPopupDisplayEnabled (false, false, nullptr); // never open a real popup window in a test
                    const auto centre = juce::Point<float> (slider.getWidth() * 0.5f, slider.getHeight() * 0.5f);
                    const auto press = [&] (juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods)
                    {
                        const auto now = juce::Time::getCurrentTime();
                        const auto make = [&] (juce::Point<float> at, bool dragged)
                        {
                            return juce::MouseEvent (source, at, mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                     &slider, &slider, now, from, now, 1, dragged);
                        };
                        slider.mouseDown (make (from, false));
                        slider.mouseDrag (make (to, true));
                        slider.mouseUp (make (to, true));
                    };
                    const auto proportion = [&slider] { return slider.valueToProportionOfLength (slider.getValue()); };
                    slider.setValue (slider.proportionOfLengthToValue (0.5), juce::sendNotificationSync);
                    press (centre, centre - juce::Point<float> (0.0f, 20.0f), {});
                    expect (std::abs (proportion() - 0.6) < 0.002, "a knob moves 10% for a 20 px drag up (" + juce::String (proportion(), 4) + ")");
                    slider.setValue (slider.proportionOfLengthToValue (0.5), juce::sendNotificationSync);
                    press (centre, centre + juce::Point<float> (30.0f, 0.0f), {});
                    expect (std::abs (proportion() - 0.5) < 0.002, "a sideways drag does not turn a knob (" + juce::String (proportion(), 4) + ")");
                    press (centre, centre - juce::Point<float> (0.0f, 20.0f), juce::ModifierKeys::shiftModifier);
                    expect (std::abs (proportion() - 0.51) < 0.002, "Shift turns a knob 10x finer (" + juce::String (proportion(), 4) + ")");
                    press (centre, centre, juce::ModifierKeys::ctrlModifier);
                    expect (std::abs (slider.getValue() - slider.getDoubleClickReturnValue()) < 1.0e-6, "Ctrl-click resets a knob to its default");
                }
                else
                {
                    expect (false, "ENV/LFO page has a continuous knob to drag");
                }
            }
        }
        else
        {
            expect (false, "ENV/LFO shows the amp envelope display");
        }
    }
}

void uiGroup_MAIN_OSC (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // MAIN shows OSC 1-3 by default, and only those; added ones get the same
    // full card, with the mode's own knobs.
    {
        pages->showPage ("MAIN");
        settle (300);

        const auto shownOnMain = [&] (const juce::String& id)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getParameterId() == id)
                    return true;
            return false;
        };

        const auto mainKnob = [&] (const juce::String& id) -> KnobControl*
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getParameterId() == id)
                    return knob;
            return nullptr;
        };

        // A switched-off oscillator keeps its place, folded to a slim strip
        // with its switch (UI review 6, V19, S8; review 8, V8-17).
        const juce::String osc3 (OscillatorIds::prefixes[2]);
        if (auto* on = processor.apvts.getParameter (osc3 + "_on"))
            on->setValueNotifyingHost (0.0f);
        settle (300);
        expect (mainKnob (osc3 + "_level") != nullptr && toggleFor (osc3 + "_on") != nullptr,
                "MAIN keeps a switched-off OSC 3's whole card, with its switch");

        if (auto* on = processor.apvts.getParameter (osc3 + "_on"))
            on->setValueNotifyingHost (1.0f);
        settle (300);

        expect (shownOnMain (osc3 + "_level") && ! shownOnMain ("osc4_level") && toggleFor ("osc4_on") == nullptr,
                "MAIN shows OSC 3 and hides OSC 4 by default");

        // Every strip is the same height, and the next one to add is one
        // button in the first empty slot (UI review 6, S33).
        {
            std::vector<juce::Button*> buttons;
            findAll<juce::Button> (*editor, buttons);
            auto adds = 0;
            for (auto* button : buttons)
                if (visibleInTree (button) && button->getButtonText().contains ("ADD OSC"))
                    ++adds;
            expect (adds <= 1, "PLAY offers at most one ADD OSC button (" + juce::String (adds) + ")");
            auto* level1 = mainKnob ("osc1_level");
            auto* level3 = mainKnob (osc3 + "_level");
            expect (level1 != nullptr && level3 != nullptr && level1->getHeight() == level3->getHeight(),
                    "PLAY's oscillator strips share one size"
                        + (level1 != nullptr && level3 != nullptr ? " (" + juce::String (level1->getHeight()) + ", " + juce::String (level3->getHeight()) + "; "
                                                                 + level1->getBoundsInParent().toString() + " / " + level3->getBoundsInParent().toString() + "; "
                                                                 + juce::String (level1->labelBlockHeight()) + " " + juce::String (level3->labelBlockHeight()) + " "
                                                                 + juce::String (level1->getNameLabel().getFont().getHeight()) + " " + juce::String (level3->getNameLabel().getFont().getHeight()) + ")" : juce::String()));
        }

        for (int osc = 3; osc < OscillatorIds::count; ++osc)
            processor.addOscillator (osc);

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
            if (auto* mode = processor.apvts.getParameter (prefix + "_mode"))
                mode->setValueNotifyingHost (mode->convertTo0to1 (3.0f));
            if (auto* on = processor.apvts.getParameter (prefix + "_on"))
                on->setValueNotifyingHost (1.0f);

            settle (400);
            std::vector<KnobControl*> mainKnobs;
            findAll<KnobControl> (*editor, mainKnobs);
            auto level = false, grainSize = false;

            for (auto* knob : mainKnobs)
            {
                if (! visibleInTree (knob))
                    continue;

                level = level || knob->getParameterId() == prefix + "_level";
                grainSize = grainSize || knob->getParameterId() == prefix + "_grain_size";
            }

            expect (level && grainSize,
                    "MAIN's OSC " + juce::String (osc + 1)
                        + " card shows level and grain size in granular mode");

            if (auto* mode = processor.apvts.getParameter (prefix + "_mode"))
                mode->setValueNotifyingHost (mode->convertTo0to1 (0.0f));
        }

        for (int osc = 3; osc < OscillatorIds::count; ++osc)
            processor.removeOscillator (osc);

        settle (300);
        expect (! shownOnMain ("osc4_level"), "removing OSC 4 takes its card off MAIN");
    }
}

void uiGroup_FX_QUICK_ADD (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // An empty FX rack offers one-click effects.
    {
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            processor.assignFxSlot (slot, 0);

        pages->showPage ("FX");
        settle (500);
        std::vector<juce::TextButton*> textButtons;
        findAll<juce::TextButton> (*editor, textButtons);
        juce::TextButton* reverb = nullptr;

        juce::TextButton* delay = nullptr;

        for (auto* button : textButtons)
        {
            if (button->getButtonText() == "REVERB" && visibleInTree (button))
                reverb = button;
            if (button->getButtonText() == "DELAY" && visibleInTree (button))
                delay = button;
        }

        expect (reverb != nullptr && delay != nullptr, "the empty rack shows the effect library");

        if (reverb != nullptr)
        {
            reverb->triggerClick();
            settle (400);
            const auto* slot1 = processor.apvts.getRawParameterValue ("fx_slot1");
            expect (slot1 != nullptr && (int) slot1->load() == 13, "the library's REVERB puts a reverb in slot 1");
            // One view of the chain (S5-18, S6-25): with an effect loaded the
            // library leaves the page; + ADD EFFECT opens it in a call-out.
            std::vector<FxLibraryButton*> libraryButtons;
            findAll<FxLibraryButton> (*editor, libraryButtons);
            expect (std::none_of (libraryButtons.begin(), libraryButtons.end(), [] (FxLibraryButton* b) { return visibleInTree (b); }),
                    "no library or chain list beside the cards once the rack has an effect");

            // (UI review 31: the one + ADD EFFECT is the add row at the page's foot.)
            juce::Button* addEffect = nullptr;
            std::vector<DashedAddButton*> allButtons;
            findAll<DashedAddButton> (*editor, allButtons);
            for (auto* button : allButtons)
                if (button->getButtonText().contains ("ADD EFFECT") && visibleInTree (button))
                    addEffect = button;
            expect (addEffect != nullptr, "the rack has + ADD EFFECT");

            if (addEffect != nullptr)
            {
                addEffect->triggerClick();
                // (Briefly: a call-out closes itself within 200 ms while the
                // process isn't in front, as under xvfb.)
                settle (80);
                FxLibraryView* callout = nullptr;
                std::vector<FxLibraryView*> views;
                findAll<FxLibraryView> (*editor, views);
                for (auto* view : views)
                    if (view->getName() == "FX LIBRARY" && visibleInTree (view))
                        callout = view;
                expect (callout != nullptr, "+ ADD EFFECT opens the library in a call-out");

                if (callout != nullptr)
                {
                    auto* inRack = callout->findButton (13);
                    expect (inRack != nullptr && inRack->isEnabled() && inRack->getInRackSlot() == 0
                                && inRack->getTooltip().contains ("slot 1"),
                            "a type already in the rack has an in-rack dot and says where, not greyed out (V6-25)");
                    // (UI review 7, I7-28: Spaces is Reverb's Airwindows model, beside it.)
                    auto* spaces = callout->findButton (34);
                    expect (spaces != nullptr && spaces->getKind() == FxLibraryButton::Kind::airwindowsModel && spaces->getTooltip().contains ("Spaces")
                                && spaces->getTooltip().contains ("Airwindows"),
                            "Airwindows effects are named by their job with an Airwindows note (S6-27)");
                    if (auto* addDelay = callout->findButton (9))
                        addDelay->triggerClick();
                    settle (400);
                    const auto* slot2 = processor.apvts.getRawParameterValue ("fx_slot2");
                    expect (slot2 != nullptr && (int) slot2->load() == 9, "a pick from the call-out goes into the next empty slot");
                    views.clear();
                    findAll<FxLibraryView> (*editor, views);
                    expect (std::none_of (views.begin(), views.end(), [] (FxLibraryView* v) { return v->getName() == "FX LIBRARY" && visibleInTree (v); }),
                            "the call-out closes after a pick");
                }
            }
        }

        // The names: by job, the catch-all "Airwindows (all)", no "AW" prefix.
        auto awPrefix = false;
        for (int type = 1; type <= IlanaSynthAudioProcessor::numFxTypes; ++type)
            awPrefix = awPrefix || fxTypeName (type).startsWith ("AW ") || fxTypeName (type) == "-";
        expect (! awPrefix && fxTypeName (30) == "Airwindows (all)" && fxTypeName (35) == "Echoes",
                "every effect has a library name, none starts with AW, the catch-all is \"Airwindows (all)\"");
    }
}

void uiGroup_FX_CARDS (UiCtx& ctx)
{
    UI_GROUP_PRELUDE
    // FX cards (UI review 4, batch D): sized to their controls, a display
    // per family, one MIX, the header's solo / band / SLOT BLEND.
    {
        const auto loadFx = [&] (std::initializer_list<int> types)
        {
            auto slot = 1;
            for (auto type : types)
                processor.assignFxSlot (slot++, type);
            for (; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
                processor.assignFxSlot (slot, 0);
            settle (400);
        };
        // The stack's viewport: the one whose content holds the FX knobs.
        const auto stackViewport = [&]() -> juce::Viewport*
        {
            std::vector<juce::Viewport*> viewports;
            findAll<juce::Viewport> (*editor, viewports);
            for (auto* viewport : viewports)
            {
                if (! visibleInTree (viewport) || viewport->getViewedComponent() == nullptr)
                    continue;
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*viewport->getViewedComponent(), knobs);
                for (auto* knob : knobs)
                    if (knob->getParameterId().startsWith ("fx_"))
                        return viewport;
            }
            return nullptr;
        };
        // Every shown control of the cards (and the displays), in content coordinates.
        const auto cardControls = [&] (juce::Viewport& viewport)
        {
            std::vector<juce::Component*> found;
            for (auto* child : viewport.getViewedComponent()->getChildren())
                if (child->isVisible() && ! child->getBounds().isEmpty())
                    found.push_back (child);
            return found;
        };
        const auto checkCards = [&] (const juce::String& what, bool allInView)
        {
            auto* viewport = stackViewport();
            expect (viewport != nullptr, what + ": the FX stack is on screen");
            if (viewport == nullptr)
                return;
            viewport->setViewPosition (0, 0);
            settle (100);
            const auto controls = cardControls (*viewport);
            const auto view = viewport->getViewArea();
            // (A packed knob's bounds reach packedPadX past its cell for the ring's room, as on PLAY:
            // neighbours are judged by their cells. A stacked choice in a knob's cell says so.)
            const auto cellOf = [] (juce::Component* control)
            {
                const auto* knob = dynamic_cast<KnobControl*> (control);
                if ((knob != nullptr && knob->isPacked()) || (bool) control->getProperties().getWithDefault ("packedCell", false))
                    return control->getBounds().reduced (KnobControl::packedPadX, 0).withTrimmedTop (KnobControl::packedPadTop);
                return control->getBounds();
            };
            juce::String clipped, overlapping;
            for (size_t i = 0; i < controls.size(); ++i)
            {
                const auto bounds = controls[i]->getBounds();
                if (! viewport->getViewedComponent()->getLocalBounds().contains (bounds) || (allInView && ! view.contains (bounds)))
                    clipped << controls[i]->getName() << typeid (*controls[i]).name() << bounds.toString() << " ";
                for (size_t j = i + 1; j < controls.size(); ++j)
                    if (cellOf (controls[i]).intersects (cellOf (controls[j])))
                        overlapping << bounds.toString() << "/" << controls[j]->getBounds().toString() << " ";
            }
            expect (clipped.isEmpty(), what + ": no FX card control is clipped " + clipped);
            expect (overlapping.isEmpty(), what + ": no two FX card controls overlap " + overlapping);
        };
        const auto shownKnob = [&] (const juce::String& id)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
                if (knob->getParameterId() == id && visibleInTree (knob) && ! knob->getBounds().isEmpty())
                    return true;
            return false;
        };
        const auto shownDisplays = [&]
        {
            std::vector<FxDisplay*> displays;
            findAll<FxDisplay> (*editor, displays);
            return (int) std::count_if (displays.begin(), displays.end(), [] (FxDisplay* d) { return visibleInTree (d); });
        };
        const auto findButtons = [&] (const juce::String& text)
        {
            std::vector<juce::TextButton*> buttons, matching;
            findAll<juce::TextButton> (*editor, buttons);
            for (auto* button : buttons)
                if (button->getButtonText() == text && visibleInTree (button))
                    matching.push_back (button);
            return matching;
        };

        pages->showPage ("FX");
        loadFx ({ 27, 2, 20 }); // Neuro Wobble's rack: Vowel, Drive, OTT
        checkCards ("3 effects", true);
        expect (shownDisplays() == 3, "Vowel, Drive and OTT cards each show a display");
        expect (findButtons ("S").size() == 3, "each card header has a SOLO button");
        loadFx ({ 27, 2, 20, 13 });
        checkCards ("4 effects", false);
        loadFx ({ 9, 4, 13, 21 });
        checkCards ("delay, comp, reverb, limiter", false);
        expect (shownDisplays() == 4, "delay, comp, reverb and limiter each show a display");

        // The delay's custom tap grid only while TAPS is on.
        const auto tapGridShown = [&]
        {
            std::vector<juce::Component*> all;
            findAll<juce::Component> (*editor, all);
            for (auto* component : all)
                if (component->getName() == "CUSTOM TAP GRID")
                    return visibleInTree (component);
            return false;
        };
        auto* taps = processor.apvts.getParameter ("fx_taps_on");
        taps->setValueNotifyingHost (0.0f);
        settle (300);
        const auto hiddenWhileOff = ! tapGridShown();
        taps->setValueNotifyingHost (1.0f);
        settle (300);
        expect (hiddenWhileOff && tapGridShown(), "the delay's tap grid shows only while TAPS is on");
        checkCards ("delay with taps", false);
        taps->setValueNotifyingHost (0.0f);

        // Solo is a visible button in the card header.
        if (auto soloButtons = findButtons ("S"); ! soloButtons.empty())
        {
            soloButtons.front()->triggerClick();
            settle (200);
            expect (processor.apvts.getRawParameterValue ("fx_slot1_solo")->load() > 0.5f, "the header's SOLO button solos its slot");
            soloButtons.front()->triggerClick();
            settle (200);
        }

        // The slot's dry / wet, but only where the effect has no MIX of its
        // own (UI review 7, V7-7): here the limiter's, as a MIX knob in its
        // row (UI review 8, S8-6).
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            auto inCards = 0;
            for (auto* knob : knobs)
                if (knob->getParameterId().startsWith ("fx_slot") && knob->getParameterId().endsWith ("_mix") && visibleInTree (knob)
                    && ! knob->getBounds().isEmpty())
                    ++inCards;
            expect (inCards == 1, "only the limiter's card (no MIX of its own) has the slot's MIX (" + juce::String (inCards) + " of 4)");
        }

        // One MIX: the Airwindows algorithms' own Dry/Wet is hidden.
        loadFx ({ 33 }); // AW Saturation; its first effect, Density3, has Dry/Wet as knob 4
        processor.apvts.getParameter ("fx_awsat_algo")->setValueNotifyingHost (0.0f);
        settle (300);
        expect (shownKnob ("fx_awsat_p1") && ! shownKnob ("fx_awsat_p4") && shownKnob ("fx_awsat_mix"),
                "AW Saturation shows DENSITY and MIX but not the algorithm's Dry/Wet");
        {
            // The rack's words and units (I9-15 / S9-5): Density3 reads
            // AMOUNT, LOW CUT and OUTPUT in dB, as the built-in twin would.
            std::vector<KnobControl*> awKnobs;
            findAll<KnobControl> (*editor, awKnobs);
            juce::String labels, outputText;
            for (const auto* id : { "fx_awsat_p1", "fx_awsat_p2", "fx_awsat_p3" })
                for (auto* knob : awKnobs)
                    if (knob->getParameterId() == id && visibleInTree (knob))
                    {
                        labels << knob->getLabelText() << "/";
                        if (juce::String (id) == "fx_awsat_p3")
                            outputText = knob->getSlider().getTextFromValue (knob->getSlider().getValue());
                    }
            expect (labels == "AMOUNT/LOW CUT/OUTPUT/" && outputText.endsWith (" dB"),
                    "an Airwindows saturation reads AMOUNT, LOW CUT, OUTPUT in dB (" + labels + " " + outputText + ")");
        }
        expect (shownDisplays() == 1, "AW Saturation shows its transfer curve");
        loadFx ({ 30 });
        if (auto* algo = processor.apvts.getParameter ("fx_aw_algo"))
            algo->setValueNotifyingHost (algo->convertTo0to1 (3.0f)); // Density: Dry/Wet is knob 4
        settle (300);
        expect (shownKnob ("fx_aw_p1") && ! shownKnob ("fx_aw_p4") && shownKnob ("fx_aw_mix"),
                "the all-in-one Airwindows module hides the algorithm's Dry/Wet");
        {
            processor.apvts.getParameter ("fx_aw_algo")->setValueNotifyingHost (0.0f); // ToTape6
            settle (300);
            std::vector<juce::ComboBox*> boxes;
            findAll<juce::ComboBox> (*editor, boxes);
            juce::String shown;
            for (auto* box : boxes)
                if (visibleInTree (box) && box->getTooltip().startsWith ("Airwindows ToTape6"))
                    shown = box->getText();
            expect (shown == "To Tape 6", "the Airwindows menu shows \"To Tape 6\" for ToTape6 (" + shown + ")");
        }
        checkCards ("Airwindows", true);

        // DICE starts its own undo step, and undo brings the chain back.
        loadFx ({ 13, 9 });
        std::vector<DiceFxButton*> dice;
        findAll<DiceFxButton> (*editor, dice);
        if (! dice.empty())
        {
            processor.getUndoManager().beginNewTransaction();
            dice.front()->triggerClick();
            settle (300);
            const auto described = processor.getUndoManager().getUndoDescription();
            processor.getUndoManager().undo();
            settle (300);
            expect (described == "Dice FX chain" && (int) processor.apvts.getRawParameterValue ("fx_slot1")->load() == 13
                        && (int) processor.apvts.getRawParameterValue ("fx_slot2")->load() == 9,
                    "DICE FX is one undo step (" + described + ")");
        }
        else
            expect (false, "the FX toolbar has the FX dice");

        // UI review 6: small cards two to a row (S6-26), a split group with
        // its crossovers inline (S5-18, I6-27), a duplicate that says what it
        // is with REMOVE, the new displays, reorder by dragging a header.
        {
            const auto setParam = [&processor] (const juce::String& id, float plain)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
            };
            const auto readParam = [&processor] (const juce::String& id)
            {
                const auto* value = processor.apvts.getRawParameterValue (id);
                return value != nullptr ? value->load() : 0.0f;
            };
            const auto typeButtons = [&]
            {
                std::vector<FxTypeButton*> found, shown;
                findAll<FxTypeButton> (*editor, found);
                for (auto* button : found)
                    if (visibleInTree (button))
                        shown.push_back (button);
                std::sort (shown.begin(), shown.end(), [] (FxTypeButton* a, FxTypeButton* b)
                           { return a->getY() != b->getY() ? a->getY() < b->getY() : a->getX() < b->getX(); });
                return shown;
            };

            loadFx ({ 27, 2, 20 });
            auto titles = typeButtons();
            auto* viewport = stackViewport();
            // The design's rack: one row per effect, one under the other (10 px
            // apart), each at its one size, 80 (ilana 2026-11-17: no card grows
            // on a short rack), and the dashed + ADD EFFECT row right under them.
            {
                const auto step1 = titles.size() == 3 ? titles[1]->getY() - titles[0]->getY() : 0;
                const auto step2 = titles.size() == 3 ? titles[2]->getY() - titles[1]->getY() : 0;
                expect (titles.size() == 3 && viewport != nullptr && titles[0]->getX() == titles[1]->getX() && step1 == step2
                            && step1 == 90,
                        "Vowel, Drive and OTT are equal 80 px rows one under the other, 10 apart, on a short rack too ("
                            + juce::String (step1) + ", " + juce::String (step2) + ")");
                std::vector<DashedAddButton*> tiles;
                findAll<DashedAddButton> (*editor, tiles);
                DashedAddButton* tile = nullptr;
                for (auto* candidate : tiles)
                    if (candidate->getButtonText().contains ("ADD EFFECT") && visibleInTree (candidate))
                        tile = candidate;
                const auto tileBox = tile != nullptr ? editor->getLocalArea (tile, tile->getLocalBounds()) : juce::Rectangle<int>();
                const auto viewBox = viewport != nullptr ? editor->getLocalArea (viewport, viewport->getLocalBounds()) : juce::Rectangle<int>();
                expect (tile != nullptr && tileBox.getY() >= viewBox.getBottom() - 4 && tileBox.getY() <= viewBox.getBottom() + 2
                            && tileBox.getHeight() >= 32,
                        "a short rack ends in a dashed + ADD EFFECT row right under its last effect");
                if (tile != nullptr)
                {
                    tile->triggerClick();
                    settle (80); // (a call-out closes itself within 200 ms under xvfb)
                    FxLibraryView* shown = nullptr;
                    std::vector<FxLibraryView*> views;
                    findAll<FxLibraryView> (*editor, views);
                    for (auto* view : views)
                        if (view->getName() == "FX LIBRARY" && visibleInTree (view))
                            shown = view;
                    expect (shown != nullptr, "the dashed + ADD EFFECT row opens the library in a call-out");
                    if (shown != nullptr)
                        if (auto* box = shown->findParentComponentOfClass<juce::CallOutBox>())
                            box->dismiss();
                    settle (300);
                }
            }
            // The knobs sit at the row's right end (no knobs stranded mid-row).
            if (titles.size() == 3 && viewport != nullptr)
            {
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*editor, knobs);
                auto ottMixRight = 0;
                for (auto* knob : knobs)
                    if (knob->getParameterId() == "fx_ott_mix" && visibleInTree (knob))
                        ottMixRight = knob->getRight();
                expect (ottMixRight > viewport->getWidth() - 40, "a row's knobs end at its right edge (V9-3)");
            }

            loadFx ({ 7, 2, 13, 20 });
            setParam ("fx_slot2_band", 1.0f);
            setParam ("fx_slot3_band", 3.0f);
            settle (300);
            std::vector<CrossoverStrip*> strips;
            findAll<CrossoverStrip> (*editor, strips);
            const auto shownStrips = std::count_if (strips.begin(), strips.end(), [] (CrossoverStrip* s) { return visibleInTree (s); });
            expect (shownStrips == 1, "two banded slots in a row form one split group with its crossovers (" + juce::String ((int) shownStrips) + ")");
            checkCards ("a split group", false);
            setParam ("fx_slot2_band", 0.0f);
            setParam ("fx_slot3_band", 0.0f);

            loadFx ({ 20, 2, 20 });
            auto removes = findButtons ("REMOVE");
            expect (removes.size() == 1, "a duplicate card offers REMOVE");
            if (! removes.empty())
            {
                processor.getUndoManager().beginNewTransaction();
                removes.front()->triggerClick();
                settle (300);
                expect ((int) readParam ("fx_slot3") == 0 && (int) readParam ("fx_slot1") == 20
                            && processor.getUndoManager().getUndoDescription() == "Remove OTT",
                        "REMOVE takes the duplicate out as one undo step (" + processor.getUndoManager().getUndoDescription() + ")");
            }

            loadFx ({ 27, 5, 7, 6 });
            expect (shownDisplays() == 4, "vowel, comb, chorus and phaser each show a display");
            loadFx ({ 31, 35, 34 });
            expect (shownDisplays() == 3, "the vocoder and the Airwindows echoes and spaces show displays");

            // Drag Vowel's header onto OTT's card: Drive, OTT, Vowel.
            loadFx ({ 27, 2, 20 });
            titles = typeButtons();
            auto* content = viewport != nullptr ? viewport->getViewedComponent() : nullptr;
            if (titles.size() == 3 && content != nullptr)
            {
                auto source = juce::Desktop::getInstance().getMainMouseSource();
                const auto now = juce::Time::getCurrentTime();
                const auto from = juce::Point<float> ((float) titles[0]->getRight() + 12.0f, (float) titles[0]->getBounds().getCentreY());
                const auto to = juce::Point<float> ((float) titles[2]->getRight() + 40.0f, (float) titles[2]->getBounds().getCentreY() + 40.0f);
                const auto make = [&] (juce::Point<float> at, bool dragged)
                {
                    return juce::MouseEvent (source, at, juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                             content, content, now, from, now, 1, dragged);
                };
                processor.getUndoManager().beginNewTransaction();
                content->mouseDown (make (from, false));
                content->mouseDrag (make (to, true));
                content->mouseUp (make (to, true));
                settle (300);
                expect ((int) readParam ("fx_slot1") == 2 && (int) readParam ("fx_slot2") == 20 && (int) readParam ("fx_slot3") == 27
                            && processor.getUndoManager().getUndoDescription() == "Move Vowel",
                        "dragging a card's header reorders the chain as one undo step (" + juce::String ((int) readParam ("fx_slot1")) + "," + juce::String ((int) readParam ("fx_slot2")) + "," + juce::String ((int) readParam ("fx_slot3")) + " " + processor.getUndoManager().getUndoDescription() + ")");
            }
            else
                expect (false, "three FX cards to drag");

            // UI review 31: a short rack's dials are the SEQ page's 38, a long one's 32 (12); the
            // OUTPUT stage in the top bar and SUGGESTED NEXT as chips beside + ADD EFFECT (13); a
            // card's menu duplicates, copies, pastes and resets one effect, Delete takes the
            // selected card out, and a new effect goes in after the selected card (31).
            {
                const auto knobOf = [&] (const juce::String& id) -> KnobControl*
                {
                    std::vector<KnobControl*> knobs;
                    findAll<KnobControl> (*editor, knobs);
                    for (auto* knob : knobs)
                        if (knob->getParameterId() == id && visibleInTree (knob))
                            return knob;
                    return nullptr;
                };
                const auto dialOf = [&] (const juce::String& id) { auto* knob = knobOf (id); return knob != nullptr ? knob->getPackedDial() : 0.0f; };
                // (ilana 2026-11-17: every card one size, a rack of five's, whatever the rack holds.)
                loadFx ({ 7, 13 });
                expect (juce::approximatelyEqual (dialOf ("fx_chorus_rate"), 32.0f) && juce::approximatelyEqual (dialOf ("fx_reverb_size"), 32.0f),
                        "a short rack's cards have the long rack's 32 px dials (" + juce::String (dialOf ("fx_chorus_rate")) + ")");
                const auto cardHeights = [&]
                {
                    juce::Array<int> tops;
                    std::vector<FxTypeButton*> found;
                    findAll<FxTypeButton> (*editor, found);
                    for (auto* button : found)
                        if (visibleInTree (button))
                            tops.addUsingDefaultSort (editor->getLocalPoint (button, juce::Point<int>()).y);
                    return tops;
                };
                loadFx ({ 7 });
                const auto loneTop = cardHeights();
                loadFx ({ 7, 13, 2, 4, 20 });
                expect (juce::approximatelyEqual (dialOf ("fx_chorus_rate"), 32.0f), "a long rack's cards have 32 px dials (" + juce::String (dialOf ("fx_chorus_rate")) + ")");
                const auto fiveTops = cardHeights();
                loadFx ({ 7, 13 });
                const auto twoTops = cardHeights();
                expect (loneTop.size() == 1 && fiveTops.size() == 5 && twoTops.size() == 2 && loneTop[0] == fiveTops[0]
                            && twoTops[1] - twoTops[0] == fiveTops[1] - fiveTops[0],
                        "a card is the same size alone, in two and in five (" + juce::String (loneTop[0]) + " / " + juce::String (fiveTops[0])
                            + ", " + juce::String (twoTops[1] - twoTops[0]) + " / " + juce::String (fiveTops[1] - fiveTops[0]) + ")");
                if (auto* page = stackViewport() != nullptr ? stackViewport()->getParentComponent() : nullptr)
                {
                    std::vector<DashedAddButton*> adds;
                    findAll<DashedAddButton> (*page, adds);
                    const auto* view = stackViewport();
                    expect (adds.size() == 1 && adds.front()->getY() <= view->getBottom() + 10 && adds.front()->getY() < page->getHeight() - 200,
                            "a short rack's add row follows its last card, not the page's foot");
                }
                loadFx ({ 7, 13, 2, 4, 20 });

                auto* view = stackViewport();
                auto* page = view != nullptr ? view->getParentComponent() : nullptr;
                if (page != nullptr)
                {
                    juce::Component* clip = nullptr;
                    std::vector<ToggleControl*> toggles;
                    findAll<ToggleControl> (*page, toggles);
                    for (auto* toggle : toggles)
                        if (toggle->getParameterId() == "master_clip" && visibleInTree (toggle))
                            clip = toggle;
                    std::vector<OutputMeter*> meters;
                    findAll<OutputMeter> (*page, meters);
                    expect (clip != nullptr && clip->getBottom() <= view->getY() && meters.size() == 1 && meters.front()->getBottom() <= view->getY(),
                            "SOFT CLIP and the output meter are in the FX page's top bar");

                    loadFx ({ 7 });
                    std::vector<DashedAddButton*> adds;
                    findAll<DashedAddButton> (*page, adds);
                    std::vector<FxSuggestTile*> tiles;
                    findAll<FxSuggestTile> (*page, tiles);
                    auto chips = 0, inRow = 0;
                    for (auto* tile : tiles)
                        if (visibleInTree (tile))
                        {
                            ++chips;
                            inRow += adds.size() == 1 && tile->getY() == adds.front()->getY() && tile->getHeight() == adds.front()->getHeight() ? 1 : 0;
                        }
                    expect (adds.size() == 1 && visibleInTree (adds.front()) && chips >= 3 && inRow == chips,
                            "SUGGESTED NEXT is one row of chips beside + ADD EFFECT (" + juce::String (chips) + " chips, " + juce::String (inRow) + " in its row)");
                }

                loadFx ({ 7, 13, 2 });
                view = stackViewport();
                page = view != nullptr ? view->getParentComponent() : nullptr;
                const auto call = [&] (const char* name, std::initializer_list<juce::var> arguments) -> juce::var
                {
                    if (page == nullptr || ! page->getProperties()[name].isMethod())
                        return {};
                    const std::vector<juce::var> list (arguments);
                    return page->getProperties()[name].getNativeFunction() (juce::var::NativeFunctionArgs ({}, list.data(), (int) list.size()));
                };
                const auto items = call ("fxCardMenu", { 0 });
                const auto itemId = [&] (const juce::String& text)
                {
                    if (const auto* array = items.getArray())
                        for (const auto& item : *array)
                            if (item.toString().fromFirstOccurrenceOf ("|", false, false).upToLastOccurrenceOf ("|", false, false) == text)
                                return item.toString().upToFirstOccurrenceOf ("|", false, false).getIntValue();
                    return 0;
                };
                expect (itemId ("Duplicate") > 0 && itemId ("Copy settings") > 0 && itemId ("Paste settings") > 0 && itemId ("Reset to default") > 0,
                        "a card's menu has Duplicate, Copy settings, Paste settings and Reset to default");

                // Duplicate: a new instance with a copy of the settings, right after the card.
                setParam ("fx_chorus_rate", 2.0f);
                processor.getUndoManager().beginNewTransaction();
                call ("fxCardAction", { 0, itemId ("Duplicate") });
                settle (300);
                auto copied = 0.0f;
                expect ((int) readParam ("fx_slot1") == 7 && (int) readParam ("fx_slot2") == 7 && (int) readParam ("fx_slot3") == 13 && (int) readParam ("fx_slot4") == 2
                            && processor.getFxInstanceValue (0, "fx_chorus_rate", copied) && std::abs (copied - 2.0f) < 0.01f
                            && std::abs (readParam ("fx_chorus_rate") - 2.0f) < 0.01f
                            && processor.getUndoManager().getUndoDescription() == "Duplicate Chorus",
                        "Duplicate puts a copy with the same settings right after the card, as one undo step ("
                            + juce::String ((int) readParam ("fx_slot2")) + ", " + processor.getUndoManager().getUndoDescription() + ")");
                processor.getUndoManager().undo();
                settle (300);
                expect ((int) readParam ("fx_slot2") == 13 && (int) readParam ("fx_slot4") == 0, "one undo takes the duplicate out again");

                // Copy and paste settings onto the same effect; reset it.
                loadFx ({ 7, 13, 2 });
                setParam ("fx_reverb_size", 0.8f);
                call ("fxCardAction", { 1, itemId ("Copy settings") });
                setParam ("fx_reverb_size", 0.2f);
                processor.getUndoManager().beginNewTransaction();
                call ("fxCardAction", { 1, itemId ("Paste settings") });
                settle (100);
                expect (std::abs (readParam ("fx_reverb_size") - 0.8f) < 0.01f && processor.getUndoManager().getUndoDescription() == "Paste Reverb settings",
                        "Paste settings brings the copied settings back as one undo step (" + juce::String (readParam ("fx_reverb_size")) + ")");
                const auto pasteOnDrive = call ("fxCardMenu", { 2 });
                expect (pasteOnDrive.toString().isNotEmpty() || pasteOnDrive.isArray(), "the menu lists for another card");
                if (const auto* array = pasteOnDrive.getArray())
                    for (const auto& item : *array)
                        if (item.toString().contains ("Paste settings"))
                            expect (item.toString().endsWith ("|0"), "Paste settings is off on another effect's card (" + item.toString() + ")");
                processor.getUndoManager().beginNewTransaction();
                call ("fxCardAction", { 1, itemId ("Reset to default") });
                settle (100);
                const auto* size = processor.apvts.getParameter ("fx_reverb_size");
                expect (size != nullptr && std::abs (size->getValue() - size->getDefaultValue()) < 0.001f && processor.getUndoManager().getUndoDescription() == "Reset Reverb",
                        "Reset to default puts the effect back to its defaults as one undo step");

                // A new effect goes in after the selected card; Delete takes the selected card out.
                titles = typeButtons();
                auto* content = view != nullptr ? view->getViewedComponent() : nullptr;
                if (titles.size() == 3 && content != nullptr)
                {
                    auto source = juce::Desktop::getInstance().getMainMouseSource();
                    const auto clickCard = [&] (FxTypeButton* title)
                    {
                        const auto at = juce::Point<float> ((float) title->getX() + 2.0f, (float) title->getY() - 6.0f);
                        const auto now = juce::Time::getCurrentTime();
                        const juce::MouseEvent event (source, at, juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                      content, content, now, at, now, 1, false);
                        content->mouseDown (event);
                        content->mouseUp (event.withNewPosition (at));
                    };
                    clickCard (titles[0]);
                    FxSuggestTile* delay = nullptr;
                    std::vector<FxSuggestTile*> tiles;
                    findAll<FxSuggestTile> (*editor, tiles);
                    for (auto* tile : tiles)
                        if (tile->getType() == 9 && visibleInTree (tile))
                            delay = tile;
                    expect (delay != nullptr, "DELAY is a suggested chip");
                    if (delay != nullptr)
                    {
                        delay->triggerClick();
                        settle (300);
                        expect ((int) readParam ("fx_slot1") == 7 && (int) readParam ("fx_slot2") == 9 && (int) readParam ("fx_slot3") == 13 && (int) readParam ("fx_slot4") == 2,
                                "a new effect goes in after the selected card (" + juce::String ((int) readParam ("fx_slot2")) + ")");
                    }
                    titles = typeButtons();
                    if (titles.size() == 4)
                    {
                        clickCard (titles[1]);
                        processor.getUndoManager().beginNewTransaction();
                        const auto taken = content->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
                        settle (300);
                        expect (taken && (int) readParam ("fx_slot2") == 0 && (int) readParam ("fx_slot3") == 13
                                    && processor.getUndoManager().getUndoDescription() == "Remove Delay",
                                "Delete takes the selected card out as one undo step (" + juce::String (taken ? "taken, " : "not taken, ")
                                    + processor.getUndoManager().getUndoDescription() + ")");
                    }
                }
                else
                    expect (false, "three FX cards for the menu tests");
            }
        }

        // The live meters: OTT and the limiter report gain while audio runs.
        {
            loadFx ({ 20, 21 });
            processor.apvts.getParameter ("fx_limit_ceiling")->setValueNotifyingHost (0.0f); // -24 dB
            juce::AudioBuffer<float> audio (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);
            for (int block = 0; block < 20; ++block)
            {
                audio.clear();
                processor.processBlock (audio, midi);
                midi.clear();
            }
            expect (processor.getLimiterGainReduction() < 0.99f && std::abs (processor.getOttBandGain (1) - 1.0f) > 0.01f,
                    "the limiter and OTT report their gain for the cards' meters");
            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            processor.processBlock (audio, off);
        }

        // The drive's transfer curve shows the live input as a dot (step 14);
        // a curve whose effect isn't in the rack shows none.
        {
            loadFx ({ 2 });
            FxDisplay display (processor), absent (processor);
            display.setSize (200, 80);
            absent.setSize (200, 80);
            display.setType (2, IlanaTheme::accent());
            absent.setType (3, IlanaTheme::accent());
            juce::AudioBuffer<float> audio (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);
            for (int block = 0; block < 20; ++block)
            {
                audio.clear();
                processor.processBlock (audio, midi);
                midi.clear();
            }
            display.refreshNow();
            absent.refreshNow();
            expect (absent.getInputDotLevel() == 0.0f && display.getInputDotLevel() > 0.004f,
                    "the drive's transfer curve marks the live input level (" + juce::String (display.getInputDotLevel())
                        + "), an effect not in the rack none (" + juce::String (absent.getInputDotLevel()) + ")");
            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            processor.processBlock (audio, off);
        }

        processor.loadFactoryPreset (neuroWobble);
        settle (300);
    }
}

