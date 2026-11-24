#pragma once
// ============================================================
//  Nebula Engine - ecs/components.hpp
//  Built-in component set. Games add their own by defining
//  plain structs and calling registry.add<MyComponent>(...).
// ============================================================
#include "engine/math/vec2.hpp"
#include "engine/math/color.hpp"
#include <string>

namespace nebula {

struct TransformComponent {
    Vec2 position = Vec2::zero();
    float rotation = 0.0f;   // radians, CCW
    Vec2 scale = Vec2::one();
};

struct SpriteComponent {
    std::string texture = "ship";   // logical texture name (portable in scene files)
    int textureId = 0;              // resolved GL/runtime handle (0 = unresolved)
    Vec2 sizePx = Vec2(96.0f, 96.0f);
    Color tint = Color::white();
    float zOrder = 0.0f;
    // Source rectangle inside the texture, in pixels. w/h <= 0 => full texture.
    float srcX = 0.0f, srcY = 0.0f, srcW = -1.0f, srcH = -1.0f;
    bool flipX = false;
    bool flipY = false;
};

struct NameComponent { std::string name = "Entity"; };
struct TagComponent  { std::string tag; };

struct RigidbodyComponent {
    Vec2 velocity = Vec2::zero();
    float gravityScale = 1.0f;
    float restitution = 0.25f;   // bounciness 0..1
    float friction = 0.45f;      // tangent impulse damping 0..1
    float density = 1.0f;        // used to derive mass from collider area
    float mass = 1.0f;           // <= 0 => derived from density * shape area
    bool isStatic = false;       // infinite mass, never integrated
};

struct BoxColliderComponent {
    Vec2 halfExtents = Vec2(32.0f, 32.0f);
    Vec2 offset = Vec2::zero();  // local-space offset
    bool isTrigger = false;      // overlaps fire events, no impulse
    float restitution = -1.0f;   // < 0 => inherit from rigidbody
    float friction = -1.0f;
};

struct CircleColliderComponent {
    float radius = 32.0f;
    Vec2 offset = Vec2::zero();
    bool isTrigger = false;
    float restitution = -1.0f;
    float friction = -1.0f;
};

struct LifetimeComponent { float secondsLeft = 1.0f; };

// Drives the engine's sprite animator (frame over a texture sheet).
struct SpriteAnimatorComponent {
    int frameW = 64, frameH = 64;
    int frameCount = 1;
    int columns = 1;
    float framesPerSecond = 8.0f;
    bool loop = true;
    bool playing = true;
    float accumulator = 0.0f;
    int currentFrame = 0;
    bool finished = false;
};

} // namespace nebula
