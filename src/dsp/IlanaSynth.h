#pragma once

#include "Voice.h"

// juce::Synthesiser plus the voice modes a lead/bass synth needs:
//   Poly    - normal polyphony, capped at a user voice count
//   Mono    - one voice, every new note retriggers the envelopes
//   Legato  - one voice, overlapping notes only change pitch
// Mono modes keep a stack of held keys so releasing the top note falls back
// to the one still held underneath it.
class IlanaSynth : public juce::Synthesiser
{
public:
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
            heldNotes.clearQuick();

            // Leaving poly: let the extra voices ring out rather than cut.
            if (newMode != Mode::Poly)
                for (int i = 1; i < voices.size(); ++i)
                    if (voices[i]->isVoiceActive())
                        voices[i]->stopNote (0.0f, true);
        }

        mode = newMode;
        polyLimit = juce::jlimit (1, voices.size() > 0 ? voices.size() : 1, newPolyLimit);
        glideOnlyLegato = newGlideOnlyLegato;
    }

    Mode getVoiceMode() const { return mode; }

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

        Synthesiser::handleMidiEvent (message);
    }

    void noteOn (int midiChannel, int midiNoteNumber, float velocity) override
    {
        // A Scala keyboard mapping can leave keys unmapped ('x'): they are silent.
        if (tuning != nullptr && ! tuning->isMapped (midiNoteNumber))
            return;

        if (mode == Mode::Poly)
        {
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
            Synthesiser::noteOff (midiChannel, midiNoteNumber, velocity, allowTailOff);
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
        }

        Synthesiser::allNotesOff (midiChannel, allowTailOff);
    }

protected:
    juce::SynthesiserVoice* findFreeVoice (juce::SynthesiserSound* sound, int midiChannel,
                                           int midiNoteNumber, bool stealIfNoneAvailable) const override
    {
        auto active = 0;

        for (auto* voice : voices)
            if (voice->isVoiceActive())
                ++active;

        if (active >= polyLimit)
            return stealActiveVoice();

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
            if (! voice->isVoiceActive())
                continue;

            if (voice->isPlayingButReleased()
                && (oldestReleased == nullptr || voice->wasStartedBefore (*oldestReleased)))
                oldestReleased = voice;

            if (oldest == nullptr || voice->wasStartedBefore (*oldest))
                oldest = voice;
        }

        return oldestReleased != nullptr ? oldestReleased : oldest;
    }

private:
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
