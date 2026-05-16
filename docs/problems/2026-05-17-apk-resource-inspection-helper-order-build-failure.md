# APK Resource Inspection Helper Order Build Failure

## Exact error

```
/home/astra/codex/wine-for-android/src/apk_loader.cpp:156:29: error: ‘ExtractFirstMatch’ was not declared in this scope
/home/astra/codex/wine-for-android/src/apk_loader.cpp:167:10: error: ‘ExtractFirstMatch’ was not declared in this scope
```

## Reproduction steps

1. Open `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe the build stop while compiling `src/apk_loader.cpp`.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Date: `2026-05-17`
- Toolchain: `CMake 4.0`, `C++20`

## First hypothesis

The new manifest helper functions were inserted above the existing `ExtractFirstMatch` helper inside the anonymous namespace, so the compiler sees calls to `ExtractFirstMatch` before any declaration is available.
