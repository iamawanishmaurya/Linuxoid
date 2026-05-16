# Solution: attached-adb-launch-contract-test-fixture-failure

Related problem: [/home/astra/codex/wine-for-android/docs/problems/2026-05-16-attached-adb-launch-contract-test-fixture-failure.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-attached-adb-launch-contract-test-fixture-failure.md)

## What Failed

The new attached-ADB installed-app contract test failed even though the implementation was consistent with the existing launch parser. The fake `am start -W` output in the test fixture omitted part of the success pattern the shared parser requires.

## What Worked

I updated the test fixture to return a more realistic launch transcript containing:

- `Status: ok`
- the launched component
- `Complete`

After that, the rebuilt test suite passed.

## Why It Worked

`LaunchOutputLooksSuccessful` checks for both `Status: ok` and `Complete`, and `LaunchOutputConfirmsComponent` also expects the component to appear in the output. The earlier fixture under-modeled the contract, so the test was failing the parser on incomplete evidence rather than finding a real behavior bug.

## Commands Run

- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
