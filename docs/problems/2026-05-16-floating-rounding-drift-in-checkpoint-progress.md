# Problem: Floating rounding drift in checkpoint progress

- Date: 2026-05-16
- Slug: floating-rounding-drift-in-checkpoint-progress

## Exact Error

```text
Errors while running CTest
Test project /home/astra/codex/wine-for-android/build
    Start 1: wfa_tests
1/1 Test #1: wfa_tests ........................***Failed    0.03 sec
Test failure: expected weighted checkpoint progress to round to 58


0% tests passed, 1 tests failed out of 1
```

## Reproduction Steps

1. Update the project progress model so `C4 Host Integration` moves to `50/100`.
2. Run `cmake --build build`.
3. Run `ctest --test-dir build --output-on-failure`.
4. Compare the test expectation to `./build/compatctl status`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Toolchain: `cmake`, `g++`, local `build/` tree
- Date: 2026-05-16

## First Hypothesis

The weighted checkpoint calculation uses floating-point arithmetic and is drifting just enough to render `57/100` instead of the expected `58/100` for a mathematically exact `57.5/100` result.
