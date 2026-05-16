# Solution: Component-form mismatch in wrapper provisioning

Related problem: [2026-05-16-component-form-mismatch-in-wrapper-provisioning.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-component-form-mismatch-in-wrapper-provisioning.md)

## What Failed

The generated Linux wrapper passed the IME component in fully qualified form, but the live Android runtime reported the same IME in short component form. The earlier comparison logic treated those as different values and incorrectly reported provisioning failure.

## What Worked

The runtime bridge now canonicalizes Android component forms before comparison, so it treats:

- `org.futo.inputmethod.latin/.LatinIME`
- `org.futo.inputmethod.latin/org.futo.inputmethod.latin.LatinIME`

as equivalent.

The same hardening now applies to:

- IME registration checks
- enabled-IME checks
- default-IME checks
- explicit activity-launch confirmation

## Why It Worked

Android commonly mixes short and fully qualified component forms across `ime list`, `enabled_input_methods`, `default_input_method`, and launch output. Canonicalizing the component identity at comparison time fixes the real interoperability bug instead of forcing one fragile string format.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl desktopify-apk emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin/.uix.settings.SettingsActivity /tmp/wfa-load /tmp/wfa-desktop
/tmp/wfa-desktop/org.futo.inputmethod.latin.sh
```
