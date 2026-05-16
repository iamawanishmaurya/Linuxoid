# Problem: outdated-phase-progress-expectation-after-backend-status-refresh

- Timestamp: 2026-05-16T14:39:29+05:30
- Environment: Codex desktop app, repository `/home/astra/codex/wine-for-android`, branch `main`, full-access mode, network enabled

## Exact Error

```text
[ 71%] Built target wfa_core
[ 78%] Linking CXX executable compatctl
[ 85%] Built target compatctl
[ 92%] Linking CXX executable wfa_tests
[100%] Built target wfa_tests
Test project /home/astra/codex/wine-for-android/build
    Start 1: wfa_tests
Errors while running CTest
1/1 Test #1: wfa_tests ........................***Failed    0.12 sec
Test failure: expected average phase progress to equal 90


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.12 sec

The following tests FAILED:
	  1 - wfa_tests (Failed)
```

## Reproduction Steps

1. Update the status model and project messaging for the backend-neutral installed-package seam.
2. Rebuild with `cmake --build build`.
3. Run `ctest --test-dir build --output-on-failure`.
4. Observe that the suite still expects the old phase average of `90`.

## First Hypothesis

The test expectation was not refreshed after the phase values changed from `80/74/86` to `84/78/86`, which raises the rounded average phase loading to `91`.
