// The Vulkan canvas and its juce::LowLevelGraphicsContext (see VulkanCanvas.h).
//
// The context reuses JUCE's RenderingHelpers exactly as the software renderer
// does: clip regions, transforms, paths, strokes, glyphs and image clips all
// become edge tables on the CPU. Where the software renderer would walk each
// edge table's spans and write pixels, this one records each span as a
// rectangle instance, with the colour or coverage scale the software renderer
// would use for that kind of span (EdgeTableFillers in juce_RenderingHelpers.h).
// The fragment shader repeats the software renderer's per-pixel maths for
// gradients and images. The frame is then replayed into one command buffer:
// image uploads first, then the draws, with each transparency layer drawn
// into its own texture and composited back as the software renderer does.

#include "VulkanCanvas.h"
// JUCE's rasteriser helpers (edge tables, clip regions, saved states), as
// juce_opengl uses them; not part of juce_graphics.h by default.
#include <juce_graphics/native/juce_RenderingHelpers.h>

#include <mutex>
#include <unordered_map>

namespace ilana::vulkan
{
namespace
{
// One edge-table span, drawn as a rectangle (matches ui.vert's inputs).
struct Instance
{
    int16_t x, y, w, h;
    uint32_t colour; // a juce PixelARGB's native value (premultiplied)
    uint32_t scale;  // coverage multiplier 0..256 (256: none)
};
static_assert (sizeof (Instance) == 16);

// Matches the push constant block of ui.frag.
struct PushConstants
{
    float targetSize[2] {};
    int32_t mode = 0;
    int32_t lutOffset = 0;
    int32_t lutEntries = 0;
    int32_t flags = 0;
    int32_t offset[2] {};
    float m[4] {};
    float n[4] {};
    float g[4] {};
    int32_t gi[4] {};
};
static_assert (sizeof (PushConstants) == 96);

enum Mode { solidMode = 0, linearMode = 1, radialMode = 2, transformedRadialMode = 3, imageMode = 4, transformedImageMode = 5 };
enum class SpanKind { pixel, pixelFull, line, lineFull };

uint32_t multipliedAlpha (juce::PixelARGB c, int alpha) // PixelARGB::multiplyAlpha
{
    c.multiplyAlpha (alpha);
    return c.getNativeARGB();
}

uint32_t blendScaled (juce::PixelARGB c, uint32_t extraAlpha) // the source side of PixelARGB::blend (src, extraAlpha)
{
    const auto even = (extraAlpha * (c.getNativeARGB() & 0x00ff00ffu)) >> 8;
    const auto odd = extraAlpha * ((c.getNativeARGB() >> 8) & 0x00ff00ffu);
    return (even & 0x00ff00ffu) | (odd & 0xff00ff00u);
}
} // namespace

//==============================================================================
// Textures for juce::Images, keyed by their pixel data and refreshed when the
// image changes. Shared by every canvas of the device.
struct ImageCache final : public juce::ImagePixelData::Listener
{
    explicit ImageCache (Device& d) : device (d) {}

    ~ImageCache() override
    {
        const std::scoped_lock sl (lock);
        for (auto& [pixels, entry] : entries)
        {
            pixels->listeners.remove (this);
            entry->texture.destroy (device);
        }
        for (auto& t : doomed)  t.destroy (device);
        for (auto& [size, t] : spare) t.destroy (device);
    }

    struct Entry
    {
        Texture texture;
        bool dirty = true;
        uint64_t uploadedInFrame = 0;
    };

    void imageDataChanged (juce::ImagePixelData* pixels) override
    {
        const std::scoped_lock sl (lock);
        if (auto it = entries.find (pixels); it != entries.end())
            it->second->dirty = true;
    }

    void imageDataBeingDeleted (juce::ImagePixelData* pixels) override
    {
        const std::scoped_lock sl (lock);
        if (auto it = entries.find (pixels); it != entries.end())
        {
            doomed.push_back (it->second->texture);
            entries.erase (it);
        }
    }

    // A texture of exactly this size: a spare one, or a new one.
    bool takeTexture (Texture& out, int w, int h)
    {
        if (auto it = spare.find ({ w, h }); it != spare.end())
        {
            out = it->second;
            spareBytes -= (size_t) w * (size_t) h * 4;
            spare.erase (it);
            return true;
        }
        return out.create (device, w, h, false);
    }

    void giveBack (Texture t)
    {
        const auto bytes = (size_t) t.width * (size_t) t.height * 4;
        if (spareBytes + bytes > maxSpareBytes)
        {
            t.destroy (device);
            return;
        }
        spareBytes += bytes;
        spare.emplace (std::pair { t.width, t.height }, t);
    }

    // Called once the GPU has finished every earlier frame: textures of
    // images deleted since become spares.
    void collect()
    {
        std::vector<Texture> done;
        {
            const std::scoped_lock sl (lock);
            done.swap (doomed);
        }
        for (auto& t : done)
            giveBack (t);
    }

    bool hasDoomed()
    {
        const std::scoped_lock sl (lock);
        return ! doomed.empty();
    }

    Device& device;
    std::mutex lock; // the listener calls can come from any thread
    std::unordered_map<juce::ImagePixelData*, std::unique_ptr<Entry>> entries;
    std::vector<Texture> doomed;
    std::multimap<std::pair<int, int>, Texture> spare;
    size_t spareBytes = 0;
    static constexpr size_t maxSpareBytes = 64 * 1024 * 1024;
};

void Device::destroyCachedTextures()
{
    delete imageCache;
    imageCache = nullptr;
}

//==============================================================================
// Per-canvas GPU resources for one frame in flight.
struct FrameResources
{
    explicit FrameResources (Device& d) : device (d) {}

    ~FrameResources()
    {
        if (fence != VK_NULL_HANDLE)
        {
            vkWaitForFences (device.device, 1, &fence, VK_TRUE, UINT64_MAX);
            vkDestroyFence (device.device, fence, nullptr);
        }
        releaseFrameTextures();
        if (cmd != VK_NULL_HANDLE)       vkFreeCommandBuffers (device.device, device.commandPool, 1, &cmd);
        if (descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool (device.device, descriptorPool, nullptr);
        instances.destroy (device);
        lut.destroy (device);
        staging.destroy (device);
        for (auto& [size, t] : layerPool) t.destroy (device);
        dummy.destroy (device);
    }

    bool init()
    {
        VkCommandBufferAllocateInfo info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        info.commandPool = device.commandPool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;
        if (vkAllocateCommandBuffers (device.device, &info, &cmd) != VK_SUCCESS)
            return false;

        VkFenceCreateInfo fenceInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (vkCreateFence (device.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS)
            return false;

        // A 1x1 transparent texture stands in for "no image" in the descriptor.
        if (! dummy.create (device, 1, 1, false))
            return false;
        dummyNeedsClear = true;
        return true;
    }

    void wait()
    {
        vkWaitForFences (device.device, 1, &fence, VK_TRUE, UINT64_MAX);
        releaseFrameTextures();
    }

    bool ensure (Buffer& b, VkDeviceSize bytes, VkBufferUsageFlags usage)
    {
        bytes = juce::jmax ((VkDeviceSize) 256, bytes);
        if (b.size >= bytes)
            return true;
        const auto grown = juce::jmax (bytes, b.size * 2);
        b.destroy (device);
        return b.create (device, grown, usage, true);
    }

    // A render target at least this big, kept for later frames.
    Texture* takeLayer (int w, int h)
    {
        const int pw = (w + 63) & ~63, ph = (h + 63) & ~63;
        for (auto it = layerPool.begin(); it != layerPool.end(); ++it)
        {
            if (it->first.first == pw && it->first.second == ph)
            {
                layersInUse.push_back (std::make_unique<Texture> (it->second));
                layerPool.erase (it);
                return layersInUse.back().get();
            }
        }
        auto t = std::make_unique<Texture>();
        if (! t->create (device, pw, ph, true))
            return nullptr;
        layersInUse.push_back (std::move (t));
        return layersInUse.back().get();
    }

    void releaseFrameTextures()
    {
        for (auto& t : layersInUse)
        {
            if (layerPool.size() < 16)
                layerPool.emplace (std::pair { t->width, t->height }, *t);
            else
                t->destroy (device);
        }
        layersInUse.clear();

        if (device.imageCache != nullptr)
            for (auto& t : transients)
                device.imageCache->giveBack (t);
        transients.clear();
    }

    Device& device;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    uint32_t descriptorCapacity = 0;
    Buffer instances, lut, staging;
    Texture dummy;
    bool dummyNeedsClear = false;
    std::multimap<std::pair<int, int>, Texture> layerPool;
    std::vector<std::unique_ptr<Texture>> layersInUse;
    std::vector<Texture> transients; // image textures only valid for this frame
};

//==============================================================================
namespace
{
uint64_t& frameCounter()
{
    static uint64_t frame = 0;
    return frame;
}

struct Command
{
    enum Kind { draw, beginLayer, endLayer } kind = draw;
    bool replace = false;
    int view = -1; // index into Recorder::views, -1 for none
    PushConstants pc;
    uint32_t first = 0, count = 0;
    Texture* layer = nullptr;
};

struct Upload
{
    Texture* texture;
    size_t offset;
};

// Everything one paint records before it is replayed into a command buffer.
struct Recorder
{
    Recorder (Device& d, FrameResources& f, Texture& canvas) : device (d), frame (f)
    {
        targets.push_back (&canvas);
    }

    Device& device;
    FrameResources& frame;
    std::vector<Instance> instances;
    std::vector<uint32_t> lut;
    std::vector<uint8_t> staging;
    std::vector<Upload> uploads;
    std::vector<Command> commands;
    std::vector<VkImageView> views;
    std::vector<Texture*> targets; // the canvas, then each open layer
    bool failed = false;

    int viewIndex (VkImageView view)
    {
        for (size_t i = 0; i < views.size(); ++i)
            if (views[i] == view)
                return (int) i;
        views.push_back (view);
        return (int) views.size() - 1;
    }

    void addSpan (int x, int y, int w, int h, uint32_t colour, uint32_t scale)
    {
        if (w <= 0 || h <= 0)
            return;

        // Rectangles fill row by row: grow the last instance down when the
        // next row is the same span.
        if (! instances.empty() && openBatch)
        {
            auto& last = instances.back();
            if (last.x == x && last.w == w && last.colour == colour && last.scale == scale && last.y + last.h == y && last.h + h < 32767)
            {
                last.h = (int16_t) (last.h + h);
                return;
            }
        }

        instances.push_back ({ (int16_t) x, (int16_t) y, (int16_t) w, (int16_t) h, colour, scale });
        openBatch = true;
    }

    // Starts a draw whose spans follow; spans merge into the previous draw
    // when nothing about it differs.
    void beginDraw (bool replace, int view, PushConstants pc)
    {
        const auto* target = targets.back();
        if (! replace)
            pc.flags |= 4;
        pc.targetSize[0] = (float) target->width;
        pc.targetSize[1] = (float) target->height;

        if (! commands.empty())
        {
            auto& last = commands.back();
            if (last.kind == Command::draw && last.replace == replace && last.view == view
                && std::memcmp (&last.pc, &pc, sizeof (pc)) == 0
                && last.first + last.count == (uint32_t) instances.size())
                return;
        }

        Command c;
        c.kind = Command::draw;
        c.replace = replace;
        c.view = view;
        c.pc = pc;
        c.first = (uint32_t) instances.size();
        commands.push_back (c);
        openBatch = false;
    }

    void endDraw()
    {
        auto& last = commands.back();
        last.count = (uint32_t) instances.size() - last.first;
    }

    Texture* beginLayer (int w, int h)
    {
        auto* layer = frame.takeLayer (w, h);
        if (layer == nullptr)
        {
            failed = true;
            return nullptr;
        }
        Command c;
        c.kind = Command::beginLayer;
        c.layer = layer;
        commands.push_back (c);
        targets.push_back (layer);
        openBatch = false;
        return layer;
    }

    void endLayer (Texture* layer)
    {
        jassert (targets.back() == layer);
        Command c;
        c.kind = Command::endLayer;
        c.layer = layer;
        commands.push_back (c);
        targets.pop_back();
        openBatch = false;
    }

    // The texture for a juce::Image, uploaded this frame if it is new or
    // has changed. -1 if it couldn't be made.
    int imageView (const juce::Image& image)
    {
        auto pixels = image.getPixelData();
        if (pixels == nullptr || image.getWidth() <= 0 || image.getHeight() <= 0)
            return -1;

        if (device.imageCache == nullptr)
            device.imageCache = new ImageCache (device);

        auto& cache = *device.imageCache;
        const auto frameNumber = frameCounter();
        Texture* texture = nullptr;
        bool upload = false;
        {
            const std::scoped_lock sl (cache.lock);
            auto it = cache.entries.find (pixels.get());

            if (it == cache.entries.end())
            {
                auto entry = std::make_unique<ImageCache::Entry>();
                if (! cache.takeTexture (entry->texture, image.getWidth(), image.getHeight()))
                    return -1;
                it = cache.entries.emplace (pixels.get(), std::move (entry)).first;
                pixels->listeners.add (&cache);
            }

            auto& entry = *it->second;

            if (entry.dirty && entry.uploadedInFrame == frameNumber)
            {
                // Changed again after being drawn in this frame: the earlier
                // draws keep the picture they saw, so this version gets a
                // texture of its own for the rest of the frame.
                Texture t;
                if (! cache.takeTexture (t, image.getWidth(), image.getHeight()))
                    return -1;
                frame.transients.push_back (t);
                texture = &frame.transients.back();
                entry.dirty = false; // the transient holds the new picture; mark the entry stale for the next frame
                transientStale.push_back (pixels.get());
                upload = true;
            }
            else
            {
                texture = &entry.texture;
                upload = entry.dirty;
                entry.dirty = false;
                if (upload)
                    entry.uploadedInFrame = frameNumber;
            }
        }

        if (upload)
            stageImage (image, *texture);

        return viewIndex (texture->view);
    }

    void stageImage (const juce::Image& image, Texture& texture)
    {
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);
        const auto w = image.getWidth(), h = image.getHeight();
        const auto offset = (staging.size() + 15) & ~(size_t) 15;
        staging.resize (offset + (size_t) w * (size_t) h * 4);
        auto* out = reinterpret_cast<uint32_t*> (staging.data() + offset);

        for (int y = 0; y < h; ++y)
        {
            const auto* line = data.getLinePointer (y);
            auto* row = out + (size_t) y * (size_t) w;

            switch (data.pixelFormat)
            {
                case juce::Image::ARGB:
                    for (int x = 0; x < w; ++x)
                        row[x] = reinterpret_cast<const juce::PixelARGB*> (line + x * data.pixelStride)->getNativeARGB();
                    break;
                case juce::Image::RGB:
                    for (int x = 0; x < w; ++x)
                    {
                        const auto* p = reinterpret_cast<const juce::PixelRGB*> (line + x * data.pixelStride);
                        row[x] = 0xff000000u | ((uint32_t) p->getRed() << 16) | ((uint32_t) p->getGreen() << 8) | p->getBlue();
                    }
                    break;
                case juce::Image::SingleChannel:
                case juce::Image::UnknownFormat:
                default:
                    for (int x = 0; x < w; ++x)
                    {
                        const uint32_t a = line[x * data.pixelStride];
                        row[x] = a * 0x01010101u;
                    }
                    break;
            }
        }

        uploads.push_back ({ &texture, offset });
    }

    // Images whose newest picture only went into a transient texture.
    std::vector<juce::ImagePixelData*> transientStale;
    bool openBatch = false;
};

// Turns edge-table callbacks into span instances. Rule (kind, alpha) gives
// the colour and coverage scale the software renderer would use.
template <typename Rule>
struct SpanCollector
{
    Recorder& recorder;
    Rule rule;
    int currentY = 0;

    void setEdgeTableYPos (int y) noexcept                      { currentY = y; }
    void handleEdgeTablePixel (int x, int alpha) noexcept       { add (x, currentY, 1, 1, SpanKind::pixel, alpha); }
    void handleEdgeTablePixelFull (int x) noexcept              { add (x, currentY, 1, 1, SpanKind::pixelFull, 255); }
    void handleEdgeTableLine (int x, int w, int alpha) noexcept { add (x, currentY, w, 1, SpanKind::line, alpha); }
    void handleEdgeTableLineFull (int x, int w) noexcept        { add (x, currentY, w, 1, SpanKind::lineFull, 255); }
    void handleEdgeTableRectangle (int x, int y, int w, int h, int alpha) noexcept { add (x, y, w, h, SpanKind::line, alpha); }
    void handleEdgeTableRectangleFull (int x, int y, int w, int h) noexcept        { add (x, y, w, h, SpanKind::lineFull, 255); }

    void add (int x, int y, int w, int h, SpanKind kind, int alpha)
    {
        const auto [colour, scale] = rule (kind, alpha);
        recorder.addSpan (x, y, w, h, colour, scale);
    }
};

template <typename Rule>
SpanCollector<Rule> makeCollector (Recorder& r, Rule rule) { return { r, rule }; }

//==============================================================================
struct SavedState final : public juce::RenderingHelpers::SavedStateBase<SavedState>
{
    using BaseClass = juce::RenderingHelpers::SavedStateBase<SavedState>;

    SavedState (Recorder& r, juce::Rectangle<int> bounds, const juce::RectangleList<int>& clipList)
        : BaseClass (clipList, {}), recorder (&r), targetBounds (bounds)
    {
    }

    SavedState (const SavedState&) = default;

    std::unique_ptr<SavedState> beginTransparencyLayer (float opacity)
    {
        auto s = std::make_unique<SavedState> (*this);

        if (clip != nullptr)
        {
            // As SoftwareRendererSavedState: the layer is the clip's bounds,
            // drawn in its own coordinates.
            auto layerBounds = clip->getClipBounds();
            s->layer = recorder->beginLayer (layerBounds.getWidth(), layerBounds.getHeight());
            s->targetBounds = { layerBounds.getWidth(), layerBounds.getHeight() };
            s->transparencyLayerAlpha = opacity;
            s->transform.moveOriginInDeviceSpace (-layerBounds.getPosition());
            s->cloneClipIfMultiplyReferenced();
            s->clip->translate (-layerBounds.getPosition());
        }

        return s;
    }

    void endTransparencyLayer (SavedState& finishedLayerState)
    {
        if (clip != nullptr && finishedLayerState.layer != nullptr)
        {
            recorder->endLayer (finishedLayerState.layer);

            // As SoftwareRendererSavedState::endTransparencyLayer: a fresh
            // context at the layer's opacity draws the layer at its bounds,
            // which is an untransformed image fill of the layer's area.
            const auto layerBounds = clip->getClipBounds();
            const auto alpha = juce::Colours::black.withAlpha (finishedLayerState.transparencyLayerAlpha).getAlpha();
            const auto area = juce::Rectangle<int> (layerBounds.getX(), layerBounds.getY(),
                                                    layerBounds.getWidth(), layerBounds.getHeight()).getIntersection (targetBounds);
            if (! area.isEmpty())
            {
                juce::EdgeTable et (area);
                drawTextureUntransformed (et, recorder->viewIndex (finishedLayerState.layer->view),
                                          alpha, layerBounds.getX(), layerBounds.getY(), false);
            }
        }
    }

    juce::Rectangle<int> getMaximumBounds() const { return targetBounds; }

    //==============================================================================
    template <typename IteratorType>
    void fillWithSolidColour (IteratorType& iter, juce::PixelARGB colour, bool replaceContents) const
    {
        recorder->beginDraw (replaceContents, -1, {});

        if (replaceContents)
        {
            auto collector = makeCollector (*recorder, [colour] (SpanKind kind, int alpha) -> std::pair<uint32_t, uint32_t>
            {
                // SolidColour<replaceExisting = true>: single pixels take the
                // colour, lines the colour times their coverage.
                if (kind == SpanKind::line)
                    return { multipliedAlpha (colour, alpha), 256u };
                return { colour.getNativeARGB(), 256u };
            });
            iter.iterate (collector);
        }
        else
        {
            auto collector = makeCollector (*recorder, [colour] (SpanKind kind, int alpha) -> std::pair<uint32_t, uint32_t>
            {
                switch (kind)
                {
                    case SpanKind::pixel: return { blendScaled (colour, (uint32_t) alpha), 256u };
                    case SpanKind::line:  return { multipliedAlpha (colour, alpha), 256u };
                    default:              return { colour.getNativeARGB(), 256u };
                }
            });
            iter.iterate (collector);
        }

        recorder->endDraw();
    }

    template <typename IteratorType>
    void fillWithGradient (IteratorType& iter, juce::ColourGradient& gradient, const juce::AffineTransform& trans, bool isIdentity) const
    {
        juce::HeapBlock<juce::PixelARGB> table;
        const auto tableSize = gradient.createLookupTable (trans, table);
        if (tableSize <= 0)
            return;

        // EdgeTableFillers::Gradient hands its pixel iterator one entry
        // fewer than the table holds; indexes run 0..numEntries.
        const auto numEntries = tableSize - 1;
        PushConstants pc;
        pc.lutOffset = (int32_t) recorder->lut.size();
        pc.lutEntries = numEntries;
        for (int i = 0; i < tableSize; ++i)
            recorder->lut.push_back (table[i].getNativeARGB());

        if (gradient.isRadial)
        {
            // GradientPixelIterators::Radial / TransformedRadial
            const auto diff = gradient.point1 - gradient.point2;
            const double maxDist = diff.x * diff.x + diff.y * diff.y;
            const double invScale = numEntries / std::sqrt (maxDist);
            pc.g[0] = gradient.point1.x;
            pc.g[1] = gradient.point1.y;
            pc.g[2] = (float) maxDist;
            pc.g[3] = (float) invScale;

            if (isIdentity)
            {
                pc.mode = radialMode;
            }
            else
            {
                pc.mode = transformedRadialMode;
                const auto inv = trans.inverted();
                pc.m[0] = inv.mat00; pc.m[1] = inv.mat01; pc.m[2] = inv.mat02; pc.m[3] = inv.mat10;
                pc.n[0] = inv.mat11; pc.n[1] = inv.mat12;
            }
        }
        else
        {
            // GradientPixelIterators::Linear, with its fixed-point set-up.
            pc.mode = linearMode;
            auto p1 = gradient.point1;
            auto p2 = gradient.point2;

            if (! trans.isIdentity())
            {
                auto p3 = juce::Line<float> (p2, p1).getPointAlongLine (0.0f, 100.0f);
                p1.applyTransform (trans);
                p2.applyTransform (trans);
                p3.applyTransform (trans);
                p2 = juce::Line<float> (p2, p3).findNearestPointTo (p1);
            }

            constexpr int numScaleBits = 12;
            const bool vertical = std::abs (p1.x - p2.x) < 0.001f;
            const bool horizontal = std::abs (p1.y - p2.y) < 0.001f;
            pc.gi[0] = vertical ? 1 : (horizontal ? 2 : 0);

            if (vertical)
            {
                const auto scale = juce::roundToInt ((double) ((int64_t) numEntries << numScaleBits) / (double) (p2.y - p1.y));
                pc.gi[1] = scale;
                pc.gi[2] = juce::roundToInt (p1.y * (float) scale);
            }
            else if (horizontal)
            {
                const auto scale = juce::roundToInt ((double) ((int64_t) numEntries << numScaleBits) / (double) (p2.x - p1.x));
                pc.gi[1] = scale;
                pc.gi[2] = juce::roundToInt (p1.x * (float) scale);
            }
            else
            {
                auto grad = (p2.y - p1.y) / (double) (p1.x - p2.x);
                const auto yTerm = p1.y - (p1.x / grad);
                const auto scale = juce::roundToInt ((double) ((int64_t) numEntries << numScaleBits) / (yTerm * grad - (p2.y * grad - p2.x)));
                grad *= scale;
                pc.gi[1] = scale;
                // start = roundToInt ((y - yTerm) * grad), as y * grad - yTerm * grad
                pc.g[0] = (float) grad;
                pc.g[1] = (float) (yTerm * grad);
            }
        }

        recorder->beginDraw (false, -1, pc);
        auto collector = makeCollector (*recorder, [] (SpanKind kind, int alpha) -> std::pair<uint32_t, uint32_t>
        {
            // EdgeTableFillers::Gradient: blend (pixel, alpha) below full coverage.
            if ((kind == SpanKind::pixel || kind == SpanKind::line) && alpha < 255)
                return { 0u, (uint32_t) alpha };
            return { 0u, 256u };
        });
        iter.iterate (collector);
        recorder->endDraw();
    }

    template <typename IteratorType>
    void renderImageUntransformed (IteratorType& iter, const juce::Image& src, int alpha, int x, int y, bool tiledFill) const
    {
        const auto view = recorder->imageView (src);
        if (view < 0)
        {
            recorder->failed = true;
            return;
        }
        drawTextureUntransformed (iter, view, alpha, x, y, tiledFill);
    }

    template <typename IteratorType>
    void drawTextureUntransformed (IteratorType& iter, int view, int alpha, int x, int y, bool tiledFill) const
    {
        PushConstants pc;
        pc.mode = imageMode;
        pc.flags = tiledFill ? 1 : 0;
        pc.offset[0] = x;
        pc.offset[1] = y;
        recorder->beginDraw (false, view, pc);

        // EdgeTableFillers::ImageFill with extraAlpha = alpha + 1.
        const auto extraAlpha = (uint32_t) alpha + 1;
        auto collector = makeCollector (*recorder, [extraAlpha] (SpanKind kind, int level) -> std::pair<uint32_t, uint32_t>
        {
            switch (kind)
            {
                case SpanKind::pixel:     return { 0u, ((uint32_t) level * extraAlpha) >> 8 };
                case SpanKind::pixelFull: return { 0u, extraAlpha };
                case SpanKind::line:
                {
                    const auto a = ((uint32_t) level * extraAlpha) >> 8;
                    return { 0u, a < 0xfe ? a : 256u };
                }
                case SpanKind::lineFull:
                default:                  return { 0u, extraAlpha < 0xfe ? extraAlpha : 256u };
            }
        });
        iter.iterate (collector);
        recorder->endDraw();
    }

    template <typename IteratorType>
    void renderImageTransformed (IteratorType& iter, const juce::Image& src, int alpha, const juce::AffineTransform& trans,
                                 juce::Graphics::ResamplingQuality quality, bool tiledFill) const
    {
        const auto view = recorder->imageView (src);
        if (view < 0)
        {
            recorder->failed = true;
            return;
        }

        PushConstants pc;
        pc.mode = transformedImageMode;
        pc.flags = (tiledFill ? 1 : 0) | (quality != juce::Graphics::lowResamplingQuality ? 2 : 0);
        const auto inv = trans.inverted();
        pc.m[0] = inv.mat00; pc.m[1] = inv.mat01; pc.m[2] = inv.mat02; pc.m[3] = inv.mat10;
        pc.n[0] = inv.mat11; pc.n[1] = inv.mat12;
        recorder->beginDraw (false, view, pc);

        // EdgeTableFillers::TransformedImageFill with extraAlpha = alpha + 1.
        const auto extraAlpha = (uint32_t) alpha + 1;
        auto collector = makeCollector (*recorder, [extraAlpha] (SpanKind kind, int level) -> std::pair<uint32_t, uint32_t>
        {
            switch (kind)
            {
                case SpanKind::pixel:     return { 0u, ((uint32_t) level * extraAlpha) >> 8 };
                case SpanKind::pixelFull: return { 0u, extraAlpha };
                case SpanKind::line:
                case SpanKind::lineFull:
                default:
                {
                    const auto a = ((uint32_t) level * extraAlpha) >> 8;
                    return { 0u, a < 0xfe ? a : 256u };
                }
            }
        });
        iter.iterate (collector);
        recorder->endDraw();
    }

    juce::Font font { juce::FontOptions {} };
    Recorder* recorder;
    juce::Rectangle<int> targetBounds;
    Texture* layer = nullptr; // set on a transparency layer's state

    static void clearGlyphCache() { juce::RenderingHelpers::GlyphCache::getInstance().reset(); }
};

struct Context final : public juce::RenderingHelpers::StackBasedLowLevelGraphicsContext<SavedState>
{
    Context (Recorder& r, juce::Rectangle<int> bounds, const juce::RectangleList<int>& clipList)
        : StackBasedLowLevelGraphicsContext (std::make_unique<SavedState> (r, bounds, clipList))
    {
    }

    // Temporary images (shadows, effects, cached component pictures) are
    // drawn in software, as they are by the software renderer, then uploaded.
    std::unique_ptr<juce::ImageType> getPreferredImageTypeForTemporaryImages() const override
    {
        return std::make_unique<juce::SoftwareImageType>();
    }
};

//==============================================================================
bool replay (Device& device, FrameResources& frame, Recorder& rec, Texture& canvas,
             const juce::RectangleList<int>& dirty, const Canvas::SubmitHooks& hooks)
{
    // Buffers: instances, lookup tables and staged image pixels.
    if (! frame.ensure (frame.instances, rec.instances.size() * sizeof (Instance), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        || ! frame.ensure (frame.lut, rec.lut.size() * sizeof (uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        || ! frame.ensure (frame.staging, rec.staging.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
        return false;

    if (! rec.instances.empty()) std::memcpy (frame.instances.mapped, rec.instances.data(), rec.instances.size() * sizeof (Instance));
    if (! rec.lut.empty())       std::memcpy (frame.lut.mapped, rec.lut.data(), rec.lut.size() * sizeof (uint32_t));
    if (! rec.staging.empty())   std::memcpy (frame.staging.mapped, rec.staging.data(), rec.staging.size());

    // One descriptor set per texture this frame (plus the stand-in).
    rec.views.push_back (frame.dummy.view);
    const auto dummyIndex = (int) rec.views.size() - 1;
    const auto setCount = (uint32_t) rec.views.size();

    if (frame.descriptorPool == VK_NULL_HANDLE || frame.descriptorCapacity < setCount)
    {
        if (frame.descriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool (device.device, frame.descriptorPool, nullptr);
        frame.descriptorCapacity = juce::jmax (64u, setCount * 2);
        VkDescriptorPoolSize sizes[2] {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frame.descriptorCapacity },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frame.descriptorCapacity },
        };
        VkDescriptorPoolCreateInfo info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        info.maxSets = frame.descriptorCapacity;
        info.poolSizeCount = 2;
        info.pPoolSizes = sizes;
        if (vkCreateDescriptorPool (device.device, &info, nullptr, &frame.descriptorPool) != VK_SUCCESS)
        {
            frame.descriptorPool = VK_NULL_HANDLE;
            return false;
        }
    }
    vkResetDescriptorPool (device.device, frame.descriptorPool, 0);

    std::vector<VkDescriptorSetLayout> layouts (setCount, device.setLayout);
    std::vector<VkDescriptorSet> sets (setCount);
    VkDescriptorSetAllocateInfo allocInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = frame.descriptorPool;
    allocInfo.descriptorSetCount = setCount;
    allocInfo.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets (device.device, &allocInfo, sets.data()) != VK_SUCCESS)
        return false;

    std::vector<VkDescriptorImageInfo> imageInfos (setCount);
    VkDescriptorBufferInfo lutInfo { frame.lut.buffer, 0, VK_WHOLE_SIZE };
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve (setCount * 2);
    for (uint32_t i = 0; i < setCount; ++i)
    {
        imageInfos[i] = { device.nearestSampler, rec.views[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet w { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        w.dstSet = sets[i];
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &imageInfos[i];
        writes.push_back (w);
        w.dstBinding = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pImageInfo = nullptr;
        w.pBufferInfo = &lutInfo;
        writes.push_back (w);
    }
    vkUpdateDescriptorSets (device.device, (uint32_t) writes.size(), writes.data(), 0, nullptr);

    auto cmd = frame.cmd;
    vkResetCommandBuffer (cmd, 0);
    VkCommandBufferBeginInfo begin { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer (cmd, &begin);

    // Uploads, before any drawing.
    if (frame.dummyNeedsClear)
    {
        transition (cmd, frame.dummy, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue clear {};
        VkImageSubresourceRange range { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage (cmd, frame.dummy.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
        transition (cmd, frame.dummy, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        frame.dummyNeedsClear = false;
    }

    for (const auto& upload : rec.uploads)
    {
        auto& t = *upload.texture;
        transition (cmd, t.image, t.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ? t.layout : VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy region {};
        region.bufferOffset = upload.offset;
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent = { (uint32_t) t.width, (uint32_t) t.height, 1 };
        vkCmdCopyBufferToImage (cmd, frame.staging.buffer, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        transition (cmd, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        t.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Image textures shared through the cache keep their layout on the
    // entry's copy; the transient and cached Texture structs here are the
    // ones that were uploaded, so their layouts are now SHADER_READ.

    transition (cmd, canvas, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkBuffer vertexBuffer = frame.instances.buffer;
    VkDeviceSize zero = 0;

    const auto beginPass = [&] (Texture& target, bool clear)
    {
        VkClearValue clearValue {};
        VkRenderPassBeginInfo info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        info.renderPass = clear ? device.clearRenderPass : device.renderPass;
        info.framebuffer = target.framebuffer;
        info.renderArea = { { 0, 0 }, { (uint32_t) target.width, (uint32_t) target.height } };
        info.clearValueCount = clear ? 1 : 0;
        info.pClearValues = clear ? &clearValue : nullptr;
        vkCmdBeginRenderPass (cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
        target.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkViewport viewport { 0, 0, (float) target.width, (float) target.height, 0, 1 };
        VkRect2D scissor { { 0, 0 }, { (uint32_t) target.width, (uint32_t) target.height } };
        vkCmdSetViewport (cmd, 0, 1, &viewport);
        vkCmdSetScissor (cmd, 0, 1, &scissor);
        vkCmdBindVertexBuffers (cmd, 0, 1, &vertexBuffer, &zero);
    };

    std::vector<Texture*> stack { &canvas };
    beginPass (canvas, false);

    // The dirty area starts transparent, as a software repaint's would.
    {
        std::vector<VkClearRect> rects;
        for (const auto& r : dirty)
            rects.push_back ({ { { r.getX(), r.getY() }, { (uint32_t) r.getWidth(), (uint32_t) r.getHeight() } }, 0, 1 });
        if (! rects.empty())
        {
            VkClearAttachment attachment {};
            attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            attachment.colorAttachment = 0;
            vkCmdClearAttachments (cmd, 1, &attachment, (uint32_t) rects.size(), rects.data());
        }
    }

    VkPipeline boundPipeline = VK_NULL_HANDLE;
    int boundSet = -2;

    for (const auto& c : rec.commands)
    {
        switch (c.kind)
        {
            case Command::beginLayer:
                vkCmdEndRenderPass (cmd);
                stack.push_back (c.layer);
                beginPass (*c.layer, true);
                boundPipeline = VK_NULL_HANDLE;
                boundSet = -2;
                break;

            case Command::endLayer:
                vkCmdEndRenderPass (cmd);
                transition (cmd, *c.layer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                stack.pop_back();
                // The parent stays in COLOR_ATTACHMENT_OPTIMAL between passes.
                stack.back()->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                beginPass (*stack.back(), false);
                boundPipeline = VK_NULL_HANDLE;
                boundSet = -2;
                break;

            case Command::draw:
            default:
            {
                if (c.count == 0)
                    break;
                const auto pipeline = c.replace ? device.replacePipeline : device.blendPipeline;
                if (pipeline != boundPipeline)
                {
                    vkCmdBindPipeline (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                    boundPipeline = pipeline;
                    boundSet = -2;
                }
                const auto setIndex = c.view >= 0 ? c.view : dummyIndex;
                if (setIndex != boundSet)
                {
                    vkCmdBindDescriptorSets (cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, device.pipelineLayout, 0, 1, &sets[(size_t) setIndex], 0, nullptr);
                    boundSet = setIndex;
                }
                vkCmdPushConstants (cmd, device.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                    0, sizeof (PushConstants), &c.pc);
                vkCmdDraw (cmd, 6, c.count, 0, c.first);
                break;
            }
        }
    }

    vkCmdEndRenderPass (cmd);
    jassert (stack.size() == 1);

    transition (cmd, canvas, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    if (hooks.afterPaint != nullptr)
        hooks.afterPaint (cmd, canvas.image);

    if (vkEndCommandBuffer (cmd) != VK_SUCCESS)
        return false;

    VkSubmitInfo submit { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    if (hooks.wait != VK_NULL_HANDLE)
    {
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &hooks.wait;
        submit.pWaitDstStageMask = &hooks.waitStage;
    }
    if (hooks.signal != VK_NULL_HANDLE)
    {
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &hooks.signal;
    }

    vkResetFences (device.device, 1, &frame.fence);
    if (vkQueueSubmit (device.queue, 1, &submit, frame.fence) == VK_SUCCESS)
        return true;
    // A failed submit leaves the fence unsignalled, and every later wait on
    // it (the fallback's waitIdle, the frame's destructor) waits forever: an
    // empty submit signals it once the queue's earlier work is done.
    vkQueueSubmit (device.queue, 0, nullptr, frame.fence);
    return false;
}
} // namespace

//==============================================================================
Canvas::Canvas (std::shared_ptr<Device> d) : device (std::move (d))
{
    if (device != nullptr)
    {
        frame = std::make_unique<FrameResources> (*device);
        if (! frame->init())
            frame.reset();
    }
}

Canvas::~Canvas()
{
    if (device != nullptr)
    {
        frame.reset(); // waits for its fence
        target.destroy (*device);
    }
}

bool Canvas::resize (int width, int height)
{
    if (device == nullptr || frame == nullptr || width <= 0 || height <= 0)
        return false;
    if (width == target.width && height == target.height && target.image != VK_NULL_HANDLE)
        return true;

    frame->wait();
    target.destroy (*device);
    return target.create (*device, width, height, true);
}

void Canvas::waitIdle()
{
    if (frame != nullptr)
        frame->wait();
}

bool Canvas::paint (const juce::RectangleList<int>& dirtyIn,
                    const std::function<void (juce::Graphics&)>& draw,
                    const SubmitHooks& hooks)
{
    if (device == nullptr || frame == nullptr || target.image == VK_NULL_HANDLE)
        return false;

    frame->wait();

    // Textures of images deleted since an earlier frame can be reused once
    // every frame that might draw them has finished.
    if (device->imageCache != nullptr && device->imageCache->hasDoomed())
    {
        vkQueueWaitIdle (device->queue);
        device->imageCache->collect();
    }

    ++frameCounter();

    auto dirty = dirtyIn;
    dirty.clipTo (juce::Rectangle<int> (target.width, target.height));

    Recorder recorder (*device, *frame, target);

    if (! dirty.isEmpty() && draw != nullptr)
    {
        Context context (recorder, { target.width, target.height }, dirty);
        juce::Graphics g (context);
        draw (g);
    }

    // An image whose newest picture only went into a transient texture
    // must upload again next time.
    if (! recorder.transientStale.empty() && device->imageCache != nullptr)
    {
        const std::scoped_lock sl (device->imageCache->lock);
        for (auto* pixels : recorder.transientStale)
            if (auto it = device->imageCache->entries.find (pixels); it != device->imageCache->entries.end())
                it->second->dirty = true;
    }

    if (recorder.failed)
        DBG ("Vulkan UI: a texture or layer couldn't be made; parts of this frame are missing");

    return replay (*device, *frame, recorder, target, dirty, hooks);
}

juce::Image Canvas::readback()
{
    if (device == nullptr || frame == nullptr || target.image == VK_NULL_HANDLE)
        return {};

    frame->wait();

    Buffer buffer;
    const auto bytes = (VkDeviceSize) target.width * (VkDeviceSize) target.height * 4;
    if (! buffer.create (*device, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true))
        return {};

    auto cmd = frame->cmd;
    vkResetCommandBuffer (cmd, 0);
    VkCommandBufferBeginInfo begin { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer (cmd, &begin);

    if (target.layout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        transition (cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue clear {};
        VkImageSubresourceRange range { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage (cmd, target.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
    }
    transition (cmd, target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkBufferImageCopy region {};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { (uint32_t) target.width, (uint32_t) target.height, 1 };
    vkCmdCopyImageToBuffer (cmd, target.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer.buffer, 1, &region);

    VkBufferMemoryBarrier barrier { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer.buffer;
    barrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier (cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    vkEndCommandBuffer (cmd);

    VkSubmitInfo submit { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkResetFences (device->device, 1, &frame->fence);
    if (vkQueueSubmit (device->queue, 1, &submit, frame->fence) != VK_SUCCESS)
    {
        vkQueueSubmit (device->queue, 0, nullptr, frame->fence); // (signalled again: see replay)
        buffer.destroy (*device);
        return {};
    }
    vkWaitForFences (device->device, 1, &frame->fence, VK_TRUE, UINT64_MAX);

    juce::Image image (juce::Image::ARGB, target.width, target.height, false, juce::SoftwareImageType());
    {
        juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);
        const auto* src = static_cast<const uint8_t*> (buffer.mapped);
        for (int y = 0; y < target.height; ++y)
            std::memcpy (data.getLinePointer (y), src + (size_t) y * (size_t) target.width * 4, (size_t) target.width * 4);
    }

    buffer.destroy (*device);
    return image;
}

//==============================================================================
juce::Image renderComponent (juce::Component& component, juce::Rectangle<int> area, float scale)
{
    auto device = sharedDevice();
    if (device == nullptr)
        return {};

    // As Component::createComponentSnapshot (area, true, scale).
    area = area.getIntersection (component.getLocalBounds());
    if (area.isEmpty())
        return {};

    const auto w = juce::roundToInt ((float) area.getWidth() * scale);
    const auto h = juce::roundToInt ((float) area.getHeight() * scale);

    Canvas canvas (device);
    if (! canvas.resize (w, h))
        return {};

    const auto ok = canvas.paint (juce::Rectangle<int> (w, h), [&] (juce::Graphics& g)
    {
        g.addTransform (juce::AffineTransform::scale ((float) w / (float) area.getWidth(),
                                                      (float) h / (float) area.getHeight()));
        g.setOrigin (-area.getPosition());
        component.paintEntireComponent (g, true);
    });

    return ok ? canvas.readback() : juce::Image();
}

} // namespace ilana::vulkan
