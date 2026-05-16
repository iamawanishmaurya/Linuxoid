# Problem: native process bootstrap compile fails without `<iostream>`

## Exact Error

```text
/home/astra/codex/wine-for-android/src/native_lifecycle.cpp: In function ‘wfa::NativeLifecycleShim wfa::RunNativeProcessBootstrap(const NativeActivityBootstrap&)’:
/home/astra/codex/wine-for-android/src/native_lifecycle.cpp:419:10: error: ‘cout’ is not a member of ‘std’
  419 |     std::cout << report.output;
      |          ^~~~
/home/astra/codex/wine-for-android/src/native_lifecycle.cpp:15:1: note: ‘std::cout’ is defined in header ‘<iostream>’; this is probably fixable by adding ‘#include <iostream>’
   14 | #include <regex>
  +++ |+#include <iostream>
   15 | #include <sstream>
/home/astra/codex/wine-for-android/src/native_lifecycle.cpp:420:10: error: ‘cout’ is not a member of ‘std’
  420 |     std::cout.flush();
      |          ^~~~
/home/astra/codex/wine-for-android/src/native_lifecycle.cpp:420:10: note: ‘std::cout’ is defined in header ‘<iostream>’; this is probably fixable by adding ‘#include <iostream>’
make[2]: *** [CMakeFiles/wfa_core.dir/build.make:149: CMakeFiles/wfa_core.dir/src/native_lifecycle.cpp.o] Error 1
make[1]: *** [CMakeFiles/Makefile2:1023: CMakeFiles/wfa_core.dir/all] Error 2
make: *** [Makefile:101: all] Error 2
```

## Reproduction Steps

1. Edit `src/native_lifecycle.cpp` to add the new `RunNativeProcessBootstrap(...)` child-runner path.
2. Run `cmake --build build`.
3. Observe the compile failure in `src/native_lifecycle.cpp`.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-16`
- Build tool: `cmake --build build`
- Compiler toolchain: repo default C++20 toolchain configured by CMake

## First Hypothesis

The new child-runner code writes `report.output` to `std::cout`, but `src/native_lifecycle.cpp` does not include `<iostream>`, so the file no longer compiles after the bootstrap implementation change.
