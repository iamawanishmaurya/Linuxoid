# Solution: implement and link the real Wayland surface fixture

Related problem: [2026-05-17-wayland-surface-fixture-missing-implementation.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-wayland-surface-fixture-missing-implementation.md)

## What failed

The tests referenced a new Wayland surface fixture API, but there was no implementation file or build linkage for it yet.

## What worked

Added `src/wayland_surface_fixture.cpp`, linked it into `linuxoid_p1`, and implemented:

- `WaylandClientSupportCompiled`
- `RunWaylandSurfaceFixture`
- `RenderWaylandSurfaceFixtureJson`

The CMake build now detects `wayland-client` headers and library when available, enables the real `wl_display` plus `wl_surface` path in that case, and keeps an honest fallback path when Wayland support is unavailable.

## Why it worked

Once the real Wayland surface fixture had a concrete implementation and optional `wayland-client` linkage, the tests could exercise both the stable artifact contract and the honest fallback/success behavior without depending on an Android runtime.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl native-wayland-surface-fixture /tmp/linuxoid-wayland-surface-smoke 120 90
```
