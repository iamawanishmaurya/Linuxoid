# Problem: status-heading-test-stale-build

## Exact Error

```text
Test project /home/astra/codex/wine-for-android/build
    Start 1: wfa_tests
1/1 Test #1: wfa_tests ........................***Failed    0.02 sec
Test failure: expected phase loading heading
```

## Reproduction Steps

1. Update `src/project_status.cpp` so the status report heading changes from `Phase Loading` to `Scaffold Readiness` and `Native Execution Readiness`.
2. Run `ctest --test-dir build --output-on-failure` without rebuilding the test binary first.
3. Observe the stale compiled test still asserting the old heading.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Date: `2026-05-16`
- Host: Codex desktop local workspace
- Build system: CMake + CTest

## First Hypothesis

The test source was updated, but `wfa_tests` was not rebuilt before rerunning `ctest`, so the old binary still contains the old assertion text.
