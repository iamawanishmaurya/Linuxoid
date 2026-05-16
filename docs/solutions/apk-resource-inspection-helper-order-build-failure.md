# APK Resource Inspection Helper Order Build Failure

Links back to: [2026-05-17-apk-resource-inspection-helper-order-build-failure.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-apk-resource-inspection-helper-order-build-failure.md)

## What failed

The new manifest-inspection helpers in `src/apk_loader.cpp` called `ExtractFirstMatch` before any declaration or definition was visible.

## What worked

Added an explicit forward declaration for `ExtractFirstMatch` near the top of the anonymous namespace and then restored the actual local definition inside the same translation unit.

## Why it worked

The compile-stage error was about declaration order, and the follow-up link-stage error showed that the helper had never been defined in this file at all. Restoring both the declaration and definition resolved the call sites cleanly without changing behavior.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
