Links back to: [2026-05-17-binder-service-manager-status-test-stale-expectation.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-binder-service-manager-status-test-stale-expectation.md)

## What Failed

After raising `P4` progress to reflect the new Binder-shaped service-manager slice, `wfa_tests` still expected the older average scaffold progress value and failed with:

```text
expected average phase progress to equal 95
```

## What Worked

Update the status expectations in `/home/astra/codex/wine-for-android/tests/test_main.cpp` from `95` to `96`, matching the current average from:

- `P1 100`
- `P2 100`
- `P3 100`
- `P4 97`
- `P5 84`
- `P6 93`

## Why It Worked

The regression was not in the runtime slice. It was a stale test expectation caused by an intentional status-model change. Bringing the test back in sync with the code restored the verification gate without weakening the contract.

## Commands Run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl status
```
