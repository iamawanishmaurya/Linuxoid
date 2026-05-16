# Problem: Post-hoc package mismatch guard

- Date: 2026-05-16
- Slug: posthoc-package-mismatch-guard

## Exact Error

```text
Test project /home/astra/codex/wine-for-android/build
    Start 1: wfa_tests
1/1 Test #1: wfa_tests ........................***Failed    0.04 sec
Test failure: expected wrong apk package pairing to fail before any adb mutation


0% tests passed, 1 tests failed out of 1
```

## Reproduction Steps

1. Update `tests/test_main.cpp` so the wrong-APK pairing case requires zero ADB mutations.
2. Run `cmake --build build`.
3. Run `ctest --test-dir build --output-on-failure`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Toolchain: `cmake`, `g++`, local `build/` tree
- Date: 2026-05-16

## First Hypothesis

The provisioning guard recognizes the APK/package mismatch, but only after it has already performed the install and IME mutation commands.
