# ART runtime class-resolution fields missing

- Exact error: `cmake --build build && ctest --test-dir build --output-on-failure` failed while compiling `tests/test_main.cpp` because `wfa::NativeArtRuntimeSmokeReport` does not expose `resolved_target_class_name` or `resolved_target_class_descriptor` yet.
- Reproduction steps:
  1. `cd /home/astra/codex/wine-for-android`
  2. `cmake --build build && ctest --test-dir build --output-on-failure`
- Environment: Codex desktop app, Linux, C++20, local repo `/home/astra/codex/wine-for-android`, date `2026-05-17`.
- First hypothesis: the ART runtime smoke seam still only models generic runtime availability and has not been upgraded into a real host-side class-resolution attempt contract.
