# Solution: Same-package component typo readiness

Related problem: [2026-05-16-same-package-component-typo-readiness.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-same-package-component-typo-readiness.md)

## What Failed

The host-launch artifact path rejected cross-package components, but it could still accept a typo that stayed inside the APK package namespace and generate a misleading “ready” launcher.

## What Worked

The desktopify path now validates the requested component against the APK’s manifest-declared components before generating launcher artifacts.

## Why It Worked

Package-prefix validation alone is not enough. Manifest-backed validation closes the last same-package false-success path and keeps launcher readiness tied to something the APK really declares.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl desktopify-apk emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin/.DoesNotExist /tmp/wfa-load /tmp/wfa-bad-unknown
```
