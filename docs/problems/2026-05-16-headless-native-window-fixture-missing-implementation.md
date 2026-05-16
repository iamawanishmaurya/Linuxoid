# Problem: headless native-window fixture API declared but not implemented

## Exact error

```
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestHeadlessNativeWindowSurfaceTracksMetadataAndLifecycle()':
test_main.cpp:(.text+0xe3d7): undefined reference to `wfa::CreateHeadlessNativeWindowSurface(...)`
/usr/bin/ld: test_main.cpp:(.text+0xe3f9): undefined reference to `wfa::InspectNativeWindow(...)`
/usr/bin/ld: test_main.cpp:(.text+0xe59f): undefined reference to `wfa::NativeWindowLifecycleReady(...)`
/usr/bin/ld: test_main.cpp:(.text+0xe5cc): undefined reference to `wfa::DestroyHeadlessNativeWindowSurface(...)`
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestHeadlessFirstPixelFixtureWritesDeterministicMarker()':
test_main.cpp:(.text+0xe80b): undefined reference to `wfa::RunHeadlessFirstPixelFixture(...)`
/usr/bin/ld: test_main.cpp:(.text+0xeba1): undefined reference to `wfa::RenderFirstPixelFixtureJson(...)`
collect2: error: ld returned 1 exit status
```

## Reproduction steps

1. Declare the P2.1 headless native-window fixture API in `include/wfa/native_window_surface.hpp`.
2. Add tests that call the new API.
3. Run `cmake --build build`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Date: `2026-05-16`
- Toolchain: local `cmake` + C++20 build

## First hypothesis

The tests now reference the new surface API, but there is no implementation file linked into `linuxoid_p1` or `wfa_tests` yet.
