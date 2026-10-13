// The sampler roadmap steps done as one (32-39, 42-44): import messages, WAV
// export, playback modes, ping-pong, random start, playback quality, the zone
// map, round robin and velocity blending, SoundFont presets, slices and the
// sample browser. Included by TableTest.cpp after MultiSampleTests.inc (it
// reuses MultiSampleLab's helpers); ILANA_SAMPLER_TEST=1 runs only these.

#include "gui/SampleBrowserModel.h"

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

// The level of one frequency in a signal, as a sine's amplitude (a
// Hann-windowed DFT bin, so a neighbouring tone doesn't leak in).
inline double toneLevel (const std::vector<float>& x, size_t from, double hz, double sampleRate)
{
    const auto w = juce::MathConstants<double>::twoPi * hz / sampleRate;
    const auto n = (double) (x.size() - from);
    double re = 0.0, im = 0.0;
    for (size_t i = from; i < x.size(); ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) (i - from) / n);
        re += window * x[i] * std::cos (w * (double) i);
        im -= window * x[i] * std::sin (w * (double) i);
    }
    return 4.0 * std::sqrt (re * re + im * im) / n;
}

// One note at a velocity after the last one has died; returns the output.
inline std::vector<float> playAt (IlanaSynthAudioProcessor& processor, int note, int velocity, int blocks)
{
    juce::AudioBuffer<float> buffer (2, 512);
    for (int block = 0; block < 60; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::allSoundOff (1), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
    }
    std::vector<float> out;
    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0);
        processor.processBlock (buffer, midi);
        for (int i = 0; i < 512; ++i)
            out.push_back (buffer.getSample (0, i));
    }
    return out;
}

// Step 37: round robin (alternates in turn or at random) and velocity
// layers crossfaded near their edges.
void runSampleRoundRobinTest()
{
    using namespace SamplerLab;
    const auto folder = scratchFolder ("ilana-sampler-rr");
    MultiSampleLab::writeWav (folder.getChildFile ("a.wav"), MultiSampleLab::sine (220.0, 48000.0, 48000), 48000.0);
    MultiSampleLab::writeWav (folder.getChildFile ("b.wav"), MultiSampleLab::sine (330.0, 48000.0, 48000), 48000.0);

    // Alternates: b listed first but second in the sequence.
    const auto rr = folder.getChildFile ("rr.sfz");
    rr.replaceWithText ("<region> sample=b.wav pitch_keycenter=57 seq_position=2\n<region> sample=a.wav pitch_keycenter=57 seq_position=1\n");
    juce::String error;
    const auto data = MultiSample::load (rr, error);
    check (data != nullptr && data->zones.size() == 2 && data->zones[0].sequence == 2 && data->zones[1].sequence == 1,
           "seq_position is read (" + error + ")");
    if (data == nullptr || data->zones.size() != 2)
        return;
    check (data->zoneFor (57, 100, false, 0, 1) == &data->zones[0], "round robin Off plays the first zone, as before");
    check (data->zoneFor (57, 100, false, 1, 0) == &data->zones[1] && data->zoneFor (57, 100, false, 1, 1) == &data->zones[0]
               && data->zoneFor (57, 100, false, 1, 2) == &data->zones[1],
           "the alternates take turns in sequence order");

    IlanaSynthAudioProcessor processor;
    MultiSampleLab::setUp (processor);
    check (processor.loadUserSample (0, rr, true), "the alternates load");
    const auto which = [&processor]
    {
        const auto out = playAt (processor, 57, 100, 12);
        return toneLevel (out, 2048, 220.0, 48000.0) > toneLevel (out, 2048, 330.0, 48000.0) ? 'a' : 'b';
    };
    juce::String heard;
    for (int i = 0; i < 4; ++i)
        heard << juce::String::charToString (which());
    check (heard == "bbbb", "Off: every note plays the first zone (" + heard + ")");
    samplerSet (processor, "osc1_sample_round_robin", 1.0f);
    heard.clear();
    for (int i = 0; i < 4; ++i)
        heard << juce::String::charToString (which());
    check (heard == "abab" || heard == "baba", "Cycle: the notes alternate (" + heard + ")");
    samplerSet (processor, "osc1_sample_round_robin", 2.0f);
    heard.clear();
    for (int i = 0; i < 16; ++i)
        heard << juce::String::charToString (which());
    check (heard.containsChar ('a') && heard.containsChar ('b'), "Random: both alternates come up (" + heard + ")");

    // Velocity layers: a below 64, b from 65.
    const auto layers = folder.getChildFile ("layers.sfz");
    layers.replaceWithText ("<region> sample=a.wav pitch_keycenter=57 lovel=1 hivel=64\n<region> sample=b.wav pitch_keycenter=57 lovel=65 hivel=127\n");
    const auto layered = MultiSample::load (layers, error);
    check (layered != nullptr && layered->zones.size() == 2, "the layers load");
    if (layered == nullptr || layered->zones.size() != 2)
        return;
    float keep = 0.0f, partner = 0.0f;
    check (layered->blendPartner (layered->zones[0], 57, 60, 0.0f, 0, 0, keep, partner) == nullptr && keep == 1.0f,
           "VELOCITY BLEND 0: no blend");
    check (layered->blendPartner (layered->zones[0], 57, 64, 1.0f, 0, 0, keep, partner) == &layered->zones[1]
               && std::abs (keep * keep + partner * partner - 1.0f) < 1.0e-4f && partner > 0.6f,
           "at the edge the two layers mix equal-power, close to half and half");
    check (layered->blendPartner (layered->zones[0], 57, 10, 1.0f, 0, 0, keep, partner) == nullptr, "far from the edge no blend");
    check (layered->blendPartner (layered->zones[1], 57, 66, 1.0f, 0, 0, keep, partner) == &layered->zones[0], "the upper layer blends downward too");

    check (processor.loadUserSample (0, layers, true), "the layered file loads in the plugin");
    samplerSet (processor, "osc1_sample_round_robin", 0.0f);
    auto out = playAt (processor, 57, 64, 12);
    const auto hardA = toneLevel (out, 2048, 220.0, 48000.0), hardB = toneLevel (out, 2048, 330.0, 48000.0);
    check (hardA > 0.01 && hardB < hardA * 0.01, "blend off: velocity 64 plays only the lower layer");
    samplerSet (processor, "osc1_sample_vel_blend", 1.0f);
    out = playAt (processor, 57, 64, 12);
    const auto mixA = toneLevel (out, 2048, 220.0, 48000.0), mixB = toneLevel (out, 2048, 330.0, 48000.0);
    check (mixB > mixA * 0.5 && mixA > hardA * 0.5,
           "blend on: velocity 64 plays both layers (" + juce::String (mixA, 4) + ", " + juce::String (mixB, 4) + ")");
    const auto power = mixA * mixA + mixB * mixB;
    check (std::abs (power / (hardA * hardA) - 1.0) < 0.2, "the blend keeps the level even");
    out = playAt (processor, 57, 10, 12);
    check (toneLevel (out, 2048, 330.0, 48000.0) < toneLevel (out, 2048, 220.0, 48000.0) * 0.01, "a soft note stays on its own layer");
    folder.deleteRecursively();
}

// A SoundFont with two presets in two banks, each one sine zone over the
// whole keyboard: "Low" (bank 0, program 0) recorded at A3 and "High"
// (bank 1, program 3) at A5.
inline void writeTwoPresetSf2 (const juce::File& file)
{
    juce::MemoryOutputStream smpl;
    const auto lowLength = 32000, highLength = 32000;
    const auto addSine = [&smpl] (double hz, int length)
    {
        for (int i = 0; i < length; ++i)
            smpl.writeShort ((short) (16000.0 * std::sin (juce::MathConstants<double>::twoPi * hz * i / 32000.0)));
        for (int i = 0; i < 46; ++i)
            smpl.writeShort (0);
    };
    addSine (220.0, lowLength);
    addSine (880.0, highLength);

    const auto chunk = [] (juce::MemoryOutputStream& out, const char* id, const juce::MemoryBlock& data)
    {
        out.write (id, 4);
        out.writeInt ((int) data.getSize());
        out.write (data.getData(), data.getSize());
        if (data.getSize() & 1)
            out.writeByte (0);
    };
    const auto name20 = [] (juce::MemoryOutputStream& out, const char* name)
    {
        char buffer[20] {};
        std::strncpy (buffer, name, 19);
        out.write (buffer, 20);
    };
    const auto gen = [] (juce::MemoryOutputStream& out, int oper, int amount)
    {
        out.writeShort ((short) oper);
        out.writeShort ((short) amount);
    };
    const auto preset = [&name20] (juce::MemoryOutputStream& out, const char* name, int program, int bank, int bag)
    {
        name20 (out, name);
        out.writeShort ((short) program); out.writeShort ((short) bank); out.writeShort ((short) bag);
        out.writeInt (0); out.writeInt (0); out.writeInt (0);
    };

    juce::MemoryOutputStream phdr, pbag, pgen, pmod, inst, ibag, igen, imod, shdr;
    // Listed High first: the browser sorts by bank and program.
    preset (phdr, "High", 3, 1, 0);
    preset (phdr, "Low", 0, 0, 1);
    preset (phdr, "EOP", 0, 0, 2);
    pbag.writeShort (0); pbag.writeShort (0);
    pbag.writeShort (1); pbag.writeShort (0);
    pbag.writeShort (2); pbag.writeShort (0);
    gen (pgen, MultiSample::Sf2::instrument, 1);
    gen (pgen, MultiSample::Sf2::instrument, 0);
    gen (pgen, 0, 0);
    for (int i = 0; i < 10; ++i) pmod.writeByte (0);
    name20 (inst, "Low Inst"); inst.writeShort (0);
    name20 (inst, "High Inst"); inst.writeShort (1);
    name20 (inst, "EOI"); inst.writeShort (2);
    ibag.writeShort (0); ibag.writeShort (0);
    ibag.writeShort (1); ibag.writeShort (0);
    ibag.writeShort (2); ibag.writeShort (0);
    gen (igen, MultiSample::Sf2::sampleID, 0);
    gen (igen, MultiSample::Sf2::sampleID, 1);
    gen (igen, 0, 0);
    for (int i = 0; i < 10; ++i) imod.writeByte (0);
    const auto header = [&shdr, &name20] (const char* name, int start, int end, int pitch)
    {
        name20 (shdr, name);
        shdr.writeInt (start); shdr.writeInt (end); shdr.writeInt (start); shdr.writeInt (start);
        shdr.writeInt (32000); shdr.writeByte ((char) pitch); shdr.writeByte (0); shdr.writeShort (0); shdr.writeShort (1);
    };
    header ("low", 0, lowLength, 57);
    header ("high", lowLength + 46, lowLength + 46 + highLength, 81);
    header ("EOS", 0, 0, 0);

    juce::MemoryOutputStream sdta, pdta, body;
    sdta.write ("sdta", 4);
    chunk (sdta, "smpl", smpl.getMemoryBlock());
    pdta.write ("pdta", 4);
    chunk (pdta, "phdr", phdr.getMemoryBlock()); chunk (pdta, "pbag", pbag.getMemoryBlock());
    chunk (pdta, "pmod", pmod.getMemoryBlock()); chunk (pdta, "pgen", pgen.getMemoryBlock());
    chunk (pdta, "inst", inst.getMemoryBlock()); chunk (pdta, "ibag", ibag.getMemoryBlock());
    chunk (pdta, "imod", imod.getMemoryBlock()); chunk (pdta, "igen", igen.getMemoryBlock());
    chunk (pdta, "shdr", shdr.getMemoryBlock());
    body.write ("sfbk", 4);
    chunk (body, "LIST", sdta.getMemoryBlock());
    chunk (body, "LIST", pdta.getMemoryBlock());
    juce::MemoryOutputStream riff;
    chunk (riff, "RIFF", body.getMemoryBlock());
    file.replaceWithData (riff.getData(), riff.getDataSize());
}

// Step 38: a SoundFont's presets: listed by bank and program, picked from
// the list, and kept with the patch and the undo history.
void runSoundFontPresetTest()
{
    using namespace SamplerLab;
    const auto folder = scratchFolder ("ilana-sampler-sf2");
    const auto sf2 = folder.getChildFile ("two.sf2");
    writeTwoPresetSf2 (sf2);

    const auto presets = MultiSample::listSf2Presets (sf2);
    check (presets.size() == 2 && presets[0].name == "Low" && presets[1].name == "High" && presets[1].bank == 1 && presets[1].program == 3,
           "the presets are listed by bank and program (" + juce::String ((int) presets.size()) + ")");

    const auto rootOf = [] (IlanaSynthAudioProcessor& p)
    {
        const auto zones = p.getSampleZones (0);
        return zones.empty() ? -1.0 : zones.front().rootNote;
    };

    IlanaSynthAudioProcessor processor;
    MultiSampleLab::setUp (processor);
    check (processor.loadUserSample (0, sf2, true), "the SoundFont loads");
    check (std::abs (rootOf (processor) - 57.0) < 0.01, "it starts on the first preset (bank 0, program 0)");
    check (processor.getSoundFontPreset (0) == std::make_pair (-1, -1), "no preset picked yet");

    processor.performEdit ("SoundFont Preset", [&] { check (processor.selectSoundFontPreset (0, 1, 3), "the second preset loads"); });
    check (std::abs (rootOf (processor) - 81.0) < 0.01, "the picked preset plays");
    check (processor.getSoundFontPreset (0) == std::make_pair (1, 3), "the pick is remembered");
    const auto out = MultiSampleLab::play (processor, 81, 20);
    check (std::abs (MultiSampleLab::sineFrequency (out, 48000.0) - 880.0) < 3.0, "the picked preset sounds at its pitch");

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    IlanaSynthAudioProcessor reloaded;
    reloaded.prepareToPlay (48000.0, 512);
    reloaded.setStateInformation (saved.getData(), (int) saved.getSize());
    reloaded.flushAsyncUpdates();
    check (std::abs (rootOf (reloaded) - 81.0) < 0.01 && reloaded.getSoundFontPreset (0) == std::make_pair (1, 3),
           "the picked preset comes back with the patch");

    processor.getUndoManager().undo();
    check (std::abs (rootOf (processor) - 57.0) < 0.01, "undo goes back to the first preset");
    processor.getUndoManager().redo();
    check (std::abs (rootOf (processor) - 81.0) < 0.01, "redo picks it again");

    // Loading the file again by hand starts from its first preset.
    check (processor.loadUserSample (0, sf2, true) && std::abs (rootOf (processor) - 57.0) < 0.01, "a fresh load starts on the first preset");
    check (! processor.selectSoundFontPreset (1, 0, 0), "an oscillator without a SoundFont has no presets");
    folder.deleteRecursively();
}

// Step 42: the sample browser's logic (lists, folder listing, preview level)
// and the processor's level-matched preview.
void runSampleBrowserTest()
{
    using namespace SamplerLab;
    namespace Model = SampleBrowserModel;

    // Favourites and recent lists.
    juce::StringArray favourites;
    check (Model::toggleEntry (favourites, "/a/one.wav") && Model::toggleEntry (favourites, "/a/two.wav"), "starring adds");
    check (! Model::toggleEntry (favourites, "/a/one.wav") && favourites.size() == 1 && favourites[0] == "/a/two.wav", "starring again removes");

    juce::StringArray recent;
    for (int i = 0; i < 40; ++i)
        Model::addRecent (recent, "/r/" + juce::String (i) + ".wav");
    check (recent.size() == Model::maxRecent && recent[0] == "/r/39.wav" && recent[29] == "/r/10.wav", "recent keeps the newest 30, newest first");
    Model::addRecent (recent, "/r/20.wav");
    check (recent.size() == Model::maxRecent && recent[0] == "/r/20.wav" && recent.indexOf ("/r/20.wav", false, 1) < 0,
           "loading a file again moves it to the front without a repeat");

    const auto parsed = Model::parseList (Model::serialiseList (favourites) + "\n\n/a/two.wav\n/b/three.wav\n");
    check (parsed.size() == 2 && parsed[0] == "/a/two.wav" && parsed[1] == "/b/three.wav", "a list survives its string, minus blanks and repeats");

    const auto folder = scratchFolder ("ilana-sample-browser");
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    check (Model::fromPortablePath (Model::toPortablePath (folder)) == folder, "a path survives the portable form");
    if (folder.isAChildOf (home))
        check (Model::toPortablePath (folder).startsWith ("~/"), "a path under the home folder is stored relative to it");
    check (Model::toPortablePath (home.getChildFile ("Music/a.wav")) == "~/Music/a.wav", "home-relative paths use forward slashes");
    check (Model::fromPortablePath ("~/Music/a.wav") == home.getChildFile ("Music/a.wav"), "the home mark expands");

    // Folder listing: subfolders first, then audio files by name, filtered.
    folder.getChildFile ("zeta").createDirectory();
    folder.getChildFile ("Alpha").createDirectory();
    folder.getChildFile (".hidden").createDirectory();
    for (const auto* name : { "kick10.wav", "kick2.wav", "Snare.flac", "pad.sf2", "notes.txt", "loop.AIFF", ".secret.wav", "song.mp3" })
        folder.getChildFile (name).replaceWithText ("x");

    const auto listing = Model::listFolder (folder);
    juce::StringArray names;
    for (const auto& entry : listing)
        names.add (entry.file.getFileName());
    check (names.joinIntoString ("|") == "Alpha|zeta|kick2.wav|kick10.wav|loop.AIFF|pad.sf2|Snare.flac|song.mp3",
           "folders first, then audio files by name with numbers in order (" + names.joinIntoString ("|") + ")");
    check (listing[0].isFolder && listing[1].isFolder && ! listing[2].isFolder, "entries know if they are folders");

    const auto filtered = Model::listFolder (folder, "KICK wav");
    check (filtered.size() == 2 && filtered[0].file.getFileName() == "kick2.wav", "the filter keeps names with every word");
    check (Model::listFolder (folder, "nothing like this").empty(), "a filter with no match lists nothing");

    juce::StringArray stored { Model::toPortablePath (folder.getChildFile ("kick2.wav")), Model::toPortablePath (folder.getChildFile ("gone.wav")) };
    check (Model::dropMissing (stored) && stored.size() == 1, "a missing file leaves the list");
    check (Model::isPreviewable (folder.getChildFile ("a.wav")) && ! Model::isPreviewable (folder.getChildFile ("a.sf2"))
               && ! Model::isPreviewable (folder.getChildFile ("a.SFZ")) && ! Model::isPreviewable (folder.getChildFile ("a.txt")),
           "only plain audio files preview");

    // Preview level.
    check (std::abs (juce::Decibels::gainToDecibels (Model::previewGain (0.05f, 0.1f) * 0.05f) - Model::previewTargetDb) < 0.01f,
           "a quiet file is lifted to the target RMS");
    check (std::abs (juce::Decibels::gainToDecibels (Model::previewGain (0.5f, 0.9f) * 0.5f) - Model::previewTargetDb) < 0.01f,
           "a loud file is lowered to the target RMS");
    check (Model::previewGain (0.01f, 1.0f) * 1.0f <= 0.981f, "a peaky file is held under full scale");
    check (Model::previewGain (0.0f, 0.0f) == 1.0f, "silence keeps its level");

    // The processor's preview: a quiet and a loud WAV come out alike.
    const auto quiet = folder.getChildFile ("quiet.wav");
    const auto loud = folder.getChildFile ("loud.wav");
    auto scaled = [] (double hz, float amplitude, int length)
    {
        auto buffer = MultiSampleLab::sine (hz, 44100.0, length);
        buffer.applyGain (amplitude / 0.5f);
        return buffer;
    };
    MultiSampleLab::writeWav (quiet, scaled (330.0, 0.02f, 44100), 44100.0);
    MultiSampleLab::writeWav (loud, scaled (220.0, 0.8f, 44100), 44100.0);

    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    juce::MidiBuffer noMidi;
    juce::AudioBuffer<float> block (2, 512);

    const auto renderBlocks = [&] (int count)
    {
        double sum = 0.0;
        float peak = 0.0f;
        long samples = 0;
        for (int b = 0; b < count; ++b)
        {
            block.clear();
            processor.processBlock (block, noMidi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                {
                    const auto v = block.getSample (c, i);
                    sum += (double) v * (double) v;
                    peak = juce::jmax (peak, std::abs (v));
                    ++samples;
                }
        }
        return std::make_pair (std::sqrt (sum / (double) juce::jmax (1L, samples)), peak);
    };

    check (renderBlocks (4).second < 1.0e-4f, "no preview, no output");
    check (! processor.isSamplePreviewPlaying(), "nothing previews before it is started");

    check (processor.startSamplePreview (quiet), "a quiet file starts a preview");
    renderBlocks (2); // the fade in
    const auto quietLevel = renderBlocks (40).first;
    check (processor.isSamplePreviewPlaying(), "the preview plays");
    processor.stopSamplePreview();
    renderBlocks (3);

    check (processor.startSamplePreview (loud), "a loud file starts a preview");
    renderBlocks (2);
    const auto loudLevel = renderBlocks (40).first;
    const auto targetRms = (double) juce::Decibels::decibelsToGain (Model::previewTargetDb);
    check (std::abs (juce::Decibels::gainToDecibels ((float) quietLevel) - juce::Decibels::gainToDecibels ((float) loudLevel)) < 0.5f,
           "quiet and loud files preview at the same level (" + juce::String (quietLevel) + " and " + juce::String (loudLevel) + ")");
    check (std::abs (juce::Decibels::gainToDecibels ((float) loudLevel) - Model::previewTargetDb) < 1.0f,
           "the level is about " + juce::String ((int) Model::previewTargetDb) + " dBFS RMS (" + juce::String (loudLevel / targetRms) + " of the target)");

    processor.stopSamplePreview();
    renderBlocks (3);
    check (renderBlocks (4).second < 1.0e-4f && ! processor.isSamplePreviewPlaying(), "stopping silences the preview");

    check (! processor.startSamplePreview (folder.getChildFile ("notes.txt")), "a file that is not audio does not preview");

    // Only the first ten seconds are decoded, and a short file plays out and ends.
    const auto longFile = folder.getChildFile ("long.wav");
    MultiSampleLab::writeWav (longFile, MultiSampleLab::sine (200.0, 8000.0, 8000 * 14), 8000.0);
    check (processor.startSamplePreview (longFile), "a long file previews");
    renderBlocks (4);
    check (processor.isSamplePreviewPlaying(), "a long file is still playing after a fraction of a second");
    const auto shortFile = folder.getChildFile ("short.wav");
    MultiSampleLab::writeWav (shortFile, scaled (440.0, 0.3f, 2000), 44100.0);
    check (processor.startSamplePreview (shortFile), "a short file previews");
    renderBlocks (8);
    check (! processor.isSamplePreviewPlaying() && renderBlocks (2).second < 1.0e-4f, "a short file plays out and ends");

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
    runSampleRoundRobinTest();
    runSoundFontPresetTest();
    runSampleBrowserTest();
}
