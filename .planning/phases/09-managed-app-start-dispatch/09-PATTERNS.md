---
phase: 9
slug: managed-app-start-dispatch
status: draft
created: 2026-05-20
---

# Phase 9 - Patterns

## Reuse These Existing Patterns

### Post-bridge truth through `apk_native_launch`
- Keep new dispatch truth flowing through [src/apk_native_launch.cpp](/home/astra/codex/wine-for-android/src/apk_native_launch.cpp) instead of adding a second launcher story.

### Primary-library and bridge ownership in `native_execute_stub`
- Keep `JNI_OnLoad`, bridge ownership, and post-bridge dispatch reporting centered in [src/native_execute_stub.cpp](/home/astra/codex/wine-for-android/src/native_execute_stub.cpp).

### Deterministic local fixture coverage
- Extend [tests/test_main.cpp](/home/astra/codex/wine-for-android/tests/test_main.cpp) with tightly scoped offline fixtures that model the exact post-bridge dispatch seam we are moving.

### Earliest-blocker watchdog propagation
- Reuse [src/apk_self_healing_watchdog.cpp](/home/astra/codex/wine-for-android/src/apk_self_healing_watchdog.cpp) so Self-Healing Android Device recovery wording stays pinned to the earliest post-bridge seam.

## Avoid These Patterns

### Do not widen into generic framework dispatch
Phase 9 is about the first Linuxoid-owned post-bridge dispatch seam, not recreating full ActivityThread yet.

### Do not bypass the real APK path
Synthetic fixtures can pin the seam, but the real keyboard APK still validates the planning truth.

### Do not collapse bridge dispatch and later managed bootstrap together
Keep post-bridge dispatch seams distinct from the later `Activity.onCreate(Bundle)` seam.
