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

// The cartridges' 10-character cuts, spelled out (UI review 7: "Harpsich 1",
// "Jazz Guit1"): whole words, as the ALL CAPS name writes them.
inline juce::String dx7ExpandedWord (const juce::String& word)
{
    static const std::pair<const char*, const char*> words[] {
        { "HARPSICH", "Harpsichord" }, { "HARPSI", "Harpsichord" }, { "GUIT", "Guitar" }, { "GUITR", "Guitar" },
        { "GTR", "Guitar" }, { "STRG", "Strings" }, { "STRGS", "Strings" }, { "STGS", "Strings" }, { "STG", "Strings" },
        { "PNO", "Piano" }, { "BRS", "Brass" }, { "CLV", "Clav" }, { "ORG", "Organ" }, { "GLOKENSPL", "Glockenspiel" },
        { "HRMNCA", "Harmonica" }, { "TBONE", "Trombone" }, { "SECN", "Section" }, { "QRT", "Quartet" },
        { "ENS", "Ensemble" }, { "CRSNDO", "Crescendo" }, { "PIZZT", "Pizzicato" }, { "MARIM", "Marimba" },
        { "SWP", "Sweep" }, { "WHISL", "Whistle" }, { "GDN", "Garden" }, { "PRC", "Perc" }, { "CLAS", "Classical" },
        { "SYNBRASS", "Synth Brass" }, { "SYNTHBRASS", "Synth Brass" }, { "SYNORGAN", "Synth Organ" },
        { "BRASSHORNS", "Brass Horns" }, { "THS", "ths" }, { "SPANISHGTR", "Spanish Guitar" }, { "HEAVYMETAL", "Heavy Metal" }
    };

    for (const auto& [cut, full] : words)
        if (word == cut)
            return full;

    return {};
}

// A DX7 name as a title: the bank and number gone, runs of spaces made one,
// junk at the ends (\ ^ / + . - and spaces) trimmed, and ALL CAPS words in
// Title Case ("E.PIANO 1" -> "E.Piano 1", "SYN-LEAD 1" -> "Syn-Lead 1"),
// cut words spelled out ("HARPSICH 1" -> "Harpsichord 1", "JAZZ GUIT1" ->
// "Jazz Guitar 1"). Words without a vowel stay capitals ("BC", "TRW"), and a
// name the cartridge already wrote in mixed case keeps it.
inline juce::String dx7DisplayName (const juce::String& name)
{
    auto text = dx7VoiceName (name);

    text = text.replace ("E.P-", "E.PIANO-").replace ("TUB BELLS", "TUBULAR BELLS").replace ("CLAS.GUIT", "CLAS GUIT");

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
        const auto expanded = dx7ExpandedWord (word);
        const auto hasVowel = word.containsAnyOf ("AEIOUY");
        out << (expanded.isNotEmpty() ? expanded : (hasVowel ? word.substring (0, 1) + word.substring (1).toLowerCase() : word));

        // "GUIT1": a number run into a word gets its space back.
        if (end < text.length() && juce::CharacterFunctions::isDigit (text[end]) && word.length() >= 3)
            out << " ";

        i = end;
    }

    return out;
}
} // namespace Presets
