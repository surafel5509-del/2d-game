#pragma once
// ============================================================
//  Nebula Engine - physics/physics_world.hpp
//  Deterministic 2D physics: semi-implicit Euler integration,
//  spatial-hash broadphase, impulse solver with positional
//  correction, triggers, and raycasts (AABB + circle).
//
//  Colliders are treated as axis-aligned bounds in world space
//  (rotated boxes use conservative rotated-extent AABBs), which
//  keeps Nebula 2D rock solid at high body counts on mobile.
// ============================================================
#include "engine/ecs/registry.hpp"
#include "engine/ecs/components.hpp"
#include "engine/math/vec2.hpp"
#include "engine/math/rect.hpp"
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

namespace nebula {

struct CollisionEvent {
    Entity a = kNullEntity;
    Entity b = kNullEntity;
    bool isTrigger = false;
};

struct RaycastHit {
    Entity entity = kNullEntity;
    Vec2 point = Vec2::zero();
    Vec2 normal = Vec2::zero();
    float distance = 0.0f;
};

struct ContactManifold {
    Entity a = kNullEntity;
    Entity b = kNullEntity;
    Vec2 normal = Vec2::zero();   // from b toward a
    Vec2 point = Vec2::zero();
    float penetration = 0.0f;
    bool isTrigger = false;
};

class PhysicsWorld {
public:
    Vec2 gravity = Vec2(0.0f, -1100.0f);  // px/s^2
    float cellSize = 128.0f;              // broadphase grid cell
    float positionalCorrectionPercent = 0.8f;
    float positionalSlop = 0.5f;          // px

    // Begin/end events are recomputed every step by diffing contact pairs.
    const std::vector<CollisionEvent>& entered() const { return entered_; }
    const std::vector<CollisionEvent>& exited()  const { return exited_; }
    const std::vector<ContactManifold>& contacts() const { return contacts_; }

    void step(Registry& registry, float dt);

    // Closest hit along the ray, ignoring `ignore` (e.g. the shooter).
    bool raycast(const Registry& registry, Vec2 origin, Vec2 direction,
                 float maxDistance, RaycastHit& outHit,
                 Entity ignore = kNullEntity) const;

    void clear() { contacts_.clear(); entered_.clear(); exited_.clear(); prevPairs_.clear(); }

    // Conservative world-space AABB of an entity's collider. False if none.
    static bool worldAABB(const Registry& registry, Entity e, AABB& out);

private:
    struct BodyRef {
        Entity entity;
        Vec2 position;
        Vec2 halfExtents;   // box: rotated conservative half extents
        float radius = 0.0f;
        bool isCircle = false;
        bool isStatic = false;
        bool isTrigger = false;
        float invMass = 0.0f;
        float restitution = 0.0f;
        float friction = 0.0f;
        AABB worldBounds;
    };

    static uint64_t pairKey(Entity a, Entity b) {
        const uint32_t lo = std::min(a, b), hi = std::max(a, b);
        return (static_cast<uint64_t>(lo) << 32) | hi;
    }

    void gatherBodies(Registry& registry, std::vector<BodyRef>& bodies) const;
    std::vector<std::pair<size_t, size_t>> broadphase(const std::vector<BodyRef>& bodies) const;
    static bool narrowphase(const BodyRef& a, const BodyRef& b, ContactManifold& out);

    std::vector<ContactManifold> contacts_;
    std::vector<CollisionEvent> entered_;
    std::vector<CollisionEvent> exited_;
    std::unordered_set<uint64_t> prevPairs_;
};

} // namespace nebula
