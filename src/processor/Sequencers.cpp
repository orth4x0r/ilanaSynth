#include "ProcessorInternal.h"

int IlanaSynthAudioProcessor::selectArpNote (int mode, int octaves)
{
    const auto count = arpHeldNotes.size();

    if (count == 0)
        return 60;

    const auto total = count * octaves;

    // Chord: all held notes stacked through the octaves, played together.
    if (mode == 7)
    {
        arpChordNotes.clear();

        for (int octave = 0; octave < octaves; ++octave)
            for (int i = 0; i < count; ++i)
                arpChordNotes.add (arpHeldNotes[i] + 12 * octave);

        return arpChordNotes[0];
    }

    if (mode == 3)
        return arpHeldNotes[arpRandom.nextInt (count)] + 12 * arpRandom.nextInt (octaves);

    // Scale Random: any scale note from the lowest held note up through the
    // octaves (random held notes when no scale is set).
    if (mode == 8)
    {
        const auto scale = (int) getParam ("gen_scale");

        if (scale <= 0)
            return arpHeldNotes[arpRandom.nextInt (count)] + 12 * arpRandom.nextInt (octaves);

        const auto low = arpHeldNotes[0];
        const auto span = juce::jmax (12 * octaves, arpHeldNotes[count - 1] - low + 1);
        return Scales::quantize (low + arpRandom.nextInt (span), scale, (int) getParam ("gen_root"));
    }

    if (total <= 1)
        return arpHeldNotes[0];

    auto index = 0;

    switch (mode)
    {
        case 2: // UpDown
        case 4: // DownUp (same zigzag, flipped)
        {
            arpStepIndex = juce::jlimit (0, total - 1, arpStepIndex);
            index = mode == 2 ? arpStepIndex : total - 1 - arpStepIndex;

            arpStepIndex += arpDirection;

            if (arpStepIndex >= total - 1 || arpStepIndex <= 0)
                arpDirection = -arpDirection;

            arpStepIndex = juce::jlimit (0, total - 1, arpStepIndex);
            break;
        }

        case 5: // Converge: low, high, 2nd low, 2nd high...
            index = (arpStepIndex % 2 == 0) ? (arpStepIndex / 2) : (total - 1 - arpStepIndex / 2);
            arpStepIndex = (arpStepIndex + 1) % total;
            break;

        case 6: // Walk: random walk up and down.
            arpStepIndex = (arpStepIndex + (arpRandom.nextBool() ? 1 : total - 1)) % total;
            index = arpStepIndex;
            break;

        default: // 0 Up, 1 Down
            index = mode == 1 ? total - 1 - (arpStepIndex % total) : (arpStepIndex % total);
            arpStepIndex = (arpStepIndex + 1) % total;
            break;
    }

    index = juce::jlimit (0, total - 1, index);
    return arpHeldNotes[index % count] + 12 * (index / count);
}

void IlanaSynthAudioProcessor::processArpeggiator (juce::MidiBuffer& midiMessages, int numSamples, juce::MidiBuffer& output)
{
    output.clear();

    const auto lastSample = juce::jmax (0, numSamples - 1);

    // One note engine serves three things (M7.1): the arpeggiator, the
    // probability sequencer (which takes over while on), and Euclid in Notes
    // mode, which rests the engine's off-beat steps. With only Euclid on,
    // the engine plays the held chord at Euclid's rate.
    const auto arpOn = getParam ("arp_on") > 0.5f;
    const auto pseqOn = getParam ("pseq_on") > 0.5f;
    const auto euclidNotes = getParam ("euc_on") > 0.5f && (int) getParam ("euc_target") == 0;
    const auto arpEnabled = arpOn || pseqOn || euclidNotes;
    const auto playing = hostPlaying.load();
    const auto transportStopped = arpHostWasPlaying && ! playing;
    arpHostWasPlaying = playing;

    const auto releaseSounding = [this, &output] (int position)
    {
        if (arpActiveNote >= 0)
            output.addEvent (juce::MidiMessage::noteOff (1, arpActiveNote), position);

        for (auto chordNote : arpChordActive)
            output.addEvent (juce::MidiMessage::noteOff (1, chordNote), position);

        arpChordActive.clearQuick();
        arpActiveNote = -1;
        arpGateRemaining = 0;
    };

    // Held keys are tracked even while the arp is off, so switching it on
    // over a held chord starts at once. All Notes Off / All Sound Off and a
    // host transport stop drop every held key: a clip or keyboard whose
    // note-offs never arrive must not leave the pattern running.
    const auto trackKeys = [this] (const juce::MidiMessage& message)
    {
        if (message.isNoteOn())
        {
            if (! arpHeldNotes.contains (message.getNoteNumber()))
            {
                arpHeldNotes.add (message.getNoteNumber());
                arpHeldNotes.sort();
            }
        }
        else if (message.isNoteOff())
        {
            arpHeldNotes.removeAllInstancesOf (message.getNoteNumber());
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            arpHeldNotes.clearQuick();
        }
    };

    if (transportStopped)
        arpHeldNotes.clearQuick();

    if (! arpEnabled)
    {
        if (arpWasEnabled)
        {
            releaseSounding (0);
            arpCounter = 0;
            arpWasEnabled = false;
        }

        for (const auto metadata : midiMessages)
            trackKeys (metadata.getMessage());

        output.addEvents (midiMessages, 0, numSamples, 0);
        return;
    }

    arpWasEnabled = true;

    const auto bpm = juce::jmax (20.0, currentBpm.load());
    auto beats = getSyncDivisionBeats ((int) getParam (pseqOn ? "pseq_div" : arpOn ? "arp_div" : "euc_div"));

    if (beats <= 0.0)
        beats = 0.5;

    const auto exactStep = (60.0 / bpm) * beats * currentSampleRate;
    const auto samplesPerStep = juce::jmax (16, (int) exactStep);
    const auto gate = juce::jlimit (0.05f, 1.0f, getParam (pseqOn ? "pseq_gate" : arpOn ? "arp_gate" : "euc_gate"));
    const auto gateSamples = juce::jmax (8, (int) ((float) samplesPerStep * gate));
    const auto octaves = arpOn && ! pseqOn ? juce::jlimit (1, 4, (int) getParam ("arp_octaves")) : 1;
    // Mode -1 is the probability sequencer; 7 (Chord) serves Euclid alone.
    const auto mode = pseqOn ? -1 : arpOn ? juce::jlimit (0, 8, (int) getParam ("arp_mode")) : 7;
    const auto chance = arpOn && ! pseqOn ? juce::jlimit (0.0f, 1.0f, getParam ("arp_chance")) : 1.0f;
    const auto euclidSteps = juce::jlimit (2, 32, (int) getParam ("euc_steps"));
    const auto euclidHits = juce::jlimit (0, 32, (int) getParam ("euc_hits"));
    const auto euclidRotate = juce::jlimit (0, 31, (int) getParam ("euc_rotate"));
    const auto pseqLength = juce::jlimit (1, 16, (int) getParam ("pseq_length"));
    const auto ppqAtBlockStart = hostPpq.load();

    // While the host plays, steps land on its beat grid. A step played
    // just before a grid line (a key pressed a little early) keeps that
    // line's slot instead of firing again straight after.
    const auto samplesToNextStep = [&] (int position)
    {
        if (! playing || exactStep < 1.0)
            return samplesPerStep;

        const auto ppq = ppqAtBlockStart + (double) position / currentSampleRate * (bpm / 60.0);
        const auto stepPosition = ppq / beats;
        auto toNext = (1.0 - (stepPosition - std::floor (stepPosition))) * exactStep;

        if (toNext < exactStep * 0.25)
            toNext += exactStep;

        return juce::jmax (16, juce::roundToInt (toNext));
    };

    if (transportStopped)
    {
        releaseSounding (0);
        arpCounter = 0;
    }

    // The step's number: from the host's beat grid while it plays (so
    // patterns line up with the bar), else counted from the first key.
    const auto stepNumber = [&] (int position)
    {
        if (! playing)
            return engineStepCount;

        const auto ppq = ppqAtBlockStart + (double) position / currentSampleRate * (bpm / 60.0);
        return (long long) std::llround (ppq / beats);
    };

    const auto triggerStep = [&] (int position)
    {
        releaseSounding (position);
        arpRatchetsLeft = 0;

        const auto number = stepNumber (position);
        ++engineStepCount;
        engineDisplayStep.store ((int) (number & 0xffffff));

        if (euclidNotes)
            euclidDisplayStep.store ((int) (((number % euclidSteps) + euclidSteps) % euclidSteps));

        if (euclidNotes && ! euclidHit ((int) (((number % euclidSteps) + euclidSteps) % euclidSteps),
                                        euclidHits, euclidSteps, euclidRotate))
        {
            // An off-beat of the Euclid rhythm: the step rests.
            arpGateRemaining = gateSamples;
            arpCounter = samplesToNextStep (position);
            return;
        }

        if (mode == -1)
        {
            // Probability sequencer: a held key, raised by up to RANGE and
            // snapped to the scale, played RATCHET times in the step.
            const auto step = (size_t) (((number % pseqLength) + pseqLength) % pseqLength);
            const auto stepChance = getParam (pseqChanceIds[step]);
            const auto range = juce::jlimit (0, 24, (int) getParam (pseqRangeIds[step]));
            const auto ratchet = juce::jlimit (1, 4, (int) getParam (pseqRatchetIds[step]));

            if (pseqRandom.nextFloat() < stepChance)
            {
                auto note = arpHeldNotes[pseqRandom.nextInt (arpHeldNotes.size())]
                            + (range > 0 ? pseqRandom.nextInt (range + 1) : 0);
                const auto scale = (int) getParam ("gen_scale");

                if (scale > 0)
                    note = Scales::quantize (note, scale, (int) getParam ("gen_root"));

                note = juce::jlimit (0, 127, note);
                const auto interval = juce::jmax (8, samplesPerStep / ratchet);
                output.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), position);
                arpActiveNote = note;
                arpGateRemaining = juce::jmax (4, (int) ((float) interval * gate));
                arpRatchetNote = note;
                arpRatchetsLeft = ratchet - 1;
                arpRatchetInterval = interval;
                arpRatchetCounter = interval;
            }
            else
            {
                arpGateRemaining = gateSamples;
            }

            arpCounter = samplesToNextStep (position);
            return;
        }

        const auto note = selectArpNote (mode, octaves);
        const auto rest = chance < 1.0f && arpRandom.nextFloat() >= chance;

        if (! rest)
        {
            if (mode == 7)
            {
                for (auto chordNote : arpChordNotes)
                {
                    output.addEvent (juce::MidiMessage::noteOn (1, chordNote, (juce::uint8) 100), position);
                    arpChordActive.add (chordNote);
                }
            }
            else
            {
                output.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), position);
                arpActiveNote = note;
            }
        }

        arpGateRemaining = gateSamples;
        arpCounter = samplesToNextStep (position);
    };

    // Runs the step clock over [from, to).
    const auto runSteps = [&] (int from, int to)
    {
        auto position = from;

        while (position < to)
        {
            const auto sounding = arpActiveNote >= 0 || ! arpChordActive.isEmpty();

            if (sounding && (arpGateRemaining <= 0 || arpHeldNotes.isEmpty()))
                releaseSounding (position);

            if (arpHeldNotes.isEmpty())
                return;

            if (arpCounter <= 0)
            {
                triggerStep (position);
            }
            else if (arpRatchetsLeft > 0 && arpRatchetCounter <= 0)
            {
                // The next repeat of a ratcheted step.
                releaseSounding (position);
                output.addEvent (juce::MidiMessage::noteOn (1, arpRatchetNote, (juce::uint8) 100), position);
                arpActiveNote = arpRatchetNote;
                arpGateRemaining = juce::jmax (4, (int) ((float) arpRatchetInterval * gate));
                arpRatchetCounter = arpRatchetInterval;
                --arpRatchetsLeft;
            }

            auto advance = juce::jmin (to - position, arpCounter);

            if ((arpActiveNote >= 0 || ! arpChordActive.isEmpty()) && arpGateRemaining > 0)
                advance = juce::jmin (advance, arpGateRemaining);

            if (arpRatchetsLeft > 0)
                advance = juce::jmin (advance, arpRatchetCounter);

            advance = juce::jmax (1, advance);
            arpCounter -= advance;
            arpGateRemaining -= advance;
            arpRatchetCounter -= advance;
            position += advance;
        }
    };

    // Walk the block event by event so releases and new keys act at the
    // sample they arrive on, not at the next block.
    auto position = 0;

    for (const auto metadata : midiMessages)
    {
        const auto eventPosition = juce::jlimit (0, lastSample, metadata.samplePosition);
        runSteps (position, eventPosition);
        position = juce::jmax (position, eventPosition);

        const auto message = metadata.getMessage();
        const auto wasEmpty = arpHeldNotes.isEmpty();
        trackKeys (message);

        if (! message.isNoteOnOrOff())
            output.addEvent (message, position);

        if (arpHeldNotes.isEmpty() && ! wasEmpty)
        {
            releaseSounding (position);
            arpCounter = 0;
            engineDisplayStep.store (-1);
        }
        else if (wasEmpty && ! arpHeldNotes.isEmpty())
        {
            arpCounter = 0;
            arpRatchetsLeft = 0;
            engineStepCount = 0;
        }
    }

    runSteps (position, numSamples);
}

// Euclid in Exciter mode: each hit re-strikes the Physical strings of the
// notes held down. The hit travels as a private SysEx marker, so it lands
// on its exact sample inside the synth's render (IlanaSynth::handleMidiEvent).
void IlanaSynthAudioProcessor::addEuclidExciterHits (juce::MidiBuffer& midi, int numSamples)
{
    if (getParam ("euc_on") < 0.5f || (int) getParam ("euc_target") != 1 || numSamples <= 0)
    {
        euclidExciterLastStep = -1;
        return;
    }

    const auto bpm = juce::jmax (20.0, currentBpm.load());
    const auto beats = juce::jmax (0.001, getSyncDivisionBeats ((int) getParam ("euc_div")));
    const auto stepsPerSample = (bpm / 60.0) / beats / currentSampleRate;
    const auto steps = juce::jlimit (2, 32, (int) getParam ("euc_steps"));
    const auto hits = juce::jlimit (0, 32, (int) getParam ("euc_hits"));
    const auto rotate = juce::jlimit (0, 31, (int) getParam ("euc_rotate"));
    const auto start = hostPlaying.load() ? hostPpq.load() / beats : euclidExciterPhase;

    // A jump back (a loop or a relocate) starts counting again.
    if ((double) euclidExciterLastStep > start + 1.0)
        euclidExciterLastStep = (long long) std::floor (start) - 1;

    const auto end = start + (double) numSamples * stepsPerSample;

    for (auto k = (long long) std::ceil (start - 1.0e-9); (double) k < end; ++k)
    {
        if (k <= euclidExciterLastStep)
            continue;

        euclidExciterLastStep = k;
        euclidDisplayStep.store ((int) (((k % steps) + steps) % steps));
        const auto offset = juce::jlimit (0, numSamples - 1, (int) std::ceil (((double) k - start) / stepsPerSample));

        if (euclidHit ((int) (((k % steps) + steps) % steps), hits, steps, rotate))
        {
            const juce::uint8 marker[] { 0xf0, IlanaSynth::exciterMarker[0], IlanaSynth::exciterMarker[1],
                                         IlanaSynth::exciterMarker[2], 100, 0xf7 };
            midi.addEvent (marker, (int) sizeof (marker), offset);
        }
    }

    euclidExciterPhase = std::fmod (end, 4096.0);
}

// Clip sequencer: plays the current clip (ClipState) as note events in the
// synth's MIDI, before the voices render. Key transpose: the held key (C3
// plays the clip as written) starts it and gates it, and the played keys
// don't reach the voices; it follows the host's beat while the transport
// plays, else counts from the key. Host play: it plays only while the host
// does, on the host's beat, and the played keys still sound. With the clip
// off nothing is touched (the sounding notes are released once).
void IlanaSynthAudioProcessor::processClip (juce::MidiBuffer& midi, int numSamples)
{
    const auto on = getParam (clipOnRef) > 0.5f;

    if (! on)
    {
        if (clipWasOn)
        {
            for (const auto& active : clipActive)
                midi.addEvent (juce::MidiMessage::noteOff (1, active.note), 0);

            clipActive.clear();

            // Keys still held in Key transpose were taken out of the MIDI:
            // give them back to the synth so they sound again.
            for (auto note : clipHeld)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, clipHeldVelocity[(size_t) note]), 0);

            clipHeld.clearQuick();
            clipWasOn = clipRunning = false;
            clipLastIndex = clipLastMode = -1;
            clipPlayhead.store (-1.0f);
        }

        return;
    }

    clipWasOn = true;
    const auto& clips = clipState.acquireForAudio();
    const auto index = juce::jlimit (0, ClipState::numClips - 1, (int) getParam (clipIndexRef));
    const auto& clip = clips[(size_t) index];
    const auto hostMode = (int) getParam (clipModeRef) == 1;
    const auto playing = hostPlaying.load();
    const auto rate = (juce::jmax (20.0, currentBpm.load()) / 60.0) / currentSampleRate; // beats per sample
    const auto lengthBeats = (double) (clip.bars * ClipState::beatsPerBar);
    const auto lastSample = juce::jmax (0, numSamples - 1);

    // Where the notes go: straight into the buffer, or (Key transpose,
    // which takes the played keys out) into a copy that replaces it.
    auto* out = &midi;

    if (! hostMode)
    {
        clipScratch.clear();
        out = &clipScratch;
    }

    const auto releaseAll = [&out, this] (int position)
    {
        for (const auto& active : clipActive)
            out->addEvent (juce::MidiMessage::noteOff (1, active.note), position);

        clipActive.clear();
    };

    if (index != clipLastIndex || (int) hostMode != clipLastMode)
    {
        releaseAll (0);

        if (hostMode && clipLastMode == 0)
            for (auto note : clipHeld)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, clipHeldVelocity[(size_t) note]), 0);

        clipHeld.clearQuick();
        clipRunning = false;
        clipLastIndex = index;
        clipLastMode = (int) hostMode;
    }

    if (hostMode)
    {
        if (! playing)
        {
            releaseAll (0);
            clipRunning = clipUsedHost = false;
            clipPlayhead.store (-1.0f);
            return;
        }

        clipRunning = true;
    }

    // The clock: the host's beat while it plays (a jump, a loop or a start
    // drops the sounding notes), else a count from zero.
    if (playing)
    {
        const auto ppq = hostPpq.load();

        if (! clipUsedHost || std::abs (ppq - clipExpected) > 0.05)
            releaseAll (0);

        clipBase = ppq;
    }
    else if (clipUsedHost)
    {
        releaseAll (0);
        clipBase = 0.0;
    }

    clipUsedHost = playing;

    // Note events for the samples [from, to), on the beat clock.
    const auto generate = [&] (int from, int to)
    {
        if (to <= from || rate <= 0.0)
            return;

        const auto b0 = clipBase + (double) from * rate;
        const auto b1 = clipBase + (double) to * rate;
        const auto transpose = hostMode || clipHeld.isEmpty() ? 0 : clipHeld.getLast() - ClipState::rootNote;
        const auto samplePosition = [&] (double beat)
        {
            return juce::jlimit (from, to - 1, from + (int) std::ceil ((beat - b0) / rate - 1.0e-9));
        };

        for (auto loop = std::floor (b0 / lengthBeats); loop * lengthBeats < b1; loop += 1.0)
        {
            for (const auto& n : clip.notes)
            {
                const auto start = loop * lengthBeats + (double) n.start;

                if (start < b0 || start >= b1 || (double) n.start >= lengthBeats)
                    continue;

                const auto position = samplePosition (start);
                const auto pitch = juce::jlimit (0, 127, n.note + transpose);

                // The same pitch still sounding is released first.
                for (size_t i = 0; i < clipActive.size(); ++i)
                    if (clipActive[i].note == pitch)
                    {
                        out->addEvent (juce::MidiMessage::noteOff (1, pitch), position);
                        clipActive.erase (clipActive.begin() + (std::ptrdiff_t) i);
                        break;
                    }

                out->addEvent (juce::MidiMessage::noteOn (1, pitch, (juce::uint8) n.velocity), position);
                clipActive.push_back ({ pitch, loop * lengthBeats + juce::jmin (lengthBeats, (double) (n.start + n.length)) });
            }
        }

        for (size_t i = 0; i < clipActive.size();)
        {
            if (clipActive[i].endBeat < b1)
            {
                out->addEvent (juce::MidiMessage::noteOff (1, clipActive[i].note), samplePosition (clipActive[i].endBeat));
                clipActive.erase (clipActive.begin() + (std::ptrdiff_t) i);
            }
            else
            {
                ++i;
            }
        }
    };

    if (hostMode)
    {
        generate (0, numSamples);
    }
    else
    {
        // Walk the keys event by event so the clip starts and stops on their samples.
        auto position = 0;

        for (const auto metadata : midi)
        {
            const auto eventPosition = juce::jlimit (0, lastSample, metadata.samplePosition);

            if (clipRunning)
                generate (position, eventPosition);

            position = juce::jmax (position, eventPosition);
            const auto message = metadata.getMessage();
            auto taken = false;

            if (message.isNoteOn())
            {
                taken = true;
                const auto wasEmpty = clipHeld.isEmpty();
                clipHeld.removeAllInstancesOf (message.getNoteNumber());
                clipHeld.add (message.getNoteNumber());
                clipHeldVelocity[(size_t) message.getNoteNumber()] = message.getVelocity();

                if (wasEmpty)
                {
                    clipRunning = true;

                    if (! playing)
                        clipBase = -(double) position * rate; // beat 0 on this sample
                }
            }
            else if (message.isNoteOff() && clipHeld.contains (message.getNoteNumber()))
            {
                taken = true;
                clipHeld.removeAllInstancesOf (message.getNoteNumber());

                if (clipHeld.isEmpty())
                {
                    releaseAll (position);
                    clipRunning = false;
                }
            }
            else if (message.isAllNotesOff() || message.isAllSoundOff())
            {
                clipHeld.clearQuick();
                releaseAll (position);
                clipRunning = false;
            }

            if (! taken)
                clipScratch.addEvent (message, eventPosition);
        }

        if (clipRunning)
            generate (position, numSamples);

        midi.swapWith (clipScratch);
    }

    clipBase += (double) numSamples * rate;
    clipExpected = clipBase;
    clipPlayhead.store (clipRunning ? (float) std::fmod (std::fmod (clipBase, lengthBeats) + lengthBeats, lengthBeats) : -1.0f);
}
