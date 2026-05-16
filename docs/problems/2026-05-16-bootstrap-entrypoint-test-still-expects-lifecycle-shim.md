# Problem: bootstrap-entrypoint-test-still-expects-lifecycle-shim

## Exact Error

```text
Test failure: expected native lifecycle shim in entrypoint script
```

## Reproduction Steps

1. Change `BuildNativeActivityBootstrap` so the generated `launch-native-activity.sh` calls `compatctl native-execute-stub ...` instead of `compatctl native-lifecycle-shim ...`.
2. Run `ctest --test-dir build --output-on-failure`.
3. Observe the bootstrap test still asserting the old entrypoint contract.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Date: `2026-05-16`
- Build: local CTest run

## First Hypothesis

The test suite still reflects the old bootstrap-to-lifecycle handoff contract, but `P1` intentionally moved the generated entrypoint to the native runner.
