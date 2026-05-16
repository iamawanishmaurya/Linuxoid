# ART runtime smoke find_if header missing

Problem: [docs/problems/2026-05-17-art-runtime-smoke-find-if-header-missing.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-art-runtime-smoke-find-if-header-missing.md)

## What failed

The ART runtime smoke implementation added `std::find_if` to choose a deterministic resolved class target from the offline DEX resolution results, but `src/art_runtime_smoke.cpp` did not include `<algorithm>`.

## What worked

Adding the missing standard header fixed the compile failure immediately.

## Why it worked

`std::find_if` is declared in `<algorithm>`. Once that header was present, the runtime-smoke translation unit could compile the new target-selection logic normally.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
