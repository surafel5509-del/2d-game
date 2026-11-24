// ============================================================
//  Nebula Engine - physics/physics_world.cpp
// ============================================================
#include "engine/physics/physics_world.hpp"
#include "engine/math/mat4.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace nebula {

namespace {
inline int64_t cellKey(int x, int y) {
    return (static_cast<int64_t>(static_cast<uint32_t>(x)) << 32) |
           static_cast<uint32_t>(y);
}
} // namespace

bool PhysicsWorld::worldAABB(const Registry& registry, Entity e, AABB& out) {
    if (!registry.alive(e)) return false;
    const auto* t = registry.has<TransformComponent>(e) ? &registry.get<TransformComponent>(e) : nullptr;
    if (!t) return false;

    if (registry.has<BoxColliderComponent>(e)) {
        const auto& box = registry.get<BoxColliderComponent>(e);
        const float c = std::cos(t->rotation), s = std::sin(t->rotation);
        const Vec2 off(box.offset.x * c - box.offset.y * s, box.offset.x * s + box.offset.y * c);
        const Vec2 center = t->position + off;
        const Vec2 he = box.halfExtents * t->scale;  // scale-aware
        const float ex = std::fabs(he.x * c) + std::fabs(he.y * s);
        const float ey = std::fabs(he.x * s) + std::fabs(he.y * c);
        out = AABB::fromCenterHalfExtents(center, Vec2(ex, ey));
        return true;
    }
    if (registry.has<CircleColliderComponent>(e)) {
        const auto& circle = registry.get<CircleColliderComponent>(e);
        const float c = std::cos(t->rotation), s = std::sin(t->rotation);
        const Vec2 off(circle.offset.x * c - circle.offset.y * s, circle.offset.x * s + circle.offset.y * c);
        const float r = circle.radius * std::max(t->scale.x, t->scale.y);
        out = AABB::fromCenterHalfExtents(t->position + off, Vec2(r, r));
        return true;
    }
    // Sprite-only fallback: use sprite size so picking/physics still function.
    if (registry.has<SpriteComponent>(e)) {
        const auto& spr = registry.get<SpriteComponent>(e);
        const Vec2 he = spr.sizePx * t->scale * 0.5f;
        out = AABB::fromCenterHalfExtents(t->position, he);
        return true;
    }
    return false;
}

void PhysicsWorld::gatherBodies(Registry& registry, std::vector<BodyRef>& bodies) const {
    bodies.clear();
    registry.eachPair<RigidbodyComponent, TransformComponent>(
        [&](Entity e, RigidbodyComponent& rb, TransformComponent& t) {
            BodyRef body;
            body.entity = e;
            body.position = t.position;
            body.isStatic = rb.isStatic;

            float area = 0.0f;
            if (registry.has<BoxColliderComponent>(e)) {
                const auto& box = registry.get<BoxColliderComponent>(e);
                const float c = std::cos(t.rotation), s = std::sin(t.rotation);
                const Vec2 off(box.offset.x * c - box.offset.y * s, box.offset.x * s + box.offset.y * c);
                const Vec2 he = box.halfExtents * t.scale;
                body.position += off;
                body.isCircle = false;
                body.halfExtents = Vec2(std::fabs(he.x * c) + std::fabs(he.y * s),
                                        std::fabs(he.x * s) + std::fabs(he.y * c));
                body.isTrigger = box.isTrigger;
                body.restitution = box.restitution >= 0.0f ? box.restitution : rb.restitution;
                body.friction = box.friction >= 0.0f ? box.friction : rb.friction;
                area = (2.0f * he.x) * (2.0f * he.y);
            } else if (registry.has<CircleColliderComponent>(e)) {
                const auto& circle = registry.get<CircleColliderComponent>(e);
                const float c = std::cos(t.rotation), s = std::sin(t.rotation);
                const Vec2 off(circle.offset.x * c - circle.offset.y * s, circle.offset.x * s + circle.offset.y * c);
                body.position += off;
                body.isCircle = true;
                body.radius = circle.radius * std::max(t.scale.x, t.scale.y);
                body.isTrigger = circle.isTrigger;
                body.restitution = circle.restitution >= 0.0f ? circle.restitution : rb.restitution;
                body.friction = circle.friction >= 0.0f ? circle.friction : rb.friction;
                area = 3.14159265f * body.radius * body.radius;
            } else {
                return;  // no shape
            }

            float mass = rb.mass;
            if (mass <= 0.0f) mass = std::max(0.01f, rb.density * area);
            body.invMass = body.isStatic ? 0.0f : 1.0f / mass;
            body.worldBounds = AABB::fromCenterHalfExtents(
                body.position,
                body.isCircle ? Vec2(body.radius, body.radius) : body.halfExtents);
            bodies.push_back(body);
        });
}

std::vector<std::pair<size_t, size_t>> PhysicsWorld::broadphase(const std::vector<BodyRef>& bodies) const {
    std::unordered_map<int64_t, std::vector<size_t>> grid;
    grid.reserve(bodies.size() * 2);
    for (size_t i = 0; i < bodies.size(); ++i) {
        const AABB& b = bodies[i].worldBounds;
        const int x0 = static_cast<int>(std::floor(b.min.x / cellSize));
        const int x1 = static_cast<int>(std::floor(b.max.x / cellSize));
        const int y0 = static_cast<int>(std::floor(b.min.y / cellSize));
        const int y1 = static_cast<int>(std::floor(b.max.y / cellSize));
        for (int cy = y0; cy <= y1; ++cy)
            for (int cx = x0; cx <= x1; ++cx)
                grid[cellKey(cx, cy)].push_back(i);
    }

    std::vector<std::pair<size_t, size_t>> pairs;
    std::unordered_set<uint64_t> seen;
    for (const auto& [key, bucket] : grid) {
        (void)key;
        for (size_t i = 0; i < bucket.size(); ++i) {
            for (size_t j = i + 1; j < bucket.size(); ++j) {
                const size_t a = bucket[i], b = bucket[j];
                if (a == b) continue;
                const uint64_t pk = (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
                if (!seen.insert(pk).second) continue;
                const BodyRef& ba = bodies[a];
                const BodyRef& bb = bodies[b];
                if (ba.isStatic && bb.isStatic) continue;
                if (!ba.worldBounds.overlaps(bb.worldBounds)) continue;
                pairs.emplace_back(std::min(a, b), std::max(a, b));
            }
        }
    }
    return pairs;
}

bool PhysicsWorld::narrowphase(const BodyRef& a, const BodyRef& b, ContactManifold& out) {
    out.a = a.entity;
    out.b = b.entity;
    out.isTrigger = a.isTrigger || b.isTrigger;

    if (!a.isCircle && !b.isCircle) {
        // AABB vs AABB — minimum translation axis.
        const Vec2 da = b.position - a.position;
        const float overlapX = (a.halfExtents.x + b.halfExtents.x) - std::fabs(da.x);
        const float overlapY = (a.halfExtents.y + b.halfExtents.y) - std::fabs(da.y);
        if (overlapX <= 0.0f || overlapY <= 0.0f) return false;
        if (overlapX < overlapY) {
            out.normal = Vec2(da.x >= 0.0f ? -1.0f : 1.0f, 0.0f);  // from b toward a
            out.penetration = overlapX;
        } else {
            out.normal = Vec2(0.0f, da.y >= 0.0f ? -1.0f : 1.0f);
            out.penetration = overlapY;
        }
        out.point = (a.position + b.position) * 0.5f;
        return true;
    }

    // Circle involved: make `c` the circle, `o` the other.
    const BodyRef& c = a.isCircle ? a : b;
    const BodyRef& o = a.isCircle ? b : a;
    Vec2 n = Vec2::zero();
    float penetration = 0.0f;
    Vec2 point;

    if (o.isCircle) {
        const Vec2 d = o.position - c.position;
        const float dist = d.length();
        const float minDist = c.radius + o.radius;
        if (dist >= minDist) return false;
        const Vec2 dir = dist > 1e-6f ? d / dist : Vec2(1.0f, 0.0f);
        n = a.isCircle ? dir * -1.0f : dir;   // ensure normal points from b toward a
        penetration = minDist - dist;
        point = c.position + dir * (c.radius * 0.5f);
    } else {
        // Circle vs AABB: closest point on the box to the circle center.
        const Vec2 d = c.position - o.position;
        const Vec2 closest(std::clamp(d.x, -o.halfExtents.x, o.halfExtents.x),
                           std::clamp(d.y, -o.halfExtents.y, o.halfExtents.y));
        const Vec2 worldClosest = o.position + closest;
        Vec2 fromClosest = c.position - worldClosest;
        float distSq = fromClosest.lengthSq();
        if (distSq > c.radius * c.radius) return false;
        float dist = std::sqrt(std::max(distSq, 1e-12f));
        Vec2 dirFromBox = dist > 1e-6f ? fromClosest / dist : Vec2(0.0f, 1.0f);
        // n must point from b toward a.
        n = a.isCircle ? dirFromBox : dirFromBox * -1.0f;
        penetration = c.radius - dist;
        point = worldClosest;
    }

    out.normal = n;
    out.penetration = penetration;
    out.point = point;
    return true;
}

void PhysicsWorld::step(Registry& registry, float dt) {
    contacts_.clear();
    entered_.clear();
    exited_.clear();
    if (dt <= 0.0f) return;

    // ---- 1) Integrate dynamic bodies (semi-implicit Euler). ----
    {
        const Vec2 g = gravity;
        registry.eachPair<RigidbodyComponent, TransformComponent>(
            [&](Entity, RigidbodyComponent& rb, TransformComponent& t) {
                if (rb.isStatic) return;
                rb.velocity.y += g.y * rb.gravityScale * dt;
                rb.velocity.x += g.x * rb.gravityScale * dt;
                t.position += rb.velocity * dt;
            });
    }

    // ---- 2) Gather fresh body state. ----
    std::vector<BodyRef> bodies;
    gatherBodies(registry, bodies);

    // ---- 3) Broadphase. ----
    const auto pairs = broadphase(bodies);

    // ---- 4) Narrowphase + solve. ----
    std::unordered_set<uint64_t> currentPairs;
    currentPairs.reserve(pairs.size());

    for (const auto& [ia, ib] : pairs) {
        const BodyRef& a = bodies[ia];
        const BodyRef& b = bodies[ib];

        ContactManifold contact;
        if (!narrowphase(a, b, contact)) continue;

        contacts_.push_back(contact);
        const uint64_t pk = pairKey(a.entity, b.entity);
        currentPairs.insert(pk);

        if (contact.isTrigger) continue;

        RigidbodyComponent& rbA = registry.get<RigidbodyComponent>(a.entity);
        RigidbodyComponent& rbB = registry.get<RigidbodyComponent>(b.entity);
        TransformComponent& tA = registry.get<TransformComponent>(a.entity);
        TransformComponent& tB = registry.get<TransformComponent>(b.entity);

        const Vec2 n = contact.normal;
        const float invA = a.invMass, invB = b.invMass;
        const float invSum = invA + invB;
        if (invSum <= 0.0f) continue;

        // Positional correction (Baumgarte-style, slop + percent).
        const float corrMag = std::max(contact.penetration - positionalSlop, 0.0f)
                            * positionalCorrectionPercent / invSum;
        if (!a.isStatic) tA.position += n * (corrMag * invA);
        if (!b.isStatic) tB.position -= n * (corrMag * invB);

        // Normal impulse with restitution.
        const Vec2 rv = rbA.velocity - rbB.velocity;
        const float velAlongNormal = rv.dot(n);
        if (velAlongNormal > 0.0f) continue;  // already separating

        const float e = std::max(a.restitution, b.restitution);
        float j = -(1.0f + e) * velAlongNormal / invSum;
        const Vec2 impulse = n * j;
        if (!a.isStatic) rbA.velocity += impulse * invA;
        if (!b.isStatic) rbB.velocity -= impulse * invB;

        // Friction (tangent) impulse, clamped to cone of normal impulse.
        const Vec2 rv2 = rbA.velocity - rbB.velocity;
        Vec2 tangent = rv2 - n * rv2.dot(n);
        const float tLen = tangent.length();
        if (tLen > 1e-6f) {
            tangent /= tLen;
            float jt = -rv2.dot(tangent) / invSum;
            const float mu = std::sqrt(a.friction * b.friction);
            jt = std::clamp(jt, -std::fabs(j) * mu, std::fabs(j) * mu);
            const Vec2 frictionImpulse = tangent * jt;
            if (!a.isStatic) rbA.velocity += frictionImpulse * invA;
            if (!b.isStatic) rbB.velocity -= frictionImpulse * invB;
        }
    }

    // ---- 5) Diff contact pairs -> enter/exit events. ----
    for (uint64_t pk : currentPairs) {
        if (prevPairs_.find(pk) == prevPairs_.end()) {
            const Entity lo = static_cast<Entity>(pk >> 32);
            const Entity hi = static_cast<Entity>(pk & 0xFFFFFFFFu);
            bool trig = false;
            for (const auto& c : contacts_) {
                if (pairKey(c.a, c.b) == pk) { trig = c.isTrigger; break; }
            }
            entered_.push_back(CollisionEvent{lo, hi, trig});
        }
    }
    for (uint64_t pk : prevPairs_) {
        if (currentPairs.find(pk) == currentPairs.end()) {
            const Entity lo = static_cast<Entity>(pk >> 32);
            const Entity hi = static_cast<Entity>(pk & 0xFFFFFFFFu);
            exited_.push_back(CollisionEvent{lo, hi, false});
        }
    }
    prevPairs_ = std::move(currentPairs);
}

bool PhysicsWorld::raycast(const Registry& registry, Vec2 origin, Vec2 direction,
                           float maxDistance, RaycastHit& outHit,
                           Entity ignore) const {
    const Vec2 dir = direction.normalized();
    if (dir.lengthSq() < 0.5f) return false;

    bool found = false;
    float best = maxDistance;

    auto testCandidate = [&](Entity e) {
        AABB bounds;
        if (!worldAABB(registry, e, bounds)) return;

        // Slab test against the (possibly padded) AABB.
        const Vec2 inv(dir.x == 0.0f ? std::numeric_limits<float>::infinity() : 1.0f / dir.x,
                       dir.y == 0.0f ? std::numeric_limits<float>::infinity() : 1.0f / dir.y);
        float t1 = (bounds.min.x - origin.x) * inv.x;
        float t2 = (bounds.max.x - origin.x) * inv.x;
        float t3 = (bounds.min.y - origin.y) * inv.y;
        float t4 = (bounds.max.y - origin.y) * inv.y;
        float tmin = std::max(std::min(t1, t2), std::min(t3, t4));
        float tmax = std::min(std::max(t1, t2), std::max(t3, t4));
        if (tmax < 0.0f || tmin > tmax) return;
        const float hitDist = tmin >= 0.0f ? tmin : tmax;
        if (hitDist < 0.0f || hitDist >= best) return;

        // Normal from the face we entered through.
        Vec2 normal(0.0f, 0.0f);
        if (std::fabs(t1) < std::fabs(t3) && std::fabs(t2) < std::fabs(t3)) {
            normal = Vec2(t1 > t2 ? 1.0f : -1.0f, 0.0f);
        } else {
            normal = Vec2(0.0f, t3 > t4 ? 1.0f : -1.0f);
        }

        best = hitDist;
        outHit.entity = e;
        outHit.distance = hitDist;
        outHit.point = origin + dir * hitDist;
        outHit.normal = normal;
        found = true;
    };

    registry.each<BoxColliderComponent>([&](Entity e, const BoxColliderComponent&) {
        if (e != ignore) testCandidate(e);
    });
    registry.each<CircleColliderComponent>([&](Entity e, const CircleColliderComponent& c) {
        if (e == ignore) return;
        const auto* t = registry.has<TransformComponent>(e) ? &registry.get<TransformComponent>(e) : nullptr;
        if (!t) return;
        const float r = c.radius * std::max(t->scale.x, t->scale.y);
        // Ray-circle in world space.
        const Vec2 oc = origin - t->position;
        const float b = oc.dot(dir);
        const float cterm = oc.dot(oc) - r * r;
        const float disc = b * b - cterm;
        if (disc < 0.0f) return;
        const float tHit = -b - std::sqrt(disc);
        if (tHit < 0.0f || tHit >= best) return;
        best = tHit;
        outHit.entity = e;
        outHit.distance = tHit;
        outHit.point = origin + dir * tHit;
        outHit.normal = (outHit.point - t->position).normalized();
        found = true;
    });

    return found;
}

} // namespace nebula
