# Runtime diagnostic replay surfaces missing

- Exact error: `cmake --build build && ctest --test-dir build --output-on-failure` failed while compiling `tests/test_main.cpp` because `wfa::NativeArtRuntimeSmokeReport` has no `trace_jsonl_path` field and `wfa::ReplayRuntimeDiagnosticBundle(...)` does not exist yet.
- Reproduction steps:
  1. `cd /home/astra/codex/wine-for-android`
  2. `cmake --build build && ctest --test-dir build --output-on-failure`
- Environment: Codex desktop app, Linux, C++20, local repo `/home/astra/codex/wine-for-android`, date `2026-05-17`.
- First hypothesis: the new replay tests correctly exposed that runtime-smoke JSONL output and the replay-bundle fixture/API have not been implemented in production code yet.
