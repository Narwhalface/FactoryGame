#include "engine/renderer/Camera2D.hpp"

namespace factory::engine {

void Camera2D::setViewport(float width, float height) noexcept {
    viewport_ = {std::max(width, 1.0f), std::max(height, 1.0f)};
}

Vec2 Camera2D::screenToWorld(Vec2 screen) const noexcept {
    return {position_.x + (screen.x - viewport_.x * 0.5f) / zoom_,
            position_.y + (screen.y - viewport_.y * 0.5f) / zoom_};
}

Vec2 Camera2D::worldToScreen(Vec2 world) const noexcept {
    return {(world.x - position_.x) * zoom_ + viewport_.x * 0.5f,
            (world.y - position_.y) * zoom_ + viewport_.y * 0.5f};
}

void Camera2D::zoomAt(float factor, Vec2 screenPoint) noexcept {
    const Vec2 before = screenToWorld(screenPoint);
    setZoom(zoom_ * factor);
    const Vec2 after = screenToWorld(screenPoint);
    position_.x += before.x - after.x;
    position_.y += before.y - after.y;
}

void Camera2D::reset(Vec2 position, float zoom) noexcept {
    position_ = position;
    setZoom(zoom);
}

WorldBounds Camera2D::visibleBounds(float padding) const noexcept {
    const float halfWidth = viewport_.x * 0.5f / zoom_ + padding;
    const float halfHeight = viewport_.y * 0.5f / zoom_ + padding;
    return {position_.x - halfWidth, position_.y - halfHeight,
            position_.x + halfWidth, position_.y + halfHeight};
}

} // namespace factory::engine
