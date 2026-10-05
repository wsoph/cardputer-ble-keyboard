#include "pairing_prompt.h"

bool PairingPrompt::begin(PairingKind kind, uint32_t number, uint32_t now, bool windowOpen) {
    if (!windowOpen || kind == PairingKind::None || number > 999999U || kind_ != PairingKind::None) return false;
    kind_ = kind; number_ = number; started_ = now;
    entered_ = 0; digits_ = 0; previous_ = 0; released_ = false;
    return true;
}
PairingInputResult PairingPrompt::update(uint64_t pressed, uint32_t now, bool windowOpen) {
    PairingInputResult result;
    if (kind_ == PairingKind::None) return result;
    result.consumed = true; result.kind = kind_;
    pressed &= (uint64_t(1) << 56) - 1;
    if (!windowOpen || uint32_t(now - started_) >= 30000U) result.reply = PairingReply::Reject;
    else if (!released_) {
        if (!pressed) released_ = true;
    } else if (pressed != previous_ && pressed) {
        constexpr uint64_t enter = uint64_t(1) << 41, backspace = uint64_t(1) << 13;
        constexpr uint64_t escape = (uint64_t(1) << 28) | 1;
        if (pressed == escape) result.reply = PairingReply::Reject;
        else if (pressed == enter && (kind_ == PairingKind::Comparison || (kind_ == PairingKind::Entry && digits_ == 6))) {
            result.reply = PairingReply::Accept;
            if (kind_ == PairingKind::Entry) result.passkey = entered_;
        } else if (kind_ == PairingKind::Entry) {
            if (pressed == backspace && digits_) { --digits_; entered_ /= 10; }
            else if (digits_ < 6) {
                for (unsigned column = 1; column <= 10; ++column) {
                    if (pressed == (uint64_t(1) << column)) {
                        entered_ = entered_ * 10 + (column == 10 ? 0 : column);
                        ++digits_; break;
                    }
                }
            }
        }
    }
    previous_ = pressed;
    if (result.reply != PairingReply::None) clear();
    return result;
}
PairingPromptStatus PairingPrompt::status() const {
    return {kind_, kind_ == PairingKind::Entry ? 0U : number_, digits_};
}
void PairingPrompt::clear() {
    kind_ = PairingKind::None; number_ = 0; entered_ = 0; started_ = 0;
    digits_ = 0; released_ = false; previous_ = 0;
}
