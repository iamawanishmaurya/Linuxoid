---
phase: 10-jni-registration-dispatch
created: 2026-05-19
---

# Phase 10 Research — JNI Registration Dispatch

**Researched:** 2026-05-19  
**Goal:** Move Linuxoid past the current `jni_registration_dispatch_required` seam for the real `keyboard-0.1.28.apk` path without widening into broad Android framework work.

## Current live seam

Verified on the real target:

- `native_loading_state: "jni_registration_dispatch_required"`
- `native_jni_state: "called"`
- `native_app_start_bridge_state: "linuxoid_managed_app_start_bridge_selected"`
- `native_post_jni_startup_state: "jni_registration_dispatch_required"`
- `native_post_jni_dispatch_symbol_kind: "registration_helper"`
- `native_post_jni_dispatch_symbol: "_ZN8latinime21registerNativeMethodsEP7_JNIEnvPKcPK15JNINativeMethodi"`
- `blocking_reason: "jni_registration_dispatch_required_for_first_app_start:libjni_latinime.so"`
- `next_blocker: "dispatch_jni_registration_for_libjni_latinime_so"`

## What Phase 10 should do

Phase 10 should stay narrow:

1. Execute or precisely attempt the discovered registration-helper boundary for `libjni_latinime.so`
2. Keep `JNI_OnLoad`, registration dispatch, registration outcome, and later managed bootstrap seams distinct
3. Preserve the later managed blocker:
   - `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

## What Phase 10 should not do

- Do not widen into full ActivityThread recreation
- Do not introduce Waydroid, emulator, Android SDK, Gradle, or network work
- Do not hide registration failure behind generic native-launch wording

## Existing code seams to extend

- `src/native_execute_stub.cpp`
- `include/wfa/native_execute_stub.hpp`
- `src/apk_native_launch.cpp`
- `include/wfa/apk_native_launch.hpp`
- `src/apk_self_healing_watchdog.cpp`
- `tests/test_main.cpp`

## Expected next blocker shape

Once registration dispatch exists, the next honest seam should become one of:

- missing or blocked JNI registration callback execution
- missing `JNIEnv`/`JavaVM` behavior required by registration
- first managed bootstrap seam after registration
- later framework seam:
  - `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

## Research conclusion

Phase 10 should focus on one small move: execute the registration-helper seam for `libjni_latinime.so`, surface exact registration outcome fields, and stop at the first post-registration managed/bootstrap blocker.

*Research completed: 2026-05-19*
