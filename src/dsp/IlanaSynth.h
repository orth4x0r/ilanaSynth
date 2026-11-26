#pragma once

#include "Voice.h"
#include "VoiceThreads.h"

// juce::Synthesiser plus the voice modes a lead/bass synth needs:
//   Poly    - normal polyphony, capped at a user voice count
//   Mono    - one voice, every new note retriggers the envelopes
//   Legato  - one voice, overlapping notes only change pitch
// Mono modes keep a stack of held keys so releasing the top note falls back
// to the one still held underneath it.
class IlanaSynth : public juce::Synthesiser
{
public:
    IlanaSynth()
    {
        heldNotes.ensureStorageAllocated (128);
    }

    enum class Mode
    {
        Poly = 0,
        Mono,
        Legato
    };

    void setVoiceMode (Mode newMode, int newPolyLimit, bool newGlideOnlyLegato)
    {
        const juce::ScopedLock sl (lock);

        if (newMode != mode)
        {
            // Mono <-> Legato keeps the keys still down (releasing the top
            // one falls back to the next); in or out of Poly starts afresh.
            if ((newMode == Mode::Poly) != (mode == Mode::Poly))
                heldNotes.clearQuick();
            pendingCount = 0;

            // Leaving poly: let the extra voices ring out rather than cut.
            if (newMode != Mode::Poly)
                for (int i = 1; i < voices.size(); ++i)
                    if (voices[i]->isVoiceActive())
                        voices[i]->stopNote (0.0f, true);
        }

        mode = newMode;
        polyLimit = juce::jlimit (1, juce::jmax (1, voices.size() - spareVoices), newPolyLimit);
        glideOnlyLegato = newGlideOnlyLegato;
    }

    // The last `count` voices are spares: the voice limit never counts them,
    // so a note that steals a voice can start on a spare while the stolen
    // one fades out (8 ms) instead of being cut dead (a click).
    void setSpareVoices (int count) noexcept { spareVoices = juce::jmax (0, count); }

    // The Scala tuning of this block (nullptr: 12-TET, every key plays).
    void setTuning (const Tuning* newTuning) noexcept { tuning = newTuning; }

    // A private SysEx (non-commercial ID 0x7D, then "IL") asking every held
    // voice to re-strike its Physical strings: Euclid's Exciter target.
    static constexpr juce::uint8 exciterMarker[3] { 0x7d, 0x49, 0x4c };

    void handleMidiEvent (const juce::MidiMessage& message) override
    {
        if (message.isSysEx() && message.getSysExDataSize() == 4)
        {
            const auto* data = message.getSysExData();

            if (data[0] == exciterMarker[0] && data[1] == exciterMarker[1] && data[2] == exciterMarker[2])
            {
                for (auto* voice : voices)
                    if (auto* ilanaVoice = dynamic_cast<Voice*> (voice))
                        ilanaVoice->reExcite ((float) data[3] / 127.0f);

                return;
            }
        }

        // All Sound Off cuts (JUCE releases it softly, as All Notes Off), and
        // with MPE either one on the master channel covers every member
        // channel's notes too.
        if (message.isAllSoundOff() || message.isAllNotesOff())
        {
            const auto channel = isMpe() && message.getChannel() == 1 ? 0 : message.getChannel();
            if (message.isAllSoundOff() || channel == 0)
            {
                allNotesOff (channel, ! message.isAllSoundOff());
                return;
            }
        }

        Synthesiser::handleMidiEvent (message);
    }

    // The voices read the last controllers, the master bend and the last
    // note's pitch from here when a note starts.
    void linkVoices()
    {
        for (auto* voice : voices)
            if (auto* ilanaVoice = dynamic_cast<Voice*> (voice))
                ilanaVoice->setSharedState (&shared);
    }

    void handleController (int midiChannel, int controllerNumber, int controllerValue) override
    {
        if (juce::isPositiveAndBelow (midiChannel, 17))
        {
            if (controllerNumber == 1)
                shared.modWheel[midiChannel] = (float) controllerValue / 127.0f;
            else if (controllerNumber == 11)
                shared.expression[midiChannel] = (float) controllerValue / 127.0f;
        }

        // MPE: the master channel's wheel, expression and pedals apply to the
        // notes on every member channel too (JUCE only tells voices playing
        // channel 1, and the voices read their own channel's values).
        if (midiChannel == 1 && isMpe())
        {
            for (int channel = 2; channel <= 16; ++channel)
            {
                if (controllerNumber == 1)
                    shared.modWheel[channel] = shared.modWheel[1];
                else if (controllerNumber == 11)
                    shared.expression[channel] = shared.expression[1];
            }

            for (int channel = 1; channel <= 16; ++channel)
                Synthesiser::handleController (channel, controllerNumber, controllerValue);
            return;
        }

        Synthesiser::handleController (midiChannel, controllerNumber, controllerValue);
    }

    void handleChannelPressure (int midiChannel, int channelPressureValue) override
    {
        if (juce::isPositiveAndBelow (midiChannel, 17))
            shared.pressure[midiChannel] = (float) channelPressureValue / 127.0f;

        Synthesiser::handleChannelPressure (midiChannel, channelPressureValue);
    }

    // The loudest pressure on any channel: the global Aftertouch source with
    // several MPE fingers down, instead of whichever channel spoke last.
    float getMaxChannelPressure (int midiChannel, float value) noexcept
    {
        if (juce::isPositiveAndBelow (midiChannel, 17))
            shared.pressure[midiChannel] = value;

        if (! isMpe())
            return value;

        auto most = 0.0f;
        for (auto pressure : shared.pressure)
            most = juce::jmax (most, pressure);
        return most;
    }

    bool isMpe() const
    {
        const auto* first = voices.isEmpty() ? nullptr : dynamic_cast<const Voice*> (voices.getFirst());
        return first != nullptr && first->isMpeNow(); // (not isMpe(): voice 0 may be idle since MPE changed)
    }

    void handlePitchWheel (int midiChannel, int wheelValue) override
    {
        // MPE: the master channel's bend moves every voice, whatever channel it plays on.
        if (midiChannel == 1 && isMpe())
        {
            shared.masterBendWheel = ((double) wheelValue - 8192.0) / 8192.0;

            for (auto* voice : voices)
                if (auto* ilanaVoice = dynamic_cast<Voice*> (voice))
                    ilanaVoice->setMasterBend (shared.masterBendWheel);
        }

        Synthesiser::handlePitchWheel (midiChannel, wheelValue);
    }

    void noteOn (int midiChannel, int midiNoteNumber, float velocity) override
    {
        if (! voicesLinked)
        {
            linkVoices();
            voicesLinked = true;
        }

        // A Scala keyboard mapping can leave keys unmapped ('x'): they are silent.
        if (tuning != nullptr && ! tuning->isMapped (midiNoteNumber))
            return;

        if (mode == Mode::Poly)
        {
            const juce::ScopedLock sl (lock);
            dropPending (midiChannel, midiNoteNumber); // the key again: this note replaces it
            noteOnVelocity = velocity;
            Synthesiser::noteOn (midiChannel, midiNoteNumber, velocity);
            return;
        }

        const juce::ScopedLock sl (lock);

        const auto wasHolding = ! heldNotes.isEmpty();
        heldNotes.removeFirstMatchingValue (midiNoteNumber);
        heldNotes.add (midiNoteNumber);
        lastVelocity = velocity;

        playMono (midiChannel, midiNoteNumber, velocity, wasHolding);
    }

    void noteOff (int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override
    {
        if (mode == Mode::Poly)
        {
            {
                // A note still waiting for its voice (below) plays its
                // release once it starts, as a note this short would.
                const juce::ScopedLock sl (lock);
                for (int i = 0; i < pendingCount; ++i)
                    if (pending[(size_t) i].channel == midiChannel && pending[(size_t) i].note == midiNoteNumber)
                        pending[(size_t) i].released = true;
            }
            Synthesiser::noteOff (midiChannel, midiNoteNumber, velocity, allowTailOff);

            // MPE: a member channel with no key down has no pressure left, so
            // it can't hold the global Aftertouch up.
            if (juce::isPositiveAndBelow (midiChannel, 17) && midiChannel != 1)
            {
                auto keyDown = false;
                for (auto* voice : voices)
                    keyDown = keyDown || (voice->isPlayingChannel (midiChannel) && voice->isKeyDown());

                if (isMpe() && ! keyDown)
                    shared.pressure[midiChannel] = 0.0f;
            }
            return;
        }

        const juce::ScopedLock sl (lock);

        heldNotes.removeFirstMatchingValue (midiNoteNumber);
        auto* voice = monoVoice();

        if (voice == nullptr || voice->getCurrentlyPlayingNote() != midiNoteNumber)
            return;

        if (! heldNotes.isEmpty())
        {
            // Fall back to the most recent key still held.
            playMono (midiChannel, heldNotes.getLast(), lastVelocity, true);
            return;
        }

        Synthesiser::noteOff (midiChannel, midiNoteNumber, velocity, allowTailOff);
    }

    void allNotesOff (int midiChannel, bool allowTailOff) override
    {
        {
            const juce::ScopedLock sl (lock);
            heldNotes.clearQuick();
            pendingCount = 0;
        }

        Synthesiser::allNotesOff (midiChannel, allowTailOff);
    }

    // MULTI-CORE: the sounding voices of a sub-block render on several
    // cores (VoiceThreads), the same sound as one after another. Off the
    // audio thread: numWorkers extra threads (0: one core), buffers for
    // sub-blocks of up to maxSamples (the threads' real-time budget is a
    // host block of blockSamples at hostRate).
    void prepareVoiceThreads (int numWorkers, int maxSamples, int blockSamples, double hostRate)
    {
        checkScratch.setSize (2, juce::jmax (1, maxSamples), false, true, true);
        checkNonFinite = false;
        voiceThreads.prepare (numWorkers, voices.size(), maxSamples,
                              [this] (int voice, juce::AudioBuffer<float>& buffer, int start, int length)
                              { voices.getUnchecked (voice)->renderNextBlock (buffer, start, length); },
                              blockSamples, hostRate);
    }

    // The host's audio workgroup for the voice threads (VoiceThreads::setWorkgroup).
    bool setVoiceThreadsWorkgroup (const juce::AudioWorkgroup& workgroup) noexcept { return voiceThreads.setWorkgroup (workgroup); }

    void setVoiceThreadsEnabled (bool shouldUse) noexcept { useVoiceThreads = shouldUse; }

    // Audio thread. The processor's last check found a sample that is not
    // finite: until it says the output is clean again, the voices render one
    // by one into a scratch buffer and are checked before they are added
    // (in voice order, each adding exactly what it would have added
    // straight to the output, so good voices sound bit for bit the same),
    // and a voice that went NaN / Inf is stopped and cleared. Without this
    // one broken voice kept the whole mix muted (its NaN summed into every
    // sample) and the effects cleared every block for as long as it rang.
    void setCheckVoicesForNonFinite (bool shouldCheck) noexcept { checkNonFinite = shouldCheck; }
    int getNonFiniteVoicesStopped() const noexcept { return nonFiniteVoicesStopped; }

    // SUSTAIN VOICES: at most this many voices ring on after their key is
    // up (released, or held by the pedal); past it the oldest fade out over
    // 40 ms. 0: no cap.
    void setSustainVoiceCap (int newCap) noexcept { sustainVoiceCap = newCap; }

protected:
    void renderVoices (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override
    {
        // Notes waiting for a stolen voice's fade (findFreeVoice): render up
        // to the first fade's end, start what is free, go on.
        while (pendingCount > 0 && numSamples > 0)
        {
            startPendingNotes();
            if (pendingCount == 0)
                break;
            auto until = numSamples;
            for (int i = 0; i < pendingCount; ++i)
                until = juce::jmin (until, juce::jmax (1, pending[(size_t) i].voice->getFadeRemaining()));
            renderVoicesNow (buffer, startSample, until);
            startSample += until;
            numSamples -= until;
        }
        if (numSamples > 0)
            renderVoicesNow (buffer, startSample, numSamples);
    }

    void renderVoicesNow (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (sustainVoiceCap > 0)
            capSustainedVoices();
        updateBowedNoiseShare (numSamples);

        if (checkNonFinite)
        {
            renderVoicesChecked (buffer, startSample, numSamples);
            return;
        }

        int sounding[64] {};
        auto numSounding = 0;
        if (useVoiceThreads)
            for (int i = 0; i < voices.size() && numSounding < 64; ++i)
                if (voices.getUnchecked (i)->isVoiceActive())
                    sounding[numSounding++] = i;

        if (! useVoiceThreads || ! voiceThreads.canRender (buffer, startSample, numSamples, numSounding))
        {
            Synthesiser::renderVoices (buffer, startSample, numSamples);
            return;
        }

        // Idle voices return at once (their own bookkeeping only), as before.
        for (int i = 0; i < voices.size(); ++i)
            if (! voices.getUnchecked (i)->isVoiceActive())
                voices.getUnchecked (i)->renderNextBlock (buffer, startSample, numSamples);

        voiceThreads.renderAndSum (sounding, numSounding, buffer, startSample, numSamples);
    }

    void renderVoicesChecked (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        // Same sample positions as the output (the voices read the shared
        // LFO buffers at startSample), as VoiceThreads does.
        const auto channels = juce::jmin (2, buffer.getNumChannels());
        if (channels <= 0 || checkScratch.getNumChannels() < 2 || startSample + numSamples > checkScratch.getNumSamples())
        {
            Synthesiser::renderVoices (buffer, startSample, numSamples);
            return;
        }
        float* scratch[2] { checkScratch.getWritePointer (0), checkScratch.getWritePointer (1) };
        juce::AudioBuffer<float> view (scratch, channels, checkScratch.getNumSamples());

        for (int i = 0; i < voices.size(); ++i)
        {
            auto* voice = voices.getUnchecked (i);
            if (! voice->isVoiceActive())
            {
                voice->renderNextBlock (buffer, startSample, numSamples); // idle: its own bookkeeping only
                continue;
            }
            for (int c = 0; c < channels; ++c)
                juce::FloatVectorOperations::clear (scratch[c] + startSample, numSamples);
            voice->renderNextBlock (view, startSample, numSamples);

            auto finite = true;
            for (int c = 0; c < channels && finite; ++c)
                for (int k = startSample; k < startSample + numSamples && finite; ++k)
                    finite = std::isfinite (scratch[c][k]);

            if (! finite)
            {
                if (auto* own = dynamic_cast<Voice*> (voice))
                    own->silenceAfterNonFinite();
                ++nonFiniteVoicesStopped;
                continue;
            }
            for (int c = 0; c < channels; ++c)
                juce::FloatVectorOperations::add (buffer.getWritePointer (c, startSample), scratch[c] + startSample, numSamples);
        }
    }

    juce::SynthesiserVoice* findFreeVoice (juce::SynthesiserSound* sound, int midiChannel,
                                           int midiNoteNumber, bool stealIfNoneAvailable) const override
    {
        auto active = 0;

        for (auto* voice : voices)
            if (voice->isVoiceActive() && ! isFading (voice))
                ++active;

        // (Notes waiting for a voice count as playing.)
        if (active + pendingCount >= polyLimit)
        {
            auto* victim = stealActiveVoice();
            auto* stolen = dynamic_cast<Voice*> (victim);

            // A free voice plays the new note while the stolen one fades out.
            if (spareVoices > 0 && stolen != nullptr && stolen->hasSounded())
            {
                const auto fade = (int) (0.008 * getSampleRate());
                if (auto* spare = Synthesiser::findFreeVoice (sound, midiChannel, midiNoteNumber, false))
                {
                    stolen->startFadeOut (fade);
                    return spare;
                }

                // No spare left (a big chord over full voices): the note
                // waits the 8 ms for the stolen voice to fade, rather than
                // cutting it dead (a click). A voice already waited for is
                // only taken when every voice is fading.
                if (! dropPending (stolen) && pendingCount < (int) pending.size())
                {
                    stolen->startFadeOut (fade);
                    pending[(size_t) pendingCount++] = { stolen, sound, midiChannel, midiNoteNumber, noteOnVelocity, false };
                    return nullptr;
                }
            }

            return victim;
        }

        return Synthesiser::findFreeVoice (sound, midiChannel, midiNoteNumber, stealIfNoneAvailable);
    }

    // JUCE's default stealer also considers idle voices, which would let a
    // voice-limited patch exceed its limit. Only ever steal a sounding voice:
    // the oldest released one first, else the oldest held one.
    juce::SynthesiserVoice* stealActiveVoice() const
    {
        juce::SynthesiserVoice* oldestReleased = nullptr;
        juce::SynthesiserVoice* oldest = nullptr;

        for (auto* voice : voices)
        {
            if (! voice->isVoiceActive() || isFading (voice))
                continue;

            if (voice->isPlayingButReleased()
                && (oldestReleased == nullptr || voice->wasStartedBefore (*oldestReleased)))
                oldestReleased = voice;

            if (oldest == nullptr || voice->wasStartedBefore (*oldest))
                oldest = voice;
        }

        if (oldest == nullptr) // every sounding voice is already fading out: take the oldest of those
            for (auto* voice : voices)
                if (voice->isVoiceActive() && (oldest == nullptr || voice->wasStartedBefore (*oldest)))
                    oldest = voice;

        return oldestReleased != nullptr ? oldestReleased : oldest;
    }

    static bool isFading (const juce::SynthesiserVoice* voice)
    {
        const auto* ilanaVoice = dynamic_cast<const Voice*> (voice);
        return ilanaVoice != nullptr && ilanaVoice->isFadingOut();
    }

private:
    // (Modal Bowed, chords) Each bowed note makes its own noise (the bow
    // hair's, the hall's floor, the residual layers), so N notes stacked N
    // noise beds, sqrt (N) louder than one, while the tone adds as notes do.
    // Every bowed voice plays 1 / sqrt (n) of its noise, n the sounding
    // notes weighed by their bow envelope and amp level against the loudest
    // (sum (w / w max)^2: a releasing tail or a quiet note counts less), so
    // the noises' power sums to about one note's. Smoothed over
    // bowedNoiseSeconds, set before the sub-block renders (here on the
    // audio thread, so the MULTI-CORE workers only read it). One note (or
    // none) is exactly 1: a single note plays bit for bit as before.
    static constexpr double bowedNoiseSeconds = 0.08;
    float bowedNoiseShare = 1.0f, bowedNoisePushed = 1.0f;

    void updateBowedNoiseShare (int numSamples)
    {
        auto loudest = 0.0f, power = 0.0f;
        auto counted = 0;
        for (auto* voice : voices)
        {
            const auto w = static_cast<Voice*> (voice)->bowedNoiseWeight();
            if (w > 0.0f)
            {
                ++counted;
                loudest = juce::jmax (loudest, w);
                power += w * w;
            }
        }
        const auto target = counted <= 1 ? 1.0f : juce::jlimit (0.0f, 1.0f, loudest / std::sqrt (power));
        if (bowedNoiseShare != target)
        {
            const auto keep = (float) std::exp (-(double) numSamples / (bowedNoiseSeconds * juce::jmax (1.0, getSampleRate())));
            bowedNoiseShare = target + (bowedNoiseShare - target) * keep;
            if (std::abs (bowedNoiseShare - target) < 1.0e-5f)
                bowedNoiseShare = target;
        }
        if (bowedNoiseShare != bowedNoisePushed)
        {
            bowedNoisePushed = bowedNoiseShare;
            for (auto* voice : voices)
                static_cast<Voice*> (voice)->setBowedNoiseShare (bowedNoiseShare);
        }
    }

    void capSustainedVoices()
    {
        while (true)
        {
            auto count = 0;
            Voice* oldest = nullptr;
            for (auto* voice : voices)
            {
                auto* v = static_cast<Voice*> (voice);
                if (! v->isVoiceActive() || v->isKeyDown() || v->isFadingOut())
                    continue;
                ++count;
                if (oldest == nullptr || v->wasStartedBefore (*oldest))
                    oldest = v;
            }
            if (count <= sustainVoiceCap || oldest == nullptr)
                return;
            oldest->startFadeOut ((int) (0.04 * getSampleRate()));
        }
    }

    // A note waiting for the voice it stole to fade out (findFreeVoice).
    struct PendingNote
    {
        Voice* voice = nullptr;
        juce::SynthesiserSound* sound = nullptr;
        int channel = 0, note = 0;
        float velocity = 0.0f;
        bool released = false;
    };
    mutable std::array<PendingNote, 64> pending {};
    mutable int pendingCount = 0;
    float noteOnVelocity = 0.8f;

    // Forgets the waiting notes on that voice, or on that key; whether any were.
    bool dropPending (const Voice* voice) const noexcept
    {
        return dropPendingIf ([voice] (const PendingNote& p) { return p.voice == voice; });
    }
    bool dropPending (int channel, int note) const noexcept
    {
        return dropPendingIf ([channel, note] (const PendingNote& p) { return p.channel == channel && p.note == note; });
    }
    template <typename Match>
    bool dropPendingIf (Match match) const noexcept
    {
        auto kept = 0;
        for (int i = 0; i < pendingCount; ++i)
            if (! match (pending[(size_t) i]))
                pending[(size_t) kept++] = pending[(size_t) i];
        const auto dropped = kept != pendingCount;
        pendingCount = kept;
        return dropped;
    }

    // Starts the waiting notes whose voice has finished its fade.
    void startPendingNotes()
    {
        auto kept = 0;
        for (int i = 0; i < pendingCount; ++i)
        {
            const auto p = pending[(size_t) i];
            if (p.voice->isVoiceActive())
            {
                if (p.voice->isFadingOut()) // (else another note took the voice: this one is dropped)
                    pending[(size_t) kept++] = p;
                continue;
            }
            startVoice (p.voice, p.sound, p.channel, p.note, p.velocity);
            if (p.released)
            {
                p.voice->setKeyDown (false);
                if (! p.voice->isSustainPedalDown())
                    stopVoice (p.voice, 0.0f, true);
            }
        }
        pendingCount = kept;
    }

    VoiceSharedState shared;
    bool voicesLinked = false;
    VoiceThreads voiceThreads;
    bool useVoiceThreads = false;
    juce::AudioBuffer<float> checkScratch;
    bool checkNonFinite = false;
    int nonFiniteVoicesStopped = 0;
    int sustainVoiceCap = 0;
    int spareVoices = 0;

    Voice* monoVoice() const
    {
        return voices.isEmpty() ? nullptr : dynamic_cast<Voice*> (voices.getFirst());
    }

    void playMono (int midiChannel, int midiNoteNumber, float velocity, bool overlapping)
    {
        auto* voice = monoVoice();

        if (voice == nullptr || sounds.isEmpty())
            return;

        const auto sounding = voice->isVoiceActive();
        const auto legato = mode == Mode::Legato && overlapping && sounding && voice->isKeyDown();
        const auto glide = ! glideOnlyLegato || (overlapping && sounding);

        voice->prepareMonoNote (legato, sounding, glide);
        startVoice (voice, sounds.getFirst().get(), midiChannel, midiNoteNumber, velocity);
    }

    Mode mode = Mode::Poly;
    int polyLimit = 16;
    bool glideOnlyLegato = false;
    const Tuning* tuning = nullptr;
    float lastVelocity = 0.8f;
    juce::Array<int> heldNotes;
};
