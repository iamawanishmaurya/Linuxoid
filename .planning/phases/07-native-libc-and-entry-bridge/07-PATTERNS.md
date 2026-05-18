---
phase: 7
slug: native-libc-and-entry-bridge
status: draft
created: 2026-05-19
---

# Phase 7 - Patterns

## Reuse These Existing Patterns

### Native seam reporting through `apk_native_launch`
- Keep new blocker truth flowing through [src/apk_native_launch.cpp](/home/astra/codex/wine-for-android/src/apk_native_launch.cpp) instead of inventing a second reporting path.

### Compatibility shim ownership in `native_execute_stub`
- Keep Android-libc/native-entry compatibility work centered in [src/native_execute_stub.cpp](/home/astra/codex/wine-for-android/src/native_execute_stub.cpp) and its helper shims.

### Deterministic local fixture coverage
- Extend [tests/test_main.cpp](/home/astra/codex/wine-for-android/tests/test_main.cpp) with tightly scoped offline fixtures that model the specific entry-library seam we are moving.

## Avoid These Patterns

### Do not widen into general framework dispatch
Phase 7 is about getting one real native entry seam smaller, not recreating ActivityThread.

### Do not bypass the real APK path
Synthetic fixtures can pin the seam, but the phase should still validate its truth against `/home/astra/Downloads/keyboard-0.1.28.apk` as an opt-in smoke target.

### Do not collapse native states together
Keep unresolved-symbol, `JNI_OnLoad`, registration, and missing-entrypoint states distinct.
