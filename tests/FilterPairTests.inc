// The stereo pair paths (LadderFilter::processPairBlock, DriveLadder and
// OtaFilter::processPairBlock via CascadePair.h, Disperser, VowelFilter and VowelMorph::processPairBlock; SSE lanes) must give bit for bit what two separate runs give, for both models, both
// slopes, any coefficients and any mix of pair and single blocks.
// Included by TableTest.cpp; ILANA_FILTERPAIR_TEST=1 runs only these.

void runFilterPairTests()
{
    std::cout << "--- stereo pair paths (ladder, drive ladder, OTA) ---" << std::endl;
    juce::Random random (4242);
    auto worstMismatch = 0;
    auto worstCase = juce::String();

    for (const auto type : { FilterType::LadderLow, FilterType::LadderHigh, FilterType::LadderBand, FilterType::LadderDrive,
                              FilterType::OtaLow, FilterType::OtaBand, FilterType::Disperser,
                              FilterType::VowelBank, FilterType::Talking, FilterType::VowelMorph })
        for (const auto slope24 : { false, true })
            for (const auto cutoff : { 60.0, 400.0, 3000.0, 12000.0, 21000.0 })
                for (const auto resonance : { 0.0, 0.5, 0.95, 1.0 })
                for (const auto morph : { 0.0f, 0.37f, 1.0f })
                {
                    if (morph != 0.0f && type != FilterType::Disperser && type != FilterType::VowelBank
                        && type != FilterType::Talking && type != FilterType::VowelMorph)
                        continue;
                    // Halfway the settings move (the Disperser glides and drops sections).
                    const int change = 3000;
                    const auto moveAll = [&] (std::initializer_list<FilterUnit*> filters)
                    {
                        for (auto* f : filters)
                            f->setCoefficients (FilterUnit::makeCoefficients (type, 48000.0, cutoff * 0.5, resonance * 0.5, morph * 0.5f));
                    };
                    FilterUnit singleL, singleR, pairL, pairR;
                    for (auto* f : { &singleL, &singleR, &pairL, &pairR })
                    {
                        f->prepare (48000.0);
                        f->setType (type, slope24);
                        f->setCoefficients (FilterUnit::makeCoefficients (type, 48000.0, cutoff, resonance, morph));
                    }

                    // Loud, uncorrelated noise with silent and huge stretches, in blocks of odd sizes.
                    const int total = 6000;
                    std::vector<float> inL ((size_t) total), inR ((size_t) total), refL ((size_t) total), refR ((size_t) total),
                                       gotL ((size_t) total), gotR ((size_t) total);
                    for (int i = 0; i < total; ++i)
                    {
                        const auto gain = (i / 700) % 3 == 0 ? 0.0f : ((i / 700) % 3 == 1 ? 1.0f : 40.0f);
                        inL[(size_t) i] = gain * (random.nextFloat() * 2.0f - 1.0f);
                        inR[(size_t) i] = gain * (random.nextFloat() * 2.0f - 1.0f);
                    }

                    singleL.processBlock (inL.data(), refL.data(), change);
                    singleR.processBlock (inR.data(), refR.data(), change);
                    moveAll ({ &singleL, &singleR });
                    singleL.processBlock (inL.data() + change, refL.data() + change, total - change);
                    singleR.processBlock (inR.data() + change, refR.data() + change, total - change);

                    // Pair blocks, with some stretches run as two single filters in between
                    // (state must carry across both ways).
                    int pos = 0, which = 0;
                    while (pos < total)
                    {
                        if (pos == change)
                            moveAll ({ &pairL, &pairR });
                        const auto len = juce::jmin ((pos < change ? change : total) - pos, 1 + random.nextInt (257));
                        if (which++ % 3 == 2)
                        {
                            pairL.processBlock (inL.data() + pos, gotL.data() + pos, len);
                            pairR.processBlock (inR.data() + pos, gotR.data() + pos, len);
                        }
                        else
                            FilterUnit::processStereoBlock (pairL, pairR, inL.data() + pos, inR.data() + pos,
                                                            gotL.data() + pos, gotR.data() + pos, len);
                        pos += len;
                    }

                    auto mismatches = 0;
                    for (int i = 0; i < total; ++i)
                        if (std::memcmp (&refL[(size_t) i], &gotL[(size_t) i], sizeof (float)) != 0
                            || std::memcmp (&refR[(size_t) i], &gotR[(size_t) i], sizeof (float)) != 0)
                            ++mismatches;

                    if (mismatches > worstMismatch)
                    {
                        worstMismatch = mismatches;
                        worstCase = "type " + juce::String (type) + " slope24 " + juce::String ((int) slope24) + " cutoff "
                                    + juce::String (cutoff) + " reso " + juce::String (resonance) + " morph " + juce::String (morph);
                    }
                }

    check (worstMismatch == 0, "pair paths match two single runs bit for bit (worst: " + juce::String (worstMismatch)
                               + " samples differ, " + worstCase + ")");
}
