# Solution: Runtime health subsystem count expectations were stale after activity bootstrap integration

Linked problem: [2026-05-17-runtime-health-subsystem-count-stale.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-runtime-health-subsystem-count-stale.md)

## What failed

After Linuxoid started treating activity bootstrap as a first-class health subsystem, older tests still expected the pre-integration counts for:

- subsystem records
- failing subsystem totals
- selected recovery-action totals
- replay-observed subsystem counts

## What worked

The regression suite now expects the expanded runtime contract and explicitly checks that:

- `activity_bootstrap_readiness` is present
- `attempt_host_activity_bootstrap` is selected deterministically
- replay includes `art_activity_bootstrap_trace`
- failing subsystem order stays deterministic after the contract expansion

## Why it worked

The underlying runtime behavior was already correct. The remaining failure was in the assertions, not the implementation. Updating the tests to match the new contract restored the suite as a useful guardrail instead of an outdated snapshot.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
