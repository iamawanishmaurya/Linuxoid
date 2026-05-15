# Problem: missing-cpp-scaffold-files

- Date: 2026-05-16
- Exact error:

```text
CMake Error at CMakeLists.txt:15 (add_library):
  Cannot find source file:

    src/checkpoint.cpp


CMake Error at CMakeLists.txt:24 (add_executable):
  Cannot find source file:

    src/main.cpp


CMake Error at CMakeLists.txt:15 (add_library):
  No SOURCES given to target: wfa_core


CMake Error at CMakeLists.txt:24 (add_executable):
  No SOURCES given to target: compatctl


CMake Generate step failed.  Build files cannot be regenerated correctly.
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Add `CMakeLists.txt` and `tests/test_main.cpp` that reference scaffold sources.
  3. Run `cmake -S . -B build`.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The failing red step is correct: the TDD scaffold references production files that have not been created yet, so the next task is to implement the smallest C++ source and header set that satisfies the tests.
