// Scala microtuning (.scl scales, .kbm keyboard mappings). Included by
// TableTest.cpp; ILANA_TUNING_TEST=1 runs these alone.

#include "dsp/Tuning.h"

namespace TuningTests
{
constexpr double sr = 48000.0;

double centsBetween (double a, double b) { return 1200.0 * std::log2 (a / b); }

juce::String edo (int steps, const juce::String& description)
{
    juce::String text ("! generated\n!\n" + description + "\n " + juce::String (steps) + "\n!\n");
    for (int k = 1; k <= steps; ++k)
        text << " " << juce::String (1200.0 * k / steps, 5) << "\n";
    return text;
}

const char* const justMajor =
    "! just.scl\n"
    "!\n"
    "Ptolemy's just major\n"
    " 7\n"
    "!\n"
    " 9/8\n"
    " 5/4   major third\n"
    " 4/3\n"
    " 3/2\n"
    " 5/3\n"
    " 15/8\n"
    " 2\n";

// A plain saw through an open filter, as the M8.6 tests use.
void plainPatch (IlanaSynthAudioProcessor& processor)
{
    setParam (processor, "osc2_on", 0.0f);
    setParam (processor, "sub_on", 0.0f);
    setParam (processor, "f1_cutoff", 5000.0f);
    setParam (processor, "amp_sustain", 1.0f);
    setParam (processor, "amp_release", 0.3f);
}

std::vector<float> play (IlanaSynthAudioProcessor& processor, int note, double seconds = 1.0)
{
    processor.prepareToPlay (sr, 256);
    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 256);
    for (int block = 0; block < (int) (seconds * sr / 256); ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        for (int i = 0; i < 256; ++i)
            out.push_back (buffer.getSample (0, i));
    }
    return out;
}

double pitchOf (const std::vector<float>& x)
{
    return fundamentalOf (std::vector<float> (x.begin() + (long) (0.2 * sr), x.end()), sr);
}

void runParserTests()
{
    // 12-TET from a file equals the default tuning.
    {
        Tuning tuning;
        juce::String error;
        const auto ok = Tuning::build (edo (12, "12-TET"), {}, tuning, error);
        auto worst = 0.0;
        for (int note = 0; note < 128; ++note)
            worst = juce::jmax (worst, std::abs (centsBetween (tuning.getFrequency (note), juce::MidiMessage::getMidiNoteInHertz (note))));
        check (ok && worst < 0.01 && tuning.getDescription() == "12-TET",
               "tuning: a 12-TET .scl matches the default tuning (worst " + juce::String (worst, 5) + " cents) " + error);

        const Tuning standard;
        auto worstDefault = 0.0;
        for (int note = 0; note < 128; ++note)
            worstDefault = juce::jmax (worstDefault, std::abs (centsBetween (standard.getFrequency (note), juce::MidiMessage::getMidiNoteInHertz (note))));
        check (worstDefault < 1.0e-6 && standard.isMapped (0) && standard.isMapped (127), "tuning: the default is 12-TET at 440 Hz");
    }

    // Just intonation as ratios (with a label after one, and a bare "2").
    {
        Tuning::Scale scale;
        juce::String error;
        const auto parsed = Tuning::parseScale (justMajor, scale, error);
        check (parsed && scale.cents.size() == 7 && std::abs (scale.cents[1] - 386.3137) < 0.001
                   && std::abs (scale.cents[6] - 1200.0) < 1.0e-9 && scale.description == "Ptolemy's just major",
               "tuning: a just scale parses its ratios " + error);

        Tuning tuning;
        Tuning::build (justMajor, {}, tuning, error);
        const auto middleC = 261.6255653005986;
        check (std::abs (tuning.getFrequency (64) - middleC * 1.5) < 1.0e-6, "tuning: the just scale's fifth is 3/2 above middle C ("
                   + juce::String (tuning.getFrequency (64), 4) + " Hz)");
        check (std::abs (tuning.getFrequency (59) - middleC * 15.0 / 16.0) < 1.0e-6
                   && std::abs (tuning.getFrequency (67) - middleC * 2.0) < 1.0e-6,
               "tuning: the just scale repeats at its period, below and above");
    }

    // 19-EDO.
    {
        Tuning tuning;
        juce::String error;
        const auto ok = Tuning::build (edo (19, "19-EDO"), {}, tuning, error);
        const auto expected = 261.6255653005986 * std::exp2 (9.0 / 19.0);
        check (ok && std::abs (centsBetween (tuning.getFrequency (69), expected)) < 0.01
                   && std::abs (centsBetween (tuning.getFrequency (79), 261.6255653005986 * 2.0)) < 0.01,
               "tuning: 19-EDO puts note 69 at " + juce::String (tuning.getFrequency (69), 3) + " Hz and note 79 an octave above C");
    }

    // A keyboard mapping: A = 432 Hz, C# unmapped, keys 21-108 only.
    {
        const char* const kbm =
            "! 432.kbm\n"
            "12\n"
            "21\n"
            "108\n"
            "60\n"
            "69\n"
            "432.0\n"
            "12\n"
            "! mapping\n"
            "0\nx\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n";

        Tuning::KeyboardMap map;
        juce::String error;
        const auto parsed = Tuning::parseKeyboardMap (kbm, map, error);
        check (parsed && map.mapSize == 12 && map.referenceNote == 69 && map.referenceFrequency == 432.0
                   && map.mapping[1] == -1 && map.mapping[11] == 11,
               "tuning: a .kbm parses " + error);

        Tuning tuning;
        const auto ok = Tuning::build (edo (12, "12-TET"), kbm, tuning, error);
        check (ok && std::abs (tuning.getFrequency (69) - 432.0) < 1.0e-9 && std::abs (tuning.getFrequency (57) - 216.0) < 1.0e-6
                   && std::abs (centsBetween (tuning.getFrequency (72), 432.0) - 300.0) < 1.0e-6,
               "tuning: the .kbm's reference frequency sets A to 432 Hz");
        check (! tuning.isMapped (61) && ! tuning.isMapped (73) && tuning.isMapped (62)
                   && ! tuning.isMapped (20) && ! tuning.isMapped (109) && tuning.isMapped (21),
               "tuning: 'x' keys and keys outside the .kbm's range are unmapped");

        // Two keys per degree: a mapping can repeat a degree.
        const char* const doubled = "2\n0\n127\n60\n60\n261.6255653\n1\n0\n0\n";
        Tuning slow;
        const auto doubledOk = Tuning::build (edo (12, "12-TET"), doubled, slow, error);
        check (doubledOk && std::abs (slow.getFrequency (61) - slow.getFrequency (60)) < 1.0e-9
                   && std::abs (centsBetween (slow.getFrequency (62), slow.getFrequency (60)) - 100.0) < 1.0e-6,
               "tuning: a 2-key map with octave degree 1 plays each degree twice " + error);
    }

    // Malformed files are refused with a reason.
    {
        const char* const badScales[] {
            "",                                   // empty
            "desc\n",                             // no count
            "desc\nseven\n1/1\n",                 // count not a number
            "desc\n3\n100.0\n200.0\n",            // too few pitches
            "desc\n2\nabc\n1200.0\n",             // not a pitch
            "desc\n2\n-3/2\n2/1\n",               // negative ratio
            "desc\n2\n3/0\n2/1\n",                // zero denominator
            "desc\n2\n1.2.3\n2/1\n",              // two points
            "desc\n1\n-100.0\n",                  // period below 1/1
        };
        auto refused = 0;
        juce::String reasons;
        for (const auto* text : badScales)
        {
            Tuning::Scale scale;
            juce::String error;
            if (! Tuning::parseScale (text, scale, error) && error.isNotEmpty())
                ++refused;
            reasons << error << "; ";
        }
        check (refused == (int) std::size (badScales), "tuning: " + juce::String (refused) + " of "
                   + juce::String ((int) std::size (badScales)) + " malformed .scl files refused (" + reasons + ")");

        const char* const badMaps[] {
            "12\n0\n127\n60\n69\n",                       // header too short
            "12\n0\n127\n60\n69\n0\n12\n0\n",             // frequency 0
            "12\n0\n127\n60\n69\n440\n12\n0\ny\n",        // not a degree
            "12\n0\n200\n60\n69\n440\n12\n",              // note out of range
            "1\n0\n127\n60\n69\n440\n1\nx\n",             // parses, but the reference is unmapped
        };
        auto mapsRefused = 0;
        for (const auto* text : badMaps)
        {
            Tuning tuning;
            juce::String error;
            if (! Tuning::build (edo (12, "12-TET"), text, tuning, error) && error.isNotEmpty())
                ++mapsRefused;
        }
        check (mapsRefused == (int) std::size (badMaps), "tuning: " + juce::String (mapsRefused) + " of "
                   + juce::String ((int) std::size (badMaps)) + " malformed .kbm files refused");

        IlanaSynthAudioProcessor processor;
        juce::String error;
        processor.loadTuningScale (edo (19, "19-EDO"), error);
        const auto refusedLoad = ! processor.loadTuningScale ("desc\n3\n", error);
        check (refusedLoad && processor.getTuningState().getDescription() == "19-EDO",
               "tuning: a malformed file leaves the loaded tuning in place (" + error + ")");
    }
}

void runProcessorTests()
{
    // A note through the processor with 19-EDO loaded.
    {
        IlanaSynthAudioProcessor processor;
        plainPatch (processor);
        juce::String error;
        const auto loaded = processor.loadTuningScale (edo (19, "19-EDO"), error);
        const auto on = processor.apvts.getRawParameterValue ("tuning_on")->load() > 0.5f;
        const auto pitch = pitchOf (play (processor, 64));
        const auto expected = 261.6255653005986 * std::exp2 (4.0 / 19.0);
        const auto error19 = centsBetween (pitch, expected);
        check (loaded && on && std::abs (error19) < 2.0, "tuning: note 64 in 19-EDO plays " + juce::String (pitch, 2)
                   + " Hz (expected " + juce::String (expected, 2) + ", " + juce::String (error19, 2) + " cents)");

        // STRETCH still applies on top (note 100: +35 cents at full stretch).
        IlanaSynthAudioProcessor stretched;
        plainPatch (stretched);
        stretched.loadTuningScale (edo (19, "19-EDO"), error);
        setParam (stretched, "stretch", 1.0f);
        const auto stretchedPitch = pitchOf (play (stretched, 100));
        const auto stretchExpected = 261.6255653005986 * std::exp2 (40.0 / 19.0) * std::exp2 (35.0 / 1200.0);
        check (std::abs (centsBetween (stretchedPitch, stretchExpected)) < 2.0,
               "tuning: STRETCH applies on top of the scale (" + juce::String (centsBetween (stretchedPitch, stretchExpected), 2) + " cents)");
    }

    // An unmapped key is silent.
    {
        IlanaSynthAudioProcessor processor;
        plainPatch (processor);
        juce::String error;
        processor.loadTuningScale (edo (12, "12-TET"), error);
        processor.loadTuningMapping ("12\n0\n127\n60\n69\n440\n12\n0\nx\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n", error);
        auto peakUnmapped = 0.0f, peakMapped = 0.0f;
        for (const auto x : play (processor, 61, 0.3)) peakUnmapped = juce::jmax (peakUnmapped, std::abs (x));
        for (const auto x : play (processor, 62, 0.3)) peakMapped = juce::jmax (peakMapped, std::abs (x));
        check (peakUnmapped == 0.0f && peakMapped > 0.01f, "tuning: an unmapped key is silent (peak " + juce::String (peakUnmapped)
                   + ", a mapped key " + juce::String (peakMapped, 3) + ")");
    }

    // The tuning survives a state save and restore; an old state is 12-TET.
    {
        IlanaSynthAudioProcessor source;
        juce::String error;
        const auto kbm = juce::String ("0\n0\n127\n60\n69\n432\n0\n");
        source.loadTuningScale (edo (19, "19-EDO"), error);
        source.loadTuningMapping (kbm, error);
        juce::MemoryBlock block;
        source.getStateInformation (block);

        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (block.getData(), (int) block.getSize());
        const auto state = restored.getTuningState().get();
        check (state != nullptr && restored.getTuningState().getDescription() == "19-EDO"
                   && restored.getTuningState().getMappingText().trim() == kbm.trim()
                   && std::abs (state->getFrequency (69) - 432.0) < 1.0e-9
                   && restored.apvts.getRawParameterValue ("tuning_on")->load() > 0.5f,
               "tuning: the scale, mapping and switch survive a state save and restore");

        // An old state (no Tuning child) loads as 12-TET.
        IlanaSynthAudioProcessor fresh;
        juce::MemoryBlock oldBlock;
        fresh.getStateInformation (oldBlock);
        std::unique_ptr<juce::XmlElement> xml (juce::AudioProcessor::getXmlFromBinary (oldBlock.getData(), (int) oldBlock.getSize()));
        const auto hadChild = xml != nullptr && xml->getChildByName ("Tuning") != nullptr;
        restored.setStateInformation (oldBlock.getData(), (int) oldBlock.getSize());
        check (! hadChild && ! restored.getTuningState().hasScale()
                   && restored.apvts.getRawParameterValue ("tuning_on")->load() < 0.5f,
               "tuning: a state without a tuning loads as 12-TET");

        // Browsing factory presets keeps the loaded tuning (they carry none).
        source.loadFactoryPreset (1);
        check (source.getTuningState().hasScale() && source.apvts.getRawParameterValue ("tuning_on")->load() > 0.5f,
               "tuning: a loaded scale stays on through factory presets");
    }

    // Tuning off renders exactly as before, even with a scale loaded.
    {
        IlanaSynthAudioProcessor plain, loadedOff, reset;
        juce::String error;
        for (auto* processor : { &plain, &loadedOff, &reset })
            plainPatch (*processor);
        loadedOff.loadTuningScale (edo (19, "19-EDO"), error);
        setParam (loadedOff, "tuning_on", 0.0f);
        reset.loadTuningScale (justMajor, error);
        reset.resetTuning();

        const auto a = play (plain, 57, 0.5);
        const auto b = play (loadedOff, 57, 0.5);
        const auto c = play (reset, 57, 0.5);
        auto peak = 0.0f;
        for (const auto x : a) peak = juce::jmax (peak, std::abs (x));
        check (peak > 0.01f && a == b && a == c, "tuning: off renders bit-identically (loaded but off, and after a reset)");

        // Switched on without a scale: still the 12-TET path.
        IlanaSynthAudioProcessor onEmpty;
        plainPatch (onEmpty);
        setParam (onEmpty, "tuning_on", 1.0f);
        check (play (onEmpty, 57, 0.5) == a, "tuning: on with no scale loaded renders as 12-TET");
    }
}
} // namespace TuningTests

void runTuningTests()
{
    TuningTests::runParserTests();
    TuningTests::runProcessorTests();
}
