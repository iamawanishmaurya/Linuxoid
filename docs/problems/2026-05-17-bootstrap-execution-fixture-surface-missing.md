# Bootstrap Execution Fixture Surface Missing

- Exact error:
  - `/home/astra/codex/wine-for-android/tests/test_main.cpp:3:10: fatal error: wfa/art_bootstrap_execution_fixture.hpp: No such file or directory`
- Reproduction steps:
  1. `cd /home/astra/codex/wine-for-android`
  2. `cmake --build build && ctest --test-dir build --output-on-failure`
- Environment:
  - Host: Codex desktop local workspace
  - Date: 2026-05-17
  - OS/runtime: Linux, C++20 build
- First hypothesis:
  - The red tests now depend on a new bootstrap-execution fixture surface and CLI contract that has not been implemented yet. Adding the header, implementation, command wiring, and health/replay integration should move the failure from “missing surface” to product-level behavior checks.
