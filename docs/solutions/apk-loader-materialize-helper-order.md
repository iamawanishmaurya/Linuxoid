# Solution: move APK loader staging helper onto declared shell helpers

Related problem: [2026-05-16-apk-loader-materialize-helper-order.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-apk-loader-materialize-helper-order.md)

## What failed

`MaterializeDecodedPayload` called `QuoteForShell` and `RunCommandCapture` before those helpers were declared inside the anonymous namespace in `src/apk_loader.cpp`.

## What worked

Added forward declarations for both helpers above `MaterializeDecodedPayload`.

## Why it worked

The compiler now sees the helper signatures before compiling the new staging function, so the function body can call them without relying on definition order.

## Commands run

```bash
cmake --build build
```
