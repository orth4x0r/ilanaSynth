#pragma once

// The editor's Vulkan renderer (Windows and Linux; macOS keeps CoreGraphics,
// it has no native Vulkan). Only built when CMake sets ILANA_VULKAN=1.
//
//   VulkanDevice.h      the shared device, pipelines and per-frame buffers
//   VulkanCanvas.h      an offscreen GPU picture that JUCE Graphics paints into
//                       (a juce::LowLevelGraphicsContext, VulkanGraphics.cpp)
//   VulkanAttachment.h  puts a Canvas on screen behind a component (swapchain)
//
// The loader (vulkan-1.dll / libvulkan.so.1) is opened at run time through
// volk, so a machine without a Vulkan driver still runs the plugin: every
// entry point returns "unavailable" and the editor keeps its old renderer.

#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <volk.h>
#include <juce_graphics/juce_graphics.h>

namespace ilana::vulkan
{
struct ImageCache;

// One per process, created on first use on the message thread and shared by
// every editor. All recording and submitting happens on the message thread.
struct Device
{
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkPhysicalDeviceMemoryProperties memory {};
    VkPhysicalDeviceLimits limits {};
    juce::String name;          // "NVIDIA GeForce RTX 3060 (Vulkan 1.3.277)"
    bool hasSurfaceExtensions = false; // VK_KHR_surface + the platform's surface extension were enabled

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;      // one BGRA8 colour attachment, LOAD/STORE, stays COLOR_ATTACHMENT_OPTIMAL
    VkRenderPass clearRenderPass = VK_NULL_HANDLE; // the same, cleared to transparent first (a new transparency layer)
    VkPipeline blendPipeline = VK_NULL_HANDLE;   // premultiplied source-over
    VkPipeline replacePipeline = VK_NULL_HANDLE; // writes the source as it is
    VkSampler nearestSampler = VK_NULL_HANDLE;

    // Textures made from juce::Images, shared by every canvas (VulkanGraphics.cpp).
    ImageCache* imageCache = nullptr;
    void destroyCachedTextures();

    ~Device();

    uint32_t findMemoryType (uint32_t typeBits, VkMemoryPropertyFlags wanted) const;
};

// While alive, keeps game overlays' and screen-capture tools' implicit
// layers (Steam overlay, OBS, Discord, RTSS, ReShade...) out of any Vulkan
// instance this process creates. Installed system-wide, they load into every
// Vulkan app, the editor included, and hook the window it presents to: on
// ilana's PC (Steam overlay + OBS + Steam Fossilize layers) menus and
// call-outs closed as they opened only while Vulkan drew the editor. The
// loader reads these variables when the instance is made, so they are set
// around that and then put back (the host's process shares them).
class ScopedOverlayLayersOff
{
public:
    ScopedOverlayLayersOff();
    ~ScopedOverlayLayersOff();

    // The variables it sets and what it sets them to (the UI test reads them).
    static const std::vector<std::pair<juce::String, juce::String>>& variables();

private:
    std::vector<std::pair<juce::String, std::optional<juce::String>>> saved;

    JUCE_DECLARE_NON_COPYABLE (ScopedOverlayLayersOff)
};

// The shared device, or nullptr when Vulkan can't be used here (no loader,
// no device, a creation call failed, or ILANA_NO_VULKAN is set). The reason
// is in lastError().
std::shared_ptr<Device> sharedDevice();
juce::String lastError();

// A device-memory buffer or image with its memory (move-only helpers).
struct Buffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr; // host-visible buffers stay mapped

    bool create (Device&, VkDeviceSize, VkBufferUsageFlags, bool hostVisible);
    void destroy (Device&);
};

struct Texture
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE; // only for render targets
    int width = 0, height = 0;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    // BGRA8 UNORM (juce::Image::ARGB's memory order); a render target can be
    // drawn into, sampled and copied from.
    bool create (Device&, int w, int h, bool renderTarget);
    void destroy (Device&);
};

// Records a layout change (and its barrier) into cmd.
void transition (VkCommandBuffer cmd, Texture&, VkImageLayout to);
void transition (VkCommandBuffer cmd, VkImage, VkImageLayout from, VkImageLayout to);

} // namespace ilana::vulkan
