Related problem: [2026-05-16-missing-auto-desktopify-launcher-inference.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-missing-auto-desktopify-launcher-inference.md)

## What Failed

Linuxoid could only generate Linux launch artifacts when the caller already knew the exact Android launcher component and when the script and `.desktop` file lived under the same root.

## What Worked

- Added manifest-backed automatic launcher selection.
- Added split desktop-entry and launcher-script roots.
- Added `desktopify-apk-auto` so Linuxoid can infer the host launch target from the APK itself.

## Why It Worked

The manifest parser already knew which activity was the launcher. Reusing that knowledge in the desktop-integration layer removed the manual component requirement and made host integration behave more like a real app installer instead of an engineering-only tool.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl desktopify-apk-auto emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk /tmp/wfa-load-auto /tmp/wfa-apps-auto /tmp/wfa-launchers-auto
./build/compatctl desktopify-apk emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin/.uix.settings.SettingsActivity /tmp/wfa-load-explicit /tmp/wfa-apps-explicit /tmp/wfa-launchers-explicit
```
