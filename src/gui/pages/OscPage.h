// The oscillator cards (OscPage, in PLAY's OscPageViewport) and what PLAY and
// FM share about an oscillator's role (OscRole). The classes are declared
// here; their member functions are in OscPage.cpp.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../PluginEditor.h"
#include "../../PluginProcessor.h"
#include "../../SectionColours.h"
#include "../AnimationUtils.h"
#include "../FmDiagram.h"
#include "../FmOperatorInfo.h"
#include "../HeldSliderAttachment.h"
#include "../IlanaLookAndFeel.h"
#include "../LfoShapeMenu.h"
#include "../MatrixWidgets.h"
#include "../ModalMarks.h"
#include "../ModuleMenu.h"
#include "../OperatorEnvDisplay.h"
#include "../OscPicker.h"
#include "../ParamControls.h"
#include "../SectionColourPicker.h"
#include "../PhysicalApplies.h"
#include "../PhysicalView.h"
#include "../SampleNameBox.h"
#include "../TableBrowser.h"
#include "../WaveDisplay.h"
#include "PageHelpers.h"

// What an oscillator is in the patch's FM, read the same way by PLAY's
// strips and OSC's cards, so an operator reads as one (its ratio, its
// Operator EG, what it modulates) wherever it shows (UI review 6, V3, S3,
// I6-5).
namespace OscRole
{
inline juce::String prefix (int osc) { return OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, osc)]; }

inline float read (const IlanaSynthAudioProcessor& p, const juce::String& id)
{
    const auto* value = p.apvts.getRawParameterValue (id);
    return value != nullptr ? value->load() : 0.0f;
}

// ENVELOPE choice 17 (appended): the Operator EG.
inline constexpr int operatorEgChoice = 17;

inline bool usesOperatorEg (const IlanaSynthAudioProcessor& p, int osc)
{
    return juce::roundToInt (read (p, prefix (osc) + "_amp_env")) == operatorEgChoice;
}

inline int tuning (const IlanaSynthAudioProcessor& p, int osc)
{
    return juce::jlimit (0, OscTuning::Count - 1, juce::roundToInt (read (p, prefix (osc) + "_tune")));
}

// The oscillators this one modulates (its feedback aside).
inline std::vector<int> targets (const IlanaSynthAudioProcessor& p, int osc)
{
    std::vector<int> result;

    for (int target = 0; target < OscillatorIds::count; ++target)
        if (target != osc && p.isOscillatorShown (target) && read (p, FmDiagram::routeId (osc, target)) > 0.001f)
            result.push_back (target);

    return result;
}

// The oscillators that modulate this one.
inline std::vector<int> sources (const IlanaSynthAudioProcessor& p, int osc)
{
    std::vector<int> result;

    for (int source = 0; source < OscillatorIds::count; ++source)
        if (source != osc && p.isOscillatorShown (source) && read (p, FmDiagram::routeId (source, osc)) > 0.001f)
            result.push_back (source);

    return result;
}

inline int mode (const IlanaSynthAudioProcessor& p, int osc)
{
    return juce::jlimit (0, OscMode::count - 1, juce::roundToInt (read (p, prefix (osc) + "_mode")));
}

// An operator is an oscillator of the FM / DX7 type (2026-10-06; before, a
// wavetable tuned by ratio or fixed Hz or on the Operator EG, which now loads
// as one). One in an FM route but of another type stays an oscillator with
// FM, as on Vital (UI review 7, V7-15).
inline bool isOperator (const IlanaSynthAudioProcessor& p, int osc)
{
    return mode (p, osc) == OscMode::fmOperator;
}

// The type menu's order: FM / DX7 next to Wavetable (both play tables);
// the saved index is unchanged.
inline const std::array<int, OscMode::count>& modeMenuOrder()
{
    static const std::array<int, OscMode::count> order { OscMode::wavetable, OscMode::fmOperator, OscMode::physical,
                                                         OscMode::sample, OscMode::granular, OscMode::live };
    return order;
}

inline juce::String modeName (int mode)
{
    return OscMode::names[(size_t) juce::jlimit (0, OscMode::count - 1, mode)];
}

// Picks an oscillator's type as one undo step, through the processor (so
// leaving FM / DX7 drops the operator's tuning and Operator EG).
inline void chooseMode (IlanaSynthAudioProcessor& p, int osc, int newMode)
{
    if (newMode < 0 || newMode >= OscMode::count || newMode == mode (p, osc))
        return;
    p.performEdit ("OSC " + juce::String (osc + 1) + " type " + modeName (newMode),
                   [&p, osc, newMode] { p.setOscillatorMode (osc, newMode); });
}

// A type menu (oscN_mode) that picks through chooseMode: its list in the
// menu's order, the arrow keys step through the same order.
inline void bindModeMenu (ComboControl& control, IlanaSynthAudioProcessor& p, int osc)
{
    auto* box = &control.getComboBox();
    control.setPopupOverride ([box, &p, osc]
    {
        juce::PopupMenu menu;
        const auto current = mode (p, osc);
        for (const auto entry : modeMenuOrder())
            menu.addItem (entry + 1, modeName (entry), true, entry == current);

        juce::Component::SafePointer<juce::ComboBox> safe (box);
        menu.showMenuAsync (IlanaTheme::menuOptions (box), [safe, &p, osc] (int result)
        {
            if (safe != nullptr && result > 0)
                chooseMode (p, osc, result - 1);
        });
    });
    control.setNudgeOverride ([&p, osc] (int step)
    {
        const auto& order = modeMenuOrder();
        const auto at = (int) (std::find (order.begin(), order.end(), mode (p, osc)) - order.begin());
        const auto next = juce::jlimit (0, OscMode::count - 1, at + step);
        chooseMode (p, osc, order[(size_t) next]);
    });
}

// The operator's own choices in an oscillator's TUNING and ENVELOPE menus:
// Ratio, Fixed Hz and OP ENV belong to FM / DX7. Elsewhere TUNING keeps only
// Semitones (Ratio and Fixed Hz greyed, so the menu says where they are),
// and a Wavetable's ENVELOPE has no OP ENV. The saved indices are unchanged.
// `current`: the ENVELOPE parameter's index, shown once the menu is rebuilt
// (its attachment could not show OP ENV while the item was missing).
inline void showOperatorChoices (juce::ComboBox* tune, juce::ComboBox* envelope, bool asOperator, bool envelopeChoice, int current)
{
    if (tune != nullptr)
        for (const auto choice : { OscTuning::Ratio, OscTuning::Fixed })
            if (tune->isItemEnabled (choice + 1) != asOperator)
                tune->setItemEnabled (choice + 1, asOperator);

    if (envelope == nullptr)
        return;

    const auto id = OperatorEg::envelopeChoice + 1;
    // (An oscillator that is not an operator but still holds OP ENV keeps the item, so the menu
    // shows its setting rather than a blank: N16-3.)
    envelopeChoice = envelopeChoice || current == OperatorEg::envelopeChoice;
    const auto has = envelope->indexOfItemId (id) >= 0;
    if (has == envelopeChoice)
    {
        if (envelope->getSelectedId() != current + 1 && envelope->indexOfItemId (current + 1) >= 0)
            envelope->setSelectedId (current + 1, juce::dontSendNotification);
        return;
    }

    // Rebuilt in place, with any section headings, so the saved index keeps
    // its item.
    struct Entry { juce::String text; int itemId; bool heading; };
    std::vector<Entry> entries;
    for (juce::PopupMenu::MenuItemIterator it (*envelope->getRootMenu(), false); it.next();)
    {
        const auto& item = it.getItem();
        if (item.itemID == id || (item.isSectionHeader && item.text == "FM PAGE"))
            continue;
        entries.push_back ({ item.text, item.itemID, item.isSectionHeader });
    }
    const auto sectioned = std::any_of (entries.begin(), entries.end(), [] (const Entry& e) { return e.heading; });
    envelope->clear (juce::dontSendNotification);
    for (const auto& entry : entries)
    {
        if (entry.heading)
            envelope->addSectionHeading (entry.text);
        else if (entry.itemId != 0)
            envelope->addItem (entry.text, entry.itemId);
    }
    if (envelopeChoice)
    {
        if (sectioned)
            envelope->addSectionHeading ("FM PAGE");
        envelope->addItem ("OP ENV", id);
    }
    envelope->setSelectedId (current + 1, juce::dontSendNotification);
}

// "OSC 2 30 %": the FM routes' depths named beside the oscillators, so the
// caption on OSC carries the number the FM page and the matrix edit (review
// 11, I11-4). `from`: the routes into osc, else out of it.
inline juce::String depthList (const IlanaSynthAudioProcessor& p, int osc, const std::vector<int>& others, bool from)
{
    juce::StringArray names;
    for (const auto other : others)
        names.add ("OSC " + juce::String (other + 1) + juce::String::fromUTF8 (" \xc2\xb7 ")
                   + juce::String (juce::roundToInt (read (p, from ? FmDiagram::routeId (other, osc) : FmDiagram::routeId (osc, other)) * 100.0f)) + "%");
    return names.joinIntoString (", ");
}

// "OUT", "MOD → 1, 3", "OUT, MOD → 2" or "SILENT": an oscillator's part in
// the FM routing, in the FM diagram's words, whether or not it is an
// operator (I8-19); empty for an oscillator in no FM route.
inline juce::String describe (const IlanaSynthAudioProcessor& p, int osc)
{
    const auto modulated = targets (p, osc);
    if (! isOperator (p, osc) && modulated.empty() && sources (p, osc).empty())
        return {};

    const auto out = read (p, prefix (osc) + "_out") > 0.5f;
    if (modulated.empty())
        return out ? "OUT" : "SILENT";

    juce::StringArray numbers;
    for (const auto target : modulated)
        numbers.add (juce::String (target + 1));

    // The arrow the matrix uses, not a greater-than sign (review 11, I11-16).
    return juce::String (out ? "OUT, MOD " : "MOD ") + juce::String::fromUTF8 ("\xe2\x86\x92 ") + numbers.joinIntoString (", ");
}

inline const char* outputKnobName (const IlanaSynthAudioProcessor& p, int osc)
{
    juce::ignoreUnused (p, osc);
    return "OUTPUT";
}

// PLAY's strip role line: what the oscillator does with its OUTPUT. A carrier
// goes "TO OUTPUT", a modulator "MODULATES 1, 3" (I12-3); the tabs keep the
// short form of describe().
inline juce::String describeLong (const IlanaSynthAudioProcessor& p, int osc);

// The same in words, for tooltips and the OSC card's header.
inline juce::String describeLong (const IlanaSynthAudioProcessor& p, int osc)
{
    if (! isOperator (p, osc))
    {
        juce::StringArray parts;
        if (const auto from = sources (p, osc); ! from.empty())
            parts.add ("FM FROM " + depthList (p, osc, from, true)); // upper case as on the tab (review 8, S8-30)
        if (const auto into = targets (p, osc); ! into.empty())
            parts.add ("FM INTO " + depthList (p, osc, into, false));
        return parts.joinIntoString (", ");
    }

    const auto out = read (p, prefix (osc) + "_out") > 0.5f;
    const auto modulated = targets (p, osc);
    juce::StringArray parts;
    parts.add ("FM operator");

    if (out)
        parts.add (modulated.empty() ? "a carrier (heard)" : "heard");
    else if (modulated.empty())
        parts.add ("silent (OUT off, modulates nothing)");

    if (! modulated.empty())
    {
        juce::StringArray names;
        for (const auto target : modulated)
            names.add ("OSC " + juce::String (target + 1));
        parts.add ("modulates " + names.joinIntoString (", "));
    }

    if (usesOperatorEg (p, osc))
        parts.add ("plays its OP ENV");

    return parts.joinIntoString (", ");
}
} // namespace OscRole

// A frame scrubber under the wave picture (review 12, S12-2): 64 ticks across
// the table, the thumb on the oscillator's FRAME; a drag sets it.
class FrameScrubber : public juce::Slider
{
public:
    FrameScrubber();

    void setColour (juce::Colour newColour) { colour = newColour; repaint(); }

    void paint (juce::Graphics& g) override;

private:
    juce::Colour colour = IlanaTheme::accent();
};

// An operator's Operator Env as a small picture, where a plain oscillator
// shows its wave (PLAY's strips: UI review 7, I7-19): the engine's own run
// at C3, time on the graphs' square-root scale, levels down to -60 dB.
struct OperatorEnvThumb
{
    // Re-reads the operator; true when its shape changed.
    bool update (const IlanaSynthAudioProcessor& p, int osc);

    void paint (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, bool lit) const;

    OperatorEnv::Settings settings;
    bool valid = false;
    std::vector<juce::Point<float>> points;
    float keyUp = 0.5f;
};

// EXCITE's menu in groups, under the one set of exciter names (UI review 7,
// I7-27). The items keep their ids, so the saved choice is unchanged.
inline void groupExciteMenu (ComboControl& control)
{
    auto& box = control.getComboBox();
    // (Read before renaming: getSelectedId() only matches while the shown
    // text equals the item's text. Re-selected so the box shows the new name.)
    const auto selectedId = box.getSelectedId();

    for (int excite = 0; excite < juce::jmin (box.getNumItems(), Exciters::names().size()); ++excite)
        box.changeItemText (excite + 1, Exciters::name (excite));

    box.setSelectedId (0, juce::dontSendNotification);
    box.setSelectedId (selectedId, juce::dontSendNotification);

    control.setPopupOverride ([combo = &box]
    {
        juce::PopupMenu menu;

        for (const auto& [group, members] : Exciters::groups())
        {
            menu.addSectionHeader (group);

            for (const auto excite : members)
                menu.addItem (excite + 1, Exciters::name (excite), true, combo->getSelectedId() == excite + 1);
        }

        juce::Component::SafePointer<juce::ComboBox> safe (combo);
        menu.showMenuAsync (IlanaTheme::menuOptions (combo), [safe] (int result)
        {
            if (safe != nullptr && result > 0)
                safe->setSelectedId (result);
        });
    });
}

// The TABLE (FM's WAVE) menu under the table browser's names: "Analog Saw", "Drive Saw", "Hard Sync", not the stored
// "Basic", "DriveSaw", "HardSync" (UI review 31, item 6: one name for one thing). The items keep their ids, so the saved
// choice is unchanged.
inline void nameTableMenu (ComboControl& control)
{
    auto& box = control.getComboBox();
    const auto selectedId = box.getSelectedId();
    const auto names = TableFactory::getFactoryTableNames();

    for (int index = 0; index < juce::jmin (box.getNumItems(), names.size()); ++index)
        box.changeItemText (index + 1, TableBrowser::displayName (index, names[index]));

    box.setSelectedId (0, juce::dontSendNotification);
    box.setSelectedId (selectedId, juce::dontSendNotification);
}

// WARP's menu in groups, and FM and Ring saying which oscillator they use
// (review 23: they read a fixed one, OSC 2 into OSC 1 and OSC 1 into the others, and
// the menu never said). The items keep their ids, so the saved choice is unchanged.
inline void groupWarpMenu (ComboControl& control, int osc, std::function<bool (int)> isShown, bool stageTwo = false)
{
    auto& box = control.getComboBox();
    const auto source = osc == 0 ? 1 : 0;
    const auto selectedId = box.getSelectedId();
    const auto fmText = "FM from OSC " + juce::String (source + 1);
    const auto ringText = "Ring with OSC " + juce::String (source + 1);

    // A mode's place in this menu: WARP lists the modes in their own order;
    // WARP 2 has FM, Ring and FM Noise appended (UI review 30: its menu
    // never named the oscillator they read).
    const auto choiceFor = [stageTwo] (int mode)
    {
        if (! stageTwo)
            return mode;
        for (int choice = 0; choice < Warp::getStageTwoNames().size(); ++choice)
            if (Warp::modeForStageTwoChoice (choice) == mode)
                return choice;
        return 0;
    };

    // (WARP 2's name is longer and its menu no wider, so the menu itself shows the short form, "FM OSC 2",
    // behind the oscillator's dot; the list it opens spells both out.)
    const auto osc2 = "OSC " + juce::String (source + 1);
    if (box.getNumItems() > choiceFor (Warp::NoiseFm))
        box.changeItemText (choiceFor (Warp::NoiseFm) + 1, stageTwo ? "FM Noise" : "FM from Noise");

    if (box.getNumItems() > choiceFor (Warp::Ring))
    {
        box.changeItemText (choiceFor (Warp::Fm) + 1, stageTwo ? "FM " + osc2 : fmText);
        box.changeItemText (choiceFor (Warp::Ring) + 1, stageTwo ? "Ring " + osc2 : ringText);
        box.setSelectedId (0, juce::dontSendNotification);
        box.setSelectedId (selectedId, juce::dontSendNotification);
    }

    box.getProperties().set ("valueDot", (int) IlanaTheme::oscColour (source).getARGB());
    box.getProperties().set ("valueDotPrefixes", "FM from OSC|Ring with|FM OSC|Ring OSC");

    control.setPopupOverride ([combo = &box, source, osc, isShown, fmText, ringText, choiceFor]
    {
        juce::PopupMenu menu;
        const auto current = combo->getSelectedId();
        const auto add = [&] (int mode, const juce::String& text, const juce::String& note = {}, bool enabled = true)
        {
            juce::PopupMenu::Item item (text);
            item.itemID = choiceFor (mode) + 1;
            item.isEnabled = enabled;
            item.isTicked = current == choiceFor (mode) + 1;
            item.shortcutKeyDescription = note;
            menu.addItem (item);
        };
        const auto names = Warp::getNames();
        const auto name = [&] (int mode) { return names[mode]; };

        add (Warp::Off, "Off");
        menu.addSectionHeader ("PHASE");
        for (int mode = Warp::Sync; mode <= Warp::Quantize; ++mode)
            add (mode, name (mode));
        menu.addSectionHeader ("PHASE DISTORTION");
        for (int mode = Warp::PdSaw; mode <= Warp::PdRes3; ++mode)
            add (mode, name (mode));
        menu.addSectionHeader ("SHAPERS");
        add (Warp::Formant, name (Warp::Formant));
        add (Warp::Squeeze, name (Warp::Squeeze));
        menu.addSectionHeader ("USES ANOTHER OSCILLATOR");
        const auto arrow = juce::String::fromUTF8 (" \xe2\x86\x92 ");
        const auto note = "OSC " + juce::String (source + 1) + arrow + "OSC " + juce::String (osc + 1);
        add (Warp::Fm, fmText, note, isShown (source));
        add (Warp::Ring, ringText, "OSC " + juce::String (source + 1) + juce::String::fromUTF8 (" \xc3\x97 ") + "OSC " + juce::String (osc + 1), isShown (source));
        add (Warp::NoiseFm, "FM from Noise", "NOISE" + arrow + "OSC " + juce::String (osc + 1));

        juce::Component::SafePointer<juce::ComboBox> safe (combo);
        menu.showMenuAsync (IlanaTheme::menuOptions (combo), [safe] (int result)
        {
            if (safe != nullptr && result > 0)
                safe->setSelectedId (result);
        });
    });
}

// The unison block's little picture (the OSC cards' UNISON row): one bar per
// voice, spread across the width as far as DETUNE reaches, the outer voices
// shorter. A picture only; it takes no clicks.
class UnisonSpreadView : public juce::Component, public juce::SettableTooltipClient
{
public:
    UnisonSpreadView (IlanaSynthAudioProcessor& p, int index);

    void setColour (juce::Colour newColour);

    // Re-reads the voices, the detune, the stereo spread and the blend; repaints when they changed.
    void update();

    // The voices as a graph (UI review 31, item 14): one bar per voice, across by its place in the stereo field
    // (SPREAD), up for sharp and down for flat by its detune (DETUNE), the side voices as bright as BLEND lets them be.
    void paint (juce::Graphics& g) override;

private:
    // The parameter with what modulates it right now, as the voices hear it
    // (DETUNE moved by an LFO or envelope spreads the bars).
    float read (const char* suffix) const;

    float norm (const char* suffix, float plain) const;

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix;
    juce::Colour colour = IlanaTheme::accent();
    int voices = 0;
    float spread = -1.0f, pan = -1.0f, blend = -1.0f;
    bool isActive = false;
};

// A Physical oscillator's card, right of its string and exciter: the
// string's partials as bars (their levels from the strike point, DAMP and
// STIFF). A picture only.
class StringPartialsView : public juce::Component,
                           public juce::SettableTooltipClient,
                           private IlanaAnim::FrameTimer
{
public:
    StringPartialsView (IlanaSynthAudioProcessor& p, int index);

    void setColour (juce::Colour newColour);

    // A compact view has no title of its own (its panel labels it): the bars fill it.
    void setCompact (bool shouldBeCompact);

    // Re-reads the settings (the frame timer also does, so the bars glide).
    void update();

    void paint (juce::Graphics& g) override;

private:
    bool compact = false;

    float read (const char* suffix) const;

    void timerCallback() override { follow (frameSeconds()); }

    // Eases the shown settings to the real ones (a quick glide, so a knob
    // drag reads as motion), and counts down how long the caption stays.
    void follow (float seconds);

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix;
    juce::Colour colour = IlanaTheme::accent();
    std::array<float, 3> target { -1.0f, -1.0f, -1.0f }, eased { -1.0f, -1.0f, -1.0f }, lastTarget { -2.0f, -2.0f, -2.0f };
    int moving = -1;
    float hold = 0.0f;
    juce::String caption;
};

// The right-hand cells of an oscillator's PITCH row: an oscillator in an FM
// route says so in a chip in its source's colour (a link to the FM page), one
// in none shows its output level as a bar.
class OscStatusView : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    OscStatusView (IlanaSynthAudioProcessor& p, int index);

    std::function<void()> onClick;

    // Which half it draws: the FM chip, the output bar, or (as before) the chip
    // else the bar. The packed cards keep the chip in the header and the bar in MIX.
    enum class Part { either, chipOnly, barOnly, meterCell };
    // meterCell: laid out as a knob beside LEVEL and PAN (the Physical MIX, UI review 28): the level over a short
    // bar on the dials' middle line, OUT on the knob names' line. Local y of both.
    void setCellLines (float dialMiddle, float nameTopIn) { cellMiddle = dialMiddle; nameTop = nameTopIn; repaint(); }
    void setPart (Part newPart) { part = newPart; repaint(); }

    // Re-reads the role and the level; repaints when they changed.
    void update (const juce::String& chipTextIn, juce::Colour chipColourIn, bool isLink, const juce::String& tip);

    void setColour (juce::Colour newColour);

    bool hitTest (int, int) override { return link && chipText.isNotEmpty(); }

    void mouseUp (const juce::MouseEvent& event) override;

    void paint (juce::Graphics& g) override;

private:
    IlanaSynthAudioProcessor& processorRef;
    int osc;
    juce::Colour colour, chipColour;
    juce::String chipText;
    bool link = false;
    float level = -1.0f;
    Part part = Part::either;
    float valueWidth = 28.0f;
    float cellMiddle = 14.0f, nameTop = 30.0f;
};

// A small quiet action in the packed cards' panels (+ WARP): dashed, in the
// packed name size. Solid draws it as a field instead.
class PackedChip : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;

    void setText (const juce::String& newText) { text = newText; repaint(); }
    const juce::String& getText() const { return text; }
    void setSolid (bool shouldBeSolid) { solid = shouldBeSolid; repaint(); }
    int preferredWidth() const { return juce::GlyphArrangement::getStringWidthInt (chipFont(), text) + 14; }

    void paint (juce::Graphics& g) override;

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent& event) override;

private:
    static juce::Font chipFont() { return IlanaTheme::font (7.8f, true).withKerningFactor (0.08f); }
    juce::String text;
    bool solid = false;
};

// The sympathetic strings' six notes (STRINGS' Manual tuning): a small panel
// over the page, above the STRINGS card. A click anywhere outside it closes it.
class TuningPopover : public juce::Component
{
public:
    static constexpr int pad = 8, headerH = 22, dial = 36, cell = 44, height = headerH + 6 + 6 + dial + 10 + pad + 2;
    static constexpr int widthFor (int count) { return pad * 2 + count * cell; }

    ~TuningPopover() override { juce::Desktop::getInstance().removeGlobalMouseListener (this); }

    void visibilityChanged() override;

    std::function<bool (juce::Component*)> isOwnButton; // (the chevron toggles it itself)
    std::function<void()> onClose;

    void paint (juce::Graphics& g) override;

    void resized() override;

private:
    void mouseDown (const juce::MouseEvent& event) override;
};

// PLAY's drop tile (mockup v4, ilana 2026-10-26): with one or two oscillators the room under the cards
// is one dashed tile holding the two add buttons, and an audio file or SoundFont dropped on it lands
// in a new oscillator.
class OscDropTile : public juce::Component,
                    public juce::FileDragAndDropTarget
{
public:
    std::function<void (const juce::File&)> onFileDropped;

    // The add buttons sit on the tile; the caption goes under them.
    void setCaptionTop (int y) { captionTop = y; repaint(); }

    void paint (juce::Graphics& g) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;

    void fileDragEnter (const juce::StringArray&, int, int) override { setDragHover (true); }
    void fileDragExit (const juce::StringArray&) override { setDragHover (false); }

    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void setDragHover (bool hover);

    bool dragHover = false;
    int captionTop = 0;
};

class OscPage : public juce::Component,
                private IlanaTheme::UiScaleListener,
                private IlanaTheme::IdentityColourListener,
                private juce::AudioProcessorValueTreeState::Listener,
                private juce::AsyncUpdater,
                private juce::Timer
{
    struct PhysicalControls
    {
        PhysicalControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefix)
            : stiffness (state, prefix + "_string_stiffness", "STIFF"),
              pickup (state, prefix + "_string_pickup", "PICKUP"),
              click (state, prefix + "_string_click", "CLICK"),
              excitePos (state, prefix + "_string_excite_pos", "EXCITE POS"),
              hardness (state, prefix + "_string_pick_hardness", "HARDNESS"),
              pickPos (state, prefix + "_string_pick_pos", "PICK POS"),
              slap (state, prefix + "_string_slap", "SLAP"),
              bowPressure (state, prefix + "_bow_pressure", "GRIP"),
              bowSpeed (state, prefix + "_bow_speed", "SPEED"),
              bridgeBuzz (state, prefix + "_bridge_buzz", "BRIDGE BUZZ"),
              fretRattle (state, prefix + "_fret_rattle", "FRET RATTLE"),
              hammer (state, prefix + "_hammer_hard", "HAMMER"),
              couple (state, prefix + "_couple", "COUPLING"), // the section says STRING (I8-25)
              damper (state, prefix + "_damper", "DAMPER"),
              registerMap (state, prefix + "_register", "REGISTER"),
              epDistance (state, prefix + "_ep_distance", "DISTANCE"),
              epPosition (state, prefix + "_ep_position", "OFFSET"),
              fbGain (state, prefix + "_fb_gain", "AMP GAIN"),
              fbDistance (state, prefix + "_fb_distance", "DISTANCE"),
              modalSize (state, prefix + "_modal_size", "SIZE"),
              modalScale (state, prefix + "_modal_scale", "SCALE"),
              // RESONANCE (review 23): the patch's sympathetic strings and soundboard model, as the strip below has them.
              resAmount (state, "sym_amount", "AMOUNT"),
              resOn (state, "sym_on", "SYMPATHETIC"),
              resBoard (state, "sb_model", "BOARD") {}

        void setColour (juce::Colour colour)
        {
            for (auto* knob : { &stiffness, &pickup, &click, &excitePos, &hardness, &pickPos, &bowPressure, &bowSpeed,
                                &bridgeBuzz, &fretRattle, &hammer, &couple, &damper, &registerMap,
                                &epDistance, &epPosition, &fbGain, &fbDistance, &modalSize, &modalScale })
                knob->setIdentityColour (colour);
        }

        KnobControl stiffness, pickup, click, excitePos, hardness, pickPos;
        KnobControl bowPressure, bowSpeed, bridgeBuzz, fretRattle;
        KnobControl hammer, couple, damper, registerMap;
        KnobControl epDistance, epPosition; // M7.3 Tine / Reed pickup
        KnobControl fbGain, fbDistance;     // M8.5 feedback amp
        KnobControl modalSize, modalScale;  // the modal exciters' body and string (2026-11-06)
        ToggleControl slap;
        KnobControl resAmount;
        ToggleControl resOn;
        ComboControl resBoard;
    };

    struct OscControls
    {
        OscControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefix, int index)
            : on (state, prefix + "_on", "ON"),
              mode (state, prefix + "_mode", ""),
              table (state, prefix + "_table", "TABLE"),
              excite (state, prefix + "_excite", "EXCITE"),
              level (state, prefix + "_level", "LEVEL"),
              pan (state, prefix + "_pan", "PAN"),
              semi (state, prefix + "_semi", "SEMI"),
              fine (state, prefix + "_fine", "FINE"),
              unison (state, prefix + "_unison", "UNISON"),
              detune (state, prefix + "_detune", "DETUNE"),
              spread (state, prefix + "_spread", "SPREAD"),
              stringDecay (state, prefix + "_string_decay", "DECAY"),
              stringDamp (state, prefix + "_string_damp", "DAMP"),
              stringSustain (state, prefix + "_string_sustain", "SUSTAIN"),
              sampleTuned (state, prefix + "_sample_tuned", "TUNED"),
              sampleLoop (state, prefix + "_sample_loop", "LOOP"),
              sampleReverse (state, prefix + "_sample_reverse", "REVERSE"),
              sampleLoopOwn (state, prefix + "_sample_loop_own", "LOOP PTS"),
              sampleSnap (state, prefix + "_sample_snap", "SNAP"),
              sampleLoopXfade (state, prefix + "_sample_loop_xfade", "CROSSFADE"),
              samplePlayback (state, prefix + "_sample_playback", "PLAYBACK"),
              samplePingPong (state, prefix + "_sample_pingpong", "PING-PONG"),
              sampleStartSafe (state, prefix + "_sample_start_safe", "TRANSIENT SAFE"),
              sampleStartRandom (state, prefix + "_sample_start_rand", "RANDOM START"),
              sampleQuality (state, prefix + "_sample_quality", "QUALITY"),
              sampleStart (state, prefix + "_sample_start", "START"),
              sampleEnd (state, prefix + "_sample_end", "END"),
              sampleFadeIn (state, prefix + "_sample_fade_in", "FADE IN"),
              sampleFadeOut (state, prefix + "_sample_fade_out", "FADE OUT"),
              sampleRoot (state, prefix + "_sample_root", "ROOT"),
              sampleSync (state, prefix + "_sample_sync", "SYNC"),
              sampleStretch (state, prefix + "_sample_stretch", "TIME"),
              sampleBpm (state, prefix + "_sample_bpm", "BPM"),
              chord (state, prefix + "_chord", "CHORD"),
              ampEnv (state, prefix + "_amp_env", "ENVELOPE"),
              warp (state, prefix + "_warp", "WARP"),
              uniMode (state, prefix + "_uni_mode", "UNI MODE"),
              warpAmt (state, prefix + "_warp_amt", "WARP AMT"),
              uniBlend (state, prefix + "_uni_blend", "BLEND"),
              uniFrame (state, prefix + "_uni_frame", juce::String::fromUTF8 ("FRAME \xc2\xb1")),
              uniWarp (state, prefix + "_uni_warp", juce::String::fromUTF8 ("WARP \xc2\xb1")),
              spectral (state, prefix + "_spectral", "SPECTRAL"),
              frameBlend (state, prefix + "_frame_blend", "FRAME BLEND"),
              scale (state, prefix + "_scale", "SCALE"),
              scaleRoot (state, prefix + "_scale_root", "ROOT"),
              spectralAmt (state, prefix + "_spectral_amt", "SPEC AMT"),
              grainPosition (state, prefix + "_grain_pos", "POSITION"),
              grainScan (state, prefix + "_grain_scan", "SCAN"),
              grainFreeze (state, prefix + "_grain_freeze", "FREEZE"),
              grainSize (state, prefix + "_grain_size", "SIZE"),
              grainDensity (state, prefix + "_grain_density", "DENSITY"),
              grainSpray (state, prefix + "_grain_spray", "SPRAY"),
              grainPitch (state, prefix + "_grain_pitch", "PITCH RANDOM"),
              grainSpread (state, prefix + "_grain_spread", "STEREO"),
              grainLive (state, prefix + "_grain_live", "LIVE"),
              warp2 (state, prefix + "_warp2", "WARP 2"),
              warp2Amt (state, prefix + "_warp2_amt", "WARP 2 AMT"),
              tune (state, prefix + "_tune", "TUNING"),
              ratio (state, prefix + "_ratio", "RATIO"),
              fixedHz (state, prefix + "_fixed_hz", "FIXED"),
              egOut (state, prefix + "_eg_out", "OUTPUT"), // one level name (UI review 9, I9-7)
              trim (state, prefix + "_level", "VOICE LEVEL"),
              feedback (state, FmDiagram::routeId (index, index), "FEEDBACK"),
              feedbackType (state, prefix + "_fb_type", "FEEDBACK TYPE") {}

        // Every knob in the oscillator's own colour, as on PLAY.
        void setColour (juce::Colour colour)
        {
            for (auto* knob : { &egOut, &trim, &feedback, &ratio, &fixedHz, &warp2Amt, &level, &pan, &semi, &fine, &unison, &detune, &spread,
                                &stringDecay, &stringDamp, &stringSustain, &sampleStart, &sampleEnd, &sampleFadeIn,
                                &sampleLoopXfade, &sampleStartRandom, &sampleRoot, &sampleFadeOut, &sampleBpm, &warpAmt, &uniBlend, &uniFrame, &uniWarp, &spectralAmt, &grainPosition, &grainScan, &grainSize,
                                &grainDensity, &grainSpray, &grainPitch, &grainSpread })
                knob->setIdentityColour (colour);
        }

        ToggleControl on, sampleTuned, sampleLoop, sampleReverse, sampleLoopOwn, sampleSnap;
        ComboControl mode, table, excite, chord, ampEnv, warp, uniMode, spectral, frameBlend, scale, scaleRoot;
        // M6: the PD chain's second stage and the warp (DCW) envelope.
        ComboControl warp2;
        KnobControl warp2Amt;
        KnobControl level, pan, semi, fine, unison, detune, spread;
        KnobControl stringDecay, stringDamp, stringSustain;
        KnobControl sampleStart, sampleEnd, sampleFadeIn, sampleFadeOut, sampleLoopXfade, sampleRoot;
        // Step 40: the sample follows the host tempo.
        ComboControl sampleSync, sampleStretch;
        // The sampler steps: the PLAYBACK row (33...).
        ComboControl samplePlayback, sampleQuality;
        ToggleControl samplePingPong, sampleStartSafe;
        KnobControl sampleStartRandom;
        KnobControl sampleBpm;
        KnobControl warpAmt, uniBlend, uniFrame, uniWarp, spectralAmt;
        KnobControl grainPosition, grainScan, grainSize, grainDensity, grainSpray, grainPitch, grainSpread;
        ToggleControl grainFreeze, grainLive; // M7.5: grains from the live input (ilanaSynth FX)
        // An FM operator's tuning, as on the FM page (UI review 6, I6-5).
        ComboControl tune;
        KnobControl ratio, fixedHz;
        // An operator on the Operator Env: its OUTPUT (dB) and the
        // oscillator's LEVEL, as the FM card names them, and its
        // feedback (UI review 7, I7-2, I7-20).
        KnobControl egOut, trim, feedback;
        ComboControl feedbackType;
    };

public:
    std::function<void()> onModeChanged;

    explicit OscPage (IlanaSynthAudioProcessor& p);

    // The zoom changed: text is laid out by its drawn width, so the cards lay out again.
    void uiScaleChanged() override;

    ~OscPage() override;

    // Colour coding: an oscillator given another colour shows it on every
    // part of its card.
    void identityColoursChanged() override;

    void parameterChanged (const juce::String&, float) override;

    void handleAsyncUpdate() override;

    // Patch loads change which oscillators are shown; FM routes (polled)
    // change what the cards say.
    void timerCallback() override;

    bool shownRendering = false;

    void paint (juce::Graphics& g) override;

    // A right-click on a card's header: switch it off, duplicate or remove
    // it (the old tab's menu). A drag by the header moves the card (ilana
    // 2026-10-26: "drag to reorder"): a line shows where it lands.
    void mouseDown (const juce::MouseEvent& event) override;

    void mouseDrag (const juce::MouseEvent& event) override;

    void mouseUp (const juce::MouseEvent&) override;

    // An open hand over a header's free space says it can be dragged.
    void mouseMove (const juce::MouseEvent& event) override;

    void paintOverChildren (juce::Graphics& g) override;

    // The card order (the shown oscillators, in slot order) of an oscillator.
    int positionOf (int osc) const;

    // Where a card dragged to y lands: the number of other cards above y's line.
    int dropPosition (int y) const;

    // Duplicate: the copy lands under the original and is selected.
    void duplicate (int osc);

    static juce::Colour oscColour (int index) { return IlanaTheme::oscColour (index); }

    // Where a scroll may come to rest, in page units: each card's top edge (half a gap above it), so a glide
    // stops with a whole card header at the top, never through a label (UI review 28).
    std::vector<float> scrollStops() const;

    // The height the page needs: every shown card at its smallest, the add
    // row and the strip.
    int getMinimumHeight() const;

    // The height the same cards would need as wavetable cards: PLAY sizes its module bands by this, so a taller
    // card (a sample, a string) never squeezes the bands: the oscillator area scrolls instead.
    int getReferenceHeight() const { return referenceHeightFor (numShown()); }

    int referenceHeightFor (int shown) const;

    void resized() override;

    // The oscillator the UI test or the FM page's links pick: its card is
    // scrolled into view.
    void selectOscillator (int osc);

    // PLAY hosts the cards alone: no VOICE / NOISE / SUB / STRINGS strip below them.
    void setEmbedded (bool shouldEmbed);

    // The drop tile: a new oscillator takes the file the way its picture would (a wavetable card asks what the
    // audio is for; a SoundFont loads as a sample).
    void addOscillatorFromFile (const juce::File& file);
    bool hasFreeSlot() const { return canAdd(); }

    // Adds the next oscillator (one undo step): as it was (-1: a Wavetable
    // unless it had a type), or as that type. The add row and the UI tests.
    void addOscillatorOfType (int mode);

    // Removes an oscillator (one undo step); its card folds away.
    void removeOscillatorCard (int osc);

    // The strip under the cards: one row (STEREO, NOISE, SUB, STRINGS,
    // SOUNDBOARD, KEYS); VOICE is in the dock. This scrolls it into view.
    enum SharedTab { sharedVoice = 0, sharedSubNoise, sharedSympathetic, sharedKeys };

    // The card's rectangle in the page (the UI tests).
    juce::Rectangle<int> getCardBounds (int osc) const;

private:
    static constexpr const char* listenedSuffixes[] { "_mode", "_on", "_excite", "_warp", "_warp2", "_spectral", "_tune", "_amp_env" };
    // (Only ids that change the cards' layout or switches: each change rebuilds the page. NOISE LEVEL was here,
    // left from when COLOUR dimmed at level 0, so every step of its knob relaid the page out and lit dimmed
    // controls until the next tick: ilana 2026-11-17, "slow and laggy".)
    static constexpr const char* listenedIds[] { "sym_on", "sym_manual", "sym_count", "sb_on", "subosc_on", "voice_mode" };

    template <typename... Components>
    void addChildComponents (Components&... components)
    {
        (addChildComponent (components), ...);
    }

    static void styleHeaderButton (juce::TextButton& button);

    // The page's measures, in design pixels. Every card is the same width; a
    // wavetable card is the one compact height; the page scrolls once the
    // cards and the strip no longer fit.
    // (Review 23: one margin all round and one gap, as the mockup's 10 px at 1.5x; the
    // header is 23 high.)
    static constexpr int sideMargin = 7, topMargin = 7, bottomMargin = 7, gap = 7;
    static constexpr int cardHeaderHeight = 23, compactCardMin = 118, compactCardMax = 130;
    // The packed cards' panels (wavetable, physical), in units: the mockup's css px / 1.5.
    static constexpr int cardPad = 5, panelGap = 4, panelTop = 5, panelTitleH = 12, panelTail = 4;
    static constexpr int pitchW = 160, mixW = 164, unisonW = 270;
    // (PLAY and OSC draw one card: the same columns on both, ilana 2026-10-21.)
    int pitchColumn() const { return pitchW; }
    int unisonColumn() const { return unisonW; }
    // (AMT's rings 24 wide, their numbers the size the old 20 px ring gave: "100" keeps 3 px from the ring, UI review 31.)
    static constexpr int mainDial = 29, ampDial = 24, ampValueDial = 20, nameLine = 10, knobCell = 35, ampCell = 31, menuH = 16;
    static constexpr int ampRowH = 30, chipH = 14, filterH = 15, outH = 12, spreadH = 17;
    static constexpr int addRowHeight = 18;
    // The strip: one row of cards (ilana's mockup, 2026-10-22), in design px.
    static constexpr int stripCardH = 130, stripHeaderH = 24, stripGap = 6, stripPad = 8, stripDial = 36, stripCell = 44,
                         stripMenuW = 46, stripCellStereo = 42, stripWrapLine = 16, stripDialTop = 36, stripLaneTop = 98, stripLaneH = 22;
    static constexpr int wellWidth = 150, rowLabelWidth = 50, colGap = 6, rowGap = 4;
    static constexpr int sampleRowHeight = 48, sampleLabelWidth = 96, sampleColumns = 7, sampleWaveMin = 190;

    // fm: the FM / DX7 type, the operator's card (its tuning, the Operator EG
    // or another envelope, feedback); wavetable: the table's card.
    enum class Kind { wavetable, fm, physical, sample, grain, live };

    Kind kindOf (int osc) const;

    // One cell of a card's grid: the control, its row, first column and width in columns.
    struct Slot
    {
        juce::Component* item;
        int row, col, span;
    };

    // The compact cards' three rows on one nine-column grid (the sample card's
    // two or three on seven): what stands in each cell. The same column holds
    // the same kind of control from card to card.
    std::vector<Slot> slotsFor (int index) const;

    // A tine's or reed's string and exciter fit one row (eight cells).
    bool physicalRowsMerged (int osc) const;

    // The rows a card shows: a Physical card adds its EXCITER row.
    // An operator's two rows still need the room its feedback knob's rings and the graph's labels take.
    int twoRowRoom (int osc) const { return kindOf (osc) == Kind::fm && rowsOf (osc) == 2 ? 10 : 0; }

    int rowsOf (int osc) const;

    bool showsRendering (int osc) const;

    // A card's row names: a Physical card's from its exciter.
    juce::String rowNameOf (int osc, int row) const;

    // One sentence on what the exciter does, for the EXCITER row's free cells
    // (as the PHYSICAL tab had them, A16-4).
    // The exciter picker's tiles, in the parameter's order: each icon's glyph (18 x 16, from the approved
    // mockup v2, 2026-10-21) and, for its tooltip, the group and what the exciter does.
    static std::vector<IconPicker::Item> exciterIcons();

    // The picker's six type icons (2026-11-11), each tinted by its family (UI review 31, item 42): a type wears
    // its main engine's glyph, and its tooltip names the switch's engines.
    static std::vector<IconPicker::Item> tintedExciterIcons();

    juce::String physicalHint (int osc) const;

    bool unisonShown (int osc) const;

    static juce::String rowName (Kind kind, int row);

    static int numRows (Kind kind, bool unisonRow);

    // One card's measures, laid out by layoutPage and read by paint.
    struct CardGeometry
    {
        int osc = 0;
        Kind kind = Kind::wavetable;
        juce::Rectangle<int> bounds, header, well, wave, group, captionArea, statusArea;
        int rows = 3, columns = 9, rowHeight = 28, gridX = 0, gridWidth = 0, labelX = 0, labelWidth = rowLabelWidth;
        std::array<int, 6> rowY {};
        juce::Rectangle<int> hintArea;
        std::vector<int> separatorY;
        // The packed cards (review 23): the panels, their small names and rules.
        struct Panel { juce::Rectangle<int> area; juce::String title, note; juce::Colour colour; };
        struct Label { juce::Rectangle<int> area; juce::String text; int lines = 1; bool hint = false; float size = 0.0f; bool bright = false; };
        std::vector<Panel> panels;
        std::vector<Label> labels;
    };

    std::vector<CardGeometry> cards;
    juce::Rectangle<int> stripArea, addArea;
    int dragOsc = -1, dropAt = -1; // a card dragged by its header, and where it would land
    bool dragging = false;

    // (A switched-off oscillator keeps its whole card, dimmed, on both tabs; no card folds, the page scrolls
    // instead: ilana 2026-10-21 "dont fold ... just let me scroll", 2026-10-22 "yeah, dim".)

    // The kinds drawn as panels (WAVE, PITCH, MIX, UNISON...): the others keep the grid.
    static bool usesPanels (Kind kind) { return kind == Kind::wavetable || kind == Kind::physical || kind == Kind::live || kind == Kind::fm || kind == Kind::sample || kind == Kind::grain; }

    int warpRowH() const { return ampRowH; }
    int warpChipH() const { return chipH; }
    int warpDial() const { return ampDial; }

    // A wavetable card always shows WARP, SPECTRAL and WARP 2 (Off until picked), so adding a stage never
    // changes the card's height (ilana 2026-10-22: "adding warp 2 changes the size of the osc").
    static constexpr int wavetableWarpRows = 3;
    int waveContentHeight() const { return wavetableWarpRows * warpRowH() + (wavetableWarpRows - 1); }

    static constexpr int physTopH = 84, physBottomH = 57, sampleTopH = 96, sampleBottomH = 65, grainH = 111;
    // The Physical card's top row: TONE, PITCH, MIX and UNISON dials on one line, CHORD, FILTER and the unison mode on another
    // (6 px over the panel's foot, as the FILTER row was).
    static constexpr int physDialY = 19, physMenuY = physTopH - menuH - 6;

    int panelCardHeight (int osc) const;

    // What a card needs at least, by what it holds.
    int minimumCardHeight (int osc) const;

    // What a compact card grows to with spare height.
    int maximumCardHeight (int osc) const;

    static int sampleGroupHeight (int rows) { return 8 + rows * sampleRowHeight + (rows - 1); }

    std::vector<int> shownList() const;

    std::vector<int> minimumHeights() const;

    // (PLAY too: past three cards the add row sits below them and the area scrolls to it, ilana 2026-10-23.)
    bool canAdd() const { return numShown() < OscillatorIds::count; }
    static constexpr int embeddedAddRowBelow = 3;

    int stripHeight() const { return stripCardH; }

    // Embedded on PLAY the cards stand closer (the mockup's 5 unit gaps), so three of them and the two module bands fit.
    int topNow() const { return embedded ? 2 : topMargin; }
    int gapNow() const { return embedded ? 0 : gap; }
    int bottomNow() const { return embedded ? 0 : bottomMargin; }

    int contentHeight (const std::vector<int>& heights) const;

    void layoutPage();

    // A grid cell of a card, in the page.
    juce::Rectangle<int> cellRect (const CardGeometry& card, int row, int col, int span) const;

    void placeSlot (const CardGeometry& card, const Slot& slot) const;

    // The four columns every panel row stands on, in page coordinates: WAVE takes
    // what PITCH, MIX and UNISON leave (the strip below uses the same ones).
    struct Columns { int x[4]; int w[4]; };

    Columns columnsFor (int left, int width, int pitch = 0, int unison = 0) const;

    static juce::Rectangle<int> panelContent (juce::Rectangle<int> panel);

    // A packed knob in a cell of `width` x (dial + name): its bounds with the room for rings.
    static void placeKnob (KnobControl& knob, int x, int y, int width, int dial);

    // One MIX and one UNISON arrangement for every engine but Physical (UI review 31, items 14 and 15): switching
    // the engine no longer moves LEVEL, the FILTER route, OUT or the unison mode menu. `c` is the panel's content.
    // MIX: LEVEL, PAN and ENV on top; FILTER under them; the OUT meter at the foot, up to twice its old height.
    void layoutMixBlock (CardGeometry& card, juce::Rectangle<int> c, KnobControl& level);

    // UNISON: the dials in one row across the panel, then the mode menu (SPREAD named under it) beside the SPREAD
    // picture, which takes the rest of the height: a bar per voice, placed by its pan, as tall as its detune.
    static int filterLabelWidth();
    void layoutUnisonBlock (CardGeometry& card, juce::Rectangle<int> c, std::initializer_list<KnobControl*> knobs);

    // PITCH's lower rows (UI review 31, item 9): SCALE (and its root while a scale is on) over CHORD, so both keep their names.
    void placeScaleChord (int index, int x, int y1, int y2, int width);

    // OCT - and OCT + under the Semitones menu (UI review 31, item 30): SEMI by twelve, within its range.
    void placeOctaveButtons (int index, juce::Rectangle<int> area);

    static int packedButtonWidth (const juce::String& text);
    juce::String shownHeaderWords; // (the header menus' words it was laid out for)
    static bool isBareHeader (ComboControl& menu); // (a header line's bare menu: laid out from its words)

    // The filter route as the bar's five segments: AUTO F1 F2 BOTH NONE.
    static int routeToSegment (int route) { return route == FilterRoute::Filter1 ? 1 : route == FilterRoute::Filter2 ? 2 : route == FilterRoute::Both ? 3 : route == FilterRoute::Direct ? 4 : 0; }
    static int segmentToRoute (int segment) { return segment == 1 ? FilterRoute::Filter1 : segment == 2 ? FilterRoute::Filter2 : segment == 3 ? FilterRoute::Both : segment == 4 ? FilterRoute::Direct : FilterRoute::Default; }

    // The header (every kind): the name, the type bar, the table's menu and steps
    // (a wavetable), the FM chip, then at the right the actions and the switch.
    void layoutCardHeader (CardGeometry& card);

    // A wavetable card as four panels (review 23): WAVE (the picture, FRAME, the warp
    // stages), PITCH, MIX and UNISON, on the page's four columns.
    void layoutWavetablePanels (CardGeometry& card);

    // A physical card as panels (review 23): STRING full height at the left; EXCITER, TONE,
    // RESONANCE and MIX above; PITCH, UNISON and MORE below. Each control is the
    // existing one, placed by the part of the instrument it belongs to.
    // The v2 mockup's card measures (ilana's go, 2026-10-21): positions are the mockup's, in a panel's own
    // coordinates, the mockup's panels starting 28 under the card's top where the app's start 24 (its other
    // cards' header), so every y here is the mockup's less 4. The mockup's card is 1060 wide, the page's 1046:
    // the first column is the 16 px narrower one, PITCH / MIX / UNISON are the same.
    static constexpr int v2Top = 4;
    static juce::Rectangle<int> inPanel (juce::Rectangle<int> panel, int x, int y, int w, int h) { return { panel.getX() + x, panel.getY() + y, w, h }; }

    // A Physical card (v2 mockup): TONE, EXCITER, PITCH, MIX, UNISON on top; RESONANCE, MORE and the STRING picture under them.
    void layoutPhysicalPanels (CardGeometry& card);

    // An FM / DX7 card as four panels (review 23 language): WAVE (the cycle, FEEDBACK, the
    // WAVE and FB TYPE menus), PITCH, MIX and, on the Operator EG, ENVELOPE (its graph and
    // UNISON); on another envelope, UNISON (and the table's shapers while one is in use).
    void layoutFmPanels (CardGeometry& card);

    // A sample card (v2 mockup): WAVE at the left over both rows (the name line, the wave and the keys, the
    // switches in one column beside them); PITCH, MIX and UNISON on the first row; RANGE, TEMPO and PLAYBACK under them.
    void layoutSamplePanels (CardGeometry& card);

    // A granular card (v2 mockup): CLOUD (the picture with FREEZE on it, the six grain knobs in two rows), PITCH,
    // MIX and UNISON on the page's four columns.
    void layoutGranularPanels (CardGeometry& card);

    // A live card as two panels (review 23 language): INPUT, the meter, over the
    // WAVE and PITCH columns; MIX over MIX and UNISON, LEVEL, PAN, ENV and FILTER on one
    // line and OUT under them.
    void layoutLivePanels (CardGeometry& card);

    void layoutCard (CardGeometry& card);

    void paintOscCard (juce::Graphics& g, const CardGeometry& card) const;

    // The panels' fill, a section colour's 2 px top rule, the small name and its note.
    void paintPanels (juce::Graphics& g, const CardGeometry& card) const;

    void paintPanelList (juce::Graphics& g, const std::vector<CardGeometry::Panel>& panels) const;

    // The strip (ilana's mockup, 2026-10-22): one row of six cards under the
    // oscillators, STEREO, NOISE, SUB, STRINGS, SOUNDBOARD and KEYS. Each card
    // has a 24 px header (its name; SUB, STRINGS and SOUNDBOARD their switch at
    // the right), one row of 36 px dials, and a lane at the foot for its menu or
    // switch. The strings' six notes open over the page from Manual tuning.
    std::vector<CardGeometry::Panel> stripPanels;

    static int textWidth (const juce::String& text, float size, bool bold = false);

    static constexpr float stripTitleText = 12.5f, stripLaneText = 11.0f, stripNameText = 11.5f; // (the mockup's sizes at 1060 px)

    // Dials and menus spread over a card with equal space around each, on the card's one dial row.
    void placeStripRow (juce::Rectangle<int> card, std::initializer_list<juce::Component*> items, int cell = stripCell) const;

    // A card's foot: a name, then its menu to the card's right edge.
    void placeLaneMenu (juce::Rectangle<int> card, const juce::String& name, ComboControl& menu);

    void layoutStrip();

    // The strings' six notes, over the cards right above the STRINGS card.
    juce::Rectangle<int> stringsCard;

    juce::Rectangle<int> tuningBounds() const;

    void showTuning (bool show);

public:
    bool isTuningShown() const { return tuningPopover.isVisible(); }
    // Esc, from the editor: closes the tuning pop-over if it is open.
    bool closeTuning();

private:
    void paintStrip (juce::Graphics& g) const;

    // The strip's controls in the packed size, like the cards'.
    void applyStripStyle();

    void scrollTo (int osc);

    juce::String layoutKey() const;

    juce::String shownKey;

    // What a card holds besides the controls every oscillator has: the pictures
    // the modes need (each card has its own, so all can be live at once) and
    // their buttons.
    struct CardAux
    {
        CardAux (IlanaSynthAudioProcessor& p, int index)
            : stringView (p, juce::String (OscillatorIds::prefixes[(size_t) index])), opEnvGraph (p),
              spreadView (p, index), partials (p, index), status (p, index), outBar (p, index),
              xfadeBar (p.apvts, juce::String (OscillatorIds::prefixes[(size_t) index]) + "_sample_loop_xfade", "CROSSFADE") {}

        PhysicalView stringView;
        OperatorEnvDisplay opEnvGraph;
        UnisonSpreadView spreadView;
        StringPartialsView partials;
        OscStatusView status;
        // The packed cards (review 23): the header's type bar and table steps, the
        // MIX panel's filter route and output bar, the WAVE panel's + WARP.
        SegmentBar typeBar, routeBar;
        OscStatusView outBar;
        juce::TextButton prevTable { juce::String::fromUTF8 ("\xe2\x80\xb9") }, nextTable { juce::String::fromUTF8 ("\xe2\x80\xba") };
        IconPicker exciterPills; // (the exciter types as small icons, the name on hover)
        PositionSwitch exciterSwitch; // (the type's engines: MODAL / the older waveguide ones)
        SampleNameBox sampleNameButton;
        BarSliderControl xfadeBar; // the sample's CROSSFADE, a row of the toggle list
        juce::TextButton opEnvButton, sampleLoadButton, sampleSliceButton, sampleExportButton, sampleZonesButton, sampleDetectButton;
        // The header's x: remove this oscillator. Drawn as two strokes, as big as
        // the buttons' capitals (a text x read small).
        struct RemoveButton : juce::TextButton
        {
            void paintButton (juce::Graphics& g, bool over, bool down) override
            {
                getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), over, down);
                const auto centre = getLocalBounds().toFloat().getCentre();
                const auto arm = 3.0f;
                g.setColour (findColour (textColourOffId).withMultipliedAlpha (over ? 1.0f : 0.85f));
                g.drawLine (centre.x - arm, centre.y - arm, centre.x + arm, centre.y + arm, 1.4f);
                g.drawLine (centre.x - arm, centre.y + arm, centre.x + arm, centre.y - arm, 1.4f);
            }
        } removeButton;
        // The header's "..." (UI review 31, item 30): the card's menu (Duplicate, Copy, Paste, Init...) without hunting for
        // a right-click spot. Three dots, as the x draws its strokes.
        struct MoreButton : juce::TextButton
        {
            void paintButton (juce::Graphics& g, bool over, bool down) override
            {
                getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), over, down);
                const auto centre = getLocalBounds().toFloat().getCentre();
                g.setColour (findColour (textColourOffId).withMultipliedAlpha (over ? 1.0f : 0.85f));
                for (int dot = -1; dot <= 1; ++dot)
                    g.fillEllipse (juce::Rectangle<float> (2.2f, 2.2f).withCentre ({ centre.x + 4.0f * (float) dot, centre.y }));
            }
        } moreButton;
        // PITCH's octave steps (UI review 31, item 30): SEMI by twelve.
        juce::TextButton octDown { "OCT -" }, octUp { "OCT +" };
        // A wavetable card's frame in its header (UI review 31, item 16): "FRAME 12 / 64", the frame playing now.
        struct FrameTag : juce::Component, juce::SettableTooltipClient
        {
            void setFrame (int frame, int total)
            {
                if (frame != shownFrame || total != shownTotal)
                {
                    shownFrame = frame;
                    shownTotal = total;
                    repaint();
                }
            }
            juce::String getText() const { return shownTotal > 1 ? "FRAME " + juce::String (shownFrame) + " / " + juce::String (shownTotal) : juce::String(); }
            void paint (juce::Graphics& g) override
            {
                if (shownTotal <= 1)
                    return;
                // (The packed menus' two sizes: the name as "TABLE", the number as the table's name.)
                const auto nameFont = IlanaTheme::font (IlanaLookAndFeel::packedNameText, true).withKerningFactor (0.09f);
                const auto nameW = juce::GlyphArrangement::getStringWidthInt (nameFont, "FRAME") + 5;
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (nameFont);
                g.drawText ("FRAME", getLocalBounds().withWidth (nameW), juce::Justification::centredLeft, false);
                g.setColour (IlanaTheme::Ui::text);
                g.setFont (IlanaTheme::font (IlanaLookAndFeel::packedMenuText, false, true));
                IlanaTheme::drawFitted (g, juce::String (shownFrame) + " / " + juce::String (shownTotal), getLocalBounds().withTrimmedLeft (nameW), juce::Justification::centredLeft, 1);
            }
            int shownFrame = 0, shownTotal = 0;
        } frameTag;
    };

    // Every knob a card can show, for the one inline style.
    static std::vector<KnobControl*> cardKnobs (OscControls& osc);

    std::vector<KnobControl*> stripKnobs();

    bool readBool (const juce::String& id) const;

    float readFloat (const juce::String& id) const;

    bool isOff (int index) const;

    int numShown() const;

    // The list the tests pick an oscillator from (not drawn), each entry's role
    // in its tooltip; also the OUTPUT name of an operator's level.
    void updateTabItems();

    // What the PITCH row's right-hand cells say: the FM role in a chip as
    // long as the cells can hold, else shorter, else none (the output bar).
    void updateStatus (int index);

    // The pictures that follow the sound: the unison's spread, the FM chip and
    // the output bar (polled with the timer).
    void updateCardPictures();

    // An oscillator card's right-click menu.
    // Colour coding: the swatches, by the card's dot.
    void showColourPicker (int band);

    void showOscMenu (int band, juce::Component* target = nullptr);

    WaveDisplay& waveDisplay (int index);

    juce::TextButton& loadButton (int index);

    int getMode (int oscIndex) const;

    void setupLoadButton (juce::TextButton& button, const juce::String& tableId, int tableChoiceOffset);

    void chooseTable (const juce::String& tableId, int tableChoiceOffset, Wavetable::LoadMode mode);

    // Loads a chosen or dropped audio file as a wavetable into a User slot and
    // selects it on the oscillator whose table choice is tableId.
    void loadTableFile (const juce::String& tableId, int tableChoiceOffset, const juce::File& file, Wavetable::LoadMode mode);

    // A file dropped on a wavetable oscillator's picture: the same ways the
    // LOAD button offers, plus using it as a sample.
    void askDroppedFile (int oscIndex, const juce::File& file);

    // Every component of one oscillator's card, to hide before the shown ones
    // are switched on.
    std::vector<juce::Component*> componentsOf (int i);

    // A card's controls in the packed style (the panels) or the grid's inline one: the
    // same controls serve every type, so this runs before each layout.
    void applyCardStyle (int index);

    // The cards show every shown oscillator with its own controls; the strip
    // its groups (the extras only while MORE is open).
    void updateModeVisibility();

    // M7.3: the Tine and Reed excites (7, 8) have their own controls.
    bool isElectric (int index) const;

    static void setGroupEnabled (std::initializer_list<juce::Component*> controls, bool enabled);

    void updateEnabled();
    void dimSampleButtons (int index);

    // The VOICE group's quiet state: the mode, so the tab says what lives in it.
    juce::String voiceModeName() const;

    int readChoice (const juce::String& id) const;

public:
    // M7.4: EDIT. A user table is edited in place; a factory table is first
    // copied into a free patch table, which the oscillator then plays.
    // M8.6: the BOUNCE menu. The choices stay set for the next bounce.
    void showBounceMenu (int index);

    // Step 44: EXPORT's choices, then where to save.
    void showExportMenu (int index);

    void updateBounce();

    void chooseExciter (int index, int choice);

    // The engine switch shows the positions of the type the exciter belongs to.
    void syncExciterSwitch (int index);

    // The header's ‹ ›: what the engine plays, the previous or next one (UI review 31, item 16): a table, FM's wave,
    // or a Physical card's exciter, in the icons' order (so a held note can be heard through them).
    void stepHeader (int index, int step);

    // OCT - / OCT +: SEMI by twelve, inside its range (no parameter of its own; old patches untouched).
    bool canStepOctave (int index, int direction) const;

    void stepOctave (int index, int direction);

    // The header's ‹ ›: the next or previous table in the oscillator's list, as one undo step.
    void stepTable (int index, int step);

    void openTableEditor (int index);

private:
    int readTableChoiceIndex (const juce::String& tableId) const;

    IlanaSynthAudioProcessor& processorRef;
    EffectRules effectRules { processorRef }; // after processorRef, which it reads
    std::array<std::unique_ptr<WaveDisplay>, OscillatorIds::count> waveDisplays;
    std::array<FrameScrubber, OscillatorIds::count> scrubbers;
    std::vector<std::unique_ptr<HeldSliderAttachment>> scrubberAttachments;
    int lastRevealVersion = -1;
    int selected = 0;
    bool embedded = false; // on PLAY: the oscillator cards without the voice strip
    float laidOutScale = 0.0f;

    std::array<std::unique_ptr<juce::TextButton>, OscillatorIds::count> loadButtons, editButtons, bounceButtons;
    std::array<std::unique_ptr<CardAux>, OscillatorIds::count> aux;
    IlanaSynthAudioProcessor::BounceRequest bounceRequest;
    int bouncingOsc = -1, bounceButtonWide = -1;
    DashedAddButton addButton { "+  ADD OSC", "+  ADD OSC" };
    DashedAddButton addFmButton { "+  ADD FM / DX7", "+  ADD FM / DX7" };
    static constexpr int addFmWidth = 150;
    static constexpr int tileMinHeight = 56, tileButtonWidth = 104, tileButtonHeight = 26;
    OscDropTile dropTile;
    juce::TextButton tuneButton { juce::String::fromUTF8 ("\xe2\x80\xba") };
    OscPicker oscTabs;
    std::unique_ptr<juce::FileChooser> tableChooser, exportChooser;
    std::array<std::unique_ptr<PhysicalControls>, OscillatorIds::count> physical;
    // Each oscillator's controls by parameter suffix, for the shared
    // physical list.
    std::array<std::map<juce::String, juce::Component*>, OscillatorIds::count> physicalLookup;
    bool chooserOpen = false;

    // Voice-wide settings that shape how the oscillators stack and drift.
    std::unique_ptr<KnobControl> voiceSpread, unisonRandom, drift, tuneLoose;
    ToggleControl symOn, symManual, looseBoth;
    KnobControl symAmount, symDecay, symCount;

    // Acoustic keys (M4): soundboard, stretch tuning, pedal resonance and
    // the mechanism's noises, shared by every voice.
    ToggleControl sbOn;
    ComboControl sbModel;
    KnobControl sbMix, sbTone, sbSize, stretch, pedalRes, mechKey, mechDamper, mechPedal;
    std::array<std::unique_ptr<KnobControl>, 6> symNotes;

    // The dedicated sub and the noise.
    std::unique_ptr<ToggleControl> subOscOn;
    std::unique_ptr<KnobControl> subOscLevel, noiseStrip, noiseColourStrip;

    std::array<std::unique_ptr<OscControls>, OscillatorIds::count> controls;
    ComboControl subShape, subOctave, noiseType;
    TuningPopover tuningPopover;
    IlanaAnim::PopGrow tuningGrow { tuningPopover }; // (NOTES grows from its button)
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> laneLabels;
};

// The OSC page scrolls only when the card's rows cannot fit at their
// smallest.
class OscPageViewport : public juce::Viewport, private IlanaAnim::FrameTimer // (glides in step with the display's frames)
{
public:
    explicit OscPageViewport (IlanaSynthAudioProcessor& processor);

    OscPage* getPage() const { return dynamic_cast<OscPage*> (getViewedComponent()); }

    // The page is laid out for 1060 design units of width, and its cards pack
    // a lot in; where the window is wider than the packing needs, the whole
    // page is drawn this much larger (the wave column takes the rest), so the
    // labels and dials read like the other tabs'. Where it would have to
    // scroll, it stays at 1.
    static constexpr float maximumContentScale = 1.3f, minimumVirtualWidth = 930.0f;
    // PLAY's copy keeps its cards at their drawn size (the bands under them are not scaled).
    void setMaximumScale (float scale) { maxScale = scale; }
    // PLAY draws its own thin scroll bar over the cards' side margin, so the columns stand where they do without one.
    void setOverlayScroll (bool overlay);

    void resized() override;

    std::function<void()> onViewMoved;

    // The wheel glides: each notch adds to a target the view eases toward, and a hard scroll past either end
    // runs a little over and springs back (ilana 2026-10-22: "smooooth scroll ... bouncing back a bit").
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // Moves the view straight to a place (the scroll bar, a card brought into view), stopping any glide.
    void jumpTo (int y);

    void visibleAreaChanged (const juce::Rectangle<int>& area) override;

private:
    // (1: the cards keep one size on every tab and with any number of oscillators, ilana 2026-10-21; the page
    // scrolls instead of shrinking or growing.)
    float maxScale = 1.0f, chosenScale = 1.0f;
    bool overlayScroll = false;
    juce::uint32 lastWheel = 0;

    static constexpr float wheelPixels = 150.0f, overshootMax = 44.0f, snapRange = 120.0f;
    float current = 0.0f, target = 0.0f, overshoot = 0.0f;
    bool gliding = false;

    int maxScroll() const;

    // The resting place nearest y: either end, or a card edge (OscPage::scrollStops), so the view never stops
    // with a row cut through. Stops farther than snapRange away leave y where it is.
    float nearestStop (float y, float maxY) const;

    // The part of the glide past an end: the page is drawn that far over, the view itself stays at its end.
    void setOvershoot (float pixels);

    void timerCallback() override;
};
