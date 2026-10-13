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

inline void samplerSet (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// A ramp: the value is the position, so the output shows where the player reads.
inline std::shared_ptr<SampleData> ramp (int length = 10000)
{
    auto data = std::make_shared<SampleData>();
    data->sampleRate = 48000.0;
    data->buffer.setSize (1, length);
    for (int i = 0; i < length; ++i)
        data->buffer.setSample (0, i, (float) i / (float) length);
    return data;
}

struct PlayerRun
{
    std::vector<float> out;
    bool active = false;
};

// Plays a SamplePlayer for count samples; releaseAt (if >= 0) lets go of the key there.
inline PlayerRun runPlayer (const SampleData& sample, const std::function<void (SamplePlayer::Params&)>& configure, int count,
                            int releaseAt = -1, double rate = 1.0)
{
    SamplePlayer player;
    player.prepare (48000.0);
    SamplePlayer::Params params;
    params.sample = &sample;
    configure (params);
    player.setParams (params);
    player.setPlaybackRatio (rate);
    player.trigger();
    PlayerRun result;
    for (int i = 0; i < count; ++i)
    {
        if (i == releaseAt)
            player.release();
        float l = 0.0f, r = 0.0f;
        player.process (l, r);
        result.out.push_back (l);
    }
    result.active = player.isActive();
    return result;
}

inline float lowestFrom (const std::vector<float>& x, size_t from) { return *std::min_element (x.begin() + (long) from, x.end()); }
inline float highestFrom (const std::vector<float>& x, size_t from) { return *std::max_element (x.begin() + (long) from, x.end()); }

// Renders the processor with one note held for heldBlocks, then released; the RMS of each block.
inline std::vector<float> renderNote (IlanaSynthAudioProcessor& processor, int heldBlocks, int totalBlocks, int note = 60)
{
    juce::AudioBuffer<float> buffer (2, 512);
    std::vector<float> levels;
    for (int block = 0; block < totalBlocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        if (block == heldBlocks)
            midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        processor.processBlock (buffer, midi);
        levels.push_back (buffer.getRMSLevel (0, 0, 512));
    }
    return levels;
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

    samplerSet (processor, "osc1_sample_start", 0.2f);
    samplerSet (processor, "osc1_sample_end", 0.6f);
    samplerSet (processor, "osc1_sample_reverse", 1.0f);
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

// Step 33: Gate, One Shot and Sustain Loop.
void runSamplePlaybackModeTest()
{
    using namespace SamplerLab;
    const auto sample = ramp();

    // Gate is the old behaviour, bit for bit, whatever the release does.
    const auto before = runPlayer (*sample, [] (SamplePlayer::Params& p) { p.loop = true; }, 30000);
    const auto gate = runPlayer (*sample, [] (SamplePlayer::Params& p) { p.loop = true; p.playback = SamplePlayer::gate; }, 30000, 5000);
    check (gate.out == before.out && gate.active, "Gate loops on through the release, as before");

    // One Shot ignores LOOP and ends at END.
    const auto shot = runPlayer (*sample, [] (SamplePlayer::Params& p) { p.loop = true; p.playback = SamplePlayer::oneShot; }, 12000);
    check (! shot.active && shot.out[9990] > 0.99f && shot.out[11000] == 0.0f, "One Shot plays to END once, LOOP ignored");

    // Sustain Loop loops while held (LOOP off), then plays out of the loop to END.
    const auto sustain = runPlayer (*sample, [] (SamplePlayer::Params& p)
                                    { p.loop = false; p.ownLoop = true; p.loopFrom = 0.4f; p.loopTo = 0.6f; p.playback = SamplePlayer::sustainLoop; },
                                    40000, 20000);
    check (lowestFrom (std::vector<float> (sustain.out.begin(), sustain.out.begin() + 20000), 4000) >= 0.4f - 1.0e-4f
               && highestFrom (std::vector<float> (sustain.out.begin(), sustain.out.begin() + 20000), 4000) < 0.6f + 1.0e-4f,
           "Sustain Loop repeats the loop while the key is down, even with LOOP off");
    check (highestFrom (sustain.out, 20000) > 0.99f && ! sustain.active, "after the release it leaves the loop and plays on to END");

    // A region's own loop: a One Shot skips it, a Sustain Loop leaves it on release.
    const auto zoneShot = runPlayer (*sample, [] (SamplePlayer::Params& p)
                                     { p.loopStart = 2000; p.loopEnd = 4000; p.playback = SamplePlayer::oneShot; }, 12000);
    check (! zoneShot.active && highestFrom (zoneShot.out, 0) > 0.99f, "One Shot plays through a region's loop");
    const auto zoneSustain = runPlayer (*sample, [] (SamplePlayer::Params& p)
                                        { p.loopStart = 2000; p.loopEnd = 4000; p.playback = SamplePlayer::sustainLoop; }, 30000, 15000);
    check (highestFrom (std::vector<float> (zoneSustain.out.begin(), zoneSustain.out.begin() + 15000), 0) < 0.401f
               && highestFrom (zoneSustain.out, 15000) > 0.99f,
           "Sustain Loop holds a region's loop until the release");

    // In the synth: a One Shot outlives a short key press and a short release.
    IlanaSynthAudioProcessor processor;
    MultiSampleLab::setUp (processor);
    auto tone = std::make_shared<SampleData>();
    tone->sampleRate = 48000.0;
    tone->buffer = MultiSampleLab::sine (261.6255653005986, 48000.0, 48000); // one second
    processor.setUserSample (0, tone, {});
    samplerSet (processor, "amp_release", 0.01f);
    samplerSet (processor, "amp_attack", 0.0f);
    const auto gated = renderNote (processor, 4, 120);
    check (gated[30] < 1.0e-4f, "Gate: the note stops soon after the key");
    samplerSet (processor, "osc1_sample_playback", 1.0f);
    const auto held = renderNote (processor, 4, 120);
    check (held[30] > 0.01f && held[80] > 0.01f, "One Shot: the sample keeps playing after the key is let go");
    check (held[110] < 1.0e-4f, "One Shot: the voice ends once the sample has played (" + juce::String (held[110], 6) + ")");
}

void runSamplerTests()
{
    runSampleImportFeedbackTest();
    runSampleExportTest();
    runSamplePlaybackModeTest();
}
