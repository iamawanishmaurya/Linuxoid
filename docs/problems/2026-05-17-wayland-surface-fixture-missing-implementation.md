# Problem: real Wayland surface fixture API declared but not implemented

## Exact error

```text
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestWaylandSurfaceFixtureWritesDeterministicMetadata()':
test_main.cpp:(.text+0xfadc): undefined reference to `wfa::RunWaylandSurfaceFixture(...)`
/usr/bin/ld: test_main.cpp:(.text+0xfde6): undefined reference to `wfa::RenderWaylandSurfaceFixtureJson(...)`
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestWaylandSurfaceFixtureReportsAvailabilityHonestly()':
test_main.cpp:(.text+0x1036c): undefined reference to `wfa::RunWaylandSurfaceFixture(...)`
/usr/bin/ld: test_main.cpp:(.text+0x10380): undefined reference to `wfa::WaylandClientSupportCompiled()'
collect2: error: ld returned 1 exit status
```

## Reproduction steps

1. Declare the real Wayland surface fixture API in `include/wfa/wayland_surface_fixture.hpp`.
2. Add tests that call the new API.
3. Run `cmake --build build`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Date: `2026-05-17`
- Toolchain: local `cmake` + C++20 build
- Wayland environment: `WAYLAND_DISPLAY=wayland-1`, `XDG_RUNTIME_DIR=/run/user/1000`

## First hypothesis

The tests reference the new Wayland surface fixture API, but there is no implementation file or CMake linkage for it yet.
