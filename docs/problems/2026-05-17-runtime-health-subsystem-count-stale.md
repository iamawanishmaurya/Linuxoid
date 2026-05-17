# Problem: Runtime health subsystem count expectations are stale after activity bootstrap integration

- Date: 2026-05-17
- Environment: `/home/astra/codex/wine-for-android` on Linux, CMake build directory `build`

## Exact error

```text
Test failure: expected six subsystem health records
```

## Reproduction steps

1. Open the repo at `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe `wfa_tests` fail after the new `activity_bootstrap_readiness` record is added to runtime health.

## First hypothesis

Linuxoid runtime health now correctly includes activity bootstrap as a first-class subsystem, but older regression tests still assume the pre-integration count and recovery totals. The code is ahead of the assertions.
