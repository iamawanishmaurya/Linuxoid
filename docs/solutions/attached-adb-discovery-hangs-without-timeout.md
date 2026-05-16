# Solution: attached-adb-discovery-hangs-without-timeout

Related problem: [/home/astra/codex/wine-for-android/docs/problems/2026-05-16-attached-adb-discovery-hangs-without-timeout.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-attached-adb-discovery-hangs-without-timeout.md)

## What Failed

The new attached-ADB runtime discovery and preflight commands could block indefinitely in the live environment because they assumed `adb devices` and related probe calls would always return promptly.

## What Worked

I wrapped the attached-ADB discovery and preflight probe calls with a 5-second timeout boundary and added explicit handling for timeout exit code `124`. Then I rebuilt the project, reran the test suite, and repeated the live smoke checks.

## Why It Worked

The feature does not need unbounded blocking behavior to be useful. A bounded probe is more honest: it either returns a target report quickly or fails fast with a clear not-ready signal. That keeps the generic backend path usable even when `adb` is slow or stuck.

## Commands Run

- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- `./build/compatctl discover-runtime attached-adb`
- `./build/compatctl preflight-runtime attached-adb`
- `./build/compatctl preflight-runtime native`
