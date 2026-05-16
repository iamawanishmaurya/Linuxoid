# Runtime diagnostic replay surfaces missing

Problem: [docs/problems/2026-05-17-runtime-diagnostic-replay-surfaces-missing.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-runtime-diagnostic-replay-surfaces-missing.md)

## What failed

The new replay tests failed because the production runtime did not yet expose:

- a `trace_jsonl_path` on `NativeArtRuntimeSmokeReport`
- a JSONL trace artifact for the ART runtime smoke seam
- a `ReplayRuntimeDiagnosticBundle(...)` API or CLI command

That meant Linuxoid could classify failures and materialize recovery plans, but it still could not merge the existing traces back into one replayable diagnosis bundle.

## What worked

We added the smallest real surfaces needed:

1. extended `native-art-runtime-smoke` to write `art/runtime-smoke-trace.jsonl`
2. added `ReplayRuntimeDiagnosticBundle(...)` plus JSON rendering in `runtime_health.cpp`
3. added `compatctl native-runtime-diagnostic-replay <bootstrap-manifest>`
4. merged these trace sources deterministically:
   - `runtime-health-trace.jsonl`
   - `runtime-recovery-actions.jsonl`
   - `art-classloader-trace.jsonl`
   - `art-class-resolution-trace.jsonl`
   - `runtime-smoke-trace.jsonl`
5. wrote stable replay artifacts:
   - `health/runtime-diagnostic-events.jsonl`
   - `health/runtime-diagnostic-replay.json`

## Why it worked

The replay bundle does not rerun the UI path. It only reads the already-produced trace artifacts under the session root, normalizes them into one merged JSONL stream, and reports missing trace sources honestly. That gives Linuxoid a deterministic diagnosis surface for harnesses and MCP clients without pretending that rerunning runtime setup is the same thing as replay.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
./build/compatctl native-runtime-diagnostic-replay /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl status
```
