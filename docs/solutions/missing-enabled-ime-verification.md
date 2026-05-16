# Solution: Missing enabled-IME verification

Related problem: [2026-05-16-missing-enabled-ime-verification.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-missing-enabled-ime-verification.md)

## What Failed

The runtime bridge could prove that the IME service was registered and selected as default, but it did not yet inspect the secure `enabled_input_methods` setting. The tighter tests failed because the status model had no explicit enabled-IME field or parser.

## What Worked

The runtime bridge was extended to:

- parse the colon-delimited `enabled_input_methods` value exactly
- store `ime_enabled` and `enabled_input_methods` on `AdbImeStatus`
- include enabled-IME evidence in both status and provisioning reports
- require explicit enabled-IME proof before `Ready for typing: yes`

## Why It Worked

Checking `enabled_input_methods` closes the gap between “the IME exists” and “the runtime has actually enabled it for use.” That makes the final readiness verdict stricter and less likely to report false success.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl adb-ime-status emulator-5590 org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME org.futo.inputmethod.latin/.uix.settings.SettingsActivity
./build/compatctl provision-ime emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME org.futo.inputmethod.latin/.uix.settings.SettingsActivity
```
