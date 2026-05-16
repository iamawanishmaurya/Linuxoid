# Solution: Floating rounding drift in checkpoint progress

Related problem: [2026-05-16-floating-rounding-drift-in-checkpoint-progress.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-floating-rounding-drift-in-checkpoint-progress.md)

## What Failed

The weighted checkpoint progress calculation used floating-point arithmetic, and the exact `57.5/100` case drifted just enough to render `57/100` instead of `58/100`.

## What Worked

The checkpoint aggregation was switched to integer math:

- accumulate `weight * percent` as integers
- round using integer arithmetic at the final division step

## Why It Worked

The progress model only needs integer percentages, so integer math is both simpler and exact for the current checkpoint scheme. It removes rounding drift and keeps the code-generated status output aligned with the test expectations.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/compatctl status
```
