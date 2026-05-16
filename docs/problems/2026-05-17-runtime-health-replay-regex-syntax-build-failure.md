# Runtime Health Replay Regex Syntax Build Failure

## Exact error

```
/home/astra/codex/wine-for-android/src/runtime_health.cpp:330:69: error: missing terminating " character
/home/astra/codex/wine-for-android/src/runtime_health.cpp:331:56: error: missing terminating " character
/home/astra/codex/wine-for-android/src/runtime_health.cpp:332:63: error: missing terminating " character
```

## Reproduction steps

1. Open `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe the build fail while compiling `src/runtime_health.cpp`.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Date: `2026-05-17`
- Toolchain: `CMake 4.0`, `C++20`

## First hypothesis

The replay parser used raw-string regex literals whose delimiters collide with the embedded `)"` sequence, so the compiler sees unterminated string literals before the regexes can compile.
