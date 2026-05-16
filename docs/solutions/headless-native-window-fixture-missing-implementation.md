# Solution: implement and link the headless native-window fixture surface

Related problem: [2026-05-16-headless-native-window-fixture-missing-implementation.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-headless-native-window-fixture-missing-implementation.md)

## What failed

The P2.1 tests referenced the new headless native-window fixture API, but there was no implementation file or build linkage for it yet.

## What worked

Added `src/native_window_surface.cpp`, linked it into `linuxoid_p1`, and implemented:

- `CreateHeadlessNativeWindowSurface`
- `InspectNativeWindow`
- `NativeWindowLifecycleReady`
- `DestroyHeadlessNativeWindowSurface`
- `RunHeadlessFirstPixelFixture`
- `RenderFirstPixelFixtureJson`

## Why it worked

Once the surface API had a concrete implementation and was linked into the existing native runtime library, both the direct metadata test and the first-pixel marker test could resolve and execute normally.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
