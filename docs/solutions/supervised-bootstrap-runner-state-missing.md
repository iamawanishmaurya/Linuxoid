# Supervised bootstrap runner state missing

## What failed

The bootstrap-execution tests expected a stable runner-state artifact and raw phase exit codes, but the execution fixture still bypassed the generated runner script and exposed only plan, trace, and result files.

## What worked

We upgraded `src/art_bootstrap_execution_fixture.cpp` so Linuxoid now:

- executes bootstrap phases through the generated `bootstrap-execution-runner.sh`
- writes `bootstrap-execution-runner-state.json`
- records `runner_invoked`, `runner_exit_code`, `application_exit_code`, and `activity_exit_code`
- keeps per-phase logs and evaluates success from the captured exit codes plus outputs

## Why it worked

The execution seam now has a real supervised runner path instead of an implied one. That gives Linuxoid a deterministic boundary between planning and execution, and it gives the self-healing runtime better evidence for replay and recovery.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-art-bootstrap-execution-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
./build/compatctl native-runtime-diagnostic-replay /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
```

## Linked problem

- [Supervised bootstrap runner state missing](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-supervised-bootstrap-runner-state-missing.md)
