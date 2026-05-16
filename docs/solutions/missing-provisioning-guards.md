# Solution: Missing provisioning guards

Related problem: [2026-05-16-missing-provisioning-guards.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-missing-provisioning-guards.md)

## What Failed

The provisioning report could still return a false green when the requested package name did not match the APK being installed, and it could still lose all post-action evidence if the readback query failed after successful install/enable/set steps.

## What Worked

The runtime bridge was hardened to:

- inspect the APK package name before the live provisioning flow
- store `apk_declared_package_name` and `apk_matches_requested_package` in the provisioning report
- keep `readback_ok` and `readback_error` in the report instead of throwing the whole result away
- require both package-match and successful readback before `Ready for typing: yes`

## Why It Worked

The package-name inspection ties the requested target package to the actual APK file, which prevents stale-device state from masking a wrong input file. The non-throwing readback path keeps partial evidence visible even when a late query step fails, which is exactly what an evidence-driven provisioning command needs.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl provision-ime emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME org.futo.inputmethod.latin/.uix.settings.SettingsActivity
./build/compatctl provision-ime emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk com.example.notkeyboard org.futo.inputmethod.latin/.LatinIME
```
