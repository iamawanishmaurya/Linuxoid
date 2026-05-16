# Runtime Health Test ZIP Helper Order Build Failure

## Exact error

```
/home/astra/codex/wine-for-android/tests/test_main.cpp:119:3: error: ‘WriteStoredZipFixture’ was not declared in this scope
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

The new runtime-health bootstrap helper was inserted above the local ZIP-fixture writer helpers, so the compiler sees the call before any declaration is available.
