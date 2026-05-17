# Solution: Activity bootstrap seam missing from runtime health and replay

Linked problem: [2026-05-17-activity-bootstrap-health-record-missing.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-activity-bootstrap-health-record-missing.md)

## What failed

Linuxoid already wrote deterministic activity-bootstrap artifacts, but the self-healing runtime still stopped at classloader/runtime-smoke. That meant:

- no `activity_bootstrap_readiness` record in `runtime-health.json`
- no deterministic recovery action for the activity-bootstrap seam
- no activity-bootstrap trace source inside the merged diagnostic replay bundle

## What worked

Linuxoid now:

- records `activity_bootstrap_readiness` in runtime health
- selects the deterministic recovery action `attempt_host_activity_bootstrap`
- merges `art/activity-bootstrap-trace.jsonl` into diagnostic replay
- keeps the same replayable artifact contract while extending it one seam deeper

## Why it worked

The activity-bootstrap fixture already had the right evidence:

- manifest-derived launcher targeting
- Binder-readiness
- runtime-smoke readiness
- deterministic trace and result artifacts

The missing piece was to thread that evidence into the existing health and replay pipeline, instead of treating it as a side surface.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
