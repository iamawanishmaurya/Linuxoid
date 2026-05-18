# Phase 12 Research — Managed Runtime Context Binding

**Goal:** Move Linuxoid past the current `managed_runtime_context_required` seam for the real `keyboard-0.1.28.apk` path without widening into broad Android framework recreation.

## Current live seam

The real keyboard APK now reports:

- `native_loading_state: "managed_runtime_context_required"`
- `native_jni_state: "called"`
- `native_loading_library_name: "libjni_latinime.so"`
- `native_managed_activity_dispatch_state: "linuxoid_dispatch_attempted"`
- `native_managed_activity_dispatch_component: "org.futo.inputmethod.latin/.uix.settings.SettingsActivity"`
- `native_post_jni_dispatch_symbol: "org.futo.inputmethod.latin.uix.settings.SettingsActivity->onCreate(Landroid/os/Bundle;)V"`
- `blocking_reason: "managed_runtime_context_required_for_first_app_start:libjni_latinime.so"`
- `next_blocker: "bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context"`

## What Phase 12 should do

Phase 12 should stay narrow:

1. Bind a Linuxoid-owned managed runtime context once JNI registration and managed activity target selection have succeeded.
2. Reuse the existing real keyboard activity target and lifecycle symbol for `SettingsActivity->onCreate(Landroid/os/Bundle;)V`.
3. Surface the first exact post-binding lifecycle or framework seam through launch JSON, first-app-start JSON, and watchdog output.
4. Keep the downstream `Activity.onCreate(Bundle)` framework boundary explicit unless Linuxoid truly moves beyond it.

## What Phase 12 should not do

- Do not widen into full Android framework recreation.
- Do not hide the difference between managed activity dispatch selection and managed runtime-context binding.
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

The next successful checkpoint should no longer stop at generic managed-runtime-context wording. It should either:

- attempt a Linuxoid-owned managed runtime-context binding path and stop at a smaller lifecycle or framework-owned seam, or
- report a precise managed runtime-context blocker with explicit component, lifecycle, or dispatch details.

That smaller seam should remain machine-readable and become the new stable repo truth.
