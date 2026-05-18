# Phase 11 Research — Managed Activity Dispatch Bridge

**Goal:** Move Linuxoid past the current `managed_activity_dispatch_required` seam for the real `keyboard-0.1.28.apk` path without widening into broad Android framework recreation.

## Current live seam

The real keyboard APK now reports:

- `native_loading_state: "managed_activity_dispatch_required"`
- `native_jni_state: "called"`
- `native_app_start_bridge_state: "linuxoid_managed_app_start_bridge_selected"`
- `native_registration_dispatch_state: "called"`
- `native_registration_dispatch_symbol_kind: "registration_callback"`
- `native_registration_dispatch_symbol: "_ZN8latinime22register_LanguageModelEP7_JNIEnv"`
- `native_registration_outcome_state: "register_natives_completed"`
- `native_registration_class_name: "org/futo/inputmethod/latin/xlm/LanguageModel"`
- `native_registration_method_count: 4`
- `blocking_reason: "managed_activity_dispatch_required_for_first_app_start:libjni_latinime.so"`
- `next_blocker: "bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context"`

## What Phase 11 should do

Phase 11 should stay narrow:

1. Bind a Linuxoid-owned managed activity dispatch strategy once JNI registration has succeeded.
2. Reuse the existing real keyboard activity target `org.futo.inputmethod.latin/.uix.settings.SettingsActivity`.
3. Surface the first exact post-dispatch runtime-context or framework seam through launch JSON, first-app-start JSON, and watchdog output.
4. Keep the downstream `Activity.onCreate(Bundle)` boundary explicit unless Linuxoid truly moves beyond it.

## What Phase 11 should not do

- Do not widen into full Android framework recreation.
- Do not hide the difference between JNI registration completion and managed activity dispatch.
- Do not add emulator, Waydroid, Android SDK, or network-dependent detours.
- Do not pretend real ART-owned ActivityThread bootstrap is complete unless code truly implements it.

## Relevant code paths

- `src/native_execute_stub.cpp`
- `include/wfa/native_execute_stub.hpp`
- `src/apk_native_launch.cpp`
- `include/wfa/apk_native_launch.hpp`
- `src/apk_dex_bridge.cpp`
- `include/wfa/apk_dex_bridge.hpp`
- `src/apk_self_healing_watchdog.cpp`
- `tests/test_main.cpp`

## Expected success shape

The next successful checkpoint should no longer stop at generic managed-activity dispatch wording. It should either:

- attempt a Linuxoid-owned managed activity dispatch path and stop at a smaller runtime-context or framework-owned seam, or
- report a precise managed dispatch blocker with explicit class, method, or runtime-context details.

That smaller seam should remain machine-readable and become the new stable repo truth.

