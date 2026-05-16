# Solution: implement and link the EGL smoke fixture

Related problem: [2026-05-17-egl-smoke-fixture-missing-implementation.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-egl-smoke-fixture-missing-implementation.md)

## What failed

The tests referenced a new EGL smoke fixture API, but there was no implementation file or build linkage for it yet.

## What worked

Added `src/egl_smoke_fixture.cpp`, linked it into `linuxoid_p1`, and implemented:

- `EglSupportCompiled`
- `RunEglSmokeFixture`
- `RenderEglSmokeFixtureJson`

The CMake build now detects `EGL/egl.h` and `libEGL` when available, enables the real `eglGetDisplay` plus `eglInitialize` plus `eglCreateContext` plus `eglCreatePbufferSurface` path in that case, and keeps an honest fallback path when EGL support is unavailable.

## Why it worked

Once the EGL smoke fixture had a concrete implementation and optional `libEGL` linkage, the tests could exercise both the stable artifact contract and the honest runtime/build fallback behavior without depending on any Android runtime.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl native-egl-smoke-fixture /tmp/linuxoid-egl-smoke 96 72
```
