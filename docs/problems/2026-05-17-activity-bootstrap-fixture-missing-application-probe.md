# Activity Bootstrap Fixture Missing Application Probe

- Exact error:
  - `Test failure: expected normalized application class in activity bootstrap json`
- Reproduction steps:
  1. `cd /home/astra/codex/wine-for-android`
  2. `cmake --build build && ctest --test-dir build --output-on-failure`
- Environment:
  - Host: Codex desktop local workspace
  - Date: 2026-05-17
  - OS/runtime: Linux, C++20 build
- First hypothesis:
  - `native-art-activity-bootstrap-fixture` still models only the launcher activity target. It needs to carry the normalized application class and emit explicit application/activity probe events so Linuxoid can represent a real host-side bootstrap sequence instead of a single activity-only plan.
