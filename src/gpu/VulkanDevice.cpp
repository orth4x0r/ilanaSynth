// The shared Vulkan device and the pieces every canvas uses (see VulkanDevice.h).

#define VOLK_IMPLEMENTATION
#include "VulkanDevice.h"
#include "VulkanShaders.h"

#include <mutex>
#include <cstdlib>

namespace ilana::vulkan
{

//==============================================================================
namespace
{
std::optional<juce::String> readVariable (const juce::String& name)
{
    // (A sentinel no real value has: tells "unset" from "set to empty".)
    static const juce::String unset ("\x01ilana-unset\x01");
    const auto value = juce::SystemStats::getEnvironmentVariable (name, unset);
    if (value == unset)
        return std::nullopt;
    return value;
}

void writeVariable (const juce::String& name, const std::optional<juce::String>& value)
{
   #if defined (_WIN32)
    // The CRT passes it on to the process's Win32 environment, which the
    // loader reads (GetEnvironmentVariable); an empty value removes it.
    _wputenv_s (name.toWideCharPointer(), value.has_value() ? value->toWideCharPointer() : L"");
   #else
    if (value.has_value())
        setenv (name.toRawUTF8(), value->toRawUTF8(), 1);
    else
        unsetenv (name.toRawUTF8());
   #endif
}
} // namespace

const std::vector<std::pair<juce::String, juce::String>>& ScopedOverlayLayersOff::variables()
{
    static const std::vector<std::pair<juce::String, juce::String>> list {
        // Loader 1.3.234 and later: globs over layer names. Only overlays and
        // capture hooks; driver layers (AMD switchable graphics, NVIDIA
        // Optimus) stay, as they pick the GPU.
        { "VK_LOADER_LAYERS_DISABLE",
          "*steam_overlay*,*steam_fossilize*,*OBS_HOOK*,*obs_hook*,*discord*,*Discord*,*RTSS*,*rtss*,"
          "*OW_OVERLAY*,*OW_OBS_HOOK*,*overwolf*,*reshade*,*ReShade*,*bandicam*,*MANGOHUD*,*mangohud*,*fraps*,*FRAPS*" },
        // Older loaders: each layer's own off switch (its manifest's disable_environment).
        { "DISABLE_VK_LAYER_VALVE_steam_overlay_1", "1" },
        { "DISABLE_VK_LAYER_VALVE_steam_fossilize_1", "1" },
        { "DISABLE_VULKAN_OBS_CAPTURE", "1" },
        { "DISABLE_RTSS_LAYER", "1" },
    };
    return list;
}

ScopedOverlayLayersOff::ScopedOverlayLayersOff()
{
    for (const auto& [name, value] : variables())
    {
        auto previous = readVariable (name);
        // (One the person set themselves is kept, with ours added.)
        if (previous.has_value() && name == "VK_LOADER_LAYERS_DISABLE")
        {
            saved.emplace_back (name, previous);
            writeVariable (name, *previous + "," + value);
            continue;
        }
        saved.emplace_back (name, std::move (previous));
        writeVariable (name, value);
    }
}

ScopedOverlayLayersOff::~ScopedOverlayLayersOff()
{
    for (auto it = saved.rbegin(); it != saved.rend(); ++it)
        writeVariable (it->first, it->second);
}
namespace
{
juce::String& errorText()
{
    static juce::String text;
    return text;
}

bool fail (const juce::String& why)
{
    errorText() = why;
    DBG ("Vulkan UI unavailable: " << why);
    return false;
}

#define ILANA_VK_CHECK(call) \
    if (const auto vkResult = (call); vkResult != VK_SUCCESS) \
        return fail (juce::String (#call) + " failed (" + juce::String ((int) vkResult) + ")")

bool hasExtension (const std::vector<VkExtensionProperties>& list, const char* name)
{
    for (const auto& e : list)
        if (std::strcmp (e.extensionName, name) == 0)
            return true;
    return false;
}

const char* platformSurfaceExtension()
{
   #if JUCE_WINDOWS
    return "VK_KHR_win32_surface";
   #elif JUCE_LINUX || JUCE_BSD
    return "VK_KHR_xlib_surface";
   #else
    return nullptr;
   #endif
}

VkShaderModule makeShader (VkDevice device, const uint32_t* code, size_t bytes)
{
    VkShaderModuleCreateInfo info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info.codeSize = bytes;
    info.pCode = code;
    VkShaderModule module = VK_NULL_HANDLE;
    vkCreateShaderModule (device, &info, nullptr, &module);
    return module;
}

bool createPipelines (Device& d)
{
    // Set 0: the image or layer being drawn (or a 1x1 stand-in) and the
    // frame's gradient lookup tables.
    VkDescriptorSetLayoutBinding bindings[2] {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo setInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setInfo.bindingCount = 2;
    setInfo.pBindings = bindings;
    ILANA_VK_CHECK (vkCreateDescriptorSetLayout (d.device, &setInfo, nullptr, &d.setLayout));

    VkPushConstantRange range {};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset = 0;
    range.size = 96;

    VkPipelineLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &d.setLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &range;
    ILANA_VK_CHECK (vkCreatePipelineLayout (d.device, &layoutInfo, nullptr, &d.pipelineLayout));

    // One BGRA8 colour attachment kept in COLOR_ATTACHMENT_OPTIMAL. The
    // clearing pass starts a transparency layer; the loading pass draws on
    // what is there (the canvas, or a parent layer after a child ended).
    for (int clear = 0; clear < 2; ++clear)
    {
        VkAttachmentDescription attachment {};
        attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = clear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference ref { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &ref;

        // Earlier passes' writes (and transfers) are finished before this
        // pass reads or writes, and its writes before later sampling.
        VkSubpassDependency deps[2] {};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

        VkRenderPassCreateInfo info { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 2;
        info.pDependencies = deps;
        ILANA_VK_CHECK (vkCreateRenderPass (d.device, &info, nullptr, clear ? &d.clearRenderPass : &d.renderPass));
    }

    auto vert = makeShader (d.device, ilanaUiVert, sizeof (ilanaUiVert));
    auto frag = makeShader (d.device, ilanaUiFrag, sizeof (ilanaUiFrag));

    if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE)
        return fail ("shader module creation failed");

    VkPipelineShaderStageCreateInfo stages[2] {};
    stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding { 0, 16, VK_VERTEX_INPUT_RATE_INSTANCE };
    VkVertexInputAttributeDescription attributes[3] {
        { 0, 0, VK_FORMAT_R16G16B16A16_SINT, 0 },
        { 1, 0, VK_FORMAT_R8G8B8A8_UNORM, 8 },
        { 2, 0, VK_FORMAT_R32_UINT, 12 },
    };
    VkPipelineVertexInputStateCreateInfo vertexInput { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo assembly { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkDynamicState dynamics[] { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamics;

    for (int replace = 0; replace < 2; ++replace)
    {
        VkPipelineColorBlendAttachmentState blend {};
        blend.blendEnable = replace ? VK_FALSE : VK_TRUE;
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.alphaBlendOp = VK_BLEND_OP_ADD;
        blend.colorWriteMask = 0xf;

        VkPipelineColorBlendStateCreateInfo blendState { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        blendState.attachmentCount = 1;
        blendState.pAttachments = &blend;

        VkGraphicsPipelineCreateInfo info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pColorBlendState = &blendState;
        info.pDynamicState = &dynamic;
        info.layout = d.pipelineLayout;
        info.renderPass = d.renderPass;
        info.subpass = 0;

        const auto result = vkCreateGraphicsPipelines (d.device, VK_NULL_HANDLE, 1, &info, nullptr,
                                                       replace ? &d.replacePipeline : &d.blendPipeline);
        if (result != VK_SUCCESS)
        {
            vkDestroyShaderModule (d.device, vert, nullptr);
            vkDestroyShaderModule (d.device, frag, nullptr);
            return fail ("pipeline creation failed (" + juce::String ((int) result) + ")");
        }
    }

    vkDestroyShaderModule (d.device, vert, nullptr);
    vkDestroyShaderModule (d.device, frag, nullptr);

    VkSamplerCreateInfo samplerInfo { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ILANA_VK_CHECK (vkCreateSampler (d.device, &samplerInfo, nullptr, &d.nearestSampler));
    return true;
}

bool formatUsable (VkPhysicalDevice physical)
{
    VkFormatProperties props {};
    vkGetPhysicalDeviceFormatProperties (physical, VK_FORMAT_B8G8R8A8_UNORM, &props);
    const auto needed = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT
                      | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT;
    return (props.optimalTilingFeatures & needed) == needed;
}

bool createDevice (Device& d)
{
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_NO_VULKAN", "").isNotEmpty())
        return fail ("ILANA_NO_VULKAN is set");

    if (volkInitialize() != VK_SUCCESS)
        return fail ("no Vulkan loader on this machine");

    // (Until the instance exists: the loader picks its layers then.)
    std::optional<ScopedOverlayLayersOff> overlaysOff;
    overlaysOff.emplace();

    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties (nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> instanceExtensions (count);
    vkEnumerateInstanceExtensionProperties (nullptr, &count, instanceExtensions.data());

    std::vector<const char*> enabled;
    const auto* surfaceExtension = platformSurfaceExtension();
    const bool surfaces = surfaceExtension != nullptr
                       && hasExtension (instanceExtensions, "VK_KHR_surface")
                       && hasExtension (instanceExtensions, surfaceExtension);
    if (surfaces)
    {
        enabled.push_back ("VK_KHR_surface");
        enabled.push_back (surfaceExtension);
    }

    VkApplicationInfo app { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.pApplicationName = "ilanaSynth";
    app.pEngineName = "JUCE";
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceInfo.pApplicationInfo = &app;
    instanceInfo.enabledExtensionCount = (uint32_t) enabled.size();
    instanceInfo.ppEnabledExtensionNames = enabled.data();
    ILANA_VK_CHECK (vkCreateInstance (&instanceInfo, nullptr, &d.instance));
    overlaysOff.reset();
    volkLoadInstanceOnly (d.instance);

    count = 0;
    vkEnumeratePhysicalDevices (d.instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices (count);
    vkEnumeratePhysicalDevices (d.instance, &count, devices.data());

    // A real GPU first (discrete, then integrated); a CPU implementation
    // (llvmpipe, SwiftShader) only when it is all there is.
    const auto rank = [] (VkPhysicalDeviceType type)
    {
        switch (type)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 0;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 1;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return 2;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            return 3;
            default:                                     return 4;
        }
    };

    int bestRank = 99;
    for (auto physical : devices)
    {
        VkPhysicalDeviceProperties props {};
        vkGetPhysicalDeviceProperties (physical, &props);

        uint32_t families = 0;
        vkGetPhysicalDeviceQueueFamilyProperties (physical, &families, nullptr);
        std::vector<VkQueueFamilyProperties> familyProps (families);
        vkGetPhysicalDeviceQueueFamilyProperties (physical, &families, familyProps.data());

        for (uint32_t i = 0; i < families; ++i)
        {
            if ((familyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && formatUsable (physical)
                && rank (props.deviceType) < bestRank)
            {
                bestRank = rank (props.deviceType);
                d.physical = physical;
                d.queueFamily = i;
                d.limits = props.limits;
                d.name = juce::String (props.deviceName) + " (Vulkan "
                       + juce::String (VK_VERSION_MAJOR (props.apiVersion)) + "." + juce::String (VK_VERSION_MINOR (props.apiVersion))
                       + "." + juce::String (VK_VERSION_PATCH (props.apiVersion)) + ")";
                break;
            }
        }
    }

    if (d.physical == VK_NULL_HANDLE)
        return fail ("no Vulkan device that can draw the UI");

    vkGetPhysicalDeviceMemoryProperties (d.physical, &d.memory);

    count = 0;
    vkEnumerateDeviceExtensionProperties (d.physical, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> deviceExtensions (count);
    vkEnumerateDeviceExtensionProperties (d.physical, nullptr, &count, deviceExtensions.data());
    std::vector<const char*> deviceEnabled;
    d.hasSurfaceExtensions = surfaces && hasExtension (deviceExtensions, "VK_KHR_swapchain");
    if (d.hasSurfaceExtensions)
        deviceEnabled.push_back ("VK_KHR_swapchain");

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueInfo.queueFamilyIndex = d.queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo deviceInfo { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = (uint32_t) deviceEnabled.size();
    deviceInfo.ppEnabledExtensionNames = deviceEnabled.data();
    ILANA_VK_CHECK (vkCreateDevice (d.physical, &deviceInfo, nullptr, &d.device));
    volkLoadDevice (d.device);
    vkGetDeviceQueue (d.device, d.queueFamily, 0, &d.queue);

    VkCommandPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = d.queueFamily;
    ILANA_VK_CHECK (vkCreateCommandPool (d.device, &poolInfo, nullptr, &d.commandPool));

    return createPipelines (d);
}
} // namespace

Device::~Device()
{
    if (device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle (device);
        destroyCachedTextures();
        if (nearestSampler != VK_NULL_HANDLE)  vkDestroySampler (device, nearestSampler, nullptr);
        if (blendPipeline != VK_NULL_HANDLE)   vkDestroyPipeline (device, blendPipeline, nullptr);
        if (replacePipeline != VK_NULL_HANDLE) vkDestroyPipeline (device, replacePipeline, nullptr);
        if (renderPass != VK_NULL_HANDLE)      vkDestroyRenderPass (device, renderPass, nullptr);
        if (clearRenderPass != VK_NULL_HANDLE) vkDestroyRenderPass (device, clearRenderPass, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)  vkDestroyPipelineLayout (device, pipelineLayout, nullptr);
        if (setLayout != VK_NULL_HANDLE)       vkDestroyDescriptorSetLayout (device, setLayout, nullptr);
        if (commandPool != VK_NULL_HANDLE)     vkDestroyCommandPool (device, commandPool, nullptr);
        vkDestroyDevice (device, nullptr);
    }

    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance (instance, nullptr);
}

uint32_t Device::findMemoryType (uint32_t typeBits, VkMemoryPropertyFlags wanted) const
{
    for (uint32_t i = 0; i < memory.memoryTypeCount; ++i)
        if ((typeBits & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & wanted) == wanted)
            return i;
    return UINT32_MAX;
}

std::shared_ptr<Device> sharedDevice()
{
    // Created once per process; a failed attempt is not retried (each try
    // costs a driver load). Closing and reopening the plugin window keeps
    // it: a holder JUCE deletes at shutdown (before the DLL unloads, where
    // Vulkan calls could deadlock on the loader lock) owns one reference.
    struct Holder final : public juce::DeletedAtShutdown
    {
        std::shared_ptr<Device> device;
    };
    static std::weak_ptr<Device> current;
    static bool failed = false;
    JUCE_ASSERT_MESSAGE_THREAD

    if (auto existing = current.lock())
        return existing;

    if (failed)
        return {};

    auto device = std::make_shared<Device>();

    if (! createDevice (*device))
    {
        failed = true;
        return {};
    }

    current = device;
    (new Holder())->device = device;
    return device;
}

juce::String lastError() { return errorText(); }

//==============================================================================
bool Buffer::create (Device& d, VkDeviceSize bytes, VkBufferUsageFlags usage, bool hostVisible)
{
    VkBufferCreateInfo info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    info.size = bytes;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer (d.device, &info, nullptr, &buffer) != VK_SUCCESS)
        return false;

    VkMemoryRequirements req {};
    vkGetBufferMemoryRequirements (d.device, buffer, &req);
    VkMemoryAllocateInfo alloc { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = d.findMemoryType (req.memoryTypeBits, hostVisible ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                                                                              : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (alloc.memoryTypeIndex == UINT32_MAX || vkAllocateMemory (d.device, &alloc, nullptr, &memory) != VK_SUCCESS)
    {
        destroy (d);
        return false;
    }

    vkBindBufferMemory (d.device, buffer, memory, 0);
    size = bytes;

    if (hostVisible && vkMapMemory (d.device, memory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS)
    {
        destroy (d);
        return false;
    }

    return true;
}

void Buffer::destroy (Device& d)
{
    if (buffer != VK_NULL_HANDLE) vkDestroyBuffer (d.device, buffer, nullptr);
    if (memory != VK_NULL_HANDLE) vkFreeMemory (d.device, memory, nullptr);
    buffer = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    mapped = nullptr;
    size = 0;
}

bool Texture::create (Device& d, int w, int h, bool renderTarget)
{
    width = w;
    height = h;
    layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageCreateInfo info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_B8G8R8A8_UNORM;
    info.extent = { (uint32_t) w, (uint32_t) h, 1 };
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
               | (renderTarget ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0);
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage (d.device, &info, nullptr, &image) != VK_SUCCESS)
        return false;

    VkMemoryRequirements req {};
    vkGetImageMemoryRequirements (d.device, image, &req);
    VkMemoryAllocateInfo alloc { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = d.findMemoryType (req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (alloc.memoryTypeIndex == UINT32_MAX)
        alloc.memoryTypeIndex = d.findMemoryType (req.memoryTypeBits, 0);
    if (alloc.memoryTypeIndex == UINT32_MAX || vkAllocateMemory (d.device, &alloc, nullptr, &memory) != VK_SUCCESS)
    {
        destroy (d);
        return false;
    }
    vkBindImageMemory (d.device, image, memory, 0);

    VkImageViewCreateInfo viewInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView (d.device, &viewInfo, nullptr, &view) != VK_SUCCESS)
    {
        destroy (d);
        return false;
    }

    if (renderTarget)
    {
        VkFramebufferCreateInfo fb { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fb.renderPass = d.renderPass;
        fb.attachmentCount = 1;
        fb.pAttachments = &view;
        fb.width = (uint32_t) w;
        fb.height = (uint32_t) h;
        fb.layers = 1;
        if (vkCreateFramebuffer (d.device, &fb, nullptr, &framebuffer) != VK_SUCCESS)
        {
            destroy (d);
            return false;
        }
    }

    return true;
}

void Texture::destroy (Device& d)
{
    if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer (d.device, framebuffer, nullptr);
    if (view != VK_NULL_HANDLE)        vkDestroyImageView (d.device, view, nullptr);
    if (image != VK_NULL_HANDLE)       vkDestroyImage (d.device, image, nullptr);
    if (memory != VK_NULL_HANDLE)      vkFreeMemory (d.device, memory, nullptr);
    framebuffer = VK_NULL_HANDLE;
    view = VK_NULL_HANDLE;
    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void transition (VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to)
{
    const auto access = [] (VkImageLayout l) -> VkAccessFlags
    {
        switch (l)
        {
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_ACCESS_SHADER_READ_BIT;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:     return VK_ACCESS_TRANSFER_READ_BIT;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:     return VK_ACCESS_TRANSFER_WRITE_BIT;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:          return 0;
            default:                                       return 0;
        }
    };
    const auto stage = [] (VkImageLayout l) -> VkPipelineStageFlags
    {
        switch (l)
        {
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:     return VK_PIPELINE_STAGE_TRANSFER_BIT;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:          return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            default:                                       return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
    };

    VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.srcAccessMask = access (from);
    barrier.dstAccessMask = access (to);
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    auto srcStage = stage (from);
    if (from == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR || from == VK_IMAGE_LAYOUT_UNDEFINED)
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    // The swapchain acquire semaphore waits at TRANSFER; a TOP_OF_PIPE source
    // with a TRANSFER destination orders after it.
    vkCmdPipelineBarrier (cmd, srcStage, stage (to), 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void transition (VkCommandBuffer cmd, Texture& t, VkImageLayout to)
{
    if (t.layout == to)
        return;
    transition (cmd, t.image, t.layout, to);
    t.layout = to;
}

} // namespace ilana::vulkan
