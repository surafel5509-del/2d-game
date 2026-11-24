#pragma once
// ============================================================
//  Nebula Engine - math/vec2.hpp
//  2D vector math: the fundamental position/direction type.
// ============================================================
#include <cmath>
#include <cstdint>

namespace nebula {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    static constexpr Vec2 zero() { return Vec2(0.0f, 0.0f); }
    static constexpr Vec2 one()  { return Vec2(1.0f, 1.0f); }
    static constexpr Vec2 up()   { return Vec2(0.0f, 1.0f); }
    static constexpr Vec2 right(){ return Vec2(1.0f, 0.0f); }

    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s)       const { return Vec2(x * s, y * s); }
    Vec2 operator*(const Vec2& o) const { return Vec2(x * o.x, y * o.y); }
    Vec2 operator/(float s)       const { return Vec2(x / s, y / s); }
    Vec2 operator-()              const { return Vec2(-x, -y); }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s)       { x *= s;   y *= s;   return *this; }
    Vec2& operator/=(float s)       { x /= s;   y /= s;   return *this; }

    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2& o) const { return !(*this == o); }

    float dot(const Vec2& o) const   { return x * o.x + y * o.y; }
    float cross(const Vec2& o) const { return x * o.y - y * o.x; }
    float lengthSq() const           { return x * x + y * y; }
    float length() const             { return std::sqrt(lengthSq()); }
    float distanceTo(const Vec2& o) const { return (*this - o).length(); }

    Vec2 normalized() const {
        const float len = length();
        return len > 1e-8f ? Vec2(x / len, y / len) : Vec2::zero();
    }

    Vec2 rotated(float radians) const {
        const float c = std::cos(radians), s = std::sin(radians);
        return Vec2(x * c - y * s, x * s + y * c);
    }

    static float angle(const Vec2& a, const Vec2& b) {
        return std::atan2(a.cross(b), a.dot(b));
    }

    static Vec2 lerp(const Vec2& a, const Vec2& b, float t) {
        return Vec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    }
};

inline Vec2 operator*(float s, const Vec2& v) { return v * s; }

} // namespace nebula
