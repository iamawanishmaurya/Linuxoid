# Bootstrap execution health subsystem missing

## What failed

Runtime health still emitted only seven subsystem records and folded the new bootstrap-execution seam into `activity_bootstrap_readiness`.

## What worked

We split the health model in `src/runtime_health.cpp` so Linuxoid now emits:

- `activity_bootstrap_readiness` for the post-class-resolution planning seam
- `bootstrap_execution_readiness` for the execution seam

We also added the deterministic recovery action `attempt_host_bootstrap_execution` and updated the replay/regression suite to assert the new subsystem ordering, counts, and recovery output.

## Why it worked

The runtime model now matches the actual runtime surfaces Linuxoid exposes:

- planning artifacts live under `art/activity-bootstrap-*`
- execution artifacts live under `art/bootstrap-execution-*`

That lets health, recovery, and replay point at the correct seam instead of treating a ready plan as if it were a failed execution.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
./build/compatctl native-runtime-diagnostic-replay /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
```

## Linked problem

- [Bootstrap execution health subsystem missing](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-bootstrap-execution-health-subsystem-missing.md)
