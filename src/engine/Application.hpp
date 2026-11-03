#pragma once

#include "engine/Window.hpp"
#include "engine/renderer/Camera2D.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/ui/DebugOverlay.hpp"
#include "game/World.hpp"

#include <optional>
#include <cstddef>

namespace factory::engine {

class Application {
public:
    Application();
    void run(std::size_t benchmarkPopulation = 0, std::size_t frameLimit = 0);

private:
    void processInput(float deltaSeconds);
    [[nodiscard]] Vec2 framebufferMousePosition() const;
    [[nodiscard]] std::optional<PlacementPreview> placementPreview() const;

    Window window_{1280, 720, "FactoryGame"};
    Renderer renderer_{window_};
    game::World world_{400, 250};
    Camera2D camera_;
    DebugOverlay overlay_;
    EditorState editor_;
    bool middleDragging_{};
    bool previousLeft_{};
    bool previousRight_{};
    Vec2 previousMouse_{};
    double averageCpuFrameMs_{};
};

} // namespace factory::engine
