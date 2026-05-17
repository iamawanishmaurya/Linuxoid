# Problem: bootstrap execution runner artifacts missing

## Exact error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp:2506:28: error: ‘const struct wfa::NativeArtBootstrapExecutionFixtureReport’ has no member named ‘execution_context_json_path’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2508:28: error: ‘const struct wfa::NativeArtBootstrapExecutionFixtureReport’ has no member named ‘runner_script_path’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2510:28: error: ‘const struct wfa::NativeArtBootstrapExecutionFixtureReport’ has no member named ‘application_execution_log_path’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2512:28: error: ‘const struct wfa::NativeArtBootstrapExecutionFixtureReport’ has no member named ‘activity_execution_log_path’
```

## Reproduction steps

```bash
cd /home/astra/codex/wine-for-android
cmake --build build && ctest --test-dir build --output-on-failure
```

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Date: `2026-05-17`
- Shell: `zsh`
- Current branch: `main`

## First hypothesis

The new tests are correctly asking for a deeper bootstrap-execution contract, but the execution fixture report still only mirrors the lower-level activity report. Linuxoid needs explicit runner/session artifacts in the bootstrap-execution seam before the planning/execution split is real.
