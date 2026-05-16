# Solution: Recovery plan missing deterministic metadata

Linked problem: [2026-05-17-recovery-plan-missing-deterministic-metadata.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-recovery-plan-missing-deterministic-metadata.md)

## What failed

The recovery-plan tests started asserting that Linuxoid's self-healing artifacts should expose explicit deterministic metadata for each recovery action. The runtime still emitted only stable action names, reasons, and artifact paths, so `wfa_tests` failed with:

```text
Test failure: expected deterministic action rank in recovery plan
```

## What worked

Linuxoid now builds each recovery action from a deterministic template keyed by subsystem. That template supplies:

- `action_rank`
- `retry_budget`
- `recovery_scope`

Those fields are now serialized into:

- `runtime-recovery-plan.json`
- `runtime-recovery-actions.jsonl`
- `runtime-health.json`

The runtime also sorts selected recovery actions by `action_rank`, then subsystem name, then action name, so the output order is stable for harnesses and replay tools.

## Why it worked

The failing tests were correct: a harness cannot safely replay or compare recovery policy if the runtime only exposes action names and free-form reasons. By turning recovery policy into a deterministic template and serializing explicit ranking and bounded retry metadata, Linuxoid now exposes the same machine-facing decision every run.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
cmake --build build && ctest --test-dir build --output-on-failure
```
