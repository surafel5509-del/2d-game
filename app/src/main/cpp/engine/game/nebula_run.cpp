// ============================================================
//  Nebula Engine - Nebula Run implementation
// ============================================================
#include "engine/game/nebula_run.hpp"
#include "engine/core/log.hpp"
#include <cmath>
#include <algorithm>

namespace nebula {

void NebulaRunGame::attach(NebulaEngine* engine) {
    engine_ = engine;
    if (engine_) {
        engine_->updateHook = [this](float dt) { this->update(dt); };
    }
}

void NebulaRunGame::startSandbox(const std::string& sceneJson) {
    if (!engine_) return;
    mode_ = Mode::Sandbox;
    gameOver_ = false;
    score_ = 0;
    lives_ = 0;           // sandbox: HUD lives hidden by the UI (mode == 1)
    soundFlags_ = 0;
    shakeTimer_ = 0.0f;
    player_ = kNullEntity;
    stars_.clear();
    engine_->loadSceneJson(sceneJson);
    NEBULA_LOG_I("Sandbox playtest started");
}

void NebulaRunGame::startDemoScene() {
    if (!engine_) return;
    mode_ = Mode::Demo;
    buildScene();
    score_ = 0;
    lives_ = 3;
    gameOver_ = false;
    soundFlags_ = 0;
    elapsed_ = 0.0f;
    fireTimer_ = 0.25f;
    enemyTimer_ = 0.8f;
    shakeTimer_ = 0.0f;
    NEBULA_LOG_I("Nebula Run demo scene started");
}

void NebulaRunGame::buildScene() {
    NebulaEngine& e = *engine_;
    e.newScene("Nebula Run");
    e.setBackgroundTexture("nebula_bg");

    // Fit a logical 760px-wide stage to the screen width.
    const float zoom = static_cast<float>(e.viewportWidth()) / 760.0f;
    e.setCameraZoom(zoom);
    e.setCameraPosition(Vec2(0.0f, 0.0f));

    spawnStars();

    const float halfH = (e.viewportHeight() * 0.5f) / e.cameraZoom();
    player_ = e.createSpriteEntity("ship", "Player", Vec2(0.0f, -halfH + 170.0f), Vec2(118.0f, 118.0f), 10.0f);
    e.setEntityTag(player_, "player");
    e.addRigidbody(player_, /*isStatic=*/true, 0.0f);
    e.addCircleCollider(player_, 34.0f);
    if (e.registry().has<CircleColliderComponent>(player_)) {
        e.registry().get<CircleColliderComponent>(player_).isTrigger = true;
    }
}

void NebulaRunGame::spawnStars() {
    NebulaEngine& e = *engine_;
    stars_.clear();
    const float halfH = (e.viewportHeight() * 0.5f) / e.cameraZoom();
    for (int i = 0; i < 80; ++i) {
        const float size = rng_.range(2.5f, 8.5f);
        const float x = rng_.range(-370.0f, 370.0f);
        const float y = rng_.range(-halfH, halfH);
        const Entity star = e.createSpriteEntity("star", "Star", Vec2(x, y), Vec2(size, size), -10.0f);
        const float brightness = rng_.range(0.35f, 1.0f);
        e.setSpriteTint(star, Color(brightness, brightness, brightness, brightness));
        stars_.push_back(star);
    }
}

void NebulaRunGame::updateStars(float dt) {
    NebulaEngine& e = *engine_;
    const float halfH = (e.viewportHeight() * 0.5f) / e.cameraZoom();
    for (size_t i = 0; i < stars_.size(); ++i) {
        Vec2 pos; float rot; Vec2 scl;
        if (!e.getTransform(stars_[i], pos, rot, scl)) continue;
        pos.y -= (starDriftBase_ + static_cast<float>(i % 7) * 14.0f) * dt;
        if (pos.y < -halfH - 10.0f) {
            pos.y = halfH + 10.0f;
            pos.x = rng_.range(-370.0f, 370.0f);
        }
        e.setPosition(stars_[i], pos);
    }
}

void NebulaRunGame::spawnEnemy() {
    NebulaEngine& e = *engine_;
    const float halfH = (e.viewportHeight() * 0.5f) / e.cameraZoom();
    const float x = rng_.range(-320.0f, 320.0f);
    const Entity enemy = e.createSpriteEntity("enemy", "Enemy", Vec2(x, halfH + 70.0f), Vec2(96.0f, 96.0f), 6.0f);
    e.setEntityTag(enemy, "enemy");
    e.addRigidbody(enemy, /*isStatic=*/false, 0.0f);
    e.addBoxCollider(enemy, Vec2(34.0f, 34.0f));
    if (e.registry().has<BoxColliderComponent>(enemy)) {
        e.registry().get<BoxColliderComponent>(enemy).isTrigger = true;
    }
    if (e.registry().has<RigidbodyComponent>(enemy)) {
        auto& rb = e.registry().get<RigidbodyComponent>(enemy);
        rb.velocity = Vec2(rng_.range(-55.0f, 55.0f), -rng_.range(150.0f, 235.0f + elapsed_ * 4.0f));
    }
    e.registry().add<LifetimeComponent>(enemy, LifetimeComponent{14.0f});
}

void NebulaRunGame::fireBullet() {
    NebulaEngine& e = *engine_;
    Vec2 pos; float rot; Vec2 scl;
    if (!e.getTransform(player_, pos, rot, scl)) return;

    const Vec2 muzzle = pos + Vec2(0.0f, 62.0f);
    const Entity bullet = e.createSpriteEntity("bullet", "Bullet", muzzle, Vec2(16.0f, 44.0f), 5.0f);
    e.setEntityTag(bullet, "bullet");
    e.addRigidbody(bullet, /*isStatic=*/false, 0.0f);
    e.addBoxCollider(bullet, Vec2(7.0f, 19.0f));
    if (e.registry().has<BoxColliderComponent>(bullet)) {
        e.registry().get<BoxColliderComponent>(bullet).isTrigger = true;
    }
    if (e.registry().has<RigidbodyComponent>(bullet)) {
        e.registry().get<RigidbodyComponent>(bullet).velocity = Vec2(0.0f, 1250.0f);
    }
    e.registry().add<LifetimeComponent>(bullet, LifetimeComponent{1.6f});

    soundFlags_ |= kLaser;
    e.particles().spray(muzzle, Vec2(0.0f, 1.0f), 3, 0.35f, 60.0f, 140.0f,
                        Color(1.0f, 0.85f, 0.35f, 0.9f), 3.0f, 7.0f, 0.15f, 0.3f,
                        static_cast<uint32_t>(elapsed_ * 1000.0f) + 7u);
}

void NebulaRunGame::handleCollisions() {
    NebulaEngine& e = *engine_;
    const auto& entered = e.physics().entered();
    if (entered.empty()) return;

    const float halfH = (e.viewportHeight() * 0.5f) / e.cameraZoom();
    (void)halfH;

    for (const auto& ev : entered) {
        const std::string tagA = e.entityTag(ev.a);
        const std::string tagB = e.entityTag(ev.b);

        auto is = [&](const std::string& t, const char* want) { return t == want; };

        // bullet <-> enemy
        const bool bulletVsEnemy = (is(tagA, "bullet") && is(tagB, "enemy")) ||
                                   (is(tagB, "bullet") && is(tagA, "enemy"));
        if (bulletVsEnemy) {
            const Entity bullet = is(tagA, "bullet") ? ev.a : ev.b;
            const Entity enemy  = is(tagA, "enemy") ? ev.a : ev.b;

            Vec2 enemyPos; float rot; Vec2 scl;
            if (e.getTransform(enemy, enemyPos, rot, scl)) {
                e.particles().burst(enemyPos, 30, 140.0f, 460.0f,
                                    Color(1.0f, 0.88f, 0.35f, 1.0f),
                                    Color(1.0f, 0.32f, 0.12f, 1.0f),
                                    6.0f, 22.0f, 0.35f, 1.0f,
                                    static_cast<uint32_t>(enemyPos.x * 131.0f) ^ 0xABCDu);
            }
            e.destroyEntity(bullet);
            e.destroyEntity(enemy);
            score_ += 100;
            soundFlags_ |= kExplosion;
            continue;
        }

        // enemy <-> player
        const bool enemyVsPlayer = (is(tagA, "enemy") && is(tagB, "player")) ||
                                   (is(tagB, "enemy") && is(tagA, "player"));
        if (enemyVsPlayer) {
            const Entity enemy = is(tagA, "enemy") ? ev.a : ev.b;
            Vec2 enemyPos; float rot; Vec2 scl;
            if (e.getTransform(enemy, enemyPos, rot, scl)) {
                e.particles().burst(enemyPos, 42, 180.0f, 560.0f,
                                    Color(0.6f, 0.9f, 1.0f, 1.0f),
                                    Color(1.0f, 0.45f, 0.15f, 1.0f),
                                    8.0f, 26.0f, 0.4f, 1.15f,
                                    static_cast<uint32_t>(enemyPos.y * 97.0f) ^ 0x77u);
            }
            e.destroyEntity(enemy);
            lives_ = std::max(0, lives_ - 1);
            shakeTimer_ = 0.55f;
            soundFlags_ |= kHit;
            if (lives_ == 0) {
                gameOver_ = true;
                soundFlags_ |= kGameOver;
                Vec2 pPos; float pRot; Vec2 pScl;
                if (e.getTransform(player_, pPos, pRot, pScl)) {
                    e.particles().burst(pPos, 70, 200.0f, 640.0f,
                                        Color(1.0f, 0.95f, 0.55f, 1.0f),
                                        Color(1.0f, 0.25f, 0.1f, 1.0f),
                                        10.0f, 30.0f, 0.6f, 1.4f, 0xDEADu);
                }
                e.destroyEntity(player_);
                player_ = kNullEntity;
            }
            continue;
        }
    }
}

void NebulaRunGame::updateShake(float dt) {
    NebulaEngine& e = *engine_;
    if (shakeTimer_ > 0.0f) {
        shakeTimer_ -= dt;
        const float mag = shakeTimer_ * 16.0f;
        e.setCameraPosition(Vec2(rng_.range(-mag, mag), rng_.range(-mag, mag)));
        if (shakeTimer_ <= 0.0f) e.setCameraPosition(Vec2(0.0f, 0.0f));
    }
    // No shake active: leave the camera exactly where the user framed it
    // (essential for sandbox playtests of editor scenes).
}

void NebulaRunGame::update(float dt) {
    if (!engine_) return;
    NebulaEngine& e = *engine_;
    elapsed_ += dt;

    if (mode_ == Mode::Sandbox) {
        updateShake(dt);
        return;  // engine.tick() already steps physics + particles
    }

    updateStars(dt);
    updateShake(dt);

    const float zoom = e.cameraZoom();
    const float halfH = (e.viewportHeight() * 0.5f) / zoom;
    const float halfW = (e.viewportWidth() * 0.5f) / zoom;

    if (!gameOver_) {
        // --- Player follows the finger (x-axis), clamped to the stage. ---
        if (player_ != kNullEntity && e.input().isTouching()) {
            const Vec2 touchWorld = e.screenToWorld(e.input().primaryPointerPosition());
            Vec2 pos; float rot; Vec2 scl;
            if (e.getTransform(player_, pos, rot, scl)) {
                const float targetX = std::clamp(touchWorld.x, -halfW + 60.0f, halfW - 60.0f);
                pos.x += (targetX - pos.x) * std::min(1.0f, dt * 14.0f);
                e.setPosition(player_, pos);
                // Gentle bank while steering.
                const float bank = std::clamp((targetX - pos.x) * 0.004f, -0.35f, 0.35f);
                if (e.registry().has<TransformComponent>(player_)) {
                    e.registry().get<TransformComponent>(player_).rotation = -bank;
                }
            }
        }

        // --- Auto fire. ---
        fireTimer_ -= dt;
        if (fireTimer_ <= 0.0f && player_ != kNullEntity) {
            fireBullet();
            fireTimer_ = 0.19f;
        }

        // --- Enemy spawner (ramps up). ---
        enemyTimer_ -= dt;
        const float interval = std::max(0.38f, 0.95f - elapsed_ * 0.011f);
        if (enemyTimer_ <= 0.0f) {
            spawnEnemy();
            enemyTimer_ = interval;
        }
    }

    // --- Clean up enemies that escaped past the bottom. ---
    std::vector<Entity> escaped;
    e.registry().each<TagComponent>([&](Entity ent, const TagComponent& tag) {
        if (tag.tag != "enemy") return;
        Vec2 pos; float rot; Vec2 scl;
        if (e.getTransform(ent, pos, rot, scl) && pos.y < -halfH - 90.0f) escaped.push_back(ent);
    });
    for (Entity ent : escaped) {
        e.destroyEntity(ent);
        if (!gameOver_) {
            lives_ = std::max(0, lives_ - 1);
            soundFlags_ |= kHit;
            if (lives_ == 0) { gameOver_ = true; soundFlags_ |= kGameOver; }
        }
    }

    handleCollisions();
}

} // namespace nebula
