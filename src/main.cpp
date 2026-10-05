#include <M5Cardputer.h>
#include "ble_keyboard.h"
#include "wheel_core.h"
#include "version.h"

namespace {
KeyboardCore keyboard;
WheelCore wheel;
BleKeyboard ble;
M5Canvas canvas(&M5Cardputer.Display);
bool canvasReady = false, frameValid = false;
BleKeyboardStatus displayedStatus;
bool displayedReady = false, displayedConfirmation = false;
uint8_t helpPage = 0, displayedHelpPage = 0;
bool supported = false, bleStarted = false, screenOn = true, confirmPairing = false;
bool bleInitializing = false;
bool confirmationReleased = false;
uint8_t brightness = 128;
uint32_t lastUiCheck = 0, lastBattery = 0;
bool lastReady = false;

uint64_t keys() {
    uint64_t pressed = 0;
    for (const auto& point : M5Cardputer.Keyboard.keyList()) {
        if (point.x >= 0 && point.x < 14 && point.y >= 0 && point.y < 4)
            pressed |= uint64_t(1) << (point.y * 14 + point.x);
    }
    return pressed;
}
void toggleScreen() {
    auto& display = M5Cardputer.Display;
    if (screenOn) { brightness = display.getBrightness(); display.setBrightness(0); screenOn = false; }
    else { screenOn = true; frameValid = false; display.setBrightness(brightness ? brightness : 128); }
}
lgfx::LovyanGFX& drawingTarget() {
    if (canvasReady) return canvas;
    return M5Cardputer.Display;
}
bool sameView(const BleKeyboardStatus& status, bool ready) {
    if (!frameValid || helpPage != displayedHelpPage || confirmPairing != displayedConfirmation) return false;
    if (confirmPairing) return true;
    const auto& previous = displayedStatus;
    if (status.prompt.kind != previous.prompt.kind) return false;
    if (status.prompt.kind == PairingKind::Entry) return status.prompt.digits == previous.prompt.digits;
    if (status.prompt.kind != PairingKind::None) return status.prompt.displayNumber == previous.prompt.displayNumber;
    if (helpPage) return true;
    return ready == displayedReady && status.connected == previous.connected
        && status.wheelReady == previous.wheelReady
        && status.authenticated == previous.authenticated && status.pairing == previous.pairing
        && (!status.pairing || status.pairingSeconds == previous.pairingSeconds)
        && status.error == previous.error && (!status.error || status.sendError == previous.sendError)
        && (status.error || status.capsLock == previous.capsLock);
}
void present() {
    drawingTarget().endWrite();
    // The LCD receives the completed frame, never the intermediate black clear.
    if (canvasReady) canvas.pushSprite(0, 0);
}
void line(int y, const char* text, uint16_t color = TFT_WHITE) {
    auto& target = drawingTarget();
    target.setTextColor(color, TFT_BLACK);
    target.setCursor(4, y);
    target.print(text);
}
void draw(bool force = false) {
    if (!screenOn) return;
    const auto status = ble.status();
    const bool ready = ble.ready();
    if (!force && sameView(status, ready)) return;
    displayedStatus = status; displayedReady = ready;
    displayedHelpPage = helpPage; displayedConfirmation = confirmPairing; frameValid = true;
    auto& display = drawingTarget();
    display.startWrite();
    display.fillScreen(TFT_BLACK);
    line(3, "Cardputer Keyboard " CARDKEY_VERSION, TFT_CYAN);
    if (!supported) {
        line(28, "Cardputer-ADV required", TFT_RED);
        present(); return;
    }
    if (!bleStarted) {
        line(28, bleInitializing ? "Starting Bluetooth..." : "Bluetooth init failed",
             bleInitializing ? TFT_YELLOW : TFT_RED);
        if (!bleInitializing) line(52, "Restart and retry");
        present(); return;
    }
    if (confirmPairing) {
        line(23, "Open pairing for 180 seconds?");
        line(40, "Connected phone will disconnect.");
        line(60, "1 Release ALL keys", TFT_YELLOW);
        line(76, "2 Press Enter to open pairing", TFT_YELLOW);
        line(96, "3 Phone Bluetooth: select name");
        line(115, "Fn+`: cancel");
    } else if (status.prompt.kind != PairingKind::None) {
        const auto kind = status.prompt.kind;
        if (kind == PairingKind::Entry) line(23, "Type phone's 6-digit PIN here:");
        else if (kind == PairingKind::Comparison) line(23, "Check PIN matches on both screens:");
        else line(23, "Enter this PIN on your phone:");
        display.setTextColor(TFT_YELLOW, TFT_BLACK);
        display.setTextSize(3);
        display.setCursor(50, 52);
        if (kind == PairingKind::Entry) {
            for (uint8_t i = 0; i < status.prompt.digits; ++i) display.print('*');
        } else display.printf("%06lu", static_cast<unsigned long>(status.prompt.displayNumber));
        display.setTextSize(1);
        if (kind == PairingKind::Comparison) line(96, "Match: Enter    Cancel: Fn+`");
        else if (kind == PairingKind::Entry) line(96, "6 digits + Enter; Del: erase");
        else line(96, "Complete on phone; Fn+`: cancel");
        line(115, "Finish within 30 seconds");
    } else if (helpPage == 1) {
        line(20, "Help 1/4: Pair & screen", TFT_YELLOW);
        line(34, "Opt+P -> release ALL -> Enter");
        line(47, "Phone Bluetooth: select");
        line(60, "Cardputer Keyboard", TFT_CYAN);
        line(73, "Follow PIN prompt within 30s");
        line(88, "Opt+B: manual screen OFF / ON", TFT_GREEN);
        line(101, "Dark: typing + scroll still work");
        line(117, "Opt+H: next page (2/4)");
    } else if (helpPage == 2) {
        line(20, "Help 2/4: Basic keys", TFT_YELLOW);
        line(34, "Ctrl / Alt / Aa: modifiers");
        line(47, "Aa = Shift; Del = Backspace");
        line(60, "Fn+; , . /: up left down right");
        line(73, "Fn+`: Esc   Fn+Del: Delete");
        line(86, "Fn+1..0 - =: F1..F12");
        line(101, "Use US physical keyboard layout");
        line(117, "Opt+H: next page (3/4)");
    } else if (helpPage == 3) {
        line(20, "Help 3/4: Scroll", TFT_YELLOW);
        line(34, "Opt+; : scroll UP", TFT_GREEN);
        line(47, "Opt+. : scroll DOWN", TFT_GREEN);
        line(60, "Use physical keys; no Fn needed");
        line(73, "Tap: 1 step; hold: repeat");
        line(86, "Release: stop immediately");
        line(101, "Point mouse at area to scroll");
        line(117, "Opt+H: next page (4/4)");
    } else if (helpPage == 4) {
        line(20, "Help 4/4: Daily use", TFT_YELLOW);
        line(34, "Select a text field, then type");
        line(47, "Opt+C / V: Ctrl+Shift+C / V");
        line(60, "Terminal copy/paste: app-specific");
        line(73, "Chinese: phone/host input method");
        line(86, "Alt+Tab: app/OS decides behavior");
        line(101, "Opt+B: manual screen OFF / ON", TFT_GREEN);
        line(117, "Opt+H: back to keyboard");
    } else {
        if (ready) line(21, status.wheelReady ? "Connected - keyboard + scroll" : "Connected - keyboard only", TFT_GREEN);
        else if (status.connected && status.authenticated) line(21, "Connected - waiting for HID", TFT_YELLOW);
        else if (status.connected) line(21, "Connected - authenticating", TFT_YELLOW);
        else line(21, "Waiting for phone", TFT_YELLOW);
        line(37, "Name: Cardputer Keyboard");
        if (ready) {
            line(53, "Scroll UP: Opt+;   DOWN: Opt+.", TFT_CYAN);
            line(67, status.wheelReady ? "Point mouse at area; hold to scroll" : "No scroll: forget + re-pair phone");
            line(81, status.capsLock ? "Caps Lock ON; Opt+H: help" : "Select text field to type; Opt+H");
        } else {
            if (status.pairing) {
                display.setTextColor(TFT_GREEN, TFT_BLACK); display.setCursor(4, 53);
                display.printf("Pairing open: %u seconds", status.pairingSeconds);
            } else line(53, "1 Opt+P -> release ALL -> Enter");
            line(67, "2 Phone Bluetooth: select name");
            line(81, "3 Follow PIN prompt to pair");
        }
        line(98, "Opt+B: manual screen OFF / ON", TFT_GREEN);
        if (status.error) {
            display.setTextColor(TFT_RED, TFT_BLACK); display.setCursor(4, 115);
            if (status.sendError) display.printf("BLE error %u (send %ld)", status.error, long(status.sendError));
            else display.printf("Error %u: Opt+P, release, Enter", status.error);
        } else line(115, ready ? "Dark: typing + scroll still work" : "Opt+H: help; typing works when dark");
    }
    present();
}
}

void setup() {
    auto config = M5.config();
    config.internal_mic = false; config.internal_spk = false;
    M5Cardputer.begin(config, true);
    auto& display = M5Cardputer.Display;
    display.setRotation(1); display.setFont(&fonts::Font0); display.setTextSize(1);
    // Launcher may hand over a zero backlight; every app boot starts visibly.
    display.setBrightness(brightness);
    Serial.begin(115200);
    // Official M5Cardputer inputText example uses createSprite/pushSprite.
    // https://github.com/m5stack/M5Cardputer/blob/2d4fa6646e4e5b47e0af96214b003aa7b15b8d81/examples/Basic/keyboard/inputText/inputText.ino
    // RGB332 needs 32,400 bytes on ADV; allocate once in internal RAM before BLE.
    canvas.setPsram(false); canvas.setColorDepth(8);
    canvasReady = canvas.createSprite(display.width(), display.height()) != nullptr;
    canvas.setFont(&fonts::Font0); canvas.setTextSize(1);
    // Allocation failure retains a visible direct-drawing UI and working keyboard.
    Serial.printf("CARDKEY_DISPLAY_BUFFER=%u heap=%u\n", canvasReady ? 1U : 0U, ESP.getFreeHeap());
    supported = M5.getBoard() == m5::board_t::board_M5CardputerADV;
    Serial.printf("CARDKEY " CARDKEY_VERSION " board=%d\n", static_cast<int>(M5.getBoard()));
    bleInitializing = supported;
    draw(true);
    Serial.printf("CARDKEY_BOOT=display_ready heap=%u\n", ESP.getFreeHeap());
    if (supported) bleStarted = ble.begin();
    bleInitializing = false;
    Serial.printf("CARDKEY_BOOT=%s heap=%u\n", bleStarted ? "ble_ready" : "ble_failed", ESP.getFreeHeap());
    draw(true);
}

void loop() {
    M5Cardputer.update();
    if (!supported || !bleStarted) { delay(10); return; }
    ble.poll();
    const uint64_t pressed = keys();
    bool ready = ble.ready();
    const bool pairingConsumed = ble.pairingInput(pressed);
    if (pairingConsumed) {
        keyboard.blockUntilRelease(); confirmPairing = false;
        if (!screenOn) toggleScreen();
    }
    if (confirmPairing && !pairingConsumed) {
        if (!pressed) confirmationReleased = true;
        if (confirmationReleased && pressed == (uint64_t(1) << 41)) {
            keyboard.blockUntilRelease(); ble.openPairing();
            confirmPairing = false; ready = false;
        } else if (confirmationReleased && pressed == ((uint64_t(1) << 28) | 1)) {
            keyboard.blockUntilRelease(); confirmPairing = false;
        }
        keyboard.blockUntilRelease();
    }
    const auto result = keyboard.update(pressed, ready && !confirmPairing && !pairingConsumed);
    if (result.action == KeyboardAction::ToggleDisplay) toggleScreen();
    else if (result.action == KeyboardAction::ToggleHelp) helpPage = (helpPage + 1U) % 5U;
    else if (result.action == KeyboardAction::Pairing) {
        // Release remote held modifiers before the confirmation UI captures keyboard input.
        if (ready) ble.send(HidReport{});
        confirmPairing = true; confirmationReleased = false;
        if (!screenOn) toggleScreen();
    }
    if (result.send && !ble.send(result.report)) {
        // A failed enqueue must not leave a held modifier/character cached indefinitely.
        keyboard.update(pressed, false);
        ready = false;
    }
    const int8_t step = wheel.update(result.wheelDirection,
        ready && ble.wheelReady() && !confirmPairing && !pairingConsumed, millis(), pressed == 0);
    if (step && !ble.sendWheel(step)) {
        wheel.blockUntilRelease(); keyboard.blockUntilRelease();
    }
    if (ready != lastReady) {
        Serial.printf("CARDKEY_READY=%u heap=%u\n", ready ? 1U : 0U, ESP.getFreeHeap());
        lastReady = ready;
    }
    if (millis() - lastBattery >= 30000U) {
        const auto level = M5Cardputer.Power.getBatteryLevel();
        if (level >= 0) ble.battery(level > 100 ? 100 : uint8_t(level));
        lastBattery = millis();
    }
    if (screenOn && millis() - lastUiCheck >= 100U) {
        lastUiCheck = millis();
        draw();
    }
    delay(5);
}
