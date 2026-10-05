#pragma once
#include <array>
#include <cstdint>

// Relative mouse input: buttons, X, Y, Wheel. Only Wheel is used.
inline std::array<uint8_t, 4> wheelReport(int8_t direction) {
    return {0, 0, 0, static_cast<uint8_t>(direction)};
}

class WheelCore {
public:
    int8_t update(int8_t direction, bool ready, uint32_t now, bool allKeysReleased) {
        const bool newReady = ready && !wasReady_;
        wasReady_ = ready;
        if (!ready || newReady) blockUntilRelease();
        if (!ready) return 0;
        if (waitingForRelease_) {
            if (!allKeysReleased) return 0;
            waitingForRelease_ = false;
        }
        if (direction != 1 && direction != -1) { held_ = 0; return 0; }
        if (direction != held_) {
            held_ = direction; lastStep_ = now; repeating_ = false;
            return direction;
        }
        const uint32_t interval = repeating_ ? 100U : 350U;
        if (uint32_t(now - lastStep_) < interval) return 0;
        lastStep_ = now; repeating_ = true;
        return direction;
    }
    void blockUntilRelease() { held_ = 0; waitingForRelease_ = true; }
private:
    uint32_t lastStep_ = 0;
    int8_t held_ = 0;
    bool wasReady_ = false, waitingForRelease_ = true, repeating_ = false;
};
