// The sampler roadmap steps done as one (32-39, 42-44): import messages, WAV
// export, playback modes, ping-pong, random start, playback quality, the zone
// map, round robin and velocity blending, SoundFont presets, slices and the
// sample browser. Included by TableTest.cpp after MultiSampleTests.inc (it
// reuses MultiSampleLab's helpers); ILANA_SAMPLER_TEST=1 runs only these.

namespace SamplerLab
{
inline juce::File scratchFolder (const char* name)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
    folder.deleteRecursively();
    folder.createDirectory();
    return folder;
}

inline void setParam (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
} // namespace SamplerLab

// Step 43: a file that can't be read says why; a long file says it was cut.
void runSampleImportFeedbackTest()
{
    using namespace SamplerLab;
    const auto folder = scratchFolder ("ilana-sampler-import");
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto missing = folder.getChildFile ("nothing.wav");
    check (! processor.loadUserSample (0, missing, true) && processor.getLastSampleError().contains ("not there"),
           "a missing file says it is not there (" + processor.getLastSampleError() + ")");

    const auto empty = folder.getChildFile ("empty.wav");
    empty.create();
    check (! processor.loadUserSample (0, empty, true) && processor.getLastSampleError().contains ("empty"),
           "an empty file says it is empty (" + processor.getLastSampleError() + ")");

    const auto text = folder.getChildFile ("notes.txt");
    text.replaceWithText ("not audio");
    check (! processor.loadUserSample (0, text, true) && processor.getLastSampleError().contains (".txt"),
           "an unknown kind of file names its extension (" + processor.getLastSampleError() + ")");

    const auto broken = folder.getChildFile ("broken.wav");
    broken.replaceWithText ("RIFF....WAVEjunkjunkjunkjunk");
    check (! processor.loadUserSample (0, broken, true) && processor.getLastSampleError().contains ("decoded"),
           "a damaged WAV says it could not be decoded (" + processor.getLastSampleError() + ")");

    // 121 s at a low rate (small file): cut at 120 s, and the sample says so.
    const auto longFile = folder.getChildFile ("long.wav");
    MultiSampleLab::writeWav (longFile, MultiSampleLab::sine (100.0, 1000.0, 121000), 1000.0);
    check (processor.loadUserSample (0, longFile, true) && processor.getLastSampleError().isEmpty(), "a long file still loads");
    const auto* loaded = processor.getSampleForOsc (0);
    check (loaded != nullptr && loaded->getNumSamples() == 120000 && loaded->loadWarning.contains ("2:01"),
           "a file over 120 s is cut and says how long it was (" + (loaded != nullptr ? loaded->loadWarning : juce::String()) + ")");

    const auto shortFile = folder.getChildFile ("short.wav");
    MultiSampleLab::writeWav (shortFile, MultiSampleLab::sine (440.0, 44100.0, 4410), 44100.0);
    check (processor.loadUserSample (0, shortFile, true) && processor.getSampleForOsc (0)->loadWarning.isEmpty(),
           "a file that came in whole has no warning");

    // An SFZ with one region whose file is missing loads the rest and says so.
    MultiSampleLab::writeWav (folder.getChildFile ("a.wav"), MultiSampleLab::sine (220.0, 44100.0, 4410), 44100.0);
    const auto sfz = folder.getChildFile ("partial.sfz");
    sfz.replaceWithText ("<region> sample=a.wav lokey=0 hikey=60\n<region> sample=gone.wav lokey=61 hikey=127\n");
    check (processor.loadUserSample (0, sfz, true), "an SFZ with a missing file still loads");
    const auto* multi = processor.getSampleForOsc (0);
    check (multi != nullptr && multi->zones.size() == 1 && multi->loadWarning.contains ("gone.wav"),
           "the SFZ names the file it could not read (" + (multi != nullptr ? multi->loadWarning : juce::String()) + ")");
    folder.deleteRecursively();
}

void runSamplerTests()
{
    runSampleImportFeedbackTest();
}
