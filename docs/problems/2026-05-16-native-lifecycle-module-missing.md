# Native Lifecycle Module Missing

## Exact Error

The first red-bar build for the lifecycle and service shim slice failed during CMake generation:

```text
-- Configuring done (0.0s)
CMake Error at CMakeLists.txt:15 (add_library):
  Cannot find source file:

    src/native_lifecycle.cpp


CMake Error at CMakeLists.txt:15 (add_library):
  No SOURCES given to target: wfa_core


CMake Generate step failed.  Build files cannot be regenerated correctly.
make: *** [Makefile:915: cmake_check_build_system] Error 1
```

## Reproduction Steps

1. Add the native lifecycle header, tests, and CMake entry.
2. Run:
   - `cmake --build build`

## Environment

- Project: Linuxoid
- Repository: `/home/astra/codex/wine-for-android`
- Host OS: Linux
- Build tool: CMake-generated Makefiles

## First Hypothesis

The tests and build wiring are correctly pointing at a module that does not exist yet. Linuxoid needs a concrete `src/native_lifecycle.cpp` implementation for the lifecycle and service shim surface.
