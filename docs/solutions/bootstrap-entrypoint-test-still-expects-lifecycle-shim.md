# Solution: bootstrap-entrypoint-test-still-expects-lifecycle-shim

Problem reference: [2026-05-16-bootstrap-entrypoint-test-still-expects-lifecycle-shim.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-bootstrap-entrypoint-test-still-expects-lifecycle-shim.md)

## What Failed

The bootstrap artifact test still asserted that `launch-native-activity.sh` should execute `native-lifecycle-shim`, even though the `P1` slice intentionally moved the generated entrypoint to `native-execute-stub`.

## What Worked

Updated the test to assert the new contract instead:

- `native-execute-stub` appears in the entrypoint script
- the package name appears in the command line
- the launcher component appears in the command line
- the bootstrap manifest path still appears in the command line

## Why It Worked

The failure was not in Linuxoid’s new runtime flow; it was in the stale test expectation. Updating the test let the suite validate the intended `P1` contract instead of the older pre-runner bootstrap flow.

## Commands Run

```bash
ctest --test-dir build --output-on-failure
cmake --build build
ctest --test-dir build --output-on-failure
```
