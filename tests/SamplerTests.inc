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

// Step 44: the sample out as a WAV, whole or the played range.
void runSampleExportTest()
{
    using namespace SamplerLab;
    const auto folder = scratchFolder ("ilana-sampler-export");
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::String error;
    check (! processor.exportSampleWav (0, folder.getChildFile ("none.wav"), false, error) && error.isNotEmpty(),
           "nothing loaded: export says there is no sample");

    // A ramp (the value is the position) shows which frames were written.
    juce::AudioBuffer<float> ramp (2, 10000);
    for (int i = 0; i < 10000; ++i)
    {
        ramp.setSample (0, i, (float) i / 10000.0f);
        ramp.setSample (1, i, -(float) i / 10000.0f);
    }
    auto data = std::make_shared<SampleData>();
    data->buffer = ramp;
    data->sampleRate = 22050.0;
    data->name = "ramp";
    processor.setUserSample (0, data, {});

    const auto read = [] (const juce::File& file, double& rate)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        juce::AudioBuffer<float> out;
        if (reader != nullptr)
        {
            rate = reader->sampleRate;
            out.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&out, 0, out.getNumSamples(), 0, true, true);
        }
        return out;
    };

    const auto whole = folder.getChildFile ("whole.wav");
    check (processor.exportSampleWav (0, whole, false, error), "the whole sample exports (" + error + ")");
    double rate = 0.0;
    auto back = read (whole, rate);
    check (back.getNumSamples() == 10000 && back.getNumChannels() == 2 && rate == 22050.0, "whole: every frame, both channels, its own rate");
    check (std::abs (back.getSample (0, 5000) - 0.5f) < 1.0e-4f && std::abs (back.getSample (1, 5000) + 0.5f) < 1.0e-4f, "whole: the audio is unchanged (24-bit)");

    setParam (processor, "osc1_sample_start", 0.2f);
    setParam (processor, "osc1_sample_end", 0.6f);
    setParam (processor, "osc1_sample_reverse", 1.0f);
    const auto range = folder.getChildFile ("range.wav");
    check (processor.exportSampleWav (0, range, true, error), "the played range exports");
    back = read (range, rate);
    check (back.getNumSamples() == 4000, "range: START to END only (" + juce::String (back.getNumSamples()) + " frames)");
    check (back.getNumSamples() == 4000 && back.getSample (0, 0) > 0.59f && back.getSample (0, 3999) < 0.201f,
           "range: REVERSE is applied, as the card plays it");

    // Overwriting keeps a valid file.
    check (processor.exportSampleWav (0, range, false, error) && read (range, rate).getNumSamples() == 10000, "exporting over a file replaces it");
    folder.deleteRecursively();
}

void runSamplerTests()
{
    runSampleImportFeedbackTest();
    runSampleExportTest();
}
