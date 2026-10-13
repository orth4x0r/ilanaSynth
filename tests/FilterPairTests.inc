// The Ladder's stereo pair path (LadderFilter::processPairBlock, SSE lanes)
// must give bit for bit what two separate runs give, for both models, both
// slopes, any coefficients and any mix of pair and single blocks.
// Included by TableTest.cpp; ILANA_FILTERPAIR_TEST=1 runs only these.

void runFilterPairTests()
{
    std::cout << "--- ladder stereo pair path ---" << std::endl;
    juce::Random random (4242);
    auto worstMismatch = 0;
    auto worstCase = juce::String();

    for (const auto type : { FilterType::LadderLow, FilterType::LadderHigh })
        for (const auto slope24 : { false, true })
            for (const auto cutoff : { 60.0, 400.0, 3000.0, 12000.0, 21000.0 })
                for (const auto resonance : { 0.0, 0.5, 0.95, 1.0 })
                {
                    FilterUnit singleL, singleR, pairL, pairR;
                    for (auto* f : { &singleL, &singleR, &pairL, &pairR })
                    {
                        f->prepare (48000.0);
                        f->setType (type, slope24);
                        f->setCoefficients (FilterUnit::makeCoefficients (type, 48000.0, cutoff, resonance, 0.0f));
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

                    singleL.processBlock (inL.data(), refL.data(), total);
                    singleR.processBlock (inR.data(), refR.data(), total);

                    // Pair blocks, with some stretches run as two single filters in between
                    // (state must carry across both ways).
                    int pos = 0, which = 0;
                    while (pos < total)
                    {
                        const auto len = juce::jmin (total - pos, 1 + random.nextInt (257));
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
                                    + juce::String (cutoff) + " reso " + juce::String (resonance);
                    }
                }

    check (worstMismatch == 0, "ladder pair path matches two single runs bit for bit (worst: " + juce::String (worstMismatch)
                               + " samples differ, " + worstCase + ")");
}
