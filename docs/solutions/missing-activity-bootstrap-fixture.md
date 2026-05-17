# Solution: Missing activity bootstrap fixture seam

Linked problem: [2026-05-18-missing-activity-bootstrap-fixture.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-18-missing-activity-bootstrap-fixture.md)

## What failed

The new regression tests expected Linuxoid to expose a post-class-resolution activity bootstrap seam, but the build stopped at link time because the declared fixture API was not implemented or wired into the build:

- `RunNativeArtActivityBootstrapFixture(...)`
- `RenderNativeArtActivityBootstrapFixtureJson(...)`

The CLI also had no way to exercise that seam directly.

## What worked

Linuxoid now has a real `native-art-activity-bootstrap-fixture` command and implementation that:

- reuses the existing class-resolution and runtime-smoke artifacts
- derives a deterministic launcher activity target from the bootstrap manifest
- records Binder-readiness as an explicit dependency
- writes stable artifacts:
  - `art/activity-bootstrap-plan.json`
  - `art/activity-bootstrap-trace.jsonl`
  - `art/activity-bootstrap-result.json`
- refuses false success when host ART is absent

The tests now prove the seam writes stable artifacts, keeps launcher-derived targeting deterministic, and stays honest when runtime dependencies are still missing.

## Why it worked

The existing Linuxoid runtime already had the right pieces:

- launcher metadata in the bootstrap manifest
- Binder/service readiness in the lifecycle shim
- classpath plus target resolution in the ART fixtures
- host-runtime probing in the runtime smoke seam

The missing step was a small orchestration layer that turned those pieces into one deterministic post-class-resolution bootstrap contract instead of leaving the next stage implicit.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl bootstrap-native-spike /tmp/linuxoid-native-calculator.apk /tmp/linuxoid-native-compat /tmp/linuxoid-native-spike
./build/compatctl native-art-activity-bootstrap-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl status
```
