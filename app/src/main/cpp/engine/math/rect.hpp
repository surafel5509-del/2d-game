#pragma once
// ============================================================
//  Nebula Engine - math/rect.hpp
//  Axis-aligned rectangles & AABBs + overlap tests.
// ============================================================
#include "vec2.hpp"
#include <algorithm>

namespace nebula {

struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

    static Rect fromCenterSize(Vec2 center, Vec2 size) {
        return Rect{center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y};
    }

    float left()   const { return x; }
    float right()  const { return x + w; }
    float bottom() const { return y; }
    float top()    const { return y + h; }
    Vec2  center() const { return Vec2(x + w * 0.5f, y + h * 0.5f); }

    bool contains(Vec2 p) const {
        return p.x >= x && p.x <= right() && p.y >= y && p.y <= top();
    }

    bool intersects(const Rect& o) const {
        return x < o.right() && right() > o.x && y < o.top() && top() > o.y;
    }
};

struct AABB {
    Vec2 min = Vec2::zero();
    Vec2 max = Vec2::zero();

    static AABB fromCenterHalfExtents(Vec2 center, Vec2 half) {
        return AABB{center - half, center + half};
    }

    Vec2  center()       const { return (min + max) * 0.5f; }
    Vec2  halfExtents()  const { return (max - min) * 0.5f; }
    float width()        const { return max.x - min.x; }
    float height()       const { return max.y - min.y; }
    float area()         const { return width() * height(); }

    bool overlaps(const AABB& o) const {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y;
    }

    bool contains(Vec2 p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
    }

    void expand(const AABB& o) {
        min.x = std::min(min.x, o.min.x); min.y = std::min(min.y, o.min.y);
        max.x = std::max(max.x, o.max.x); max.y = std::max(max.y, o.max.y);
    }
};

} // namespace nebula
