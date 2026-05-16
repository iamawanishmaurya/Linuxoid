# APK Resource Readiness Test Missing `<algorithm>` Include

## Exact error

```
/home/astra/codex/wine-for-android/tests/test_main.cpp:1184:19: error: no matching function for call to ‘find(...)’
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

The new missing-manifest test uses `std::find` over `std::vector<std::string>`, but `tests/test_main.cpp` does not include `<algorithm>`, so only unrelated overloads are visible.
