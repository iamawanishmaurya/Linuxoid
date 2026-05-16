# Native Bootstrap Surface Missing

Related problem: [2026-05-16-native-bootstrap-surface-missing.md](../problems/2026-05-16-native-bootstrap-surface-missing.md)

## What Failed

The first test-driven build for the native activity bootstrap slice failed because Linuxoid declared `BuildNativeActivityBootstrap` and `RenderNativeActivityBootstrapReport` without implementing them.

## What Worked

Implemented the missing native bootstrap surface directly in Linuxoid:

- added `BuildNativeActivityBootstrap`
- added `BootstrapNativeLaunchSpike`
- added `RenderNativeActivityBootstrapReport`
- added `bootstrap-native-spike` and `native-execute-stub` CLI commands
- added bootstrap artifact generation for:
  - `activity-bootstrap.json`
  - `native-env.sh`
  - `launch-native-activity.sh`
  - `bootstrap-report.txt`

## Why It Worked

The tests were already targeting the right missing seam. Once Linuxoid could turn a native plan into concrete bootstrap artifacts and a local stub entrypoint, the linker failure disappeared and the new slice became verifiable both in tests and with a real Calculator APK.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl bootstrap-native-spike /tmp/linuxoid-native-calculator.apk /tmp/linuxoid-native-compat /tmp/linuxoid-native-spike
/tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/launch-native-activity.sh
```
