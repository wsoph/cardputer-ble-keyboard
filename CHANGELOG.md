# Changelog

## 0.2.0

- Add Opt+physical semicolon / period for standard mouse wheel up / down; no Fn or mode switch.
- Tap for one step; hold for bounded continuous scrolling; release to stop, including while the screen is off.
- Keep keyboard input independent of the mouse report subscription; prevent replay after reconnect, partial releases or send failures.
- Show scroll readiness and physical shortcuts on Home; add a scroll help page.
- Document RustDesk Touch mode, pointer placement and phone re-pairing for cached HID descriptors.
- Phone/RustDesk and each application's scroll behavior require device acceptance before publishing binaries.

## 0.1.4

- Replace app-specific screen wording with general Bluetooth keyboard instructions.
- Show pairing: Opt+P, release ALL keys, Enter, then select the keyboard on the phone.
- Explain Opt+B manual screen OFF/ON and continued typing while dark.
- Add three help pages cycled with Opt+H.
- Portable build scripts and separate Launcher/standalone release artifacts.
- Fix Linux native builds by using explicit standard size types.
- Pin both PlatformIO and esptool so clean environments can package firmware.

## 0.1.3

- Stop continuous screen flicker with offscreen rendering and redraw on displayed changes.

## 0.1.2

- Secure keyboard PIN entry, numeric comparison and device PIN display.
- Keep pairing input local; fixed diagnostics without PINs or typed text.

## 0.1.1

- Fix Bluetooth initialization crash and show a visible startup screen.

## 0.1.0

- Initial ADV BLE keyboard, US keys, two-key local shortcuts and manual backlight.
