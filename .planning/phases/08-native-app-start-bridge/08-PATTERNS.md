---
phase: 8
slug: native-app-start-bridge
status: draft
created: 2026-05-19
---

# Phase 8 - Patterns

## Reuse These Existing Patterns

### Native seam reporting through `apk_native_launch`
- Keep new app-start bridge truth flowing through [src/apk_native_launch.cpp](/home/astra/codex/wine-for-android/src/apk_native_launch.cpp) instead of inventing a second reporting path.

### Primary-library selection and JNI surfaces in `native_execute_stub`
- Keep JNI-shaped library selection, `JNI_OnLoad`, and later app-start seam reporting centered in [src/native_execute_stub.cpp](/home/astra/codex/wine-for-android/src/native_execute_stub.cpp).

### Deterministic local fixture coverage
- Extend [tests/test_main.cpp](/home/astra/codex/wine-for-android/tests/test_main.cpp) with tightly scoped offline fixtures that model the exact post-`JNI_OnLoad` app-start seam we are moving.

### Earliest-blocker watchdog propagation
- Reuse [src/apk_self_healing_watchdog.cpp](/home/astra/codex/wine-for-android/src/apk_self_healing_watchdog.cpp) so Self-Healing Android Device recovery wording stays pinned to the earliest post-`JNI_OnLoad` seam.

## Avoid These Patterns

### Do not widen into general framework dispatch
Phase 8 is about turning a JNI-shaped native library into a Linuxoid-owned app-start bridge, not recreating ActivityThread yet.

### Do not bypass the real APK path
Synthetic fixtures can pin the seam, but the real keyboard APK still validates the planning truth.

### Do not collapse native and managed blockers together
Keep post-`JNI_OnLoad` native app-start seams distinct from the later managed `Activity.onCreate(Bundle)` seam.
