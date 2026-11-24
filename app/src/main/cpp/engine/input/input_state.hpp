#pragma once
// ============================================================
//  Nebula Engine - input/input_state.hpp
//  Engine-side input abstraction fed by the platform layer
//  (Android MotionEvent / KeyEvent via JNI). Multi-touch with
//  up to 10 simultaneous pointers, plus a held-key set.
// ============================================================
#include "engine/math/vec2.hpp"
#include <array>
#include <mutex>
#include <unordered_set>
#include <cstdint>

namespace nebula {

inline constexpr int kMaxPointers = 10;

struct PointerState {
    bool down = false;
    Vec2 position = Vec2::zero();      // screen pixels
    Vec2 downPosition = Vec2::zero();  // where the stroke began
    uint64_t downTimeMs = 0;
};

class InputState {
public:
    static constexpr int ACTION_DOWN = 0;
    static constexpr int ACTION_MOVE = 1;
    static constexpr int ACTION_UP   = 2;
    static constexpr int ACTION_CANCEL = 3;

    void pointerEvent(int pointerIndex, int action, Vec2 position, uint64_t timeMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pointerIndex < 0 || pointerIndex >= kMaxPointers) return;
        PointerState& p = pointers_[pointerIndex];
        switch (action) {
            case ACTION_DOWN:
                p.down = true;
                p.position = position;
                p.downPosition = position;
                p.downTimeMs = timeMs;
                ++activeTouches_;
                break;
            case ACTION_MOVE:
                p.position = position;
                break;
            case ACTION_UP:
            case ACTION_CANCEL:
                if (p.down && activeTouches_ > 0) --activeTouches_;
                p.down = false;
                p.position = position;
                break;
            default: break;
        }
    }

    void keyEvent(int keyCode, bool down) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (down) keysDown_.insert(keyCode); else keysDown_.erase(keyCode);
    }

    void endFrame() {
        // (reserved for per-frame edge detection; state is live)
    }

    // --- Queries (screen-pixel space) ---
    bool isTouching() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return activeTouches_ > 0;
    }

    int activePointerCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return activeTouches_;
    }

    PointerState pointer(int index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index < 0 || index >= kMaxPointers) return PointerState{};
        return pointers_[index];
    }

    Vec2 primaryPointerPosition() const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& p : pointers_) if (p.down) return p.position;
        return Vec2::zero();
    }

    bool isKeyDown(int keyCode) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return keysDown_.find(keyCode) != keysDown_.end();
    }

private:
    mutable std::mutex mutex_;
    std::array<PointerState, kMaxPointers> pointers_{};
    std::unordered_set<int> keysDown_;
    int activeTouches_ = 0;
};

} // namespace nebula
