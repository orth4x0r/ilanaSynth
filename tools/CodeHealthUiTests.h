// Code health before RC 1.0 (2026-11-26, ilana: "do this before rc 1.0 ...
// check that it doesnt introduce new bugs"):
//  1. A plain select's own list (a ComboBox without a menu of its own) opens
//     inside the editor, as every other menu does since 2026-11-24.
//  2. A sample dropped or chosen on an oscillator is decoded in the background:
//     the well says LOADING, the sample lands a moment later, a patch or a
//     preset loaded meanwhile wins, and a bad file still says why.
// Included by Snapshot.cpp after its helpers; group CODE_HEALTH.
#pragma once

namespace CodeHealthUi
{
// A short sine as a WAV file.
inline bool writeTestWav (const juce::File& file, double seconds = 0.5)
{
    file.deleteFile();
    juce::AudioBuffer<float> buffer (1, juce::roundToInt (44100.0 * seconds));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * i / 44100.0));
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
        return false;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), 44100.0, 1, 16, {}, 0));
    if (writer == nullptr)
        return false;
    stream.release(); // (the writer owns it now)
    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

} // namespace CodeHealthUi

void runCodeHealthTests (IlanaSynthAudioProcessor& processor, IlanaSynthAudioProcessorEditor& editor)
{
    using namespace CodeHealthUi;
    // ILANA_UITEST_SHOTS=<dir>: a picture of the LOADING well.
    const auto shotsDir = juce::SystemStats::getEnvironmentVariable ("ILANA_UITEST_SHOTS", "");
    const auto shot = [&] (const char* name)
    {
        if (shotsDir.isNotEmpty())
            save (editor, juce::File (shotsDir).getChildFile (name));
    };
    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    editor.showPage ("MAIN");
    settle (350);

    // 1. A plain select's list opens inside the editor.
    {
        std::vector<ComboControl*> combos;
        findAll<ComboControl> (editor, combos);
        juce::ComboBox* plain = nullptr;
        for (auto* control : combos)
        {
            auto& box = control->getComboBox();
            auto* own = dynamic_cast<OwnMenuBox*> (&box);
            if (visibleInTree (&box) && box.isEnabled() && box.getNumItems() > 1 && (own == nullptr || ! own->opensOwnMenu()))
            {
                plain = &box;
                break;
            }
        }
        expect (plain != nullptr, "MAIN has a plain select (a list of its own choices)");

        if (plain != nullptr)
        {
            // (The editor is never on screen here, and a menu doesn't open over
            // a target that isn't showing: the options the select's list is
            // opened with are checked instead. They are what every other menu
            // uses: IlanaTheme::menuOptions, parented to the editor's content.)
            juce::Label label;
            label.setSize (plain->getWidth(), plain->getHeight());
            const auto options = plain->getLookAndFeel().getOptionsForComboBoxPopupMenu (*plain, label);
            auto* parent = options.getParentComponent();
            const auto expected = IlanaTheme::menuOptions (*plain).getParentComponent();
            expect (parent != nullptr && parent == expected && parent->getProperties().contains ("ilanaCalloutHost"),
                    "a plain select's list opens inside the editor (" + plain->getText() + ")");
            expect (options.getTargetComponent() == plain && options.getMinimumWidth() == plain->getWidth(),
                    "... still anchored to the select, at least as wide as it");
        }
    }

    // 2. A sample loads in the background.
    std::vector<WaveDisplay*> displays;
    findAll<WaveDisplay> (editor, displays);
    WaveDisplay* well = nullptr;
    for (auto* display : displays)
        if (display->getOscIndex() == 0 && visibleInTree (display))
            well = display;
    expect (well != nullptr, "OSC 1's picture is on MAIN");
    if (well == nullptr)
        return;

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-code-health-" + juce::String (juce::Time::currentTimeMillis()));
    folder.createDirectory();
    const auto first = folder.getChildFile ("first.wav");
    const auto second = folder.getChildFile ("second.wav");
    expect (writeTestWav (first) && writeTestWav (second), "test samples written");

    const auto waitLoaded = [&] (int limitMs)
    {
        for (int waited = 0; waited < limitMs && well->getLoadingName().isNotEmpty(); waited += 50)
            settle (50);
    };

    well->loadSampleFile (first);
    expect (well->getLoadingName() == "first.wav", "the well says which file it is loading (" + well->getLoadingName() + ")");
    shot ("code-health-loading.png");
    waitLoaded (5000);
    expect (well->getLoadingName().isEmpty(), "the load finishes");
    expect (processor.getSamplePath (0) == first.getFullPathName(), "the dropped sample is OSC 1's (" + processor.getSamplePath (0) + ")");
    const auto mode = (int) processor.apvts.getRawParameterValue ("osc1_mode")->load();
    expect (mode == 2 || mode == 3, "OSC 1 plays the sample (mode " + juce::String (mode) + ")");

    // A preset loaded while the file decodes wins: the file is dropped.
    well->loadSampleFile (second);
    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    waitLoaded (5000);
    settle (200);
    expect (well->getLoadingName().isEmpty(), "a replaced load says LOADING no longer");
    expect (processor.getSamplePath (0).isEmpty(), "a preset loaded meanwhile wins over the decoding file (Init has no sample; OSC 1 has "
                                                       + processor.getSamplePath (0) + ")");

    // Two drops in a row: the second one is the oscillator's, and the well
    // says LOADING until it is in (the same file twice too: the first
    // one's "replaced" answer doesn't take LOADING down).
    well->loadSampleFile (first);
    well->loadSampleFile (second);
    waitLoaded (5000);
    well->loadSampleFile (second);
    well->loadSampleFile (second);
    settle (1);
    expect (well->getLoadingName() == "second.wav", "a second drop of the same file keeps LOADING up");
    waitLoaded (5000);
    settle (200);
    expect (processor.getSamplePath (0) == second.getFullPathName(), "the later of two drops wins (" + processor.getSamplePath (0) + ")");

    // A file that can't be read says why and leaves the sample as it was.
    const auto broken = folder.getChildFile ("broken.wav");
    broken.replaceWithText ("not audio");
    well->loadSampleFile (broken);
    waitLoaded (5000);
    expect (processor.getLastSampleError().contains ("broken.wav"), "a broken file says why (" + processor.getLastSampleError() + ")");
    expect (processor.getSamplePath (0) == second.getFullPathName(), "a broken file leaves the sample as it was");
    {
        std::vector<ConfirmOverlay*> overlays;
        findAll<ConfirmOverlay> (editor, overlays);
        auto told = false;
        for (auto* overlay : overlays)
            if (overlay->isVisible())
            {
                told = true;
                overlay->finish (false);
            }
        expect (told, "the reason shows in the window");
        settle (100);
    }

    folder.deleteRecursively();
    processor.loadFactoryPreset (juce::jmax (0, processor.getFactoryPresetNames().indexOf ("Init")));
    settle (200);
}
