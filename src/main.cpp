#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kWidth = 960;
constexpr uint32_t kHeight = 540;
constexpr int kFramesInFlight = 2;
const std::vector<const char*> kDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    [[nodiscard]] bool complete() const { return graphics.has_value() && present.has_value(); }
};

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

std::vector<char> readFile(const std::string& filename) {
    std::string path = "shaders/" + filename;
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        path = std::string(HELLO_TRIANGLE_SHADER_DIR) + "/" + filename;
        file.open(path, std::ios::ate | std::ios::binary);
    }
    if (!file) {
        throw std::runtime_error("Could not open shader: " + path);
    }

    const auto size = static_cast<size_t>(file.tellg());
    std::vector<char> bytes(size);
    file.seekg(0);
    file.read(bytes.data(), static_cast<std::streamsize>(size));
    return bytes;
}

class HelloTriangleEngine {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    VkFormat swapchainFormat_{};
    VkExtent2D swapchainExtent_{};

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, kFramesInFlight> commandBuffers_{};

    std::array<VkSemaphore, kFramesInFlight> imageAvailable_{};
    std::array<VkSemaphore, kFramesInFlight> renderFinished_{};
    std::array<VkFence, kFramesInFlight> inFlight_{};
    size_t currentFrame_ = 0;
    bool framebufferResized_ = false;

    static void framebufferResizeCallback(GLFWwindow* window, int, int) {
        auto* app = static_cast<HelloTriangleEngine*>(glfwGetWindowUserPointer(window));
        app->framebufferResized_ = true;
    }

    static void keyCallback(GLFWwindow* window, int key, int, int action, int) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    void initWindow() {
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("GLFW initialization failed");
        }
        if (glfwVulkanSupported() != GLFW_TRUE) {
            throw std::runtime_error("GLFW could not find a Vulkan loader");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        window_ = glfwCreateWindow(static_cast<int>(kWidth), static_cast<int>(kHeight),
                                   "Vulkan Hello Triangle", nullptr, nullptr);
        if (!window_) {
            throw std::runtime_error("Window creation failed");
        }
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
        glfwSetKeyCallback(window_, keyCallback);
    }

    void initVulkan() {
        createInstance();
        createSurface();
        selectPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
        createSwapchainObjects();
        createCommandBuffers();
        createSyncObjects();
    }

    void createInstance() {
        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName = "Vulkan Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "FactoryGame Mini Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        uint32_t extensionCount = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        if (!extensions || extensionCount == 0) {
            throw std::runtime_error("GLFW returned no required Vulkan extensions");
        }

        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.pApplicationInfo = &appInfo;
        info.enabledExtensionCount = extensionCount;
        info.ppEnabledExtensionNames = extensions;

        if (vkCreateInstance(&info, nullptr, &instance_) != VK_SUCCESS) {
            throw std::runtime_error("Vulkan instance creation failed");
        }
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
            throw std::runtime_error("Window surface creation failed");
        }
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const {
        QueueFamilyIndices result;
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

        for (uint32_t i = 0; i < count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                result.graphics = i;
            }
            VkBool32 supportsPresent = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &supportsPresent);
            if (supportsPresent == VK_TRUE) {
                result.present = i;
            }
            if (result.complete()) break;
        }
        return result;
    }

    bool supportsDeviceExtensions(VkPhysicalDevice device) const {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available(count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

        std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
        for (const auto& extension : available) required.erase(extension.extensionName);
        return required.empty();
    }

    SwapchainSupport querySwapchainSupport(VkPhysicalDevice device) const {
        SwapchainSupport support;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &support.capabilities);

        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, nullptr);
        support.formats.resize(count);
        if (count) vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, support.formats.data());

        count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, nullptr);
        support.presentModes.resize(count);
        if (count) vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, support.presentModes.data());
        return support;
    }

    bool isDeviceSuitable(VkPhysicalDevice device) const {
        const auto indices = findQueueFamilies(device);
        if (!indices.complete() || !supportsDeviceExtensions(device)) return false;
        const auto support = querySwapchainSupport(device);
        return !support.formats.empty() && !support.presentModes.empty();
    }

    void selectPhysicalDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance_, &count, nullptr);
        if (!count) throw std::runtime_error("No Vulkan-capable GPU was found");

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance_, &count, devices.data());
        for (const auto device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice_ = device;
                break;
            }
        }
        if (physicalDevice_ == VK_NULL_HANDLE) {
            throw std::runtime_error("No GPU supports the required graphics, presentation, and swapchain features");
        }
    }

    void createLogicalDevice() {
        const auto indices = findQueueFamilies(physicalDevice_);
        const std::set<uint32_t> uniqueFamilies = {*indices.graphics, *indices.present};
        constexpr float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        for (uint32_t family : uniqueFamilies) {
            VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueInfo.queueFamilyIndex = family;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &priority;
            queueInfos.push_back(queueInfo);
        }

        VkPhysicalDeviceFeatures features{};
        VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        info.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
        info.pQueueCreateInfos = queueInfos.data();
        info.pEnabledFeatures = &features;
        info.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
        info.ppEnabledExtensionNames = kDeviceExtensions.data();

        if (vkCreateDevice(physicalDevice_, &info, nullptr, &device_) != VK_SUCCESS) {
            throw std::runtime_error("Logical device creation failed");
        }
        vkGetDeviceQueue(device_, *indices.graphics, 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, *indices.present, 0, &presentQueue_);
    }

    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return format;
        }
        return formats.front();
    }

    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const {
        for (const auto mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        return {
            std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width,
                       capabilities.maxImageExtent.width),
            std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height,
                       capabilities.maxImageExtent.height)
        };
    }

    void createSwapchain() {
        const auto support = querySwapchainSupport(physicalDevice_);
        const auto surfaceFormat = chooseSurfaceFormat(support.formats);
        const auto presentMode = choosePresentMode(support.presentModes);
        const auto extent = chooseExtent(support.capabilities);

        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount && imageCount > support.capabilities.maxImageCount) {
            imageCount = support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = surface_;
        info.minImageCount = imageCount;
        info.imageFormat = surfaceFormat.format;
        info.imageColorSpace = surfaceFormat.colorSpace;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        const auto indices = findQueueFamilies(physicalDevice_);
        const uint32_t families[] = {*indices.graphics, *indices.present};
        if (indices.graphics != indices.present) {
            info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = families;
        } else {
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        info.preTransform = support.capabilities.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = presentMode;
        info.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_) != VK_SUCCESS) {
            throw std::runtime_error("Swapchain creation failed");
        }
        vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
        swapchainImages_.resize(imageCount);
        vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());
        swapchainFormat_ = surfaceFormat.format;
        swapchainExtent_ = extent;
    }

    void createImageViews() {
        swapchainImageViews_.resize(swapchainImages_.size());
        for (size_t i = 0; i < swapchainImages_.size(); ++i) {
            VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            info.image = swapchainImages_[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = swapchainFormat_;
            info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;
            if (vkCreateImageView(device_, &info, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) {
                throw std::runtime_error("Swapchain image-view creation failed");
            }
        }
    }

    void createRenderPass() {
        VkAttachmentDescription color{};
        color.format = swapchainFormat_;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;

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
        if (vkCreateRenderPass(device_, &info, nullptr, &renderPass_) != VK_SUCCESS) {
            throw std::runtime_error("Render-pass creation failed");
        }
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) const {
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = code.size();
        info.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device_, &info, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("Shader-module creation failed");
        }
        return module;
    }

    void createGraphicsPipeline() {
        const auto vertexCode = readFile("triangle.vert.spv");
        const auto fragmentCode = readFile("triangle.frag.spv");
        const auto vertexModule = createShaderModule(vertexCode);
        const auto fragmentModule = createShaderModule(fragmentCode);

        VkPipelineShaderStageCreateInfo vertexStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertexModule;
        vertexStage.pName = "main";
        VkPipelineShaderStageCreateInfo fragmentStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = fragmentModule;
        fragmentStage.pName = "main";
        const VkPipelineShaderStageCreateInfo stages[] = {vertexStage, fragmentStage};

        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            vkDestroyShaderModule(device_, fragmentModule, nullptr);
            vkDestroyShaderModule(device_, vertexModule, nullptr);
            throw std::runtime_error("Pipeline-layout creation failed");
        }

        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &inputAssembly;
        info.pViewportState = &viewportState;
        info.pRasterizationState = &rasterizer;
        info.pMultisampleState = &multisampling;
        info.pColorBlendState = &colorBlending;
        info.pDynamicState = &dynamicState;
        info.layout = pipelineLayout_;
        info.renderPass = renderPass_;
        info.subpass = 0;

        const VkResult result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr,
                                                           &graphicsPipeline_);
        vkDestroyShaderModule(device_, fragmentModule, nullptr);
        vkDestroyShaderModule(device_, vertexModule, nullptr);
        if (result != VK_SUCCESS) throw std::runtime_error("Graphics-pipeline creation failed");
    }

    void createFramebuffers() {
        swapchainFramebuffers_.resize(swapchainImageViews_.size());
        for (size_t i = 0; i < swapchainImageViews_.size(); ++i) {
            VkImageView attachments[] = {swapchainImageViews_[i]};
            VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            info.renderPass = renderPass_;
            info.attachmentCount = 1;
            info.pAttachments = attachments;
            info.width = swapchainExtent_.width;
            info.height = swapchainExtent_.height;
            info.layers = 1;
            if (vkCreateFramebuffer(device_, &info, nullptr, &swapchainFramebuffers_[i]) != VK_SUCCESS) {
                throw std::runtime_error("Framebuffer creation failed");
            }
        }
    }

    void createCommandPool() {
        const auto indices = findQueueFamilies(physicalDevice_);
        VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        info.queueFamilyIndex = *indices.graphics;
        if (vkCreateCommandPool(device_, &info, nullptr, &commandPool_) != VK_SUCCESS) {
            throw std::runtime_error("Command-pool creation failed");
        }
    }

    void createCommandBuffers() {
        VkCommandBufferAllocateInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        info.commandPool = commandPool_;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
        if (vkAllocateCommandBuffers(device_, &info, commandBuffers_.data()) != VK_SUCCESS) {
            throw std::runtime_error("Command-buffer allocation failed");
        }
    }

    void recordCommandBuffer(VkCommandBuffer buffer, uint32_t imageIndex) const {
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if (vkBeginCommandBuffer(buffer, &begin) != VK_SUCCESS) {
            throw std::runtime_error("Command-buffer recording failed");
        }

        const VkClearValue clearColor = {{{0.025f, 0.03f, 0.05f, 1.0f}}};
        VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderPassInfo.renderPass = renderPass_;
        renderPassInfo.framebuffer = swapchainFramebuffers_[imageIndex];
        renderPassInfo.renderArea.extent = swapchainExtent_;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchainExtent_.width);
        viewport.height = static_cast<float>(swapchainExtent_.height);
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(buffer, 0, 1, &viewport);
        VkRect2D scissor{{0, 0}, swapchainExtent_};
        vkCmdSetScissor(buffer, 0, 1, &scissor);
        vkCmdDraw(buffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(buffer);

        if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
            throw std::runtime_error("Command-buffer finalization failed");
        }
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (int i = 0; i < kFramesInFlight; ++i) {
            if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailable_[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinished_[i]) != VK_SUCCESS ||
                vkCreateFence(device_, &fenceInfo, nullptr, &inFlight_[i]) != VK_SUCCESS) {
                throw std::runtime_error("Frame synchronization-object creation failed");
            }
        }
    }

    void createSwapchainObjects() {
        createSwapchain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
    }

    void cleanupSwapchain() {
        for (const auto framebuffer : swapchainFramebuffers_) vkDestroyFramebuffer(device_, framebuffer, nullptr);
        swapchainFramebuffers_.clear();
        vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        for (const auto imageView : swapchainImageViews_) vkDestroyImageView(device_, imageView, nullptr);
        swapchainImageViews_.clear();
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    void recreateSwapchain() {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        while (width == 0 || height == 0) {
            glfwWaitEvents();
            glfwGetFramebufferSize(window_, &width, &height);
        }
        vkDeviceWaitIdle(device_);
        cleanupSwapchain();
        createSwapchainObjects();
    }

    void drawFrame() {
        vkWaitForFences(device_, 1, &inFlight_[currentFrame_], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = 0;
        const VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                                        imageAvailable_[currentFrame_], VK_NULL_HANDLE,
                                                        &imageIndex);
        if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Could not acquire a swapchain image");
        }

        vkResetFences(device_, 1, &inFlight_[currentFrame_]);
        vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
        recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

        const VkSemaphore waitSemaphores[] = {imageAvailable_[currentFrame_]};
        const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        const VkSemaphore signalSemaphores[] = {renderFinished_[currentFrame_]};
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = waitSemaphores;
        submit.pWaitDstStageMask = waitStages;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffers_[currentFrame_];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(graphicsQueue_, 1, &submit, inFlight_[currentFrame_]) != VK_SUCCESS) {
            throw std::runtime_error("Graphics-queue submission failed");
        }

        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = signalSemaphores;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain_;
        present.pImageIndices = &imageIndex;
        const VkResult result = vkQueuePresentKHR(presentQueue_, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_) {
            framebufferResized_ = false;
            recreateSwapchain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Swapchain presentation failed");
        }
        currentFrame_ = (currentFrame_ + 1) % kFramesInFlight;
    }

    void mainLoop() {
        while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device_);
    }

    void cleanup() {
        cleanupSwapchain();
        for (int i = 0; i < kFramesInFlight; ++i) {
            vkDestroyFence(device_, inFlight_[i], nullptr);
            vkDestroySemaphore(device_, renderFinished_[i], nullptr);
            vkDestroySemaphore(device_, imageAvailable_[i], nullptr);
        }
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        vkDestroyDevice(device_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
};

} // namespace

int main() {
    try {
        HelloTriangleEngine engine;
        engine.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
