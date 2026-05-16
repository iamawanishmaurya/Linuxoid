# Attached-ADB auto-launch overuses metadata lookup

- Exact error:
  ```text
  Test project /home/astra/codex/wine-for-android/build
      Start 1: wfa_tests
  Errors while running CTest
  1/1 Test #1: wfa_tests ........................***Failed    0.02 sec
  Test failure: unexpected command in attached-adb auto-resolve launch test


  0% tests passed, 1 tests failed out of 1

  Total Test time (real) =   0.02 sec

  The following tests FAILED:
  	  1 - wfa_tests (Failed)
  ```

- Reproduction steps:
  1. Add attached-ADB auto-resolution tests for `launch-package`, `preflight-runtime`, and package metadata lookup.
  2. Implement auto-resolution by routing `launch-package` through the new full metadata query path.
  3. Build with `cmake --build build`.
  4. Run `ctest --test-dir build --output-on-failure`.

- Environment:
  - Repository: `/home/astra/codex/wine-for-android`
  - Branch: `main`
  - Date: `2026-05-16`
  - Host: Codex desktop session with full filesystem and network access

- First hypothesis:
  The launch fallback path only needs a resolved launcher component, but the first implementation reused the full attached-ADB metadata lookup, which adds package-path and dumpsys queries. That makes the launch path more coupled than necessary and breaks the focused test contract.
