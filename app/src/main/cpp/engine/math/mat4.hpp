#pragma once
// ============================================================
//  Nebula Engine - math/mat4.hpp
//  Column-major 4x4 matrix: view/projection math for the
//  orthographic 2D camera. CPU-side, allocation-free.
// ============================================================
#include "vec2.hpp"
#include <cmath>
#include <cstring>

namespace nebula {

struct Mat4 {
    // Column-major: m[col*4 + row]
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    static Mat4 identity() { return Mat4{}; }

    static Mat4 translation(float x, float y, float z = 0.0f) {
        Mat4 r;
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }

    static Mat4 scale(float sx, float sy, float sz = 1.0f) {
        Mat4 r;
        r.m[0] = sx; r.m[5] = sy; r.m[10] = sz;
        return r;
    }

    static Mat4 rotationZ(float radians) {
        Mat4 r;
        const float c = std::cos(radians), s = std::sin(radians);
        r.m[0] = c; r.m[4] = -s;
        r.m[1] = s; r.m[5] =  c;
        return r;
    }

    // Classic orthographic projection (right-handed, maps to NDC).
    static Mat4 ortho(float left, float right, float bottom, float top,
                      float nearPlane, float farPlane) {
        Mat4 r;
        const float rl = right - left, tb = top - bottom, fn = farPlane - nearPlane;
        r.m[0]  =  2.0f / rl;
        r.m[5]  =  2.0f / tb;
        r.m[10] = -2.0f / fn;
        r.m[12] = -(right + left) / rl;
        r.m[13] = -(top + bottom) / tb;
        r.m[14] = -(farPlane + nearPlane) / fn;
        r.m[15] = 1.0f;
        return r;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r{};
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                r.m[col * 4 + row] =
                      m[0 * 4 + row] * o.m[col * 4 + 0]
                    + m[1 * 4 + row] * o.m[col * 4 + 1]
                    + m[2 * 4 + row] * o.m[col * 4 + 2]
                    + m[3 * 4 + row] * o.m[col * 4 + 3];
            }
        }
        return r;
    }
};

} // namespace nebula
