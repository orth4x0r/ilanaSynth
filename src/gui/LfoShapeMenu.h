#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
// and the legacy ones marked).
inline juce::String displayName (int shape, const juce::String& parameterText)
{
    if (shape == LfoSimShapes::Rossler)
        return juce::String (juce::CharPointer_UTF8 ("R\xc3\xb6ssler"));
    if (shape == LfoSimShapes::Henon)
        return juce::String (juce::CharPointer_UTF8 ("H\xc3\xa9non Map"));
    if (shape == LfoShapes::SampleHold || shape == LfoShapes::SmoothRandom || shape == LfoShapes::Drunk)
        return parameterText + " (classic)";
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

// Gives an LFO's SHAPE control the display names and the grouped popup.
inline void apply (ComboControl& control, const IlanaSynthAudioProcessor& processor, std::function<void (int)> picked = {})
{
    auto& box = control.getComboBox();
    for (int i = 0; i < box.getNumItems(); ++i)
        box.changeItemText (box.getItemId (i), displayName (box.getItemId (i) - 1, box.getItemText (i)));

    control.setPopupOverride ([&box, &processor, picked] { show (box, processor, picked); });
}
} // namespace LfoShapeMenu
