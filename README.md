# Vulkan Hello Triangle

A minimal C++20 Vulkan renderer that opens a GLFW window and draws a colored triangle. Press **Escape** or close the window to exit. The swapchain is recreated when the window is resized.

## Prerequisites

- CMake 3.21 or newer
- A C++20 compiler
- The [Vulkan SDK](https://vulkan.lunarg.com/) and a Vulkan-capable graphics driver
- Git (only needed if CMake must download GLFW)

GLFW 3.3+ may be installed system-wide. If CMake cannot find it, GLFW 3.4 is fetched automatically.

## Build on Windows

From a Developer PowerShell with `VULKAN_SDK` configured:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\hello_triangle.exe
```

For a single-config generator such as Ninja, the executable is usually `build\hello_triangle.exe`.

