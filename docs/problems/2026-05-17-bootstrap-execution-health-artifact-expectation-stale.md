# Bootstrap Execution Health Artifact Expectation Stale

- Exact error:
  - `Test failure: expected activity bootstrap result artifact path`
- Reproduction steps:
  1. `cd /home/astra/codex/wine-for-android`
  2. `cmake --build build && ctest --test-dir build --output-on-failure`
- Environment:
  - Host: Codex desktop local workspace
  - Date: 2026-05-17
  - OS/runtime: Linux, C++20 build
- First hypothesis:
  - `RunRuntimeHealthFixture(...)` now points `activity_bootstrap_readiness` at the higher-level bootstrap-execution artifact, but the older regression still expects the lower-level `activity-bootstrap-result.json` path. The test suite should be updated to the new contract.
