#pragma once
// ============================================================
//  Nebula Engine - nebula_engine.hpp
//  The engine facade: owns the scene, renderer, physics, particles
//  and input, and exposes a flat, JNI-friendly API consumed by the
//  Android layer (launcher game, on-device scene editor).
// ============================================================
#include "engine/ecs/registry.hpp"
#include "engine/ecs/components.hpp"
#include "engine/scene/scene.hpp"
#include "engine/physics/physics_world.hpp"
#include "engine/particles/particle_system.hpp"
#include "engine/input/input_state.hpp"
#include "engine/render/camera2d.hpp"
#include "engine/math/vec2.hpp"
#include "engine/math/color.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace nebula {

class GlRenderer;  // platform GL implementation (Android GLES3)

struct EngineStats {
    int drawCalls = 0;
    float fps = 0.0f;
    int entityCount = 0;
    int particleCount = 0;
    int textureCount = 0;
};

class NebulaEngine {
public:
    NebulaEngine();
    ~NebulaEngine();

    // ---- Lifecycle (called on the GL thread) ----
    void initGL(int viewportW, int viewportH);
    void resizeGL(int viewportW, int viewportH);
    void shutdownGL();
    bool glReady() const { return glReady_; }

    // ---- Frame orchestration ----
    std::function<void(float dt)> updateHook;   // game logic hook (set by game)
    void tick(float dt);
    void drawFrame();

    // ---- Input plumbing (thread-safe) ----
    InputState& input() { return input_; }

    // ---- Textures ----
    int registerTexture(const std::string& name, const uint8_t* rgba, int width, int height);
    int textureIdFor(const std::string& name) const;
    int textureCount() const;
    void setBackgroundTexture(const std::string& name);

    // ---- Scene management ----
    void newScene(const std::string& name);
    std::string currentSceneName() const;
    std::string saveSceneJson() const;
    bool loadSceneJson(const std::string& text);   // resolves textures by name
    Scene& scene() { return scene_; }
    Registry& registry() { return scene_.registry(); }

    // ---- Entity helpers (editor + game) ----
    Entity createSpriteEntity(const std::string& textureName, const std::string& name,
                              Vec2 position, Vec2 sizePx, float zOrder = 0.0f);
    void destroyEntity(Entity e);
    Entity duplicateEntity(Entity e);
    std::vector<Entity> entityIds() const;

    // Transform
    bool getTransform(Entity e, Vec2& outPos, float& outRot, Vec2& outScale) const;
    bool setTransform(Entity e, Vec2 pos, float rot, Vec2 scale);
    bool setPosition(Entity e, Vec2 pos);

    // Sprite
    bool getSprite(Entity e, std::string& outTexture, Vec2& outSize, Color& outTint,
                   float& outZ, float& outRot) const;
    bool setSpriteTexture(Entity e, const std::string& textureName);
    bool setSpriteSize(Entity e, Vec2 sizePx);
    bool setSpriteTint(Entity e, Color tint);
    bool setSpriteZOrder(Entity e, float zOrder);

    // Physics
    bool addRigidbody(Entity e, bool isStatic, float gravityScale = 1.0f);
    bool addBoxCollider(Entity e, Vec2 halfExtents);
    bool addCircleCollider(Entity e, float radius);
    bool removePhysics(Entity e);

    // Tags & names
    std::string entityName(Entity e) const;
    bool setEntityName(Entity e, const std::string& name);
    std::string entityTag(Entity e) const;
    bool setEntityTag(Entity e, const std::string& tag);

    // ---- Editor support ----
    void setEditorMode(bool enabled) { editorMode_ = enabled; }
    bool editorMode() const { return editorMode_; }
    void setDebugDraw(bool enabled) { debugDraw_ = enabled; }
    bool debugDraw() const { return debugDraw_; }
    void setGridEnabled(bool enabled) { gridEnabled_ = enabled; }

    void selectEntity(Entity e) { scene_.setSelectedEntity(e); }
    Entity selectedEntity() const { return scene_.selectedEntity(); }

    Entity pick(Vec2 worldPoint) const;  // topmost sprite AABB under point

    void panCamera(Vec2 worldDelta) { scene_.camera().position += worldDelta; }
    void zoomCamera(float factor) { scene_.camera().zoomBy(factor); }
    void setCameraPosition(Vec2 p) { scene_.camera().position = p; }
    void setCameraZoom(float zoom) { scene_.camera().zoom = std::clamp(zoom, 0.15f, 6.0f); }
    Vec2 cameraPosition() const { return scene_.camera().position; }
    float cameraZoom() const { return scene_.camera().zoom; }

    Vec2 screenToWorld(Vec2 screenPx) const;
    Vec2 worldToScreen(Vec2 world) const;

    // ---- Particles ----
    ParticleSystem& particles() { return particles_; }

    // ---- Physics ----
    PhysicsWorld& physics() { return physics_; }

    // ---- Stats / debug ----
    EngineStats stats() const;

    int viewportWidth() const { return viewportW_; }
    int viewportHeight() const { return viewportH_; }

private:
    void drawSprites();
    void resolveTextureIds();

    Scene scene_;
    ParticleSystem particles_{1024};
    PhysicsWorld physics_;
    InputState input_;

    std::unique_ptr<GlRenderer> renderer_;
    std::unordered_map<std::string, int> textureIds_;   // name -> GL texture id
    std::unordered_map<int, std::pair<int,int>> textureSizes_; // id -> (w,h)

    int backgroundTextureId_ = 0;
    int whiteTextureId_ = 0;

    bool glReady_ = false;
    bool editorMode_ = false;
    bool debugDraw_ = false;
    bool gridEnabled_ = true;

    int viewportW_ = 1280;
    int viewportH_ = 720;

    float fpsSmoothed_ = 60.0f;
    int drawCallsLastFrame_ = 0;
};

} // namespace nebula
