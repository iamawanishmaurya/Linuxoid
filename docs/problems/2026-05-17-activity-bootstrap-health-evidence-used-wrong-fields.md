# Activity bootstrap health evidence used wrong fields

## Exact error

```
/home/astra/codex/wine-for-android/src/runtime_health.cpp:416:33: error: ‘const struct wfa::NativeArtActivityBootstrapFixtureReport’ has no member named ‘application_execution_attempted’; did you mean ‘application_probe_attempted’?
/home/astra/codex/wine-for-android/src/runtime_health.cpp:418:33: error: ‘const struct wfa::NativeArtActivityBootstrapFixtureReport’ has no member named ‘application_execution_succeeded’; did you mean ‘application_probe_succeeded’?
```

## Reproduction steps

1. Open `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe compilation fail in `src/runtime_health.cpp` while building `wfa_core`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`
- Toolchain: `CMake 4.x`, `C++20`

## First hypothesis

`BuildActivityBootstrapRecord()` was refactored to model the planning seam, but its evidence string still referenced bootstrap-execution field names instead of the activity-bootstrap probe fields defined in `NativeArtActivityBootstrapFixtureReport`.
