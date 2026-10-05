# Third-party notices

The project source is MIT; that does not relicense dependencies. Firmware is built from the following upstream sources. Retain these notices and the full texts in `LICENSES/` when redistributing binaries.

| Dependency | Pinned version | License / source |
|---|---|---|
| M5Cardputer | 2d4fa6646e4e5b47e0af96214b003aa7b15b8d81 | MIT; https://github.com/m5stack/M5Cardputer |
| M5Unified | e126f900d74fc4f759a1def65ebd84a4c6451b76 | MIT; https://github.com/m5stack/M5Unified |
| M5GFX / embedded LovyanGFX | 0.2.32 | MIT with retained component notices; https://github.com/m5stack/M5GFX |
| Arduino-IRremote | 4.4.1 | MIT; https://github.com/Arduino-IRremote/Arduino-IRremote |
| Arduino-ESP32 core | 2.0.17 | LGPL-2.1-or-later core and separate component licenses; https://github.com/espressif/arduino-esp32/tree/2.0.17 |
| Arduino-ESP32 BLE | 2.0.17 | Apache-2.0; https://github.com/espressif/arduino-esp32/tree/2.0.17/libraries/BLE |
| ESP-IDF components / bundled SDK | Arduino-ESP32 2.0.17 SDK | Apache-2.0 and per-component licenses; https://github.com/espressif/esp-idf/tree/v4.4.7 |
| Adafruit TCA8418 code in M5Cardputer | Pinned M5Cardputer tree | BSD; https://github.com/adafruit/Adafruit_TCA8418 |

Original component copyright/license headers remain in upstream sources. M5GFX also ships fonts and component-specific notices; consult the pinned tree. The application uses Font0, not external downloaded fonts.

Full application source, pinned dependency sources and build instructions are available so users can rebuild with modified LGPL core code. No modified proprietary library is added. Tools such as PlatformIO/esptool are build-time tools, not relicensed project code.
