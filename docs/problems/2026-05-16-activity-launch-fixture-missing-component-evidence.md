# Problem: Activity launch fixture missing component evidence

- Date: 2026-05-16
- Slug: activity-launch-fixture-missing-component-evidence

## Exact Error

```text
Test project /home/astra/codex/wine-for-android/build
    Start 1: wfa_tests
1/1 Test #1: wfa_tests ........................***Failed    0.01 sec
Test failure: expected successful activity launch


0% tests passed, 1 tests failed out of 1
```

## Reproduction Steps

1. Implement the new explicit-component `launch-activity` bridge.
2. Run `cmake --build build`.
3. Run `ctest --test-dir build --output-on-failure`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Toolchain: `cmake`, `g++`, local `build/` tree
- Date: 2026-05-16

## First Hypothesis

The new bridge now requires the launch output to confirm the requested explicit component, but the unit-test fixture only returned generic `Status: ok` and `Complete` lines.
