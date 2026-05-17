# Runtime Health Bootstrap Execution Duplicates Work

- Exact error:
  - `timeout 20 ./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline`
  - Result: `TIMEOUT:124`
- Reproduction steps:
  1. `cd /home/astra/codex/wine-for-android`
  2. `timeout 20 ./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline`
- Environment:
  - Host: Codex desktop local workspace
  - Date: 2026-05-17
  - OS/runtime: Linux, C++20 build
- First hypothesis:
  - `RunRuntimeHealthFixture(...)` now triggers the new bootstrap-execution fixture, and that fixture reruns the full activity-bootstrap chain internally instead of reusing the report the health path already built. On the real staged Calculator bundle, that duplicated class-resolution/runtime-smoke/bootstrap work is expensive enough to push the command past the timeout.
