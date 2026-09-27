#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <vector>

#include "Wavetable.h"

// M7.4: an editable wavetable. It keeps the frames (2048 samples each, 1-256
// of them) and, per frame, the recipe that made it, so an edited table can be
// rebuilt exactly after reloading and edited again.
//
// A patch stores each edited table as a versioned <Wavetable> child with up
// to three descriptions, and the loader uses the best one it finds:
// - the recipe (small, exact, editable),
// - embedded data (the frames as compressed 16-bit), written whenever the
//   recipe can't rebuild the table exactly (a file, a crossfade morph),
// - the source file's path, as a hint.
// If none works, the slot falls back to its default table with a notice.
struct FrameRecipe
{
    enum class Kind { Raw = 0, Draw, Harmonics, Formula };

    Kind kind = Kind::Raw;
    std::vector<float> points; // Draw: x, y pairs (x 0..1, y -1..1), sorted by x
    float smoothing = 0.0f;    // Draw: 0..1
    std::vector<float> magnitudes, phases; // Harmonics: from harmonic 1 up
    juce::String formula;      // Formula (x, f, n; see Formula.h)

    bool operator== (const FrameRecipe& other) const
    {
        return kind == other.kind && points == other.points && smoothing == other.smoothing
               && magnitudes == other.magnitudes && phases == other.phases && formula == other.formula;
    }
};

class WavetableDoc
{
public:
    static constexpr int frameSize = Wavetable::frameSize;
    static constexpr int maxFrames = 256;
    static constexpr int formatVersion = 1;
    // A 256-frame table is about 1 MB at 16 bits before compression.
    static constexpr int largeTableFrames = 256;

    juce::String name;
    std::vector<std::vector<float>> frames;
    std::vector<FrameRecipe> recipes;

    // An unedited factory table (-1 if not): its index rebuilds it exactly.
    int factoryIndex = -1;
    // The file the table came from (a hint for reloading an edited source).
    juce::String sourcePath;
    int sourceMode = 0;

    int getNumFrames() const { return (int) frames.size(); }

    // True when the recipes alone rebuild every frame exactly.
    bool canRebuildExactly() const;

    // Renders frame i from its recipe (Raw frames are left alone). The
    // formula's error, if any, goes to error.
    void renderFrame (int index, juce::String* error = nullptr);
    void renderAll();

    // Frame edits keep frames and recipes in step.
    void insertFrame (int index, std::vector<float> frame, FrameRecipe recipe);
    void removeFrame (int index);
    void moveFrame (int from, int to);

    // Fills the frames strictly between a and b by morphing: a crossfade
    // (Raw frames) or a spectral morph (magnitudes interpolated, phases from
    // the nearer end; exact Harmonics recipes).
    void morph (int a, int b, bool spectral);

    // Recipe renderers, usable on their own.
    static std::vector<float> renderDraw (const std::vector<float>& points, float smoothing);
    static std::vector<float> renderHarmonics (const std::vector<float>& magnitudes, const std::vector<float>& phases);
    static std::vector<float> renderFormula (const juce::String& formula, double framePosition, int frameIndex,
                                             juce::String* error = nullptr);
    // A frame's harmonic magnitudes and phases (up to count harmonics).
    static void analyse (const std::vector<float>& frame, std::vector<float>& magnitudes,
                         std::vector<float>& phases, int count = Wavetable::numHarmonics);

    // Starting points.
    static WavetableDoc fromFactory (int index);
    static WavetableDoc fromFrames (const juce::String& name, std::vector<std::vector<float>> frames);
    static WavetableDoc sine();

    // Patch storage (see the class comment). fromValueTree returns false if
    // no description could be used; notice says what happened.
    juce::ValueTree toValueTree() const;
    static bool fromValueTree (const juce::ValueTree& tree, WavetableDoc& doc, juce::String& notice);

    // The recipe on its own, for the library's .ilwt sidecar.
    juce::ValueTree recipeTree() const;
    bool applyRecipeTree (const juce::ValueTree& tree);

    // Export as a .wav that Serum and Vital read: 2048-sample frames back to
    // back, 32-bit float, with a "clm " chunk.
    bool exportWav (const juce::File& file) const;

    // The user library: Documents/ilanaSynth Wavetables, a .wav plus an
    // .ilwt sidecar holding the recipe.
    static juce::File getLibraryFolder();
    bool saveToLibrary (const juce::String& fileName, juce::File* written = nullptr) const;
    static bool loadFromFile (const juce::File& file, WavetableDoc& doc, Wavetable::LoadMode mode = Wavetable::LoadMode::Automatic);

    static juce::String encodeFrames (const std::vector<std::vector<float>>& frames);
    static bool decodeFrames (const juce::String& text, std::vector<std::vector<float>>& frames);
};
