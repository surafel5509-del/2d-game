#pragma once
// ============================================================
//  Nebula Engine - render/camera2d.hpp
//  Orthographic 2D camera: position, zoom, and the screen<->world
//  mapping used by the renderer and by editor picking.
//  World space is y-up, origin at world center by default.
// ============================================================
#include "engine/math/vec2.hpp"
#include "engine/math/mat4.hpp"
#include <cmath>
#include <algorithm>

namespace nebula {

struct Camera2D {
    Vec2 position = Vec2::zero();
    float zoom = 1.0f;   // 1 = 1 world unit per pixel
    float minZoom = 0.15f;
    float maxZoom = 6.0f;

    void zoomBy(float factor) {
        zoom = std::clamp(zoom * factor, minZoom, maxZoom);
    }

    // Ortho projection * view translate, as uVP for the vertex shader.
    Mat4 viewProjection(float viewportW, float viewportH) const {
        const float halfW = (viewportW * 0.5f) / zoom;
        const float halfH = (viewportH * 0.5f) / zoom;
        Mat4 projection = Mat4::ortho(-halfW, halfW, -halfH, halfH, -10.0f, 10.0f);
        Mat4 view = Mat4::translation(-position.x, -position.y, 0.0f);
        return projection * view;
    }

    Vec2 screenToWorld(Vec2 screenPx, float viewportW, float viewportH) const {
        const float wx = (screenPx.x - viewportW * 0.5f) / zoom + position.x;
        const float wy = (viewportH * 0.5f - screenPx.y) / zoom + position.y;
        return Vec2(wx, wy);
    }

    Vec2 worldToScreen(Vec2 world, float viewportW, float viewportH) const {
        const float sx = (world.x - position.x) * zoom + viewportW * 0.5f;
        const float sy = viewportH * 0.5f - (world.y - position.y) * zoom;
        return Vec2(sx, sy);
    }
};

} // namespace nebula
