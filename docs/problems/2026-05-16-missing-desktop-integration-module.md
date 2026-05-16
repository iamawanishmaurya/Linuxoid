# Problem: Missing desktop integration module

- Date: 2026-05-16
- Slug: missing-desktop-integration-module

## Exact Error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp:3:10: fatal error: wfa/desktop_integration.hpp: No such file or directory
    3 | #include "wfa/desktop_integration.hpp"
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~
compilation terminated.
make[2]: *** [CMakeFiles/wfa_tests.dir/build.make:79: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o] Error 1
make[1]: *** [CMakeFiles/Makefile2:1081: CMakeFiles/wfa_tests.dir/all] Error 2
make: *** [Makefile:101: all] Error 2
```

## Reproduction Steps

1. Add P5 tests that require host desktop launch artifact generation and generic activity-launch reporting.
2. Run `cmake --build build` from `/home/astra/codex/wine-for-android`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Toolchain: `cmake`, `g++`, local `build/` tree
- Date: 2026-05-16

## First Hypothesis

The next phase depends on a new desktop-integration module that has not been created yet, including types for Linux launcher artifacts and helper functions for writing launcher scripts and `.desktop` entries.
