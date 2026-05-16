# ART Runtime Smoke Missing Module

## Exact error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp:3:10: fatal error: wfa/art_runtime_smoke.hpp: No such file or directory
    3 | #include "wfa/art_runtime_smoke.hpp"
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~
compilation terminated.
make[2]: *** [CMakeFiles/wfa_tests.dir/build.make:79: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o] Error 1
make[1]: *** [CMakeFiles/Makefile2:1121: CMakeFiles/wfa_tests.dir/all] Error 2
make: *** [Makefile:101: all] Error 2
```

## Reproduction steps

1. Open `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe the build fail while compiling `tests/test_main.cpp`.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Date: `2026-05-17`
- Toolchain: `CMake 4.0`, `C++20`

## First hypothesis

The tests now depend on a new ART runtime smoke seam, but the production header and implementation have not been created or wired into the build graph yet.
