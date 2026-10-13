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

// Step 34: the loop turns at its ends instead of jumping back.
void runSamplePingPongTest()
{
    using namespace SamplerLab;
    const auto sample = ramp();
    const auto biggestStep = [] (const std::vector<float>& x, size_t from)
    {
        float step = 0.0f;
        for (size_t i = from + 1; i < x.size(); ++i)
            step = std::max (step, std::abs (x[i] - x[i - 1]));
        return step;
    };

    const auto off = runPlayer (*sample, [] (SamplePlayer::Params& p) { p.loop = true; p.ownLoop = true; p.loopFrom = 0.4f; p.loopTo = 0.6f; }, 30000);
    const auto offSet = runPlayer (*sample, [] (SamplePlayer::Params& p)
                                   { p.loop = true; p.ownLoop = true; p.loopFrom = 0.4f; p.loopTo = 0.6f; p.pingPong = false; }, 30000);
    check (off.out == offSet.out, "ping-pong off plays exactly as before");

    const auto pong = runPlayer (*sample, [] (SamplePlayer::Params& p) { p.loop = true; p.ownLoop = true; p.loopFrom = 0.4f; p.loopTo = 0.6f; p.pingPong = true; }, 30000);
    check (lowestFrom (pong.out, 4000) >= 0.4f - 1.0e-4f && highestFrom (pong.out, 4000) < 0.6f + 1.0e-4f, "ping-pong stays inside the loop");
    check (biggestStep (pong.out, 100) < 0.0002f, "ping-pong never jumps: the ramp runs up and down smoothly");
    check (pong.out[6150] < pong.out[6050], "after the loop's end the read runs backward");
    check (pong.out[8150] > pong.out[8050], "after the loop's start it runs forward again");

    const auto whole = runPlayer (*sample, [] (SamplePlayer::Params& p) { p.loop = true; p.start = 0.2f; p.end = 0.5f; p.pingPong = true; }, 20000);
    check (lowestFrom (whole.out, 0) >= 0.2f - 1.0e-4f && highestFrom (whole.out, 0) < 0.5f + 1.0e-4f && biggestStep (whole.out, 1) < 0.0002f,
           "without LOOP PTS the whole START..END range ping-pongs");

    const auto back = runPlayer (*sample, [] (SamplePlayer::Params& p)
                                 { p.loop = true; p.ownLoop = true; p.reverse = true; p.loopFrom = 0.4f; p.loopTo = 0.6f; p.pingPong = true; }, 30000);
    check (lowestFrom (back.out, 6000) >= 0.4f - 1.0e-4f && highestFrom (back.out, 6000) < 0.6f + 1.0e-4f && biggestStep (back.out, 1) < 0.0002f,
           "a reversed ping-pong stays in its loop without jumps");

    const auto fast = runPlayer (*sample, [] (SamplePlayer::Params& p)
                                 { p.loop = true; p.ownLoop = true; p.loopFrom = 0.4f; p.loopTo = 0.6f; p.pingPong = true; }, 30000, -1, 9.7);
    check (lowestFrom (fast.out, 1000) >= 0.4f - 1.0e-4f && highestFrom (fast.out, 1000) < 0.6f + 1.0e-4f, "a fast read folds back inside the loop");

    const auto zone = runPlayer (*sample, [] (SamplePlayer::Params& p) { p.loopStart = 2000; p.loopEnd = 4000; p.pingPong = true; p.loop = true; }, 20000);
    check (lowestFrom (zone.out, 4100) >= 0.2f - 1.0e-4f && highestFrom (zone.out, 0) < 0.4f + 1.0e-4f, "a region's loop ping-pongs too");

    const auto sustain = runPlayer (*sample, [] (SamplePlayer::Params& p)
                                    { p.ownLoop = true; p.loopFrom = 0.4f; p.loopTo = 0.6f; p.pingPong = true; p.playback = SamplePlayer::sustainLoop; },
                                    40000, 7000);
    check (highestFrom (sustain.out, 7000) > 0.99f && ! sustain.active, "a released Sustain Loop ping-pong plays on out to END");

    const auto stretched = runPlayer (*sample, [] (SamplePlayer::Params& p)
                                      { p.loop = true; p.ownLoop = true; p.loopFrom = 0.4f; p.loopTo = 0.6f; p.pingPong = true; p.stretch = true; }, 20000);
    check (std::all_of (stretched.out.begin(), stretched.out.end(), [] (float v) { return std::isfinite (v) && std::abs (v) < 2.0f; }),
           "STRETCH with ping-pong stays finite");
}

// Step 35: a random start per note, optionally landing on an attack.
void runSampleRandomStartTest()
{
    using namespace SamplerLab;
    const auto sample = ramp();
    const auto none = runPlayer (*sample, [] (SamplePlayer::Params&) {}, 10);
    check (none.out[0] == 0.0f, "no random start: the note starts at START");

    // The start goes up to the amount's share of the region.
    const auto startAt = [&] (float amount, float unit, const std::function<void (SamplePlayer::Params&)>& more = {})
    {
        SamplePlayer player;
        player.prepare (48000.0);
        SamplePlayer::Params params;
        params.sample = sample.get();
        params.startRandom = amount;
        if (more)
            more (params);
        player.setParams (params);
        player.trigger (unit);
        float l = 0.0f, r = 0.0f;
        player.process (l, r);
        return l;
    };
    check (std::abs (startAt (0.5f, 1.0f) - 0.5f) < 0.002f, "amount 0.5 reaches halfway at most");
    check (std::abs (startAt (0.5f, 0.5f) - 0.25f) < 0.002f, "the random value scales the offset");
    check (startAt (0.0f, 0.9f) == 0.0f, "amount 0 ignores the random value");
    check (startAt (1.0f, 1.0f, [] (SamplePlayer::Params& p) { p.loop = true; p.ownLoop = true; p.loopFrom = 0.2f; p.loopTo = 0.4f; }) < 0.4f,
           "a random start stays before the loop's end");
    check (startAt (1.0f, 0.5f, [] (SamplePlayer::Params& p) { p.reverse = true; }) > 0.45f, "a reversed note starts back from END");

    // TRANSIENT SAFE: hits every 4000 frames; a start lands on the hit before it.
    auto hits = std::make_shared<SampleData>();
    hits->sampleRate = 48000.0;
    hits->buffer.setSize (1, 48000);
    hits->buffer.clear();
    for (int hit = 0; hit < 12; ++hit)
        for (int i = 0; i < 3000; ++i)
            hits->buffer.setSample (0, hit * 4000 + i, (float) (std::exp (-i / 400.0) * std::sin (i * 0.3)));
    hits->findOnsets();
    check (hits->onsets.size() >= 10, "the hits' attacks are found (" + juce::String ((int) hits->onsets.size()) + ")");
    SamplePlayer player;
    player.prepare (48000.0);
    SamplePlayer::Params params;
    params.sample = hits.get();
    params.startRandom = 1.0f;
    params.startSafe = true;
    player.setParams (params);
    auto onHits = true;
    juce::Random random (7);
    for (int note = 0; note < 40; ++note)
    {
        player.trigger (random.nextFloat());
        const auto at = (int) player.getSourcePosition();
        onHits = onHits && (at % 4000 < 60 || at % 4000 > 3940);
    }
    check (onHits, "TRANSIENT SAFE starts every note on (or just before) a hit");

    // In the synth: unison voices start at different places, and older patches don't change.
    IlanaSynthAudioProcessor processor;
    MultiSampleLab::setUp (processor);
    processor.setUserSample (0, ramp (48000), {});
    samplerSet (processor, "osc1_unison", 4.0f);
    const auto plain = renderNote (processor, 20, 21);
    samplerSet (processor, "osc1_sample_start_rand", 0.8f);
    const auto spread = renderNote (processor, 20, 21);
    check (std::abs (spread[2] - plain[2]) > 1.0e-4f, "a random start changes what the unison voices play");
}

// Step 39: higher-quality reads; Linear stays the default and unchanged.
void runSampleQualityTest()
{
    using namespace SamplerLab;
    // A bright tone (11.3 kHz at 48 kHz) read an octave and a half up folds back
    // into the audible range unless the read filters it.
    auto bright = std::make_shared<SampleData>();
    bright->sampleRate = 48000.0;
    bright->buffer.setSize (1, 48000);
    for (int i = 0; i < 48000; ++i)
        bright->buffer.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 11300.0 * i / 48000.0));
    const auto level = [&] (int quality, double rate)
    {
        const auto run = runPlayer (*bright, [quality] (SamplePlayer::Params& p) { p.loop = true; p.quality = quality; }, 8000, -1, rate);
        double sum = 0.0;
        for (size_t i = 1000; i < run.out.size(); ++i)
            sum += (double) run.out[i] * run.out[i];
        return std::sqrt (sum / (double) (run.out.size() - 1000));
    };
    const auto linearPlain = runPlayer (*bright, [] (SamplePlayer::Params& p) { p.loop = true; }, 4000, -1, 1.37);
    const auto linearSet = runPlayer (*bright, [] (SamplePlayer::Params& p) { p.loop = true; p.quality = 0; }, 4000, -1, 1.37);
    check (linearPlain.out == linearSet.out, "Linear is the old read, bit for bit");
    for (int quality = 1; quality <= 3; ++quality)
        check (level (quality, 0.73) > 0.3, "quality " + juce::String (quality) + " keeps an in-band tone (" + juce::String (level (quality, 0.73), 3) + ")");
    const auto aliased = level (2, 2.8), filtered = level (3, 2.8);
    check (filtered < 0.05 && aliased > 0.2, "Sinc + Anti-Alias removes the folded tone of a strong transposition ("
                                                 + juce::String (aliased, 3) + " -> " + juce::String (filtered, 3) + ")");
    const auto edges = runPlayer (*bright, [] (SamplePlayer::Params& p) { p.quality = 3; p.reverse = true; }, 30000, -1, 3.9);
    check (std::all_of (edges.out.begin(), edges.out.end(), [] (float v) { return std::isfinite (v) && std::abs (v) < 2.0f; }),
           "the quality reads stay finite at the sample's edges");
}

// Step 36: the zone map: silence outside the zones, edits that survive a
// save and undo, and samples added to a map.
void runSampleZoneMapTest()
{
    using namespace SamplerLab;
    const auto folder = scratchFolder ("ilana-sampler-zones");
    MultiSampleLab::writeWav (folder.getChildFile ("low.wav"), MultiSampleLab::sine (220.0, 44100.0, 44100), 44100.0);
    MultiSampleLab::writeWav (folder.getChildFile ("high.wav"), MultiSampleLab::sine (880.0, 44100.0, 44100), 44100.0);
    MultiSampleLab::writeWav (folder.getChildFile ("extra.wav"), MultiSampleLab::sine (440.0, 44100.0, 44100), 44100.0);
    const auto sfz = folder.getChildFile ("map.sfz");
    sfz.replaceWithText ("<region> sample=low.wav lokey=40 hikey=59 pitch_keycenter=57\n<region> sample=high.wav lokey=60 hikey=80 pitch_keycenter=81\n");

    // The zone choice: nearest outside the map, or nothing.
    juce::String error;
    const auto data = MultiSample::load (sfz, error);
    check (data != nullptr && data->zones.size() == 2 && data->zones[0].origin == 0 && data->zones[1].origin == 1, "zones know their place in the file");
    if (data == nullptr || data->zones.size() != 2)
        return;
    check (data->zoneFor (20, 100) == &data->zones[0] && data->zoneFor (100, 100) == &data->zones[1], "outside the map the nearest zone plays");
    check (data->zoneFor (20, 100, true) == nullptr && data->zoneFor (50, 100, true) == &data->zones[0], "with Silence a key outside plays nothing");

    IlanaSynthAudioProcessor processor;
    MultiSampleLab::setUp (processor);
    check (processor.loadUserSample (0, sfz, true), "the map loads");
    const auto loudness = [&processor] (int note)
    {
        // All notes off and a few blocks to let the last note die.
        juce::AudioBuffer<float> quiet (2, 512);
        for (int block = 0; block < 60; ++block)
        {
            juce::MidiBuffer midi;
            if (block == 0)
                midi.addEvent (juce::MidiMessage::allSoundOff (1), 0);
            processor.processBlock (quiet, midi);
        }
        const auto out = MultiSampleLab::play (processor, note, 20);
        double sum = 0.0;
        for (size_t i = 4096; i < out.size(); ++i)
            sum += (double) out[i] * out[i];
        return std::sqrt (sum / (double) (out.size() - 4096));
    };
    check (loudness (20) > 0.01, "a key below the map plays the nearest zone");
    samplerSet (processor, "osc1_sample_unmapped", 1.0f);
    check (loudness (20) < 1.0e-5, "OUTSIDE ZONES Silence: the key plays nothing");
    check (loudness (50) > 0.01, "a key inside the map still plays");
    samplerSet (processor, "osc1_sample_unmapped", 0.0f);

    // Edits to a file's map: saved as edits, they come back with the patch.
    auto zones = processor.getSampleZones (0);
    check (zones.size() == 2, "the editor reads the zones");
    zones[0].hiKey = 50;
    zones[1].loKey = 51;
    zones[1].rootNote = 76.0;
    check (processor.setSampleZones (0, zones, "Edit zones"), "the edited zones are set");
    check (processor.getSampleZones (0)[0].hiKey == 50 && processor.getSampleZones (0)[1].loKey == 51, "the edit plays at once");

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    IlanaSynthAudioProcessor reloaded;
    reloaded.prepareToPlay (48000.0, 512);
    reloaded.setStateInformation (saved.getData(), (int) saved.getSize());
    reloaded.flushAsyncUpdates();
    const auto back = reloaded.getSampleZones (0);
    check (back.size() == 2 && back[0].hiKey == 50 && back[1].loKey == 51 && std::abs (back[1].rootNote - 76.0) < 1.0e-9,
           "an edited file map comes back with the patch (" + juce::String ((int) back.size()) + " zones)");

    // Undo puts the file's own map back.
    processor.getUndoManager().undo();
    check (processor.getSampleZones (0).size() == 2 && processor.getSampleZones (0)[0].hiKey == 59, "undo puts the map back as the file had it");
    processor.getUndoManager().redo();
    check (processor.getSampleZones (0)[0].hiKey == 50, "redo brings the edit back");

    // Adding a sample: a third zone, saved by its file.
    check (processor.addSampleZone (0, folder.getChildFile ("extra.wav"), 90, 100, 1, 127, 95, error), "a sample is added to the map (" + error + ")");
    check (processor.getSampleZones (0).size() == 3 && processor.getSampleZones (0)[2].loKey == 90, "the added zone covers its keys");
    processor.getStateInformation (saved);
    IlanaSynthAudioProcessor again;
    again.prepareToPlay (48000.0, 512);
    again.setStateInformation (saved.getData(), (int) saved.getSize());
    again.flushAsyncUpdates();
    check (again.getSampleZones (0).size() == 3, "the added zone comes back with the patch");

    // A plain sample turns into a map when a sample is added.
    IlanaSynthAudioProcessor plain;
    MultiSampleLab::setUp (plain);
    check (plain.loadUserSample (0, folder.getChildFile ("low.wav"), true) && plain.getSampleZones (0).empty(), "a plain sample has no zones");
    check (plain.addSampleZone (0, folder.getChildFile ("high.wav"), 72, 127, 1, 127, 81, error), "adding to a plain sample works");
    const auto made = plain.getSampleZones (0);
    check (made.size() == 2 && made[0].loKey == 0 && made[0].hiKey == 127 && made[1].loKey == 72 && made[0].data != nullptr,
           "the plain sample is the first zone, the added file the second");

    // A zone map kept in the patch (no file) keeps its edits in the patch.
    IlanaSynthAudioProcessor embedded;
    embedded.prepareToPlay (48000.0, 512);
    auto multi = std::make_shared<SampleData>();
    multi->sampleRate = 48000.0;
    for (int z = 0; z < 2; ++z)
    {
        SampleZone zone;
        zone.data = std::make_shared<SampleData>();
        zone.data->sampleRate = 48000.0;
        zone.data->buffer = MultiSampleLab::sine (z == 0 ? 220.0 : 440.0, 48000.0, 4800);
        zone.loKey = z == 0 ? 0 : 64;
        zone.hiKey = z == 0 ? 63 : 127;
        multi->zones.push_back (zone);
    }
    MultiSample::finish (*multi);
    embedded.setUserSample (0, multi, {});
    auto inPatch = embedded.getSampleZones (0);
    inPatch[1].loKey = 70;
    check (embedded.setSampleZones (0, inPatch, "Edit zones") && embedded.isSampleEmbedded (0), "a resampled map stays in the patch");
    embedded.getStateInformation (saved);
    IlanaSynthAudioProcessor embeddedBack;
    embeddedBack.prepareToPlay (48000.0, 512);
    embeddedBack.setStateInformation (saved.getData(), (int) saved.getSize());
    check (embeddedBack.getSampleZones (0).size() == 2 && embeddedBack.getSampleZones (0)[1].loKey == 70, "its edit is saved with the patch");
    check (! embedded.setSampleZones (0, {}, "Delete everything"), "a map can't lose its last zone");
    folder.deleteRecursively();
}

void runSamplerTests()
{
    runSampleImportFeedbackTest();
    runSampleExportTest();
    runSamplePlaybackModeTest();
    runSamplePingPongTest();
    runSampleRandomStartTest();
    runSampleQualityTest();
    runSampleZoneMapTest();
}
