# Solution: Desktop Exec quoting and component mismatch

Related problem: [2026-05-16-desktop-exec-quoting-and-component-mismatch.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-desktop-exec-quoting-and-component-mismatch.md)

## What Failed

The `.desktop` launcher could still overstate readiness when its `Exec=` path contained spaces, and the desktopify path could still generate a launcher for a component from a different package than the APK being processed.

## What Worked

The desktop integration layer was hardened to:

- quote the `.desktop` `Exec=` path
- reject caller-selected components whose package does not match the desktopified APK package
- add regression coverage for both cases

## Why It Worked

The quoting fix makes the Linux launcher artifact honest for space-containing desktop roots, and the package-consistency check prevents one APK from accidentally generating a launcher for another app’s activity.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
PATH=/home/astra/codex/wine-for-android/build:$PATH compatctl desktopify-apk emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk org.futo.inputmethod.latin/.uix.settings.SettingsActivity /tmp/wfa-load '/tmp/wfa desktop path'
sed -n '1,20p' '/tmp/wfa desktop path/org.futo.inputmethod.latin.desktop'
./build/compatctl desktopify-apk emulator-5590 /home/astra/Downloads/keyboard-0.1.28.apk com.example.other/.SettingsActivity /tmp/wfa-load /tmp/wfa-bad
```
