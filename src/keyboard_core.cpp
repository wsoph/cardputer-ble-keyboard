#include "keyboard_core.h"
#include <cstddef>

namespace {
constexpr uint64_t bit(int row, int column) { return uint64_t(1) << (row * 14 + column); }
constexpr auto Fn = bit(2, 0), Shift = bit(2, 1), Ctrl = bit(3, 0), Opt = bit(3, 1), Alt = bit(3, 2);
// USB keyboard usages for the physical US matrix in the pinned M5Cardputer Keyboard.h.
// https://github.com/m5stack/M5Cardputer/blob/2d4fa6646e4e5b47e0af96214b003aa7b15b8d81/src/utility/Keyboard/Keyboard.h
constexpr uint8_t normal[56] = {
    0x35,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2d,0x2e,0x2a,
    0x2b,0x14,0x1a,0x08,0x15,0x17,0x1c,0x18,0x0c,0x12,0x13,0x2f,0x30,0x31,
    0,0,0x04,0x16,0x07,0x09,0x0a,0x0b,0x0d,0x0e,0x0f,0x33,0x34,0x28,
    0,0,0,0x1d,0x1b,0x06,0x19,0x05,0x11,0x10,0x36,0x37,0x38,0x2c
};
uint8_t usage(int index, bool fn) {
    if (!fn) return normal[index];
    if (index == 0) return 0x29;
    if (index >= 1 && index <= 12) return 0x39 + index;
    if (index == 13) return 0x4c;
    if (index == 39) return 0x52;
    if (index == 52) return 0x50;
    if (index == 53) return 0x51;
    if (index == 54) return 0x4f;
    return 0;
}
}

KeyboardResult KeyboardCore::update(uint64_t pressed, bool ready) {
    pressed &= (uint64_t(1) << 56) - 1;
    KeyboardResult result;
    const bool newReady = ready && !wasReady_;
    if (!ready || newReady) waitingForRelease_ = true;
    wasReady_ = ready;
    if (blocked_ && !pressed) blocked_ = false;
    if (!blocked_ && (pressed & Opt) && !(pressed & (Ctrl | Shift | Alt | Fn))) {
        if (pressed & bit(3, 7)) result.action = KeyboardAction::ToggleDisplay;
        else if (pressed & bit(1, 10)) result.action = KeyboardAction::Pairing;
        else if (pressed & bit(2, 7)) result.action = KeyboardAction::ToggleHelp;
        if (result.action != KeyboardAction::None) blocked_ = true;
    }
    if (!ready) return result;
    if (waitingForRelease_) {
        if (pressed) return result;
        waitingForRelease_ = false;
        previous_ = {};
        result.send = true;
        return result;
    }
    if (!blocked_) {
        if (pressed & Opt) {
            // Two physical keys produce the Windows Terminal three-key shortcuts.
            if (!(pressed & (Ctrl | Shift | Alt | Fn))) {
                if (pressed & bit(3, 6)) { result.report.bytes[0] = 3; result.report.bytes[2] = 0x19; }
                else if (pressed & bit(3, 5)) { result.report.bytes[0] = 3; result.report.bytes[2] = 0x06; }
            }
        } else {
            result.report.bytes[0] = ((pressed & Ctrl) ? 1 : 0) | ((pressed & Shift) ? 2 : 0) | ((pressed & Alt) ? 4 : 0);
            std::size_t slot = 2;
            for (int index = 0; index < 56; ++index) {
                const uint8_t code = usage(index, pressed & Fn);
                if (!(pressed & (uint64_t(1) << index)) || !code) continue;
                if (slot == 8) {
                    // USB ErrorRollOver instead of silently dropping held keys.
                    for (std::size_t i = 2; i < 8; ++i) result.report.bytes[i] = 1;
                    break;
                }
                result.report.bytes[slot++] = code;
            }
        }
    }
    result.send = !(result.report == previous_);
    previous_ = result.report;
    return result;
}
