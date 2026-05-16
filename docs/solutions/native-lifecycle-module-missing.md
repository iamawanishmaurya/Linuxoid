# Native Lifecycle Module Missing

Related problem: [2026-05-16-native-lifecycle-module-missing.md](../problems/2026-05-16-native-lifecycle-module-missing.md)

## What Failed

The first lifecycle-shim build failed because Linuxoid referenced `src/native_lifecycle.cpp` from CMake before the module existed.

## What Worked

Implemented the missing lifecycle surface directly:

- added `include/wfa/native_lifecycle.hpp`
- added `src/native_lifecycle.cpp`
- wired the module into `wfa_core`
- added `native-lifecycle-shim` in `compatctl`
- switched the generated native bootstrap entrypoint over to the lifecycle shim
- added tests for session artifacts, activity state, and service bindings

## Why It Worked

The red-bar failure was accurately pointing at the missing architectural seam. Once Linuxoid had a concrete lifecycle module that could build session artifacts and expose service bindings from the bootstrap manifest, the build recovered and the local Calculator path gained a real bootstrap-to-lifecycle handoff.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl bootstrap-native-spike /tmp/linuxoid-native-calculator.apk /tmp/linuxoid-native-compat /tmp/linuxoid-native-spike
./build/compatctl native-lifecycle-shim /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
/tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/launch-native-activity.sh
```
