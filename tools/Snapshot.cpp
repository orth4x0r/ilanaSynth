// Dev tool: renders the plugin editor offscreen and writes one PNG per tab,
// so the UI can be reviewed without a DAW or a display session.
//   ilanaSnapshot <output dir> [factory preset index]
//   ilanaSnapshot --uitest     (drives the editor and checks the wiring)

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

#include "PluginProcessor.h"
#include "gui/HeaderWidgets.h"
#include "gui/CardTabs.h"
#include "gui/EnvThumbs.h"
#include "gui/TableBrowser.h"
#include "gui/EnvelopeDisplay.h"
#include "gui/LfoThumbs.h"
#include "gui/MatrixWidgets.h"
#include "gui/ParamControls.h"
#include "gui/SubTabBar.h"
#include "gui/TutorialOverlay.h"

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

void save (juce::Component& editor, const juce::File& file)
{
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

    auto* tabs = findChild<juce::TabbedComponent> (*editor);
    expect (tabs != nullptr && tabs->getTabNames()[0] == "MAIN", "MAIN is the first tab");

    const auto tabIndex = [tabs] (const juce::String& name) { return tabs->getTabNames().indexOf (name); };

    // Matrix shows the preset's routing with the right destination text.
    tabs->setCurrentTabIndex (tabIndex ("MATRIX"));
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
    tabs->setCurrentTabIndex (tabIndex ("FILTER"));
    settle (300);

    std::vector<KnobControl*> knobs;
    findAll<KnobControl> (*editor, knobs);
    KnobControl* reso = nullptr;

    for (auto* knob : knobs)
        if (knob->getParameterId() == "f1_reso" && visibleInTree (knob))
            reso = knob;

    expect (reso != nullptr, "filter page has a resonance knob");

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
        }
        else
        {
            expect (false, "knob has a dot strip");
        }

        // An effect knob is a drop target too (plain-parameter destination).
        tabs->setCurrentTabIndex (tabIndex ("FX"));
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
    tabs->setCurrentTabIndex (tabIndex ("MAIN"));
    settle (300);

    if (auto* page = tabs->getCurrentContentComponent())
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
            expect (tabs->getTabNames()[tabs->getCurrentTabIndex()] == "MAIN", "dragging an LFO card on MAIN stays on MAIN");

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

            expect (tabs->getTabNames()[tabs->getCurrentTabIndex()] == "MAIN" && lfoTabs != nullptr,
                    "clicking an LFO card selects it on MAIN");

            if (lfoTabs != nullptr && lfoTabs->onOpen != nullptr)
            {
                lfoTabs->onOpen();
                settle (100);
                expect (tabs->getTabNames()[tabs->getCurrentTabIndex()] == "ENV/LFO", "the LFO card's open button goes to ENV/LFO");
            }

            // The envelope tabs swap MAIN's envelope controls (AMP -> MOD).
            tabs->setCurrentTabIndex (tabIndex ("MAIN"));
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
    tabs->setCurrentTabIndex (tabIndex ("ENV/LFO"));
    settle (300);

    if (auto* page = tabs->getCurrentContentComponent())
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
    tabs->setCurrentTabIndex (tabIndex ("ENV/LFO"));
    settle (200);

    if (auto* page = tabs->getCurrentContentComponent())
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
        tabs->setCurrentTabIndex (tabIndex ("MAIN"));
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

        expect (shownOnMain ("sub_level") && ! shownOnMain ("osc4_level"),
                "MAIN shows OSC 3 and hides OSC 4 by default");

        for (int osc = 3; osc < OscillatorIds::count; ++osc)
            processor.addOscillator (osc);

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
            if (auto* mode = processor.apvts.getParameter (prefix + "_mode"))
                mode->setValueNotifyingHost (mode->convertTo0to1 (3.0f));

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

    // An empty FX rack offers one-click effects.
    {
        for (int slot = 1; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
            processor.assignFxSlot (slot, 0);

        tabs->setCurrentTabIndex (tabIndex ("FX"));
        settle (500);
        std::vector<juce::TextButton*> textButtons;
        findAll<juce::TextButton> (*editor, textButtons);
        juce::TextButton* reverb = nullptr;

        for (auto* button : textButtons)
            if (button->getButtonText().contains ("REVERB") && visibleInTree (button))
                reverb = button;

        expect (reverb != nullptr, "the empty rack shows quick-add buttons");

        if (reverb != nullptr)
        {
            reverb->triggerClick();
            settle (400);
            const auto* slot1 = processor.apvts.getRawParameterValue ("fx_slot1");
            expect (slot1 != nullptr && (int) slot1->load() == 13, "quick-add REVERB puts a reverb in slot 1");
            expect (! reverb->isVisible(), "quick-add buttons hide once the rack has an effect");
        }
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

    editor.reset();
    std::cout << (uiFailures == 0 ? "UI TESTS PASSED" : "UI TESTS FAILED") << " (" << uiFailures << " failures)" << std::endl;
    return uiFailures == 0 ? 0 : 1;
}
} // namespace

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

    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    outDir.createDirectory();

    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    if (argc > 2)
        processor.loadFactoryPreset (juce::String (argv[2]).getIntValue());

    // Shows the SEQ tab, which is hidden until a step LFO is in use.
    if (auto* shape = processor.apvts.getParameter ("lfo4_shape"))
        shape->setValueNotifyingHost (shape->convertTo0to1 (7.0f));

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1060, 720);
    settle (400);

    if (auto* tutorial = findChild<TutorialOverlay> (*editor); tutorial != nullptr && tutorial->isVisible())
    {
        save (*editor, outDir.getChildFile ("00-tutorial.png"));
        tutorial->setVisible (false);
    }

    auto* tabs = findChild<juce::TabbedComponent> (*editor);

    if (tabs == nullptr)
        return 1;

    for (int i = 0; i < tabs->getNumTabs(); ++i)
    {
        tabs->setCurrentTabIndex (i);
        settle (450);
        const auto stem = juce::String (i + 1).paddedLeft ('0', 2) + "-" + tabs->getTabNames()[i].replaceCharacter ('/', '-');
        save (*editor, outDir.getChildFile (stem + ".png"));

        if (tabs->getTabNames()[i] == "OSC")
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
                if (auto* viewport = dynamic_cast<juce::Viewport*> (tabs->getCurrentContentComponent()))
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

        if (auto* page = tabs->getCurrentContentComponent())
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
        if (auto* page = tabs->getCurrentContentComponent())
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
        if (tabs->getTabNames()[i] == "FX")
        {
            for (int type = 1; type <= IlanaSynthAudioProcessor::numFxTypes; ++type)
            {
                processor.assignFxSlot (1, type);
                settle (900);
                save (*editor, outDir.getChildFile ("fx-" + juce::String (type).paddedLeft ('0', 2) + ".png"));
            }
        }
    }

    // The drawable Curve LFO editor.
    if (auto* shape = processor.apvts.getParameter ("lfo1_shape"))
    {
        tabs->setCurrentTabIndex (tabs->getTabNames().indexOf ("ENV/LFO"));
        if (auto* page = tabs->getCurrentContentComponent())
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
        tabs->setCurrentTabIndex (tabs->getTabNames().indexOf ("ENV/LFO"));

        if (auto* page = tabs->getCurrentContentComponent())
            if (auto* thumbs = findChild<LfoThumbBar> (*page); thumbs != nullptr && thumbs->onSelect != nullptr)
                thumbs->onSelect (0);

        settle (500);
        save (*editor, outDir.getChildFile ("lfo-curve.png"));
    }

    // The LFO pool, every card revealed, the last one selected.
    {
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, true);
        tabs->setCurrentTabIndex (tabs->getTabNames().indexOf ("ENV/LFO"));
        if (auto* page = tabs->getCurrentContentComponent())
            if (auto* thumbs = findChild<LfoThumbBar> (*page); thumbs != nullptr && thumbs->onSelect != nullptr)
                thumbs->onSelect (13);
        settle (400);
        save (*editor, outDir.getChildFile ("lfo-pool-full.png"));
        tabs->setCurrentTabIndex (tabs->getTabNames().indexOf ("MAIN"));
        settle (300);
        save (*editor, outDir.getChildFile ("lfo-pool-main.png"));
        for (int lfo = 3; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, false);
    }

    // M4: the Grand Piano preset on the OSC page.
    if (const auto program = processor.getFactoryPresetNames().indexOf ("Grand Piano"); program >= 0)
    {
        processor.loadFactoryPreset (program);
        tabs->setCurrentTabIndex (tabs->getTabNames().indexOf ("OSC"));
        settle (500);
        save (*editor, outDir.getChildFile ("keys-grand-osc.png"));
        if (auto* viewport = dynamic_cast<juce::Viewport*> (tabs->getCurrentContentComponent()))
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
            tabs->setCurrentTabIndex (tabs->getTabNames().indexOf (page));
            settle (400);
            save (*editor, outDir.getChildFile ("added-osc-" + juce::String (page) + ".png"));

            if (auto* content = tabs->getCurrentContentComponent())
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
