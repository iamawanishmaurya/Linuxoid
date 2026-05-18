# Phase 13 Research — Activity onCreate Dispatch Bridge

**Goal:** Move Linuxoid past the current `activity_oncreate_bundle_dispatch_required` seam for the real `keyboard-0.1.28.apk` path without widening into broad Android framework recreation.

## Current live seam

The real keyboard APK now reports:

- `native_loading_state: "activity_oncreate_bundle_dispatch_required"`
- `native_jni_state: "called"`
- `native_loading_library_name: "libjni_latinime.so"`
- `native_managed_activity_runtime_binding_state: "linuxoid_runtime_context_bound"`
- `native_managed_activity_runtime_context_kind: "linuxoid_managed_runtime_context_placeholder"`
- `native_post_jni_dispatch_symbol: "org.futo.inputmethod.latin.uix.settings.SettingsActivity->onCreate(Landroid/os/Bundle;)V"`
- `blocking_reason: "activity_oncreate_bundle_dispatch_required_for_first_app_start:libjni_latinime.so"`
- `next_blocker: "bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context"`

## What Phase 13 should do

Phase 13 should stay narrow:

1. Spend the existing `linuxoid_runtime_context_bound` checkpoint on a real `Activity.onCreate(Bundle)` dispatch attempt for the keyboard settings activity.
2. Reuse the real `SettingsActivity->onCreate(Landroid/os/Bundle;)V` lifecycle symbol already selected by the current launch path.
3. Surface the first exact post-dispatch blocker through launch JSON, first-app-start JSON, and watchdog output.
4. Preserve the visible-launch path as future work unless code truly moves far enough to expose it honestly.

## What Phase 13 should not do

- Do not widen into full Android framework recreation.
- Do not blur runtime-context binding and lifecycle dispatch into one generic managed failure.
- Do not claim visible launch or IME behavior unless the real keyboard path actually reaches those states.
- Do not introduce emulator, Waydroid, Android SDK, Gradle, or network-dependent detours.

## Relevant code paths

- `src/native_execute_stub.cpp`
- `include/wfa/native_execute_stub.hpp`
- `src/apk_dex_bridge.cpp`
- `include/wfa/apk_dex_bridge.hpp`
- `src/apk_native_launch.cpp`
- `include/wfa/apk_native_launch.hpp`
- `src/apk_window_bridge.cpp`
- `src/apk_self_healing_watchdog.cpp`
- `tests/test_main.cpp`

## Expected success shape

The next successful checkpoint should no longer stop at `activity_oncreate_bundle_dispatch_required`. It should either:

- dispatch `Activity.onCreate(Bundle)` and surface the first exact post-dispatch framework, resource, or surface blocker, or
- report a smaller lifecycle-dispatch blocker with explicit class, method, and session details if the dispatch still cannot complete

That smaller seam should become the new stable repo truth for Phase 14 to target.
