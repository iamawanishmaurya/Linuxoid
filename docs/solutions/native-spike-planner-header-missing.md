# Native Spike Planner Header Missing

Related problem: [2026-05-16-native-spike-planner-header-missing.md](../problems/2026-05-16-native-spike-planner-header-missing.md)

## What Failed

The first red-bar build for the native spike slice failed because the new test coverage referenced `wfa/native_spike.hpp` before the native spike module existed.

## What Worked

Implemented the missing native spike surface end to end:

- Added `include/wfa/native_spike.hpp`
- Added `src/native_spike.cpp`
- Wired the source into `CMakeLists.txt`
- Added the `plan-native-spike` CLI command in `src/main.cpp`
- Added unit coverage in `tests/test_main.cpp`

## Why It Worked

The tests were accurate: they were exercising a real missing module. Once the header, implementation, and build wiring existed, both the CLI and the test binary could link against the new planner surface.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
