#include "engine/renderer/Renderer.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>

namespace factory::engine {
namespace {

constexpr std::array<const char*, 1> kDeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
constexpr std::string_view kValidationLayer = "VK_LAYER_KHRONOS_validation";

std::vector<char> readShader(const char* filename) {
    std::string path = std::string("shaders/") + filename;
    std::ifstream file(path, std::ios::ate | std::ios::binary);
#ifdef FACTORY_SHADER_DIR
    if (!file) {
        path = std::string(FACTORY_SHADER_DIR) + "/" + filename;
        file.open(path, std::ios::ate | std::ios::binary);
    }
#endif
    if (!file) throw std::runtime_error("Could not open shader: " + path);
    const auto size = static_cast<std::size_t>(file.tellg());
    std::vector<char> data(size);
    file.seekg(0);
    file.read(data.data(), static_cast<std::streamsize>(size));
    return data;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                              VkDebugUtilsMessageTypeFlagsEXT,
                                              const VkDebugUtilsMessengerCallbackDataEXT* data,
                                              void*) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "Vulkan validation: " << data->pMessage << '\n';
    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo() {
    VkDebugUtilsMessengerCreateInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    return info;
}

std::string versionString(std::uint32_t version) {
    return std::to_string(VK_API_VERSION_MAJOR(version)) + "." +
           std::to_string(VK_API_VERSION_MINOR(version)) + "." +
           std::to_string(VK_API_VERSION_PATCH(version));
}

void require(VkResult result, std::string_view operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + " failed (VkResult " + std::to_string(result) + ")");
}

} // namespace

struct Renderer::QueueFamilies {
    std::optional<std::uint32_t> graphics;
    std::optional<std::uint32_t> present;
    [[nodiscard]] bool complete() const noexcept { return graphics.has_value() && present.has_value(); }
};

struct Renderer::SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct Renderer::Instance {
    float x, y;
    float width, height;
    float r, g, b, a;
    float mode;
};

struct Renderer::InstanceBuffer {
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    void* mapped{};
    std::size_t count{};
    std::size_t visibleMachines{};
    std::uint64_t revision{std::numeric_limits<std::uint64_t>::max()};
    Vec2 cameraPosition{};
    Vec2 viewport{};
    float zoom{};
    bool grid{};
    game::MachineId selected{};
    std::optional<PlacementPreview> preview;
};

Renderer::Renderer(Window& window) : window_(window) {
#ifndef NDEBUG
    validationEnabled_ = true;
#endif
    try { initialize(); } catch (...) { cleanup(); throw; }
}

Renderer::~Renderer() { cleanup(); }

void Renderer::initialize() {
    createInstance();
    createDebugMessenger();
    createSurface();
    selectPhysicalDevice();
    createDevice();
    createCommandPool();
    createSwapchainObjects();
    createCommandBuffers();
    createSyncObjects();
    createInstanceBuffers();
    initImGui();
}

void Renderer::createInstance() {
    std::uint32_t supportedVersion = VK_API_VERSION_1_0;
    vkEnumerateInstanceVersion(&supportedVersion);
    vulkanApiVersion_ = std::min(supportedVersion, VK_API_VERSION_1_3);

    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
    if (validationEnabled_) {
        validationEnabled_ = std::any_of(layers.begin(), layers.end(), [](const auto& layer) {
            return std::string_view(layer.layerName) == kValidationLayer;
        });
        if (!validationEnabled_) std::cerr << "Vulkan validation layer is unavailable; continuing without it.\n";
    }

    std::uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (!glfwExtensions || glfwExtensionCount == 0)
        throw std::runtime_error("GLFW returned no Vulkan instance extensions");
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (validationEnabled_) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "FactoryGame";
    app.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
    app.pEngineName = "FactoryGame Engine";
    app.engineVersion = VK_MAKE_VERSION(0, 2, 0);
    app.apiVersion = vulkanApiVersion_;

    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    const char* validationLayer = kValidationLayer.data();
    auto debugInfo = debugCreateInfo();
    if (validationEnabled_) {
        info.enabledLayerCount = 1;
        info.ppEnabledLayerNames = &validationLayer;
        info.pNext = &debugInfo;
    }
    require(vkCreateInstance(&info, nullptr, &instance_), "Vulkan instance creation");
}

void Renderer::createDebugMessenger() {
    if (!validationEnabled_) return;
    const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    if (!create) throw std::runtime_error("VK_EXT_debug_utils was enabled but its entry point is unavailable");
    auto info = debugCreateInfo();
    require(create(instance_, &info, nullptr, &debugMessenger_), "Vulkan debug messenger creation");
}

void Renderer::createSurface() {
    require(glfwCreateWindowSurface(instance_, window_.native(), nullptr, &surface_), "Window surface creation");
}

Renderer::QueueFamilies Renderer::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilies result;
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    for (std::uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) result.graphics = i;
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present);
        if (present) result.present = i;
        if (result.complete()) break;
    }
    return result;
}

Renderer::SwapchainSupport Renderer::querySwapchain(VkPhysicalDevice device) const {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &support.capabilities);
    std::uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, nullptr);
    support.formats.resize(count);
    if (count) vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, support.formats.data());
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, nullptr);
    support.presentModes.resize(count);
    if (count) vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, support.presentModes.data());
    return support;
}

bool Renderer::deviceSuitable(VkPhysicalDevice device) const {
    if (!findQueueFamilies(device).complete()) return false;
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());
    std::set<std::string> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& extension : available) requiredExtensions.erase(extension.extensionName);
    if (!requiredExtensions.empty()) return false;
    const auto support = querySwapchain(device);
    return !support.formats.empty() && !support.presentModes.empty();
}

void Renderer::selectPhysicalDevice() {
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan-capable GPU was found");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());
    for (VkPhysicalDevice device : devices) {
        if (deviceSuitable(device)) { physicalDevice_ = device; break; }
    }
    if (!physicalDevice_) throw std::runtime_error("No GPU supports graphics, presentation, and VK_KHR_swapchain");
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    gpuName_ = properties.deviceName;
    apiVersion_ = versionString(properties.apiVersion);
    std::cout << "GPU: " << gpuName_ << ", Vulkan " << apiVersion_ << '\n';
}

void Renderer::createDevice() {
    const auto families = findQueueFamilies(physicalDevice_);
    const std::set<std::uint32_t> unique{*families.graphics, *families.present};
    constexpr float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queues;
    for (std::uint32_t family : unique) {
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex = family;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        queues.push_back(queue);
    }
    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size());
    info.pQueueCreateInfos = queues.data();
    info.pEnabledFeatures = &features;
    info.enabledExtensionCount = static_cast<std::uint32_t>(kDeviceExtensions.size());
    info.ppEnabledExtensionNames = kDeviceExtensions.data();
    require(vkCreateDevice(physicalDevice_, &info, nullptr, &device_), "Logical device creation");
    vkGetDeviceQueue(device_, *families.graphics, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, *families.present, 0, &presentQueue_);
}

void Renderer::createCommandPool() {
    VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = *findQueueFamilies(physicalDevice_).graphics;
    require(vkCreateCommandPool(device_, &info, nullptr, &commandPool_), "Command pool creation");
}

void Renderer::createSwapchain() {
    const auto support = querySwapchain(physicalDevice_);
    VkSurfaceFormatKHR format = support.formats.front();
    for (const auto candidate : support.formats) {
        if (candidate.format == VK_FORMAT_B8G8R8A8_SRGB &&
            candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { format = candidate; break; }
    }
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto candidate : support.presentModes)
        if (candidate == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = candidate; break; }

    VkExtent2D extent = support.capabilities.currentExtent;
    if (extent.width == std::numeric_limits<std::uint32_t>::max()) {
        extent.width = std::clamp(static_cast<std::uint32_t>(window_.framebufferWidth()),
                                  support.capabilities.minImageExtent.width,
                                  support.capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<std::uint32_t>(window_.framebufferHeight()),
                                   support.capabilities.minImageExtent.height,
                                   support.capabilities.maxImageExtent.height);
    }
    std::uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount)
        imageCount = std::min(imageCount, support.capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = surface_;
    info.minImageCount = imageCount;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    const auto families = findQueueFamilies(physicalDevice_);
    const std::uint32_t indices[]{*families.graphics, *families.present};
    if (families.graphics != families.present) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = indices;
    } else info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;
    require(vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_), "Swapchain creation");
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());
    swapchainFormat_ = format.format;
    swapchainExtent_ = extent;
}

void Renderer::createImageViews() {
    imageViews_.resize(swapchainImages_.size());
    for (std::size_t i = 0; i < imageViews_.size(); ++i) {
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.image = swapchainImages_[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = swapchainFormat_;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        require(vkCreateImageView(device_, &info, nullptr, &imageViews_[i]), "Swapchain image view creation");
    }
}

void Renderer::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = swapchainFormat_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;
    require(vkCreateRenderPass(device_, &info, nullptr, &renderPass_), "Render pass creation");
}

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const std::uint32_t*>(code.data());
    VkShaderModule module{};
    require(vkCreateShaderModule(device_, &info, nullptr, &module), "Shader module creation");
    return module;
}

void Renderer::createPipeline() {
    const auto vertexCode = readShader("factory.vert.spv");
    const auto fragmentCode = readShader("factory.frag.spv");
    const VkShaderModule vertex = createShaderModule(vertexCode);
    const VkShaderModule fragment = createShaderModule(fragmentCode);
    try {
        VkPipelineShaderStageCreateInfo vertexStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertex;
        vertexStage.pName = "main";
        VkPipelineShaderStageCreateInfo fragmentStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = fragment;
        fragmentStage.pName = "main";
        const VkPipelineShaderStageCreateInfo stages[]{vertexStage, fragmentStage};

        VkVertexInputBindingDescription binding{0, sizeof(Instance), VK_VERTEX_INPUT_RATE_INSTANCE};
        const std::array<VkVertexInputAttributeDescription, 4> attributes{{
            {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Instance, x)},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Instance, width)},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instance, r)},
            {3, 0, VK_FORMAT_R32_SFLOAT, offsetof(Instance, mode)},
        }};
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blend{};
        blend.blendEnable = VK_TRUE;
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.alphaBlendOp = VK_BLEND_OP_ADD;
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blending.attachmentCount = 1;
        blending.pAttachments = &blend;
        constexpr std::array<VkDynamicState, 2> dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamics.size());
        dynamic.pDynamicStates = dynamics.data();
        VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 5};
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.pushConstantRangeCount = 1;
        layout.pPushConstantRanges = &push;
        require(vkCreatePipelineLayout(device_, &layout, nullptr, &pipelineLayout_), "Pipeline layout creation");
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &rasterizer;
        info.pMultisampleState = &multisample;
        info.pColorBlendState = &blending;
        info.pDynamicState = &dynamic;
        info.layout = pipelineLayout_;
        info.renderPass = renderPass_;
        require(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_),
                "Graphics pipeline creation");
    } catch (...) {
        vkDestroyShaderModule(device_, fragment, nullptr);
        vkDestroyShaderModule(device_, vertex, nullptr);
        throw;
    }
    vkDestroyShaderModule(device_, fragment, nullptr);
    vkDestroyShaderModule(device_, vertex, nullptr);
}

void Renderer::createFramebuffers() {
    framebuffers_.resize(imageViews_.size());
    for (std::size_t i = 0; i < imageViews_.size(); ++i) {
        VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = renderPass_;
        info.attachmentCount = 1;
        info.pAttachments = &imageViews_[i];
        info.width = swapchainExtent_.width;
        info.height = swapchainExtent_.height;
        info.layers = 1;
        require(vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]), "Framebuffer creation");
    }
}

void Renderer::createSwapchainObjects() {
    createSwapchain(); createImageViews(); createRenderPass(); createPipeline(); createFramebuffers();
}

void Renderer::createCommandBuffers() {
    VkCommandBufferAllocateInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    info.commandPool = commandPool_;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size());
    require(vkAllocateCommandBuffers(device_, &info, commandBuffers_.data()), "Command buffer allocation");
}

void Renderer::createSyncObjects() {
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (std::size_t i = 0; i < kFramesInFlight; ++i) {
        require(vkCreateSemaphore(device_, &semaphore, nullptr, &imageAvailable_[i]), "Image semaphore creation");
        require(vkCreateFence(device_, &fence, nullptr, &inFlight_[i]), "Frame fence creation");
    }
    createPresentSemaphores();
}

void Renderer::createPresentSemaphores() {
    renderFinished_.resize(swapchainImages_.size());
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (auto& value : renderFinished_)
        require(vkCreateSemaphore(device_, &semaphore, nullptr, &value), "Present semaphore creation");
}

std::uint32_t Renderer::findMemoryType(std::uint32_t filter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memory{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memory);
    for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i)
        if ((filter & (1u << i)) && (memory.memoryTypes[i].propertyFlags & properties) == properties) return i;
    throw std::runtime_error("No compatible Vulkan memory type was found");
}

void Renderer::createInstanceBuffers() {
    for (auto& slot : instanceBuffers_) {
        slot = new InstanceBuffer{};
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = sizeof(Instance) * kInstanceCapacity;
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        require(vkCreateBuffer(device_, &info, nullptr, &slot->buffer), "Instance buffer creation");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, slot->buffer, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        require(vkAllocateMemory(device_, &allocation, nullptr, &slot->memory), "Instance memory allocation");
        require(vkBindBufferMemory(device_, slot->buffer, slot->memory, 0), "Instance memory binding");
        require(vkMapMemory(device_, slot->memory, 0, info.size, 0, &slot->mapped), "Instance memory mapping");
    }
    stats_.instanceBufferBytes = sizeof(Instance) * kInstanceCapacity * kFramesInFlight;
}

void Renderer::initImGuiVulkan() {
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = vulkanApiVersion_;
    info.Instance = instance_;
    info.PhysicalDevice = physicalDevice_;
    info.Device = device_;
    info.QueueFamily = *findQueueFamilies(physicalDevice_).graphics;
    info.Queue = graphicsQueue_;
    info.DescriptorPoolSize = 1000;
    info.RenderPass = renderPass_;
    info.MinImageCount = std::max(2u, static_cast<std::uint32_t>(swapchainImages_.size()));
    info.ImageCount = static_cast<std::uint32_t>(swapchainImages_.size());
    info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    if (!ImGui_ImplVulkan_Init(&info)) throw std::runtime_error("Dear ImGui Vulkan backend initialization failed");
    imguiVulkanInitialized_ = true;
}

void Renderer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imguiContextCreated_ = true;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplGlfw_InitForVulkan(window_.native(), true))
        throw std::runtime_error("Dear ImGui GLFW backend initialization failed");
    imguiGlfwInitialized_ = true;
    initImGuiVulkan();
}

void Renderer::beginUiFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Renderer::endUiFrame() { ImGui::Render(); }

void Renderer::updateInstances(InstanceBuffer& target, const game::World& world, const Camera2D& camera,
                               bool showGrid, game::MachineId selected,
                               const std::optional<PlacementPreview>& preview) {
    const Vec2 position = camera.position();
    const Vec2 viewport = camera.viewport();
    const auto previewEqual = [&] {
        if (target.preview.has_value() != preview.has_value()) return false;
        if (!preview) return true;
        return target.preview->x == preview->x && target.preview->y == preview->y &&
               target.preview->valid == preview->valid;
    };
    if (target.revision == world.revision() && target.cameraPosition.x == position.x &&
        target.cameraPosition.y == position.y && target.viewport.x == viewport.x &&
        target.viewport.y == viewport.y && target.zoom == camera.zoom() && target.grid == showGrid &&
        target.selected == selected && previewEqual()) {
        stats_.visibleInstances = target.visibleMachines;
        stats_.submittedInstances = target.count;
        stats_.drawCalls = target.count ? 1 : 0;
        return;
    }

    std::vector<Instance> instances;
    instances.reserve(std::min(world.machineCount() + 2, kInstanceCapacity));
    const auto bounds = camera.visibleBounds(1.0f);
    if (showGrid) {
        const float left = std::floor(bounds.left);
        const float top = std::floor(bounds.top);
        instances.push_back({left, top, std::ceil(bounds.right) - left, std::ceil(bounds.bottom) - top,
                             0.32f, 0.38f, 0.45f, 0.38f, 1.0f});
    }
    std::size_t visible = 0;
    for (const auto& [id, machine] : world.machines()) {
        if (machine.x + 1.0f < bounds.left || machine.x > bounds.right ||
            machine.y + 1.0f < bounds.top || machine.y > bounds.bottom) continue;
        std::array<float, 4> color{};
        switch (machine.type) {
        case game::MachineType::Furnace: color = {0.90f, 0.30f, 0.16f, 1.0f}; break;
        case game::MachineType::Crusher: color = {0.42f, 0.55f, 0.67f, 1.0f}; break;
        case game::MachineType::ChemicalReactor: color = {0.20f, 0.72f, 0.50f, 1.0f}; break;
        case game::MachineType::Generator: color = {0.94f, 0.72f, 0.14f, 1.0f}; break;
        }
        instances.push_back({machine.x + 0.08f, machine.y + 0.08f, 0.84f, 0.84f,
                             color[0], color[1], color[2], color[3], id == selected ? 2.0f : 0.0f});
        ++visible;
        if (instances.size() == kInstanceCapacity - 1) break;
    }
    if (preview && world.contains(preview->x, preview->y) && instances.size() < kInstanceCapacity) {
        const auto color = preview->valid ? std::array<float, 4>{0.25f, 0.95f, 0.45f, 0.55f}
                                          : std::array<float, 4>{0.95f, 0.18f, 0.18f, 0.55f};
        instances.push_back({preview->x + 0.04f, preview->y + 0.04f, 0.92f, 0.92f,
                             color[0], color[1], color[2], color[3], 3.0f});
    }
    std::memcpy(target.mapped, instances.data(), instances.size() * sizeof(Instance));
    target.count = instances.size();
    target.visibleMachines = visible;
    target.revision = world.revision();
    target.cameraPosition = position;
    target.viewport = viewport;
    target.zoom = camera.zoom();
    target.grid = showGrid;
    target.selected = selected;
    target.preview = preview;
    stats_.visibleInstances = visible;
    stats_.submittedInstances = target.count;
    stats_.drawCalls = target.count ? 1 : 0;
}

void Renderer::recordCommands(VkCommandBuffer commandBuffer, std::uint32_t imageIndex,
                              const InstanceBuffer& instances, const Camera2D& camera) {
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    require(vkBeginCommandBuffer(commandBuffer, &begin), "Command buffer recording");
    const VkClearValue clear{{{0.025f, 0.035f, 0.055f, 1.0f}}};
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = renderPass_;
    pass.framebuffer = framebuffers_[imageIndex];
    pass.renderArea.extent = swapchainExtent_;
    pass.clearValueCount = 1;
    pass.pClearValues = &clear;
    vkCmdBeginRenderPass(commandBuffer, &pass, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{0.0f, 0.0f, static_cast<float>(swapchainExtent_.width),
                        static_cast<float>(swapchainExtent_.height), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, swapchainExtent_};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    if (instances.count) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &instances.buffer, &offset);
        struct Push { float x, y, width, height, zoom; } push{
            camera.position().x, camera.position().y, camera.viewport().x, camera.viewport().y, camera.zoom()};
        vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
        vkCmdDraw(commandBuffer, 6, static_cast<std::uint32_t>(instances.count), 0, 0);
    }
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    vkCmdEndRenderPass(commandBuffer);
    require(vkEndCommandBuffer(commandBuffer), "Command buffer finalization");
}

void Renderer::draw(const game::World& world, const Camera2D& camera, bool showGrid,
                    game::MachineId selected, std::optional<PlacementPreview> preview) {
    stats_.totalInstances = world.machineCount();
    require(vkWaitForFences(device_, 1, &inFlight_[frameIndex_], VK_TRUE, UINT64_MAX), "Frame fence wait");
    std::uint32_t imageIndex = 0;
    const VkResult acquired = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                                     imageAvailable_[frameIndex_], VK_NULL_HANDLE, &imageIndex);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); return; }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("Swapchain image acquisition failed (VkResult " + std::to_string(acquired) + ")");
    auto& buffer = *instanceBuffers_[frameIndex_];
    updateInstances(buffer, world, camera, showGrid, selected, preview);
    require(vkResetFences(device_, 1, &inFlight_[frameIndex_]), "Frame fence reset");
    require(vkResetCommandBuffer(commandBuffers_[frameIndex_], 0), "Command buffer reset");
    recordCommands(commandBuffers_[frameIndex_], imageIndex, buffer, camera);
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &imageAvailable_[frameIndex_];
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffers_[frameIndex_];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinished_[imageIndex];
    require(vkQueueSubmit(graphicsQueue_, 1, &submit, inFlight_[frameIndex_]), "Graphics queue submission");
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinished_[imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &imageIndex;
    const VkResult result = vkQueuePresentKHR(presentQueue_, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window_.resized())
        recreateSwapchain();
    else if (result != VK_SUCCESS)
        throw std::runtime_error("Swapchain presentation failed (VkResult " + std::to_string(result) + ")");
    frameIndex_ = (frameIndex_ + 1) % kFramesInFlight;
}

void Renderer::cleanupSwapchain() noexcept {
    if (!device_) return;
    for (VkSemaphore semaphore : renderFinished_)
        if (semaphore) vkDestroySemaphore(device_, semaphore, nullptr);
    renderFinished_.clear();
    for (VkFramebuffer framebuffer : framebuffers_) if (framebuffer) vkDestroyFramebuffer(device_, framebuffer, nullptr);
    framebuffers_.clear();
    if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
    if (pipelineLayout_) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    pipelineLayout_ = VK_NULL_HANDLE;
    if (renderPass_) vkDestroyRenderPass(device_, renderPass_, nullptr);
    renderPass_ = VK_NULL_HANDLE;
    for (VkImageView view : imageViews_) if (view) vkDestroyImageView(device_, view, nullptr);
    imageViews_.clear();
    if (swapchain_) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
    swapchainImages_.clear();
}

void Renderer::recreateSwapchain() {
    window_.waitForUsableFramebuffer();
    require(vkDeviceWaitIdle(device_), "Device idle wait during resize");
    if (imguiVulkanInitialized_) { ImGui_ImplVulkan_Shutdown(); imguiVulkanInitialized_ = false; }
    cleanupSwapchain();
    createSwapchainObjects();
    createPresentSemaphores();
    initImGuiVulkan();
}

void Renderer::waitIdle() const noexcept { if (device_) vkDeviceWaitIdle(device_); }

void Renderer::cleanup() noexcept {
    if (device_) vkDeviceWaitIdle(device_);
    if (imguiVulkanInitialized_) { ImGui_ImplVulkan_Shutdown(); imguiVulkanInitialized_ = false; }
    if (imguiGlfwInitialized_) { ImGui_ImplGlfw_Shutdown(); imguiGlfwInitialized_ = false; }
    if (imguiContextCreated_) { ImGui::DestroyContext(); imguiContextCreated_ = false; }
    cleanupSwapchain();
    if (device_) {
        for (auto*& slot : instanceBuffers_) {
            if (!slot) continue;
            if (slot->mapped && slot->memory) vkUnmapMemory(device_, slot->memory);
            if (slot->buffer) vkDestroyBuffer(device_, slot->buffer, nullptr);
            if (slot->memory) vkFreeMemory(device_, slot->memory, nullptr);
            delete slot;
            slot = nullptr;
        }
        for (std::size_t i = 0; i < kFramesInFlight; ++i) {
            if (inFlight_[i]) vkDestroyFence(device_, inFlight_[i], nullptr);
            if (imageAvailable_[i]) vkDestroySemaphore(device_, imageAvailable_[i], nullptr);
        }
        if (commandPool_) vkDestroyCommandPool(device_, commandPool_, nullptr);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (surface_ && instance_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
    if (debugMessenger_ && instance_) {
        const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy) destroy(instance_, debugMessenger_, nullptr);
    }
    debugMessenger_ = VK_NULL_HANDLE;
    if (instance_) vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
}

} // namespace factory::engine
