# Solution: Runtime health report missing self-healing summary fields

Linked problem: [2026-05-18-runtime-health-missing-summary-fields.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-18-runtime-health-missing-summary-fields.md)

## What failed

The new regression tests expected Linuxoid's runtime-health report to expose an explicit self-healing summary instead of forcing callers to scan the raw subsystem records. The build failed because `RuntimeHealthReport` did not yet contain:

- `dependency_blocked`
- `failing_subsystem_count`
- `recovery_actions_selected`
- `failing_subsystems`

## What worked

Linuxoid now computes those summary fields directly while building the runtime-health report and serializes them into:

- `runtime-health.json`
- `native-runtime-health-fixture` CLI output
- `runtime-recovery-plan.json`

The new tests also prove that the command JSON stays stable across repeated runs against the same bootstrap fixture.

## Why it worked

The self-healing runtime already had the raw information, but it was still implicit. By making the summary explicit, the runtime now gives harnesses a stable answer to the questions they actually need to ask first:

- Is execution blocked on a real dependency?
- How many subsystems are failing?
- Which subsystems are failing?
- How many bounded recovery actions were selected?

That keeps the contract smaller, clearer, and easier to validate without re-deriving state.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
./build/compatctl status
```
