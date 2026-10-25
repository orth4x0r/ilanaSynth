// FRAME BLEND (oscN_frame_blend): Crossfade (every older patch), Spectral
// (Wavetable::makeSpectralBlend, built by the SpectralCache) and Stepped.

namespace FrameBlendTest
{
// A sine and the same sine a quarter turn later: a plain crossfade halfway
// between them dips to 0.707, a spectral blend keeps full level.
inline std::vector<std::vector<float>> quarterTurnFrames()
{
    std::vector<std::vector<float>> frames (2, std::vector<float> ((size_t) Wavetable::frameSize));

    for (int i = 0; i < Wavetable::frameSize; ++i)
    {
        const auto angle = juce::MathConstants<double>::twoPi * (double) i / (double) Wavetable::frameSize;
        frames[0][(size_t) i] = (float) std::sin (angle);
        frames[1][(size_t) i] = (float) std::cos (angle);
    }

    return frames;
}

inline float peakAt (const Wavetable& table, float position, bool stepped)
{
    WavetableOscillator osc;
    osc.setSampleRate (48000.0);
    osc.setWavetable (&table);
    osc.setFrequency (100.0);
    const auto frames = WavetableOscillator::frameReadFor (&table, position, stepped);
    auto peak = 0.0f;

    for (int i = 0; i < 960; ++i)
        peak = juce::jmax (peak, std::abs (osc.getNextSample (0.0, frames)));

    return peak;
}
} // namespace FrameBlendTest

void runFrameBlendTests()
{
    using namespace FrameBlendTest;

    Wavetable table;
    table.buildFromFrames (quarterTurnFrames());
    const auto blended = Wavetable::makeSpectralBlend (table);

    check (blended != nullptr && blended->getNumFrames() == 129,
           "Spectral blend gives a two-frame table 128 steps (" + juce::String (blended != nullptr ? blended->getNumFrames() : 0) + " frames)");

    if (blended == nullptr)
        return;

    // The ends are the source frames, unchanged.
    auto worst = 0.0f;
    for (const auto [frame, source] : { std::pair { 0, 0 }, std::pair { 128, 1 } })
        for (int i = 0; i < Wavetable::frameSize + 3; ++i)
            worst = juce::jmax (worst, std::abs (blended->getFrameData (0, frame)[i] - table.getFrameData (0, source)[i]));
    check (worst < 1.0e-4f, "Spectral blend keeps the source frames at both ends (worst difference " + juce::String (worst, 7) + ")");

    const auto crossfadeMiddle = peakAt (table, 0.5f, false);
    const auto spectralMiddle = peakAt (*blended, 0.5f, false);
    check (crossfadeMiddle < 0.75f && spectralMiddle > 0.98f,
           "halfway between two out-of-phase frames, Crossfade dips and Spectral keeps its level (peak "
               + juce::String (crossfadeMiddle, 3) + " vs " + juce::String (spectralMiddle, 3) + ")");

    // Stepped plays the nearest whole frame.
    const auto low = WavetableOscillator::frameReadFor (&table, 0.4f, true);
    const auto high = WavetableOscillator::frameReadFor (&table, 0.6f, true);
    check (low.frame0 == 0 && low.frame1 == 0 && low.frac == 0.0f && high.frame0 == 1 && high.frame1 == 1 && high.frac == 0.0f,
           "Stepped snaps to the nearest frame (0.4 -> " + juce::String (low.frame0) + ", 0.6 -> " + juce::String (high.frame0) + ")");
    check (std::abs (peakAt (table, 0.4f, true) - peakAt (table, 0.0f, false)) < 1.0e-6f, "Stepped at 0.4 plays frame 1 whole");

    // Through the processor: Spectral swaps in the blended copy, Stepped holds a frame,
    // Crossfade (the default) is unchanged.
    const auto render = [] (int blend, float frame)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        processor.setUserTable (0, WavetableDoc::fromFrames ("Quarter turn", quarterTurnFrames()));
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "osc1_table", (float) TableFactory::getNumFactoryTables());
        setParam (processor, "osc1_frame", frame);
        setParam (processor, "osc1_frame_blend", (float) blend);
        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        return renderPeakAndCentroid (processor, 57, 24, centroid);
    };

    {
        IlanaSynthAudioProcessor fresh;
        auto allCrossfade = true;
        for (const auto* prefix : OscillatorIds::prefixes)
            allCrossfade = allCrossfade && fresh.apvts.getRawParameterValue (juce::String (prefix) + "_frame_blend")->load() == 0.0f;
        check (allCrossfade, "FRAME BLEND defaults to Crossfade on every oscillator");
    }

    const auto crossfade = render (FrameBlend::crossfade, 0.5f);
    const auto spectral = render (FrameBlend::spectral, 0.5f);
    const auto stepped = render (FrameBlend::stepped, 0.5f);
    const auto endFrame = render (FrameBlend::crossfade, 1.0f);
    check (spectral > crossfade * 1.25f,
           "a patch on Spectral keeps its level halfway through a phase-turning table (peak " + juce::String (crossfade, 3) + " -> "
               + juce::String (spectral, 3) + ")");
    check (std::abs (stepped - endFrame) < endFrame * 0.02f && stepped > crossfade * 1.25f,
           "a patch on Stepped plays a whole frame at 0.5 (peak " + juce::String (stepped, 3) + ", frame 2 alone "
               + juce::String (endFrame, 3) + ")");
}
