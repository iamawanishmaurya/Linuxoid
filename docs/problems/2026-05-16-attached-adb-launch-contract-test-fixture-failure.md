# Problem: attached-adb-launch-contract-test-fixture-failure

- Timestamp: 2026-05-16T14:32:01+05:30
- Environment: Codex desktop app, repository `/home/astra/codex/wine-for-android`, branch `main`, full-access mode, network enabled

## Exact Error

```text
Test project /home/astra/codex/wine-for-android/build
    Start 1: wfa_tests
1/1 Test #1: wfa_tests ........................***Failed    0.21 sec
Test failure: expected attached-adb launch success


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.29 sec

The following tests FAILED:
	  1 - wfa_tests (Failed)
Errors while running CTest
```

## Reproduction Steps

1. Build the current backend-abstraction slice with `cmake --build build`.
2. Run `ctest --test-dir build --output-on-failure`.
3. Observe that `wfa_tests` fails in the new attached-ADB installed-app launch contract test.

## First Hypothesis

The new test fixture probably returns an incomplete fake `am start -W` output. The shared `LaunchOutputLooksSuccessful` helper requires both `Status: ok` and `Complete`, so the test likely needs to mirror the existing Android launch output contract more accurately.
