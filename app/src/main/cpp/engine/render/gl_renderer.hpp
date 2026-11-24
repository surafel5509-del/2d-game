#pragma once
// ============================================================
//  Nebula Engine - render/gl_renderer.hpp
//  Android GLES 3.0 batch renderer:
//   * one dynamic VBO, sprites batched into a single draw call
//     per texture switch (typical mobile scenes: < 20 draw calls)
//   * straight-alpha blending, no depth buffer (2D painter's algorithm)
//   * dynamic line pass for editor grid / gizmos / collider debug
// ============================================================
#include "engine/render/renderer.hpp"
#include <unordered_map>
#include <memory>
#include <string>

namespace nebula {

class GlRenderer : public IRenderer {
public:
    GlRenderer();
    ~GlRenderer() override;

    void resize(int width, int height) override;
    void beginFrame(Color clearColor) override;
    void endFrame() override;

    int createTexture(const uint8_t* rgba, int width, int height) override;
    void destroyTexture(int textureId) override;

    void bindViewProjection(const Mat4& vp) override;
    void drawSprite(const SpriteDraw& draw, int textureWidth, int textureHeight) override;
    void drawLines(const std::vector<Vec2>& points, Color color, bool loop) override;
    void flush() override;

    int drawCalls() const override { return drawCalls_; }
    int viewportWidth() const override { return width_; }
    int viewportHeight() const override { return height_; }

private:
    struct Vertex { float x, y, z, u, v, r, g, b, a; };

    void initShaders();
    void destroyShaders();
    unsigned int compileShader(unsigned int type, const char* source);
    int linkProgram(unsigned int vs, unsigned int fs);
    void flushBatch();

    int width_ = 0;
    int height_ = 0;
    int drawCalls_ = 0;

    unsigned int spriteProgram_ = 0;
    unsigned int lineProgram_ = 0;
    unsigned int spriteVao_ = 0;   // (kept for future core-profile style binding)
    unsigned int vbo_ = 0;
    int spriteVPLocation_ = -1;
    int spriteTexLocation_ = -1;
    int lineVPLocation_ = -1;

    Mat4 viewProjection_{};
    std::vector<Vertex> batch_;
    int batchTexture_ = -1;   // GL id of the texture the batch is building for; -1 = empty
    static constexpr size_t kMaxBatchVertices = 16384;  // ~2730 quads
};

} // namespace nebula
