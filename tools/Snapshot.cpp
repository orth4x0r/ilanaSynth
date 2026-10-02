// Dev tool: renders the plugin editor offscreen and writes one PNG per tab,
// so the UI can be reviewed without a DAW or a display session.
//   ilanaSnapshot <output dir> [factory preset index]
//   ilanaSnapshot --uitest     (drives the editor and checks the wiring)
//   ilanaSnapshot --idlecpu    (UI cost of each page while nothing moves)

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <iostream>
#include <functional>
#include <typeinfo>
#include <map>
#include <set>
#include <thread>

#include "PluginProcessor.h"
#include "gui/HeaderWidgets.h"
#include "gui/CardTabs.h"
#include "gui/FilterWidgets.h"
#include "gui/VectorPad.h"
#include "gui/PhysicalView.h"
#include "gui/EnvThumbs.h"
#include "gui/TableBrowser.h"
#include "gui/WavetableEditor.h"
#include "PluginEditor.h"
#include "gui/EnvelopeDisplay.h"
#include "gui/FmWidgets.h"
#include "gui/LfoThumbs.h"
#include "gui/MatrixWidgets.h"
#include "gui/FilterDisplay.h"
#include "gui/OutputView.h"
#include "gui/ParamControls.h"
#include "gui/SubTabBar.h"
#include "gui/TutorialOverlay.h"
#include "gui/WaveDisplay.h"
#include "gui/ModHoverPopup.h"
#include "gui/ClipEditor.h"
#include "gui/ConfirmOverlay.h"
#include "gui/LfoDisplay.h"
#include "gui/PoolIndexRow.h"
#include "gui/RemapEditor.h"
#include "gui/FxDisplays.h"

namespace
{
template <typename Type>
void findAll (juce::Component& parent, std::vector<Type*>& found)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* match = dynamic_cast<Type*> (child))
            found.push_back (match);

        findAll<Type> (*child, found);
    }
}

template <typename Type>
Type* findChild (juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* match = dynamic_cast<Type*> (child))
            return match;

        if (auto* nested = findChild<Type> (*child))
            return nested;
    }

    return nullptr;
}

void settle (int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}

// Snapshot mode plays a held note so the live views (scope, modulation
// markers) have something to show; this runs the audio before each capture.
std::function<void()>& beforeSave()
{
    static std::function<void()> callback;
    return callback;
}

void save (juce::Component& editor, const juce::File& file)
{
    if (beforeSave() != nullptr)
        beforeSave()();

    const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.5f);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::cout << file.getFullPathName() << std::endl;
}
int uiFailures = 0;

// isShowing() needs a window on screen; the editor here is offscreen, so
// walk the visibility chain instead.
bool visibleInTree (const juce::Component* component)
{
    // The editor itself is never put on screen here, so stop below it.
    for (; component != nullptr && component->getParentComponent() != nullptr; component = component->getParentComponent())
        if (! component->isVisible())
            return false;

    return true;
}

void expect (bool condition, const juce::String& message)
{
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << std::endl;

    if (! condition)
        ++uiFailures;
}

// Drives the real editor the way a user would and checks the processor
// state, for the wiring that unit tests can't see.
int runUiTests()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto names = processor.getFactoryPresetNames();
    const auto neuroWobble = names.indexOf ("Neuro Wobble");
    processor.loadFactoryPreset (neuroWobble);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
    settle (400);

    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);

    auto* pages = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
    expect (pages != nullptr && pages->getPageIds()[0] == "MAIN", "MAIN is the first page");

    // Loads don't ask over edited patches except in the test that checks it
    // (the user's own choice is put back at the end).
    const auto askedBefore = pages->asksBeforeReplacingEdits();
    pages->setAsksBeforeReplacingEdits (false);

    // Undo checks start from an empty history, with every parameter change
    // already in the tree (it otherwise catches up on a timer).
    const auto clearHistory = [&processor]
    {
        processor.apvts.copyState();
        processor.getUndoManager().clearUndoHistory();
    };
    const auto undoSteps = [&processor]
    {
        processor.apvts.copyState();
        return processor.getUndoManager().getUndoDescriptions();
    };
    expect (findChild<juce::TabbedComponent> (*editor) != nullptr
                && findChild<juce::TabbedComponent> (*editor)->getNumTabs() == (IlanaSynthAudioProcessor::isEffectBuild ? 8 : 7),
            "seven tabs");

    // An oscillator card's switch on the page shown (its tooltip names the
    // parameter), and a click on that card's title line: a card folded to
    // fit opens (UI review 4, V13).
    const auto toggleFor = [&processor, &editor] (const juce::String& id) -> ToggleControl*
    {
        auto* parameter = processor.apvts.getParameter (id);
        std::vector<ToggleControl*> toggles;
        findAll<ToggleControl> (*editor, toggles);
        for (auto* toggle : toggles)
            if (parameter != nullptr && visibleInTree (toggle) && toggle->getWidth() > 0
                && toggle->getButton().getTooltip().startsWith (parameter->getName (64)))
                return toggle;
        return nullptr;
    };
    const auto clickCardTitle = [&toggleFor] (const juce::String& onId, bool rightClick = false)
    {
        auto* toggle = toggleFor (onId);
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
    };

    // Pages inside a tab: MATRIX is under MOD, and the scope opens over any page.
    pages->showPage ("MATRIX");
    settle (200);
    expect (pages->getCurrentPageId() == "MATRIX" && pages->getCurrentPage() != nullptr && visibleInTree (pages->getCurrentPage()),
            "MATRIX opens inside MOD");
    pages->showPage ("STEPS");
    settle (200);
    expect (pages->getCurrentPageId() == "STEPS" && visibleInTree (pages->getCurrentPage()), "STEPS & MSEG opens inside MOD");
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
        expect (shownSwitchers == 1, "MOD shows its page switch (" + where + ")");
    }
    pages->setScopeOpen (true);
    settle (250);
    expect (pages->isScopeOpen(), "the scope panel opens");
    pages->setScopeOpen (false);
    expect (! pages->isScopeOpen(), "the scope panel closes");

    // KEYS opens the keyboard and gives the page less room; COMPARE flips
    // to B and lights; the page switch changes the page.
    {
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*editor, buttons);
        juce::TextButton* keys = nullptr;
        juce::TextButton* compare = nullptr;
        for (auto* b : buttons)
        {
            keys = b->getButtonText() == "KEYS" ? b : keys;
            compare = b->getButtonText().startsWith ("A/B") ? b : compare;
        }

        auto* tabsComponent = findChild<juce::TabbedComponent> (*editor);
        auto* keyboard = findChild<KeyboardStrip> (*editor);
        expect (keys != nullptr && keyboard != nullptr && ! visibleInTree (keyboard), "the keyboard starts hidden");

        if (keys != nullptr && keyboard != nullptr && tabsComponent != nullptr)
        {
            const auto pageHeight = tabsComponent->getHeight();
            keys->triggerClick();
            settle (150);
            expect (visibleInTree (keyboard) && tabsComponent->getHeight() < pageHeight, "KEYS shows the keyboard and the page shrinks");
            keys->triggerClick();
            settle (150);
            expect (! visibleInTree (keyboard) && tabsComponent->getHeight() == pageHeight, "KEYS hides it again");
        }

        if (compare != nullptr)
        {
            compare->triggerClick();
            settle (150);
            expect (compare->getButtonText() == "A/B:  B" && compare->getToggleState(), "COMPARE flips to B and lights");
            compare->triggerClick();
            settle (150);
            expect (compare->getButtonText() == "A/B:  A" && ! compare->getToggleState(), "COMPARE flips back to A");
        }

        pages->showPage ("ENV/LFO");
        settle (150);
        std::vector<SectionSwitcher*> switchers;
        findAll<SectionSwitcher> (*editor, switchers);
        SectionSwitcher* shown = nullptr;
        for (auto* sw : switchers)
            shown = visibleInTree (sw) ? sw : shown;
        if (shown != nullptr && shown->onSelect != nullptr)
        {
            shown->setSelected (2, true);
            shown->onSelect (2);
            settle (200);
        }
        expect (shown != nullptr && pages->getCurrentPageId() == "MATRIX", "the MOD switch opens MATRIX");
    }

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
            showsCutoff = showsCutoff || combo->getText() == "Filter1 Cutoff";

        expect (showsCutoff, "matrix row shows the preset's destination (Filter1 Cutoff)");

        for (auto* combo : combos)
        {
            if (combo->getText() == "Filter1 Cutoff")
            {
                combo->setSelectedId ((int) Mod::Destination::Osc2Warp + 1, juce::sendNotificationSync);
                settle (50);
                expect (processor.readModSlot (0).destination == (int) Mod::Destination::Osc2Warp,
                        "picking a grouped destination writes the right index");
            }
        }
    }

    // Dropping a source on a knob routes it, and the knob grows a dot.
    pages->showPage ("FILTER");
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

        std::vector<ModDotStrip*> strips;
        findAll<ModDotStrip> (*reso, strips);

        if (! strips.empty() && strips[0]->onDepthChange != nullptr && routed >= 0)
        {
            strips[0]->onDepthChange (routed, -0.6f);
            expect (std::abs (processor.readModSlot (routed).depth + 0.6f) < 1.0e-3f, "dragging the dot sets the depth");

            // UI review 4 (V2): a double-click zeroes the depth (as knobs and
            // the source card do), keeps the routing, is one named undo
            // step, and undo brings the depth back.
            auto& strip = static_cast<juce::Component&> (*strips[0]);
            const juce::Point<float> at ((float) strip.getWidth() * 0.5f, (float) ModDotStrip::dotPitch * 0.5f);
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

    // MAIN's cards: select in place, open the full page on request.
    pages->showPage ("MAIN");
    settle (300);

    if (auto* page = pages->getCurrentPage())
    {
        if (auto* thumbs = findChild<LfoThumbBar> (*page); thumbs != nullptr && thumbs->onSelect != nullptr)
        {
            // A press that moves away (a drag to a knob) must not switch pages.
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            const auto now = juce::Time::getCurrentTime();
            const juce::Point<float> from (20.0f, (float) thumbs->getHeight() * 0.5f);
            const juce::Point<float> to (from.x + 60.0f, from.y + 40.0f);
            const auto make = [&] (juce::Point<float> at, bool dragged)
            {
                return juce::MouseEvent (source, at, juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         thumbs, thumbs, now, from, now, 1, dragged);
            };

            auto& component = static_cast<juce::Component&> (*thumbs);
            component.mouseDown (make (from, false));
            component.mouseUp (make (to, true));
            settle (50);
            expect (pages->getCurrentPageId() == "MAIN", "dragging an LFO card on MAIN stays on MAIN");

            // A click selects the LFO right on MAIN; the card's open button
            // jumps to the full page with that LFO.
            thumbs->onSelect (2);
            settle (100);

            std::vector<CardTabs*> cardTabs;
            findAll<CardTabs> (*page, cardTabs);
            CardTabs* lfoTabs = nullptr;

            for (auto* candidate : cardTabs)
                if (candidate->getSelected() == 2)
                    lfoTabs = candidate;

            expect (pages->getCurrentPageId() == "MAIN" && lfoTabs != nullptr,
                    "clicking an LFO card selects it on MAIN");

            if (lfoTabs != nullptr && lfoTabs->onOpen != nullptr)
            {
                lfoTabs->onOpen();
                settle (100);
                expect (pages->getCurrentPageId() == "ENV/LFO", "the LFO card's open button goes to ENV/LFO");
            }

            // The envelope tabs swap MAIN's envelope controls (AMP -> MOD).
            pages->showPage ("MAIN");
            settle (100);

            for (auto* candidate : cardTabs)
            {
                std::vector<KnobControl*> before;
                candidate->setSelected (3, true);
                settle (50);
                findAll<KnobControl> (*page, before);
                auto modVisible = false;

                for (auto* knob : before)
                    modVisible = modVisible || (knob->getParameterId() == "me_attack" && visibleInTree (knob));

                if (modVisible)
                {
                    expect (true, "MAIN's envelope tabs show the MOD envelope");
                    break;
                }

                candidate->setSelected (0, true);
            }
        }
        else
        {
            expect (false, "MAIN page has LFO cards");
        }
    }

    // Legacy presets get named default macros, and the matrix shows the names.
    {
        auto showsMacroName = false;

        for (auto* row : rows)
        {
            std::vector<juce::ComboBox*> combos;
            findAll<juce::ComboBox> (*row, combos);

            for (auto* combo : combos)
                showsMacroName = showsMacroName || combo->getText() == "Macro 1 (TONE)";
        }

        expect (showsMacroName, "matrix source shows the macro's name ('Macro 1 (TONE)')");
    }

    // ENV/LFO: every envelope has a card, and picking one shows its controls.
    pages->showPage ("ENV/LFO");
    settle (300);

    if (auto* page = pages->getCurrentPage())
    {
        if (auto* envCards = findChild<EnvThumbBar> (*page); envCards != nullptr && envCards->onSelect != nullptr)
        {
            envCards->onSelect (3);
            settle (100);

            knobs.clear();
            findAll<KnobControl> (*page, knobs);
            auto modAttackShown = false, ampAttackShown = false;

            for (auto* knob : knobs)
            {
                modAttackShown = modAttackShown || (knob->getParameterId() == "me_attack" && visibleInTree (knob));
                ampAttackShown = ampAttackShown || (knob->getParameterId() == "amp_attack" && visibleInTree (knob));
            }

            expect (modAttackShown && ! ampAttackShown, "clicking the MOD envelope card shows the mod envelope");
            envCards->onSelect (0);
        }
        else
        {
            expect (false, "ENV/LFO page has envelope cards");
        }
    }

    // Envelope graph: a dragged handle lands where the mouse is.
    pages->showPage ("ENV/LFO");
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
            const auto plot = amp->getLocalBounds().toFloat().reduced (12.0f, 14.0f);
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
        }
        else
        {
            expect (false, "ENV/LFO shows the amp envelope display");
        }
    }

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

        // A switched-off oscillator folds to its title line.
        const juce::String osc3 (OscillatorIds::prefixes[2]);
        if (auto* on = processor.apvts.getParameter (osc3 + "_on"))
            on->setValueNotifyingHost (0.0f);
        settle (300);
        expect (! shownOnMain (osc3 + "_level"), "MAIN folds a switched-off OSC 3");

        if (auto* on = processor.apvts.getParameter (osc3 + "_on"))
            on->setValueNotifyingHost (1.0f);
        settle (300);

        // Three open cards may not fit with the rest: OSC 3 then folds to
        // fit, and opens on a click on its title.
        const auto osc3Folded = ! shownOnMain (osc3 + "_level") && toggleFor (osc3 + "_on") != nullptr;
        if (osc3Folded)
            clickCardTitle (osc3 + "_on");
        expect (shownOnMain (osc3 + "_level") && ! shownOnMain ("osc4_level") && toggleFor ("osc4_on") == nullptr,
                juce::String ("MAIN shows OSC 3 and hides OSC 4 by default") + (osc3Folded ? " (OSC 3 folded to fit; its title opened it)" : ""));

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
            if (! shownOnMain (prefix + "_level"))
                clickCardTitle (prefix + "_on"); // folded to fit: open it
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

        expect (reverb != nullptr, "the empty rack shows quick-add buttons");

        if (reverb != nullptr)
        {
            reverb->triggerClick();
            settle (400);
            const auto* slot1 = processor.apvts.getRawParameterValue ("fx_slot1");
            expect (slot1 != nullptr && (int) slot1->load() == 13, "quick-add REVERB puts a reverb in slot 1");
            // With an effect loaded the picks move under the rack's rows.
            const auto* page = pages->getCurrentPage();
            expect (reverb->isVisible() && page != nullptr && reverb->getX() < 330 && reverb->getY() > 100,
                    "quick-add buttons move under the rack once it has an effect");
            // The rack takes each effect once: REVERB greys out, DELAY goes into slot 2.
            expect (! reverb->isEnabled() && delay != nullptr && delay->isEnabled(),
                    "a type already in the rack is greyed out in the library");
            if (delay != nullptr)
                delay->triggerClick();
            settle (400);
            const auto* slot2 = processor.apvts.getRawParameterValue ("fx_slot2");
            expect (slot2 != nullptr && (int) slot2->load() == 9, "a second quick-add goes into slot 2");
        }
    }

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
            juce::String clipped, overlapping;
            for (size_t i = 0; i < controls.size(); ++i)
            {
                const auto bounds = controls[i]->getBounds();
                if (! viewport->getViewedComponent()->getLocalBounds().contains (bounds) || (allInView && ! view.contains (bounds)))
                    clipped << controls[i]->getName() << typeid (*controls[i]).name() << bounds.toString() << " ";
                for (size_t j = i + 1; j < controls.size(); ++j)
                    if (bounds.intersects (controls[j]->getBounds()))
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
        expect (shownDisplays() == 2, "Drive and OTT cards show a display, Vowel none");
        expect (findButtons ("S").size() == 3, "each card header has a solo button");
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
            expect (processor.apvts.getRawParameterValue ("fx_slot1_solo")->load() > 0.5f, "the header's S button solos its slot");
            soloButtons.front()->triggerClick();
            settle (200);
        }

        // The slot blend sits in the selected card, not beside CHAIN.
        {
            std::vector<juce::Slider*> sliders;
            findAll<juce::Slider> (*editor, sliders);
            auto* viewport = stackViewport();
            auto inCard = false;
            for (auto* slider : sliders)
                if (slider->getTooltip().startsWith ("Slot blend") && visibleInTree (slider))
                    inCard = viewport != nullptr && slider->getParentComponent() == viewport->getViewedComponent();
            expect (inCard, "SLOT BLEND is in the selected card's header");
        }

        // One MIX: the Airwindows algorithms' own Dry/Wet is hidden.
        loadFx ({ 33 }); // AW Saturation; its first effect, Density3, has Dry/Wet as knob 4
        processor.apvts.getParameter ("fx_awsat_algo")->setValueNotifyingHost (0.0f);
        settle (300);
        expect (shownKnob ("fx_awsat_p1") && ! shownKnob ("fx_awsat_p4") && shownKnob ("fx_awsat_mix"),
                "AW Saturation shows DENSITY and MIX but not the algorithm's Dry/Wet");
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
        if (auto dice = findButtons ("DICE FX"); ! dice.empty())
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
            expect (false, "the FX toolbar has a DICE FX button");

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

        processor.loadFactoryPreset (neuroWobble);
        settle (300);
    }

    // M5/M6/M6b pages.
    {
        const auto visibleKnob = [&] (const juce::String& id)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getParameterId() == id && ! knob->getBounds().isEmpty())
                    return true;
            return false;
        };
        const auto set = [&processor] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        // FM: clicking an algorithm routes the operators.
        pages->showPage ("FM");
        settle (300);

        if (auto* strip = findChild<FmAlgorithmStrip> (*editor))
        {
            // An earlier edit in its own undo step, so undoing the algorithm
            // must leave it alone.
            processor.getUndoManager().beginNewTransaction();
            set ("osc2_level", 0.33f);
            settle (200);

            auto source = juce::Desktop::getInstance().getMainMouseSource();
            const auto at = strip->getCellCentre (10).toFloat();
            const auto now = juce::Time::getCurrentTime();
            const juce::MouseEvent click (source, at, juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                          strip, strip, now, at, now, 1, false);
            static_cast<juce::Component&> (*strip).mouseDown (click);
            static_cast<juce::Component&> (*strip).mouseUp (click);
            settle (400);
            expect (processor.findMatchingFmAlgorithm() == 10 && processor.isOscillatorShown (5),
                    "clicking algorithm 11 (DX 5 Keys) routes six operators");
            expect (visibleKnob ("fm_6to5") && visibleKnob ("fm_noise6"),
                    "the FM matrix grows to six operators, with the noise row");

            processor.getUndoManager().undo();
            settle (300);
            const auto afterUndo = processor.findMatchingFmAlgorithm();
            const auto level = processor.apvts.getRawParameterValue ("osc2_level")->load();
            processor.getUndoManager().redo();
            settle (300);
            expect (afterUndo != 10 && std::abs (level - 0.33f) < 0.01f && processor.findMatchingFmAlgorithm() == 10,
                    "undo reverts an algorithm as one step and keeps the edit before it (level "
                        + juce::String (level, 2) + ")");
        }
        else
        {
            expect (false, "the FM page has the algorithm strip");
        }

        // The operator panel follows the selected operator and its tuning.
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*editor, buttons);
        for (auto* button : buttons)
            if (button->getButtonText() == "OSC 2" && visibleInTree (button))
                button->triggerClick();
        set ("osc2_tune", (float) OscTuning::Ratio);
        settle (400);
        expect (visibleKnob ("osc2_ratio") && ! visibleKnob ("osc1_ratio") && ! visibleKnob ("osc2_fixed_hz"),
                "selecting OSC 2 in Ratio tuning shows its RATIO knob");
        set ("osc2_tune", (float) OscTuning::Fixed);
        settle (400);
        expect (visibleKnob ("osc2_fixed_hz") && ! visibleKnob ("osc2_ratio"), "Fixed tuning swaps RATIO for FIXED");

        // OSC: picking a warp opens the PD chain row.
        pages->showPage ("OSC");
        settle (300);
        set ("osc1_mode", 0.0f);
        set ("osc1_warp", 0.0f);
        set ("osc1_warp2", 0.0f);
        settle (300);
        const auto hiddenBefore = ! visibleKnob ("osc1_warp2_amt");
        set ("osc1_warp", (float) Warp::PdSaw);
        settle (300);
        expect (hiddenBefore && visibleKnob ("osc1_warp2_amt") && visibleKnob ("osc1_pd_env_amt"),
                "a warp on OSC 1 opens its PD chain row (second stage and warp envelope)");

        // M7.3: Tine and Reed swap the string controls for the pickup.
        set ("osc1_mode", 1.0f);
        set ("osc1_excite", 7.0f);
        settle (300);
        expect (visibleKnob ("osc1_ep_distance") && visibleKnob ("osc1_ep_position") && visibleKnob ("osc1_hammer_hard")
                    && ! visibleKnob ("osc1_string_stiffness") && ! visibleKnob ("osc1_string_sustain"),
                "Tine shows DISTANCE, OFFSET and HAMMER instead of the string controls");
        set ("osc1_excite", 0.0f);
        settle (300);
        expect (! visibleKnob ("osc1_ep_distance") && visibleKnob ("osc1_string_stiffness"),
                "a plucked string hides the pickup controls again");
        set ("osc1_mode", 0.0f);
        settle (300);

        // M7.4: EDIT copies a factory table into a free patch table and
        // opens the editor on it; edits reach the table the oscillator plays.
        {
            const auto factoryCount = TableFactory::getNumFactoryTables();
            set ("osc1_table", 3.0f);
            settle (200);
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*editor, buttons);
            juce::TextButton* editButton = nullptr;
            for (auto* button : buttons)
                if (button->getButtonText() == "EDIT" && visibleInTree (button) && editButton == nullptr)
                    editButton = button;
            expect (editButton != nullptr, "the OSC page has an EDIT button for the wavetable");
            if (editButton != nullptr)
            {
                editButton->triggerClick();
                settle (300);
                auto* host = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
                auto* tableEditor = host != nullptr ? host->getWavetableEditor() : nullptr;
                const auto choice = juce::roundToInt (processor.apvts.getRawParameterValue ("osc1_table")->load());
                expect (tableEditor != nullptr && tableEditor->isVisible() && choice >= factoryCount
                            && processor.isUserSlotEdited (choice - factoryCount),
                        "EDIT on a factory table opens the editor on a patch table the oscillator now plays");
                if (tableEditor != nullptr)
                {
                    const auto slot = tableEditor->getSlot();
                    const auto before = processor.getUserTableDoc (slot).frames[0];
                    tableEditor->selectFrame (0);
                    tableEditor->drawLine (0.0f, 1.0f, 0.5f, -1.0f);
                    auto doc = processor.getUserTableDoc (slot);
                    expect (doc.recipes[0].kind == FrameRecipe::Kind::Draw && doc.frames[0] != before,
                            "drawing in the editor changes the table's first frame");
                    tableEditor->setHarmonic (3, 0.8f);
                    doc = processor.getUserTableDoc (slot);
                    expect (doc.recipes[0].kind == FrameRecipe::Kind::Harmonics && doc.recipes[0].magnitudes.size() >= 3
                                && std::abs (doc.recipes[0].magnitudes[2] - 0.8f) < 1.0e-6f,
                            "a SPECTRUM bar sets that harmonic");
                    expect (tableEditor->applyFormulaText ("sin(2*pi*x) * (1 - f)", true)
                                && processor.getUserTableDoc (slot).recipes.back().kind == FrameRecipe::Kind::Formula,
                            "a formula applies to every frame");
                    expect (! tableEditor->applyFormulaText ("sin(", false), "a broken formula is refused");
                    tableEditor->morphTable (16, true);
                    expect (processor.getUserTableDoc (slot).getNumFrames() == 16
                                && processor.getWavetable (factoryCount + slot)->getNumFrames() == 16,
                            "MORPH rebuilds the table at 16 frames and the oscillator plays it");
                    tableEditor->applyShape (1);
                    save (*tableEditor, juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-wt-editor.png"));
                    host->closeWavetableEditor();
                    settle (100);
                    expect (host->getWavetableEditor() == nullptr, "CLOSE takes the editor away");
                }
            }
            set ("osc1_table", 7.0f);
            settle (200);
        }

        // ENV: DAHDSR and key-rate knobs.
        pages->showPage ("ENV/LFO");
        settle (300);
        expect (visibleKnob ("amp_delay") && visibleKnob ("amp_hold") && visibleKnob ("amp_keyrate"),
                "the amp envelope shows DELAY, HOLD and KEY RATE");

        // The destination menu files the M5/M6 targets with their oscillator
        // and under FM, not under Physical & Keys.
        {
            juce::ComboBox combo;
            MatrixMenus::fillDestinations (combo);
            const auto submenuHolding = [&combo] (const juce::String& itemText)
            {
                for (juce::PopupMenu::MenuItemIterator top (*combo.getRootMenu()); top.next();)
                    if (auto* sub = top.getItem().subMenu.get())
                        for (juce::PopupMenu::MenuItemIterator inner (*sub); inner.next();)
                            if (inner.getItem().text == itemText)
                                return top.getItem().text;
                return juce::String();
            };
            expect (submenuHolding ("Osc2 Warp 2") == "Oscillator 2" && submenuHolding ("FM Noise > Osc3") == "FM"
                        && submenuHolding ("FM Osc4 > Osc1") == "FM" && submenuHolding ("Osc1 Hammer") == "Physical & Keys",
                    "the destination menu files new targets by oscillator and FM ("
                        + submenuHolding ("Osc2 Warp 2") + ", " + submenuHolding ("FM Noise > Osc3") + ")");
        }

        // MATRIX: a routing in slot 60 shows up as a row.
        set ("mod60_src", (float) Mod::Source::Lfo3);
        set ("mod60_dst", (float) Mod::Destination::Filter1Cutoff);
        set ("mod60_amt", 0.3f);
        pages->showPage ("MATRIX");
        settle (400);
        std::vector<MatrixRow*> matrixRows;
        findAll<MatrixRow> (*editor, matrixRows);
        auto slot60 = false;
        for (auto* row : matrixRows)
            slot60 = slot60 || (row->isVisible() && row->getSlotIndex() == 59);
        expect (matrixRows.size() == (size_t) Mod::maxSlots && slot60, "the matrix page has 64 rows and shows slot 60");
    }

    // Header: next steps to the following preset and the display follows.
    std::vector<IconButton*> buttons;
    findAll<IconButton> (*editor, buttons);

    for (auto* button : buttons)
    {
        if (button->getName() == "next")
        {
            button->triggerClick();
            settle (400);
            expect (processor.getCurrentPresetName() == names[neuroWobble + 1],
                    "next-preset button loads the following preset ('" + processor.getCurrentPresetName() + "')");
        }
    }

    // UI review 4 (V1, S1): drawn data (LFO curves, remaps, clips) is undone
    // one gesture at a time, EDITED sees it, and a load over an edited patch
    // asks once.
    {
        const auto event = [] (juce::Component& component, juce::Point<float> position, bool dragged)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys(),
                                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(),
                                     position, juce::Time::getCurrentTime(), 1, dragged);
        };
        const auto gesture = [&event] (juce::Component& component, juce::Point<float> from, juce::Point<float> to)
        {
            component.mouseDown (event (component, from, false));
            component.mouseDrag (event (component, to, true));
            component.mouseUp (event (component, to, true));
        };
        const auto sameCurve = [] (const LfoCurve& a, const LfoCurve& b) { return a.toString() == b.toString(); };

        // An LFO curve stroke.
        {
            if (auto* shape = processor.apvts.getParameter ("lfo1_shape"))
                shape->setValueNotifyingHost (shape->convertTo0to1 ((float) IlanaSynthAudioProcessor::curveShape));

            LfoDisplay display (processor, 0);
            display.setSize (300, 150);
            const auto before = processor.getLfoCurve (0);
            clearHistory();
            gesture (display, { 100.0f, 50.0f }, { 120.0f, 40.0f });
            settle (50);
            const auto drawn = processor.getLfoCurve (0);
            const auto steps = undoSteps();
            processor.getUndoManager().undo();
            const auto undone = processor.getLfoCurve (0);
            processor.getUndoManager().redo();
            expect (! sameCurve (before, drawn) && steps.size() == 1 && steps[0] == "LFO 1 curve" && sameCurve (undone, before)
                        && sameCurve (processor.getLfoCurve (0), drawn),
                    "an LFO curve stroke is one undo step ('" + steps.joinIntoString ("', '") + "'); undo and redo restore the curve");
        }

        // A remap curve drag.
        {
            RemapEditor remap (processor, 5, juce::Colours::orange);
            clearHistory();
            gesture (remap, { 150.0f, 60.0f }, { 160.0f, 50.0f });
            const auto drawn = processor.isModRemapOn (5);
            const auto steps = undoSteps();
            processor.getUndoManager().undo();
            expect (drawn && steps.size() == 1 && steps[0] == "Remap curve 6" && ! processor.isModRemapOn (5),
                    "a remap curve drag is one undo step and undo straightens it again");
        }

        // A clip note: one step, and EDITED follows it (no parameter changes).
        auto* confirm = findChild<ConfirmOverlay> (*editor);
        IconButton* nextButton = nullptr;
        for (auto* button : buttons)
            if (button->getName() == "next")
                nextButton = button;

        if (confirm != nullptr && nextButton != nullptr)
        {
            // Start from a freshly loaded preset.
            nextButton->triggerClick();
            settle (300);
            const auto loadedName = processor.getCurrentPresetName();
            expect (! pages->isPatchEdited() && ! processor.getClipState().hasAny(), "a freshly loaded preset is not EDITED and has no clips");

            ClipEditor clips (processor, juce::Colours::orange);
            clips.setSize (400, 200);
            clearHistory();
            gesture (clips, { 80.0f, 100.0f }, { 80.0f, 100.0f });
            settle (50);
            const auto steps = undoSteps();
            const auto edited = pages->isPatchEdited();
            processor.getUndoManager().undo();
            const auto undone = ! processor.getClipState().hasAny() && ! pages->isPatchEdited();
            processor.getUndoManager().redo();
            expect (steps.size() == 1 && steps[0] == "Add clip note" && undone && processor.getClipState().hasAny(),
                    "adding a clip note is one undo step ('" + steps.joinIntoString ("', '") + "'); undo and redo bring it back");
            expect (edited && pages->isPatchEdited(), "a clip edit lights EDITED, and undoing it clears it");

            // A load over the edit asks; Cancel keeps the patch.
            pages->setAsksBeforeReplacingEdits (true);
            nextButton->triggerClick();
            settle (100);
            expect (confirm->isAsking() && processor.getCurrentPresetName() == loadedName,
                    "the next-preset button asks before replacing an edited patch");
            confirm->finish (false);
            settle (100);
            expect (processor.getCurrentPresetName() == loadedName && pages->isPatchEdited() && processor.getClipState().hasAny(),
                    "Cancel keeps the edited patch");

            // Load anyway loads it, and EDITED clears.
            nextButton->triggerClick();
            settle (100);
            confirm->finish (true);
            settle (300);
            const auto landed = processor.getCurrentPresetName();
            expect (landed != loadedName && ! pages->isPatchEdited() && ! processor.getClipState().hasAny(),
                    "Load anyway loads the next preset and EDITED clears ('" + landed + "')");

            // Stepping on from an unedited patch doesn't ask again.
            nextButton->triggerClick();
            settle (300);
            expect (! confirm->isAsking() && processor.getCurrentPresetName() != landed,
                    "stepping on from the loaded preset doesn't ask again");

            // Undoing the load brings the clip back with the old patch.
            processor.getUndoManager().undo();
            processor.getUndoManager().undo();
            settle (100);
            expect (processor.getClipState().hasAny(), "undoing a load restores the clips it replaced");

            pages->setAsksBeforeReplacingEdits (false);
            processor.loadFactoryPreset (0);
            settle (100);
        }
        else
        {
            expect (false, "the editor has the confirm overlay and the next button");
        }
    }

    // Init puts the modules back to three each and forgets the old patch's
    // macro CCs and drawn LFO shapes.
    {
        using M = IlanaSynthAudioProcessor::Module;
        processor.addOscillator (4);
        processor.setRevealed (M::Lfo, 8, true);
        processor.setRevealed (M::Envelope, 9, true);
        processor.setLfoCustomPoint (0, 3, 0.9f);
        processor.loadFactoryPreset (0);
        settle (100);
        expect (! processor.isOscillatorShown (4) && ! processor.isLfoShown (8)
                    && ! processor.isRevealed (M::Envelope, 9) && processor.isOscillatorShown (2),
                "Init shows three oscillators, LFOs and envelopes again");
        expect (std::abs (processor.getLfoCustomPoint (0, 3) - 0.9f) > 0.1f && processor.getMacroCc (0) == 20,
                "Init resets drawn LFO shapes and macro CCs");
    }

    // M7.0: on Init, a matrix starter adds its routing and an FX quick-add
    // fills the first slot.
    {
        processor.loadFactoryPreset (0);
        pages->showPage ("MAIN");
        settle (100);
        pages->showPage ("MATRIX");
        settle (300);

        const auto findButton = [&editor] (const juce::String& text) -> juce::TextButton*
        {
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*editor, buttons);
            for (auto* button : buttons)
                if (button->getButtonText() == text && button->isVisible())
                    return button;
            return nullptr;
        };

        auto* starter = findButton ("WHEEL  >  VIBRATO");
        expect (starter != nullptr, "an empty matrix shows the starter routings");

        if (starter != nullptr)
        {
            starter->triggerClick();
            settle (300);
            const auto slot = processor.readModSlot (0);
            expect (slot.source == Mod::Source::Lfo2 && slot.aux == Mod::Source::ModWheel
                        && slot.destination == (int) Mod::Destination::Osc1Pitch
                        && processor.readModSlot (2).destination == (int) Mod::Destination::SubPitch,
                    "WHEEL > VIBRATO routes LFO 2 via the wheel to OSC 1-3 pitch");
            expect (findButton ("WHEEL  >  VIBRATO") == nullptr, "the starters hide once something is routed");
        }

        pages->showPage ("FX");
        settle (300);
        auto* tapeStop = findButton ("TAPE STOP");
        expect (tapeStop != nullptr, "the empty rack offers every effect, grouped");

        if (tapeStop != nullptr)
        {
            tapeStop->triggerClick();
            settle (300);
            expect ((int) processor.apvts.getRawParameterValue ("fx_slot1")->load() == 17, "a quick-add button fills slot 1");
        }

        processor.loadFactoryPreset (0);
        settle (200);
    }

    // M7.1: the Generative card switches between ARP, EUCLID and PROB SEQ.
    {
        pages->showPage ("ARP/SEQ");
        settle (100);
        std::vector<CardTabs*> cardTabs;
        findAll<CardTabs> (*editor, cardTabs);
        CardTabs* engineTabs = nullptr;
        for (auto* candidate : cardTabs)
            if (candidate->getNames().contains ("PROB SEQ"))
                engineTabs = candidate;

        expect (engineTabs != nullptr, "the ARP/SEQ page has a Generative card with ARP, EUCLID and PROB SEQ");

        if (engineTabs != nullptr)
        {
            pages->showPage ("ARP/SEQ");
            engineTabs->setSelected (1, true);
            settle (200);

            const auto visibleWith = [&editor] (const juce::String& id)
            {
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*editor, knobs);
                for (auto* knob : knobs)
                    if (knob->getParameterId() == id)
                        return knob->isVisible();
                return false;
            };

            expect (visibleWith ("euc_hits") && ! visibleWith ("arp_gate") && ! visibleWith ("pseq_length"),
                    "the EUCLID tab shows only Euclid's controls");
            engineTabs->setSelected (2, true);
            settle (200);
            expect (visibleWith ("pseq_length") && ! visibleWith ("euc_hits"), "the PROB SEQ tab shows the sequencer");
            expect (visibleWith ("spray_strum_time"), "the GENERATE card has STRUM TIME");
            engineTabs->setSelected (0, true);
        }
    }

    // M8.1: the LFO card for a simulated shape.
    {
        const auto knobFor = [&editor] (const juce::String& id) -> KnobControl*
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
                if (knob->getParameterId() == id)
                    return knob;
            return nullptr;
        };
        const auto setShape = [&processor] (int shape)
        {
            if (auto* parameter = processor.apvts.getParameter ("lfo1_shape"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) shape));
        };
        pages->showPage ("ENV/LFO");
        if (auto* page = pages->getCurrentPage())
            if (auto* thumbs = findChild<LfoThumbBar> (*page); thumbs != nullptr && thumbs->onSelect != nullptr)
                thumbs->onSelect (0);
        setShape (LfoSimShapes::Lorenz);
        settle (300);
        auto* p1 = knobFor ("lfo1_p1");
        auto* p4 = knobFor ("lfo1_p4");
        auto* start = knobFor ("lfo1_phase");
        auto* smooth = knobFor ("lfo1_smooth");
        expect (p1 != nullptr && p1->isVisible() && p1->getLabelText() == "SIGMA", "Lorenz shows a SIGMA knob");
        expect (p1 != nullptr && p1->getSlider().getTextFromValue (0.5) == "10.00", "SIGMA reads 10.00 at the middle");
        expect (p4 != nullptr && ! p4->isVisible(), "Lorenz hides the knobs it doesn't use");
        expect (start != nullptr && ! start->isVisible() && smooth != nullptr && smooth->isVisible(),
                "simulated shapes show SMOOTH instead of START");
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*editor, buttons);
        auto fireShown = false;
        for (auto* button : buttons)
            fireShown = fireShown || (button->getButtonText() == "FIRE" && button->isVisible());
        expect (fireShown, "the card has a FIRE button");

        setShape (LfoSimShapes::Bounce);
        settle (300);
        expect (p1 != nullptr && p1->getLabelText() == "GRAVITY" && p4 != nullptr && p4->isVisible() && p4->getLabelText() == "DRAG",
                "Bounce names its knobs GRAVITY ... DRAG");
        setShape (0);
        settle (300);
        expect (p1 != nullptr && ! p1->isVisible() && smooth != nullptr && smooth->isVisible() && start != nullptr && start->isVisible(),
                "classic shapes keep START and gain SMOOTH");
        expect (Mod::getSourceNames().contains ("LFO 16 B"), "every LFO's output B is a mod source");
    }

    // M8.5: the VECTOR page has the pad and EVOLVE.
    {
        pages->showPage ("VECTOR");
        settle (200);
        auto* page = pages->getCurrentPage();
        expect (page != nullptr && findChild<VectorPadDisplay> (*page) != nullptr, "the VECTOR page shows the vector pad");
        std::vector<KnobControl*> knobs;
        if (page != nullptr)
            findAll<KnobControl> (*page, knobs);
        auto evolveKnobs = 0;
        for (auto* knob : knobs)
            if (knob->getParameterId().endsWith ("_evolve"))
                ++evolveKnobs;
        expect (evolveKnobs == Mod::numMacros, "EVOLVE has a knob for each macro");
        std::vector<juce::TextButton*> buttons;
        if (page != nullptr)
            findAll<juce::TextButton> (*page, buttons);
        auto freeze = false;
        for (auto* b : buttons)
            freeze = freeze || b->getButtonText() == "FREEZE";
        expect (freeze, "EVOLVE has a FREEZE button");
    }

    // M8.6: BOUNCE on every oscillator card; a bounce turns the card to
    // Sample mode.
    {
        pages->showPage ("OSC");
        settle (200);
        std::vector<juce::TextButton*> buttons;
        findAll<juce::TextButton> (*editor, buttons);
        auto bounces = 0;
        for (auto* button : buttons)
            bounces += button->getButtonText() == "BOUNCE" && visibleInTree (button) ? 1 : 0;
        expect (bounces >= 1, "the switched-on oscillator cards have BOUNCE buttons (" + juce::String (bounces) + ")");

        IlanaSynthAudioProcessor::BounceRequest request;
        request.targetOsc = 1;
        request.holdSeconds = 0.5;
        request.tailSeconds = 0.5;
        request.muteOthers = false;
        processor.startBounce (request);
        for (int i = 0; i < 300 && processor.getBounceState() == IlanaSynthAudioProcessor::BounceState::Rendering; ++i)
            settle (50);
        settle (300);
        {
            std::vector<KnobControl*> shownKnobs;
            findAll<KnobControl> (*editor, shownKnobs);
            if (std::none_of (shownKnobs.begin(), shownKnobs.end(), [] (KnobControl* k) { return k->getParameterId() == "osc2_level" && visibleInTree (k); }))
                clickCardTitle ("osc2_on"); // folded to fit: open it
        }
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        auto sampleShown = false;
        for (auto* knob : knobs)
            sampleShown = sampleShown || (knob->getParameterId() == "osc2_sample_start" && visibleInTree (knob));
        expect (processor.getBounceState() == IlanaSynthAudioProcessor::BounceState::Done && sampleShown,
                "a bounce puts OSC 2 in Sample mode with the sample controls showing");
    }

    // M8.7: the PHYSICAL page follows the patch's Physical oscillator.
    {
        processor.loadFactoryPreset (names.indexOf ("Felt Hammer Board"));
        pages->showPage ("PHYSICAL");
        settle (400);
        auto* page = pages->getCurrentPage();
        expect (page != nullptr && findChild<PhysicalView> (*page) != nullptr, "the PHYSICAL page shows the animated string");
        std::vector<KnobControl*> knobs;
        if (page != nullptr)
            findAll<KnobControl> (*page, knobs);
        juce::String physicalPrefix;
        for (const auto* prefix : OscillatorIds::prefixes)
            if (physicalPrefix.isEmpty() && juce::roundToInt (processor.apvts.getRawParameterValue (juce::String (prefix) + "_mode")->load()) == 1)
                physicalPrefix = prefix;
        auto bound = false;
        for (auto* knob : knobs)
            bound = bound || (knob->getParameterId() == physicalPrefix + "_string_decay" && visibleInTree (knob));
        expect (physicalPrefix.isNotEmpty() && bound, "the PHYSICAL page shows the Felt Hammer Board's string (" + physicalPrefix + ")");
        processor.loadFactoryPreset (neuroWobble);
        settle (200);
    }

    // M8.4: the type grid turns to the page holding a new model.
    {
        if (auto* parameter = processor.apvts.getParameter ("f1_type"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) FilterType::Steiner));
        pages->showPage ("FILTER");
        settle (200);
        std::vector<FilterTypeGrid*> grids;
        findAll<FilterTypeGrid> (*editor, grids);
        auto onSecond = false;
        for (auto* grid : grids)
        {
            grid->repaint();
            juce::Image image (juce::Image::ARGB, juce::jmax (1, grid->getWidth()), juce::jmax (1, grid->getHeight()), true);
            juce::Graphics g (image);
            grid->paintEntireComponent (g, false);
            onSecond = onSecond || grid->getPage() == 1;
        }
        expect (! grids.empty() && onSecond, "the filter type grid shows the page with the new models");

        // Every type has a short name, and an Airwindows model opens its page.
        expect (FilterTypeGrid::shortNames().size() == FilterType::Count, "every filter type has a short name in the grid");
        if (auto* parameter = processor.apvts.getParameter ("f1_type"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) FilterType::Disperser));
        settle (200);
        auto onAirwindows = false;
        for (auto* grid : grids)
        {
            juce::Image image (juce::Image::ARGB, juce::jmax (1, grid->getWidth()), juce::jmax (1, grid->getHeight()), true);
            juce::Graphics g (image);
            grid->paintEntireComponent (g, false);
            onAirwindows = onAirwindows || grid->getPage() == 3;
        }
        expect (onAirwindows, "the filter type grid shows the AIRWINDOWS page for the Disperser");
        if (auto* parameter = processor.apvts.getParameter ("f1_type"))
            parameter->setValueNotifyingHost (0.0f);
    }

    // M8.3: the FILTER page's WEST tab shows the west-coast card.
    {
        pages->showPage ("FILTER");
        settle (200);
        std::vector<CardTabs*> cardTabs;
        findAll<CardTabs> (*editor, cardTabs);
        CardTabs* westTabs = nullptr;
        for (auto* bar : cardTabs)
            if (bar->getNames().contains ("WEST"))
                westTabs = bar;
        expect (westTabs != nullptr, "the FILTER page has FILTER 2 / WEST tabs");
        if (westTabs != nullptr)
        {
            const auto visibleKnob = [&editor] (const juce::String& id)
            {
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*editor, knobs);
                for (auto* knob : knobs)
                    if (knob->getParameterId() == id)
                    {
                        auto shown = true;
                        for (juce::Component* c = knob; c != nullptr && c->getParentComponent() != nullptr; c = c->getParentComponent())
                            shown = shown && c->isVisible();
                        if (shown)
                            return true;
                    }
                return false;
            };
            westTabs->setSelected (1, true);
            settle (200);
            expect (visibleKnob ("west_fold") && visibleKnob ("west_decay") && ! visibleKnob ("f2_cutoff"),
                    "the WEST tab shows FOLD and DECAY in Filter 2's place");
            westTabs->setSelected (0, true);
            settle (200);
            expect (visibleKnob ("f2_cutoff") && ! visibleKnob ("west_fold"), "the FILTER 2 tab brings Filter 2 back");
        }
    }

    // Knobs, chips, the filter graph, the browser and SUB + NOISE (UI review 1 fixes).
    {
        const auto makeEvent = [] (juce::Component& component, juce::Point<float> position, bool dragged)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys(),
                                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(),
                                     position, juce::Time::getCurrentTime(), 1, dragged);
        };

        // A click on a source chip keeps every knob it drives lit; another click clears it.
        processor.loadFactoryPreset (neuroWobble);
        pages->showPage ("MAIN");
        settle (400);
        std::vector<ModSourceChip*> chipsFound;
        std::vector<KnobControl*> knobsFound;
        findAll<ModSourceChip> (*editor, chipsFound);
        findAll<KnobControl> (*editor, knobsFound);
        ModSourceChip* chosen = nullptr;
        auto drivenKnobs = 0;

        for (auto* chip : chipsFound)
        {
            if (! visibleInTree (chip))
                continue;

            auto count = 0;

            for (auto* knob : knobsFound)
                if (visibleInTree (knob) && knob->isDrivenBy (chip->getSourceIndex()))
                    ++count;

            if (count > 0)
            {
                chosen = chip;
                drivenKnobs = count;
                break;
            }
        }

        expect (chosen != nullptr, "Neuro Wobble has a source chip that drives a visible knob");

        if (chosen != nullptr)
        {
            auto lit = [&]
            {
                auto count = 0, wrong = 0;
                for (auto* knob : knobsFound)
                    if (visibleInTree (knob))
                    {
                        count += knob->isLitByPinnedSource() ? 1 : 0;
                        wrong += knob->isLitByPinnedSource() != knob->isDrivenBy (chosen->getSourceIndex()) ? 1 : 0;
                    }
                return std::pair<int, int> { count, wrong };
            };

            expect (pinnedModSource() == 0 && lit().first == 0, "no source is pinned to start with");
            chosen->mouseUp (makeEvent (*chosen, { 8.0f, 8.0f }, false));
            settle (100);
            expect (pinnedModSource() == chosen->getSourceIndex() && chosen->isPinned(), "clicking a chip pins its source");
            expect (lit().first == drivenKnobs && lit().second == 0,
                    "the pinned chip lights exactly the knobs it drives (" + juce::String (drivenKnobs) + ")");
            chosen->mouseUp (makeEvent (*chosen, { 8.0f, 8.0f }, true)); // a drag is not a click
            expect (pinnedModSource() == chosen->getSourceIndex(), "dragging a chip does not unpin it");
            chosen->mouseUp (makeEvent (*chosen, { 8.0f, 8.0f }, false));
            expect (pinnedModSource() == 0 && lit().first == 0, "clicking the chip again clears the highlight");
        }

        // UI review 4, batch B (V8, V9, V17, V18, V24, S7, S12): modulation.
        {
            using Module = IlanaSynthAudioProcessor::Module;
            const auto usedSource = [&processor] (int source)
            {
                for (int i = 0; i < Mod::maxSlots; ++i)
                {
                    const auto slot = processor.readModSlot (i);
                    if (slot.destination != 0 && ((int) slot.source == source || (int) slot.aux == source))
                        return true;
                }
                return false;
            };
            const auto poolKindOf = [] (int source) -> std::pair<int, int>
            {
                if (const auto lfo = Mod::lfoIndexFor ((Mod::Source) source); lfo >= 0)
                    return { (int) Module::Lfo, lfo };
                using S = Mod::Source;
                const S fixedEnvs[] { S::AmpEnv, S::FilterEnv, S::FilterEnv2, S::ModEnv, S::Env4 };
                for (int i = 0; i < 5; ++i)
                    if ((S) source == fixedEnvs[i])
                        return { (int) Module::Envelope, i };
                if (source >= (int) S::Env6 && source <= (int) S::Env16)
                    return { (int) Module::Envelope, 5 + source - (int) S::Env6 };
                return { -1, 0 };
            };
            const auto chipState = [&]
            {
                std::vector<ModSourceChip*> found;
                findAll<ModSourceChip> (*editor, found);
                auto shown = 0, compact = 0, strays = 0;
                juce::StringArray strayNames;
                for (auto* chip : found)
                {
                    if (! chip->isVisible() || chip->getParentComponent() == nullptr)
                        continue;
                    ++shown;
                    compact += chip->isCompact() ? 1 : 0;
                    const auto [kind, index] = poolKindOf (chip->getSourceIndex());
                    if (kind >= 0 && ! processor.isRevealed ((Module) kind, index) && ! usedSource (chip->getSourceIndex()))
                    {
                        ++strays;
                        strayNames.add (chip->getSourceName());
                    }
                }
                return std::make_tuple (shown, compact, strays, strayNames.joinIntoString (", "));
            };

            // The chip row follows the pool, and abbreviates all chips or none.
            {
                const auto [shown, compact, strays, strayNames] = chipState();
                expect (shown > 0 && strays == 0, "the chip row shows only sources in the pool or in use (" + juce::String (shown)
                                                      + " shown" + (strays > 0 ? "; not in the pool: " + strayNames : juce::String()) + ")");
                expect (compact == 0 || compact == shown, "no chip is abbreviated unless all are (" + juce::String (compact) + " of "
                                                              + juce::String (shown) + ")");
            }

            // "+" adds a source to the pool: LFO 9 (chip index 8) gets a chip.
            const auto lfoMask = processor.isRevealed (Module::Lfo, 8);
            pages->addPoolSource (8);
            settle (100);
            {
                std::vector<ModSourceChip*> found;
                findAll<ModSourceChip> (*editor, found);
                auto lfo9 = false;
                for (auto* chip : found)
                    lfo9 = lfo9 || (chip->isVisible() && chip->getSourceIndex() == (int) Mod::lfoSourceFor (8));
                expect (processor.isRevealed (Module::Lfo, 8) && lfo9, "the chip picker adds LFO 9 to the pool and the row");
            }
            processor.setRevealed (Module::Lfo, 8, lfoMask);

            // A full pool: every chip shown, all shortened alike.
            std::vector<bool> lfoBefore, envBefore;
            for (int i = 0; i < 16; ++i)
            {
                lfoBefore.push_back (processor.isRevealed (Module::Lfo, i));
                envBefore.push_back (processor.isRevealed (Module::Envelope, i));
                processor.setRevealed (Module::Lfo, i, true);
                processor.setRevealed (Module::Envelope, i, true);
            }
            settle (200);
            {
                const auto [shown, compact, strays, strayNames] = chipState();
                expect (shown >= 32 + 6 && (compact == 0 || compact == shown),
                        "a full pool shows every chip, none hidden behind '+N', shortened all alike (" + juce::String (shown)
                            + " shown, " + juce::String (compact) + " short)");
            }
            for (int i = 0; i < 16; ++i)
            {
                processor.setRevealed (Module::Lfo, i, lfoBefore[(size_t) i]);
                processor.setRevealed (Module::Envelope, i, envBefore[(size_t) i]);
            }
            settle (200);

            // One colour per source: eight macros, the performance sources,
            // and LFO 1 off the accent.
            {
                using S = Mod::Source;
                std::vector<int> sources;
                for (int m = 0; m < Mod::numMacros; ++m)
                    sources.push_back ((int) Mod::macroSourceFor (m));
                for (const auto s : { S::Velocity, S::KeyTrack, S::Random, S::ClockSh, S::ModWheel, S::Aftertouch, S::Expression,
                                      S::Lfo1, S::Lfo2, S::Lfo3, S::Lfo4, S::AmpEnv, S::FilterEnv, S::FilterEnv2, S::ModEnv, S::Env4 })
                    sources.push_back ((int) s);
                for (int s = (int) S::Env6; s <= (int) S::Env16; ++s)
                    sources.push_back (s);
                for (int s = (int) S::Lfo5; s <= (int) S::Lfo16; ++s)
                    sources.push_back (s);

                auto clashes = 0;
                juce::String example;
                for (size_t a = 0; a < sources.size(); ++a)
                    for (size_t b = a + 1; b < sources.size(); ++b)
                    {
                        const auto ca = modSourceColour (sources[a]), cb = modSourceColour (sources[b]);
                        const auto distance = std::abs (ca.getFloatRed() - cb.getFloatRed()) + std::abs (ca.getFloatGreen() - cb.getFloatGreen())
                                              + std::abs (ca.getFloatBlue() - cb.getFloatBlue());
                        if (distance < 0.06f)
                        {
                            ++clashes;
                            example = Mod::getSourceNames()[sources[a]] + " / " + Mod::getSourceNames()[sources[b]];
                        }
                    }
                expect (clashes == 0, "every source has its own colour (" + juce::String (clashes) + " clashes"
                                          + (example.isNotEmpty() ? ", e.g. " + example : juce::String()) + ")");

                std::set<juce::uint32> macroColours;
                for (int m = 0; m < Mod::numMacros; ++m)
                    macroColours.insert (modSourceColour ((int) Mod::macroSourceFor (m)).getARGB());
                expect (macroColours.size() == (size_t) Mod::numMacros, "the eight macros have eight colours");

                auto hueGap = std::abs (modSourceColour ((int) S::Lfo1).getHue() - IlanaTheme::accent().getHue());
                hueGap = juce::jmin (hueGap, 1.0f - hueGap);
                expect (hueGap > 0.08f, "LFO 1 is not the accent's orange");
            }

            // MATRIX: rows numbered 1..n as shown, repeats flagged, sortable,
            // the remap editor docked under its row.
            pages->showPage ("MATRIX");
            settle (400);
            const auto shownRows = [&]
            {
                std::vector<MatrixRow*> found, visible;
                findAll<MatrixRow> (*editor, found);
                for (auto* row : found)
                    if (row->isVisible())
                        visible.push_back (row);
                std::sort (visible.begin(), visible.end(), [] (auto* a, auto* b) { return a->getY() < b->getY(); });
                return visible;
            };
            {
                const auto visible = shownRows();
                auto numbered = ! visible.empty();
                auto duplicates = 0;
                for (size_t i = 0; i < visible.size(); ++i)
                {
                    numbered = numbered && visible[i]->getDisplayNumber() == (int) i + 1;
                    duplicates += visible[i]->isDuplicate() ? 1 : 0;
                }
                const auto lastSlot = visible.empty() ? 0 : visible.back()->getSlotIndex() + 1;
                expect (numbered && lastSlot > (int) visible.size(),
                        "matrix rows are numbered 1.." + juce::String (visible.size()) + " as shown (the last is slot " + juce::String (lastSlot) + ")");
                expect (duplicates >= 2, "Neuro Wobble's repeated LFO 1 > Filter1 Cutoff routing is flagged on both rows ("
                                             + juce::String (duplicates) + ")");
                if (! visible.empty())
                    expect (visible[0]->getHeight() <= 30, "matrix rows are compact (" + juce::String (visible[0]->getHeight()) + " px)");
            }

            if (auto* page = pages->getCurrentPage())
            {
                // The add button is pinned in the header, outside the scrolling list.
                std::vector<juce::TextButton*> buttons;
                findAll<juce::TextButton> (*page, buttons);
                juce::TextButton* add = nullptr;
                for (auto* button : buttons)
                    if (button->getButtonText().contains ("ADD MODULATION"))
                        add = button;
                expect (add != nullptr && add->getParentComponent() == page && add->getY() < 40,
                        "+ ADD MODULATION is pinned in the matrix header");

                // A click on SOURCE sorts by source name; on # goes back to slot order.
                const auto clickAt = [&] (juce::Point<float> at)
                {
                    page->mouseUp (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(), 1.0f,
                                                     0.0f, 0.0f, 0.0f, 0.0f, page, page, juce::Time::getCurrentTime(), at,
                                                     juce::Time::getCurrentTime(), 1, false));
                    settle (100);
                };
                using C = MatrixRow::Columns;
                const auto headingY = 6.0f + 32.0f + 9.0f;
                clickAt ({ 12.0f + (float) (C::number + C::bypass + C::gap + C::meter + C::gap) + 20.0f, headingY });
                auto bySource = true;
                {
                    const auto visible = shownRows();
                    for (size_t i = 1; i < visible.size(); ++i)
                    {
                        const auto a = Mod::getSourceNames()[(int) processor.readModSlot (visible[i - 1]->getSlotIndex()).source];
                        const auto b = Mod::getSourceNames()[(int) processor.readModSlot (visible[i]->getSlotIndex()).source];
                        bySource = bySource && a.compareNatural (b) <= 0;
                    }
                    bySource = bySource && ! visible.empty() && visible[0]->getDisplayNumber() == 1;
                }
                clickAt ({ 20.0f, headingY });
                auto bySlot = true;
                {
                    const auto visible = shownRows();
                    for (size_t i = 1; i < visible.size(); ++i)
                        bySlot = bySlot && visible[i - 1]->getSlotIndex() < visible[i]->getSlotIndex();
                }
                expect (bySource && bySlot, "clicking SOURCE sorts the matrix by source, # goes back to slot order");

                // The remap editor opens under its row, covers no other row, and closes with its X.
                const auto visible = shownRows();
                if (visible.size() >= 3)
                {
                    auto* row = visible[1];
                    row->getCurve().openRemapEditor();
                    settle (200);
                    auto* remap = findChild<RemapEditor> (*page);
                    auto covers = 0;
                    if (remap != nullptr)
                        for (auto* other : shownRows())
                            if (page->getLocalArea (other, other->getLocalBounds()).intersects (page->getLocalArea (remap, remap->getLocalBounds())))
                                ++covers;
                    const auto below = remap != nullptr && page->getLocalArea (remap, remap->getLocalBounds()).getY()
                                                               >= page->getLocalArea (row, row->getLocalBounds()).getBottom();
                    expect (remap != nullptr && covers == 0 && below, "the remap editor opens docked under its row and covers no row ("
                                                                          + juce::String (covers) + ")");
                    if (remap != nullptr)
                    {
                        remap->createComponentSnapshot (remap->getLocalBounds());
                        expect (remap->getLiveInput() >= 0.0f, "the remap editor shows the live input on the curve");
                        std::vector<juce::TextButton*> tools;
                        findAll<juce::TextButton> (*remap, tools);
                        juce::TextButton* close = nullptr;
                        auto shapes = false;
                        for (auto* button : tools)
                        {
                            close = button->getButtonText() == juce::String::fromUTF8 ("\xc3\x97") && button->isVisible() ? button : close;
                            shapes = shapes || (button->getButtonText().startsWith ("SHAPES") && button->isVisible());
                        }
                        expect (close != nullptr && shapes, "the remap editor has a close button and a shapes button");
                        if (close != nullptr)
                        {
                            close->triggerClick();
                            settle (200);
                            expect (findChild<RemapEditor> (*page) == nullptr, "the remap editor's X closes it");
                        }
                    }
                }
            }

            // MIDI learn on any automatable knob: the next CC drives it, and
            // the map is saved with the patch.
            {
                processor.startParamLearn ("f1_cutoff");
                const auto learning = processor.getParamLearnTarget() == "f1_cutoff";
                juce::AudioBuffer<float> buffer (2, 512);
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 100), 0);
                processor.processBlock (buffer, midi);
                settle (100);
                auto* cutoff = processor.apvts.getParameter ("f1_cutoff");
                const auto mapped = processor.getParamCc ("f1_cutoff") == 74 && processor.getParamLearnTarget().isEmpty();
                const auto moved = cutoff != nullptr && std::abs (cutoff->getValue() - 100.0f / 127.0f) < 0.01f;
                expect (learning && mapped && moved, "MIDI learn on a knob maps the next CC (74) and the CC moves it");

                juce::MemoryBlock saved;
                processor.getStateInformation (saved);
                processor.clearParamCc ("f1_cutoff");
                const auto cleared = processor.getParamCc ("f1_cutoff") < 0;
                processor.setStateInformation (saved.getData(), (int) saved.getSize());
                settle (100);
                expect (cleared && processor.getParamCc ("f1_cutoff") == 74, "a learned CC is saved and restored with the patch");
                processor.clearParamCc ("f1_cutoff");
                processor.loadFactoryPreset (neuroWobble);
                settle (200);
            }
        }

        // The macro names the browser lists match what the preset loads with.
        {
            const auto listed = processor.getFactoryMacroNames (neuroWobble);
            auto matches = true;
            for (const auto& name : listed)
            {
                auto found = false;
                for (int macro = 0; macro < 4; ++macro)
                    found = found || processor.getMacroName (macro) == name;
                matches = matches && found;
            }
            expect (! listed.isEmpty() && matches, "the browser's macro names for Neuro Wobble are the ones it loads ("
                                                       + listed.joinIntoString (", ") + ")");
        }

        // Worked out without loading, the list's macro names equal what every factory preset loads with.
        {
            juce::StringArray mismatches;
            for (int index = 0; index < names.size(); ++index)
            {
                processor.loadFactoryPreset (index);
                const auto listed = processor.getFactoryMacroNames (index);
                for (int macro = 0; macro < 4; ++macro)
                {
                    const auto loaded = processor.apvts.state.getProperty ("macroName" + juce::String (macro + 1)).toString();
                    if (loaded != listed[macro])
                        mismatches.add (names[index] + " #" + juce::String (macro + 1) + " listed '" + listed[macro] + "' loads '" + loaded + "'");
                }
            }
            expect (mismatches.isEmpty(), "the browser lists every factory preset's macro names as loaded ("
                                              + juce::String (names.size()) + " presets"
                                              + (mismatches.isEmpty() ? juce::String() : "; " + juce::String (mismatches.size()) + " differ, e.g. " + mismatches[0]) + ")");
            for (const auto& line : mismatches)
                std::cout << "  " << line << std::endl;
            processor.loadFactoryPreset (neuroWobble);
        }

        // Up / down in the open browser load the next / previous preset and leave it open.
        if (auto* display = findChild<PresetDisplay> (*editor); display != nullptr && display->onClick != nullptr)
        {
            display->onClick();
            settle (500);
            auto* panel = findChild<PresetPanel> (*editor);
            expect (panel != nullptr && panel->isOpen(), "the preset browser opens");

            if (panel != nullptr)
            {
                const auto start = processor.getCurrentPresetName();
                panel->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
                settle (150);
                const auto next = processor.getCurrentPresetName();
                expect (next.isNotEmpty() && next != start && panel->isOpen(), "Down loads the next preset and keeps the browser open");
                panel->keyPressed (juce::KeyPress (juce::KeyPress::upKey));
                settle (150);
                expect (processor.getCurrentPresetName() == start && panel->isOpen(), "Up steps back to the preset before");
            }

            display->onClick();
            settle (500);
        }

        // SUB + NOISE folds to one line while both are off, and opens for either.
        {
            pages->showPage ("MAIN");
            const auto set = [&] (const char* id, float value)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            const auto noiseShown = [&]
            {
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*editor, knobs);
                for (auto* knob : knobs)
                    if (visibleInTree (knob) && knob->getParameterId() == "noise_level")
                        return true;
                return false;
            };

            processor.loadFactoryPreset (0);
            set ("subosc_on", 0.0f);
            set ("noise_level", 0.0f);
            settle (600);
            expect (! noiseShown(), "PLAY folds SUB + NOISE to one line while both are off");
            auto* output = findChild<OutputView> (*editor);
            expect (output != nullptr && visibleInTree (output) && output->getHeight() >= 44,
                    "the freed space shows the live output view");
            set ("noise_level", 0.3f);
            settle (600);
            expect (noiseShown(), "turning the noise up opens SUB + NOISE");
            set ("noise_level", 0.0f);
            set ("subosc_on", 1.0f);
            settle (600);
            expect (noiseShown(), "switching the sub on opens SUB + NOISE");
            set ("subosc_on", 0.0f);
            settle (600);
            expect (! noiseShown(), "SUB + NOISE folds again when both are off");
        }

        // Dragging the filter graph: across for cutoff, up and down for resonance, one gesture each.
        if (auto* display = findChild<FilterDisplay> (*editor); display != nullptr)
        {
            struct GestureCounter : juce::AudioProcessorParameter::Listener
            {
                int begins = 0, ends = 0;
                void parameterValueChanged (int, float) override {}
                void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
            } cutoffGestures, resoGestures;

            processor.loadFactoryPreset (0);
            pages->showPage ("MAIN");
            settle (400);
            auto* cutoff = processor.apvts.getParameter ("f1_cutoff");
            auto* reso = processor.apvts.getParameter ("f1_reso");
            cutoff->addListener (&cutoffGestures);
            reso->addListener (&resoGestures);
            const auto before = processor.apvts.getRawParameterValue ("f1_cutoff")->load();
            const auto resoBefore = processor.apvts.getRawParameterValue ("f1_reso")->load();
            const auto w = (float) display->getWidth();
            const auto h = (float) display->getHeight();
            auto* component = static_cast<juce::Component*> (display);
            clearHistory();
            component->mouseDown (makeEvent (*component, { 10.0f + 0.5f * (w - 20.0f), 0.5f * h }, false));
            component->mouseDrag (makeEvent (*component, { 10.0f + 0.3f * (w - 20.0f), 0.2f * h }, true));
            component->mouseUp (makeEvent (*component, { 10.0f + 0.3f * (w - 20.0f), 0.2f * h }, true));
            const auto after = processor.apvts.getRawParameterValue ("f1_cutoff")->load();
            const auto resoAfter = processor.apvts.getRawParameterValue ("f1_reso")->load();
            {
                settle (100);
                const auto steps = undoSteps();
                processor.getUndoManager().undo();
                const auto undone = processor.apvts.getRawParameterValue ("f1_cutoff")->load();
                processor.getUndoManager().redo();
                expect (steps.size() == 1 && steps[0] == "Filter 1 graph" && std::abs (undone / before - 1.0f) < 0.01f
                            && std::abs (processor.apvts.getRawParameterValue ("f1_cutoff")->load() / after - 1.0f) < 0.01f,
                        "the filter graph drag is one undo step ('" + steps.joinIntoString ("', '") + "'); undo and redo move the cutoff");
            }
            expect (before > 5000.0f && std::abs (after / 159.0f - 1.0f) < 0.15f,
                    "dragging across the filter graph sets the cutoff (" + juce::String (before, 0) + " -> " + juce::String (after, 0) + " Hz)");
            expect (resoAfter > resoBefore + 0.1f, "dragging up on the filter graph raises the resonance");
            expect (cutoffGestures.begins == 1 && cutoffGestures.ends == 1 && resoGestures.begins == 1 && resoGestures.ends == 1,
                    "the filter drag is one gesture per parameter");
            cutoff->removeListener (&cutoffGestures);
            reso->removeListener (&resoGestures);
            processor.loadFactoryPreset (0);
            settle (200);
        }

        // The oscillator display: across scrubs the frame, up raises the warp, one gesture each; the corner key cycles WAVE / 3D / SPEC.
        {
            processor.loadFactoryPreset (0);
            pages->showPage ("MAIN");
            const auto set = [&] (const char* id, float value)
            {
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            set ("osc1_warp", 2.0f); // Bend +
            set ("osc1_warp_amt", 0.2f);
            settle (400);

            std::vector<WaveDisplay*> waves;
            findAll<WaveDisplay> (*editor, waves);
            WaveDisplay* wave = nullptr;
            for (auto* candidate : waves)
                if (visibleInTree (candidate) && candidate->getOscIndex() == 0)
                    wave = candidate;
            expect (wave != nullptr, "PLAY shows OSC 1's display");

            if (wave != nullptr)
            {
                struct GestureCounter : juce::AudioProcessorParameter::Listener
                {
                    int begins = 0, ends = 0;
                    void parameterValueChanged (int, float) override {}
                    void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
                } frameGestures, warpGestures;

                auto* frame = processor.apvts.getParameter ("osc1_frame");
                auto* warp = processor.apvts.getParameter ("osc1_warp_amt");
                frame->addListener (&frameGestures);
                warp->addListener (&warpGestures);
                const auto w = (float) wave->getWidth();
                const auto h = (float) wave->getHeight();
                auto* component = static_cast<juce::Component*> (wave);
                component->mouseDown (makeEvent (*component, { 10.0f + 0.25f * (w - 20.0f), 0.6f * h }, false));
                component->mouseDrag (makeEvent (*component, { 10.0f + 0.75f * (w - 20.0f), 0.2f * h }, true));
                const auto frameAfter = frame->getValue();
                const auto warpAfter = processor.apvts.getRawParameterValue ("osc1_warp_amt")->load();
                expect (wave->isDraggingWarp(), "a drag on a warped table moves the warp");
                component->mouseUp (makeEvent (*component, { 10.0f + 0.75f * (w - 20.0f), 0.2f * h }, true));
                expect (std::abs (frameAfter - 0.75f) < 0.03f, "dragging across the oscillator display scrubs the frame (" + juce::String (frameAfter, 2) + ")");
                expect (warpAfter > 0.3f, "dragging up on the oscillator display raises the warp amount (0.20 -> " + juce::String (warpAfter, 2) + ")");
                expect (frameGestures.begins == 1 && frameGestures.ends == 1 && warpGestures.begins == 1 && warpGestures.ends == 1,
                        "the oscillator drag is one gesture per parameter");
                component->mouseDoubleClick (makeEvent (*component, { 20.0f, 20.0f }, false));
                expect (processor.apvts.getRawParameterValue ("osc1_warp_amt")->load() < 0.001f, "a double-click sets the warp back to zero");
                frame->removeListener (&frameGestures);
                warp->removeListener (&warpGestures);

                // With no warp chosen a vertical drag leaves the amount alone.
                set ("osc1_warp", 0.0f);
                set ("osc1_warp_amt", 0.4f);
                settle (100);
                component->mouseDown (makeEvent (*component, { 30.0f, 0.8f * h }, false));
                component->mouseDrag (makeEvent (*component, { 30.0f, 0.1f * h }, true));
                component->mouseUp (makeEvent (*component, { 30.0f, 0.1f * h }, true));
                expect (std::abs (processor.apvts.getRawParameterValue ("osc1_warp_amt")->load() - 0.4f) < 0.001f,
                        "without a warp the vertical drag changes nothing");

                std::vector<juce::TextButton*> keys;
                findAll<juce::TextButton> (*wave, keys);
                juce::StringArray seen;
                for (int i = 0; i < 3 && ! keys.empty(); ++i)
                {
                    seen.add (keys.front()->getButtonText());
                    keys.front()->triggerClick();
                    settle (60);
                    const auto image = wave->createComponentSnapshot (wave->getLocalBounds());
                    juce::ignoreUnused (image);
                }
                expect (seen.joinIntoString (",") == "WAVE,3D,SPEC" && wave->getViewMode() == 0,
                        "the view key cycles WAVE, 3D and SPEC (" + seen.joinIntoString (",") + ")");
            }

            processor.loadFactoryPreset (0);
            settle (200);
        }

        // A macro's arc on an OSC 1 knob (yellow on gold) is drawn paler; an unrelated hue is left alone.
        {
            const auto macro = modSourceColour ((int) Mod::Source::Macro1);
            const auto onGold = modArcColour (macro, IlanaTheme::oscColour (0));
            const auto onBlue = modArcColour (macro, IlanaTheme::oscColour (1));
            expect (onGold.getBrightness() > macro.getBrightness() + 0.02f || onGold.getSaturation() < macro.getSaturation() - 0.2f,
                    "a macro arc on an OSC 1 knob stands apart from its gold");
            expect (onBlue == macro, "a macro arc on an OSC 2 knob keeps the macro colour");
        }

        // Resting on a modulated knob opens a card listing its sources with live bars.
        {
            processor.loadFactoryPreset (neuroWobble);
            pages->showPage ("MAIN");
            settle (400);
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            KnobControl* modulated = nullptr;
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getNumRoutings() > 0)
                {
                    modulated = knob;
                    break;
                }
            expect (modulated != nullptr, "Neuro Wobble has a modulated knob on PLAY");
            auto* card = findChild<ModHoverPopup> (*editor);
            expect (card != nullptr, "the editor has the modulation card");

            if (modulated != nullptr && card != nullptr)
            {
                modulated->openModCard(); // (hover opens it after a short rest; the test has no mouse)
                const auto inside = card->getParentComponent() != nullptr
                                    && card->getParentComponent()->getLocalBounds().contains (card->getBounds());
                expect (card->isShowingFor (*modulated) && card->getNumRows() >= modulated->getNumRoutings() && inside,
                        "the card lists the knob's " + juce::String (modulated->getNumRoutings()) + " source(s) inside the window ("
                            + modulated->getParameterId() + ")");
                modulated->closeModCard();
                settle (60);
                expect (! card->isVisible(), "leaving the knob closes the card");

                // The card's rows are controls: drag for depth (one gesture), double-click for zero, right-click to bypass or remove.
                modulated->openModCard (true);
                const auto slot = card->getRowSlot (0);
                auto* depth = slot >= 0 ? processor.apvts.getParameter (processor.getModSlotParamId (slot, "amt")) : nullptr;
                expect (depth != nullptr, "the card's first row names its routing");

                if (depth != nullptr)
                {
                    struct GestureCounter : juce::AudioProcessorParameter::Listener
                    {
                        int begins = 0, ends = 0;
                        void parameterValueChanged (int, float) override {}
                        void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
                    } gestures;
                    depth->addListener (&gestures);

                    const auto row = card->getRowBounds (0).toFloat();
                    const auto from = row.getCentre();
                    const auto to = from.translated (25.0f, 0.0f);
                    const auto rowEvent = [&] (juce::Point<float> at, bool dragged, juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
                    {
                        const auto now = juce::Time::getCurrentTime();
                        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                 card, card, now, from, now, 1, dragged);
                    };
                    const auto read = [&] { return processor.readModSlot (slot); };
                    const auto start = read().depth;
                    // Away from the clamp, so the move shows.
                    processor.setModSlotValue (slot, "amt", 0.2f);
                    settle (80); // the card re-reads its rows each frame
                    auto& component = static_cast<juce::Component&> (*card);
                    clearHistory();
                    component.mouseDown (rowEvent (from, false));
                    component.mouseDrag (rowEvent (to, true));
                    component.mouseUp (rowEvent (to, true));
                    settle (50);
                    const auto dragSteps = undoSteps();
                    expect (dragSteps.size() == 1 && dragSteps[0].endsWith (" depth"),
                            "a card row drag is one undo step ('" + dragSteps.joinIntoString ("', '") + "')");
                    expect (std::abs (read().depth - 0.35f) < 0.02f && gestures.begins == 1 && gestures.ends == 1,
                            "dragging a card row sets its depth in one gesture (0.20 -> " + juce::String (read().depth, 2) + ")");
                    component.mouseDoubleClick (rowEvent (from, false));
                    expect (std::abs (read().depth) < 0.001f, "double-clicking a card row zeroes its depth");
                    depth->removeListener (&gestures);
                    processor.setModSlotValue (slot, "amt", start);

                    card->applyRowAction (0, ModHoverPopup::RowAction::toggleBypass);
                    expect (read().bypass, "the row menu bypasses the routing");
                    card->applyRowAction (0, ModHoverPopup::RowAction::toggleBypass);
                    expect (! read().bypass, "and turns it back on");

                    const auto rowsBefore = card->getNumRows();
                    const auto sourceBefore = read().source;
                    clearHistory();
                    card->applyRowAction (0, ModHoverPopup::RowAction::remove);
                    expect (read().source == Mod::Source::None && (card->getNumRows() == rowsBefore - 1 || ! card->isVisible()),
                            "the row menu removes the routing and the card follows");
                    const auto removeSteps = undoSteps();
                    processor.getUndoManager().undo();
                    expect (removeSteps.size() == 1 && removeSteps[0] == "Remove modulation" && read().source == sourceBefore,
                            "the card's Remove is one undo step and undo restores the routing");
                }

                modulated->closeModCard();
                processor.loadFactoryPreset (neuroWobble);
                settle (200);
            }
        }

        // DOCK keeps the browser at the side: the window grows, loading doesn't close it, the name toggles it, FLOAT returns it.
        if (auto* display = findChild<PresetDisplay> (*editor); display != nullptr && display->onClick != nullptr)
        {
            processor.loadFactoryPreset (0);
            const auto baseWidth = editor->getWidth();
            display->onClick();
            settle (400);
            auto* panel = findChild<PresetPanel> (*editor);
            expect (panel != nullptr && panel->onDockRequest != nullptr, "the browser has a DOCK key");

            if (panel != nullptr && panel->onDockRequest != nullptr)
            {
                panel->onDockRequest (true);
                settle (300);
                expect (panel->isDocked() && panel->isVisible() && editor->getWidth() == baseWidth + 340 && editor->getHeight() == 720,
                        "docking grows the window by the browser's column (" + juce::String (baseWidth) + " -> " + juce::String (editor->getWidth()) + ")");
                const auto start = processor.getCurrentPresetName();
                panel->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
                settle (150);
                panel->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
                settle (150);
                expect (processor.getCurrentPresetName() != start && panel->isVisible(),
                        "the docked browser loads with the arrows and stays open on Enter");
                display->onClick();
                settle (300);
                expect (! panel->isVisible() && editor->getWidth() == baseWidth, "the preset name closes the docked browser and the window shrinks back");
                display->onClick();
                settle (300);
                expect (panel->isVisible() && panel->isDocked() && editor->getWidth() == baseWidth + 340, "the preset name reopens it docked");
                panel->onDockRequest (false);
                settle (400);
                expect (! panel->isDocked() && panel->isOpen() && editor->getWidth() == baseWidth, "FLOAT returns it to the drop-down");
                panel->close();
                settle (400);
            }
        }

        // UI review 4 (V3, V15, V25, V31, S11, S28, S29): SAVE / SAVE AS, the
        // browser's sort, DX7 banks, stars and tags, the keys and the table
        // search. Saves go to a temporary folder, never the user's presets.
        {
            const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("ilanaUiTestPresets-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
            tempDir.createDirectory();
            IlanaSynthAudioProcessor::userPresetDirectoryOverride = tempDir;
            const auto countFiles = [&tempDir] { return tempDir.findChildFiles (juce::File::findFiles, true, "*.ilanapreset").size(); };
            const auto setCutoff = [&processor] (float hz)
            {
                if (auto* parameter = processor.apvts.getParameter ("f1_cutoff"))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (hz));
            };
            const auto cutoffIn = [&processor] (const juce::File& file)
            {
                IlanaSynthAudioProcessor other;
                other.loadPresetFromFile (file);
                return other.apvts.getRawParameterValue ("f1_cutoff")->load();
            };

            processor.loadFactoryPreset (neuroWobble);
            settle (200);
            auto& saveOverlay = pages->getSaveOverlay();
            pages->savePreset();
            settle (100);
            expect (saveOverlay.isShowing(), "SAVE on a factory preset opens the SAVE AS panel");

            saveOverlay.getNameField().setText (juce::String::fromUTF8 ("Rock'n (Roll): \xc3\xb1/1?"), true);
            settle (100);
            expect (saveOverlay.getNote().contains (":") && saveOverlay.getNote().contains ("/") && saveOverlay.getNote().contains ("?"),
                    "SAVE AS says which characters a file name can't hold (" + saveOverlay.getNote() + ")");
            saveOverlay.getTagsField().setText ("dark, wide", false);
            setCutoff (900.0f);
            saveOverlay.save();
            settle (100);
            const auto savedName = juce::String::fromUTF8 ("Rock'n (Roll) \xc3\xb1" "1");
            const auto savedFile = tempDir.getChildFile (savedName + ".ilanapreset");
            expect (! saveOverlay.isShowing() && savedFile.existsAsFile() && processor.getCurrentPresetName() == savedName,
                    "SAVE AS keeps apostrophes, brackets and accents, dropping only : / ? (" + processor.getCurrentPresetName() + ")");
            expect (! pages->isPatchEdited(), "a saved patch is no longer EDITED");

            setCutoff (2400.0f);
            pages->savePreset();
            settle (100);
            auto* display = findChild<PresetDisplay> (*editor);
            expect (! saveOverlay.isShowing() && countFiles() == 1 && std::abs (cutoffIn (savedFile) - 2400.0f) < 5.0f,
                    "SAVE overwrites the loaded user preset in place, with no dialog");
            expect (display != nullptr && display->getNotice() == "SAVED", "SAVE says SAVED by the preset name");

            setCutoff (3100.0f);
            editor->keyPressed (juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0));
            settle (100);
            expect (! saveOverlay.isShowing() && countFiles() == 1 && std::abs (cutoffIn (savedFile) - 3100.0f) < 5.0f, "Ctrl+S saves in place");

            editor->keyPressed (juce::KeyPress ('s', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0));
            settle (100);
            expect (saveOverlay.isShowing(), "Ctrl+Shift+S opens SAVE AS");
            saveOverlay.getNameField().setText (savedName, true);
            setCutoff (500.0f);
            saveOverlay.save();
            expect (saveOverlay.isAskingOverwrite() && countFiles() == 1, "SAVE AS asks before overwriting a name in use");
            saveOverlay.finishOverwrite (false);
            expect (saveOverlay.isShowing() && ! saveOverlay.isAskingOverwrite() && std::abs (cutoffIn (savedFile) - 3100.0f) < 5.0f,
                    "Cancel keeps the old preset and goes back to the name");
            saveOverlay.save();
            saveOverlay.finishOverwrite (true);
            settle (100);
            expect (! saveOverlay.isShowing() && countFiles() == 1 && std::abs (cutoffIn (savedFile) - 500.0f) < 5.0f,
                    "Overwrite replaces it, with no \"Name 2\" copy");

            pages->savePresetAs();
            saveOverlay.getNameField().setText ("neuro wobble", true);
            saveOverlay.save();
            expect (saveOverlay.isShowing() && ! saveOverlay.isAskingOverwrite() && saveOverlay.getNote().contains ("factory") && countFiles() == 1,
                    "a factory preset's name can't be taken");
            editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            expect (! saveOverlay.isShowing(), "Esc closes the SAVE AS panel");

            // Ctrl+Right / Left step through the presets, through the
            // confirm over an edited patch.
            processor.loadFactoryPreset (neuroWobble);
            settle (200);
            editor->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys::commandModifier, 0));
            settle (150);
            const auto afterRight = processor.getCurrentPresetName();
            editor->keyPressed (juce::KeyPress (juce::KeyPress::leftKey, juce::ModifierKeys::commandModifier, 0));
            settle (150);
            expect (afterRight != "Neuro Wobble" && processor.getCurrentPresetName() == "Neuro Wobble",
                    "Ctrl+Right / Ctrl+Left load the next and previous preset (" + afterRight + ")");
            pages->setAsksBeforeReplacingEdits (true);
            setCutoff (700.0f);
            settle (100);
            editor->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys::commandModifier, 0));
            settle (100);
            auto* confirm = findChild<ConfirmOverlay> (*editor);
            expect (confirm != nullptr && confirm->isAsking() && processor.getCurrentPresetName() == "Neuro Wobble",
                    "Ctrl+Right over an edited patch asks first");
            if (confirm != nullptr && confirm->isAsking())
                confirm->finish (false);
            pages->setAsksBeforeReplacingEdits (false);
            processor.loadFactoryPreset (neuroWobble);
            settle (150);

            // Digits pick tabs 1-9; there is no tenth tab for 0.
            if (auto* tabbed = findChild<juce::TabbedComponent> (*editor))
            {
                const auto before = tabbed->getCurrentTabIndex();
                editor->keyPressed (juce::KeyPress ('2'));
                const auto onTwo = tabbed->getCurrentTabIndex();
                expect (onTwo == 1 && ! editor->keyPressed (juce::KeyPress ('0')) && tabbed->getCurrentTabIndex() == 1,
                        "2 picks the second tab and 0 does nothing");
                tabbed->setCurrentTabIndex (before);
                settle (150);
            }

            // Esc closes the drop-down browser.
            if (display != nullptr && display->onClick != nullptr)
            {
                display->onClick();
                settle (400);
                auto* panel = findChild<PresetPanel> (*editor);
                expect (panel != nullptr && panel->isOpen(), "the browser opens for the browser checks");

                if (panel != nullptr)
                {
                    const auto sortBefore = panel->getSortMode();
                    const auto dx7Before = panel->isShowingDx7();
                    panel->setSortMode (PresetPanel::sortByName);
                    panel->setShowDx7 (false);
                    panel->selectFilter ("");
                    auto listed = panel->getListedNames();
                    auto sorted = true;
                    for (int i = 1; i < listed.size(); ++i)
                        sorted = sorted && listed[i - 1].compareNatural (listed[i], false) <= 0;
                    auto dx7Shown = 0;
                    for (const auto& name : listed)
                        dx7Shown += name.endsWith ("(ROM1A)") || name.endsWith ("(DEXED01)") ? 1 : 0;
                    expect (sorted && listed.size() > 100, "the browser sorts by name (" + listed[0] + ", " + listed[1] + ", " + listed[2] + "...)");
                    expect (dx7Shown == 0 && listed.contains (savedName), "All leaves the DX7 ROM voices out by default ("
                                                                          + juce::String (listed.size()) + " listed)");

                    panel->selectFilter ("DX7");
                    const auto chipKeys = panel->getChipKeys();
                    expect (panel->getListedNames().size() >= 288 && chipKeys.contains ("bank:ROM1A") && chipKeys.contains ("bank:DEXED01"),
                            "DX7 lists its voices with a chip per bank (" + chipKeys.joinIntoString (" ") + ")");
                    panel->clickChip ("bank:ROM1B");
                    listed = panel->getListedNames();
                    auto allRom1B = listed.size() == 32;
                    for (const auto& name : listed)
                        allRom1B = allRom1B && name.endsWith ("(ROM1B)");
                    expect (allRom1B, "the ROM1B chip lists that bank's 32 voices");

                    panel->setShowDx7 (true);
                    panel->selectFilter ("");
                    expect (panel->getListedNames().size() > listed.size() + 288, "Show DX7 voices puts them back into All");
                    panel->setShowDx7 (false);

                    panel->setSortMode (PresetPanel::sortByCategory);
                    listed = panel->getListedNames();
                    const auto categories = processor.getAllPresetCategories();
                    const auto allNames = processor.getAllPresetNames();
                    expect (categories[allNames.indexOf (listed[1])] == "Bass", "SORT: CATEGORY puts the basses first after Init (" + listed[1] + ")");
                    panel->setSortMode (PresetPanel::sortByName);

                    // The star on a row toggles the favourite without loading it.
                    listed = panel->getListedNames();
                    const auto row = listed.indexOf ("Metal Pad");
                    panel->clickRow (row, 12);
                    panel->selectFilter ("*fav");
                    const auto starred = panel->getListedNames().contains ("Metal Pad");
                    panel->selectFilter ("");
                    panel->clickRow (panel->getListedNames().indexOf ("Metal Pad"), 12);
                    panel->selectFilter ("*fav");
                    const auto unstarred = ! panel->getListedNames().contains ("Metal Pad");
                    expect (row >= 0 && starred && unstarred && processor.getCurrentPresetName() == "Neuro Wobble",
                            "a row's star toggles the favourite (and doesn't load the preset)");

                    // Tags saved with user presets become chips.
                    panel->selectFilter ("*user");
                    expect (panel->getChipKeys().contains ("tag:dark") && panel->getChipKeys().contains ("tag:wide"),
                            "user preset tags show as chips (" + panel->getChipKeys().joinIntoString (" ") + ")");
                    panel->clickChip ("tag:dark");
                    expect (panel->getListedNames().size() == 1, "a tag chip filters the list");
                    panel->clickChip ("tag:dark");

                    panel->selectFilter ("");
                    panel->setSortMode (sortBefore);
                    panel->setShowDx7 (dx7Before);
                    editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
                    settle (400);
                    expect (! panel->isOpen(), "Esc closes the browser");
                }
            }

            // The wavetable browser: a search field, spaced Title Case names.
            {
                TableBrowser browser (processor, "osc1_table", IlanaTheme::accent());
                browser.setSize (740, 520);
                const auto all = browser.getShownNames();
                browser.setSearchText ("saw");
                settle (100);
                const auto saws = browser.getShownNames();
                auto allSaws = saws.size() > 2 && saws.size() < all.size();
                for (const auto& name : saws)
                    allSaws = allSaws && name.containsIgnoreCase ("saw");
                expect (allSaws && saws.contains ("Drive Saw") && saws.contains ("Analog Saw"),
                        "the table search filters (" + saws.joinIntoString (", ") + ")");
                expect (all.contains ("Hard Sync") && ! all.contains ("DriveSaw") && ! all.contains ("Basic"),
                        "table names show as spaced Title Case, ANALOG's Basic as Analog Saw");
            }

            IlanaSynthAudioProcessor::userPresetDirectoryOverride = juce::File();
            tempDir.deleteRecursively();
            processor.loadFactoryPreset (neuroWobble);
            settle (200);
        }

        // The header's live waveform and the VOICES hotspot.
        {
            auto* scope = findChild<OutputView> (*editor);
            std::vector<OutputView*> views;
            findAll<OutputView> (*editor, views);
            auto strips = 0;
            for (auto* view : views)
                strips += view->isStrip() && visibleInTree (view) && view->getWidth() > 100 ? 1 : 0;
            juce::ignoreUnused (scope);
            expect (strips == 1, "the header shows one live waveform strip");
        }
    }

    // Modulation rings: every knob on every page that drives a destination
    // shows that destination's live depth. Each destination is routed from a
    // macro at full travel, a note is played, and the depth the ring reads
    // must move.
    if (pages != nullptr)
    {
        std::map<int, juce::String> destinations;
        juce::StringArray ringless;
        for (const auto& id : pages->getPageIds())
        {
            pages->showPage (id);
            settle (60);
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
            {
                if (knob->getRingDestination() > 0)
                    destinations.emplace (knob->getRingDestination(), knob->getParameterId());
                else if (knob->getParameterId().isNotEmpty())
                    ringless.addIfNotAlreadyThere (knob->getParameterId());
            }
        }
        pages->showPage ("MAIN");

        juce::StringArray silent;
        for (const auto& [destination, id] : destinations)
        {
            IlanaSynthAudioProcessor probe;
            probe.prepareToPlay (48000.0, 512);
            if (auto* macro = probe.apvts.getParameter ("macro1"))
                macro->setValueNotifyingHost (1.0f);
            probe.assignModSlot ((int) Mod::Source::Macro1, destination, 0.5f);
            juce::AudioBuffer<float> buffer (2, 512);
            for (int block = 0; block < 3; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                buffer.clear();
                probe.processBlock (buffer, midi);
            }
            if (std::abs (probe.getModDisplay (destination)) < 0.001f)
                silent.add (id);
        }
        expect (silent.isEmpty(), "every modulatable knob's ring shows its depth (" + juce::String ((int) destinations.size())
                                      + " destinations" + (silent.isEmpty() ? juce::String() : "; silent: " + silent.joinIntoString (", ")) + ")");
        if (juce::SystemStats::getEnvironmentVariable ("ILANA_RING_REPORT", "").isNotEmpty())
            std::cout << "knobs without a ring: " << ringless.joinIntoString (", ") << std::endl;
    }

    // UI review 4, batch C (V4, V10, V12, V29, S5, S7, S19, S21, S22):
    // LFOs and envelopes.
    {
        processor.loadFactoryPreset (neuroWobble);
        settle (200);

        const auto event = [] (juce::Component& component, juce::Point<float> position, bool dragged)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys(),
                                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(),
                                     position, juce::Time::getCurrentTime(), 1, dragged);
        };
        const auto readShape = [&processor] (int lfo)
        {
            return juce::roundToInt (processor.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "_shape")->load());
        };
        const auto setShape = [&processor] (int lfo, int shape)
        {
            auto* parameter = processor.apvts.getParameter ("lfo" + juce::String (lfo + 1) + "_shape");
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) shape));
        };
        const auto clearHistory = [&processor]
        {
            processor.apvts.copyState();
            processor.getUndoManager().clearUndoHistory();
        };
        const auto undoSteps = [&processor]
        {
            processor.apvts.copyState();
            return processor.getUndoManager().getUndoDescriptions();
        };

        // The first drag on a preset wave makes it a Curve with the wave's
        // points, as one undo step; a plain click changes nothing.
        {
            setShape (0, LfoShapes::Triangle);
            const auto curveBefore = processor.getLfoCurve (0).toString();
            LfoDisplay lfoDisplay (processor, 0);
            juce::Component& display = lfoDisplay;
            display.setSize (300, 150);
            clearHistory();
            display.mouseDown (event (display, { 80.0f, 110.0f }, false));
            display.mouseUp (event (display, { 80.0f, 110.0f }, false));
            const auto clickChanged = readShape (0) != LfoShapes::Triangle || ! undoSteps().isEmpty();

            display.mouseDown (event (display, { 80.0f, 110.0f }, false));
            display.mouseDrag (event (display, { 90.0f, 70.0f }, true));
            display.mouseUp (event (display, { 90.0f, 70.0f }, true));
            const auto converted = processor.getLfoCurve (0);
            const auto steps = undoSteps();
            const auto shapeAfter = readShape (0);
            processor.getUndoManager().undo();
            const auto undoneShape = readShape (0);
            const auto undoneCurve = processor.getLfoCurve (0).toString();
            processor.getUndoManager().redo();

            expect (! clickChanged, "a click on a Triangle LFO changes nothing");
            expect (shapeAfter == IlanaSynthAudioProcessor::curveShape && steps.size() == 1 && steps[0] == "LFO 1 curve"
                        && std::abs (converted.valueAt (0.5) - 1.0f) < 0.02f && std::abs (converted.valueAt (0.75)) < 0.05f
                        && std::abs (converted.valueAt (0.0) + 1.0f) < 0.02f,
                    "dragging a Triangle LFO turns it into a Curve with the triangle's points in one undo step ('"
                        + steps.joinIntoString ("', '") + "', peak " + juce::String (converted.valueAt (0.5), 2) + ")");
            expect (undoneShape == LfoShapes::Triangle && undoneCurve == curveBefore && readShape (0) == IlanaSynthAudioProcessor::curveShape,
                    "undo puts the Triangle and the old curve back, redo the converted curve");
            expect (lfoDisplay.getGridDivisions() == 8, "the LFO graph shows its snap grid control (GRID 8)");
            setShape (0, LfoShapes::Triangle);
        }

        // STEPS & MSEG: a row whose LFO plays another shape offers "Use on
        // LFO n", which sets it to Steps as one undo step.
        {
            pages->showPage ("STEPS");
            settle (400);
            juce::TextButton* use = nullptr;
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*editor, buttons);
            for (auto* button : buttons)
                if (button->getButtonText().startsWith ("Use on LFO ") && visibleInTree (button) && button->getWidth() > 20)
                    use = use == nullptr ? button : use;

            expect (use != nullptr, "STEPS & MSEG offers a 'Use on LFO n' button for a row whose LFO isn't playing Steps");

            if (use != nullptr)
            {
                const auto lfo = use->getButtonText().fromLastOccurrenceOf (" ", false, false).getIntValue() - 1;
                const auto before = readShape (lfo);
                clearHistory();
                use->triggerClick();
                settle (300);
                const auto steps = undoSteps();
                const auto after = readShape (lfo);
                const auto hidden = ! use->isVisible();
                processor.getUndoManager().undo();
                expect (after == LfoShapes::Steps && steps.size() == 1 && steps[0] == "LFO " + juce::String (lfo + 1) + " shape"
                            && hidden && readShape (lfo) == before,
                        "'Use on LFO " + juce::String (lfo + 1) + "' sets its shape to Steps in one undo step ('"
                            + steps.joinIntoString ("', '") + "'), the button goes, undo restores it");
            }
        }

        // One RATE knob: while synced it shows the division, on MOD and PLAY.
        {
            const auto findKnob = [&editor] (const juce::String& id) -> KnobControl*
            {
                std::vector<KnobControl*> knobs;
                findAll<KnobControl> (*editor, knobs);
                for (auto* knob : knobs)
                    if (knob->getParameterId() == id && visibleInTree (knob) && knob->getWidth() > 0)
                        return knob;
                return nullptr;
            };

            for (const auto* page : { "ENV/LFO", "MAIN" })
            {
                pages->showPage (page);
                settle (300);

                // Whichever LFO the page shows.
                auto lfo = -1;
                for (int index = 0; index < IlanaSynthAudioProcessor::numLfos && lfo < 0; ++index)
                    if (findKnob ("lfo" + juce::String (index + 1) + "_rate") != nullptr || findKnob ("lfo" + juce::String (index + 1) + "_div") != nullptr)
                        lfo = index;

                const auto prefix = "lfo" + juce::String (lfo + 1);
                auto* sync = processor.apvts.getParameter (prefix + "_sync");
                auto* divParam = processor.apvts.getParameter (prefix + "_div");

                if (lfo < 0 || sync == nullptr || divParam == nullptr)
                {
                    expect (false, juce::String (page) + ": an LFO RATE knob is on the page");
                    continue;
                }

                const auto syncBefore = sync->getValue();
                sync->setValueNotifyingHost (1.0f);
                settle (300);
                const auto divText = divParam->getText (divParam->getValue(), 0);
                auto* division = findKnob (prefix + "_div");
                const auto shownText = division != nullptr ? division->getSlider().getTextFromValue (division->getSlider().getValue()) : juce::String();
                expect (division != nullptr && division->getLabelText() == "RATE" && shownText == divText && findKnob (prefix + "_rate") == nullptr,
                        juce::String (page) + ": a synced LFO's RATE knob shows the division ('" + shownText + "', want '" + divText + "')");

                sync->setValueNotifyingHost (0.0f);
                settle (300);
                expect (findKnob (prefix + "_rate") != nullptr && findKnob (prefix + "_div") == nullptr,
                        juce::String (page) + ": free-running, RATE is the Hz knob again");
                sync->setValueNotifyingHost (syncBefore);
                settle (100);
            }
        }

        // The pools' index rows: one click opens any LFO or envelope, adding
        // it to the patch when needed.
        {
            pages->showPage ("ENV/LFO");
            settle (300);
            std::vector<PoolIndexRow*> rows;
            findAll<PoolIndexRow> (*editor, rows);
            rows.erase (std::remove_if (rows.begin(), rows.end(), [] (PoolIndexRow* row) { return ! visibleInTree (row) || row->getWidth() < 100; }),
                        rows.end());
            std::sort (rows.begin(), rows.end(), [] (PoolIndexRow* a, PoolIndexRow* b) { return a->getScreenY() < b->getScreenY(); });
            expect (rows.size() == 2 && rows[0]->getCount() == 16 && rows[1]->getCount() == 16,
                    "ENV / LFO shows a 1-16 index row above the LFO cards and above the envelope cards");

            if (rows.size() == 2)
            {
                using M = IlanaSynthAudioProcessor::Module;
                const auto lfoShownBefore = processor.isLfoShown (9);
                rows[0]->pick (9);
                settle (100);
                std::vector<LfoDisplay*> lfoDisplays;
                findAll<LfoDisplay> (*pages->getCurrentPage(), lfoDisplays);
                auto shownDisplays = 0;
                for (auto* display : lfoDisplays)
                    shownDisplays += visibleInTree (display) ? 1 : 0;
                expect (! lfoShownBefore && processor.isLfoShown (9) && rows[0]->getSelected() == 9 && rows[0]->getState (9).shown
                            && shownDisplays == 1,
                        "clicking 10 in the LFO row adds LFO 10 and opens it");

                const auto envShownBefore = processor.isRevealed (M::Envelope, 12);
                rows[1]->pick (12);
                settle (100);
                expect (! envShownBefore && processor.isRevealed (M::Envelope, 12) && rows[1]->getSelected() == 12
                            && rows[0]->getState (0).inUse && rows[1]->getState (0).inUse,
                        "clicking 13 in the envelope row adds ENV 13 and opens it; LFO 1 and the amp envelope show as in use");

                processor.setRevealed (M::Lfo, 9, false);
                processor.setRevealed (M::Envelope, 12, false);
                rows[0]->pick (0);
                rows[1]->pick (0);
                settle (100);
            }
        }

        // The envelope graph's playhead rides the curve for the playing note
        // and goes when it ends.
        {
            EnvelopeDisplay display (processor, "amp");
            display.setSize (400, 160);
            juce::AudioBuffer<float> buffer (2, 512);
            const auto run = [&processor, &buffer] (int blocks, bool noteOn, bool noteOff)
            {
                for (int block = 0; block < blocks; ++block)
                {
                    juce::MidiBuffer midi;
                    if (block == 0 && noteOn)
                        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                    if (block == 0 && noteOff)
                        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                    buffer.clear();
                    processor.processBlock (buffer, midi);
                }
            };
            run (40, true, false); // 0.4 s: past Neuro Wobble's 5 ms attack and 300 ms decay
            const auto held = display.playheadPoint();
            const auto stage = (int) processor.getEnvMonitorPosition (0);
            run (60, false, true); // past the 50 ms release
            const auto after = display.playheadPoint();
            expect (held.has_value() && stage == 4 && ! after.has_value(),
                    "the envelope's playhead sits on the sustain while a note is held (stage " + juce::String (stage)
                        + ") and goes once it has released");
        }
    }

    // UI review 4, batch E: oscillators, filter, PLAY.
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
        const auto findKnob = [&editor] (const juce::String& id) -> KnobControl*
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
                if (knob->getParameterId() == id && visibleInTree (knob) && knob->getWidth() > 0)
                    return knob;
            return nullptr;
        };
        const auto loadNamed = [&processor] (const juce::String& name)
        {
            processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (name));
            settle (500);
        };
        const juce::StringArray presets { "Init", "Neuro Wobble", "E.PIANO 1 (ROM1A)" };

        // No card is cut mid-knob: every visible knob on PLAY and OSC is
        // wholly inside each of its ancestors (the scrolling viewports
        // included) or wholly out of view, at 100 % and 75 % (V13, S8).
        const auto clippedKnobs = [&editor] (juce::StringArray& names)
        {
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            for (auto* knob : knobs)
            {
                if (! visibleInTree (knob) || knob->getWidth() <= 0 || knob->getHeight() <= 0)
                    continue;
                // The dial and its value: the label above may tuck under a header.
                const auto area = editor->getLocalArea (knob, knob->getLocalBounds().withTrimmedTop (13));
                for (auto* parent = knob->getParentComponent(); parent != nullptr && parent != editor.get(); parent = parent->getParentComponent())
                {
                    const auto view = editor->getLocalArea (parent, parent->getLocalBounds());
                    if (view.intersects (area) && ! view.contains (area))
                    {
                        names.add (knob->getParameterId());
                        break;
                    }
                }
            }
        };

        for (const auto width : { 1060, 795 })
        {
            editor->setSize (width, width * 720 / 1060);
            settle (300);
            for (const auto& preset : presets)
            {
                loadNamed (preset);
                for (const auto* page : { "MAIN", "OSC" })
                {
                    pages->showPage (page);
                    settle (400);
                    juce::StringArray clipped;
                    clippedKnobs (clipped);
                    expect (clipped.isEmpty() && processor.getFactoryPresetNames().contains (preset),
                            juce::String (page) + " at " + juce::String (width) + " px, " + preset + ": no knob is cut by its card or the page"
                                + (clipped.isEmpty() ? juce::String() : " (cut: " + clipped.joinIntoString (", ") + ")"));
                }
            }
        }
        editor->setSize (1060, 720);
        settle (300);

        // The DX7 voice's six oscillators don't fit open: the lower ones fold
        // to their titles; a click on one opens it (another folds instead),
        // still with no knob cut.
        loadNamed ("E.PIANO 1 (ROM1A)");
        for (const auto* page : { "MAIN", "OSC" })
        {
            pages->showPage (page);
            settle (400);
            const auto osc4FoldedBefore = findKnob ("osc4_level") == nullptr && toggleFor ("osc4_on") != nullptr;
            clickCardTitle ("osc4_on");
            juce::StringArray clipped;
            clippedKnobs (clipped);
            expect (osc4FoldedBefore && findKnob ("osc4_level") != nullptr && findKnob ("osc1_level") != nullptr && clipped.isEmpty(),
                    juce::String (page) + ": DX7 OSC 4 is folded to fit; a click on its title opens it, nothing cut ("
                        + clipped.joinIntoString (", ") + ")");
        }

        // No remove button on an oscillator's title (S16): it is on the
        // title's right-click menu.
        loadNamed ("Neuro Wobble");
        for (const auto* page : { "MAIN", "OSC" })
        {
            pages->showPage (page);
            settle (300);
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*editor, buttons);
            auto crosses = 0;
            for (auto* button : buttons)
                crosses += visibleInTree (button) && (button->getButtonText() == juce::String (juce::CharPointer_UTF8 ("\xc3\x97"))
                                                      || button->getButtonText() == "x") ? 1 : 0;
            expect (crosses == 0, juce::String (page) + ": no remove button on the oscillator titles");
        }

        // The filter graph's markers: the set cutoff's, solid, inside the
        // plot and apart when both sit at 20 kHz; a click on one picks that
        // filter (V11).
        loadNamed ("Init");
        pages->showPage ("MAIN");
        settle (300);
        if (auto* display = findChild<FilterDisplay> (*editor); display != nullptr && visibleInTree (display))
        {
            const auto markers = display->getMarkerCentres();
            const auto plot = display->getLocalBounds().toFloat().reduced (10.0f, 12.0f);
            expect (plot.contains (markers[0]) && plot.contains (markers[1]) && markers[0].getDistanceFrom (markers[1]) >= 10.0f
                        && display->filterAt (markers[0]) == 0 && display->filterAt (markers[1]) == 1,
                    "Init: both filter markers are inside the graph, apart, and each picks its own filter");
        }
        else
            expect (false, "PLAY shows the filter graph");

        // PLAY's filter TYPE reads like FILTER's (its short names), SLOPE is
        // FILTER's 12 / 24 dB switch (S10).
        {
            std::vector<ComboControl*> combos;
            findAll<ComboControl> (*editor, combos);
            auto typeText = juce::String();
            for (auto* combo : combos)
                if (visibleInTree (combo) && combo->getComboBox().getNumItems() == FilterTypeGrid::shortNames().size()
                    && combo->getComboBox().getItemText (0) == FilterTypeGrid::shortNames()[0])
                    typeText = combo->getComboBox().getText();
            std::vector<SlopeSwitch*> slopes;
            findAll<SlopeSwitch> (*editor, slopes);
            auto slopeShown = false;
            for (auto* slope : slopes)
                slopeShown = slopeShown || visibleInTree (slope);
            expect (typeText.isNotEmpty() && typeText == FilterTypeGrid::shortNames()[juce::roundToInt (readParam ("f1_type"))] && slopeShown,
                    "PLAY's filter TYPE shows FILTER's name ('" + typeText + "') and SLOPE is the 12 / 24 dB switch");
        }

        // The WAVE view: the frame as a readout and scrubber (V12).
        pages->showPage ("OSC");
        settle (300);
        {
            std::vector<WaveDisplay*> waves;
            findAll<WaveDisplay> (*editor, waves);
            auto readout = juce::String();
            for (auto* wave : waves)
                if (visibleInTree (wave) && readout.isEmpty())
                    readout = wave->getFrameReadout();
            expect (readout.startsWith ("FRAME ") && readout.contains (" / "), "the WAVE view reads out the frame ('" + readout + "')");
        }

        // One dimming rule (V26): a control that does nothing now dims, says
        // why on hover, and still takes edits.
        {
            auto* spectral = findKnob ("osc1_spectral_amt");
            const auto dimmed = spectral != nullptr && std::abs (spectral->getAlpha() - IlanaTheme::dimmedAlpha) < 0.01f
                                && spectral->isEnabled() && spectral->getSlider().getTooltip().contains ("No effect now");
            setParam ("osc1_spectral", 1.0f);
            settle (500);
            const auto lit = spectral != nullptr && spectral->getAlpha() > 0.99f && ! spectral->getSlider().getTooltip().contains ("No effect now");
            setParam ("osc1_spectral", 0.0f);
            settle (300);
            auto* detune = findKnob ("osc1_detune");
            expect (dimmed && lit && detune != nullptr && detune->getAlpha() < 0.99f,
                    "OSC: SPEC AMT dims (and says why) while SPECTRAL is Off, lights when it is on; DETUNE dims at UNISON 1");

            pages->showPage ("FILTER");
            settle (400);
            auto* balance = findKnob ("filter_balance");
            const auto serialDim = balance != nullptr && balance->getAlpha() < 0.99f;
            setParam ("filters_parallel", 1.0f);
            settle (400);
            expect (serialDim && balance != nullptr && balance->getAlpha() > 0.99f, "FILTER: BALANCE dims in serial, not in parallel");
            setParam ("filters_parallel", 0.0f);
            settle (200);
        }

        // Three knob sizes by role (V22): FM sends are mini, OSC's core
        // knobs as large as PLAY's.
        {
            pages->showPage ("FM");
            settle (400);
            std::vector<KnobControl*> knobs;
            findAll<KnobControl> (*editor, knobs);
            auto sends = 0, miniSends = 0;
            for (auto* knob : knobs)
                if (visibleInTree (knob) && knob->getParameterId().startsWith ("fm_") && knob->getLabelText().isEmpty())
                {
                    ++sends;
                    miniSends += knob->getMaxDial() == IlanaTheme::KnobSize::mini ? 1 : 0;
                }
            expect (sends > 0 && sends == miniSends, "FM: the matrix's send knobs are mini (" + juce::String (miniSends) + " of " + juce::String (sends) + ")");

            pages->showPage ("MAIN");
            settle (300);
            auto* playLevel = findKnob ("osc1_level");
            const auto playDial = playLevel != nullptr ? playLevel->getDialSize() : 0;
            pages->showPage ("OSC");
            settle (300);
            auto* oscLevel = findKnob ("osc1_level");
            expect (playDial > 0 && oscLevel != nullptr && oscLevel->getDialSize() >= playDial,
                    "OSC's LEVEL dial (" + juce::String (oscLevel != nullptr ? oscLevel->getDialSize() : 0) + " px) is not smaller than PLAY's ("
                        + juce::String (playDial) + " px)");
        }

        // The physical card: LOAD .WAV only for a wavetable (V30).
        {
            setParam ("osc1_mode", 1.0f);
            settle (500);
            std::vector<juce::TextButton*> buttons;
            findAll<juce::TextButton> (*editor, buttons);
            auto loadShown = false;
            for (auto* button : buttons)
                loadShown = loadShown || (visibleInTree (button) && button->getButtonText().startsWith ("LOAD"));
            expect (! loadShown && findKnob ("osc1_string_decay") != nullptr && findKnob ("osc1_unison") != nullptr,
                    "a Physical card has no LOAD .WAV, and shows its string and voice rows");
            setParam ("osc1_mode", 0.0f);
            settle (300);
        }

        // VECTOR: a corner whose oscillator is off says so (S27).
        {
            pages->showPage ("VECTOR");
            settle (300);
            auto* pad = findChild<VectorPadDisplay> (*editor);
            auto offCorner = -1;
            for (int corner = 0; corner < 4 && pad != nullptr; ++corner)
                if (! pad->isCornerSounding (corner))
                    offCorner = corner;
            expect (pad != nullptr && offCorner >= 0 && pad->getCornerLabel (offCorner, 0.5f).endsWith ("(off)")
                        && pad->isCornerSounding (0) && ! pad->getCornerLabel (0, 0.5f).contains ("(off)"),
                    "VECTOR: Init's corners on switched-off oscillators read '(off)', OSC 1's does not");
        }

        // The synced RATE knob takes modulation for RATE (V14).
        {
            pages->showPage ("ENV/LFO");
            settle (300);
            setParam ("lfo1_sync", 1.0f);
            settle (400);
            auto* division = findKnob ("lfo1_div");
            const juce::DragAndDropTarget::SourceDetails drag (juce::var ("modsource:0"), nullptr, {});
            expect (division != nullptr && division->getRingDestination() == (int) Mod::Destination::Lfo1Rate
                        && division->isInterestedInDragSource (drag),
                    "a synced LFO's RATE knob shows RATE's modulation and takes a dropped source");
            setParam ("lfo1_sync", 0.0f);
            settle (200);
        }
        loadNamed ("Neuro Wobble");
    }

    pages->setAsksBeforeReplacingEdits (askedBefore);
    editor.reset();
    std::cout << (uiFailures == 0 ? "UI TESTS PASSED" : "UI TESTS FAILED") << " (" << uiFailures << " failures)" << std::endl;
    return uiFailures == 0 ? 0 : 1;
}
} // namespace

// The editor on screen, idle, on every page: how much of a core the UI
// spends repainting when nothing moves (animations must settle).
#include <ctime>
#if JUCE_WINDOWS
 #define NOMINMAX
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif

// Process CPU time in seconds (std::clock is wall time on Windows).
double cpuSeconds()
{
   #if JUCE_WINDOWS
    FILETIME created, exited, kernel, user;
    GetProcessTimes (GetCurrentProcess(), &created, &exited, &kernel, &user);
    const auto toSeconds = [] (const FILETIME& t) { return (double) (((unsigned long long) t.dwHighDateTime << 32) | t.dwLowDateTime) * 1.0e-7; };
    return toSeconds (kernel) + toSeconds (user);
   #else
    return (double) std::clock() / CLOCKS_PER_SEC;
   #endif
}

// An opaque window holding the editor, as hosts do: a non-opaque editor put
// on the desktop directly gets a layered window that redraws in full.
struct HostWindow : juce::Component
{
    explicit HostWindow (juce::Component& editor)
    {
        setOpaque (true);
        addAndMakeVisible (editor);
        setSize (editor.getWidth(), editor.getHeight());
        addToDesktop (juce::ComponentPeer::windowHasTitleBar);
        setVisible (true);
        toFront (true); // a background window may get fewer vblanks
    }
    ~HostWindow() override { removeFromDesktop(); }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
};

// Records what gets repainted: a transparent component over the editor
// sees the clip of every paint.
struct RegionProbe : juce::Component
{
    RegionProbe() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        ++frames;
        // Which 40-px cells the clip region touches (the clip's bounds
        // alone can be the whole window).
        for (int y = 0; y < getHeight(); y += 40)
            for (int x = 0; x < getWidth(); x += 40)
                if (g.clipRegionIntersects ({ x, y, 40, 40 }))
                    ++counts[juce::Rectangle<int> (x, y, 40, 40).toString()];
    }
    int frames = 0;
    std::map<juce::String, int> counts;
};

int runIdleCpu()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
    HostWindow window (*editor);
    settle (800);

    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);

    // Listing regions needs exact clips: Direct2D's clip tests are coarse.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_IDLE_REGIONS", "").isNotEmpty())
        if (auto* peer = window.getPeer())
            peer->setCurrentRenderingEngine (0);

    auto* pages = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
    auto worst = 0.0;

    // ILANA_IDLE_HIDE=LogoComponent,ModSourceChip,... hides every component
    // of those classes (by their type name) to find what keeps repainting.
    const auto hide = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_IDLE_HIDE", ""), ",", "");
    if (! hide.isEmpty())
    {
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                const juce::String type (typeid (*child).name());
                for (const auto& h : hide)
                    if (h.isNotEmpty() && type.contains (h))
                        child->setVisible (false);
                walk (*child);
            }
        };
        walk (*editor);
    }

    const auto only = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_FPS_PAGES", ""), ",", "");

    for (const auto& id : pages->getPageIds())
    {
        if (! only.isEmpty() && ! only.contains (id))
            continue;
        pages->showPage (id);
        settle (1200); // let entrance animations finish
        RegionProbe regions;
        const auto listRegions = juce::SystemStats::getEnvironmentVariable ("ILANA_IDLE_REGIONS", "").isNotEmpty();
        if (listRegions)
        {
            editor->addAndMakeVisible (regions);
            regions.setBounds (editor->getLocalBounds());
        }
        const auto start = cpuSeconds();
        settle (3000);
        const auto percent = 100.0 * (cpuSeconds() - start) / 3.0;
        worst = juce::jmax (worst, percent);
        std::cout << "idle " << id << ": " << juce::String (percent, 1) << "% of a core" << std::endl;
        if (listRegions)
        {
            editor->removeChildComponent (&regions);
            std::vector<std::pair<int, juce::String>> sorted;
            for (auto& [rect, n] : regions.counts)
                sorted.push_back ({ n, rect });
            std::sort (sorted.rbegin(), sorted.rend());
            std::cout << "  " << regions.frames << " paints in 3 s" << std::endl;
            for (auto* top : editor->getChildren())
                for (auto* child : top->getChildren())
                    if (child->isVisible())
                        std::cout << "    child " << typeid (*child).name() << " " << child->getBounds().toString() << std::endl;
            for (size_t i = 0; i < juce::jmin<size_t> (40, sorted.size()); ++i)
            {
                const auto r = juce::Rectangle<int>::fromString (sorted[i].second);
                auto* at = editor->getComponentAt (r.getCentre());
                juce::String path;
                for (auto* c = at; c != nullptr && c != editor.get(); c = c->getParentComponent())
                    path = juce::String (typeid (*c).name()).fromLastOccurrenceOf (" ", false, false) + (path.isEmpty() ? "" : " > " + path);
                std::cout << "  " << sorted[i].first << "x " << sorted[i].second << "  " << path.substring (juce::jmax (0, path.length() - 160)) << std::endl;
            }
        }
    }

    pages->setScopeOpen (true);
    settle (1200);
    const auto start = cpuSeconds();
    settle (3000);
    std::cout << "idle SCOPE panel: " << juce::String (100.0 * (cpuSeconds() - start) / 3.0, 1) << "% of a core" << std::endl;
    std::cout << "worst page: " << juce::String (worst, 1) << "%" << std::endl;
    window.removeChildComponent (editor.get());
    return 0;
}

// Frames the editor actually paints while it animates: a page switch, then a
// held note (scopes, meters and modulation move). A transparent component
// over the whole editor is painted whenever anything under it is, so its
// paints are the frames drawn. 60 fps means gaps of at most ~17 ms.
struct FrameProbe : juce::Component
{
    FrameProbe() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override { times.push_back (juce::Time::getMillisecondCounterHiRes()); }
    std::vector<double> times;
};

juce::String frameStats (const std::vector<double>& times, double windowMs)
{
    if (times.size() < 2)
        return juce::String ((int) times.size()) + " frames";
    std::vector<double> gaps;
    for (size_t i = 1; i < times.size(); ++i)
        gaps.push_back (times[i] - times[i - 1]);
    std::sort (gaps.begin(), gaps.end());
    const auto p95 = gaps[(size_t) ((double) (gaps.size() - 1) * 0.95)];
    return juce::String (1000.0 * (double) times.size() / windowMs, 0) + " fps, median gap "
         + juce::String (gaps[gaps.size() / 2], 1) + " ms, p95 " + juce::String (p95, 1) + " ms, worst " + juce::String (gaps.back(), 1) + " ms";
}

int runFps()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
    HostWindow window (*editor);
    settle (800);

    if (auto* peer = window.getPeer())
    {
        const auto engines = peer->getAvailableRenderingEngines();
        std::cout << "renderer: " << engines[peer->getCurrentRenderingEngine()] << " (of " << engines.joinIntoString (", ") << ")" << std::endl;
    }

    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
        tutorial->setVisible (false);

    // The display's refresh rate.
    {
        int vblanks = 0;
        juce::VBlankAttachment counter (editor.get(), [&vblanks] { ++vblanks; });
        settle (1000);
        std::cout << "display: " << vblanks << " vblanks per second" << std::endl;
    }

    FrameProbe probe;
    editor->addAndMakeVisible (probe);
    int vblanks = 0;
    juce::VBlankAttachment vblankCounter (editor.get(), [&vblanks] { ++vblanks; });
    probe.setBounds (editor->getLocalBounds());

    // Audio in real time on another thread, a chord held while "playing".
    std::atomic<bool> running { true }, playing { false };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (2, 256);
        auto wasPlaying = false;
        auto next = juce::Time::getMillisecondCounterHiRes();
        while (running)
        {
            juce::MidiBuffer midi;
            const auto now = playing.load();
            if (now != wasPlaying)
                for (auto note : { 48, 55, 60, 64 })
                    midi.addEvent (now ? juce::MidiMessage::noteOn (1, note, 0.8f) : juce::MidiMessage::noteOff (1, note), 0);
            wasPlaying = now;
            buffer.clear();
            processor.processBlock (buffer, midi);
            next += 256.0 / 48.0;
            const auto wait = next - juce::Time::getMillisecondCounterHiRes();
            if (wait > 0.0)
                std::this_thread::sleep_for (std::chrono::microseconds ((int) (wait * 1000.0)));
        }
    });

    auto* pages = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());
    const auto filter = juce::SystemStats::getEnvironmentVariable ("ILANA_FPS_PAGES", "");

    for (const auto& id : pages->getPageIds())
    {
        if (filter.isNotEmpty() && ! juce::StringArray::fromTokens (filter, ",", "").contains (id))
            continue;
        playing = false;
        settle (400);
        probe.times.clear();
        const auto cpuStart = cpuSeconds();
        pages->showPage (id);
        probe.toFront (false);
        settle (400);
        const auto switchCpu = 100.0 * (cpuSeconds() - cpuStart) / 0.4;
        const auto switchStats = frameStats (probe.times, 400.0);

        playing = true;
        settle (300);
        probe.times.clear();
        vblanks = 0;
        const auto playStart = cpuSeconds();
        settle (1500);
        std::cout << id << "\n  switch:  " << switchStats << " (" << juce::String (switchCpu, 0) << "% cpu)"
                  << "\n  playing: " << frameStats (probe.times, 1500.0) << " (" << juce::String (100.0 * (cpuSeconds() - playStart) / 1.5, 0) << "% cpu, "
                  << juce::String (vblanks / 1.5, 0) << " vblanks/s)" << std::endl;
    }

    running = false;
    audio.join();
    editor->removeChildComponent (&probe);
    window.removeChildComponent (editor.get());
    return 0;
}

// Times what a host does when it opens a saved project: construct, prepare,
// restore the state, open the editor, and restore again with it open.
static int runLoadTime()
{
    const auto now = [] { return juce::Time::getMillisecondCounterHiRes(); };
    const auto pump = []
    {
        for (int i = 0; i < 20; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
    };

    auto start = now();
    auto processor = std::make_unique<IlanaSynthAudioProcessor>();
    std::cout << "construct:        " << juce::String (now() - start, 0) << " ms" << std::endl;

    start = now();
    processor->prepareToPlay (48000.0, 512);
    std::cout << "prepareToPlay:    " << juce::String (now() - start, 0) << " ms" << std::endl;

    for (const auto* name : { "Swarm", "Init" })
    {
        const auto index = processor->getFactoryPresetNames().indexOf (name);
        start = now();
        processor->loadFactoryPreset (index);
        std::cout << "load preset " << name << ": " << juce::String (now() - start, 0) << " ms" << std::endl;
    }

    processor->loadFactoryPreset (processor->getFactoryPresetNames().indexOf ("Swarm"));
    juce::MemoryBlock state;
    start = now();
    processor->getStateInformation (state);
    std::cout << "getState:         " << juce::String (now() - start, 0) << " ms (" << (int) state.getSize() << " bytes)" << std::endl;

    start = now();
    processor->setStateInformation (state.getData(), (int) state.getSize());
    std::cout << "setState:         " << juce::String (now() - start, 0) << " ms" << std::endl;

    start = now();
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorIfNeeded());
    std::cout << "createEditor:     " << juce::String (now() - start, 0) << " ms" << std::endl;
    start = now();
    pump();
    std::cout << "editor settle:    " << juce::String (now() - start - 100.0, 0) << " ms (beyond the 100 ms pump)" << std::endl;

    start = now();
    processor->setStateInformation (state.getData(), (int) state.getSize());
    std::cout << "setState+editor:  " << juce::String (now() - start, 0) << " ms" << std::endl;

    // What a host hears while presets are clicked through with the editor open.
    struct Counter : juce::AudioProcessorListener
    {
        int changes = 0;
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override { ++changes; }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
    } counter;
    processor->addListener (&counter);

    for (const auto* name : { "Init", "Swarm", "Rip Bass", "Grain Choir", "Scream Lead" })
    {
        counter.changes = 0;
        start = now();
        processor->loadFactoryPreset (processor->getFactoryPresetNames().indexOf (name));
        const auto loadMs = now() - start;
        start = now();
        pump();
        std::cout << "preset " << name << " with editor: " << juce::String (loadMs, 0) << " ms, settle "
                  << juce::String (now() - start - 100.0, 0) << " ms, " << counter.changes << " host notifications" << std::endl;
    }

    processor->removeListener (&counter);
    start = now();
    pump();
    std::cout << "after settle:     " << juce::String (now() - start - 100.0, 0) << " ms (beyond the 100 ms pump)" << std::endl;

    editor.reset();
    return 0;
}

// A 1-bar clip looped in a DAW: four notes held for the whole bar, the
// note-offs and the next loop's note-ons on the same sample. Prints each
// loop's render time and the live voice count.
static int runLoopTest (const juce::String& presetName, bool allNotesOffAtLoop)
{
    IlanaSynthAudioProcessor processor;
    const auto rate = 48000.0;
    const auto blockSize = 512;
    processor.prepareToPlay (rate, blockSize);
    if (juce::File::isAbsolutePath (presetName))
        processor.loadPresetFromFile (juce::File (presetName));
    else
        processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (presetName));

    // ILANA_LOOP_SET="id=value;id=value" sets parameters (real values) after the load.
    for (const auto& pair : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_LOOP_SET", ""), ";", ""))
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (pair.upToFirstOccurrenceOf ("=", false, false))))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));

    const int notes[] { 48, 55, 60, 64 };
    const auto barSamples = (int) (rate * 2.0); // 120 BPM, 4/4
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::int64 position = 0;

    for (int loop = 0; loop < 12; ++loop)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        auto maxVoices = 0;

        for (int offset = 0; offset < barSamples; offset += blockSize)
        {
            juce::MidiBuffer midi;
            const auto n = juce::jmin (blockSize, barSamples - offset);

            if (offset == 0)
            {
                if (loop > 0)
                {
                    if (allNotesOffAtLoop)
                        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                    else
                        for (auto note : notes)
                            midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                }

                for (auto note : notes)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            }

            buffer.setSize (2, n, false, false, true);
            buffer.clear();
            processor.processBlock (buffer, midi);
            maxVoices = juce::jmax (maxVoices, processor.getActiveVoiceCount());
            position += n;
        }

        const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
        std::cout << "loop " << loop + 1 << ": " << juce::String (100.0 * ms / 2000.0, 1) << "% of real time, up to "
                  << maxVoices << " voices" << std::endl;
    }

    // Release everything and see how long the voices take to free up.
    juce::MidiBuffer offs;
    for (auto note : notes)
        offs.addEvent (juce::MidiMessage::noteOff (1, note), 0);

    buffer.setSize (2, blockSize, false, false, true);
    for (int block = 0; block < (int) (rate * 60.0) / blockSize; ++block)
    {
        buffer.clear();
        juce::MidiBuffer none;
        processor.processBlock (buffer, block == 0 ? offs : none);

        if (processor.getActiveVoiceCount() == 0)
        {
            std::cout << "all voices free " << juce::String (block * blockSize / rate, 2) << " s after the last note-off" << std::endl;
            return 0;
        }
    }

    std::cout << processor.getActiveVoiceCount() << " voices still live 60 s after the last note-off" << std::endl;
    return 0;
}

// Loudness of each physical exciter on Init with OSC 1 switched to Physical:
// one note held 1 s, RMS and peak in dBFS. ILANA_LOOP_SET applies as above.
static int runExciterLevels()
{
    const juce::StringArray names { "Burst", "Noise", "Saw", "Pulse", "Bow", "Hammer", "Osc In", "Tine", "Reed", "Piano", "Feedback" };
    const auto rate = 48000.0;
    const auto blockSize = 512;

    for (const auto note : { 48, 60, 72 })
    {
        std::cout << "note " << note << ":" << std::endl;

        for (int excite = 0; excite < names.size(); ++excite)
        {
            IlanaSynthAudioProcessor processor;
            processor.prepareToPlay (rate, blockSize);
            processor.loadFactoryPreset (0);
            const auto set = [&processor] (const juce::String& id, float value)
            {
                if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (id)))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            set ("osc1_mode", 1.0f);
            set ("osc1_excite", (float) excite);
            for (const auto& pair : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_LOOP_SET", ""), ";", ""))
                set (pair.upToFirstOccurrenceOf ("=", false, false), pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue());

            juce::AudioBuffer<float> buffer (2, blockSize);
            double sum = 0.0;
            float peak = 0.0f;
            int count = 0;

            for (int block = 0; block < (int) rate / blockSize; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                buffer.clear();
                processor.processBlock (buffer, midi);

                for (int i = 0; i < blockSize; ++i)
                {
                    const auto s = buffer.getSample (0, i);
                    sum += (double) s * s;
                    peak = juce::jmax (peak, std::abs (s));
                    ++count;
                }
            }

            std::cout << "  " << names[excite].paddedRight (' ', 9) << " rms " << juce::String (juce::Decibels::gainToDecibels ((float) std::sqrt (sum / count), -120.0f), 1)
                      << " dB, peak " << juce::String (juce::Decibels::gainToDecibels (peak, -120.0f), 1) << " dB" << std::endl;
        }
    }

    return 0;
}

// Loads a saved state (as a host would hand it back) and plays a note: for
// chasing a crash the state fuzz test finds. ILANA_STRIP="Samples;Wavetables"
// removes those children first.
static int runLoadState (const juce::File& file)
{
    juce::MemoryBlock data;
    file.loadFileAsData (data);

    if (const auto strip = juce::SystemStats::getEnvironmentVariable ("ILANA_STRIP", ""); strip.isNotEmpty())
        if (auto xml = juce::AudioProcessor::getXmlFromBinary (data.getData(), (int) data.getSize()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            for (const auto& name : juce::StringArray::fromTokens (strip, ";", ""))
                tree.removeChild (tree.getChildWithName (name), nullptr);
            data.reset();
            juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), data);
        }

    auto target = std::make_unique<IlanaSynthAudioProcessor>();
    std::cout << "setState" << std::endl;
    target->setStateInformation (data.getData(), (int) data.getSize());
    std::cout << "flush" << std::endl;
    target->flushAsyncUpdates();
    std::cout << "play" << std::endl;
    target->prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);
    for (int b = 0; b < 30; ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (b == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        target->processBlock (buffer, midi);
    }
    std::cout << "save" << std::endl;
    juce::MemoryBlock again;
    target->getStateInformation (again);
    target.reset();
    std::cout << "ok" << std::endl;
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
    {
        std::cout << "usage: ilanaSnapshot <output dir> [factory preset index]\n"
                     "       ilanaSnapshot --uitest" << std::endl;
        return 1;
    }

    if (juce::String (argv[1]) == "--uitest")
        return runUiTests();

    if (juce::String (argv[1]) == "--idlecpu")
        return runIdleCpu();

    if (juce::String (argv[1]) == "--fps")
        return runFps();

    if (juce::String (argv[1]) == "--loadtime")
        return runLoadTime();

    if (juce::String (argv[1]) == "--loadstate" && argc > 2)
        return runLoadState (juce::File (juce::String (argv[2])));

    if (juce::String (argv[1]) == "--exciters")
        return runExciterLevels();

    if (juce::String (argv[1]) == "--looptest")
        return runLoopTest (argc > 2 ? juce::String (argv[2]) : juce::String ("Swarm"), argc > 3);

    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    outDir.createDirectory();

    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // A factory preset by index or by name.
    if (argc > 2)
    {
        const juce::String preset (argv[2]);
        const auto byName = processor.getFactoryPresetNames().indexOf (preset);
        processor.loadFactoryPreset (byName >= 0 ? byName : preset.getIntValue());
    }

    // Shows the SEQ tab, which is hidden until a step LFO is in use.
    if (auto* shape = processor.apvts.getParameter ("lfo4_shape"))
        shape->setValueNotifyingHost (shape->convertTo0to1 (7.0f));

    // A held note (unless ILANA_SNAPSHOT_SILENT is set), so the scope and the
    // modulation markers are live in the pictures.
    juce::AudioBuffer<float> audio (2, 512);
    bool noteSent = false;
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_SILENT", "").isEmpty())
        beforeSave() = [&]
        {
            for (int block = 0; block < 24; ++block)
            {
                juce::MidiBuffer midi;
                if (! noteSent)
                {
                    midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.8f), 0);
                    noteSent = true;
                }
                audio.clear();
                processor.processBlock (audio, midi);
            }
            settle (60);
        };

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    // ILANA_SNAPSHOT_WIDTH renders at another zoom (795 is 75%, 2120 is 200%).
    const auto width = juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_WIDTH", "1060").getIntValue();
    editor->setSize (width, width * 720 / 1060);
    settle (400);

    // The intro only shows until it has been seen once; capture it anyway.
    if (auto* tutorial = findChild<TutorialOverlay> (*editor))
    {
        tutorial->setVisible (true);
        tutorial->toFront (false);
        settle (200);
        save (*editor, outDir.getChildFile ("00-tutorial.png"));
        tutorial->setVisible (false);
    }

    // ILANA_SNAPSHOT_CONFIRM: the question a load over an edited patch asks.
    if (auto* confirm = findChild<ConfirmOverlay> (*editor);
        confirm != nullptr && juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_CONFIRM", "").isNotEmpty())
    {
        confirm->ask ("Replace your edits?",
                      "'" + processor.getCurrentPresetName() + "' has changes that aren't saved. Loading 'Init' replaces them.",
                      "Load anyway", [] (bool, bool) {});
        settle (100);
        save (*editor, outDir.getChildFile ("00-confirm.png"));
        confirm->finish (false);
    }

    auto* pages = dynamic_cast<IlanaSynthAudioProcessorEditor*> (editor.get());

    if (pages == nullptr)
        return 1;

    // ILANA_SNAPSHOT_EXTRAS: the UI review 2 additions (the SPEC view, a
    // knob's source card, the docked browser), then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_EXTRAS", "").isNotEmpty())
    {
        pages->showPage ("MAIN");
        settle (400);
        std::vector<WaveDisplay*> waves;
        findAll<WaveDisplay> (*editor, waves);
        for (auto* wave : waves)
            if (visibleInTree (wave) && wave->getOscIndex() == 0)
            {
                std::vector<juce::TextButton*> keys;
                findAll<juce::TextButton> (*wave, keys);
                if (! keys.empty())
                {
                    keys.front()->triggerClick();
                    settle (60);
                    keys.front()->triggerClick();
                    settle (300);
                }
            }
        std::vector<KnobControl*> knobs;
        findAll<KnobControl> (*editor, knobs);
        KnobControl* busiest = nullptr;
        for (auto* knob : knobs)
            if (visibleInTree (knob) && knob->getNumRoutings() > (busiest != nullptr ? busiest->getNumRoutings() : 0))
                busiest = knob;
        if (busiest != nullptr)
            busiest->openModCard (true);
        save (*editor, outDir.getChildFile ("extra-spec-and-card.png"));
        for (auto* knob : knobs)
            knob->closeModCard();

        if (auto* display = findChild<PresetDisplay> (*editor); display != nullptr && display->onClick != nullptr)
        {
            display->onClick();
            settle (500);
            save (*editor, outDir.getChildFile ("extra-browser-dropdown.png"));
            if (auto* panel = findChild<PresetPanel> (*editor); panel != nullptr && panel->onDockRequest != nullptr)
            {
                // DX7 with its bank chips (the settings are left as they were).
                panel->selectFilter ("DX7");
                panel->clickChip ("bank:ROM1A");
                settle (200);
                save (*editor, outDir.getChildFile ("extra-browser-dx7.png"));
                panel->onDockRequest (true);
                settle (500);
                save (*editor, outDir.getChildFile ("extra-browser-docked-dx7.png"));
                panel->selectFilter ("");
                settle (200);
                save (*editor, outDir.getChildFile ("extra-browser-docked.png"));
                panel->onDockRequest (false);
                settle (300);
                panel->close();
                settle (300);
            }
        }

        // SAVE AS, with a name that loses characters (cancelled, so nothing is written).
        pages->savePresetAs();
        pages->getSaveOverlay().getNameField().setText ("Acid: Bass / Mk 2", true);
        settle (300);
        save (*editor, outDir.getChildFile ("extra-save-as.png"));
        pages->getSaveOverlay().cancel();

        {
            TableBrowser browser (processor, "osc1_table", IlanaTheme::accent());
            browser.setSize (912, 576);
            browser.setSearchText ("saw");
            settle (200);
            save (browser, outDir.getChildFile ("extra-table-search.png"));
        }
        return 0;
    }

    // ILANA_SNAPSHOT_REMAP: the matrix with a drawn remap on the first row,
    // its editor open, then stop.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_REMAP", "").isNotEmpty())
    {
        pages->showPage ("MATRIX");
        settle (400);
        std::vector<CurveControl*> curves;
        findAll<CurveControl> (*editor, curves);
        for (auto* curve : curves)
            if (visibleInTree (curve))
            {
                const auto slot = curve->getSlotIndex();
                processor.setModRemap (slot, RemapEditor::shape (4));
                settle (300);
                save (*editor, outDir.getChildFile ("remap-matrix.png"));
                // The editor, docked under its row.
                curve->openRemapEditor();
                settle (500);
                save (*editor, outDir.getChildFile ("remap-editor.png"));
                curve->openRemapEditor(); // closes it again
                break;
            }
        return 0;
    }

    const auto pageIds = pages->getPageIds();
    // ILANA_SNAPSHOT_PAGES="MAIN,OSC": only those pages (no extras), then stop.
    const auto onlyPages = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ILANA_SNAPSHOT_PAGES", ""), ",", "");

    for (int i = 0; i < pageIds.size(); ++i)
    {
        if (! onlyPages.isEmpty() && ! onlyPages.contains (pageIds[i]))
            continue;

        pages->showPage (pageIds[i]);
        settle (450);
        const auto stem = juce::String (i + 1).paddedLeft ('0', 2) + "-" + pageIds[i].replaceCharacter ('/', '-');
        save (*editor, outDir.getChildFile (stem + ".png"));

        if (pageIds[i] == "OSC")
        {
            if (auto* mode = processor.apvts.getParameter ("osc1_mode"))
            {
                mode->setValueNotifyingHost (mode->convertTo0to1 (1.0f));
                settle (300);
                save (*editor, outDir.getChildFile ("osc-physical.png"));
                if (auto* excite = processor.apvts.getParameter ("osc1_excite"))
                    excite->setValueNotifyingHost (excite->convertTo0to1 (4.0f));
                if (auto* buzz = processor.apvts.getParameter ("osc1_bridge_buzz"))
                    buzz->setValueNotifyingHost (buzz->convertTo0to1 (0.6f));
                if (auto* rattle = processor.apvts.getParameter ("osc1_fret_rattle"))
                    rattle->setValueNotifyingHost (rattle->convertTo0to1 (0.4f));
                settle (300);
                save (*editor, outDir.getChildFile ("osc-bow-buzz.png"));
                if (auto* viewport = dynamic_cast<juce::Viewport*> (pages->getCurrentPage()))
                {
                    if (auto* on = processor.apvts.getParameter ("sym_on"))
                        on->setValueNotifyingHost (on->convertTo0to1 (1.0f));
                    if (auto* amount = processor.apvts.getParameter ("sym_amount"))
                        amount->setValueNotifyingHost (amount->convertTo0to1 (0.5f));
                    // Let the page grow for the opened card, then scroll to it.
                    settle (250);
                    viewport->setViewPosition (0, 10000);
                    settle (100);
                    save (*editor, outDir.getChildFile ("osc-sympathetic.png"));
                    if (auto* manual = processor.apvts.getParameter ("sym_manual"))
                        manual->setValueNotifyingHost (manual->convertTo0to1 (1.0f));
                    settle (250);
                    viewport->setViewPosition (0, 10000);
                    settle (100);
                    save (*editor, outDir.getChildFile ("osc-sympathetic-manual.png"));
                    viewport->setViewPosition (0, 0);
                }
                mode->setValueNotifyingHost (mode->convertTo0to1 (0.0f));
            }
        }

        // Every sub-tab past the first on this page.
        std::vector<SubTabBar*> bars;

        if (auto* page = pages->getCurrentPage())
            findAll<SubTabBar> (*page, bars);

        for (size_t b = 0; b < bars.size(); ++b)
        {
            for (int item = 1; item < bars[b]->getNumRevealed(); ++item)
            {
                bars[b]->onSelect (item);
                settle (300);
                save (*editor, outDir.getChildFile (stem + "-bar" + juce::String ((int) b + 1) + "-" + juce::String (item + 1) + ".png"));
            }

            if (bars[b]->getNumRevealed() > 1)
                bars[b]->onSelect (0);
        }

        // Every envelope card on the ENV/LFO page.
        if (auto* page = pages->getCurrentPage())
        {
            if (auto* envCards = findChild<EnvThumbBar> (*page); envCards != nullptr && envCards->onSelect != nullptr)
            {
                for (int env = 1; env < 5; ++env)
                {
                    envCards->onSelect (env);
                    settle (300);
                    save (*editor, outDir.getChildFile (stem + "-env" + juce::String (env + 1) + ".png"));
                }

                processor.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, 5, true);
                envCards->onSelect (5);
                settle (300);
                save (*editor, outDir.getChildFile ("env-pool-revealed.png"));
                for (int env = 0; env < 16; ++env)
                    processor.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, env, true);
                envCards->onSelect (15);
                settle (300);
                save (*editor, outDir.getChildFile ("env-pool-full.png"));

                envCards->onSelect (0);
            }
        }

        // Each FX module's editor, placed in slot 1.
        if (pageIds[i] == "FX")
        {
            for (int type = 1; type <= IlanaSynthAudioProcessor::numFxTypes; ++type)
            {
                processor.assignFxSlot (1, type);
                settle (900);
                save (*editor, outDir.getChildFile ("fx-" + juce::String (type).paddedLeft ('0', 2) + ".png"));
            }
        }
    }

    if (! onlyPages.isEmpty())
        return 0;

    // The scope floats over a page, then fills it.
    pages->showPage ("MAIN");
    pages->setScopeOpen (true);
    settle (500);
    save (*editor, outDir.getChildFile ("scope-panel.png"));
    pages->setScopeOpen (false);
    settle (100);

    // The drawable Curve LFO editor.
    if (auto* shape = processor.apvts.getParameter ("lfo1_shape"))
    {
        pages->showPage ("ENV/LFO");
        if (auto* page = pages->getCurrentPage())
            if (auto* thumbs = findChild<LfoThumbBar> (*page); thumbs != nullptr && thumbs->onSelect != nullptr)
                thumbs->onSelect (0);
        for (const auto physicsShape : { LfoShapes::Bounce, LfoShapes::Pendulum, LfoShapes::Spring, LfoShapes::Friction })
        {
            shape->setValueNotifyingHost (shape->convertTo0to1 ((float) physicsShape));
            settle (350);
            save (*editor, outDir.getChildFile ("lfo-physics-" + juce::String (physicsShape) + ".png"));
        }
    }

    // The drawable Curve LFO editor.
    if (auto* shape = processor.apvts.getParameter ("lfo1_shape"))
    {
        shape->setValueNotifyingHost (shape->convertTo0to1 ((float) IlanaSynthAudioProcessor::curveShape));
        processor.setLfoCurve (0, LfoCurve::preset (9));
        pages->showPage ("ENV/LFO");

        if (auto* page = pages->getCurrentPage())
            if (auto* thumbs = findChild<LfoThumbBar> (*page); thumbs != nullptr && thumbs->onSelect != nullptr)
                thumbs->onSelect (0);

        settle (500);
        save (*editor, outDir.getChildFile ("lfo-curve.png"));
    }

    // The LFO pool, every card revealed, the last one selected.
    {
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, true);
        pages->showPage ("ENV/LFO");
        if (auto* page = pages->getCurrentPage())
            if (auto* thumbs = findChild<LfoThumbBar> (*page); thumbs != nullptr && thumbs->onSelect != nullptr)
                thumbs->onSelect (13);
        settle (400);
        save (*editor, outDir.getChildFile ("lfo-pool-full.png"));
        pages->showPage ("MAIN");
        settle (300);
        save (*editor, outDir.getChildFile ("lfo-pool-main.png"));
        for (int lfo = 3; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, false);
    }

    // M8.1: the simulated LFO shapes, each with its picture and named knobs.
    {
        pages->showPage ("ENV/LFO");
        if (auto* page = pages->getCurrentPage())
            if (auto* thumbs = findChild<LfoThumbBar> (*page); thumbs != nullptr && thumbs->onSelect != nullptr)
                thumbs->onSelect (0);
        const std::pair<int, const char*> shapes[] {
            { LfoSimShapes::Bounce, "bounce" }, { LfoSimShapes::Pendulum, "pendulum" }, { LfoSimShapes::Spring, "spring" },
            { LfoSimShapes::Friction, "friction" }, { LfoSimShapes::Lorenz, "lorenz" }, { LfoSimShapes::DoublePendulum, "double-pendulum" },
            { LfoSimShapes::Duffing, "duffing" }, { LfoSimShapes::Perlin, "perlin" }, { LfoSimShapes::Henon, "henon" } };
        for (const auto& [shape, name] : shapes)
        {
            if (auto* parameter = processor.apvts.getParameter ("lfo1_shape"))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) shape));
            const auto& info = LfoSimInfo::get (shape);
            for (int param = 0; param < LfoSimInfo::numParams; ++param)
                if (auto* parameter = processor.apvts.getParameter ("lfo1_p" + juce::String (param + 1)))
                    parameter->setValueNotifyingHost (info.params[(size_t) param].defaultValue);
            if (auto* rate = processor.apvts.getParameter ("lfo1_rate"))
                rate->setValueNotifyingHost (rate->convertTo0to1 (1.0f));
            settle (1500);
            save (*editor, outDir.getChildFile ("lfo-sim-" + juce::String (name) + ".png"));
        }
        if (auto* parameter = processor.apvts.getParameter ("lfo1_shape"))
            parameter->setValueNotifyingHost (0.0f);
        if (auto* rate = processor.apvts.getParameter ("lfo1_rate"))
            rate->setValueNotifyingHost (rate->convertTo0to1 (4.0f));
        settle (200);
    }

    // The filter overhaul: the grid's second page with the new models
    // (303 Acid on Filter 1, Vowel Morph on Filter 2).
    {
        const auto setType = [&processor] (const char* id, int type)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) type));
        };
        setType ("f1_type", FilterType::Acid303);
        setType ("f2_type", FilterType::VowelMorph);
        pages->showPage ("FILTER");
        settle (500);
        save (*editor, outDir.getChildFile ("filter-new-models.png"));
        setType ("f1_type", 0);
        setType ("f2_type", 0);
        settle (200);
    }

    // M8.3: the WEST card (FILTER page, FILTER 2 / WEST tabs).
    {
        if (auto* parameter = processor.apvts.getParameter ("west_on"))
            parameter->setValueNotifyingHost (1.0f);
        pages->showPage ("FILTER");
        std::vector<CardTabs*> cardTabs;
        if (auto* page = pages->getCurrentPage())
            findAll<CardTabs> (*page, cardTabs);
        for (auto* bar : cardTabs)
            if (bar->getNames().contains ("WEST"))
                bar->setSelected (1, true);
        settle (500);
        save (*editor, outDir.getChildFile ("filter-west.png"));
        for (auto* bar : cardTabs)
            if (bar->getNames().contains ("WEST"))
                bar->setSelected (0, true);
        if (auto* parameter = processor.apvts.getParameter ("west_on"))
            parameter->setValueNotifyingHost (0.0f);
    }

    // M8.6: an oscillator playing a bounce of the patch.
    {
        IlanaSynthAudioProcessor::BounceRequest request;
        request.targetOsc = 1;
        request.muteOthers = false;
        processor.startBounce (request);
        for (int i = 0; i < 400 && processor.getBounceState() == IlanaSynthAudioProcessor::BounceState::Rendering; ++i)
            settle (50);
        pages->showPage ("OSC");
        settle (600);
        save (*editor, outDir.getChildFile ("osc-bounce.png"));
        for (const auto* id : { "osc2_mode", "osc2_on", "osc2_semi" })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
    }

    // M8.7: the PHYSICAL page, a moment after a note (Felt Hammer Board, then a
    // feedback guitar).
    for (const auto& [presetName, file] : { std::pair<const char*, const char*> { "Felt Hammer Board", "physical-page.png" },
                                            { "", "physical-feedback.png" } })
    {
        if (juce::String (presetName).isNotEmpty())
            processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf (presetName));
        else
        {
            processor.loadFactoryPreset (0);
            for (const auto& [id, value] : { std::pair<const char*, float> { "osc1_mode", 1.0f }, { "osc1_excite", 10.0f },
                                             { "osc1_string_sustain", 0.6f }, { "osc2_on", 0.0f } })
                if (auto* parameter = processor.apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        pages->showPage ("PHYSICAL");
        settle (300);
        juce::AudioBuffer<float> buffer (2, 512);
        for (int block = 0; block < 20; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
        settle (350);
        save (*editor, outDir.getChildFile (file));
        processor.panic();
    }
    processor.loadFactoryPreset (0);

    // M8.5: the VECTOR page, with the pad on and a path.
    {
        for (const auto& [id, value] : { std::pair<const char*, float> { "vec_on", 1.0f }, { "vec_path", 1.0f },
                                         { "vec_drift", 0.3f }, { "macro1_evolve", 0.4f }, { "macro3_evolve", 0.2f },
                                         { "osc4_on", 1.0f } })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        pages->showPage ("VECTOR");
        settle (600);
        save (*editor, outDir.getChildFile ("vector-page.png"));
        for (const auto* id : { "vec_on", "vec_path", "vec_drift", "macro1_evolve", "macro3_evolve", "osc4_on" })
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
    }

    // M8.4: the second page of filter models.
    {
        if (auto* parameter = processor.apvts.getParameter ("f1_type"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) FilterType::VowelBank));
        if (auto* parameter = processor.apvts.getParameter ("f1_reso"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.6f));
        pages->showPage ("FILTER");
        settle (500);
        save (*editor, outDir.getChildFile ("filter-models-2.png"));
        if (auto* parameter = processor.apvts.getParameter ("f1_type"))
            parameter->setValueNotifyingHost (0.0f);
        if (auto* parameter = processor.apvts.getParameter ("f1_reso"))
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
    }

    // M4: the Hammered Strings preset on the OSC page.
    if (const auto program = processor.getFactoryPresetNames().indexOf ("Hammered Strings"); program >= 0)
    {
        processor.loadFactoryPreset (program);
        pages->showPage ("OSC");
        settle (500);
        save (*editor, outDir.getChildFile ("keys-grand-osc.png"));
        if (auto* viewport = dynamic_cast<juce::Viewport*> (pages->getCurrentPage()))
        {
            viewport->setViewPosition (0, 10000);
            settle (200);
            save (*editor, outDir.getChildFile ("keys-grand-osc-scrolled.png"));
            viewport->setViewPosition (0, 0);
        }
        processor.loadFactoryPreset (0);
    }

    // Added oscillators: MAIN and OSC scroll, FM grows its matrix.
    {
        processor.addOscillator (3);
        processor.addOscillator (4);
        for (const auto* page : { "MAIN", "OSC", "FM" })
        {
            pages->showPage (page);
            settle (400);
            save (*editor, outDir.getChildFile ("added-osc-" + juce::String (page) + ".png"));

            if (auto* content = pages->getCurrentPage())
            {
                std::vector<juce::Viewport*> views;
                if (auto* own = dynamic_cast<juce::Viewport*> (content))
                    views.push_back (own);
                findAll<juce::Viewport> (*content, views);
                for (auto* view : views)
                    view->setViewPosition (0, 10000);
                if (! views.empty())
                {
                    settle (150);
                    save (*editor, outDir.getChildFile ("added-osc-" + juce::String (page) + "-scrolled.png"));
                    for (auto* view : views)
                        view->setViewPosition (0, 0);
                }
            }
        }
        processor.removeOscillator (4);
        processor.removeOscillator (3);
    }

    // M5/M6: a six-operator algorithm, a PD chain and a DAHDSR envelope.
    {
        const auto set = [&processor] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        processor.applyFmAlgorithm (10);
        set ("osc1_tune", 1.0f);
        set ("fm_noise3", 0.2f);
        pages->showPage ("FM");
        settle (400);
        save (*editor, outDir.getChildFile ("fm-dx-keys.png"));
        pages->showPage ("FILTER");
        settle (300);
        save (*editor, outDir.getChildFile ("filter-six-osc.png"));

        set ("osc1_mode", 0.0f);
        set ("osc1_table", 8.0f);
        set ("osc1_warp", (float) Warp::PdRes2);
        set ("osc1_warp_amt", 0.4f);
        set ("osc1_warp2", 8.0f);
        set ("osc1_warp2_amt", 0.3f);
        set ("osc1_pd_env", 2.0f);
        pages->showPage ("OSC");
        settle (400);
        save (*editor, outDir.getChildFile ("osc-pd-chain.png"));

        set ("amp_delay", 0.15f);
        set ("amp_hold", 0.3f);
        pages->showPage ("ENV/LFO");
        settle (400);
        save (*editor, outDir.getChildFile ("env-dahdsr.png"));

        set ("lfo1_shape", 12.0f);   // Bounce
        set ("lfo1_phys_b", 0.7f);
        settle (400);
        save (*editor, outDir.getChildFile ("lfo-bounce.png"));
    }

    // FM into oscillators that ignore it: OSC 2 as a sample, OSC 3 a string.
    {
        const auto set = [&processor] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        processor.loadFactoryPreset (0);
        set ("osc2_mode", 2.0f);
        set ("sub_mode", 1.0f);
        set ("fm_1to2", 0.4f);
        pages->showPage ("FM");
        settle (400);
        save (*editor, outDir.getChildFile ("fm-no-input.png"));
        processor.loadFactoryPreset (0);
        pages->showPage ("MAIN");
        settle (300);
    }

    // M7.1: the Generative card's EUCLID and PROB SEQ tabs, and strum.
    {
        const auto set = [&processor] (const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        pages->showPage ("ARP/SEQ");
        settle (100);
        std::vector<CardTabs*> cardTabs;
        findAll<CardTabs> (*editor, cardTabs);
        CardTabs* engineTabs = nullptr;
        for (auto* candidate : cardTabs)
            if (candidate->getNames().contains ("EUCLID"))
                engineTabs = candidate;

        set ("euc_on", 1.0f);
        set ("euc_hits", 7.0f);
        set ("euc_rotate", 2.0f);
        set ("spray_strum", 1.0f);

        if (engineTabs != nullptr)
        {
            engineTabs->setSelected (1, true);
            settle (400);
            save (*editor, outDir.getChildFile ("gen-euclid.png"));

            set ("pseq_on", 1.0f);
            set ("pseq_length", 12.0f);
            for (int step = 1; step <= 16; ++step)
            {
                set ("pseq_chance" + juce::String (step), step % 3 == 0 ? 0.4f : 0.9f);
                set ("pseq_range" + juce::String (step), (float) ((step * 5) % 13));
                set ("pseq_ratchet" + juce::String (step), step % 4 == 0 ? 3.0f : 1.0f);
            }
            engineTabs->setSelected (2, true);
            settle (400);
            save (*editor, outDir.getChildFile ("gen-probseq.png"));
            engineTabs->setSelected (0, true);
        }

        processor.loadFactoryPreset (0);
        pages->showPage ("MAIN");
        settle (300);
    }

    // A source chip clicked: the knobs it drives stay lit (the first chip that drives a visible knob).
    {
        pages->showPage ("MAIN");
        settle (300);
        std::vector<ModSourceChip*> chipsFound;
        std::vector<KnobControl*> knobsFound;
        findAll<ModSourceChip> (*editor, chipsFound);
        findAll<KnobControl> (*editor, knobsFound);

        for (auto* chip : chipsFound)
        {
            auto drives = false;
            for (auto* knob : knobsFound)
                drives = drives || (visibleInTree (knob) && knob->isDrivenBy (chip->getSourceIndex()));

            if (! visibleInTree (chip) || ! drives)
                continue;

            chip->togglePinned();
            settle (150);
            save (*editor, outDir.getChildFile ("mod-pinned-chip.png"));
            chip->togglePinned();
            break;
        }
    }

    // The preset browser, opened from the preset name.
    if (auto* display = findChild<PresetDisplay> (*editor); display != nullptr && display->onClick != nullptr)
    {
        display->onClick();
        settle (500);
        save (*editor, outDir.getChildFile ("preset-browser.png"));
        display->onClick();
        settle (400);
    }

    // The wavetable browser on its own.
    {
        TableBrowser browser (processor, "osc1_table", IlanaTheme::accent());
        browser.setSize (740, 520);
        settle (100);
        save (browser, outDir.getChildFile ("table-browser.png"));

        // The size it opens at over a 100% editor (see TableBrowser::show).
        browser.setSize (912, 576);
        settle (100);
        save (browser, outDir.getChildFile ("table-browser-editor.png"));
    }

    editor.reset();
    return 0;
}
