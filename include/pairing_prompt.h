#pragma once
#include <cstdint>

enum class PairingKind { None, Display, Entry, Comparison };
enum class PairingReply { None, Accept, Reject };
struct PairingPromptStatus {
    PairingKind kind = PairingKind::None;
    uint32_t displayNumber = 0;
    uint8_t digits = 0;
};
struct PairingInputResult {
    bool consumed = false;
    PairingKind kind = PairingKind::None;
    PairingReply reply = PairingReply::None;
    uint32_t passkey = 0;
};
class PairingPrompt {
public:
    bool begin(PairingKind kind, uint32_t number, uint32_t now, bool windowOpen);
    PairingInputResult update(uint64_t pressed, uint32_t now, bool windowOpen);
    PairingPromptStatus status() const;
    void clear();
private:
    PairingKind kind_ = PairingKind::None;
    uint32_t number_ = 0, entered_ = 0, started_ = 0;
    uint8_t digits_ = 0;
    bool released_ = false;
    uint64_t previous_ = 0;
};
