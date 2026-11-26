#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

#include "PluginEditor.h"
#include "BuildInfo.h"

#include <set>

#include <algorithm>
#include <array>

#include "dsp/BodyTuning.h"
#include "dsp/Modulation.h"
#include "dsp/TableFactory.h"
#include "gui/EnvelopeDisplay.h"
#include "gui/EqCurve.h"
#include "gui/FilterDisplay.h"
#include "gui/LfoDisplay.h"
#include "gui/CardTabs.h"
#include "gui/FilterWidgets.h"
#include "gui/VectorPad.h"
#include "gui/PhysicalView.h"
#include "gui/FmDiagram.h"
#include "gui/FmWidgets.h"
#include "gui/FmOperatorInfo.h"
#include "gui/OperatorEnvDisplay.h"
#include "gui/OperatorPoolCards.h"
#include "gui/GenerativeWidgets.h"
#include "gui/ClipEditor.h"
#include "gui/TableBrowser.h"
#include "gui/WavetableEditor.h"
#include "gui/ZoneMapEditor.h"
#include "gui/SampleBrowser.h"
#include "gui/LfoShapeMenu.h"
#include "gui/ModRoutes.h"
#include "gui/ModuleMenu.h"
#include "gui/ModulationCardView.h"
#include "gui/MatrixWidgets.h"
#include "gui/ParamControls.h"
#include "gui/FxInstanceControls.h"
#include "gui/OutputView.h"
#include "gui/ScopeDisplay.h"
#include "gui/SequencerEditors.h"
#include "gui/SubTabBar.h"
#include "gui/FxCarousel.h"
#include "gui/PatchCableView.h"
#include "gui/SectionColourPicker.h"
#include "gui/TutorialOverlay.h"
#include "gui/WaveDisplay.h"
#include "gui/SampleNameBox.h"

// The pages. Each header declares its page (its member functions are in the
// .cpp of the same name) and includes what it uses (tools/check_headers.sh),
// so the order here does not matter.
#include "gui/StateTabs.h"
#include "gui/pages/PageHelpers.h"
#include "gui/PhysicalApplies.h"
#include "gui/pages/OscPage.h"
#include "gui/pages/VectorPage.h"
#include "gui/pages/EnvLfoPages.h"
#include "gui/pages/ModSourcePanel.h"
#include "gui/pages/FmInputPages.h"
#include "gui/pages/SeqPage.h"
#include "gui/pages/PlayPage.h"
#include "gui/pages/MatrixPage.h"
#include "gui/FxInfoText.h"
#include "gui/pages/FxWidgets.h"
#include "gui/pages/FxPage.h"
#include "gui/pages/PatchLfo.h"
#include "gui/pages/PatchPage.h"

// The editor's header bar: loading and stepping presets, the history, A/B and
// favourite marks, the preset, dice and settings menus, the preset panel and
// the DX7 bank import. (Moved out of PluginEditor.cpp unchanged, code health
// before RC 1.0; the rest of the editor shell is there.)

void IlanaSynthAudioProcessorEditor::loadPresetIndex (int index, std::function<void (bool loaded)> then)
{
    const auto names = processorRef.getAllPresetNames();

    if (! juce::isPositiveAndBelow (index, names.size()))
        return;

    const auto bank = processorRef.getAllPresetBanks()[index];
    // The click resolves to a file now: a folder that changes before the
    // answer (a second instance saving) can't shift which preset loads.
    const auto factoryCount = (int) processorRef.getFactoryPresetNames().size();
    const auto userFile = index >= factoryCount ? processorRef.getUserPresetFiles()[index - factoryCount] : juce::File();
    // While the browser is open, one "Load anyway" covers the session.
    const auto browsing = presetPanel != nullptr && (presetPanel->isOpen() || presetDockShown);
    const auto asking = browsing && ! browsingAccepted && isPatchEdited() && asksBeforeReplacingEdits();
    const auto load = [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), index, userFile, name = names[index], then] (bool confirmed)
    {
        if (safeThis == nullptr)
            return;

        auto loaded = false;

        if (confirmed)
        {
            // One undo step for the whole preset, its data included.
            auto& self = *safeThis;
            self.closeWavetableEditor(); // it works on the old patch's table
            self.closeZoneEditor();
            self.closeSampleBrowser();
            IlanaAnim::armMorphs(); // (the old shapes fade out over the new ones)
            self.processorRef.beginEdit ("Load " + name);
            // KEEP FX (FX page): the rack as it was goes back over the preset's.
            const auto keptRack = FxPage::keepRackOnPresetLoad() ? self.processorRef.makeFxChainXml() : nullptr;
            loaded = userFile != juce::File() ? self.processorRef.loadUserPreset (userFile) : self.processorRef.loadPresetByIndex (index);
            if (loaded && keptRack != nullptr)
                self.processorRef.applyFxChainXml (*keptRack);
            self.processorRef.endEdit();

            if (loaded)
            {
                self.presetLoadFlash = 1.0f;
                IlanaAnim::notePresetLoad();
                IlanaAnim::playMorphs();
                self.animator.startTimerHz (60);

                self.updateHeaderButtons();
                self.rememberLoadedFingerprint();
            }
            else
                self.presetDisplay.showNotice ("COULDN'T LOAD"); // the patch and its name are unchanged
        }

        if (then != nullptr)
            then (loaded);
    };

    if (browsing && browsingAccepted)
    {
        load (true);
        return;
    }

    confirmReplacingPatch ("Loading '" + (bank.isNotEmpty() ? Presets::dx7DisplayName (names[index]) : names[index]) + "'", "Load anyway",
                           [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), load, asking] (bool confirmed)
                           {
                               if (safeThis != nullptr && confirmed && asking)
                                   safeThis->browsingAccepted = true;

                               load (confirmed);
                           });
}

void IlanaSynthAudioProcessorEditor::showHistoryMenu()
{
    auto& undoManager = processorRef.getUndoManager();

    const auto undos = undoManager.getUndoDescriptions();
    const auto redos = undoManager.getRedoDescriptions();

    constexpr int maxEntries = 32;

    juce::PopupMenu menu;

    if (undos.isEmpty() && redos.isEmpty())
    {
        menu.addItem (1, "No history yet", false);
    }
    else
    {
        if (! redos.isEmpty())
        {
            menu.addSectionHeader ("REDO");

            for (int i = 0; i < juce::jmin (maxEntries, redos.size()); ++i)
                menu.addItem (2000 + i, redos[i].isNotEmpty() ? redos[i]
                                                              : "Redo change " + juce::String (i + 1));
        }

        if (! undos.isEmpty())
        {
            if (! redos.isEmpty())
                menu.addSeparator();

            menu.addSectionHeader ("UNDO");

            for (int i = 0; i < juce::jmin (maxEntries, undos.size()); ++i)
                menu.addItem (1000 + i, undos[i].isNotEmpty() ? undos[i]
                                                              : "Undo change " + juce::String (i + 1));
        }
    }

    menu.showMenuAsync (IlanaTheme::menuOptions (&historyButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result < 1000)
                                return;

                            if (result < 2000)
                            {
                                for (int i = 0; i <= result - 1000; ++i)
                                    safeThis->undoOrRedo (false);
                            }
                            else
                            {
                                for (int i = 0; i <= result - 2000; ++i)
                                    safeThis->undoOrRedo (true);
                            }

                            safeThis->updateHeaderButtons();
                        });
}

void IlanaSynthAudioProcessorEditor::updateHeaderButtons()
{
    const auto nameChanged = processorRef.getCurrentPresetName() != shownPresetName;
    shownPresetName = processorRef.getCurrentPresetName();

    const auto names = processorRef.getAllPresetNames();
    currentPresetIndex = names.indexOf (shownPresetName);
    shownCategory = juce::isPositiveAndBelow (currentPresetIndex, names.size())
                        ? processorRef.getAllPresetCategories()[currentPresetIndex]
                        : juce::String();

    // A DX7 voice shows its name in Title Case and its bank by the category
    // ("KEYS  ·  DX7 ROM1A"); the saved name is unchanged.
    const auto bank = juce::isPositiveAndBelow (currentPresetIndex, names.size()) ? processorRef.getAllPresetBanks()[currentPresetIndex]
                                                                                   : juce::String();
    presetDisplayName = bank.isNotEmpty() ? Presets::dx7DisplayName (shownPresetName) : shownPresetName;

    if (bank.isNotEmpty())
        shownCategory << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  DX7 ")) << bank;

    if (nameChanged)
        adoptLoadedFingerprint();

    presetDisplay.setPreset (presetDisplayName, shownCategory, isFavourite (shownPresetName), isPatchEdited());
    favButton.setToggleState (isFavourite (shownPresetName), juce::dontSendNotification);
    favButton.setIconColour (isFavourite (shownPresetName) ? std::optional<juce::Colour> (juce::Colour (0xffffd447))
                                                           : std::nullopt);
    abButton.setButtonText (showingA ? "A" : "B");
    abButton.setToggleState (! showingA, juce::dontSendNotification);

    for (auto& knob : macroKnobs)
        knob->refreshName();
}

void IlanaSynthAudioProcessorEditor::updateUndoButtons()
{
    auto& undoManager = processorRef.getUndoManager();
    undoButton.setEnabled (undoManager.canUndo());
    redoButton.setEnabled (undoManager.canRedo());
    historyButton.setEnabled (undoManager.canUndo() || undoManager.canRedo());
}

bool IlanaSynthAudioProcessorEditor::isFavourite (const juce::String& presetName) const
{
    return settings != nullptr && settings->getValue ("fav_" + presetName, "0") == "1";
}

void IlanaSynthAudioProcessorEditor::toggleFavourite()
{
    if (settings == nullptr)
        return;

    const auto names = processorRef.getAllPresetNames();

    if (! juce::isPositiveAndBelow (currentPresetIndex, names.size()))
        return;

    const auto name = names[currentPresetIndex];
    settings->setValue ("fav_" + name, isFavourite (name) ? "0" : "1");
    settings->saveIfNeeded();
}

void IlanaSynthAudioProcessorEditor::toggleAB()
{
    closeWavetableEditor(); // the other patch has its own tables
    closeZoneEditor();
    closeSampleBrowser();

    // The whole patch, not just the parameters: tables, samples, mod remaps,
    // the FX patch, clips, tuning and the preset name go with each slot.
    auto current = processorRef.captureWholePatch();

    if (showingA)
    {
        slotA = current;

        if (! slotB.isValid())
            slotB = slotA;

        processorRef.restoreWholePatch (slotB);
    }
    else
    {
        slotB = current;
        processorRef.restoreWholePatch (slotA);
    }

    processorRef.easeNextBlock(); // the new patch continues from the old one's last sample
    showingA = ! showingA;
    abButton.setButtonText (showingA ? "A" : "B");
    abButton.setToggleState (! showingA, juce::dontSendNotification);
    updateHeaderButtons();
}

// The A|B button's right-click (UI review 31, item 24): one side becomes a
// copy of the other, through the same whole-patch slots a flip uses. The
// side playing stays the same; when it is the one overwritten, the copy
// plays at once.
void IlanaSynthAudioProcessorEditor::copyAB (bool aToB)
{
    const auto overwritesPlaying = aToB != showingA; // copying into the side you hear

    if (overwritesPlaying)
    {
        auto& from = aToB ? slotA : slotB;

        if (! from.isValid())
            return;

        closeWavetableEditor(); // the other patch has its own tables
        closeZoneEditor();
        closeSampleBrowser();
        processorRef.restoreWholePatch (from);
        processorRef.easeNextBlock();
        (aToB ? slotB : slotA) = from;
    }
    else
    {
        (aToB ? slotB : slotA) = processorRef.captureWholePatch();
    }

    updateHeaderButtons();
    presetDisplay.showNotice (aToB ? "A COPIED TO B" : "B COPIED TO A");
}

void IlanaSynthAudioProcessorEditor::setTheme (int newThemeIndex)
{
    themeIndex = juce::jlimit (0, IlanaTheme::numPalettes - 1, newThemeIndex);
    lookAndFeel.setAccent (juce::Colour (IlanaTheme::palette[themeIndex]), juce::Colour (IlanaTheme::paletteSecond[themeIndex]));

    if (settings != nullptr)
    {
        settings->setValue ("themeIndex", themeIndex);
        settings->saveIfNeeded();
    }

    sendLookAndFeelChange();
    if (scopeWindow != nullptr)
    {
        scopeWindow->sendLookAndFeelChange();
        scopeWindow->repaint();
    }
}

// The preset's own level (output_trim, review 7): each factory preset sets
// it so that MASTER reads 0 dB, and it is saved with a patch, so two patches
// at MASTER 0 dB can sit 15 dB apart. MASTER's hover names it, and the
// preset menu resets it.
juce::String IlanaSynthAudioProcessorEditor::presetLevelText() const
{
    const auto* trim = processorRef.apvts.getRawParameterValue ("output_trim");
    const auto db = trim != nullptr ? trim->load() : 0.0f;
    return (db > 0.05f ? "+" : "") + juce::String (std::abs (db) < 0.05f ? 0.0f : db, 1) + " dB";
}

void IlanaSynthAudioProcessorEditor::updateMasterTooltip()
{
    const auto text = presetLevelText();

    if (masterKnob == nullptr || text == shownPresetLevel)
        return;

    auto& knob = masterKnob->getKnob();

    if (masterBaseTooltip.isEmpty())
        masterBaseTooltip = knob.getTooltip();

    shownPresetLevel = text;
    const auto tooltip = masterBaseTooltip + "  This preset's own level: " + text
                         + " (set when it loads, saved with it; the preset menu resets it).";
    knob.setTooltip (tooltip);

    for (auto* child : knob.getChildren())
        if (auto* slider = dynamic_cast<juce::Slider*> (child))
            slider->setTooltip (tooltip);
}

void IlanaSynthAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "New patch");
    menu.addSeparator();
    menu.addItem (2, "Save preset", true);
    menu.addItem (7, "Save preset as...");
    menu.addItem (5, "Export preset file...");
    menu.addItem (3, "Load preset file...");
    menu.addItem (4, "Open user preset folder");
    menu.addSeparator();
    menu.addItem (8, "Reset preset level  (" + presetLevelText() + ")", presetLevelText() != "0.0 dB");
    menu.addSeparator();
    menu.addItem (6, "Import DX7 / Dexed bank (.syx)...");

    menu.showMenuAsync (IlanaTheme::menuOptions (&moreButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr)
                                return;

                            switch (result)
                            {
                                case 1: safeThis->loadPresetIndex (0); break;
                                case 2: safeThis->savePreset(); break;
                                case 7: safeThis->savePresetAs(); break;
                                case 3: safeThis->loadPreset(); break;
                                case 5: safeThis->exportPreset(); break;
                                case 6: safeThis->chooseSyxFile(); break;
                                case 8: safeThis->resetPresetLevel(); break;
                                case 4:
                                {
                                    const auto directory = safeThis->processorRef.getUserPresetDirectory();
                                    directory.createDirectory();
                                    directory.startAsProcess();
                                    break;
                                }
                                default: break;
                            }
                        });
}

void IlanaSynthAudioProcessorEditor::resetPresetLevel()
{
    if (auto* trim = processorRef.apvts.getParameter ("output_trim"))
    {
        processorRef.beginEdit ("Reset preset level");
        trim->beginChangeGesture();
        trim->setValueNotifyingHost (trim->getDefaultValue());
        trim->endChangeGesture();
        processorRef.endEdit();
        updateMasterTooltip();
    }
}

void IlanaSynthAudioProcessorEditor::showDiceMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("RANDOMISE");
    menu.addItem (1, "Whole patch");
    menu.addItem (2, "Oscillators only");
    menu.addItem (3, "Filters only");
    menu.addItem (4, "Envelopes only");
    menu.addItem (5, "Modulation only");
    menu.addItem (6, "Effects chain only");
    menu.addSeparator();
    menu.addSectionHeader ("MUTATE");
    menu.addItem (10, "Nudge  (small changes)");
    menu.addItem (11, "Shake  (bigger changes)");

    menu.showMenuAsync (IlanaTheme::menuOptions (&diceButton),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            // Each roll is one named undo step.
                            const juce::String names[] { {}, "Random patch", "Random oscillators", "Random filters",
                                                         "Random envelopes", "Random modulation", "Random effects" };
                            if (result == 1)
                            {
                                safeThis->randomizeWholePatch();
                                return;
                            }

                            const auto name = result <= 6 ? names[result] : juce::String (result == 10 ? "Nudge" : "Shake");
                            const auto roll = [safeThis, result, name]
                            {
                                if (safeThis == nullptr)
                                    return;

                                auto& self = *safeThis;
                                self.processorRef.performEdit (name, [&self, result]
                                {
                                    if (result == 6)
                                        self.processorRef.randomizeFxChain();
                                    else if (result == 10)
                                        self.mutate (0.06f);
                                    else if (result == 11)
                                        self.mutate (0.18f);
                                    else
                                        self.randomizeGroup (result);
                                });
                            };

                            roll();
                        });
}

// The dice's "Whole patch" (and Ctrl+R): one named undo step; a whole new
// patch replaces the edited one, so it asks first.
void IlanaSynthAudioProcessorEditor::randomizeWholePatch()
{
    confirmReplacingPatch ("A random patch", "Roll anyway",
                           [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (bool confirmed)
                           {
                               if (safeThis == nullptr || ! confirmed)
                                   return;
                               auto& self = *safeThis;
                               self.processorRef.performEdit ("Random patch", [&self] { self.randomize(); });
                           });
}

void IlanaSynthAudioProcessorEditor::showSettingsMenu (bool tuningOnly)
{
    const char* const skinNames[] { "Ember", "Ice", "Acid", "Neon", "Neon to Ice" };
    const float zoomChoices[] { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

    juce::PopupMenu skins;

    for (int i = 0; i < IlanaTheme::numPalettes; ++i)
        skins.addItem (100 + i, skinNames[i], true, i == themeIndex);

    juce::PopupMenu sizes;

    for (int i = 0; i < 6; ++i)
        sizes.addItem (200 + i, juce::String (juce::roundToInt (zoomChoices[i] * 100.0f)) + "%",
                       zoomChoices[i] <= fitZoom() + 0.001f || juce::approximatelyEqual (uiZoom, zoomChoices[i]),
                       juce::approximatelyEqual (uiZoom, zoomChoices[i]));

    // Engine quality and oversampling, also on the scope panel (where they were
    // hard to find).
    const auto read = [this] (const char* id) { return processorRef.apvts.getRawParameterValue (id)->load(); };
    juce::PopupMenu quality;
    const char* const qualityNames[] { "Eco (unison capped at 4)", "Normal", "High" };
    for (int i = 0; i < 3; ++i)
        quality.addItem (600 + i, qualityNames[i], true, juce::roundToInt (read ("quality")) == i);
    juce::PopupMenu oversampling;
    const auto oversampled = read ("oversampling") > 0.5f;
    const auto factor = juce::roundToInt (read ("os_factor"));
    oversampling.addItem (700, "Off", true, ! oversampled);
    oversampling.addItem (701, "2x", true, oversampled && factor == 0);
    oversampling.addItem (702, "4x", true, oversampled && factor == 1);

    // Scala microtuning: the tick shows it is on, with the scale's name.
    juce::PopupMenu tuning;
    const auto& tuningState = processorRef.getTuningState();
    const auto hasScale = tuningState.hasScale();
    const auto tuningOn = hasScale && read ("tuning_on") > 0.5f;
    tuning.addItem (800, hasScale ? "Tuning on: " + tuningState.getDescription() : juce::String ("Tuning on (no scale loaded)"),
                    hasScale, tuningOn);
    tuning.addSeparator();
    tuning.addItem (801, "Load Scala tuning (.scl)...");
    tuning.addItem (802, "Load keyboard mapping (.kbm)...");
    tuning.addItem (803, "Reset to 12-TET", hasScale || read ("tuning_on") > 0.5f);
    tuning.addSeparator();
    // MTS-ESP: shown for information; a master in the session takes over.
    tuning.addItem (804, processorRef.isMtsEspConnected() ? "MTS-ESP: " + processorRef.getMtsEspScaleName() + " (overrides the scale)"
                                                          : juce::String ("MTS-ESP: no master in this session"),
                    false, processorRef.isMtsEspConnected());

    juce::PopupMenu menu;
    // The voice settings are in the bottom bar (VOICE MODE and the rest); the header's VOICES opens the same menu.
    menu.addItem (960, "Voice mode (bottom bar)...");
    menu.addSeparator();
    menu.addSubMenu ("Skin", skins);
    menu.addSubMenu ("Interface size", sizes);
    menu.addSubMenu ("Engine quality", quality);
    menu.addSubMenu ("Oversampling", oversampling);
    menu.addSubMenu (tuningOn ? "Tuning: " + tuningState.getDescription() : juce::String ("Tuning"), tuning, true, nullptr, tuningOn);
    menu.addItem (300, "Show keyboard", true, keyboardVisible);
    // The live waveform under the preset name moves all the time beside
    // the most-read text: it can be turned off (review 8, S8-41).
    menu.addItem (320, "Waveform under the preset name", true, headerScope.isVisible());
    // Motion: Full or Less (review 26). Less keeps the meters and the live
    // dots and drops the fades, glides, morphs, trails and pop-up growth.
    menu.addItem (330, "Full motion (animations)", true, IlanaAnim::fullMotion());
   #if ILANA_GPU_UI
    menu.addItem (310, "GPU rendering (OpenGL)", true, openGL != nullptr);
   #endif
   #if ILANA_VULKAN
    menu.addItem (340, "Vulkan rendering", true, vulkan != nullptr);
   #endif
   #if defined (_WIN32)
    menu.addItem (350, "Experimental: sharp when Live stretches the window (reopen the plugin)", true,
                  settings->getBoolValue ("sharpWhenStretched", false));
   #endif
    menu.addItem (500, "MPE mode (per-note pitch, pressure and slide)", true,
                  processorRef.apvts.getRawParameterValue ("mpe_mode")->load() > 0.5f);
    menu.addItem (510, "Multi-core voices (several threads, same sound)", true, read ("multi_core") > 0.5f);
    menu.addItem (511, "Repeatable random (Freeze and Export play what you heard)", true, read ("repeat_random") > 0.5f);
    menu.addItem (410, "Ask before replacing an edited patch", true, asksBeforeReplacingEdits());
    menu.addSeparator();
    menu.addItem (400, "Show welcome tour");
    menu.addSeparator();
    menu.addItem (990, "Build " + juce::String (ilana::buildLabel()), false); // (information only)
    menu.addItem (991, scaleReadout(), false); // (information only: what the window is scaled by, Fable S plan step 1)

    // The header's tuning indicator opens just the tuning items.
    if (tuningOnly)
    {
        menu = juce::PopupMenu();
        menu.addSectionHeader ("TUNING");
        for (juce::PopupMenu::MenuItemIterator items (tuning); items.next();)
            menu.addItem (items.getItem());
    }

    auto* target = tuningOnly ? static_cast<juce::Component*> (&presetDisplay) : &settingsButton;
    menu.showMenuAsync (IlanaTheme::menuOptions (target),
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            const float zooms[] { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

                            if (result >= 100 && result < 200)
                                safeThis->setTheme (result - 100);
                            else if (result >= 200 && result < 300)
                                safeThis->applyUiZoom (zooms[juce::jlimit (0, 5, result - 200)]);
                            else if (result == 300)
                                safeThis->setKeyboardVisible (! safeThis->keyboardVisible);
                            else if (result == 330)
                            {
                                IlanaAnim::fullMotion() = ! IlanaAnim::fullMotion();
                                safeThis->settings->setValue ("fullMotion", IlanaAnim::fullMotion());
                                safeThis->settings->saveIfNeeded();
                                safeThis->repaint();
                            }
                            else if (result == 320)
                            {
                                const auto show = ! safeThis->headerScope.isVisible();
                                safeThis->headerScope.setVisible (show);
                                safeThis->settings->setValue ("headerWaveform", show);
                                safeThis->settings->saveIfNeeded();
                            }
                           #if ILANA_GPU_UI
                            else if (result == 310)
                            {
                                const auto useGpu = safeThis->openGL == nullptr;
                               #if ILANA_VULKAN
                                // Picking OpenGL while Vulkan draws means OpenGL instead.
                                if (useGpu && safeThis->vulkan != nullptr)
                                {
                                    safeThis->setVulkanRendering (false);
                                    safeThis->settings->setValue ("vulkanRendering", false);
                                }
                               #endif
                                safeThis->setGpuRendering (useGpu);
                                safeThis->settings->setValue ("gpuRendering", useGpu);
                                safeThis->settings->saveIfNeeded();
                            }
                           #endif
                           #if ILANA_VULKAN
                            else if (result == 340)
                            {
                                const auto useVulkan = safeThis->vulkan == nullptr;
                                safeThis->setVulkanRendering (useVulkan);
                                safeThis->settings->setValue ("vulkanRendering", useVulkan);
                                safeThis->settings->saveIfNeeded();
                            }
                           #endif
                            else if (result == 350)
                            {
                                safeThis->settings->setValue ("sharpWhenStretched", ! safeThis->settings->getBoolValue ("sharpWhenStretched", false));
                                safeThis->settings->saveIfNeeded();
                            }
                            else if (result == 510)
                            {
                                if (auto* multiCore = safeThis->processorRef.apvts.getParameter ("multi_core"))
                                {
                                    multiCore->beginChangeGesture();
                                    multiCore->setValueNotifyingHost (multiCore->getValue() > 0.5f ? 0.0f : 1.0f);
                                    multiCore->endChangeGesture();
                                }
                            }
                            else if (result == 511)
                            {
                                if (auto* repeatable = safeThis->processorRef.apvts.getParameter ("repeat_random"))
                                {
                                    repeatable->beginChangeGesture();
                                    repeatable->setValueNotifyingHost (repeatable->getValue() > 0.5f ? 0.0f : 1.0f);
                                    repeatable->endChangeGesture();
                                }
                            }
                            else if (result == 500)
                            {
                                if (auto* mpe = safeThis->processorRef.apvts.getParameter ("mpe_mode"))
                                {
                                    mpe->beginChangeGesture();
                                    mpe->setValueNotifyingHost (mpe->getValue() > 0.5f ? 0.0f : 1.0f);
                                    mpe->endChangeGesture();
                                }
                            }
                            else if (result >= 600 && result < 800)
                            {
                                const auto set = [&safeThis] (const char* id, float value)
                                {
                                    if (auto* parameter = safeThis->processorRef.apvts.getParameter (id))
                                    {
                                        parameter->beginChangeGesture();
                                        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                                        parameter->endChangeGesture();
                                    }
                                };
                                if (result < 700)
                                    set ("quality", (float) (result - 600));
                                else
                                {
                                    set ("oversampling", result == 700 ? 0.0f : 1.0f);
                                    if (result > 700)
                                        set ("os_factor", (float) (result - 701));
                                }
                            }
                            else if (result == 960)
                                safeThis->showVoicePanel();
                            else if (result == 400)
                            {
                                safeThis->tutorial.setVisible (true);
                                safeThis->tutorial.toFront (false);
                            }
                            else if (result == 410)
                            {
                                safeThis->setAsksBeforeReplacingEdits (! safeThis->asksBeforeReplacingEdits());
                            }
                            else if (result == 800)
                            {
                                if (auto* on = safeThis->processorRef.apvts.getParameter ("tuning_on"))
                                {
                                    on->beginChangeGesture();
                                    on->setValueNotifyingHost (on->getValue() > 0.5f ? 0.0f : 1.0f);
                                    on->endChangeGesture();
                                }
                            }
                            else if (result == 801 || result == 802)
                            {
                                const auto mapping = result == 802;
                                safeThis->fileChooser = std::make_unique<juce::FileChooser> (
                                    mapping ? "Load keyboard mapping" : "Load Scala tuning",
                                    juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                    mapping ? "*.kbm" : "*.scl");
                                safeThis->fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                                                        | juce::FileBrowserComponent::canSelectFiles,
                                                                    [safeThis, mapping] (const juce::FileChooser& chooser)
                                                                    {
                                                                        const auto file = chooser.getResult();
                                                                        if (safeThis == nullptr || ! file.existsAsFile())
                                                                            return;
                                                                        juce::String error;
                                                                        const auto text = file.loadFileAsString();
                                                                        const auto loaded = mapping ? safeThis->processorRef.loadTuningMapping (text, error)
                                                                                                    : safeThis->processorRef.loadTuningScale (text, error);
                                                                        if (! loaded)
                                                                            ConfirmOverlay::tell (safeThis.getComponent(), "Could not load " + file.getFileName(), error);
                                                                    });
                            }
                            else if (result == 803)
                                safeThis->processorRef.resetTuning();
                        });
}

void IlanaSynthAudioProcessorEditor::stepPreset (int delta)
{
    createPresetPanel();

    if (const auto next = presetPanel->presetBeside (currentPresetIndex, delta); next >= 0)
    {
        loadPresetIndex (next);
        return;
    }

    // Nothing listed (a search that matches nothing): every preset, in order.
    if (const auto count = processorRef.getNumAllPresets(); count > 0)
        loadPresetIndex ((juce::jmax (0, currentPresetIndex) + count + delta) % count);
}

void IlanaSynthAudioProcessorEditor::createPresetPanel()
{
    if (presetPanel != nullptr)
        return;

    presetPanel = std::make_unique<PresetPanel> (processorRef, settings.get());
    presetPanel->onLoad = [this] (int index, bool closeAfter)
    {
        loadPresetIndex (index, [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), closeAfter] (bool loaded)
        {
            if (safeThis == nullptr || safeThis->presetPanel == nullptr)
                return;

            if (! loaded)
                safeThis->presetPanel->selectLoadedPreset(); // back to the patch still playing
            else if (closeAfter)
                safeThis->presetPanel->close();
        });
    };
    presetPanel->onFavouriteChanged = [this] { updateHeaderButtons(); };
    presetPanel->onSaveAs = [this] { savePresetAs(); };
    // Rename or edit info: the preset loads (asking over an edited patch),
    // then the save sheet opens on its own file.
    presetPanel->onEditInfo = [this] (int index)
    {
        loadPresetIndex (index, [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (bool loaded)
        {
            if (safeThis == nullptr || ! loaded)
                return;

            const auto own = PresetFiles::loadedUserPreset (safeThis->processorRef);

            if (! own.existsAsFile())
                return;

            if (safeThis->presetPanel != nullptr && safeThis->presetPanel->isOpen() && ! safeThis->presetPanel->isDocked())
                safeThis->presetPanel->close();

            safeThis->saveOverlay.setBounds (safeThis->content.getLocalBounds());
            safeThis->saveOverlay.showForEdit (own);
        });
    };
    presetPanel->onImportSyx = [this] { chooseSyxFile(); };
    presetPanel->setAnchor (&presetDisplay);
    presetPanel->onDockRequest = [this] (bool dock)
    {
        presetDocked = dock;

        if (settings != nullptr)
        {
            settings->setValue ("presetBrowserDocked", presetDocked);
            settings->saveIfNeeded();
        }

        if (dock)
        {
            if (presetDockShown)
            {
                presetPanel->setDocked (true);
                presetPanel->open();
                resized();
            }
            else
            {
                presetPanel->setVisible (false);
                setPresetDockShown (true);
            }
        }
        else
        {
            setPresetDockShown (false);
            togglePresetPanel();
        }
    };
    presetPanel->onDockedClose = [this] { setPresetDockShown (false); };
    presetPanel->onClosed = [this] { browsingAccepted = false; };

    // Hidden until open() fades it in.
    content.addChildComponent (*presetPanel);
}

// Docked, the browser covers the page area (the tab row, the chips, the
// macros and the keyboard stay): the window keeps its size (review 6; it
// used to grow by a column).
void IlanaSynthAudioProcessorEditor::setPresetDockShown (bool shown)
{
    createPresetPanel();

    if (shown == presetDockShown)
        return;

    presetDockShown = shown;
    browsingAccepted = false;
    presetPanel->setVisible (false);
    presetPanel->setDocked (shown);

    if (shown)
    {
        presetPanel->setBounds (pageArea().reduced (2, 0).withTrimmedBottom (2));
        presetPanel->open();
    }

    if (settings != nullptr)
    {
        settings->setValue ("presetBrowserDockOpen", presetDockShown);
        settings->saveIfNeeded();
    }

    resized();
}

void IlanaSynthAudioProcessorEditor::togglePresetPanel()
{
    createPresetPanel();

    if (presetDocked)
    {
        setPresetDockShown (! presetDockShown);
        return;
    }

    // Drop down under the preset name, kept inside the window.
    {
        // Wide enough for the details card beside the list (S10-5).
        const auto width = 920;
        const auto x = juce::jlimit (10, designWidth - 10 - width, presetDisplay.getX() - 40);
        presetPanel->setBounds (x, presetDisplay.getBottom() + 6, width, 540);
        presetPanel->setScrimArea (content.getLocalBounds());
    }

    if (presetPanel->isOpen())
        presetPanel->close();
    else
        presetPanel->open();
}

// IMPORT .SYX (the browser) and the preset menu's import: each voice of the
// bank becomes a user preset under DX7/<bank>/.
void IlanaSynthAudioProcessorEditor::chooseSyxFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Import a DX7 or Dexed bank",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.syx");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this)] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (safeThis != nullptr && file.existsAsFile())
                                      safeThis->importSyxFile (file);
                              });
}

// Imports, then says so in the theme; "Show in browser" opens the browser
// on the new bank.
void IlanaSynthAudioProcessorEditor::importSyxFile (const juce::File& file)
{
    juce::String message;
    const auto count = processorRef.importDx7File (file, message);
    const auto bank = file.getFileNameWithoutExtension();

    if (presetPanel != nullptr)
        presetPanel->refresh();

    ConfirmOverlay::Choices choices;
    choices.confirmText = count > 0 ? "Show in browser" : "OK";
    choices.cancelText = count > 0 ? "Close" : "Cancel";
    choices.offerDontAsk = false;

    confirmOverlay.ask (count > 0 ? "Imported " + bank : juce::String ("Nothing imported"),
                        count > 0 ? juce::String (count) + (count == 1 ? " DX7 voice is" : " DX7 voices are")
                                        + " now your presets, filed by sound with the bank " + bank + " under the browser's DX7 chip."
                                  : message,
                        choices,
                        [safeThis = juce::Component::SafePointer<IlanaSynthAudioProcessorEditor> (this), count, bank] (bool show, bool)
                        {
                            if (safeThis == nullptr || ! show || count == 0)
                                return;

                            safeThis->createPresetPanel();

                            if (! safeThis->presetPanel->isOpen())
                                safeThis->togglePresetPanel();

                            safeThis->presetPanel->showDx7Bank (bank);
                        });
}
