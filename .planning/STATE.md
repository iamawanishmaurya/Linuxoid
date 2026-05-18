---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: active
stopped_at: Phase 8 complete; next work is to implement the Linuxoid-managed app-start bridge and then bridge the managed `Activity.onCreate(Bundle)` seam
last_updated: "2026-05-19T20:25:00.000Z"
last_activity: 2026-05-19 -- Phase 8 Native App-Start Bridge executed
progress:
  total_phases: 8
  completed_phases: 8
  total_plans: 22
  completed_plans: 22
  percent: 100
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-18)

**Core value:** Run a real Android app directly on Linux through Linuxoid's own compatibility/runtime path, with honest execution and honest blockers instead of emulator fallback.
**Current focus:** Phase 8 complete - Native App-Start Bridge

## Current Position

Phase: 8 of 8 (Native App-Start Bridge)
Plan: complete
Status: Phase 8 complete - Linuxoid now reports the real keyboard APK blocker as a Linuxoid-managed app-start bridge requirement instead of a generic missing native activity entrypoint
Last activity: 2026-05-19 -- Phase 8 executed around the post-`JNI_OnLoad` app-start seam

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**

- Total plans completed: 22
- Average duration: -
- Total execution time: 0.0 hours

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Initialization: Use `keyboard-0.1.28.apk` as the first real verification target
- Initialization: Prioritize direct Linux app execution over emulator fallback

### Pending Todos

- Implement the Linuxoid-managed app-start bridge for the JNI-shaped `libjni_latinime.so`
- Bridge the downstream managed seam `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`
- Start the next milestone for JNI registration, framework bootstrap, and managed app ownership

### Blockers/Concerns

- Current project blocker: the real keyboard APK now resolves `org.futo.inputmethod.latin.uix.settings.SettingsActivity`, stages six `x86_64` native libraries, preserves repeated sandbox/permission/AppOps continuity, preloads the current Android-compat shim set, gets `libjni_latinime.so` loaded, calls `JNI_OnLoad`, and now reports the sharpened upstream blocker through `native_loading_state`, `native_app_start_bridge_state`, `native_post_jni_startup_state`, `native_loading_library_name`, and nested `native_execute.*` fields
- Current exact live blocker: `native_loading_state: linuxoid_managed_app_start_bridge_required`, `native_jni_state: called`, `native_loading_library_name: libjni_latinime.so`, `next_blocker: implement_linuxoid_managed_app_start_bridge_for_libjni_latinime_so`
- Next managed-runtime seam after the native load moves: `framework_boundary_reason: android_activity_oncreate_bundle_stubbed_for_minimal_checkpoint` with `next_blocker: bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

## Session Continuity

Last session: 2026-05-20 00:05
Stopped at: Phase 8 complete; next work is to implement the Linuxoid-managed app-start bridge for `libjni_latinime.so` and then bridge the managed `Activity.onCreate(Bundle)` seam
Resume file: .planning/ROADMAP.md
