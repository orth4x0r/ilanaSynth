#include "ProcessorInternal.h"

// FX files and preloads, message thread: the IR reverb's impulse file (saved
// with the patch) and the Airwindows algorithms made ahead of the audio
// thread. (Split from Effects.cpp, which holds the audio-thread side.)

void IlanaSynthAudioProcessor::FxEngine::loadReverbIr (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    convolution.loadImpulseResponse (file,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::yes,
                                     0,
                                     juce::dsp::Convolution::Normalise::yes);
    reverbIrLoaded.store (true);
    reverbIrPath = file.getFullPathName();
}

void IlanaSynthAudioProcessor::FxEngine::syncReverbIr (const juce::String& path)
{
    if (path == reverbIrPath)
        return;
    if (path.isEmpty())
    {
        reverbIrLoaded.store (false);
        reverbIrPath = {};
        return;
    }
    // (A file that is gone leaves the reverb dry; the patch keeps its path.)
    const juce::File file (path);
    if (file.existsAsFile())
        loadReverbIr (file);
    else
    {
        reverbIrLoaded.store (false);
        reverbIrPath = {};
    }
}

void IlanaSynthAudioProcessor::preloadAirwindows (int algorithm)
{
    fxMain->preloadAirwindows (algorithm);
    for (auto& engine : slotEngines)
        if (auto* own = engine.load (std::memory_order_acquire))
            own->preloadAirwindows (algorithm);
}

void IlanaSynthAudioProcessor::preloadAirwindowsCategory (int category, int choice)
{
    fxMain->preloadAirwindowsCategory (category, choice);
    for (auto& engine : slotEngines)
        if (auto* own = engine.load (std::memory_order_acquire))
            own->preloadAirwindowsCategory (category, choice);
}

// After a patch or session load: every Airwindows slot's algorithm is made
// here, on the message thread, instead of on the audio thread's first block.
void IlanaSynthAudioProcessor::preloadAirwindowsForPatch()
{
    auto main = false;
    std::array<bool, (size_t) numAwCategories> categories {};
    for (int slot = 0; slot < numFxSlots; ++slot)
    {
        const auto type = (int) getParam (fxSlotIds[(size_t) slot].type);
        main = main || type == 30;
        if (const auto category = airwindows::categoryForFxType (type); juce::isPositiveAndBelow (category, (int) categories.size()))
            categories[(size_t) category] = true;
    }
    if (main)
        preloadAirwindows ((int) getParam ("fx_aw_algo"));
    const auto refs = makeAwCategoryRefs();
    for (int category = 0; category < (int) categories.size() && category < (int) refs.size(); ++category)
        if (categories[(size_t) category])
            preloadAirwindowsCategory (category, (int) getParam (refs[(size_t) category][0]));
}

// The REVERB card's IR file: one per patch, saved with it and in undo, and
// on every engine that runs a reverb (the shared one and each slot's own).
void IlanaSynthAudioProcessor::loadReverbIr (const juce::File& file)
{
    if (! file.existsAsFile())
        return;
    if (pendingEditData.has_value())
        setReverbIrPath (file.getFullPathName());
    else
        performEdit ("Load impulse response", [this, &file] { setReverbIrPath (file.getFullPathName()); });
}

void IlanaSynthAudioProcessor::setReverbIrPath (const juce::String& path)
{
    if (path != reverbIrFile)
    {
        reverbIrFile = path;
        ++dataEpoch; // part of the patch's data (undo, the EDITED marker)
    }
    syncReverbIrEngines();
}

void IlanaSynthAudioProcessor::syncReverbIrEngines()
{
    fxMain->syncReverbIr (reverbIrFile);
    for (auto& engine : slotEngines)
        if (auto* own = engine.load (std::memory_order_acquire))
            own->syncReverbIr (reverbIrFile);
}
