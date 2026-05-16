# Runtime recovery plan fields missing

## Exact error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp:2445:28: error: ‘const struct wfa::RuntimeHealthReport’ has no member named ‘recovery_plan_path’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2447:28: error: ‘const struct wfa::RuntimeHealthReport’ has no member named ‘recovery_actions_jsonl_path’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2461:23: error: ‘const struct wfa::RuntimeRecoveryAction’ has no member named ‘artifact_path’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2463:23: error: ‘const struct wfa::RuntimeRecoveryAction’ has no member named ‘replay_trace_path’
```

## Reproduction steps

1. `cd /home/astra/codex/wine-for-android`
2. Add runtime recovery-plan tests to `tests/test_main.cpp`
3. Run `cmake --build build && ctest --test-dir build --output-on-failure`

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`
- Toolchain: CMake + C++20 local build

## First hypothesis

Linuxoid already selects deterministic recovery actions inside `runtime_health`, but it does not yet materialize them as first-class recovery-plan artifacts or expose per-action artifact metadata. The next fix is to extend the runtime-health types, emit stable recovery-plan files, and add a CLI command that returns the recovery plan explicitly.
