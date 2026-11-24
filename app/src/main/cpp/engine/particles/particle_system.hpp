#pragma once
// ============================================================
//  Nebula Engine - particles/particle_system.hpp
//  Fixed-capacity CPU particle pool. Particles are simulated in
//  world space and drawn as tinted quads by the renderer.
//  Perfect for explosions, thrusters, sparks, rain, magic.
// ============================================================
#include "engine/math/vec2.hpp"
#include "engine/math/color.hpp"
#include "engine/math/random.hpp"
#include <vector>
#include <cstdint>

namespace nebula {

struct Particle {
    Vec2 position = Vec2::zero();
    Vec2 velocity = Vec2::zero();
    Vec2 acceleration = Vec2::zero();
    Color startColor = Color::white();
    Color endColor = Color::clear();
    float startSize = 24.0f;
    float endSize = 2.0f;
    float lifetime = 1.0f;
    float age = 0.0f;
    float drag = 0.0f;     // velocity damping per second (0..~4)
    float rotation = 0.0f;
    float angularVelocity = 0.0f;
    bool alive = false;
};

class ParticleSystem {
public:
    explicit ParticleSystem(size_t capacity = 512) { particles_.resize(capacity); }

    // Spawn a single particle; returns false when the pool is full.
    bool spawn(const Particle& p) {
        for (auto& slot : particles_) {
            if (!slot.alive) { slot = p; slot.alive = true; slot.age = 0.0f; return true; }
        }
        return false;
    }

    // Radial burst (explosions, impacts).
    void burst(Vec2 origin, int count, float speedMin, float speedMax,
               Color inner, Color outer, float sizeMin = 8.0f, float sizeMax = 26.0f,
               float lifeMin = 0.4f, float lifeMax = 1.1f, uint32_t seed = 0xC0FFEE) {
        Random rng(seed);
        for (int i = 0; i < count; ++i) {
            Particle p;
            const float angle = rng.range(0.0f, 6.2831853f);
            const float speed = rng.range(speedMin, speedMax);
            p.position = origin;
            p.velocity = Vec2(std::cos(angle), std::sin(angle)) * speed;
            p.acceleration = Vec2(0.0f, -140.0f);
            p.startColor = Color::lerp(inner, outer, rng.next01());
            p.endColor = Color(p.startColor.r, p.startColor.g, p.startColor.b, 0.0f);
            p.startSize = rng.range(sizeMin, sizeMax);
            p.endSize = 1.5f;
            p.lifetime = rng.range(lifeMin, lifeMax);
            p.drag = rng.range(1.0f, 2.6f);
            p.angularVelocity = rng.range(-4.0f, 4.0f);
            spawn(p);
        }
        alive_ += static_cast<size_t>(count);
    }

    // Directional spray (thrusters, muzzle sparks).
    void spray(Vec2 origin, Vec2 direction, int count, float spreadRadians,
               float speedMin, float speedMax, Color inner, float sizeMin = 4.0f,
               float sizeMax = 12.0f, float lifeMin = 0.25f, float lifeMax = 0.6f,
               uint32_t seed = 0xBEEF) {
        Random rng(seed);
        const float base = std::atan2(direction.y, direction.x);
        for (int i = 0; i < count; ++i) {
            Particle p;
            const float angle = base + rng.range(-spreadRadians, spreadRadians);
            const float speed = rng.range(speedMin, speedMax);
            p.position = origin;
            p.velocity = Vec2(std::cos(angle), std::sin(angle)) * speed;
            p.startColor = inner;
            p.endColor = Color(inner.r, inner.g, inner.b, 0.0f);
            p.startSize = rng.range(sizeMin, sizeMax);
            p.endSize = 1.0f;
            p.lifetime = rng.range(lifeMin, lifeMax);
            p.drag = rng.range(1.5f, 3.0f);
            spawn(p);
        }
    }

    void update(float dt) {
        for (auto& p : particles_) {
            if (!p.alive) continue;
            p.age += dt;
            if (p.age >= p.lifetime) { p.alive = false; if (alive_ > 0) --alive_; continue; }
            p.velocity += p.acceleration * dt;
            p.velocity *= std::max(0.0f, 1.0f - p.drag * dt);
            p.position += p.velocity * dt;
            p.rotation += p.angularVelocity * dt;
        }
    }

    template <typename Fn>
    void forEachAlive(Fn&& fn) const {
        for (const auto& p : particles_) {
            if (!p.alive) continue;
            const float t = p.lifetime > 0.0f ? p.age / p.lifetime : 1.0f;
            const float size = p.startSize + (p.endSize - p.startSize) * t;
            const Color color = Color::lerp(p.startColor, p.endColor, t);
            fn(p, size, color);
        }
    }

    size_t aliveCount() const { return alive_; }
    size_t capacity() const { return particles_.size(); }
    void clear() { for (auto& p : particles_) p.alive = false; alive_ = 0; }

private:
    std::vector<Particle> particles_;
    size_t alive_ = 0;
};

} // namespace nebula
