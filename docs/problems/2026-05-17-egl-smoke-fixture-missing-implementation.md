# Problem: EGL smoke fixture API declared but not implemented

## Exact error

```text
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestEglSmokeFixtureWritesDeterministicMetadata()':
test_main.cpp:(.text+0x10956): undefined reference to `wfa::RunEglSmokeFixture(...)'
/usr/bin/ld: test_main.cpp:(.text+0x10c60): undefined reference to `wfa::RenderEglSmokeFixtureJson(...)'
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestEglSmokeFixtureReportsAvailabilityHonestly()':
test_main.cpp:(.text+0x111e6): undefined reference to `wfa::RunEglSmokeFixture(...)'
/usr/bin/ld: test_main.cpp:(.text+0x111fa): undefined reference to `wfa::EglSupportCompiled()'
collect2: error: ld returned 1 exit status
```

## Reproduction steps

1. Declare the EGL smoke fixture API in `include/wfa/egl_smoke_fixture.hpp`.
2. Add tests that call the new API.
3. Run `cmake --build build`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Date: `2026-05-17`
- Toolchain: local `cmake` + C++20 build
- EGL environment: `pkg-config --exists egl` returns success

## First hypothesis

The tests reference the new EGL smoke fixture API, but there is no implementation file or CMake linkage for it yet.
