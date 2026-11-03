#include "engine/ui/DebugOverlay.hpp"

#include <imgui.h>

#include <array>
#include <string>

namespace factory::engine {

void DebugOverlay::draw(const Window& window, const Renderer& renderer, const game::World& world,
                        const Camera2D& camera, EditorState& editor, double averageCpuFrameMs) {
    ImGui::SetNextWindowPos({12.0f, 12.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({370.0f, 560.0f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("FactoryGame Development");
    if (ImGui::CollapsingHeader("Engine", ImGuiTreeNodeFlags_DefaultOpen)) {
        const float fps = ImGui::GetIO().Framerate;
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame time: %.3f ms", fps > 0.0f ? 1000.0f / fps : 0.0f);
        ImGui::Text("Average CPU frame: %.3f ms", averageCpuFrameMs);
        ImGui::Text("GPU: %s", renderer.gpuName().c_str());
        ImGui::Text("Vulkan API: %s", renderer.apiVersion().c_str());
        ImGui::Text("Window: %d x %d", window.width(), window.height());
        ImGui::Text("Framebuffer: %d x %d", window.framebufferWidth(), window.framebufferHeight());
    }
    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& stats = renderer.stats();
        ImGui::Text("Total machines: %zu", stats.totalInstances);
        ImGui::Text("Visible machines: %zu", stats.visibleInstances);
        ImGui::Text("Submitted instances: %zu", stats.submittedInstances);
        ImGui::Text("World draw calls: %u", stats.drawCalls);
        ImGui::Text("Instance buffers: %.2f MiB",
                    static_cast<double>(stats.instanceBufferBytes) / (1024.0 * 1024.0));
        ImGui::Text("Camera: %.1f, %.1f @ %.1f px/tile",
                    camera.position().x, camera.position().y, camera.zoom());
    }
    if (ImGui::CollapsingHeader("World controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        constexpr std::array<const char*, 4> names{"Furnace", "Crusher", "Chemical Reactor", "Generator"};
        int machineType = static_cast<int>(editor.machineType);
        ImGui::Combo("Machine", &machineType, names.data(), static_cast<int>(names.size()));
        editor.machineType = static_cast<game::MachineType>(machineType);
        int mode = static_cast<int>(editor.mode);
        ImGui::RadioButton("Select", &mode, static_cast<int>(InteractionMode::Select)); ImGui::SameLine();
        ImGui::RadioButton("Place", &mode, static_cast<int>(InteractionMode::Place)); ImGui::SameLine();
        ImGui::RadioButton("Remove", &mode, static_cast<int>(InteractionMode::Remove));
        editor.mode = static_cast<InteractionMode>(mode);
        ImGui::Checkbox("Show grid", &editor.showGrid);
        if (ImGui::Button("Reset camera")) editor.resetCamera = true;
        ImGui::SeparatorText("Deterministic benchmark scene");
        for (const std::size_t count : {1'000u, 10'000u, 50'000u, 100'000u}) {
            if (ImGui::Button((std::to_string(count) + " machines").c_str())) editor.benchmarkRequest = count;
            if (count != 100'000u) ImGui::SameLine();
        }
        if (ImGui::Button("Clear world")) editor.benchmarkRequest = static_cast<std::size_t>(-1);
    }
    if (ImGui::CollapsingHeader("Machine inspector", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (const auto* machine = world.find(editor.selected)) {
            ImGui::Text("ID: %llu", static_cast<unsigned long long>(machine->id));
            ImGui::Text("Type: %s", game::machineTypeName(machine->type).data());
            ImGui::Text("Grid: %d, %d", machine->x, machine->y);
            ImGui::Text("Operational: %s (placeholder)", machine->operational ? "yes" : "no");
        } else ImGui::TextDisabled("No machine selected");
    }
    ImGui::Separator();
    ImGui::TextDisabled("LMB: active tool | RMB: remove | MMB/WASD: pan");
    ImGui::TextDisabled("Wheel: zoom | 1-4: machine type | Esc: exit");
    ImGui::End();
}

} // namespace factory::engine
