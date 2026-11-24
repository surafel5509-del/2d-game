#pragma once
// ============================================================
//  Nebula Engine - animation/sprite_animator.hpp
//  Advances SpriteAnimatorComponent and writes the current
//  frame's pixel rectangle into the SpriteComponent srcRect.
// ============================================================
#include "engine/ecs/registry.hpp"
#include "engine/ecs/components.hpp"
#include <cmath>

namespace nebula {

class SpriteAnimator {
public:
    // Advance one entity's animation; call once per frame.
    static void update(Registry& registry, Entity e, float dt) {
        if (!registry.has<SpriteAnimatorComponent>(e) || !registry.has<SpriteComponent>(e)) return;
        auto& anim = registry.get<SpriteAnimatorComponent>(e);
        auto& spr = registry.get<SpriteComponent>(e);
        if (!anim.playing || anim.frameCount <= 1) return;

        anim.accumulator += dt;
        const float frameTime = anim.framesPerSecond > 0.0f ? 1.0f / anim.framesPerSecond : 0.1f;
        while (anim.accumulator >= frameTime) {
            anim.accumulator -= frameTime;
            ++anim.currentFrame;
            if (anim.currentFrame >= anim.frameCount) {
                if (anim.loop) {
                    anim.currentFrame = 0;
                } else {
                    anim.currentFrame = anim.frameCount - 1;
                    anim.playing = false;
                    anim.finished = true;
                }
            }
        }

        const int col = anim.columns > 0 ? anim.currentFrame % anim.columns : 0;
        const int row = anim.columns > 0 ? anim.currentFrame / anim.columns : 0;
        spr.srcX = static_cast<float>(col * anim.frameW);
        spr.srcY = static_cast<float>(row * anim.frameH);
        spr.srcW = static_cast<float>(anim.frameW);
        spr.srcH = static_cast<float>(anim.frameH);
    }

    static void updateAll(Registry& registry, float dt) {
        registry.each<SpriteAnimatorComponent>(
            [&](Entity e, SpriteAnimatorComponent&) { update(registry, e, dt); });
    }
};

} // namespace nebula
