# Problem: Diagnostic replay missing trace-index surface

- Date: 2026-05-18
- Environment: `/home/astra/codex/wine-for-android` on Linux, CMake build directory `build`

## Exact error

```text
/home/astra/codex/wine-for-android/tests/test_main.cpp: In function ‘void {anonymous}::TestRuntimeDiagnosticReplayWritesStableArtifacts()’:
/home/astra/codex/wine-for-android/tests/test_main.cpp:2561:28: error: ‘const struct wfa::RuntimeDiagnosticReplayReport’ has no member named ‘trace_index_json_path’
/home/astra/codex/wine-for-android/tests/test_main.cpp:2570:55: error: ‘const struct wfa::RuntimeDiagnosticReplayReport’ has no member named ‘trace_index_json_path’
```

## Reproduction steps

1. Open the repo at `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe the compile failure in `tests/test_main.cpp`.

## First hypothesis

Linuxoid already writes merged replay JSONL output, but the replay report does not yet expose a deterministic trace-index artifact or a dedicated diagnostic-fixture command surface. The new tests are correctly failing because the replay contract still lacks the stronger offline diagnosis index we now want for harness-driven debugging.
