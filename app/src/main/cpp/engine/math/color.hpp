#pragma once
// ============================================================
//  Nebula Engine - math/color.hpp
//  RGBA color, straight alpha, float 0..1 channels.
// ============================================================
#include <cstdint>

namespace nebula {

struct Color {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

    constexpr Color() = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}

    static constexpr Color white()   { return Color(1, 1, 1, 1); }
    static constexpr Color black()   { return Color(0, 0, 0, 1); }
    static constexpr Color clear()   { return Color(0, 0, 0, 0); }

    static constexpr Color fromHex(uint32_t hex) {
        return Color(((hex >> 16) & 0xFF) / 255.0f,
                     ((hex >> 8)  & 0xFF) / 255.0f,
                     ((hex)       & 0xFF) / 255.0f, 1.0f);
    }

    static constexpr Color fromHexRGBA(uint32_t hex) {
        return Color(((hex >> 24) & 0xFF) / 255.0f,
                     ((hex >> 16) & 0xFF) / 255.0f,
                     ((hex >> 8)  & 0xFF) / 255.0f,
                     ((hex)       & 0xFF) / 255.0f);
    }

    static Color lerp(const Color& x, const Color& y, float t) {
        return Color(x.r + (y.r - x.r) * t, x.g + (y.g - x.g) * t,
                     x.b + (y.b - x.b) * t, x.a + (y.a - x.a) * t);
    }
};

} // namespace nebula
