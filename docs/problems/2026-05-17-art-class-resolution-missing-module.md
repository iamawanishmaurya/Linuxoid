# ART class-resolution missing module

## Exact error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp:3:10: fatal error: wfa/art_class_resolution_fixture.hpp: No such file or directory
    3 | #include "wfa/art_class_resolution_fixture.hpp"
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
compilation terminated.
make[2]: *** [CMakeFiles/wfa_tests.dir/build.make:79: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o] Error 1
make[1]: *** [CMakeFiles/Makefile2:1121: CMakeFiles/wfa_tests.dir/all] Error 2
make: *** [Makefile:101: all] Error 2
```

## Reproduction steps

1. `cd /home/astra/codex/wine-for-android`
2. Add the new ART class-resolution tests to `tests/test_main.cpp`
3. Run `cmake --build build && ctest --test-dir build --output-on-failure`

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`
- Toolchain: CMake + C++20 local build

## First hypothesis

The new test gate correctly references a production module that does not exist yet. Linuxoid needs a dedicated ART class-resolution fixture that can read staged DEX entries, resolve manifest-target descriptors offline, write deterministic artifacts, and expose a CLI command before the test suite can build again.
