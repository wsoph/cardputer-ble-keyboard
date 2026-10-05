# Validation

Build success does not prove radio, LCD or boot behavior.

| Check | Evidence |
|---|---|
| Native keyboard/pairing/wheel | 214 real C++ checks pass on 0.2.0, including HID descriptor decoding and repeat/release gates |
| ESP32-S3 / pinned library build | 0.2.0 passed with pinned Arduino-ESP32 2.0.17 and M5 libraries |
| Original SSH firmware regression | 96 terminal/keyboard/pinyin checks pass; SSH source unchanged |
| Pairing and ordinary input | User-confirmed 0.1.3, ADV / Honor 400 Pro |
| Screen flicker | User confirmed 0.1.3 no longer flickers |
| 0.1.4 guidance / three help pages | Pending device acceptance |
| 0.2.0 composite BLE HID / four help pages | Pending device acceptance |
| RustDesk Android 1.5.0 Touch mode / wheel | Source path verified; Honor phone end-to-end acceptance pending |
| Windows Terminal / Explorer / WorkBuddy / Hermes Desktop | Each application's actual scrolling pending |
| Opt+B and typing while dark | Pending explicit device acceptance |
| Extended idle / reconnect / other phones | Pending dedicated acceptance |
| Clean standalone image boot | Pending; static image checks are separate |

Device acceptance: version; Opt+P → release → Enter; pair and type into a phone field; inspect three Opt+H pages; steady LCD; Opt+B off, keep typing, Opt+B on. Reboot/reconnect without replayed keys. Record actual results only.

0.2.0 acceptance: phone forgets only this device if it has a stale descriptor, then re-pairs; screen says `keyboard + scroll`. Keep RustDesk Touch mode. In each target application's content area, test Opt+semicolon up, Opt+period down, short press, held repeat, and stopping when either key is released. Test partial releases do not type punctuation; ordinary punctuation and Fn arrows still work after full release. Repeat typing/scrolling with Opt+B dark screen. Disconnect/reconnect while holding a scroll chord: nothing replays until full release and a fresh press. Windows Terminal normal history and full-screen terminal programs are separate cases.
