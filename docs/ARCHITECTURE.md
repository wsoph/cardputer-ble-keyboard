# Architecture

ADV-only firmware; no Wi-Fi, server or SD data file.

- `keyboard_core.*`: pure C++17 physical US matrix → eight-byte HID reports; press/hold/release; Fn keys; two-key local actions; release gates.
- `pairing_prompt.*`: bounded six-digit entry/comparison/display, release-before-confirm, 30-second expiry and local cancel. No pairing input becomes HID.
- `ble_keyboard.*`: Arduino-ESP32 2.0.17 HID and asynchronous ESP-IDF GAP replies. SC+MITM bonding; 180-second new-pairing window; saved-bond reconnect; authenticated/subscribed/not-suspended send gate.
- `main.cpp`: one 32,400-byte RGB332 M5Canvas in internal RAM. Render offscreen then push the frame; check visible state every 100ms; redraw on changes or wake only. Allocation failure keeps direct drawing on changes. Opt+B controls backlight only.

Input and credentials are never logged or embedded. Protocol-required PINs appear only during pairing; entry is masked. System-managed NVS stores bonds; the application never globally erases bonds or NVS.

The synchronous BLE security callbacks cannot wait for user input without blocking. Public custom GAP events plus `esp_ble_passkey_reply` / `esp_ble_confirm_reply` let the main loop complete verification. The library already initiates encryption; the app does not duplicate it.

Official pinned references:

- https://docs.m5stack.com/en/core/Cardputer-Adv
- https://github.com/m5stack/M5Cardputer/blob/2d4fa6646e4e5b47e0af96214b003aa7b15b8d81/examples/Basic/keyboard/inputText/inputText.ino
- https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/BLE/src/BLEDevice.cpp
- https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/bluetooth/esp_gap_ble.html
