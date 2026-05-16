# Solution: Missing desktop integration module

Related problem: [2026-05-16-missing-desktop-integration-module.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-missing-desktop-integration-module.md)

## What Failed

The new P5 tests required a desktop-integration module, but the project had no types or helpers for writing Linux launcher scripts and `.desktop` entries yet.

## What Worked

The new desktop-integration slice added:

- `include/wfa/desktop_integration.hpp`
- `src/desktop_integration.cpp`
- `compatctl launch-activity`
- `compatctl desktopify-apk`

The implementation writes a Linux wrapper script plus a `.desktop` entry for a caller-selected Android component and reuses the existing APK inspection and staging path.

## Why It Worked

It gave the project a real host-side launch surface without pretending we already have native Linux rendering or compositor integration. That keeps the scope honest while moving P5 forward in a verifiable way.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl launch-activity emulator-5590 org.futo.inputmethod.latin/.uix.settings.SettingsActivity
./build/compatctl desktopify-apk emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin/.uix.settings.SettingsActivity /tmp/wfa-load /tmp/wfa-desktop
```
