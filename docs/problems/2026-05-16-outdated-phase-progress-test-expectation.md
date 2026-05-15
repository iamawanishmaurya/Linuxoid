# Problem: outdated-phase-progress-test-expectation

- Date: 2026-05-16
- Exact error:

```text
Test failure: expected average phase progress to equal 54
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Increase the `P4` phase loading in `src/project_status.cpp`.
  3. Run `ctest --test-dir build --output-on-failure`.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The implementation is correct, but `tests/test_main.cpp` still expects the old average phase-progress value from before the new manifest-assessment capability raised `P4`.
