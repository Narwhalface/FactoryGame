#include "engine/renderer/Camera2D.hpp"
#include "game/World.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void check(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

bool near(float a, float b) { return std::abs(a - b) < 0.001f; }

void testCamera() {
    factory::engine::Camera2D camera;
    camera.setViewport(1000.0f, 600.0f);
    camera.setPosition({20.0f, 10.0f});
    camera.setZoom(25.0f);
    const auto screen = camera.worldToScreen({24.0f, 8.0f});
    check(near(screen.x, 600.0f) && near(screen.y, 250.0f), "world-to-screen conversion");
    const auto world = camera.screenToWorld(screen);
    check(near(world.x, 24.0f) && near(world.y, 8.0f), "camera transform round trip");
    camera.setZoom(-100.0f);
    check(near(camera.zoom(), 2.0f), "minimum camera zoom");
    camera.setZoom(1'000.0f);
    check(near(camera.zoom(), 96.0f), "maximum camera zoom");
    camera.setZoom(20.0f);
    const auto anchorBefore = camera.screenToWorld({150.0f, 120.0f});
    camera.zoomAt(2.0f, {150.0f, 120.0f});
    const auto anchorAfter = camera.screenToWorld({150.0f, 120.0f});
    check(near(anchorBefore.x, anchorAfter.x) && near(anchorBefore.y, anchorAfter.y), "cursor-anchored zoom");
}

void testWorld() {
    factory::game::World world(4, 3);
    check(world.contains(0, 0) && world.contains(3, 2), "valid world boundaries");
    check(!world.contains(-1, 0) && !world.contains(4, 2) && !world.contains(2, 3), "invalid world boundaries");
    const auto first = world.place(1, 1, factory::game::MachineType::Furnace);
    check(first.has_value() && *first == 1, "first machine identifier");
    check(world.occupied(1, 1) && world.machineAt(1, 1) == *first, "tile occupancy query");
    check(!world.place(1, 1, factory::game::MachineType::Crusher), "duplicate placement prevention");
    check(!world.place(-1, 0, factory::game::MachineType::Crusher), "out-of-bounds placement prevention");
    const auto second = world.place(2, 1, factory::game::MachineType::Generator);
    check(second.has_value() && *second == 2, "monotonic machine identifiers");
    check(world.find(*second) && world.find(*second)->type == factory::game::MachineType::Generator,
          "machine selection/query");
    check(world.remove(1, 1) && !world.occupied(1, 1), "machine removal by tile");
    check(!world.remove(1, 1), "empty tile removal");
    check(world.remove(*second) && world.machineCount() == 0, "machine removal by identifier");
    check(!world.remove(999), "unknown identifier removal");

    check(world.populateBenchmark(99) == 12 && world.machineCount() == 12, "benchmark capacity clamp");
    check(world.machineAt(0, 0) != 0 && world.machineAt(3, 2) != 0, "tile coordinate mapping");
}

} // namespace

int main() {
    try {
        testCamera();
        testWorld();
        std::cout << "All FactoryGame core tests passed.\n";
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
