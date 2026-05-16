# ART runtime smoke find_if header missing

- Exact error: `cmake --build build && ctest --test-dir build --output-on-failure` failed because `src/art_runtime_smoke.cpp` uses `std::find_if` without including `<algorithm>`.
- Reproduction steps:
  1. `cd /home/astra/codex/wine-for-android`
  2. `cmake --build build && ctest --test-dir build --output-on-failure`
- Environment: Codex desktop app, Linux, C++20, local repo `/home/astra/codex/wine-for-android`, date `2026-05-17`.
- First hypothesis: the new host-side class-resolution attempt logic added a standard algorithm call but the translation unit still lacks the required header.
