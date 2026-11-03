#version 450

layout(location = 0) in vec2 instancePosition;
layout(location = 1) in vec2 instanceSize;
layout(location = 2) in vec4 instanceColor;
layout(location = 3) in float instanceMode;

layout(push_constant) uniform Camera {
    vec2 position;
    vec2 viewport;
    float zoom;
} camera;

layout(location = 0) out vec4 color;
layout(location = 1) out vec2 localPosition;
layout(location = 2) out vec2 worldPosition;
layout(location = 3) flat out float mode;

const vec2 vertices[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
);

void main() {
    vec2 local = vertices[gl_VertexIndex];
    vec2 world = instancePosition + local * instanceSize;
    vec2 screen = (world - camera.position) * camera.zoom + camera.viewport * 0.5;
    // Vulkan's positive-height viewport maps NDC -1 to the framebuffer top.
    // Camera and GLFW screen coordinates also start at the top, so Y must not
    // use the OpenGL-style inversion here.
    vec2 ndc = vec2(screen.x / camera.viewport.x * 2.0 - 1.0,
                    screen.y / camera.viewport.y * 2.0 - 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    color = instanceColor;
    localPosition = local;
    worldPosition = world;
    mode = instanceMode;
}
