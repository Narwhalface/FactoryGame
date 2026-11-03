#version 450

layout(location = 0) in vec4 color;
layout(location = 1) in vec2 localPosition;
layout(location = 2) in vec2 worldPosition;
layout(location = 3) flat in float mode;
layout(location = 0) out vec4 outColor;

void main() {
    if (mode > 0.5 && mode < 1.5) {
        vec2 cell = abs(fract(worldPosition) - 0.5);
        vec2 width = fwidth(worldPosition) * 0.8;
        float line = max(smoothstep(0.5 - width.x, 0.5, cell.x),
                         smoothstep(0.5 - width.y, 0.5, cell.y));
        outColor = vec4(color.rgb, color.a * line);
        return;
    }
    if (mode > 1.5 && mode < 2.5) {
        float edge = min(min(localPosition.x, 1.0 - localPosition.x),
                         min(localPosition.y, 1.0 - localPosition.y));
        outColor = edge < 0.09 ? vec4(1.0, 0.95, 0.3, 1.0) : color;
        return;
    }
    outColor = color;
}
