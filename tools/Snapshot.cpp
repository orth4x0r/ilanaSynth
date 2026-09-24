// Dev tool: renders the plugin editor offscreen and writes one PNG per tab,
// so the UI can be reviewed without a DAW or a display session.
//   ilanaSnapshot <output dir> [factory preset index]

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

#include "PluginProcessor.h"
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
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
    {
        std::cout << "usage: ilanaSnapshot <output dir> [factory preset index]" << std::endl;
        return 1;
    }

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

        // Each FX module's editor, placed in slot 1.
        if (tabs->getTabNames()[i] == "FX")
        {
            for (int type = 1; type <= 28; ++type)
            {
                processor.assignFxSlot (1, type);
                settle (900);
                save (*editor, outDir.getChildFile ("fx-" + juce::String (type).paddedLeft ('0', 2) + ".png"));
            }
        }
    }

    editor.reset();
    return 0;
}
