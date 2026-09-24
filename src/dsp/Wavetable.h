#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <complex>
#include <vector>

class Wavetable
{
public:
    static constexpr int frameSize = 2048;
    static constexpr int numHarmonics = frameSize / 2;
    static constexpr int numLevels = 11;

    Wavetable() = default;

    bool loadFromFile (const juce::File& file);
    void buildFromFrames (const std::vector<std::vector<float>>& frames);

    int getNumFrames() const noexcept { return numFrames; }

    const float* getFrameData (int level, int frame) const noexcept
    {
        return levels[(size_t) level][(size_t) frame].data();
    }

    int getLevelForFrequency (double frequency, double sampleRate) const noexcept;

    const juce::String& getName() const noexcept { return name; }
    void setName (juce::String newName) { name = std::move (newName); }

private:
    void buildLevels (const std::vector<std::vector<float>>& frames);

    int numFrames = 0;
    juce::String name;
    std::vector<std::vector<std::vector<float>>> levels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Wavetable)
};
