#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

#include "../PluginProcessor.h"
#include "../dsp/LfoShape.h"
#include "../dsp/LfoSim.h"
#include "ParamControls.h"

// The LFO SHAPE list, grouped (UI review 6, I6-23): Basic, Drawn, Random,
// Chaos and Physics, with the shapes newer ones replaced (the "(classic)"
// pair of each physics shape, the first S&H, Smooth Random and Drunk) in a
// Legacy submenu that only shows while the patch uses one. Display only: the
// saved index of every shape stays as it was, so the combo keeps its items
// in parameter order and only the popup is arranged.
namespace LfoShapeMenu
{
// Shapes a newer one does better; patches keep playing them.
inline bool isLegacy (int shape)
{
    return shape == LfoShapes::SampleHold || shape == LfoShapes::SmoothRandom || shape == LfoShapes::Drunk
           || shape == LfoShapes::Chaos || LfoShapes::isPhysics (shape);
}

// The name shown for a shape (the parameter's own text, with its accents
// and the legacy ones marked). The drawn shapes are one system (UI review
// 7, S7-7 / V7-19): Draw, Steps and MSEG, the points-and-curves shape (the
// parameter's "Curve") that any LFO can be. S&H is the current one; the
// first is "S&H (stepped)", not "(classic)", which read as deprecated
// (I7-44).
inline juce::String displayName (int shape, const juce::String& parameterText)
{
    if (shape == LfoSimShapes::Rossler)
        return juce::String (juce::CharPointer_UTF8 ("R\xc3\xb6ssler"));
    if (shape == LfoSimShapes::Henon)
        return juce::String (juce::CharPointer_UTF8 ("H\xc3\xa9non Map"));
    if (shape == LfoShapes::Curve)
        return "MSEG";
    if (shape == LfoSimShapes::RandomHold)
        return "S&H";
    if (shape == LfoShapes::SampleHold)
        return "S&H (stepped)";
    // Every older take whose name a newer shape also has is "(classic)", so
    // the menu never lists two "Bounce"s (UI review 8, I8-36).
    if (shape == LfoShapes::SmoothRandom || shape == LfoShapes::Drunk || shape == LfoShapes::Chaos || LfoShapes::isPhysics (shape))
        return parameterText.upToFirstOccurrenceOf (" (", false, false) + " (classic)";
    return parameterText;
}

struct Group
{
    const char* heading;
    std::vector<int> shapes;
};

inline const std::vector<Group>& groups()
{
    using namespace LfoShapes;
    namespace S = LfoSimShapes;
    static const std::vector<Group> list {
        { "Basic", { Sine, Triangle, SawUp, SawDown, Square } },
        { "Drawn", { Draw, Steps, Curve } },
        { "Random", { S::RandomHold, S::SineRandom, S::Perlin, S::DrunkWalk } },
        { "Chaos", { S::Lorenz, S::Rossler, S::Duffing, S::Logistic, S::Henon, S::DoublePendulum } },
        { "Physics", { S::Bounce, S::Pendulum, S::Spring, S::Friction } },
        { "Legacy", { SampleHold, SmoothRandom, Drunk, Chaos, Bounce, Pendulum, Spring, Friction } },
    };
    return list;
}

// Whether any LFO of the patch plays a legacy shape.
inline bool patchUsesLegacy (const IlanaSynthAudioProcessor& processor)
{
    for (int lfo = 1; lfo <= IlanaSynthAudioProcessor::numLfos; ++lfo)
        if (const auto* value = processor.apvts.getRawParameterValue ("lfo" + juce::String (lfo) + "_shape"))
            if (isLegacy ((int) value->load()))
                return true;

    return false;
}

// Shows the grouped menu for an LFO's SHAPE box; `picked` runs with the
// shape picked (after the box, and so the parameter, has it).
inline void show (juce::ComboBox& box, const IlanaSynthAudioProcessor& processor, std::function<void (int)> picked)
{
    const auto current = box.getSelectedId() - 1;
    juce::PopupMenu menu;

    for (const auto& group : groups())
    {
        const auto legacy = juce::String (group.heading) == "Legacy";
        if (legacy && ! patchUsesLegacy (processor))
            continue;

        juce::PopupMenu items;
        for (const auto shape : group.shapes)
            if (shape < box.getNumItems())
                items.addItem (shape + 1, box.getItemText (shape), true, shape == current);

        if (legacy)
        {
            menu.addSeparator();
            menu.addSubMenu ("Legacy (older takes on these shapes)", items, true, nullptr, isLegacy (current));
        }
        else
        {
            menu.addSectionHeader (group.heading);
            for (juce::PopupMenu::MenuItemIterator it (items); it.next();)
                menu.addItem (it.getItem());
        }
    }

    juce::Component::SafePointer<juce::ComboBox> safeBox (&box);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&box).withMinimumWidth (box.getWidth())
                            .withItemThatMustBeVisible (box.getSelectedId()),
                        [safeBox, picked = std::move (picked)] (int id)
                        {
                            if (safeBox == nullptr || id <= 0)
                                return;
                            safeBox->setSelectedId (id, juce::sendNotificationSync);
                            if (picked != nullptr)
                                picked (id - 1);
                        });
}

// The Operator Env's LFO (the DX7's) in the pool LFOs' words and order (UI
// review 7, I7-18): Sine, Triangle, Saw Up, Saw Down, Square, then S&H. Its
// parameter keeps the DX7's order (Triangle, Saw Down, Saw Up, Square, Sine,
// S&H); only the names and the popup's order change.
inline const std::array<int, 6>& operatorLfoOrder()
{
    static const std::array<int, 6> order { 4, 0, 2, 1, 3, 5 };
    return order;
}

inline void applyOperatorLfo (ComboControl& control)
{
    auto& box = control.getComboBox();
    static const char* const names[] { "Triangle", "Saw Down", "Saw Up", "Square", "Sine", "S&H" };
    for (int i = 0; i < box.getNumItems() && i < 6; ++i)
        box.changeItemText (box.getItemId (i), names[i]);

    control.setPopupOverride ([&box]
    {
        juce::PopupMenu menu;
        const auto current = box.getSelectedId() - 1;
        menu.addSectionHeader ("Basic");
        for (const auto wave : operatorLfoOrder())
        {
            if (wave == 5)
                menu.addSectionHeader ("Random");
            if (wave < box.getNumItems())
                menu.addItem (wave + 1, box.getItemText (wave), true, wave == current);
        }
        juce::Component::SafePointer<juce::ComboBox> safeBox (&box);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&box).withMinimumWidth (box.getWidth()),
                            [safeBox] (int id)
                            {
                                if (safeBox != nullptr && id > 0)
                                    safeBox->setSelectedId (id, juce::sendNotificationSync);
                            });
    });
}

// What an LFO panel's header says about how it runs (UI review 8, I8-16):
// a simulated shape's TRIGGER decides when a shared one restarts, so its
// caption is built from TRIGGER, not from RETRIG alone; a shape with a
// second output says so (S8-16 / V8-28). `lfo` is 0-based.
inline juce::String runCaption (const IlanaSynthAudioProcessor& processor, int lfo)
{
    const auto prefix = "lfo" + juce::String (lfo + 1);
    const auto read = [&processor, &prefix] (const char* suffix)
    {
        const auto* value = processor.apvts.getRawParameterValue (prefix + suffix);
        return value != nullptr ? value->load() : 0.0f;
    };
    const auto shape = juce::roundToInt (read ("_shape"));
    const auto perVoice = read ("_retrig") > 0.5f;
    juce::String text;

    if (read ("_key") > 0.5f)
        text = "per voice, rate follows the note (4 Hz = its pitch)";
    else if (LfoSimShapes::isSim (shape))
    {
        const char* const shared[] { "shared by all voices, restarts on each note", "shared by all voices, runs free",
                                     "shared by all voices, restarts on the beat", "shared, restarts on each EUCLID / PROB SEQ step" };
        const char* const voiced[] { "per voice, restarts on each note", "per voice, starts on its note, then runs free",
                                     "per voice, restarts on its note and on the beat", "per voice, restarts on its note and each SEQ step" };
        text = (perVoice ? voiced : shared)[juce::jlimit (0, 3, juce::roundToInt (read ("_trigger")))];
    }
    else
        text = perVoice ? "runs per voice, restarts on each note" : "free-running, shared by all voices";

    if (LfoSimShapes::isSim (shape))
        text << "; 2 outputs (A, B)";
    return text;
}

// Gives an LFO's SHAPE control the display names and the grouped popup.
inline void apply (ComboControl& control, const IlanaSynthAudioProcessor& processor, std::function<void (int)> picked = {})
{
    auto& box = control.getComboBox();
    for (int i = 0; i < box.getNumItems(); ++i)
        box.changeItemText (box.getItemId (i), displayName (box.getItemId (i) - 1, box.getItemText (i)));

    control.setPopupOverride ([&box, &processor, picked] { show (box, processor, picked); });
}
} // namespace LfoShapeMenu
