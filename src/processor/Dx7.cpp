#include "ProcessorInternal.h"

#include "../Presets.h"

// DX7 voices: loading one as an ordinary patch (its operators on the
// Operator EG) and importing .syx banks as user presets (see
// dsp/Dx7Engine.h and Dx7Presets.h).

void IlanaSynthAudioProcessor::loadDx7Voice (const Dx7::Voice& voice, const juce::String& name)
{
    // Init underneath, then the voice's operators, matrix and macros.
    loadFactoryPreset (0);
    std::vector<std::pair<juce::String, float>> values;
    for (const auto& value : Presets::Dx7Import::values (voice))
        values.push_back ({ juce::String (value.id), value.value });
    moveLevelToTrim (values); // MASTER at 0 dB, as on the factory voices
    for (const auto& value : values)
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (value.first)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value.second));
    for (int macro = 0; macro < 4; ++macro)
        setMacroName (macro, Presets::Dx7Import::macroNamesFor (Presets::Dx7Import::soundCategory (Presets::dx7VoiceName (name), &voice))[(size_t) macro]);
    setCurrentPresetName (name);
    // Filed by sound (Keys, Bass...) like the factory voices; the DX7 tag
    // keeps it with them under the browser's DX7 chip.
    setPresetMeta (Presets::Dx7Import::soundCategory (Presets::dx7VoiceName (name), &voice), "DX7, FM");
}

namespace
{
// The voices in a .syx file: a 32-voice bulk dump (format 9) or a single
// voice (format 0, already in the 155-byte layout).
std::vector<std::pair<Dx7::Voice, juce::String>> readSyx (const juce::MemoryBlock& data)
{
    std::vector<std::pair<Dx7::Voice, juce::String>> voices;
    const auto* bytes = static_cast<const std::uint8_t*> (data.getData());
    const auto size = (int) data.getSize();

    for (int start = 0; start + 6 < size; ++start)
    {
        if (bytes[start] != 0xF0 || bytes[start + 1] != 0x43)
            continue;
        const auto format = bytes[start + 3];
        if (format == 9 && start + 6 + 4096 <= size)
        {
            for (int i = 0; i < 32; ++i)
            {
                const auto voice = Dx7::unpack (bytes + start + 6 + i * 128);
                voices.push_back ({ voice, juce::String (Dx7::name (voice)) });
            }
            start += 6 + 4096;
        }
        else if (format == 0 && start + 6 + 155 <= size)
        {
            Dx7::Voice voice {};
            std::memcpy (voice.data(), bytes + start + 6, 155);
            voice[155] = 0x3f;
            Dx7::clampRanges (voice);
            voices.push_back ({ voice, juce::String (Dx7::name (voice)) });
            start += 6 + 155;
        }
    }
    return voices;
}
}

int IlanaSynthAudioProcessor::importDx7File (const juce::File& file, juce::String& message)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data))
    {
        message = "Could not read " + file.getFileName() + ".";
        return 0;
    }
    const auto voices = readSyx (data);
    if (voices.empty())
    {
        message = file.getFileName() + " has no DX7 voices (a 32-voice bank or a single voice .syx).";
        return 0;
    }

    const auto folder = getUserPresetDirectory().getChildFile ("DX7").getChildFile (file.getFileNameWithoutExtension());
    folder.createDirectory();

    // Each voice goes through the processor (as a load would), is saved, and
    // the patch that was open comes back afterwards.
    const auto before = buildFullState();
    auto count = 0;
    for (size_t i = 0; i < voices.size(); ++i)
    {
        const auto& [voice, name] = voices[i];
        const auto label = juce::String (i + 1).paddedLeft ('0', 2) + " " + juce::File::createLegalFileName (name.isEmpty() ? "VOICE" : name);
        loadDx7Voice (voice, label);
        if (savePresetToFile (folder.getChildFile (label + ".ilanapreset")))
            ++count;
    }
    applyFullState (before);
    message = "Imported " + juce::String (count) + (count == 1 ? " voice to " : " voices to ") + folder.getFullPathName() + ".";
    return count;
}
