# Architecture Research: Build Order for Linuxoid v1.1 Visible App Launch

## Goal

Derive the smallest component progression needed to move the real keyboard settings activity from the current dispatch blocker into a visible launch on Linux.

## Current Live Seam

The milestone starts here:

- `native_loading_state: activity_oncreate_bundle_dispatch_required`
- `native_jni_state: called`
- `native_managed_activity_runtime_binding_state: linuxoid_runtime_context_bound`
- `native_post_jni_dispatch_symbol: org.futo.inputmethod.latin.uix.settings.SettingsActivity->onCreate(Landroid/os/Bundle;)V`
- `next_blocker: bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

## Major Components

### 1. Managed lifecycle dispatch

Responsibilities:
- carry the selected keyboard lifecycle target into real `Activity.onCreate(Bundle)` dispatch
- preserve exact post-dispatch blocker reporting

Current code anchors:
- `/home/astra/codex/wine-for-android/src/native_execute_stub.cpp`
- `/home/astra/codex/wine-for-android/src/apk_dex_bridge.cpp`
- `/home/astra/codex/wine-for-android/src/apk_native_launch.cpp`

### 2. Resource and session readiness

Responsibilities:
- keep storage, permissions, assets, and resource metadata available for the real settings launch
- expose the exact missing piece if visible startup still cannot proceed

Current code anchors:
- `/home/astra/codex/wine-for-android/src/apk_storage_bridge.cpp`
- `/home/astra/codex/wine-for-android/src/apk_permission_bridge.cpp`
- `/home/astra/codex/wine-for-android/src/apk_native_launch.cpp`

### 3. Visible surface path

Responsibilities:
- carry successful startup into a visible Wayland or EGL-backed surface when available
- keep headless-safe truth when it is not

Current code anchors:
- `/home/astra/codex/wine-for-android/src/apk_window_bridge.cpp`
- `/home/astra/codex/wine-for-android/src/native_window_surface.cpp`
- `/home/astra/codex/wine-for-android/src/apk_runtime_bridge.cpp`

### 4. Interaction and recovery

Responsibilities:
- keep focus and interaction tied to the same launch session
- preserve earliest-blocker authority in watchdog output

Current code anchors:
- `/home/astra/codex/wine-for-android/src/apk_self_healing_watchdog.cpp`
- `/home/astra/codex/wine-for-android/src/apk_window_bridge.cpp`

## Recommended Build Order

1. Bridge `Activity.onCreate(Bundle)` dispatch inside the bound Linuxoid runtime context
2. Expose the first exact post-dispatch framework, resource, or surface blocker
3. Carry successful dispatch into visible surface readiness
4. Tighten focus, interaction, and recovery truth around the visible launch

## Key Integration Boundary

The most important architectural seam for this milestone is the one between:

- Linuxoid's current `linuxoid_runtime_context_bound` placeholder state
- the first real post-dispatch startup state needed for a visible settings launch

That seam should remain explicit in code, reports, and roadmap phases because it is the shortest path from today's blocked proof to a visible app on Linux.
