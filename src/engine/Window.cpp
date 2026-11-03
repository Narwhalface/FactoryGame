#include "engine/Window.hpp"

#include <stdexcept>
#include <string>

namespace factory::engine {

Window::Window(int width, int height, std::string_view title) {
    if (glfwInit() != GLFW_TRUE) throw std::runtime_error("GLFW initialization failed");
    if (glfwVulkanSupported() != GLFW_TRUE) {
        glfwTerminate();
        throw std::runtime_error("GLFW could not find a Vulkan loader");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    const std::string ownedTitle(title);
    handle_ = glfwCreateWindow(width, height, ownedTitle.c_str(), nullptr, nullptr);
    if (!handle_) {
        glfwTerminate();
        throw std::runtime_error("Window creation failed");
    }
    glfwSetWindowUserPointer(handle_, this);
    glfwSetFramebufferSizeCallback(handle_, framebufferCallback);
    glfwSetScrollCallback(handle_, scrollCallback);
}

Window::~Window() {
    if (handle_) glfwDestroyWindow(handle_);
    glfwTerminate();
}

bool Window::shouldClose() const noexcept { return glfwWindowShouldClose(handle_) == GLFW_TRUE; }
void Window::pollEvents() noexcept { glfwPollEvents(); }

void Window::waitForUsableFramebuffer() const {
    while (framebufferWidth() == 0 || framebufferHeight() == 0) glfwWaitEvents();
}

int Window::width() const noexcept { int w, h; glfwGetWindowSize(handle_, &w, &h); return w; }
int Window::height() const noexcept { int w, h; glfwGetWindowSize(handle_, &w, &h); return h; }
int Window::framebufferWidth() const noexcept { int w, h; glfwGetFramebufferSize(handle_, &w, &h); return w; }
int Window::framebufferHeight() const noexcept { int w, h; glfwGetFramebufferSize(handle_, &w, &h); return h; }

bool Window::resized() noexcept { const bool value = resized_; resized_ = false; return value; }
double Window::consumeScroll() noexcept { const double value = scroll_; scroll_ = 0.0; return value; }

void Window::framebufferCallback(GLFWwindow* window, int, int) {
    static_cast<Window*>(glfwGetWindowUserPointer(window))->resized_ = true;
}

void Window::scrollCallback(GLFWwindow* window, double, double yOffset) {
    static_cast<Window*>(glfwGetWindowUserPointer(window))->scroll_ += yOffset;
}

} // namespace factory::engine
