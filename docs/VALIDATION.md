# Validation

Build success does not prove radio, LCD or boot behavior.

| Check | Evidence |
|---|---|
| Native keyboard/pairing | 100 real C++ checks pass on 0.1.4 |
| ESP32-S3 / pinned library build | 0.1.4 passed |
| Pairing and ordinary input | User-confirmed 0.1.3, ADV / Honor 400 Pro |
| Screen flicker | User confirmed 0.1.3 no longer flickers |
| 0.1.4 guidance / three help pages | Pending device acceptance |
| Opt+B and typing while dark | Pending explicit device acceptance |
| Extended idle / reconnect / other phones | Pending dedicated acceptance |
| Clean standalone image boot | Pending; static image checks are separate |

Device acceptance: version; Opt+P → release → Enter; pair and type into a phone field; inspect three Opt+H pages; steady LCD; Opt+B off, keep typing, Opt+B on. Reboot/reconnect without replayed keys. Record actual results only.
