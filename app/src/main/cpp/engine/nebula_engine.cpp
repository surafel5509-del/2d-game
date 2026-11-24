// ============================================================
//  Nebula Engine - nebula_engine.cpp
//  Engine facade implementation. GL calls happen on the GL thread;
//  input/editor setters are synchronized inside InputState/Scene.
// ============================================================
#if !defined(NDEBUG)
#define NEBULA_DEBUG_BUILD 1
#endif

#include "engine/nebula_engine.hpp"
#include "engine/render/gl_renderer.hpp"
#include "engine/animation/sprite_animator.hpp"
#include "engine/core/log.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace nebula {

NebulaEngine::NebulaEngine() = default;
NebulaEngine::~NebulaEngine() = default;

// ---------------- Lifecycle ----------------

void NebulaEngine::initGL(int viewportW, int viewportH) {
    viewportW_ = std::max(1, viewportW);
    viewportH_ = std::max(1, viewportH);
    if (!renderer_) renderer_ = std::make_unique<GlRenderer>();
    renderer_->resize(viewportW_, viewportH_);

    // Engine-owned built-in textures: 1x1 white + soft radial dot.
    const uint8_t white[4] = {255, 255, 255, 255};
    whiteTextureId_ = renderer_->createTexture(white, 1, 1);
    textureIds_["__white"] = whiteTextureId_;
    textureSizes_[whiteTextureId_] = {1, 1};

    constexpr int dotSize = 64;
    std::vector<uint8_t> dot(static_cast<size_t>(dotSize) * dotSize * 4);
    for (int y = 0; y < dotSize; ++y) {
        for (int x = 0; x < dotSize; ++x) {
            const float dx = (x + 0.5f - dotSize * 0.5f) / (dotSize * 0.5f);
            const float dy = (y + 0.5f - dotSize * 0.5f) / (dotSize * 0.5f);
            const float d = std::sqrt(dx * dx + dy * dy);
            float alpha = std::clamp(1.0f - d, 0.0f, 1.0f);
            alpha = alpha * alpha * (3.0f - 2.0f * alpha);  // smoothstep
            const size_t i = (static_cast<size_t>(y) * dotSize + x) * 4;
            dot[i] = dot[i + 1] = dot[i + 2] = 255;
            dot[i + 3] = static_cast<uint8_t>(alpha * 255.0f);
        }
    }
    const int dotId = renderer_->createTexture(dot.data(), dotSize, dotSize);
    textureIds_["__dot"] = dotId;
    textureSizes_[dotId] = {dotSize, dotSize};

    glReady_ = true;
    NEBULA_LOG_I("NebulaEngine GL ready (%dx%d)", viewportW_, viewportH_);
}

void NebulaEngine::resizeGL(int viewportW, int viewportH) {
    viewportW_ = std::max(1, viewportW);
    viewportH_ = std::max(1, viewportH);
    if (renderer_) renderer_->resize(viewportW_, viewportH_);
}

void NebulaEngine::shutdownGL() {
    if (renderer_) {
        for (auto& [name, id] : textureIds_) {
            if (!name.empty() && name[0] == '_' && name[1] == '_') continue;  // builtins die with context
        }
        renderer_.reset();
    }
    textureIds_.clear();
    textureSizes_.clear();
    backgroundTextureId_ = 0;
    whiteTextureId_ = 0;
    glReady_ = false;
}

// ---------------- Textures ----------------

int NebulaEngine::registerTexture(const std::string& name, const uint8_t* rgba, int width, int height) {
    if (!glReady_ || !renderer_) {
        NEBULA_LOG_E("registerTexture('%s') before GL init", name.c_str());
        return 0;
    }
    auto it = textureIds_.find(name);
    if (it != textureIds_.end()) return it->second;  // idempotent

    const int id = renderer_->createTexture(rgba, width, height);
    if (id <= 0) return 0;
    textureIds_.emplace(name, id);
    textureSizes_[id] = {width, height};
    return id;
}

int NebulaEngine::textureIdFor(const std::string& name) const {
    auto it = textureIds_.find(name);
    return it != textureIds_.end() ? it->second : 0;
}

int NebulaEngine::textureCount() const {
    return static_cast<int>(textureIds_.size());
}

void NebulaEngine::setBackgroundTexture(const std::string& name) {
    backgroundTextureId_ = textureIdFor(name);
}

// ---------------- Frame ----------------

void NebulaEngine::tick(float dt) {
    dt = std::clamp(dt, 0.0f, 0.05f);
    if (dt > 0.0f) fpsSmoothed_ = fpsSmoothed_ * 0.95f + (1.0f / dt) * 0.05f;

    if (updateHook) updateHook(dt);

    // Physics substeps for stability at low framerates.
    const int substeps = 2;
    const float subDt = dt / substeps;
    for (int i = 0; i < substeps; ++i) physics_.step(scene_.registry(), subDt);

    SpriteAnimator::updateAll(scene_.registry(), dt);

    // Expire lifetime-bound entities.
    std::vector<Entity> expired;
    scene_.registry().each<LifetimeComponent>(
        [&](Entity e, LifetimeComponent& life) {
            life.secondsLeft -= dt;
            if (life.secondsLeft <= 0.0f) expired.push_back(e);
        });
    for (Entity e : expired) destroyEntity(e);

    particles_.update(dt);
}

void NebulaEngine::drawFrame() {
    if (!glReady_ || !renderer_) return;

    const Color clear = editorMode_ ? Color(0.055f, 0.065f, 0.09f, 1.0f)
                                    : Color(0.015f, 0.02f, 0.045f, 1.0f);
    renderer_->beginFrame(clear);

    const Camera2D& cam = scene_.camera();
    const Mat4 vp = cam.viewProjection(static_cast<float>(viewportW_), static_cast<float>(viewportH_));
    renderer_->bindViewProjection(vp);

    // Fullscreen background (letterboxed to the visible world rect).
    if (backgroundTextureId_ > 0 && !editorMode_) {
        const float halfW = (viewportW_ * 0.5f) / cam.zoom;
        const float halfH = (viewportH_ * 0.5f) / cam.zoom;
        SpriteDraw bg;
        bg.center = cam.position;
        bg.sizePx = Vec2(halfW * 2.0f, halfH * 2.0f);
        bg.textureId = backgroundTextureId_;
        bg.tint = Color::white();
        auto sizeIt = textureSizes_.find(backgroundTextureId_);
        const int tw = sizeIt != textureSizes_.end() ? sizeIt->second.first : 1024;
        const int th = sizeIt != textureSizes_.end() ? sizeIt->second.second : 1024;
        renderer_->drawSprite(bg, tw, th);
    }

    // Editor grid (world-space lines, adaptive spacing).
    if (editorMode_ && gridEnabled_) {
        const float halfW = (viewportW_ * 0.5f) / cam.zoom;
        const float halfH = (viewportH_ * 0.5f) / cam.zoom;
        const float left = cam.position.x - halfW, right = cam.position.x + halfW;
        const float bottom = cam.position.y - halfH, top = cam.position.y + halfH;

        float spacing = 64.0f;
        while (spacing * cam.zoom < 34.0f) spacing *= 2.0f;   // keep readable at min zoom

        std::vector<Vec2> lines;
        const float minorMin = -100000.0f, minorMax = 100000.0f;
        for (float x = std::floor(left / spacing) * spacing; x <= right; x += spacing) {
            if (x < minorMin || x > minorMax) continue;
            lines.push_back(Vec2(x, bottom)); lines.push_back(Vec2(x, top));
        }
        for (float y = std::floor(bottom / spacing) * spacing; y <= top; y += spacing) {
            lines.push_back(Vec2(left, y)); lines.push_back(Vec2(right, y));
        }
        renderer_->drawLines(lines, Color(1.0f, 1.0f, 1.0f, 0.055f), false);
        // Axes through origin.
        renderer_->drawLines({Vec2(-10000.0f, 0.0f), Vec2(10000.0f, 0.0f)}, Color(0.45f, 0.75f, 1.0f, 0.35f), false);
        renderer_->drawLines({Vec2(0.0f, -10000.0f), Vec2(0.0f, 10000.0f)}, Color(1.0f, 0.55f, 0.55f, 0.35f), false);
    }

    drawSprites();

    // Particles (soft additive-ish dots).
    const int dotId = textureIdFor("__dot");
    if (dotId > 0) {
        particles_.forEachAlive([&](const Particle& p, float size, const Color& color) {
            SpriteDraw quad;
            quad.center = p.position;
            quad.rotation = p.rotation;
            quad.sizePx = Vec2(size, size);
            quad.tint = color;
            quad.textureId = dotId;
            renderer_->drawSprite(quad, 64, 64);
        });
    }

    // Selection gizmo (editor).
    const Entity selected = scene_.selectedEntity();
    if (editorMode_ && selected != kNullEntity && scene_.registry().alive(selected)
        && scene_.registry().has<TransformComponent>(selected)) {
        const auto& t = scene_.registry().get<TransformComponent>(selected);
        Vec2 he(40.0f, 40.0f);
        if (scene_.registry().has<SpriteComponent>(selected)) {
            const auto& spr = scene_.registry().get<SpriteComponent>(selected);
            he = spr.sizePx * t.scale * 0.5f;
        }
        const float c = std::cos(t.rotation), s = std::sin(t.rotation);
        const Vec2 local[4] = { {-he.x, -he.y}, {he.x, -he.y}, {he.x, he.y}, {-he.x, he.y} };
        std::vector<Vec2> outline;
        for (const Vec2& p : local) {
            outline.push_back(t.position + Vec2(p.x * c - p.y * s, p.x * s + p.y * c));
        }
        renderer_->drawLines(outline, Color(1.0f, 0.78f, 0.25f, 0.95f), true);

        // Corner handles as filled white squares tinted amber.
        const float handle = 11.0f / cam.zoom;
        const int whiteId = whiteTextureId_;
        if (whiteId > 0) {
            for (const Vec2& p : outline) {
                SpriteDraw h;
                h.center = p;
                h.sizePx = Vec2(handle, handle);
                h.tint = Color(1.0f, 0.78f, 0.25f, 1.0f);
                h.textureId = whiteId;
                renderer_->drawSprite(h, 1, 1);
            }
        }
    }

    // Collider debug (editor/debug builds).
    if (debugDraw_) {
        scene_.registry().eachPair<RigidbodyComponent, TransformComponent>(
            [&](Entity, RigidbodyComponent&, TransformComponent& t) { (void)t; });
        scene_.registry().each<BoxColliderComponent>([&](Entity e, const BoxColliderComponent& box) {
            if (!scene_.registry().has<TransformComponent>(e)) return;
            const auto& t = scene_.registry().get<TransformComponent>(e);
            const float c = std::cos(t.rotation), s = std::sin(t.rotation);
            const Vec2 off(box.offset.x * c - box.offset.y * s, box.offset.x * s + box.offset.y * c);
            const Vec2 center = t.position + off;
            const Vec2 he = box.halfExtents * t.scale;
            const Vec2 local[4] = { {-he.x, -he.y}, {he.x, -he.y}, {he.x, he.y}, {-he.x, he.y} };
            std::vector<Vec2> outline;
            for (const Vec2& p : local) {
                outline.push_back(center + Vec2(p.x * c - p.y * s, p.x * s + p.y * c));
            }
            renderer_->drawLines(outline, Color(0.3f, 1.0f, 0.45f, 0.85f), true);
        });
        scene_.registry().each<CircleColliderComponent>([&](Entity e, const CircleColliderComponent& circle) {
            if (!scene_.registry().has<TransformComponent>(e)) return;
            const auto& t = scene_.registry().get<TransformComponent>(e);
            const Vec2 center = t.position + circle.offset;
            const float r = circle.radius * std::max(t.scale.x, t.scale.y);
            std::vector<Vec2> pts;
            constexpr int kSegs = 28;
            for (int i = 0; i < kSegs; ++i) {
                const float a = (6.2831853f * i) / kSegs;
                pts.push_back(center + Vec2(std::cos(a), std::sin(a)) * r);
            }
            renderer_->drawLines(pts, Color(0.3f, 0.95f, 1.0f, 0.85f), true);
        });
    }

    renderer_->endFrame();
    drawCallsLastFrame_ = renderer_->drawCalls();
}

void NebulaEngine::drawSprites() {
    Registry& reg = scene_.registry();

    // Gather + sort by zOrder (stable, painter's algorithm).
    std::vector<Entity> sprites;
    sprites.reserve(reg.componentCount<SpriteComponent>());
    reg.each<SpriteComponent>([&](Entity e, SpriteComponent&) { sprites.push_back(e); });
    std::sort(sprites.begin(), sprites.end(), [&](Entity a, Entity b) {
        const float za = reg.get<SpriteComponent>(a).zOrder;
        const float zb = reg.get<SpriteComponent>(b).zOrder;
        return za == zb ? a < b : za < zb;
    });

    // Camera culling bounds.
    const Camera2D& cam = scene_.camera();
    const float halfW = (viewportW_ * 0.5f) / cam.zoom;
    const float halfH = (viewportH_ * 0.5f) / cam.zoom;
    const AABB viewBounds = AABB::fromCenterHalfExtents(cam.position, Vec2(halfW + 64.0f, halfH + 64.0f));

    for (Entity e : sprites) {
        auto& spr = reg.get<SpriteComponent>(e);
        if (!reg.has<TransformComponent>(e)) continue;
        const auto& t = reg.get<TransformComponent>(e);

        if (spr.textureId == 0) {
            spr.textureId = textureIdFor(spr.texture);
        }
        if (spr.textureId <= 0) continue;

        const Vec2 he = spr.sizePx * t.scale * 0.5f;
        const AABB bounds = AABB::fromCenterHalfExtents(t.position, he);
        if (!viewBounds.overlaps(bounds)) continue;

        auto sizeIt = textureSizes_.find(spr.textureId);
        const int texW = sizeIt != textureSizes_.end() ? sizeIt->second.first : 1;
        const int texH = sizeIt != textureSizes_.end() ? sizeIt->second.second : 1;

        SpriteDraw draw;
        draw.center = t.position;
        draw.rotation = t.rotation;
        draw.scale = t.scale;
        draw.sizePx = spr.sizePx;
        draw.tint = spr.tint;
        draw.textureId = spr.textureId;
        draw.srcX = spr.srcX; draw.srcY = spr.srcY;
        draw.srcW = spr.srcW; draw.srcH = spr.srcH;
        draw.flipX = spr.flipX; draw.flipY = spr.flipY;
        renderer_->drawSprite(draw, texW, texH);
    }
}

// ---------------- Scene ----------------

void NebulaEngine::newScene(const std::string& name) {
    scene_.clear();
    scene_.setName(name.empty() ? "Untitled" : name);
    physics_.clear();
    particles_.clear();
}

std::string NebulaEngine::currentSceneName() const { return scene_.name(); }

std::string NebulaEngine::saveSceneJson() const { return scene_.serialize(); }

bool NebulaEngine::loadSceneJson(const std::string& text) {
    const bool ok = scene_.deserialize(text, [this](const std::string& name) { return textureIdFor(name); });
    if (ok) {
        physics_.clear();
        particles_.clear();
        NEBULA_LOG_I("Scene '%s' loaded: %zu entities", scene_.name().c_str(), scene_.registry().entityCount());
    }
    return ok;
}

// ---------------- Entities ----------------

Entity NebulaEngine::createSpriteEntity(const std::string& textureName, const std::string& name,
                                        Vec2 position, Vec2 sizePx, float zOrder) {
    const Entity e = scene_.registry().create();
    scene_.registry().add<NameComponent>(e, NameComponent{name.empty() ? "Sprite" : name});
    scene_.registry().add<TransformComponent>(e, TransformComponent{position, 0.0f, Vec2::one()});
    SpriteComponent spr;
    spr.texture = textureName;
    spr.textureId = textureIdFor(textureName);
    spr.sizePx = sizePx;
    spr.zOrder = zOrder;
    scene_.registry().add<SpriteComponent>(e, spr);
    return e;
}

void NebulaEngine::destroyEntity(Entity e) {
    if (scene_.registry().alive(e)) {
        if (scene_.selectedEntity() == e) scene_.setSelectedEntity(kNullEntity);
        scene_.registry().destroy(e);
    }
}

Entity NebulaEngine::duplicateEntity(Entity e) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e)) return kNullEntity;
    const Entity copy = reg.create();
    if (reg.has<NameComponent>(e)) reg.add<NameComponent>(copy, NameComponent{reg.get<NameComponent>(e).name + " Copy"});
    else reg.add<NameComponent>(copy, NameComponent{"Entity Copy"});
    if (reg.has<TagComponent>(e)) reg.add<TagComponent>(copy, reg.get<TagComponent>(e));
    if (reg.has<TransformComponent>(e)) {
        auto t = reg.get<TransformComponent>(e);
        t.position += Vec2(32.0f, 32.0f);
        reg.add<TransformComponent>(copy, t);
    } else {
        reg.add<TransformComponent>(copy, TransformComponent{});
    }
    if (reg.has<SpriteComponent>(e)) reg.add<SpriteComponent>(copy, reg.get<SpriteComponent>(e));
    if (reg.has<RigidbodyComponent>(e)) reg.add<RigidbodyComponent>(copy, reg.get<RigidbodyComponent>(e));
    if (reg.has<BoxColliderComponent>(e)) reg.add<BoxColliderComponent>(copy, reg.get<BoxColliderComponent>(e));
    if (reg.has<CircleColliderComponent>(e)) reg.add<CircleColliderComponent>(copy, reg.get<CircleColliderComponent>(e));
    scene_.setSelectedEntity(copy);
    return copy;
}

std::vector<Entity> NebulaEngine::entityIds() const {
    return scene_.registry().entities<NameComponent>();
}

bool NebulaEngine::getTransform(Entity e, Vec2& outPos, float& outRot, Vec2& outScale) const {
    const Registry& reg = scene_.registry();
    if (!reg.alive(e) || !reg.has<TransformComponent>(e)) return false;
    const auto& t = reg.get<TransformComponent>(e);
    outPos = t.position; outRot = t.rotation; outScale = t.scale;
    return true;
}

bool NebulaEngine::setTransform(Entity e, Vec2 pos, float rot, Vec2 scale) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e) || !reg.has<TransformComponent>(e)) return false;
    auto& t = reg.get<TransformComponent>(e);
    t.position = pos; t.rotation = rot; t.scale = scale;
    return true;
}

bool NebulaEngine::setPosition(Entity e, Vec2 pos) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e) || !reg.has<TransformComponent>(e)) return false;
    reg.get<TransformComponent>(e).position = pos;
    return true;
}

bool NebulaEngine::getSprite(Entity e, std::string& outTexture, Vec2& outSize, Color& outTint,
                             float& outZ, float& outRot) const {
    const Registry& reg = scene_.registry();
    if (!reg.alive(e) || !reg.has<SpriteComponent>(e) || !reg.has<TransformComponent>(e)) return false;
    const auto& spr = reg.get<SpriteComponent>(e);
    outTexture = spr.texture;
    outSize = spr.sizePx;
    outTint = spr.tint;
    outZ = spr.zOrder;
    outRot = reg.get<TransformComponent>(e).rotation;
    return true;
}

bool NebulaEngine::setSpriteTexture(Entity e, const std::string& textureName) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e) || !reg.has<SpriteComponent>(e)) return false;
    auto& spr = reg.get<SpriteComponent>(e);
    spr.texture = textureName;
    spr.textureId = textureIdFor(textureName);
    return true;
}

bool NebulaEngine::setSpriteSize(Entity e, Vec2 sizePx) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e) || !reg.has<SpriteComponent>(e)) return false;
    reg.get<SpriteComponent>(e).sizePx = sizePx;
    return true;
}

bool NebulaEngine::setSpriteTint(Entity e, Color tint) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e) || !reg.has<SpriteComponent>(e)) return false;
    reg.get<SpriteComponent>(e).tint = tint;
    return true;
}

bool NebulaEngine::setSpriteZOrder(Entity e, float zOrder) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e) || !reg.has<SpriteComponent>(e)) return false;
    reg.get<SpriteComponent>(e).zOrder = zOrder;
    return true;
}

bool NebulaEngine::addRigidbody(Entity e, bool isStatic, float gravityScale) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e)) return false;
    if (reg.has<RigidbodyComponent>(e)) {
        auto& rb = reg.get<RigidbodyComponent>(e);
        rb.isStatic = isStatic;
        rb.gravityScale = gravityScale;
        return true;
    }
    RigidbodyComponent rb;
    rb.isStatic = isStatic;
    rb.gravityScale = gravityScale;
    reg.add<RigidbodyComponent>(e, rb);
    return true;
}

bool NebulaEngine::addBoxCollider(Entity e, Vec2 halfExtents) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e)) return false;
    if (reg.has<BoxColliderComponent>(e)) { reg.get<BoxColliderComponent>(e).halfExtents = halfExtents; return true; }
    reg.add<BoxColliderComponent>(e, BoxColliderComponent{halfExtents, Vec2::zero(), false, -1.0f, -1.0f});
    return true;
}

bool NebulaEngine::addCircleCollider(Entity e, float radius) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e)) return false;
    if (reg.has<CircleColliderComponent>(e)) { reg.get<CircleColliderComponent>(e).radius = radius; return true; }
    reg.add<CircleColliderComponent>(e, CircleColliderComponent{radius, Vec2::zero(), false, -1.0f, -1.0f});
    return true;
}

bool NebulaEngine::removePhysics(Entity e) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e)) return false;
    if (reg.has<RigidbodyComponent>(e)) reg.remove<RigidbodyComponent>(e);
    if (reg.has<BoxColliderComponent>(e)) reg.remove<BoxColliderComponent>(e);
    if (reg.has<CircleColliderComponent>(e)) reg.remove<CircleColliderComponent>(e);
    return true;
}

std::string NebulaEngine::entityName(Entity e) const {
    const Registry& reg = scene_.registry();
    if (!reg.alive(e) || !reg.has<NameComponent>(e)) return {};
    return reg.get<NameComponent>(e).name;
}

bool NebulaEngine::setEntityName(Entity e, const std::string& name) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e)) return false;
    if (reg.has<NameComponent>(e)) reg.get<NameComponent>(e).name = name;
    else reg.add<NameComponent>(e, NameComponent{name});
    return true;
}

std::string NebulaEngine::entityTag(Entity e) const {
    const Registry& reg = scene_.registry();
    if (!reg.alive(e) || !reg.has<TagComponent>(e)) return {};
    return reg.get<TagComponent>(e).tag;
}

bool NebulaEngine::setEntityTag(Entity e, const std::string& tag) {
    Registry& reg = scene_.registry();
    if (!reg.alive(e)) return false;
    if (reg.has<TagComponent>(e)) reg.get<TagComponent>(e).tag = tag;
    else reg.add<TagComponent>(e, TagComponent{tag});
    return true;
}

// ---------------- Editor support ----------------

Entity NebulaEngine::pick(Vec2 worldPoint) const {
    const Registry& reg = scene_.registry();
    Entity best = kNullEntity;
    float bestZ = -1e30f;
    reg.each<SpriteComponent>([&](Entity e, const SpriteComponent& spr) {
        if (!reg.has<TransformComponent>(e)) return;
        const auto& t = reg.get<TransformComponent>(e);
        const Vec2 he = spr.sizePx * t.scale * 0.5f;
        const AABB bounds = AABB::fromCenterHalfExtents(t.position, he);
        if (bounds.contains(worldPoint) && spr.zOrder >= bestZ) {
            bestZ = spr.zOrder;
            best = e;
        }
    });
    return best;
}

Vec2 NebulaEngine::screenToWorld(Vec2 screenPx) const {
    return scene_.camera().screenToWorld(screenPx, static_cast<float>(viewportW_), static_cast<float>(viewportH_));
}

Vec2 NebulaEngine::worldToScreen(Vec2 world) const {
    return scene_.camera().worldToScreen(world, static_cast<float>(viewportW_), static_cast<float>(viewportH_));
}

EngineStats NebulaEngine::stats() const {
    EngineStats s;
    s.drawCalls = drawCallsLastFrame_;
    s.fps = fpsSmoothed_;
    s.entityCount = static_cast<int>(scene_.registry().entityCount());
    s.particleCount = static_cast<int>(particles_.aliveCount());
    s.textureCount = static_cast<int>(textureIds_.size());
    return s;
}

void NebulaEngine::resolveTextureIds() {
    scene_.registry().each<SpriteComponent>([&](Entity, SpriteComponent& spr) {
        if (spr.textureId == 0) spr.textureId = textureIdFor(spr.texture);
    });
}

} // namespace nebula
