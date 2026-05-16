# Solution: status-heading-test-stale-build

Problem reference: [2026-05-16-status-heading-test-stale-build.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-status-heading-test-stale-build.md)

## What Failed

Linuxoid changed the status-report heading from `Phase Loading` to `Scaffold Readiness` and added `Native Execution Readiness`, but the test runner was executed before the rebuilt `wfa_tests` binary picked up the updated assertion.

## Options Evaluated

1. Rebuild the workspace, then rerun `ctest`.
   - Pros: preserves the new truthful status wording and aligns the binary with the source tree.
   - Cons: none beyond the normal rebuild cost.

2. Revert the status heading back to `Phase Loading`.
   - Pros: the existing test would pass immediately.
   - Cons: it would undo the clearer scaffold-vs-execution distinction the phased plan needs.

3. Make the test accept either heading string forever.
   - Pros: tolerant to future wording shifts.
   - Cons: weakens the regression signal and keeps the status contract vague.

## What Worked

Option 1 is the best fit. After updating the test source, rebuild the project and rerun `ctest`.

## Why It Worked

`ctest` runs the last compiled `wfa_tests` binary in `build/`. Rebuilding recompiles the modified test source so the executable matches the new status output contract.

## Commands Run

```bash
ctest --test-dir build --output-on-failure
cmake --build build
ctest --test-dir build --output-on-failure
```
