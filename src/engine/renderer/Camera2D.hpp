#pragma once

#include <algorithm>

namespace factory::engine {

struct Vec2 {
    float x{};
    float y{};
};

struct WorldBounds {
    float left{};
    float top{};
    float right{};
    float bottom{};
};

class Camera2D {
public:
    void setViewport(float width, float height) noexcept;
    void setPosition(Vec2 position) noexcept { position_ = position; }
    void pan(Vec2 delta) noexcept { position_.x += delta.x; position_.y += delta.y; }
    void setZoom(float zoom) noexcept { zoom_ = std::clamp(zoom, minZoom_, maxZoom_); }
    void zoomAt(float factor, Vec2 screenPoint) noexcept;
    void reset(Vec2 position = {50.0f, 35.0f}, float zoom = 20.0f) noexcept;

    [[nodiscard]] Vec2 position() const noexcept { return position_; }
    [[nodiscard]] float zoom() const noexcept { return zoom_; }
    [[nodiscard]] Vec2 viewport() const noexcept { return viewport_; }
    [[nodiscard]] Vec2 screenToWorld(Vec2 screen) const noexcept;
    [[nodiscard]] Vec2 worldToScreen(Vec2 world) const noexcept;
    [[nodiscard]] WorldBounds visibleBounds(float padding = 0.0f) const noexcept;

private:
    Vec2 position_{50.0f, 35.0f};
    Vec2 viewport_{1280.0f, 720.0f};
    float zoom_{20.0f};
    float minZoom_{2.0f};
    float maxZoom_{96.0f};
};

} // namespace factory::engine
