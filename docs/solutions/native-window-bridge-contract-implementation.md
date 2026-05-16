# Solution: implement the ANativeWindow bridge contract over the existing fixture seams

Related problem: [2026-05-17-native-window-bridge-fixture-missing-implementation.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-native-window-bridge-fixture-missing-implementation.md)

## What failed

Linuxoid had headless, Wayland, and EGL fixture proofs, but the `ANativeWindow` stub still lacked an explicit bridge contract for geometry updates, deterministic metadata artifacts, and honest fallback reporting.

## What worked

Extended `native_window_surface.hpp` and `native_window_surface.cpp` with:

- `UpdateNativeWindowBufferGeometry`
- `GetNativeWindowGeometryUpdateCount`
- `RunNativeWindowBridgeFixture`
- `RenderNativeWindowBridgeFixtureJson`

The bridge fixture now records deterministic geometry updates, writes stable metadata and event artifacts, and reports whether it is running in headless fallback mode or probe-only mode when real Wayland and EGL probes succeed.

## Why it worked

The bridge contract stays centered on the `ANativeWindow` seam itself while reusing the existing Wayland and EGL probes only as honest backing signals. That makes the contract testable without pretending the real rendering path is already wired.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl native-window-bridge-fixture /tmp/linuxoid-native-window-bridge-smoke 44 28 1
```
