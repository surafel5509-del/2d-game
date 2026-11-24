#pragma once
// ============================================================
//  Nebula Engine - render/renderer.hpp
//  Renderer abstraction. Nebula ships a GLES 3.0 implementation
//  (Android), with the seam left open for a Vulkan backend.
// ============================================================
#include "engine/math/vec2.hpp"
#include "engine/math/color.hpp"
#include "engine/math/mat4.hpp"
#include <cstdint>
#include <vector>

namespace nebula {

struct SpriteDraw {
    Vec2 center = Vec2::zero();
    float rotation = 0.0f;          // radians
    Vec2 scale = Vec2::one();
    Vec2 sizePx = Vec2(64.0f, 64.0f);
    Color tint = Color::white();
    int textureId = 0;
    // Source rect in pixels; srcW/srcH <= 0 => whole texture.
    float srcX = 0.0f, srcY = 0.0f, srcW = -1.0f, srcH = -1.0f;
    bool flipX = false;
    bool flipY = false;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void resize(int width, int height) = 0;
    virtual void beginFrame(Color clearColor) = 0;
    virtual void endFrame() = 0;

    virtual int createTexture(const uint8_t* rgba, int width, int height) = 0;
    virtual void destroyTexture(int textureId) = 0;

    virtual void bindViewProjection(const Mat4& vp) = 0;
    virtual void drawSprite(const SpriteDraw& draw, int textureWidth, int textureHeight) = 0;
    virtual void drawLines(const std::vector<Vec2>& points, Color color, bool loop) = 0;
    virtual void flush() = 0;

    virtual int drawCalls() const = 0;
    virtual int viewportWidth() const = 0;
    virtual int viewportHeight() const = 0;
};

} // namespace nebula
