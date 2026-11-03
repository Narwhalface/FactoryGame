#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <string_view>

namespace factory::engine {

class Window {
public:
    Window(int width, int height, std::string_view title);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] GLFWwindow* native() const noexcept { return handle_; }
    [[nodiscard]] bool shouldClose() const noexcept;
    void pollEvents() noexcept;
    void waitForUsableFramebuffer() const;
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] int framebufferWidth() const noexcept;
    [[nodiscard]] int framebufferHeight() const noexcept;
    [[nodiscard]] bool resized() noexcept;
    [[nodiscard]] double consumeScroll() noexcept;

private:
    static void framebufferCallback(GLFWwindow* window, int, int);
    static void scrollCallback(GLFWwindow* window, double, double yOffset);

    GLFWwindow* handle_{};
    bool resized_{};
    double scroll_{};
};

} // namespace factory::engine
