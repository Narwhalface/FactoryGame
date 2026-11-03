# FactoryGame

FactoryGame is a purpose-built C++ factory-simulation engine inspired by the long production chains of GregTech New Horizons and the large automated worlds of Factorio and Satisfactory. The long-term goal is a highly scalable industrial simulation with many processing routes, machine types, logistics systems, and engineering-focused interfaces.

This repository currently contains the rendering and interaction foundation. It does **not** yet simulate recipes, inventories, energy, fluids, temperature, pressure, research, or logistics.

## Current capabilities

- Vulkan/GLFW renderer with Dear ImGui development tools
- Debug-build Vulkan validation and `VK_EXT_debug_utils` reporting when available
- Vulkan device compatibility checks, GPU/API reporting, resize and minimisation handling
- Top-down 2D camera with panning, cursor-anchored zoom, coordinate conversion, and reset
- GPU-instanced coloured machine quads with CPU visibility culling and one world draw call
- Shader-rendered optional grid, selection outline, and valid/invalid placement preview
- Dense grid world independent of Vulkan, GLFW, and ImGui
- Furnace, Crusher, Chemical Reactor, and Generator placeholders
- Placement, selection, inspection, and removal
- Deterministic 1,000, 10,000, 50,000, and 100,000-machine benchmark scenes
- Display-free CTest coverage for camera maths and world operations

## Prerequisites

- CMake 3.21 or newer
- A compiler with C++23 support (Visual Studio 2022 is supported)
- Vulkan SDK and a Vulkan-capable graphics driver
- Git and an internet connection for the first CMake configure

GLFW 3.4 and Dear ImGui 1.91.9b are pinned and fetched by CMake.

## Build and test

From a Developer PowerShell with `VULKAN_SDK` configured:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release --parallel
```

Run the multi-config Debug executable with:

```powershell
.\build\Debug\factory_game.exe
```

For a single-config generator, the configuration subdirectory may be omitted.

## Controls

- Left mouse: use the selected tool (place, select, or remove)
- Right mouse: remove a machine
- Middle-mouse drag or W/A/S/D: pan
- Mouse wheel: zoom around the cursor
- 1–4: choose Furnace, Crusher, Chemical Reactor, or Generator
- Escape: exit

Input is ignored when Dear ImGui requests mouse or keyboard capture.

## Architecture

- `engine/Application` owns the main loop and connects input, world, UI, and rendering.
- `engine/Window` owns GLFW lifetime and window events.
- `engine/renderer/Renderer` owns Vulkan resources, swapchain lifecycle, instancing, culling, and ImGui's Vulkan backend.
- `engine/renderer/Camera2D` contains display-independent camera maths.
- `engine/ui/DebugOverlay` exposes metrics, tools, benchmark selection, and inspection.
- `game/World` and `game/Machine` contain display-independent game state using a dense occupancy grid and ID-indexed machines.
- `factory_core` contains the world and camera code and has no Vulkan, GLFW, or ImGui dependency.

The renderer uses persistently mapped, host-coherent instance buffers per frame in flight. A scene/camera revision cache avoids unchanged uploads. Only visible machines are submitted; the grid, machines, selection, and placement preview share a single instanced draw.

## Benchmarks

Benchmark populations can be selected in the World controls panel. The overlay reports average CPU frame time, FPS, total and visible machines, submitted instances, draw calls, and allocated instance-buffer memory.

A finite run suitable for recording metrics is also available:

```powershell
.\build\Release\factory_game.exe --benchmark 100000 --frames 300
```

The first 30 frames are warm-up frames. The final console line reports measured CPU wall-clock frame time and FPS; it does not claim GPU timing. VSync/presentation mode and visible population affect results.

## Known limitations

- Machines are single-tile coloured placeholders with no textures or animation.
- Culling is a linear CPU scan of placed machines; spatial indexing is deferred until measurements justify it.
- Instance data uses host-visible memory for a simple, reliable upload path rather than a device-local staging system.
- Grid and machine interactions are intentionally 2D and single-layer.
- Automated tests cover non-graphics logic; interactive rendering still requires a Vulkan-capable desktop session.

## Next milestones

The next stage is the first functioning industrial machine: item inventories, a small data-driven recipe set, processing state, and energy consumption. That work should keep simulation state independent of graphics and establish deterministic simulation tests before expanding production chains.
