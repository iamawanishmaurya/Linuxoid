# Solution: Diagnostic replay missing trace-index surface

Linked problem: [2026-05-18-diagnostic-replay-missing-trace-index-surface.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-18-diagnostic-replay-missing-trace-index-surface.md)

## What failed

The new replay-fixture tests expected Linuxoid's diagnostic replay report to expose a deterministic trace-index artifact and a dedicated non-UI fixture command. The build failed because `RuntimeDiagnosticReplayReport` did not yet have `trace_index_json_path`, and the CLI did not yet have a fixture path that materialized traces before replay.

## What worked

Linuxoid now:

- writes `runtime-diagnostic-trace-index.json`
- records per-source:
  - `source_fingerprint`
  - `first_event_type`
  - `last_event_type`
- exposes that index path through `RuntimeDiagnosticReplayReport`
- supports `native-runtime-diagnostic-fixture <bootstrap-manifest> [scenario]`

The fixture command runs the health fixture first, then emits the replay bundle and trace index without relying on a full UI rerun.

## Why it worked

The replay bundle was already valuable, but it still made offline comparisons awkward because there was no stable source index or fingerprint summary. By adding a trace index and making the fixture generation explicit in the CLI, Linuxoid now gives harnesses a deterministic way to compare diagnostic bundles across runs and scenarios.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-runtime-diagnostic-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
./build/compatctl status
```
