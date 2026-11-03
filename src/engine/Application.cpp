#include "engine/Application.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace factory::engine {

Application::Application() {
    camera_.reset({50.0f, 25.0f}, 16.0f);
}

Vec2 Application::framebufferMousePosition() const {
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window_.native(), &x, &y);
    const float scaleX = static_cast<float>(window_.framebufferWidth()) / std::max(window_.width(), 1);
    const float scaleY = static_cast<float>(window_.framebufferHeight()) / std::max(window_.height(), 1);
    return {static_cast<float>(x) * scaleX, static_cast<float>(y) * scaleY};
}

std::optional<PlacementPreview> Application::placementPreview() const {
    if (editor_.mode != InteractionMode::Place || ImGui::GetIO().WantCaptureMouse) return std::nullopt;
    const Vec2 world = camera_.screenToWorld(framebufferMousePosition());
    const int x = static_cast<int>(std::floor(world.x));
    const int y = static_cast<int>(std::floor(world.y));
    if (!world_.contains(x, y)) return std::nullopt;
    return PlacementPreview{x, y, !world_.occupied(x, y)};
}

void Application::processInput(float deltaSeconds) {
    GLFWwindow* handle = window_.native();
    if (glfwGetKey(handle, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(handle, GLFW_TRUE);
    const ImGuiIO& io = ImGui::GetIO();
    const Vec2 mouse = framebufferMousePosition();
    camera_.setViewport(static_cast<float>(window_.framebufferWidth()),
                        static_cast<float>(window_.framebufferHeight()));
    const double scroll = window_.consumeScroll();
    if (!io.WantCaptureMouse && scroll != 0.0)
        camera_.zoomAt(std::pow(1.15f, static_cast<float>(scroll)), mouse);

    const bool middle = glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    if (!io.WantCaptureMouse && middle && middleDragging_)
        camera_.pan({(previousMouse_.x - mouse.x) / camera_.zoom(),
                     (previousMouse_.y - mouse.y) / camera_.zoom()});
    middleDragging_ = middle;
    previousMouse_ = mouse;

    if (!io.WantCaptureKeyboard) {
        Vec2 movement{};
        if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS) movement.x -= 1.0f;
        if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS) movement.x += 1.0f;
        if (glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS) movement.y -= 1.0f;
        if (glfwGetKey(handle, GLFW_KEY_S) == GLFW_PRESS) movement.y += 1.0f;
        const float speed = 650.0f / camera_.zoom() * deltaSeconds;
        camera_.pan({movement.x * speed, movement.y * speed});
        for (int i = 0; i < 4; ++i)
            if (glfwGetKey(handle, GLFW_KEY_1 + i) == GLFW_PRESS)
                editor_.machineType = static_cast<game::MachineType>(i);
    }

    const bool left = glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool right = glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (!io.WantCaptureMouse) {
        const Vec2 worldPosition = camera_.screenToWorld(mouse);
        const int x = static_cast<int>(std::floor(worldPosition.x));
        const int y = static_cast<int>(std::floor(worldPosition.y));
        if (left && !previousLeft_) {
            if (editor_.mode == InteractionMode::Place) {
                if (const auto id = world_.place(x, y, editor_.machineType)) editor_.selected = *id;
            } else if (editor_.mode == InteractionMode::Remove) {
                if (world_.machineAt(x, y) == editor_.selected) editor_.selected = 0;
                world_.remove(x, y);
            } else editor_.selected = world_.machineAt(x, y);
        }
        if (right && !previousRight_) {
            if (world_.machineAt(x, y) == editor_.selected) editor_.selected = 0;
            world_.remove(x, y);
        }
    }
    previousLeft_ = left;
    previousRight_ = right;
}

void Application::run(std::size_t benchmarkPopulation, std::size_t frameLimit) {
    if (benchmarkPopulation) {
        world_.populateBenchmark(benchmarkPopulation);
        camera_.reset({world_.width() * 0.5f, world_.height() * 0.5f}, 2.0f);
    }
    auto previous = std::chrono::steady_clock::now();
    std::size_t frameCount = 0;
    double benchmarkTotalMs = 0.0;
    std::size_t benchmarkSamples = 0;
    while (!window_.shouldClose() && (frameLimit == 0 || frameCount < frameLimit)) {
        const auto frameStart = std::chrono::steady_clock::now();
        const float delta = std::chrono::duration<float>(frameStart - previous).count();
        previous = frameStart;
        window_.pollEvents();
        renderer_.beginUiFrame();
        processInput(std::min(delta, 0.1f));
        overlay_.draw(window_, renderer_, world_, camera_, editor_, averageCpuFrameMs_);
        if (editor_.resetCamera) {
            camera_.reset({50.0f, 35.0f}, 20.0f);
            editor_.resetCamera = false;
        }
        if (editor_.benchmarkRequest) {
            if (editor_.benchmarkRequest == static_cast<std::size_t>(-1)) world_.clear();
            else world_.populateBenchmark(editor_.benchmarkRequest);
            editor_.selected = 0;
            editor_.benchmarkRequest = 0;
        }
        renderer_.endUiFrame();
        renderer_.draw(world_, camera_, editor_.showGrid, editor_.selected, placementPreview());
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - frameStart).count();
        averageCpuFrameMs_ = averageCpuFrameMs_ == 0.0 ? elapsed : averageCpuFrameMs_ * 0.95 + elapsed * 0.05;
        if (frameCount >= 30) { benchmarkTotalMs += elapsed; ++benchmarkSamples; }
        ++frameCount;
    }
    renderer_.waitIdle();
    if (frameLimit) {
        const double meanMs = benchmarkSamples ? benchmarkTotalMs / static_cast<double>(benchmarkSamples) : 0.0;
        const auto& stats = renderer_.stats();
        std::cout << "BENCHMARK population=" << world_.machineCount()
                  << " frames=" << benchmarkSamples
                  << " avg_cpu_ms=" << meanMs
                  << " avg_fps=" << (meanMs > 0.0 ? 1000.0 / meanMs : 0.0)
                  << " visible=" << stats.visibleInstances
                  << " submitted=" << stats.submittedInstances
                  << " draw_calls=" << stats.drawCalls
                  << " instance_buffer_bytes=" << stats.instanceBufferBytes << '\n';
    }
}

} // namespace factory::engine
