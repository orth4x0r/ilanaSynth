#pragma once

#include "../PluginProcessor.h"

// The dialogs of portable sample presets (roadmap step 31): the missing-file
// prompt (locate one file, or search a folder for all of them), embedding a
// sample in the patch and collecting samples into a folder. The audio work
// is in IlanaSynthAudioProcessor (src/processor/SamplePortability.cpp).
namespace SamplePortability
{
using Owner = juce::Component::SafePointer<juce::Component>;

inline std::unique_ptr<juce::FileChooser>& chooser()
{
    static std::unique_ptr<juce::FileChooser> held;
    return held;
}

// A background job with a progress window and a cancel button.
class Job : public juce::ThreadWithProgressWindow
{
public:
    Job (const juce::String& title, std::function<void (Job&)> workIn, std::function<void (bool cancelled)> doneIn)
        : juce::ThreadWithProgressWindow (title, true, true), work (std::move (workIn)), done (std::move (doneIn)) {}

    void run() override { work (*this); }
    void threadComplete (bool cancelled) override
    {
        auto finish = std::move (done);
        delete this; // (the window's timer is finished with us)
        if (finish != nullptr)
            finish (cancelled);
    }

private:
    std::function<void (Job&)> work;
    std::function<void (bool)> done;
};

inline void tell (const juce::String& title, const juce::String& text, juce::MessageBoxIconType icon = juce::MessageBoxIconType::InfoIcon)
{
    juce::AlertWindow::showMessageBoxAsync (icon, title, text);
}

inline juce::String missingText (const std::vector<IlanaSynthAudioProcessor::MissingSample>& missing)
{
    juce::String text = "These sample files are not where the patch expects them, so those oscillators are silent:\n";
    auto shown = 0;
    for (const auto& entry : missing)
    {
        if (++shown > 6)
        {
            text << "\n...and " << juce::String ((int) missing.size() - 6) << " more";
            break;
        }
        const juce::File file (entry.path);
        text << "\nOSC " << juce::String (entry.osc + 1) << ":  " << file.getFileName();
        if (! entry.partial.isEmpty())
            text << "  (missing " << entry.partial.joinIntoString (", ", 0, 3) << (entry.partial.size() > 3 ? ", ..." : "") << ")";
    }
    return text;
}

inline void promptMissing (IlanaSynthAudioProcessor& processor, Owner owner);

// Searches `folder` (in the background), then relinks what it found.
inline void searchAndRelink (IlanaSynthAudioProcessor& processor, Owner owner, const juce::File& folder, bool recursive)
{
    auto found = std::make_shared<std::vector<std::pair<int, juce::File>>>();
    auto* job = new Job ("Searching " + folder.getFileName(),
                         [&processor, folder, recursive, found] (Job& self)
                         {
                             self.setStatusMessage ("Looking for the missing samples...");
                             self.setProgress (-1.0);
                             *found = processor.findMissingSampleFiles (folder, recursive, [&self] { return self.threadShouldExit(); });
                         },
                         [&processor, owner, folder, found] (bool cancelled)
                         {
                             if (cancelled)
                                 return;
                             auto relinked = 0;
                             for (const auto& [osc, file] : *found)
                                 if (processor.relinkSample (osc, file))
                                     ++relinked;
                             if (relinked == 0)
                                 tell ("Search", "No file with a missing sample's name was found in " + folder.getFullPathName() + ".");
                             if (owner != nullptr)
                                 promptMissing (processor, owner);
                         });
    job->launchThread();
}

inline void promptMissing (IlanaSynthAudioProcessor& processor, Owner owner)
{
    const auto missing = processor.getMissingSamples();
    if (missing.empty() || owner == nullptr)
        return;

    auto* alert = new juce::AlertWindow ("Missing samples", missingText (missing), juce::MessageBoxIconType::WarningIcon, owner.getComponent());
    alert->addButton ("Locate...", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Search Folder...", 2);
    alert->addButton ("Ignore", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    const auto first = missing.front();
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([&processor, owner, first] (int choice)
    {
        if (owner == nullptr || choice == 0)
            return;

        const auto start = juce::File (first.path).getParentDirectory().isDirectory() ? juce::File (first.path).getParentDirectory()
                                                                                       : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        if (choice == 1)
        {
            chooser() = std::make_unique<juce::FileChooser> ("Locate " + juce::File (first.path).getFileName(), start,
                                                             "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a;*.sf2;*.sfz");
            chooser()->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                    [&processor, owner, first] (const juce::FileChooser& picked)
                                    {
                                        const auto file = picked.getResult();
                                        if (owner == nullptr || ! file.existsAsFile())
                                            return;
                                        if (! processor.relinkSample (first.osc, file))
                                            tell ("Locate", "That file could not be read as a sample.", juce::MessageBoxIconType::WarningIcon);
                                        // Its neighbours often moved with it.
                                        for (const auto& [osc, sibling] : processor.findMissingSampleFiles (file.getParentDirectory(), false))
                                            processor.relinkSample (osc, sibling);
                                        promptMissing (processor, owner);
                                    });
        }
        else
        {
            chooser() = std::make_unique<juce::FileChooser> ("Search this folder (and the folders in it)", start);
            chooser()->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [&processor, owner] (const juce::FileChooser& picked)
                                    {
                                        if (owner == nullptr || ! picked.getResult().isDirectory())
                                            return;
                                        searchAndRelink (processor, owner, picked.getResult(), true);
                                    });
        }
    }), true);
}

inline void embed (IlanaSynthAudioProcessor& processor, int oscIndex, bool all)
{
    juce::String error;
    const auto count = all ? processor.embedAllSamples (error) : (processor.embedSample (oscIndex, error) ? 1 : 0);
    if (error.isNotEmpty())
        tell ("Embed samples", error, juce::MessageBoxIconType::WarningIcon);
    else if (count == 0)
        tell ("Embed samples", "There is no sample file to store in the patch.");
}

inline void collect (IlanaSynthAudioProcessor& processor, Owner owner)
{
    chooser() = std::make_unique<juce::FileChooser> ("Collect samples into this folder",
                                                     juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
    chooser()->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                            [&processor, owner] (const juce::FileChooser& picked)
                            {
                                const auto folder = picked.getResult();
                                if (owner == nullptr || folder == juce::File())
                                    return;
                                auto result = std::make_shared<IlanaSynthAudioProcessor::CollectResult>();
                                auto* job = new Job ("Collecting samples",
                                                     [&processor, folder, result] (Job& self)
                                                     {
                                                         self.setStatusMessage ("Copying into " + folder.getFileName() + "...");
                                                         *result = processor.collectSamples (folder, [&self] (float done)
                                                                                             {
                                                                                                 self.setProgress ((double) done);
                                                                                                 return ! self.threadShouldExit();
                                                                                             });
                                                     },
                                                     [result, folder] (bool)
                                                     {
                                                         if (result->error.isNotEmpty())
                                                         {
                                                             tell ("Collect samples", result->error, juce::MessageBoxIconType::WarningIcon);
                                                             return;
                                                         }
                                                         auto text = result->cancelled ? juce::String ("Stopped. ") : juce::String();
                                                         text << juce::String (result->files) << " file(s) are in " << folder.getFullPathName()
                                                              << ", and the patch now plays them from there. Save the preset to keep that.";
                                                         if (! result->missing.isEmpty())
                                                             text << "\n\nNot found, so not copied:\n" << result->missing.joinIntoString ("\n", 0, 5);
                                                         tell ("Collect samples", text);
                                                     });
                                job->launchThread();
                            });
}
} // namespace SamplePortability
