## Exact Error

Command:

```bash
cmake --build build
```

Observed output:

```text
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestNativeInputQueueFixtureWritesStableArtifacts()':
test_main.cpp:(.text+0x12c5f): undefined reference to `wfa::RunNativeInputQueueFixture(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, wfa::NativeWindowMetadata const&)'
/usr/bin/ld: test_main.cpp:(.text+0x135a4): undefined reference to `wfa::RenderNativeInputQueueFixtureJson[abi:cxx11](wfa::NativeInputQueueFixtureReport const&)'
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestNativeInputQueueFixtureReportsFallbackHonestly()':
test_main.cpp:(.text+0x13c7f): undefined reference to `wfa::RunNativeInputQueueFixture(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, wfa::NativeWindowMetadata const&)'
collect2: error: ld returned 1 exit status
```

## Reproduction Steps

1. Add tests that call `wfa::RunNativeInputQueueFixture(...)` and `wfa::RenderNativeInputQueueFixtureJson(...)` from `tests/test_main.cpp`.
2. Run:

```bash
cmake --build build
```

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`
- Toolchain: `C++20` via current CMake project

## First Hypothesis

The new header `/home/astra/codex/wine-for-android/include/wfa/native_input_queue_fixture.hpp` declares the input queue fixture surface, but no corresponding `.cpp` implementation or CMake target wiring exists yet, so the new test contract cannot link.
