# Native spike planner header missing

- Exact error:
  ```text
  [ 71%] Built target wfa_core
  [ 85%] Built target compatctl
  [ 92%] Building CXX object CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o
  /home/astra/codex/wine-for-android/tests/test_main.cpp:6:10: fatal error: wfa/native_spike.hpp: No such file or directory
      6 | #include "wfa/native_spike.hpp"
        |          ^~~~~~~~~~~~~~~~~~~~~~
  compilation terminated.
  make[2]: *** [CMakeFiles/wfa_tests.dir/build.make:79: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o] Error 1
  make[1]: *** [CMakeFiles/Makefile2:1081: CMakeFiles/wfa_tests.dir/all] Error 2
  make: *** [Makefile:101: all] Error 2
  ```

- Reproduction steps:
  1. Add new tests that include `wfa/native_spike.hpp`.
  2. Run `cmake --build build`.
  3. Observe the compiler failure because the native spike planner module does not exist yet.

- Environment:
  - Repository: `/home/astra/codex/wine-for-android`
  - Branch: `main`
  - Date: `2026-05-16`
  - Host: Codex desktop session with full filesystem and network access

- First hypothesis:
  The red test phase is correctly showing the missing implementation surface. Linuxoid needs a new `native_spike` header/source pair plus a `wfa_core` build entry before the tests can compile.
