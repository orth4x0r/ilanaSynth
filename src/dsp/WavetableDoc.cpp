#include "WavetableDoc.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <complex>

#include "Formula.h"
#include "TableFactory.h"

namespace
{
using Complex = std::complex<float>;

juce::String encodeFloats (const std::vector<float>& values)
{
    juce::MemoryBlock block (values.data(), values.size() * sizeof (float));
    return block.toBase64Encoding();
}

std::vector<float> decodeFloats (const juce::String& text)
{
    juce::MemoryBlock block;
    if (text.isEmpty() || ! block.fromBase64Encoding (text))
        return {};
    std::vector<float> values (block.getSize() / sizeof (float));
    std::memcpy (values.data(), block.getData(), values.size() * sizeof (float));
    for (auto& value : values)
        if (! std::isfinite (value))
            value = 0.0f;
    return values;
}

juce::ValueTree frameRecipeTree (const FrameRecipe& recipe)
{
    juce::ValueTree tree ("Frame");
    tree.setProperty ("kind", (int) recipe.kind, nullptr);
    switch (recipe.kind)
    {
        case FrameRecipe::Kind::Draw:
            tree.setProperty ("points", encodeFloats (recipe.points), nullptr);
            tree.setProperty ("smoothing", recipe.smoothing, nullptr);
            break;
        case FrameRecipe::Kind::Harmonics:
            tree.setProperty ("magnitudes", encodeFloats (recipe.magnitudes), nullptr);
            tree.setProperty ("phases", encodeFloats (recipe.phases), nullptr);
            break;
        case FrameRecipe::Kind::Formula:
            tree.setProperty ("formula", recipe.formula, nullptr);
            break;
        case FrameRecipe::Kind::Raw:
            break;
    }
    return tree;
}

FrameRecipe frameRecipeFrom (const juce::ValueTree& tree)
{
    FrameRecipe recipe;
    recipe.kind = (FrameRecipe::Kind) juce::jlimit (0, 3, (int) tree.getProperty ("kind", 0));
    recipe.points = decodeFloats (tree.getProperty ("points").toString());
    if (recipe.points.size() % 2 != 0)
        recipe.points.pop_back();
    recipe.smoothing = (float) tree.getProperty ("smoothing", 0.0f);
    recipe.magnitudes = decodeFloats (tree.getProperty ("magnitudes").toString());
    recipe.phases = decodeFloats (tree.getProperty ("phases").toString());
    recipe.formula = tree.getProperty ("formula").toString();
    if (recipe.kind == FrameRecipe::Kind::Draw && recipe.points.size() < 4)
        recipe.kind = FrameRecipe::Kind::Raw;
    if (recipe.kind == FrameRecipe::Kind::Formula && recipe.formula.isEmpty())
        recipe.kind = FrameRecipe::Kind::Raw;
    return recipe;
}

void writeChunk (juce::OutputStream& out, const char* id, const void* data, size_t size)
{
    out.write (id, 4);
    out.writeInt ((int) size);
    out.write (data, size);
    if (size & 1)
        out.writeByte (0);
}
} // namespace

bool WavetableDoc::canRebuildExactly() const
{
    if (factoryIndex >= 0)
        return true;
    if (recipes.size() != frames.size() || frames.empty())
        return false;
    for (const auto& recipe : recipes)
        if (recipe.kind == FrameRecipe::Kind::Raw)
            return false;
    return true;
}

void WavetableDoc::renderFrame (int index, juce::String* error)
{
    if (index < 0 || index >= (int) frames.size() || index >= (int) recipes.size())
        return;
    const auto& recipe = recipes[(size_t) index];
    const auto position = frames.size() > 1 ? (double) index / (double) (frames.size() - 1) : 0.0;
    switch (recipe.kind)
    {
        case FrameRecipe::Kind::Draw:
            frames[(size_t) index] = renderDraw (recipe.points, recipe.smoothing);
            break;
        case FrameRecipe::Kind::Harmonics:
            frames[(size_t) index] = renderHarmonics (recipe.magnitudes, recipe.phases);
            break;
        case FrameRecipe::Kind::Formula:
            frames[(size_t) index] = renderFormula (recipe.formula, position, index, error);
            break;
        case FrameRecipe::Kind::Raw:
            break;
    }
}

void WavetableDoc::renderAll()
{
    for (int i = 0; i < (int) frames.size(); ++i)
        renderFrame (i);
}

void WavetableDoc::insertFrame (int index, std::vector<float> frame, FrameRecipe recipe)
{
    if ((int) frames.size() >= maxFrames)
        return;
    frame.resize ((size_t) frameSize, 0.0f);
    recipes.resize (frames.size());
    index = juce::jlimit (0, (int) frames.size(), index);
    frames.insert (frames.begin() + index, std::move (frame));
    recipes.insert (recipes.begin() + index, std::move (recipe));
    factoryIndex = -1;
}

void WavetableDoc::removeFrame (int index)
{
    if (frames.size() <= 1 || index < 0 || index >= (int) frames.size())
        return;
    recipes.resize (frames.size());
    frames.erase (frames.begin() + index);
    recipes.erase (recipes.begin() + index);
    factoryIndex = -1;
}

void WavetableDoc::moveFrame (int from, int to)
{
    const auto count = (int) frames.size();
    if (from < 0 || from >= count || to < 0 || to >= count || from == to)
        return;
    recipes.resize (frames.size());
    auto frame = std::move (frames[(size_t) from]);
    auto recipe = std::move (recipes[(size_t) from]);
    frames.erase (frames.begin() + from);
    recipes.erase (recipes.begin() + from);
    frames.insert (frames.begin() + to, std::move (frame));
    recipes.insert (recipes.begin() + to, std::move (recipe));
    factoryIndex = -1;
}

void WavetableDoc::morph (int a, int b, bool spectral)
{
    if (a > b)
        std::swap (a, b);
    if (a < 0 || b >= (int) frames.size() || b - a < 2)
        return;
    recipes.resize (frames.size());
    factoryIndex = -1;

    std::vector<float> magA, phaseA, magB, phaseB;
    if (spectral)
    {
        analyse (frames[(size_t) a], magA, phaseA);
        analyse (frames[(size_t) b], magB, phaseB);
    }
    for (int i = a + 1; i < b; ++i)
    {
        const auto t = (float) (i - a) / (float) (b - a);
        if (spectral)
        {
            FrameRecipe recipe;
            recipe.kind = FrameRecipe::Kind::Harmonics;
            // The ends can list different numbers of harmonics: the missing
            // ones are silent.
            const auto count = juce::jmax (magA.size(), magB.size());
            recipe.magnitudes.resize (count);
            recipe.phases.resize (count);
            for (size_t k = 0; k < count; ++k)
            {
                const auto from = k < magA.size() ? magA[k] : 0.0f;
                const auto to = k < magB.size() ? magB[k] : 0.0f;
                recipe.magnitudes[k] = from + (to - from) * t;
                const auto& nearer = t < 0.5f ? (k < phaseA.size() && from > 0.0f ? phaseA : phaseB)
                                              : (k < phaseB.size() && to > 0.0f ? phaseB : phaseA);
                recipe.phases[k] = k < nearer.size() ? nearer[k] : 0.0f;
            }
            recipes[(size_t) i] = std::move (recipe);
            renderFrame (i);
        }
        else
        {
            auto& frame = frames[(size_t) i];
            for (int s = 0; s < frameSize; ++s)
                frame[(size_t) s] = frames[(size_t) a][(size_t) s]
                                    + (frames[(size_t) b][(size_t) s] - frames[(size_t) a][(size_t) s]) * t;
            recipes[(size_t) i] = {};
        }
    }
}

std::vector<float> WavetableDoc::renderDraw (const std::vector<float>& points, float smoothing)
{
    std::vector<float> frame ((size_t) frameSize, 0.0f);
    const auto count = (int) points.size() / 2;
    if (count == 0)
        return frame;

    // Linear between the points, wrapping round the cycle.
    auto segment = 0;
    for (int i = 0; i < frameSize; ++i)
    {
        const auto x = (float) i / (float) frameSize;
        while (segment < count && points[(size_t) segment * 2] <= x)
            ++segment;
        const auto previous = segment == 0 ? count - 1 : segment - 1;
        const auto next = segment == count ? 0 : segment;
        auto x0 = points[(size_t) previous * 2];
        auto x1 = points[(size_t) next * 2];
        if (segment == 0) x0 -= 1.0f;
        if (segment == count) x1 += 1.0f;
        const auto y0 = points[(size_t) previous * 2 + 1];
        const auto y1 = points[(size_t) next * 2 + 1];
        const auto t = x1 > x0 ? (x - x0) / (x1 - x0) : 0.0f;
        frame[(size_t) i] = juce::jlimit (-1.0f, 1.0f, y0 + (y1 - y0) * t);
    }

    // Smoothing: a circular moving average, up to 1/16 of the cycle.
    const auto radius = (int) (juce::jlimit (0.0f, 1.0f, smoothing) * (float) frameSize / 32.0f);
    if (radius > 0)
    {
        std::vector<float> smoothed ((size_t) frameSize, 0.0f);
        auto sum = 0.0;
        for (int k = -radius; k <= radius; ++k)
            sum += frame[(size_t) ((k + frameSize) % frameSize)];
        const auto width = (double) (2 * radius + 1);
        for (int i = 0; i < frameSize; ++i)
        {
            smoothed[(size_t) i] = (float) (sum / width);
            sum += frame[(size_t) ((i + radius + 1) % frameSize)] - frame[(size_t) ((i - radius + frameSize) % frameSize)];
        }
        frame = std::move (smoothed);
    }
    return frame;
}

std::vector<float> WavetableDoc::renderHarmonics (const std::vector<float>& magnitudes, const std::vector<float>& phases)
{
    juce::dsp::FFT fft (11);
    std::vector<Complex> spectrum ((size_t) frameSize, Complex (0.0f, 0.0f));
    std::vector<Complex> time ((size_t) frameSize);
    const auto count = juce::jmin ((int) magnitudes.size(), Wavetable::numHarmonics - 1);
    for (int k = 0; k < count; ++k)
    {
        const auto phase = k < (int) phases.size() ? phases[(size_t) k] : 0.0f;
        // A sine at phase 0: the bin's value for amplitude a is -i * a * N / 2.
        const auto value = std::polar (magnitudes[(size_t) k] * (float) frameSize * 0.5f,
                                       phase - juce::MathConstants<float>::halfPi);
        spectrum[(size_t) k + 1] = value;
        spectrum[(size_t) (frameSize - k - 1)] = std::conj (value);
    }
    fft.perform (spectrum.data(), time.data(), true);
    std::vector<float> frame ((size_t) frameSize);
    for (int i = 0; i < frameSize; ++i)
        frame[(size_t) i] = time[(size_t) i].real();
    return frame;
}

std::vector<float> WavetableDoc::renderFormula (const juce::String& text, double framePosition, int frameIndex,
                                                juce::String* error)
{
    std::vector<float> frame ((size_t) frameSize, 0.0f);
    Formula formula;
    const auto problem = formula.parse (text);
    if (error != nullptr)
        *error = problem;
    if (! formula.isValid())
        return frame;
    Formula::Variables variables;
    variables.f = framePosition;
    variables.n = frameIndex;
    for (int i = 0; i < frameSize; ++i)
    {
        variables.x = (double) i / (double) frameSize;
        frame[(size_t) i] = (float) juce::jlimit (-4.0, 4.0, formula.evaluate (variables));
    }
    return frame;
}

void WavetableDoc::analyse (const std::vector<float>& frame, std::vector<float>& magnitudes,
                            std::vector<float>& phases, int count)
{
    juce::dsp::FFT fft (11);
    std::vector<Complex> time ((size_t) frameSize), spectrum ((size_t) frameSize);
    for (int i = 0; i < frameSize; ++i)
        time[(size_t) i] = Complex (i < (int) frame.size() ? frame[(size_t) i] : 0.0f, 0.0f);
    fft.perform (time.data(), spectrum.data(), false);
    count = juce::jlimit (1, Wavetable::numHarmonics - 1, count);
    magnitudes.assign ((size_t) count, 0.0f);
    phases.assign ((size_t) count, 0.0f);
    for (int k = 0; k < count; ++k)
    {
        const auto bin = spectrum[(size_t) k + 1];
        magnitudes[(size_t) k] = std::abs (bin) * 2.0f / (float) frameSize;
        phases[(size_t) k] = magnitudes[(size_t) k] > 1.0e-7f ? std::arg (bin) + juce::MathConstants<float>::halfPi : 0.0f;
    }
    // Trailing silence costs nothing to store.
    auto last = count;
    while (last > 1 && magnitudes[(size_t) last - 1] < 1.0e-6f)
        --last;
    magnitudes.resize ((size_t) last);
    phases.resize ((size_t) last);
}

WavetableDoc WavetableDoc::fromFactory (int index)
{
    const auto names = TableFactory::getFactoryTableNames();
    index = juce::jlimit (0, TableFactory::getNumFactoryTables() - 1, index);
    auto doc = fromFrames (names[index], TableFactory::generate (index));
    doc.factoryIndex = index;
    return doc;
}

WavetableDoc WavetableDoc::fromFrames (const juce::String& name, std::vector<std::vector<float>> frames)
{
    WavetableDoc doc;
    doc.name = name;
    if (frames.size() > (size_t) maxFrames)
        frames.resize ((size_t) maxFrames);
    for (auto& frame : frames)
        frame.resize ((size_t) frameSize, 0.0f);
    doc.frames = std::move (frames);
    doc.recipes.assign (doc.frames.size(), {});
    return doc;
}

WavetableDoc WavetableDoc::sine()
{
    WavetableDoc doc;
    doc.name = "Sine";
    FrameRecipe recipe;
    recipe.kind = FrameRecipe::Kind::Harmonics;
    recipe.magnitudes = { 1.0f };
    recipe.phases = { 0.0f };
    doc.frames.push_back (renderHarmonics (recipe.magnitudes, recipe.phases));
    doc.recipes.push_back (recipe);
    return doc;
}

juce::ValueTree WavetableDoc::recipeTree() const
{
    juce::ValueTree tree ("Recipe");
    if (factoryIndex >= 0)
    {
        tree.setProperty ("factory", factoryIndex, nullptr);
        return tree;
    }
    tree.setProperty ("frames", (int) frames.size(), nullptr);
    for (size_t i = 0; i < frames.size(); ++i)
        tree.appendChild (frameRecipeTree (i < recipes.size() ? recipes[i] : FrameRecipe {}), nullptr);
    return tree;
}

bool WavetableDoc::applyRecipeTree (const juce::ValueTree& tree)
{
    if (! tree.hasType ("Recipe"))
        return false;
    if (tree.hasProperty ("factory"))
    {
        const auto keepName = name;
        *this = fromFactory ((int) tree.getProperty ("factory"));
        if (keepName.isNotEmpty())
            name = keepName;
        return true;
    }
    const auto count = juce::jlimit (0, maxFrames, tree.getNumChildren());
    if (count == 0)
        return false;
    std::vector<FrameRecipe> loaded;
    for (int i = 0; i < count; ++i)
        loaded.push_back (frameRecipeFrom (tree.getChild (i)));
    // Raw frames need the frames from elsewhere (embedded data or a file).
    if (frames.size() != loaded.size())
    {
        for (const auto& recipe : loaded)
            if (recipe.kind == FrameRecipe::Kind::Raw)
                return false;
        frames.assign (loaded.size(), std::vector<float> ((size_t) frameSize, 0.0f));
    }
    recipes = std::move (loaded);
    factoryIndex = -1;
    renderAll();
    return true;
}

juce::String WavetableDoc::encodeFrames (const std::vector<std::vector<float>>& frames)
{
    auto peak = 0.0f;
    for (const auto& frame : frames)
        for (auto value : frame)
            peak = juce::jmax (peak, std::abs (value));
    const auto scale = peak > 0.0f ? 32767.0f / peak : 1.0f;

    juce::MemoryOutputStream raw;
    raw.writeInt ((int) frames.size());
    raw.writeFloat (peak);
    for (const auto& frame : frames)
        for (int i = 0; i < frameSize; ++i)
            raw.writeShort ((short) juce::roundToInt (juce::jlimit (-32767.0f, 32767.0f,
                                                                     (i < (int) frame.size() ? frame[(size_t) i] : 0.0f) * scale)));

    juce::MemoryOutputStream compressed;
    {
        juce::GZIPCompressorOutputStream zip (compressed, 9);
        zip.write (raw.getData(), raw.getDataSize());
    }
    return compressed.getMemoryBlock().toBase64Encoding();
}

bool WavetableDoc::decodeFrames (const juce::String& text, std::vector<std::vector<float>>& frames)
{
    juce::MemoryBlock compressed;
    if (text.isEmpty() || ! compressed.fromBase64Encoding (text))
        return false;
    juce::MemoryInputStream source (compressed, false);
    juce::GZIPDecompressorInputStream zip (source);
    juce::MemoryBlock raw;
    zip.readIntoMemoryBlock (raw);
    juce::MemoryInputStream in (raw, false);
    const auto count = in.readInt();
    const auto peak = in.readFloat();
    if (count < 1 || count > maxFrames || ! std::isfinite (peak)
        || (juce::int64) raw.getSize() < 8 + (juce::int64) count * frameSize * 2)
        return false;
    const auto scale = peak > 0.0f ? peak / 32767.0f : 1.0f / 32767.0f;
    frames.assign ((size_t) count, std::vector<float> ((size_t) frameSize));
    for (auto& frame : frames)
        for (auto& value : frame)
            value = (float) in.readShort() * scale;
    return true;
}

juce::ValueTree WavetableDoc::toValueTree() const
{
    juce::ValueTree tree ("Wavetable");
    tree.setProperty ("version", formatVersion, nullptr);
    tree.setProperty ("name", name, nullptr);
    tree.appendChild (recipeTree(), nullptr);
    if (! canRebuildExactly())
        tree.setProperty ("data", encodeFrames (frames), nullptr);
    if (sourcePath.isNotEmpty())
    {
        tree.setProperty ("path", sourcePath, nullptr);
        tree.setProperty ("mode", sourceMode, nullptr);
    }
    return tree;
}

bool WavetableDoc::fromValueTree (const juce::ValueTree& tree, WavetableDoc& doc, juce::String& notice)
{
    doc = {};
    doc.name = tree.getProperty ("name").toString();
    doc.sourcePath = tree.getProperty ("path").toString();
    doc.sourceMode = (int) tree.getProperty ("mode", 0);
    if ((int) tree.getProperty ("version", 1) > formatVersion)
        notice = "\"" + doc.name + "\" was saved by a newer ilanaSynth; some of it may be missing.";

    const auto recipe = tree.getChildWithName ("Recipe");
    std::vector<std::vector<float>> embedded;
    const auto hasData = decodeFrames (tree.getProperty ("data").toString(), embedded);

    // 1. The recipe, with the embedded frames for any Raw frames.
    if (recipe.isValid())
    {
        WavetableDoc attempt;
        attempt.name = doc.name;
        if (hasData)
            attempt.frames = embedded;
        if (attempt.applyRecipeTree (recipe))
        {
            attempt.sourcePath = doc.sourcePath;
            attempt.sourceMode = doc.sourceMode;
            if (doc.name.isNotEmpty())
                attempt.name = doc.name;
            doc = std::move (attempt);
            return true;
        }
    }

    // 2. The embedded frames.
    if (hasData)
    {
        doc.frames = std::move (embedded);
        doc.recipes.assign (doc.frames.size(), {});
        return true;
    }

    // 3. The file.
    if (doc.sourcePath.isNotEmpty())
    {
        std::vector<std::vector<float>> frames;
        const juce::File file (doc.sourcePath);
        if (file.existsAsFile()
            && Wavetable::readFrames (file, (Wavetable::LoadMode) juce::jlimit (0, 4, doc.sourceMode), frames))
        {
            doc.frames = std::move (frames);
            doc.recipes.assign (doc.frames.size(), {});
            return true;
        }
    }

    notice = "The wavetable \"" + doc.name + "\" could not be restored; its slot is back to the default table.";
    return false;
}

bool WavetableDoc::exportWav (const juce::File& file) const
{
    if (frames.empty())
        return false;
    juce::MemoryOutputStream data;
    auto peak = 0.0f;
    for (const auto& frame : frames)
        for (auto value : frame)
            peak = juce::jmax (peak, std::abs (value));
    const auto scale = peak > 1.0f ? 1.0f / peak : 1.0f;
    for (const auto& frame : frames)
        for (int i = 0; i < frameSize; ++i)
            data.writeFloat ((i < (int) frame.size() ? frame[(size_t) i] : 0.0f) * scale);

    juce::MemoryOutputStream format;
    format.writeShort (3);          // IEEE float
    format.writeShort (1);          // mono
    format.writeInt (44100);
    format.writeInt (44100 * 4);
    format.writeShort (4);
    format.writeShort (32);

    const juce::String clmText ("<!>2048 00000000 wavetable (ilanaSynth)");
    const auto clmSize = (size_t) clmText.getNumBytesAsUTF8();

    juce::MemoryOutputStream body;
    body.write ("WAVE", 4);
    writeChunk (body, "fmt ", format.getData(), format.getDataSize());
    writeChunk (body, "clm ", clmText.toRawUTF8(), clmSize);
    writeChunk (body, "data", data.getData(), data.getDataSize());

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk())
        return false;
    out.write ("RIFF", 4);
    out.writeInt ((int) body.getDataSize());
    out.write (body.getData(), body.getDataSize());
    out.flush();
    return ! out.getStatus().failed();
}

juce::File WavetableDoc::getLibraryFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("ilanaSynth Wavetables");
}

bool WavetableDoc::saveToLibrary (const juce::String& fileName, juce::File* written) const
{
    auto folder = getLibraryFolder();
    if (! folder.createDirectory())
        return false;
    const auto safeName = juce::File::createLegalFileName (fileName.isNotEmpty() ? fileName : juce::String ("Wavetable"));
    const auto wav = folder.getChildFile (safeName + ".wav");
    if (! exportWav (wav))
        return false;
    auto recipe = recipeTree();
    recipe.setProperty ("name", name, nullptr);
    recipe.setProperty ("version", formatVersion, nullptr);
    if (auto xml = recipe.createXml())
        xml->writeTo (wav.withFileExtension ("ilwt"));
    if (written != nullptr)
        *written = wav;
    return true;
}

bool WavetableDoc::loadFromFile (const juce::File& file, WavetableDoc& doc, Wavetable::LoadMode mode)
{
    std::vector<std::vector<float>> frames;
    if (! Wavetable::readFrames (file, mode, frames))
        return false;
    doc = fromFrames (file.getFileNameWithoutExtension(), std::move (frames));
    doc.sourcePath = file.getFullPathName();
    doc.sourceMode = (int) mode;

    // A library table's sidecar makes it editable again.
    const auto sidecar = file.withFileExtension ("ilwt");
    if (sidecar.existsAsFile())
        if (auto xml = juce::XmlDocument::parse (sidecar))
        {
            auto attempt = doc;
            if (attempt.applyRecipeTree (juce::ValueTree::fromXml (*xml)))
                doc = std::move (attempt);
        }
    return true;
}
