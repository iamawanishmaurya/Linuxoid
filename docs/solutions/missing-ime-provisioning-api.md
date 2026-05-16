# Solution: Missing IME provisioning API

Related problem: [2026-05-16-missing-ime-provisioning-api.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-missing-ime-provisioning-api.md)

## What Failed

The new tests described a provisioning flow that needed a reusable command result type, a provision-and-verify orchestration function, and a rendered provisioning report, but none of those APIs existed in the runtime bridge yet.

## What Worked

The runtime bridge was extended with:

- `CommandResult` and `CommandRunner`
- `ProvisionAdbImeWithRunner(...)` for testable orchestration
- `ProvisionAdbIme(...)` for the real CLI path
- `RenderAdbProvisioningReport(...)` for evidence-rich output
- a new `compatctl provision-ime ...` command in `src/main.cpp`

## Why It Worked

The new runner abstraction let the tests drive the provisioning sequence without a real device, while the real CLI still uses live `adb` commands under the same control flow. That kept the logic small, testable, and aligned with the live runtime path.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl provision-ime emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME org.futo.inputmethod.latin/.uix.settings.SettingsActivity
```
