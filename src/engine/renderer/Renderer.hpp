#pragma once

#include "engine/Window.hpp"
#include "engine/renderer/Camera2D.hpp"
#include "game/World.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace factory::engine {

struct RenderStats {
    std::size_t totalInstances{};
    std::size_t visibleInstances{};
    std::size_t submittedInstances{};
    std::uint32_t drawCalls{};
    std::size_t instanceBufferBytes{};
};

struct PlacementPreview {
    int x{};
    int y{};
    bool valid{};
};

class Renderer {
public:
    explicit Renderer(Window& window);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void beginUiFrame();
    void endUiFrame();
    void draw(const game::World& world, const Camera2D& camera, bool showGrid,
              game::MachineId selected, std::optional<PlacementPreview> preview);
    void waitIdle() const noexcept;

    [[nodiscard]] const RenderStats& stats() const noexcept { return stats_; }
    [[nodiscard]] const std::string& gpuName() const noexcept { return gpuName_; }
    [[nodiscard]] const std::string& apiVersion() const noexcept { return apiVersion_; }

private:
    static constexpr std::size_t kFramesInFlight = 2;
    static constexpr std::size_t kInstanceCapacity = 100'002;

    struct QueueFamilies;
    struct SwapchainSupport;
    struct Instance;
    struct InstanceBuffer;

    void initialize();
    void cleanup() noexcept;
    void createInstance();
    void createDebugMessenger();
    void createSurface();
    void selectPhysicalDevice();
    void createDevice();
    void createCommandPool();
    void createSwapchainObjects();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createPipeline();
    void createFramebuffers();
    void createCommandBuffers();
    void createSyncObjects();
    void createPresentSemaphores();
    void createInstanceBuffers();
    void initImGui();
    void initImGuiVulkan();
    void recreateSwapchain();
    void cleanupSwapchain() noexcept;
    void updateInstances(InstanceBuffer& buffer, const game::World& world, const Camera2D& camera,
                         bool showGrid, game::MachineId selected,
                         const std::optional<PlacementPreview>& preview);
    void recordCommands(VkCommandBuffer commandBuffer, std::uint32_t imageIndex,
                        const InstanceBuffer& instances, const Camera2D& camera);

    [[nodiscard]] QueueFamilies findQueueFamilies(VkPhysicalDevice device) const;
    [[nodiscard]] SwapchainSupport querySwapchain(VkPhysicalDevice device) const;
    [[nodiscard]] bool deviceSuitable(VkPhysicalDevice device) const;
    [[nodiscard]] std::uint32_t findMemoryType(std::uint32_t filter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& code) const;

    Window& window_;
    VkInstance instance_{};
    VkDebugUtilsMessengerEXT debugMessenger_{};
    VkSurfaceKHR surface_{};
    VkPhysicalDevice physicalDevice_{};
    VkDevice device_{};
    VkQueue graphicsQueue_{};
    VkQueue presentQueue_{};
    VkSwapchainKHR swapchain_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkFramebuffer> framebuffers_;
    VkFormat swapchainFormat_{};
    VkExtent2D swapchainExtent_{};
    VkRenderPass renderPass_{};
    VkPipelineLayout pipelineLayout_{};
    VkPipeline pipeline_{};
    VkCommandPool commandPool_{};
    std::array<VkCommandBuffer, kFramesInFlight> commandBuffers_{};
    std::array<VkSemaphore, kFramesInFlight> imageAvailable_{};
    std::vector<VkSemaphore> renderFinished_;
    std::array<VkFence, kFramesInFlight> inFlight_{};
    std::array<InstanceBuffer*, kFramesInFlight> instanceBuffers_{};
    std::size_t frameIndex_{};
    bool validationEnabled_{};
    bool imguiContextCreated_{};
    bool imguiGlfwInitialized_{};
    bool imguiVulkanInitialized_{};
    std::uint32_t vulkanApiVersion_{VK_API_VERSION_1_0};
    std::string gpuName_{"Unknown"};
    std::string apiVersion_{"1.0"};
    RenderStats stats_{};
};

} // namespace factory::engine
