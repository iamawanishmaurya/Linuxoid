# Solution: Post-hoc package mismatch guard

Related problem: [2026-05-16-posthoc-package-mismatch-guard.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-posthoc-package-mismatch-guard.md)

## What Failed

The mismatch guard could detect that the APK did not match the requested package name, but only after it had already started mutating the live Android target.

## What Worked

The provisioning flow now:

- resolves the APK’s declared package before any ADB mutation
- returns immediately with a mismatch error when the declared package does not match the requested package
- preserves already-discovered readback facts as later readback commands succeed or fail

## Why It Worked

Failing closed before mutation removes the last false-green safety hole from the provisioning command. Preserving partial readback facts keeps the command evidence-rich even when a later query step fails.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl provision-ime emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk com.example.notkeyboard org.futo.inputmethod.latin/.LatinIME
./build/compatctl provision-ime emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME org.futo.inputmethod.latin/.uix.settings.SettingsActivity
```
