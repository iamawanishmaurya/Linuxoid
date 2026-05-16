# Runtime recovery plan fields missing

Problem: [2026-05-17-runtime-recovery-plan-fields-missing.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-runtime-recovery-plan-fields-missing.md)

## What failed

The new recovery-plan tests compiled against fields that did not exist yet in `RuntimeHealthReport` and `RuntimeRecoveryAction`. Linuxoid could choose deterministic actions in memory, but it could not expose stable recovery-plan artifact paths or per-action artifact metadata.

## What worked

Linuxoid now extends the runtime-health contract with:

- `RuntimeHealthReport.recovery_plan_path`
- `RuntimeHealthReport.recovery_actions_jsonl_path`
- `RuntimeRecoveryAction.action_id`
- `RuntimeRecoveryAction.artifact_path`
- `RuntimeRecoveryAction.replay_trace_path`

It also now writes:

- `health/runtime-recovery-plan.json`
- `health/runtime-recovery-actions.jsonl`

And exposes a dedicated CLI surface:

```bash
./build/compatctl native-runtime-recovery-plan <bootstrap-manifest> [scenario]
```

## Why it worked

The missing piece was not the recovery decision logic itself; that already existed. The missing piece was a first-class deterministic artifact contract around those decisions so MCP and harness clients can inspect, replay, and diff recovery plans directly.

## Commands run

```bash
cd /home/astra/codex/wine-for-android
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-runtime-recovery-plan /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
./build/compatctl native-runtime-recovery-plan /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json missing_artifact
```
