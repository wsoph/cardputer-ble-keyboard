#include "keyboard_core.h"
#include "hid_device_info.h"
#include "pairing_prompt.h"
#include "wheel_core.h"
#include "hid_report_map.h"
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
void scrollShortcuts() {
    auto core = connected();
    const auto up = Opt | key(2, 11), down = Opt | key(3, 11);
    auto result = core.update(up, true);
    check(result.wheelDirection == 1 && report(result, 0, {}), "Opt+semicolon scrolls up without typing");
    check(core.update(up, true).wheelDirection == 1, "held scroll chord stays available for wheel repeat");
    check(core.update(key(2, 11), true).wheelDirection == 0, "releasing Opt stops wheel immediately");
    check(report(core.update(key(2, 11), true), 0, {}), "partial scroll release cannot type semicolon");
    core.update(0, true);
    check(report(core.update(key(2, 11), true), 0, {0x33}), "ordinary semicolon works after full release");
    core.update(0, true);
    check(core.update(down, true).wheelDirection == -1, "Opt+period scrolls down");
    check(core.update(up | down, true).wheelDirection == 0, "both scroll directions cancel");
    check(core.update(down | Shift, true).wheelDirection == 0, "three-key scroll chord is rejected");
    core.update(0, true);
    core.blockUntilRelease();
    check(core.update(up, true).wheelDirection == 0, "pairing input cannot generate a wheel event");
    core.update(0, true);
    check(core.update(up, false).wheelDirection == 0, "offline scroll is discarded");
    check(core.update(up, true).wheelDirection == 0, "reconnect never replays a held scroll chord");
    core.update(0, true);
    check(core.update(up, true).wheelDirection == 1, "fresh scroll chord after reconnect works");
}
void wheelTiming() {
    WheelCore wheel;
    check(wheel.update(0, true, 0, true) == 0, "wheel connection starts without scrolling");
    check(wheel.update(1, true, 10, false) == 1, "short up press emits one wheel step");
    check(wheel.update(1, true, 359, false) == 0, "holding does not repeat before initial delay");
    check(wheel.update(1, true, 360, false) == 1, "hold repeats after 350 milliseconds");
    check(wheel.update(1, true, 459, false) == 0, "repeat is limited to ten steps per second");
    check(wheel.update(1, true, 460, false) == 1, "repeat continues at 100 milliseconds");
    check(wheel.update(1, true, 10000, false) == 1, "late loop emits at most one step");
    check(wheel.update(1, true, 10001, false) == 0, "late loop never replays a backlog");
    check(wheel.update(0, true, 10002, true) == 0, "release immediately stops scrolling");
    check(wheel.update(-1, true, 10003, false) == -1, "fresh down press reverses wheel sign");
    check(wheel.update(-1, false, 10004, false) == 0, "loss of authentication or subscription stops wheel");
    check(wheel.update(-1, true, 10005, false) == 0, "resubscribe requires a fresh press");
    check(wheel.update(-1, true, 11000, false) == 0, "held scroll remains blocked after resubscribe");
    check(wheel.update(0, true, 11000, false) == 0, "partial chord release does not re-arm a lost subscription");
    check(wheel.update(-1, true, 11000, false) == 0, "restoring a partial chord cannot replay scroll");
    wheel.update(0, true, 11001, true);
    check(wheel.update(-1, true, 11002, false) == -1, "release re-arms wheel after resubscribe");
    wheel.blockUntilRelease();
    check(wheel.update(-1, true, 12000, false) == 0, "failed send cannot repeat while held");
    wheel.update(0, true, 12000, false);
    check(wheel.update(-1, true, 12000, false) == 0, "failed send requires all physical keys up");
    wheel.update(0, true, 12001, true);
    check(wheel.update(1, true, 12002, false) == 1, "fresh press recovers from failed send");
    check(wheel.update(2, true, 12003, false) == 0, "invalid wheel direction is ignored");
    WheelCore preheld;
    check(preheld.update(1, true, 0, false) == 0, "new mouse subscription never replays a preheld chord");
    preheld.update(0, true, 1, true);
    check(preheld.update(1, true, 0xffffff00U, false) == 1, "wheel starts before millis wraps");
    check(preheld.update(1, true, 93, false) == 0, "initial delay remains correct across millis wrap");
    check(preheld.update(1, true, 94, false) == 1, "wheel repeats at wrapped deadline");
    const auto up = wheelReport(1), down = wheelReport(-1);
    check(up[0] == 0 && up[1] == 0 && up[2] == 0 && up[3] == 1, "up report contains no click or pointer movement");
    check(down[0] == 0 && down[1] == 0 && down[2] == 0 && down[3] == 255, "down report uses signed HID wheel byte");
}
void hidDescriptor() {
    // Decode short HID items, verifying the actual payload sizes and Wheel field.
    unsigned reportId = 0, size = 0, count = 0, page = 0;
    unsigned bits[3] = {}, outputs[3] = {}, usages[4] = {}, usageCount = 0;
    int minimum = 0, maximum = 0;
    bool wheelDeclared = false;
    for (std::size_t offset = 0; offset < sizeof(hidReportMap);) {
        const uint8_t prefix = hidReportMap[offset++];
        unsigned length = prefix & 3U;
        if (length == 3) length = 4;
        check(prefix != 0xfe && offset + length <= sizeof(hidReportMap), "HID item is bounded");
        unsigned value = 0;
        for (unsigned i = 0; i < length; ++i) value |= unsigned(hidReportMap[offset++]) << (i * 8);
        const unsigned tag = prefix & 0xfcU;
        if (tag == 0x84) { reportId = value; check(reportId == 1 || reportId == 2, "HID uses the two declared report IDs"); }
        else if (tag == 0x74) size = value;
        else if (tag == 0x94) count = value;
        else if (tag == 0x04) page = value;
        else if (tag == 0x14) minimum = length == 1 ? int(int8_t(value)) : int(value);
        else if (tag == 0x24) maximum = int(value);
        else if (tag == 0x08 && usageCount < 4) usages[usageCount++] = value;
        if (tag == 0x80 || tag == 0x90) {
            check(reportId > 0 && reportId <= 2, "HID data field has a valid report ID");
            if (tag == 0x80) {
                for (unsigned i = 0; i < usageCount; ++i) {
                    if (reportId == 2 && page == 1 && usages[i] == 0x38) {
                        wheelDeclared = true;
                        check(size == 8 && minimum == -127 && maximum == 127 && value == 6,
                              "Wheel is a signed eight-bit relative field");
                        check(i < count && bits[2] + i * size == 24, "Wheel occupies payload byte four");
                    }
                }
                bits[reportId] += size * count;
            } else outputs[reportId] += size * count;
        }
        if ((prefix & 0x0cU) == 0) usageCount = 0;
    }
    check(bits[1] == 64 && outputs[1] == 8, "keyboard descriptor preserves input and LED payload sizes");
    check(bits[2] == 32 && outputs[2] == 0 && wheelDeclared, "mouse descriptor matches four-byte wheel reports");
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
    matrix(); localActions(); scrollShortcuts(); wheelTiming(); hidDescriptor(); reconnect(); pairing(); deviceInformation(); pairingPrompts();
    std::printf("PASS: %d keyboard checks\n", checks);
}
