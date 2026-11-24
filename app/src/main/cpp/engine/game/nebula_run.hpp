#pragma once
// ============================================================
//  Nebula Engine - Nebula Run (flagship demo game, C++)
//  A portrait space-shooter built entirely on the engine's
//  public API: ECS entities, physics triggers, particles,
//  physics events, camera shake, lifetimes and textures.
//  Kotlin drives rendering/HUD; this class holds game state.
// ============================================================
#include "engine/nebula_engine.hpp"
#include "engine/math/random.hpp"
#include <vector>

namespace nebula {

class NebulaRunGame {
public:
    enum SoundBits : int {
        kLaser     = 1 << 0,
        kExplosion = 1 << 1,
        kHit       = 1 << 2,
        kGameOver  = 1 << 3,
    };

    enum class Mode : int { Demo = 0, Sandbox = 1 };

    void attach(NebulaEngine* engine);
    void startDemoScene();   // (re)builds the demo scene
    void startSandbox(const std::string& sceneJson);  // playtest an editor scene
    Mode mode() const { return mode_; }
    void update(float dt);

    int score() const { return score_; }
    int lives() const { return lives_; }
    bool isGameOver() const { return gameOver_; }
    int consumeSoundFlags() { const int f = soundFlags_; soundFlags_ = 0; return f; }

private:
    void buildScene();
    void spawnStars();
    void spawnEnemy();
    void fireBullet();
    void handleCollisions();
    void updateStars(float dt);
    void updateShake(float dt);

    NebulaEngine* engine_ = nullptr;
    Mode mode_ = Mode::Demo;
    Random rng_{0x5EED1234u};

    std::vector<Entity> stars_;
    Entity player_ = kNullEntity;
    float elapsed_ = 0.0f;
    float fireTimer_ = 0.0f;
    float enemyTimer_ = 0.0f;
    float starDriftBase_ = 30.0f;
    float shakeTimer_ = 0.0f;

    int score_ = 0;
    int lives_ = 3;
    int soundFlags_ = 0;
    bool gameOver_ = false;
};

} // namespace nebula
