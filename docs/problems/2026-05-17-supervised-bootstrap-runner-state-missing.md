# Supervised bootstrap runner state missing

## Exact error

```
/home/astra/codex/wine-for-android/tests/test_main.cpp:2522:28: error: ‘const struct wfa::NativeArtBootstrapExecutionFixtureReport’ has no member named ‘runner_state_json_path’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2633:17: error: ‘const struct wfa::NativeArtBootstrapExecutionFixtureReport’ has no member named ‘runner_invoked’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2634:17: error: ‘const struct wfa::NativeArtBootstrapExecutionFixtureReport’ has no member named ‘runner_exit_code’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2635:17: error: ‘const struct wfa::NativeArtBootstrapExecutionFixtureReport’ has no member named ‘application_exit_code’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2637:17: error: ‘const struct wfa::NativeArtBootstrapExecutionFixtureReport’ has no member named ‘activity_exit_code’
```

## Reproduction steps

1. Open `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe the bootstrap-execution tests fail to compile.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`
- Toolchain: `CMake 4.x`, `C++20`

## First hypothesis

Linuxoid writes a runner script for bootstrap execution, but the execution report and implementation still bypass that script and do not persist a supervised runner-state artifact or raw per-phase exit codes.
