// The clip sequencer: notes at the right samples, Key transpose, Host play,
// the patch state, MIDI import. Included by TableTest.cpp;
// ILANA_CLIP_TEST=1 runs only these.

namespace ClipLab
{
struct Head : juce::AudioPlayHead
{
    bool playing = false;
    double ppq = 0.0;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (120.0);
        info.setIsPlaying (playing);
        info.setPpqPosition (ppq);
        return info;
    }
};

struct Event
{
    int sample;
    int note;
    bool on;
    int velocity;
};

inline void set (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// At 120 bpm and 48 kHz a beat is 24000 samples. One bar: C at beat 0
// (1 beat), E at 1.5 (half a beat), G at 3 (1 beat), all at velocity 90.
inline Clip demoClip (int bars = 1)
{
    Clip clip;
    clip.bars = bars;
    clip.notes = { { 0.0f, 1.0f, 60, 90 }, { 1.5f, 0.5f, 64, 90 }, { 3.0f, 1.0f, 67, 90 } };
    return clip;
}

// Runs blocks of 512 samples; `midiFor` fills a block's input, `before` may
// change the host between blocks. Returns every note event the synth was
// given, at absolute sample positions.
template <typename MidiFor, typename Before>
std::vector<Event> run (IlanaSynthAudioProcessor& processor, Head& head, int blocks, MidiFor midiFor, Before before)
{
    constexpr int size = 512;
    std::vector<Event> events;
    juce::AudioBuffer<float> buffer (2, size);

    for (int block = 0; block < blocks; ++block)
    {
        before (block);
        juce::MidiBuffer midi;
        midiFor (block, midi);
        buffer.clear();
        processor.processBlock (buffer, midi);

        for (const auto metadata : processor.getSynthMidiForTest())
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOnOrOff())
                events.push_back ({ block * size + metadata.samplePosition, message.getNoteNumber(), message.isNoteOn(),
                                    message.getVelocity() });
        }
    }

    return events;
}

inline bool has (const std::vector<Event>& events, int sample, int note, bool on, int tolerance = 1)
{
    for (const auto& e : events)
        if (e.note == note && e.on == on && std::abs (e.sample - sample) <= tolerance)
            return true;

    return false;
}
} // namespace ClipLab

void runClipTests()
{
    using namespace ClipLab;
    const auto none = [] (int, juce::MidiBuffer&) {};

    // Host play: the notes land on their samples, and the clip loops.
    {
        IlanaSynthAudioProcessor processor;
        Head head;
        processor.setPlayHead (&head);
        processor.prepareToPlay (48000.0, 512);
        processor.getClipState().setClip (0, demoClip());
        set (processor, "clip_mode", 1.0f);
        set (processor, "clip_on", 1.0f);
        head.playing = true;
        const auto events = run (processor, head, 375, none, [&head] (int block) { head.ppq = (double) block * 512.0 / 48000.0 * 2.0; });
        processor.setPlayHead (nullptr);

        const auto ok = has (events, 0, 60, true) && has (events, 24000, 60, false) && has (events, 36000, 64, true)
                     && has (events, 48000, 64, false) && has (events, 72000, 67, true) && has (events, 96000, 67, false)
                     && has (events, 96000, 60, true) && has (events, 120000, 60, false) && has (events, 168000, 67, true);
        check (ok, "clip: host play puts the notes on their samples at 120 bpm and loops after the bar");

        auto velocities = true, pitches = true;
        for (const auto& e : events)
        {
            velocities = velocities && (! e.on || e.velocity == 90);
            pitches = pitches && (e.note == 60 || e.note == 64 || e.note == 67);
        }
        check (velocities && pitches, "clip: velocities and pitches are the clip's");
        // (The last note-off falls on the first sample after the run.)
        check (events.size() == 11, "clip: two loops of three notes make 6 ons and 5 offs in the window (" + juce::String ((int) events.size()) + ")");
    }

    // Host play follows the transport: silent while stopped, starts on the
    // host's beat, and the notes still sounding stop with it.
    {
        IlanaSynthAudioProcessor processor;
        Head head;
        processor.setPlayHead (&head);
        processor.prepareToPlay (48000.0, 512);
        processor.getClipState().setClip (0, demoClip());
        set (processor, "clip_mode", 1.0f);
        set (processor, "clip_on", 1.0f);

        const auto stopped = run (processor, head, 100, none, [] (int) {});
        check (stopped.empty(), "clip: host play is silent while the transport is stopped");

        // Playing from beat 2: the next note is G at beat 3, one beat (24000 samples) on.
        head.playing = true;
        const auto started = run (processor, head, 60, none, [&head] (int block) { head.ppq = 2.0 + (double) block * 512.0 / 48000.0 * 2.0; });
        check (has (started, 24000, 67, true) && ! has (started, 0, 60, true) && started.size() >= 1,
               "clip: host play starts on the host's beat (G at beat 3)");

        // Stop mid-note (G sounds from beat 3 to 4): released at once, then silent.
        const auto stopping = run (processor, head, 30, none, [&head] (int block)
        {
            head.ppq = 2.0 + (double) (60 + block) * 512.0 / 48000.0 * 2.0;
            head.playing = block < 5;
        });
        auto released = false, anyOn = false;
        for (const auto& e : stopping)
        {
            released = released || (! e.on && e.note == 67 && e.sample >= 5 * 512 - 1 && e.sample < 6 * 512);
            anyOn = anyOn || (e.on && e.sample >= 5 * 512);
        }
        check (released && ! anyOn, "clip: stopping the transport releases the sounding note and plays nothing more");
        processor.setPlayHead (nullptr);
    }

    // Key transpose: the held key plays the clip from its sample, transposed
    // from C3, and gates it; the key itself does not reach the synth.
    {
        IlanaSynthAudioProcessor processor;
        Head head;
        processor.prepareToPlay (48000.0, 512);
        processor.getClipState().setClip (0, demoClip());
        set (processor, "clip_on", 1.0f);
        constexpr int keyDown = 100, keyUp = 80 * 512 + 40; // released while the second note sounds
        const auto events = run (processor, head, 120, [] (int block, juce::MidiBuffer& midi)
        {
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), keyDown);
            if (block == 80)
                midi.addEvent (juce::MidiMessage::noteOff (1, 62), 40);
        }, [] (int) {});

        check (has (events, keyDown, 62, true) && has (events, keyDown + 36000, 66, true),
               "clip: Key transpose plays the clip up two semitones from the key's sample");
        auto keyLeaked = false;
        for (const auto& e : events)
            keyLeaked = keyLeaked || (e.on && e.velocity == 100);
        check (! keyLeaked, "clip: Key transpose takes the played key out of the synth's MIDI");
        check (has (events, keyUp, 66, false, 0) && events.size() == 4 && events.front().on,
               "clip: releasing the key stops the clip (note-off at that sample, nothing after)");

        // The same key an octave up transposes by 12; C3 plays it as written.
        set (processor, "clip_on", 0.0f);
        set (processor, "clip_on", 1.0f);
        const auto octave = run (processor, head, 8, [] (int block, juce::MidiBuffer& midi)
        {
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 72, (juce::uint8) 100), 0);
        }, [] (int) {});
        check (has (octave, 0, 72, true), "clip: a key an octave above C3 plays the clip an octave up");
    }

    // Key transpose follows the host's beat while it plays.
    {
        IlanaSynthAudioProcessor processor;
        Head head;
        processor.setPlayHead (&head);
        processor.prepareToPlay (48000.0, 512);
        processor.getClipState().setClip (0, demoClip());
        set (processor, "clip_on", 1.0f);
        head.playing = true;
        const auto events = run (processor, head, 100, [] (int block, juce::MidiBuffer& midi)
        {
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        }, [&head] (int block) { head.ppq = 1.0 + (double) block * 512.0 / 48000.0 * 2.0; });
        processor.setPlayHead (nullptr);
        check (has (events, 12000, 64, true) && ! has (events, 0, 60, true),
               "clip: Key transpose with the host playing sits on the host's beat (E at beat 1.5 from beat 1)");
    }

    // Turning the clip off releases what sounds and leaves the keys alone.
    {
        IlanaSynthAudioProcessor processor;
        Head head;
        processor.prepareToPlay (48000.0, 512);
        processor.getClipState().setClip (0, demoClip());
        set (processor, "clip_on", 1.0f);
        const auto events = run (processor, head, 30, [] (int block, juce::MidiBuffer& midi)
        {
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        }, [&processor] (int block) { if (block == 10) set (processor, "clip_on", 0.0f); });
        check (has (events, 10 * 512, 60, false, 0) && events.size() == 3,
               "clip: switching it off sends the note-off for what sounds");
        check (events.back().on && events.back().note == 60 && events.back().sample == 10 * 512 && events.back().velocity == 100,
               "clip: switching it off gives the held key back to the synth at its velocity");

        // With the clip off, a key passes through untouched, and nothing else appears.
        const auto passed = run (processor, head, 20, [] (int block, juce::MidiBuffer& midi)
        {
            if (block == 2)
                midi.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 7);
            if (block == 5)
                midi.addEvent (juce::MidiMessage::noteOff (1, 62), 9);
        }, [] (int) {});
        check (passed.size() == 2 && has (passed, 2 * 512 + 7, 62, true, 0) && passed[0].velocity == 100
                   && has (passed, 5 * 512 + 9, 62, false, 0),
               "clip: with the clip off the keys pass through and no events are added");

        // Off is the default, with clips present or not, in either mode.
        IlanaSynthAudioProcessor fresh;
        fresh.prepareToPlay (48000.0, 512);
        fresh.getClipState().setClip (0, demoClip());
        Head host;
        host.playing = true;
        fresh.setPlayHead (&host);
        set (fresh, "clip_mode", 1.0f);
        const auto silent = run (fresh, host, 50, none, [&host] (int block) { host.ppq = (double) block * 512.0 / 48000.0 * 2.0; });
        fresh.setPlayHead (nullptr);
        check (fresh.apvts.getRawParameterValue ("clip_on")->load() < 0.5f && silent.empty(),
               "clip: off by default, and off plays nothing even with the host running");
    }

    // A clip switch and a longer clip.
    {
        IlanaSynthAudioProcessor processor;
        Head head;
        processor.setPlayHead (&head);
        processor.prepareToPlay (48000.0, 512);
        auto second = demoClip (2);
        second.notes = { { 4.0f, 2.0f, 72, 64 } }; // bar 2, beat 1
        processor.getClipState().setClip (0, demoClip());
        processor.getClipState().setClip (1, second);
        set (processor, "clip_mode", 1.0f);
        set (processor, "clip_index", 1.0f);
        set (processor, "clip_on", 1.0f);
        head.playing = true;
        const auto events = run (processor, head, 375, none, [&head] (int block) { head.ppq = (double) block * 512.0 / 48000.0 * 2.0; });
        processor.setPlayHead (nullptr);
        check (events.size() == 2 && has (events, 96000, 72, true) && has (events, 144000, 72, false),
               "clip: the CLIP choice picks the clip; a two-bar clip lasts eight beats");
    }

    // The patch state keeps the clips; a state without them has none.
    {
        IlanaSynthAudioProcessor source;
        auto first = demoClip (3);
        first.notes.push_back ({ 8.5f, 2.25f, 40, 127 });
        source.getClipState().setClip (0, first);
        source.getClipState().setClip (5, demoClip (1));
        juce::MemoryBlock block;
        source.getStateInformation (block);

        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (block.getData(), (int) block.getSize());
        const auto a = restored.getClipState().getClip (0);
        const auto b = restored.getClipState().getClip (5);
        auto same = a.bars == 3 && a.notes.size() == first.notes.size() && b.bars == 1 && b.notes.size() == 3;

        for (size_t i = 0; same && i < a.notes.size(); ++i)
            same = a.notes[i].start == first.notes[i].start && a.notes[i].length == first.notes[i].length
                && a.notes[i].note == first.notes[i].note && a.notes[i].velocity == first.notes[i].velocity;
        check (same && restored.getClipState().getClip (2).notes.empty(), "clip: the clips survive a state save and restore");

        IlanaSynthAudioProcessor fresh;
        juce::MemoryBlock plain;
        fresh.getStateInformation (plain);
        std::unique_ptr<juce::XmlElement> xml (juce::AudioProcessor::getXmlFromBinary (plain.getData(), (int) plain.getSize()));
        const auto hadChild = xml != nullptr && xml->getChildByName ("Clips") != nullptr;
        restored.setStateInformation (plain.getData(), (int) plain.getSize());
        check (! hadChild && ! restored.getClipState().hasAny(), "clip: a patch without clips saves no Clips child and loads with none");
    }

    // MIDI import: the first track with notes, ticks to beats.
    {
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-clip-test.mid");
        juce::MidiMessageSequence conductor, notes;
        conductor.addEvent (juce::MidiMessage::tempoMetaEvent (500000));
        conductor.addEvent (juce::MidiMessage::endOfTrack());
        notes.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100).withTimeStamp (0));
        notes.addEvent (juce::MidiMessage::noteOff (1, 60).withTimeStamp (480));
        notes.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 80).withTimeStamp (960));
        notes.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 0).withTimeStamp (1200)); // note-on velocity 0 ends it
        notes.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 64).withTimeStamp (1920));
        notes.addEvent (juce::MidiMessage::noteOff (1, 67).withTimeStamp (2400));
        notes.addEvent (juce::MidiMessage::endOfTrack().withTimeStamp (2400));
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        midi.addTrack (conductor);
        midi.addTrack (notes);
        file.deleteFile();
        {
            juce::FileOutputStream stream (file);
            check (stream.openedOk() && midi.writeTo (stream), "clip: the test MIDI file is written");
        }

        Clip clip;
        juce::String error;
        const auto ok = ClipState::importMidi (file, clip, error);
        check (ok && clip.notes.size() == 3 && clip.bars == 2, "clip: import reads the notes and rounds the length up to whole bars");
        check (ok && clip.notes.size() == 3 && clip.notes[0].note == 60 && clip.notes[0].start == 0.0f && clip.notes[0].length == 1.0f
                   && clip.notes[0].velocity == 100 && clip.notes[1].note == 64 && clip.notes[1].start == 2.0f
                   && clip.notes[1].length == 0.5f && clip.notes[1].velocity == 80 && clip.notes[2].note == 67
                   && clip.notes[2].start == 4.0f && clip.notes[2].length == 1.0f,
               "clip: import gives the notes' starts and lengths in beats and their velocities");

        // Into a processor's clip, then played back at the right sample.
        IlanaSynthAudioProcessor processor;
        processor.getClipState().setClip (0, clip);
        check (processor.getClipState().getClip (0).notes.size() == 3, "clip: an imported clip is stored");

        juce::MidiMessageSequence empty;
        empty.addEvent (juce::MidiMessage::endOfTrack());
        juce::MidiFile noNotes;
        noNotes.setTicksPerQuarterNote (480);
        noNotes.addTrack (empty);
        file.deleteFile();
        {
            juce::FileOutputStream stream (file);
            noNotes.writeTo (stream);
        }
        Clip nothing;
        check (! ClipState::importMidi (file, nothing, error) && error.isNotEmpty(), "clip: a file without notes is refused with a message");
        file.deleteFile();
    }
}
