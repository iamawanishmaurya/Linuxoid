# Research Summary: Linuxoid v1.1 Visible App Launch

## Project Focus

Linuxoid should stay tightly focused on one goal for this milestone: move the real keyboard APK from the current `activity_oncreate_bundle_dispatch_required` seam into a visible and interactive settings launch on Linux without emulator or Waydroid fallback.

## Current Live Blocker

The current repo truth is already precise:

- `libjni_latinime.so` loads
- `JNI_OnLoad` succeeds
- JNI registration completes
- `SettingsActivity->onCreate(Landroid/os/Bundle;)V` is selected
- `linuxoid_runtime_context_bound` is recorded
- launch still stops at `activity_oncreate_bundle_dispatch_required`

## Key Findings

### Stack

The existing C++ and CMake bridge architecture is still the right stack. The next milestone should reuse:

- the current managed dispatch bridge
- staged assets, storage, permissions, and runtime state
- current native and JNI continuity
- the existing Wayland and EGL surface contracts
- the Self-Healing Android Device watchdog

### Table Stakes

- real `Activity.onCreate(Bundle)` dispatch for the keyboard settings activity
- visible surface readiness for the same launch session
- resource and app-state continuity deep enough for settings startup
- focus and interaction truth once the window exists
- exact blocker reporting throughout the path

### Watch Out For

- moving dispatch forward without proving visible launch
- hidden compiled-resource or initialization blockers after dispatch
- host-surface claims that drift away from actual launch state
- milestone scope drifting into IME behavior before settings launch works
- noisy recovery advice that stops reflecting the earliest real blocker

## Recommended Direction

The next roadmap should aim straight at:

1. `Activity.onCreate(Bundle)` dispatch inside the bound Linuxoid runtime context
2. the first exact post-dispatch framework, resource, or window-readiness seam
3. visible settings surface readiness on Wayland
4. focus, interaction, and recovery truth for the visible launch

## Bottom Line

Linuxoid already has the right execution-first spine. This milestone should not widen the architecture; it should turn the current bound runtime-context checkpoint into one visible and usable keyboard settings launch on Linux, with every remaining blocker still reported honestly.
