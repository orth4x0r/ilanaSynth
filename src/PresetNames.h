#pragma once

#include <juce_core/juce_core.h>

// How preset names are shown (UI review 6). Saved names never change (the
// browser, favourites and host sessions find presets by them); only the
// text on screen does. DX7 voices are stored as the cartridge wrote them,
// "E.PIANO 1 (ROM1A)" for a factory voice and "01 E.PIANO 1" for an
// imported one, and shown as "E.Piano 1", the bank beside it.
namespace Presets
{
// The bank in a factory DX7 voice's name ("ROM1A"), or "" for any other
// preset.
inline juce::String dx7BankOf (const juce::String& name, const char* category)
{
    if (category == nullptr || juce::String (category) != "DX7" || ! name.trimEnd().endsWithChar (')'))
        return {};

    return name.fromLastOccurrenceOf ("(", false, false).upToFirstOccurrenceOf (")", false, false).trim();
}

// The cartridge's own name: without the " (BANK)" a factory voice carries,
// or the "NN " voice number an imported one starts with.
inline juce::String dx7VoiceName (const juce::String& name)
{
    auto text = name.trim();

    if (text.endsWithChar (')') && text.containsChar ('('))
        text = text.upToLastOccurrenceOf ("(", false, false).trim();

    if (text.length() > 3 && juce::CharacterFunctions::isDigit (text[0]) && juce::CharacterFunctions::isDigit (text[1])
        && text[2] == ' ')
        text = text.substring (3).trim();

    return text;
}

// A DX7 name as a title: the bank and number gone, runs of spaces made one,
// junk at the ends (\ ^ / + . - and spaces) trimmed, and ALL CAPS words in
// Title Case ("E.PIANO 1" -> "E.Piano 1", "SYN-LEAD 1" -> "Syn-Lead 1").
// Words without a vowel stay capitals ("BC", "TRW"), and a name the
// cartridge already wrote in mixed case keeps it.
inline juce::String dx7DisplayName (const juce::String& name)
{
    auto text = dx7VoiceName (name);

    while (text.contains ("  "))
        text = text.replace ("  ", " ");

    text = text.trimCharactersAtStart (" \\^/+.-_*=").trimCharactersAtEnd (" \\^/+.-_*=");

    if (text.isEmpty())
        return dx7VoiceName (name);

    if (text != text.toUpperCase())
        return text;

    juce::String out;
    auto i = 0;

    while (i < text.length())
    {
        if (! juce::CharacterFunctions::isLetter (text[i]))
        {
            out << juce::String::charToString (text[i++]);
            continue;
        }

        auto end = i;

        while (end < text.length() && juce::CharacterFunctions::isLetter (text[end]))
            ++end;

        const auto word = text.substring (i, end);
        const auto hasVowel = word.containsAnyOf ("AEIOUY");
        out << (hasVowel ? word.substring (0, 1) + word.substring (1).toLowerCase() : word);
        i = end;
    }

    return out;
}
} // namespace Presets
