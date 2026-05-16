## Exact Error

Command:

```bash
cmake --build build
```

Observed output:

```text
/usr/bin/ld: CMakeFiles/wfa_tests.dir/tests/test_main.cpp.o: in function `(anonymous namespace)::TestBinderServiceManagerFixtureWritesStableArtifacts()':
test_main.cpp:(.text+0x14155): undefined reference to `wfa::RunBinderServiceManagerFixture(wfa::BinderServiceManagerSpec const&)'
/usr/bin/ld: test_main.cpp:(.text+0x14f7d): undefined reference to `wfa::RenderBinderServiceManagerFixtureJson[abi:cxx11](wfa::BinderServiceManagerFixtureReport const&)'
collect2: error: ld returned 1 exit status
```

## Reproduction Steps

1. Add tests that call `wfa::RunBinderServiceManagerFixture(...)` and `wfa::RenderBinderServiceManagerFixtureJson(...)` from `tests/test_main.cpp`.
2. Run:

```bash
cmake --build build
```

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`
- Toolchain: `C++20` via the current CMake project

## First Hypothesis

The new header `/home/astra/codex/wine-for-android/include/wfa/binder_service_manager.hpp` declares a Binder-shaped local service manager contract, but Linuxoid has no corresponding implementation file or target wiring yet, so the new fixture tests cannot link.
