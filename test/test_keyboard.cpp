#include "keyboard_core.h"
#include "hid_device_info.h"
#include "pairing_prompt.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool value, const char* name) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
constexpr uint64_t key(int row, int column) { return uint64_t(1) << (row * 14 + column); }
constexpr auto Fn = key(2, 0), Shift = key(2, 1), Ctrl = key(3, 0), Opt = key(3, 1), Alt = key(3, 2);
bool report(const KeyboardResult& result, uint8_t modifiers, std::initializer_list<uint8_t> keys) {
    HidReport expected;
    expected.bytes[0] = modifiers;
    size_t index = 2;
    for (auto usage : keys) expected.bytes[index++] = usage;
    return result.report == expected;
}
KeyboardCore connected() {
    KeyboardCore core;
    check(core.update(0, true).send, "new connection starts with empty report");
    return core;
}
void matrix() {
    auto core = connected();
    auto a = core.update(key(2, 2), true);
    check(a.send && report(a, 0, {0x04}), "a press reaches host");
    check(!core.update(key(2, 2), true).send, "held a is not repeatedly tapped");
    check(core.update(0, true).send, "release reaches host");
    check(report(core.update(Shift | key(2, 2), true), 2, {0x04}), "shift+a retains modifier");
    check(report(core.update(key(2, 2), true), 0, {0x04}), "releasing shift while a held updates host");
    check(report(core.update(Ctrl | key(3, 5), true), 1, {0x06}), "Ctrl+C interrupt");
    check(report(core.update(Alt | key(1, 0), true), 4, {0x2b}), "Alt+Tab uses two physical keys");
    check(report(core.update(Shift | key(0, 1), true), 2, {0x1e}), "exclamation US keyboard");
    check(report(core.update(Shift | key(1, 13), true), 2, {0x31}), "vertical bar US keyboard");
    check(report(core.update(key(2, 13), true), 0, {0x28}), "Enter");
    check(report(core.update(key(0, 13), true), 0, {0x2a}), "Backspace");
    check(report(core.update(key(3, 13), true), 0, {0x2c}), "Space");
    check(report(core.update(key(0, 0), true), 0, {0x35}), "grave");
    check(report(core.update(Fn | key(0, 0), true), 0, {0x29}), "Fn+grave Escape");
    check(report(core.update(Fn | key(0, 13), true), 0, {0x4c}), "Fn+Backspace Delete");
    check(report(core.update(Fn | key(2, 11), true), 0, {0x52}), "Fn+semicolon Up");
    check(report(core.update(Fn | key(3, 10), true), 0, {0x50}), "Fn+comma Left");
    check(report(core.update(Fn | key(3, 11), true), 0, {0x51}), "Fn+period Down");
    check(report(core.update(Fn | key(3, 12), true), 0, {0x4f}), "Fn+slash Right");
    for (int column = 1; column <= 12; ++column)
        check(report(core.update(Fn | key(0, column), true), 0, {uint8_t(0x39 + column)}), "Fn F1-F12");
    check(report(core.update(Fn, true), 0, {}), "Fn never emits invalid usage FF");
    check(report(core.update(Fn | key(2, 2), true), 0, {}), "unassigned Fn layer stays empty");
    check(report(core.update(uint64_t(1) << 63, true), 0, {}), "invalid matrix bit ignored");
    check(report(core.update(key(2, 2) | key(2, 3), true), 0, {0x04, 0x16}), "two simultaneous letters");
    check(report(core.update(Opt | key(3, 6), true), 3, {0x19}), "Opt+V generates terminal Ctrl+Shift+V");
    check(report(core.update(Opt | key(3, 5), true), 3, {0x06}), "Opt+C generates terminal Ctrl+Shift+C");
    check(report(core.update(Opt | key(2, 2), true), 0, {}), "unknown Opt chord consumed");
    uint64_t tooMany = 0;
    for (int column = 1; column <= 7; ++column) tooMany |= key(0, column);
    check(report(core.update(tooMany, true), 0, {1,1,1,1,1,1}), "overflow reports rollover instead of dropping a release");
}
void localActions() {
    auto core = connected();
    core.update(Opt, true);
    auto result = core.update(Opt | key(3, 7), true);
    check(result.action == KeyboardAction::ToggleDisplay && report(result, 0, {}), "Opt then B toggles without typing b");
    check(core.update(Opt | key(3, 7), true).action == KeyboardAction::None, "holding screen shortcut toggles once");
    check(report(core.update(key(3, 7), true), 0, {}), "partial shortcut release does not leak b");
    core.update(0, true);
    check(report(core.update(key(3, 7), true), 0, {0x05}), "ordinary b still usable after all released");
    core.update(0, true);
    check(core.update(Opt | key(1, 10), true).action == KeyboardAction::Pairing, "two-key pairing action");
    core.update(0, true);
    check(core.update(Opt | key(2, 7), true).action == KeyboardAction::ToggleHelp, "two-key help action");
    core.blockUntilRelease();
    check(report(core.update(key(2, 13), true), 0, {}), "pair confirmation Enter cannot reach host");
    core.update(0, true);
    check(report(core.update(key(2, 13), true), 0, {0x28}), "fresh Enter after confirmation can reach host");
}
void reconnect() {
    KeyboardCore core;
    check(!core.update(key(2, 2), false).send, "unpaired input ignored");
    check(!core.update(key(2, 2), true).send, "ready transition does not replay held key");
    check(core.update(0, true).send, "release after reconnect sends empty baseline");
    check(report(core.update(key(2, 2), true), 0, {0x04}), "fresh post-connect key sent");
    check(!core.update(Ctrl | key(3, 5), false).send, "disconnect cancels remote input");
    check(!core.update(Ctrl | key(3, 5), true).send, "disconnect interrupt is not replayed");
    check(core.update(0, true).send, "resubscribe baseline even when prior report empty");
    check(core.update(Opt | key(3, 7), false).action == KeyboardAction::ToggleDisplay, "display action works offline");
    check(!hidReady(true, false, true, false), "unencrypted channel is not ready");
    check(!hidReady(true, true, false, false), "no notification subscription is not ready");
    check(!hidReady(true, true, true, true), "HID suspend is not ready");
    check(hidReady(true, true, true, false), "authenticated subscribed channel ready");
}
void pairing() {
    check(pairingOpen(100, 100, true), "pair window initially open");
    check(pairingOpen(180099, 100, true), "pair window remains open before limit");
    check(!pairingOpen(180100, 100, true), "pair window closes at 180 seconds");
    check(!pairingOpen(100, 100, false), "disabled pairing closed");
    check(pairingOpen(20, 0xfffffff0U, true), "millis wrap safe within window");
    check(!authenticationAllowed(false, true, false, false), "failed authentication rejected");
    check(!authenticationAllowed(true, false, false, true), "encryption without secure MITM rejected");
    check(!authenticationAllowed(true, true, true, false), "new pairing completion after window closes rejected");
    check(authenticationAllowed(true, true, true, true), "new authenticated pairing within window accepted");
    check(authenticationAllowed(true, true, false, false), "bonded encrypted reconnect without pairing window accepted");
}
// Arduino-ESP32 2.0.17 creates this optional characteristic only in manufacturer().
// Its string overload dereferences the existing pointer; construction alone is insufficient.
struct FakeCharacteristic {
    std::string value;
    void setValue(std::string next) { value = next; }
};
struct FakeHidDevice {
    bool allocationAvailable = true, created = false;
    FakeCharacteristic characteristic;
    FakeCharacteristic* manufacturer() {
        created = allocationAvailable;
        return created ? &characteristic : nullptr;
    }
    void manufacturer(std::string name) {
        if (!created) throw std::logic_error("uninitialized optional characteristic");
        characteristic.setValue(name);
    }
};
void deviceInformation() {
    FakeHidDevice device;
    bool invalidAccess = false, initialized = false;
    try { initialized = initializeHidManufacturer(device); }
    catch (const std::logic_error&) { invalidAccess = true; }
    check(!invalidAccess, "fresh HID initialization avoids access to an uncreated characteristic");
    check(initialized, "fresh HID manufacturer initialization succeeds");
    check(device.characteristic.value == "Cardputer Keyboard", "manufacturer value is available to the phone");
    FakeHidDevice unavailable;
    unavailable.allocationAvailable = false;
    check(!initializeHidManufacturer(unavailable), "unavailable manufacturer characteristic fails initialization safely");
    check(unavailable.characteristic.value.empty(), "failed characteristic allocation cannot write a value");
}
void pairingPrompts() {
    PairingPrompt prompt;
    check(!prompt.update(key(2, 2), 0, true).consumed, "normal input is unaffected without pairing UI");
    check(!prompt.begin(PairingKind::Entry, 0, 0, false), "closed pairing window cannot request PIN input");
    check(prompt.begin(PairingKind::Comparison, 123456, 100, true), "numeric comparison can wait for user confirmation");
    check(prompt.status().displayNumber == 123456, "comparison number available only on display");
    check(prompt.update(key(2, 13), 101, true).reply == PairingReply::None, "preheld Enter cannot approve pairing");
    prompt.update(0, 102, true);
    auto accepted = prompt.update(key(2, 13), 103, true);
    check(accepted.consumed && accepted.kind == PairingKind::Comparison && accepted.reply == PairingReply::Accept,
          "fresh Enter accepts comparison and is consumed locally");
    check(prompt.status().kind == PairingKind::None, "completed comparison clears its displayed number");
    prompt.begin(PairingKind::Entry, 0, 200, true); prompt.update(0, 201, true);
    for (int column : {10, 1, 2, 3, 4, 5}) {
        prompt.update(key(0, column), 202, true); prompt.update(0, 203, true);
    }
    check(prompt.status().digits == 6 && prompt.status().displayNumber == 0, "entered PIN is masked and supports leading zero");
    auto entered = prompt.update(key(2, 13), 204, true);
    check(entered.consumed && entered.reply == PairingReply::Accept && entered.passkey == 12345,
          "six-digit keyboard PIN including leading zero reaches pairing reply");
    prompt.begin(PairingKind::Entry, 0, 300, true); prompt.update(0, 301, true);
    prompt.update(key(0, 1), 302, true);
    prompt.update(key(0, 1), 303, true);
    check(prompt.status().digits == 1, "held digit is entered once");
    prompt.update(0, 304, true);
    check(prompt.update(key(2, 13), 305, true).reply == PairingReply::None, "partial PIN cannot be submitted");
    prompt.update(0, 306, true); prompt.update(key(0, 13), 307, true);
    check(prompt.status().digits == 0, "Backspace edits a pairing PIN locally");
    prompt.update(0, 308, true); prompt.update(Shift | key(0, 2), 309, true);
    check(prompt.status().digits == 0, "modified digit chords cannot alter pairing PIN");
    prompt.update(0, 310, true);
    check(prompt.update(Fn | key(0, 0), 311, true).reply == PairingReply::Reject, "two-key Escape rejects pairing");
    prompt.begin(PairingKind::Comparison, 1, 400, true);
    check(prompt.update(0, 404, false).reply == PairingReply::Reject, "pairing window expiry cancels pending comparison");
    prompt.begin(PairingKind::Entry, 0, 500, true);
    check(prompt.update(0, 30499, true).reply == PairingReply::None, "pairing UI remains pending before 30-second limit");
    check(prompt.update(0, 30500, true).reply == PairingReply::Reject, "unanswered pairing expires at 30 seconds");
    prompt.begin(PairingKind::Comparison, 1, 0xfffffff0U, true);
    check(prompt.update(0, 20, true).reply == PairingReply::None, "pairing timeout handles millis wrap");
    check(!prompt.begin(PairingKind::Entry, 0, 21, true), "pending challenge cannot be silently replaced");
    prompt.clear();
    check(!prompt.begin(PairingKind::Comparison, 1000000, 22, true), "invalid comparison number rejected");
    check(!prompt.begin(PairingKind::None, 0, 22, true), "empty challenge rejected");
    prompt.begin(PairingKind::Display, 654321, 600, true); prompt.update(0, 601, true);
    check(prompt.update(key(2, 13), 602, true).reply == PairingReply::None, "phone-entry PIN display cannot be locally approved");
    check(prompt.status().displayNumber == 654321, "phone-entry passkey remains available on device display");
    prompt.clear();
    check(prompt.status().digits == 0 && prompt.status().displayNumber == 0, "disconnect clears pairing data");
}
}
int main() {
    matrix(); localActions(); reconnect(); pairing(); deviceInformation(); pairingPrompts();
    std::printf("PASS: %d keyboard checks\n", checks);
}
