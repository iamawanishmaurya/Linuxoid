# Installed package matrix test misses component confirmation

- Exact error:
  ```text
  Test project /home/astra/codex/wine-for-android/build
      Start 1: wfa_tests
  1/1 Test #1: wfa_tests ........................***Failed    0.11 sec
  Test failure: expected first generic matrix package to pass


  0% tests passed, 1 tests failed out of 1

  Total Test time (real) =   0.11 sec

  The following tests FAILED:
  	  1 - wfa_tests (Failed)
  Errors while running CTest
  ```

- Reproduction steps:
  1. Build the project with `cmake --build build`.
  2. Run `ctest --test-dir build --output-on-failure`.
  3. Observe `TestInstalledPackageMatrixSuccessPath` failing with `expected first generic matrix package to pass`.

- Environment:
  - Repository: `/home/astra/codex/wine-for-android`
  - Branch: `main`
  - Date: `2026-05-16`
  - Host: Codex desktop session with full filesystem and network access

- First hypothesis:
  The new generic matrix unit test uses a mocked attached-ADB launch result that says only `Status: ok` and `Complete`, while the real `LaunchInstalledAppWithRunner` path requires the output to confirm the requested component. The implementation may be correct and the test fixture may be under-specified.
