#pragma once
#include "keyboard_core.h"
#include "pairing_prompt.h"

struct BleKeyboardStatus {
    bool connected = false, authenticated = false, subscribed = false;
    bool pairing = false, capsLock = false;
    PairingPromptStatus prompt;
    unsigned pairingSeconds = 0;
    uint8_t error = 0;
    int32_t sendError = 0;
};
class BleKeyboard {
public:
    bool begin();
    void poll();
    void openPairing();
    bool pairingInput(uint64_t pressed);
    bool ready() const;
    bool send(const HidReport& report);
    BleKeyboardStatus status() const;
    void battery(uint8_t percent);
};
