#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <vector>

// Scala microtuning: a scale (.scl) and an optional keyboard mapping (.kbm)
// turned into a frequency for each MIDI note. The file formats are the ones
// documented at https://www.huygens-fokker.org/scala/scl_format.html and
// .../help.htm#mappings. The table is built once when a file is loaded, so a
// voice pays one array read per note.
class Tuning
{
public:
    // A .scl file: the pitches of degrees 1..N in cents above 1/1. The last
    // one is the period (the "octave") the scale repeats at.
    struct Scale
    {
        juce::String description;
        std::vector<double> cents;
    };

    // A .kbm file. A mapping entry of -1 is an unmapped key ('x'). A map
    // size of 0 is a linear mapping (one key per scale degree).
    struct KeyboardMap
    {
        int mapSize = 0;
        int firstNote = 0;
        int lastNote = 127;
        int middleNote = 60;
        int referenceNote = 60;
        double referenceFrequency = 261.6255653005986; // middle C in 12-TET at A = 440 Hz
        int octaveDegree = 0;
        std::vector<int> mapping;
    };

    // 12-TET at A = 440 Hz.
    Tuning()
    {
        for (int note = 0; note < 128; ++note)
        {
            frequencies[(size_t) note] = 440.0 * std::exp2 ((note - 69) / 12.0);
            mapped[(size_t) note] = true;
        }
    }

    double getFrequency (int midiNote) const noexcept { return frequencies[(size_t) juce::jlimit (0, 127, midiNote)]; }
    bool isMapped (int midiNote) const noexcept { return midiNote >= 0 && midiNote < 128 && mapped[(size_t) midiNote]; }

    const juce::String& getDescription() const noexcept { return description; }
    const juce::String& getScaleText() const noexcept { return scaleText; }
    const juce::String& getMappingText() const noexcept { return mappingText; }

    //==============================================================================
    static bool parseScale (const juce::String& text, Scale& out, juce::String& error)
    {
        const auto lines = contentLines (text);
        out = {};

        if (lines.isEmpty())
            return fail (error, "the file is empty");

        out.description = lines[0].trim();

        if (lines.size() < 2)
            return fail (error, "no note count after the description");

        const auto countToken = firstToken (lines[1]);
        if (! isUnsigned (countToken))
            return fail (error, "the note count '" + countToken + "' is not a whole number");

        const auto count = countToken.getIntValue();
        if (count < 1 || count > 1024)
            return fail (error, "the note count must be 1 to 1024");

        if (lines.size() - 2 < count)
            return fail (error, "the file lists " + juce::String (lines.size() - 2) + " of its " + juce::String (count) + " pitches");

        for (int i = 0; i < count; ++i)
        {
            const auto token = firstToken (lines[2 + i]);
            double cents = 0.0;

            if (! parsePitch (token, cents))
                return fail (error, "pitch " + juce::String (i + 1) + " ('" + token + "') is neither cents nor a ratio");

            out.cents.push_back (cents);
        }

        if (out.cents.back() <= 0.0)
            return fail (error, "the last pitch (the period) must be above 1/1");

        return true;
    }

    static bool parseKeyboardMap (const juce::String& text, KeyboardMap& out, juce::String& error)
    {
        const auto lines = contentLines (text);
        out = {};

        if (lines.size() < 7)
            return fail (error, "a keyboard mapping needs seven header lines");

        int header[5] {};
        for (int i = 0; i < 5; ++i)
        {
            const auto token = firstToken (lines[i]);
            if (! isUnsigned (token))
                return fail (error, "line " + juce::String (i + 1) + " ('" + token + "') is not a whole number");
            header[i] = token.getIntValue();
        }

        out.mapSize = header[0];
        out.firstNote = header[1];
        out.lastNote = header[2];
        out.middleNote = header[3];
        out.referenceNote = header[4];

        if (out.mapSize > 1024)
            return fail (error, "the map size must be 0 to 1024");
        for (const auto note : { out.firstNote, out.lastNote, out.middleNote, out.referenceNote })
            if (note > 127)
                return fail (error, "MIDI notes must be 0 to 127");
        if (out.firstNote > out.lastNote)
            return fail (error, "the first note is above the last");

        const auto frequencyToken = firstToken (lines[5]);
        if (! isDecimal (frequencyToken) || frequencyToken.getDoubleValue() <= 0.0)
            return fail (error, "the reference frequency '" + frequencyToken + "' is not a positive number");
        out.referenceFrequency = frequencyToken.getDoubleValue();

        const auto octaveToken = firstToken (lines[6]);
        if (! isUnsigned (octaveToken))
            return fail (error, "the octave degree '" + octaveToken + "' is not a whole number");
        out.octaveDegree = octaveToken.getIntValue();

        // Entries left out at the end are unmapped.
        out.mapping.assign ((size_t) out.mapSize, -1);
        for (int i = 0; i < out.mapSize && 7 + i < lines.size(); ++i)
        {
            const auto token = firstToken (lines[7 + i]);
            if (token.equalsIgnoreCase ("x"))
                continue;
            if (! isUnsigned (token))
                return fail (error, "mapping entry " + juce::String (i + 1) + " ('" + token + "') is neither a degree nor x");
            out.mapping[(size_t) i] = token.getIntValue();
        }

        return true;
    }

    // Builds a tuning from .scl text and optional .kbm text (empty: middle C
    // stays at 261.63 Hz and each key plays the next scale degree).
    static bool build (const juce::String& sclText, const juce::String& kbmText, Tuning& out, juce::String& error)
    {
        Scale scale;
        if (! parseScale (sclText, scale, error))
            return false;

        KeyboardMap map;
        if (kbmText.trim().isNotEmpty() && ! parseKeyboardMap (kbmText, map, error))
            return false;

        const auto count = (int) scale.cents.size();
        const auto period = scale.cents.back();
        const auto octaveDegree = map.mapSize > 0 && map.octaveDegree > 0 ? map.octaveDegree : count;

        // Cents above the middle note, or false for an unmapped key.
        const auto centsOf = [&] (int note, double& cents)
        {
            const auto offset = note - map.middleNote;
            int degree = offset;

            if (map.mapSize > 0)
            {
                const auto repeat = floorDiv (offset, map.mapSize);
                const auto entry = map.mapping[(size_t) (offset - repeat * map.mapSize)];
                if (entry < 0)
                    return false;
                degree = repeat * octaveDegree + entry;
            }

            const auto octave = floorDiv (degree, count);
            const auto step = degree - octave * count;
            cents = octave * period + (step == 0 ? 0.0 : scale.cents[(size_t) (step - 1)]);
            return true;
        };

        double referenceCents = 0.0;
        if (! centsOf (map.referenceNote, referenceCents))
            return fail (error, "the reference note is unmapped");

        Tuning result;
        for (int note = 0; note < 128; ++note)
        {
            double cents = 0.0;
            const auto inRange = note >= map.firstNote && note <= map.lastNote;
            result.mapped[(size_t) note] = inRange && centsOf (note, cents);
            result.frequencies[(size_t) note] = result.mapped[(size_t) note]
                ? juce::jlimit (1.0, 30000.0, map.referenceFrequency * std::exp2 ((cents - referenceCents) / 1200.0))
                : 0.0;
        }

        result.description = scale.description.isNotEmpty() ? scale.description : juce::String (count) + "-note scale";
        result.scaleText = sclText;
        result.mappingText = kbmText.trim().isNotEmpty() ? kbmText : juce::String();
        out = std::move (result);
        return true;
    }

private:
    std::array<double, 128> frequencies {};
    std::array<bool, 128> mapped {};
    juce::String description { "12-TET" }, scaleText, mappingText;

    static bool fail (juce::String& error, const juce::String& message)
    {
        error = message;
        return false;
    }

    static int floorDiv (int a, int b) noexcept { return a >= 0 ? a / b : -((-a + b - 1) / b); }

    // The lines that are not comments ('!' in the first column).
    static juce::StringArray contentLines (const juce::String& text)
    {
        juce::StringArray lines, result;
        lines.addLines (text);
        for (const auto& line : lines)
            if (! line.startsWithChar ('!'))
                result.add (line);
        return result;
    }

    static juce::String firstToken (const juce::String& line)
    {
        return line.trim().upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("\t", false, false).trim();
    }

    static bool isUnsigned (const juce::String& token)
    {
        return token.isNotEmpty() && token.containsOnly ("0123456789");
    }

    static bool isDecimal (const juce::String& token)
    {
        auto body = token.startsWithChar ('-') || token.startsWithChar ('+') ? token.substring (1) : token;
        return body.isNotEmpty() && body.containsOnly ("0123456789.") && body.indexOfChar ('.') == body.lastIndexOfChar ('.')
               && body != ".";
    }

    // Cents contain a '.'; anything else is a ratio a/b or a whole number.
    static bool parsePitch (const juce::String& token, double& cents)
    {
        if (token.containsChar ('.'))
        {
            if (! isDecimal (token))
                return false;
            cents = token.getDoubleValue();
            return std::isfinite (cents);
        }

        const auto numerator = token.upToFirstOccurrenceOf ("/", false, false);
        const auto denominator = token.containsChar ('/') ? token.fromFirstOccurrenceOf ("/", false, false) : juce::String ("1");
        if (! isUnsigned (numerator) || ! isUnsigned (denominator))
            return false;

        const auto a = numerator.getDoubleValue(), b = denominator.getDoubleValue();
        if (a <= 0.0 || b <= 0.0)
            return false;

        cents = 1200.0 * std::log2 (a / b);
        return std::isfinite (cents);
    }
};
