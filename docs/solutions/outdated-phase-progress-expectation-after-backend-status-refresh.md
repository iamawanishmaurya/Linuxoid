# Solution: outdated-phase-progress-expectation-after-backend-status-refresh

Related problem: [/home/astra/codex/wine-for-android/docs/problems/2026-05-16-outdated-phase-progress-expectation-after-backend-status-refresh.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-outdated-phase-progress-expectation-after-backend-status-refresh.md)

## What Failed

After the status model was updated to reflect the backend-neutral installed-package seam, the test suite still expected the old average phase loading of `90/100`.

## What Worked

I refreshed the stale assertions in `tests/test_main.cpp` from `90` to the new rounded value of `91`, then rebuilt and reran the full test suite.

## Why It Worked

The implementation change was intentional: `P4` and `P5` increased to reflect the new generic installed-package seam, which raises the rounded average across the six phases. The failure was in the outdated expectation, not in the status calculation.

## Commands Run

- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
