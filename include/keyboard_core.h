#pragma once
#include <array>
#include <cstdint>

struct HidReport {
    std::array<uint8_t, 8> bytes{};
    bool operator==(const HidReport& other) const { return bytes == other.bytes; }
};
enum class KeyboardAction { None, ToggleDisplay, Pairing, ToggleHelp };
struct KeyboardResult {
    HidReport report;
    KeyboardAction action = KeyboardAction::None;
    bool send = false;
};
class KeyboardCore {
public:
    KeyboardResult update(uint64_t pressed, bool ready);
    void blockUntilRelease() { blocked_ = true; }
private:
    HidReport previous_;
    bool wasReady_ = false, blocked_ = false, waitingForRelease_ = true;
};
inline bool hidReady(bool connected, bool authenticated, bool subscribed, bool suspended) {
    return connected && authenticated && subscribed && !suspended;
}
inline bool pairingOpen(uint32_t now, uint32_t started, bool enabled) {
    return enabled && uint32_t(now - started) < 180000U;
}
inline bool authenticationAllowed(bool success, bool secureMitm, bool newPairing, bool windowOpen) {
    return success && secureMitm && (!newPairing || windowOpen);
}
