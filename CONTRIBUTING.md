# Contributing

Keep changes focused. Run native C++ checks and the ESP32-S3 build. Screen lines must fit 38 Font0 characters at size 1 within the 240×135 LCD.

Preserve:

- At most two simultaneous physical keys for documented operations.
- Opt+B changes backlight only; BLE and typing continue. No automatic sleep.
- Authenticated encrypted HID; new pairing needs an open window.
- Pairing and local shortcuts never leak to phone input.
- Release gates after local actions/reconnect; never replay offline keys.
- No PINs, typed keys, Bluetooth addresses, credentials or raw logs in diagnostics.
- Buffered drawing; unchanged screens never repeatedly clear the LCD.

Bug reports: firmware version, ADV hardware, phone/OS, reproduction and visible error. Share only sanitized fixed BLE events, never passcodes or private text. Separate compile success from device results.
