# Problem: Runtime health report missing self-healing summary fields

- Date: 2026-05-18
- Environment: `/home/astra/codex/wine-for-android` on Linux, CMake build directory `build`

## Exact error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp:2472:17: error: ‘const struct wfa::RuntimeHealthReport’ has no member named ‘dependency_blocked’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2474:17: error: ‘const struct wfa::RuntimeHealthReport’ has no member named ‘failing_subsystem_count’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2476:17: error: ‘const struct wfa::RuntimeHealthReport’ has no member named ‘recovery_actions_selected’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2478:17: error: ‘const struct wfa::RuntimeHealthReport’ has no member named ‘failing_subsystems’
```

## Reproduction steps

1. Open the repo at `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe the compile failure in `tests/test_main.cpp`.

## First hypothesis

Linuxoid already writes the raw health records and recovery actions, but the report does not yet expose an explicit self-healing summary. The new regression tests are correctly failing because harnesses still have to derive dependency blockage and failing-subsystem counts by scanning the raw records themselves.
