// The parameter text tables (step 48, src/gui/ParamInfo.h): a value in each
// unit, the rows that decline and pass a value on, and every row covering at
// least one real parameter (a row no parameter reaches is dead text, or a
// broader row above it shadows it). Included by TableTest.cpp;
// ILANA_PARAMINFO_TEST=1 runs only these.

namespace ParamInfoLab
{
// The row that covers an ID first, or -1.
template <typename Row, size_t count>
int firstRow (const Row (&rows)[count], const ParamInfo::RowIndex& index, const juce::String& id)
{
    int found = -1;
    ParamInfo::firstShown (rows, index, id, [&] (const Row& row) { found = (int) (&row - rows); return juce::String ("x"); });
    return found;
}

inline void expectText (const juce::String& id, float value, const juce::String& expected)
{
    const auto shown = describeValue (id, value);
    check (shown == expected, "param text: " + id + " at " + juce::String (value) + " shows \"" + shown + "\" (want \"" + expected + "\")");
}
} // namespace ParamInfoLab

void runParamInfoTests()
{
    using ParamInfoLab::expectText;
    const auto times = juce::String (juce::CharPointer_UTF8 ("\xc3\x97"));

    // One value per unit, and the custom ones.
    expectText ("fx_voc_bands", 16.0f, "16 bands");
    expectText ("fx_voc_width", 1.5f, times + "1.5");
    expectText ("fx_voc_formant", 2.0f, "+2.0 st");
    expectText ("fx_voc_level", -3.25f, "-3.3 dB");
    expectText ("osc1_pan", -0.25f, "L 25");
    expectText ("osc1_pan", 0.0f, "C");
    expectText ("lfo3_seed", 0.0f, "Random");
    expectText ("lfo3_seed", 7.0f, "#7");
    expectText ("mod5_amt", 0.25f, "+25%");
    expectText ("mod5_amt", -0.25f, "-25%");
    expectText ("amp_attack", 0.5f, "500 ms");
    expectText ("amp_attack", 1.234f, "1.23 s");
    expectText ("osc1_delay", 0.0f, "0 ms");
    expectText ("f1_cutoff", 1500.0f, "1.50 kHz");
    expectText ("f1_cutoff", 440.0f, "440 Hz");
    expectText ("osc2_semi", 7.0f, "7 st");
    expectText ("osc2_fine", 12.5f, "12.5 ct");
    expectText ("arp_pitch3", 5.0f, "+5 st");
    expectText ("arp_len3", 0.0f, "rest");
    expectText ("filter_balance", -0.3f, "F1 +30%");
    expectText ("filter_balance", 0.0f, "Even");
    expectText ("west_stages", 1.0f, "1 stage");
    expectText ("west_decay", 1.0f, times + "1.00");
    expectText ("poly_voices", 8.0f, "8");
    expectText ("osc1_ratio", 2.0f, times + "2.00");
    expectText ("osc1_eg_l1", 0.0f, silentDecibels());
    expectText ("opeg_lfo_pms", 0.0f, "0 st");

    // A row may decline a value and pass it on: 0 is Auto or Off, anything
    // else is a percent from the rows below.
    expectText ("osc1_string_excite_pos", 0.0f, "Auto");
    expectText ("osc1_string_excite_pos", 0.5f, "50%");
    expectText ("osc1_string_pick_pos", 0.0f, "Off");
    expectText ("osc1_string_pick_pos", 0.25f, "25%");

    // No row: a plain number, whole when it rounds to one.
    expectText ("not_a_parameter", 0.997f, "1");
    expectText ("not_a_parameter", 0.5f, "0.50");
    check (describeParameter ("not_a_parameter").isEmpty(), "param help: an unknown ID has none");
    check (describeParameter ("lfo3_smooth").startsWith ("Glide on the LFO"), "param help: an LFO's SMOOTH");
    check (describeParameter ("opeg_pitch_l2").startsWith ("MID: the pitch envelope's level 2"), "param help: the pitch envelope's L2");

    // Every row is the first to cover at least one parameter. (21 parameters
    // have no help line yet: the Airwindows modules' MIX, the envelopes'
    // curve choices, Multi-Core Voices and sustain_voices; listed, not failed.)
    IlanaSynthAudioProcessor processor;
    std::vector<bool> valueUsed (std::size (ParamInfo::valueRows)), helpUsed (std::size (ParamInfo::helpRows));
    juce::StringArray withoutHelp;
    for (auto* parameter : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
        {
            const auto id = withId->paramID;
            if (const auto row = ParamInfoLab::firstRow (ParamInfo::valueRows, ParamInfo::valueIndex(), id); row >= 0)
                valueUsed[(size_t) row] = true;
            if (const auto row = ParamInfoLab::firstRow (ParamInfo::helpRows, ParamInfo::helpIndex(), id); row >= 0)
                helpUsed[(size_t) row] = true;
            else
                withoutHelp.add (id);
        }

    for (size_t row = 0; row < valueUsed.size(); ++row)
        if (! valueUsed[row])
            check (false, "param text: value row " + juce::String ((int) row) + " covers no parameter");
    for (size_t row = 0; row < helpUsed.size(); ++row)
        if (! helpUsed[row])
            check (false, "param help: help row " + juce::String ((int) row) + " covers no parameter (" + ParamInfo::helpRows[row].text + ")");
    std::cout << "param help: " << withoutHelp.size() << " parameters without a help line: "
              << withoutHelp.joinIntoString (" ") << std::endl;
}
