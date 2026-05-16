# Problem: Recovery plan missing deterministic metadata

- Date: 2026-05-17
- Environment: `/home/astra/codex/wine-for-android` on Linux, CMake build directory `build`

## Exact error

```text
Test project /home/astra/codex/wine-for-android/build
    Start 1: wfa_tests
1/1 Test #1: wfa_tests ........................***Failed    3.57 sec
[asset-stub] manager created for: /tmp/base.apk using asset root /tmp/linuxoid-asset-read-test/resources/assets
[asset-stub] manager created for: /tmp/linuxoid-asset-zip-list-test/fixture.apk using asset root
Test failure: expected deterministic action rank in recovery plan
```

## Reproduction steps

1. Open the repo at `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe `wfa_tests` fail in the recovery-plan assertions.

## First hypothesis

The runtime recovery JSON currently exposes stable action names, reasons, and artifact paths, but it does not yet serialize explicit deterministic metadata such as action rank, retry budget, or recovery scope. The new tests are correctly failing because the self-healing contract is still underspecified for harness-driven replay and ordering.
