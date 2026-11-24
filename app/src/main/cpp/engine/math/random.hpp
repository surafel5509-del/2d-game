#pragma once
// ============================================================
//  Nebula Engine - math/random.hpp
//  Deterministic fast PRNG (xorshift32) for gameplay randomness.
// ============================================================
#include <cstdint>
#include <cmath>

namespace nebula {

class Random {
public:
    explicit Random(uint32_t seed = 0x9E3779B9u) : state_(seed ? seed : 0x9E3779B9u) {}

    uint32_t nextU32() {
        uint32_t x = state_;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        state_ = x;
        return x;
    }

    float next01() { return (nextU32() & 0x00FFFFFFu) / 16777216.0f; }
    float range(float lo, float hi) { return lo + (hi - lo) * next01(); }
    int   rangeInt(int loInclusive, int hiInclusive) {
        if (hiInclusive <= loInclusive) return loInclusive;
        return loInclusive + static_cast<int>(nextU32() % static_cast<uint32_t>(hiInclusive - loInclusive + 1));
    }
    bool chance(float p) { return next01() < p; }

    // Approx. unit gaussian via sum of uniforms.
    float gaussian() {
        return (next01() + next01() + next01() + next01() - 2.0f) * 1.7320508f;
    }

private:
    uint32_t state_;
};

} // namespace nebula
