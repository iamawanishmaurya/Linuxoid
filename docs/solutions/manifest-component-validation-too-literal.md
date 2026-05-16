# Solution: Manifest component validation too literal

Related problem: [2026-05-16-manifest-component-validation-too-literal.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-manifest-component-validation-too-literal.md)

## What Failed

The first manifest-backed validator treated short and fully qualified component spellings as different values, so a valid launcher component could still be rejected.

## What Worked

The desktop integration layer now canonicalizes component identity before comparing:

- caller-selected components
- manifest-declared components

This matches the same short-vs-full component hardening already used in the runtime bridge.

## Why It Worked

Android manifests and runtime outputs freely mix short and fully qualified component forms. Canonical comparison keeps the validator strict about declaration while staying tolerant of equivalent syntax.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl desktopify-apk emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin/.uix.settings.SettingsActivity /tmp/wfa-load /tmp/wfa-desktop-verified
```
