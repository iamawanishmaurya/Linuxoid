Related problem: [2026-05-16-fdroid-apk-auto-launcher-inference-picks-missing-component.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-fdroid-apk-auto-launcher-inference-picks-missing-component.md)

## What Failed

Linuxoid's automatic launcher inference chose `org.fdroid.fdroid.panic.CalculatorActivity` for the local F-Droid APK because it was the first manifest activity with `MAIN` and `LAUNCHER`. That activity is explicitly disabled in the manifest, so the generated Linux launcher failed with `Error type 3`.

## What Worked

- Added a manifest-level filter that skips launcher candidates with `android:enabled="false"`.
- Added a regression test that models the F-Droid-style manifest shape with a disabled launcher candidate followed by a real enabled main activity.
- Rebuilt Linuxoid, reran the tests, and reran the live local-APK verifier against `/home/astra/Downloads/F-Droid.apk`.

## Why It Worked

The original heuristic assumed the first `MAIN/LAUNCHER` activity was always a valid desktop entry point. F-Droid shows that this is not always true. By ignoring disabled launcher candidates, Linuxoid now selects the real foreground launcher activity and generates a Linux launcher that can start the app successfully on the live runtime.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl verify-apk-host-launch-auto 192.168.240.112:5555 /home/astra/Downloads/F-Droid.apk /tmp/linuxoid-apk-verify-fdroid /tmp/linuxoid-apk-applications-fdroid /tmp/linuxoid-apk-launchers-fdroid
```
