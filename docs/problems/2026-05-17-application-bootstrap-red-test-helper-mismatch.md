# Application Bootstrap Red Test Helper Mismatch

- Exact error:
  - `/home/astra/codex/wine-for-android/tests/test_main.cpp:2307:10: error: 'rendered' was not declared in this scope`
  - `/home/astra/codex/wine-for-android/tests/test_main.cpp:2336:29: error: 'ReadFile' was not declared in this scope; did you mean 'ReadTextFile'?`
- Reproduction steps:
  1. `cd /home/astra/codex/wine-for-android`
  2. `cmake --build build && ctest --test-dir build --output-on-failure`
- Environment:
  - Host: Codex desktop local workspace
  - Date: 2026-05-17
  - OS/runtime: Linux, C++20 build
- First hypothesis:
  - The new red test assertions were inserted before the existing `rendered` JSON variable was declared, and the trace reader used the wrong local helper name. Fixing the test harness should expose the intended product-level red failure next.
