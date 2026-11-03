#pragma once

#include "engine/Window.hpp"
#include "engine/renderer/Camera2D.hpp"
#include "engine/renderer/Renderer.hpp"
#include "game/World.hpp"

#include <cstddef>

namespace factory::engine {

enum class InteractionMode { Select, Place, Remove };

struct EditorState {
    game::MachineType machineType{game::MachineType::Furnace};
    InteractionMode mode{InteractionMode::Place};
    bool showGrid{true};
    game::MachineId selected{};
    std::size_t benchmarkRequest{};
    bool resetCamera{};
};

class DebugOverlay {
public:
    void draw(const Window& window, const Renderer& renderer, const game::World& world,
              const Camera2D& camera, EditorState& editor, double averageCpuFrameMs);
};

} // namespace factory::engine
