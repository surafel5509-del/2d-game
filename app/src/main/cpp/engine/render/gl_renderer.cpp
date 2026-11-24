// ============================================================
//  Nebula Engine - render/gl_renderer.cpp
//  GLES 3.0 backend. Android-only translation unit (the NDK build).
// ============================================================
#if defined(__ANDROID__)

#include "engine/render/gl_renderer.hpp"
#include "engine/core/log.hpp"
#include <GLES3/gl3.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

namespace nebula {

static const char* kSpriteVertex = R"(#version 300 es
uniform mat4 uVP;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
out vec2 vUV;
out vec4 vColor;
void main() {
    gl_Position = uVP * vec4(aPos, 1.0);
    vUV = aUV;
    vColor = aColor;
})";

static const char* kSpriteFragment = R"(#version 300 es
precision mediump float;
uniform sampler2D uTex;
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = texture(uTex, vUV) * vColor;
})";

static const char* kLineVertex = R"(#version 300 es
uniform mat4 uVP;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
out vec4 vColor;
void main() {
    gl_Position = uVP * vec4(aPos, 1.0);
    vColor = aColor;
})";

static const char* kLineFragment = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = vColor;
})";

GlRenderer::GlRenderer() { batch_.reserve(kMaxBatchVertices); }

GlRenderer::~GlRenderer() {
    destroyShaders();
    if (vbo_) glDeleteBuffers(1, &vbo_);
}

void GlRenderer::resize(int width, int height) {
    width_ = width;
    height_ = height;
    glViewport(0, 0, width, height);
    if (!spriteProgram_) initShaders();
    if (!vbo_) glGenBuffers(1, &vbo_);
}

void GlRenderer::initShaders() {
    const unsigned int vs = compileShader(GL_VERTEX_SHADER, kSpriteVertex);
    const unsigned int fs = compileShader(GL_FRAGMENT_SHADER, kSpriteFragment);
    spriteProgram_ = linkProgram(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);

    const unsigned int lvs = compileShader(GL_VERTEX_SHADER, kLineVertex);
    const unsigned int lfs = compileShader(GL_FRAGMENT_SHADER, kLineFragment);
    lineProgram_ = linkProgram(lvs, lfs);
    glDeleteShader(lvs); glDeleteShader(lfs);

    spriteVPLocation_ = glGetUniformLocation(spriteProgram_, "uVP");
    spriteTexLocation_ = glGetUniformLocation(spriteProgram_, "uTex");
    lineVPLocation_ = glGetUniformLocation(lineProgram_, "uVP");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    NEBULA_LOG_I("GL renderer initialized (sprites=%u lines=%u)", spriteProgram_, lineProgram_);
}

void GlRenderer::destroyShaders() {
    if (spriteProgram_) glDeleteProgram(spriteProgram_);
    if (lineProgram_) glDeleteProgram(lineProgram_);
    spriteProgram_ = lineProgram_ = 0;
}

unsigned int GlRenderer::compileShader(unsigned int type, const char* source) {
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        NEBULA_LOG_E("Shader compile failed: %s", log);
    }
    return shader;
}

int GlRenderer::linkProgram(unsigned int vs, unsigned int fs) {
    const unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        NEBULA_LOG_E("Program link failed: %s", log);
    }
    return static_cast<int>(program);
}

void GlRenderer::beginFrame(Color clearColor) {
    drawCalls_ = 0;
    batch_.clear();
    batchTexture_ = -1;
    glViewport(0, 0, width_, height_);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GlRenderer::endFrame() { flush(); }

int GlRenderer::createTexture(const uint8_t* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0) return 0;

    // Flip rows on the CPU so GL texture row 0 is the image bottom
    // (keeps v-sampling intuitive with y-up world coordinates).
    std::vector<uint8_t> flipped(static_cast<size_t>(width) * height * 4);
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    for (int y = 0; y < height; ++y) {
        std::memcpy(flipped.data() + static_cast<size_t>(y) * rowBytes,
                    rgba + static_cast<size_t>(height - 1 - y) * rowBytes,
                    rowBytes);
    }

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, flipped.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return static_cast<int>(texture);
}

void GlRenderer::destroyTexture(int textureId) {
    if (textureId > 0) {
        const unsigned int t = static_cast<unsigned int>(textureId);
        glDeleteTextures(1, &t);
    }
}

void GlRenderer::bindViewProjection(const Mat4& vp) {
    viewProjection_ = vp;
}

void GlRenderer::flushBatch() {
    if (batch_.empty() || batchTexture_ < 0) return;

    glUseProgram(spriteProgram_);
    glUniformMatrix4fv(spriteVPLocation_, 1, GL_FALSE, viewProjection_.m);
    glUniform1i(spriteTexLocation_, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(batchTexture_));

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, batch_.size() * sizeof(Vertex), batch_.data(), GL_STREAM_DRAW);

    const GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));
    const GLint posLoc = 0, uvLoc = 1, colorLoc = 2;
    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(uvLoc);
    glVertexAttribPointer(uvLoc, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(12));
    glEnableVertexAttribArray(colorLoc);
    glVertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(20));

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batch_.size()));
    ++drawCalls_;

    batch_.clear();
    batchTexture_ = -1;
}

void GlRenderer::flush() { flushBatch(); }

void GlRenderer::drawSprite(const SpriteDraw& draw, int textureWidth, int textureHeight) {
    if (draw.textureId <= 0 || textureWidth <= 0 || textureHeight <= 0) return;

    if (batchTexture_ != draw.textureId && !batch_.empty()) flushBatch();
    batchTexture_ = draw.textureId;

    // Source rect (pixels -> uv). Negative srcW/srcH => full texture.
    const float sx = draw.srcX, sy = draw.srcY;
    const float sw = draw.srcW > 0.0f ? draw.srcW : static_cast<float>(textureWidth);
    const float sh = draw.srcH > 0.0f ? draw.srcH : static_cast<float>(textureHeight);
    float u0 = sx / textureWidth;
    float u1 = (sx + sw) / textureWidth;
    float v0 = 1.0f - (sy + sh) / textureHeight;
    float v1 = 1.0f - sy / textureHeight;
    if (draw.flipX) std::swap(u0, u1);
    if (draw.flipY) std::swap(v0, v1);

    const float c = std::cos(draw.rotation), s = std::sin(draw.rotation);
    const float hw = draw.sizePx.x * 0.5f * draw.scale.x;
    const float hh = draw.sizePx.y * 0.5f * draw.scale.y;

    const Vec2 local[4] = { {-hw, -hh}, { hw, -hh}, { hw,  hh}, {-hw,  hh} };
    const float uvs[4][2] = { {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1} };
    Vec2 world[4];
    for (int i = 0; i < 4; ++i) {
        world[i] = draw.center + Vec2(local[i].x * c - local[i].y * s,
                                      local[i].x * s + local[i].y * c);
    }

    const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int i : idx) {
        batch_.push_back(Vertex{ world[i].x, world[i].y, 0.0f,
                                 uvs[i][0], uvs[i][1],
                                 draw.tint.r, draw.tint.g, draw.tint.b, draw.tint.a });
    }

    if (batch_.size() >= kMaxBatchVertices) flushBatch();
}

void GlRenderer::drawLines(const std::vector<Vec2>& points, Color color, bool loop) {
    if (points.size() < 2) return;
    flushBatch();  // keep the painter order correct

    glUseProgram(lineProgram_);
    glUniformMatrix4fv(lineVPLocation_, 1, GL_FALSE, viewProjection_.m);

    // Upload line segment pairs as (pos.xyz, color.rgba).
    std::vector<float> upload;
    upload.reserve((loop ? points.size() : points.size() - 1) * 2 * 7);
    auto push = [&](const Vec2& p) {
        upload.push_back(p.x); upload.push_back(p.y); upload.push_back(0.0f);
        upload.push_back(color.r); upload.push_back(color.g); upload.push_back(color.b); upload.push_back(color.a);
    };
    for (size_t i = 0; i + 1 < points.size(); ++i) { push(points[i]); push(points[i + 1]); }
    if (loop) { push(points.back()); push(points.front()); }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, upload.size() * sizeof(float), upload.data(), GL_STREAM_DRAW);
    const GLsizei stride = static_cast<GLsizei>(7 * sizeof(float));
    const GLint posLoc = 0, colorLoc = 1;
    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(colorLoc);
    glVertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(12));
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(upload.size() / 7));
    ++drawCalls_;
}

} // namespace nebula

#else  // !__ANDROID__

// Non-Android host build (unit tests): no-op GL backend so engine code
// links and logic runs without a GPU. Textures get fake incremental ids.
#include "engine/render/gl_renderer.hpp"
#include <atomic>

namespace nebula {

static std::atomic<int> sFakeTextureId{1};

GlRenderer::GlRenderer() : batch_() { width_ = 0; height_ = 0; drawCalls_ = 0; }
GlRenderer::~GlRenderer() = default;

void GlRenderer::resize(int width, int height) { width_ = width; height_ = height; }
void GlRenderer::beginFrame(Color) { drawCalls_ = 0; batch_.clear(); batchTexture_ = -1; }
void GlRenderer::endFrame() {}
int GlRenderer::createTexture(const uint8_t*, int, int) { return sFakeTextureId.fetch_add(1); }
void GlRenderer::destroyTexture(int) {}
void GlRenderer::bindViewProjection(const Mat4& vp) { viewProjection_ = vp; }
void GlRenderer::drawSprite(const SpriteDraw&, int, int) { ++drawCalls_; }
void GlRenderer::drawLines(const std::vector<Vec2>&, Color, bool) { ++drawCalls_; }
void GlRenderer::flush() {}

} // namespace nebula

#endif // __ANDROID__
