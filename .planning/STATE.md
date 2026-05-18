---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: active
stopped_at: Phase 9 planned; next work is to turn the Linuxoid-managed app-start bridge candidate into the first real post-bridge dispatch seam
last_updated: "2026-05-20T02:05:00.000Z"
last_activity: 2026-05-20 -- Phase 9 Managed App-Start Dispatch planned
progress:
  total_phases: 9
  completed_phases: 8
  total_plans: 25
  completed_plans: 22
  percent: 88
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-18)

**Core value:** Run a real Android app directly on Linux through Linuxoid's own compatibility/runtime path, with honest execution and honest blockers instead of emulator fallback.
**Current focus:** Phase 9 planned - Managed App-Start Dispatch

## Current Position

Phase: 9 of 9 (Managed App-Start Dispatch)
Plan: planned
Status: Phase 9 planned - Linuxoid now needs to turn the managed app-start bridge candidate for `libjni_latinime.so` into the first real post-bridge dispatch seam
Last activity: 2026-05-20 -- Phase 9 planning created around the post-bridge dispatch seam

Progress: [█████████░] 88%

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
- Expose the first exact post-bridge dispatch or JNI registration seam for `libjni_latinime.so`
- Bridge the downstream managed seam `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`
- Start the next milestone for JNI registration, framework bootstrap, and managed app ownership

### Blockers/Concerns

- Current project blocker: the real keyboard APK now resolves `org.futo.inputmethod.latin.uix.settings.SettingsActivity`, stages six `x86_64` native libraries, preserves repeated sandbox/permission/AppOps continuity, preloads the current Android-compat shim set, gets `libjni_latinime.so` loaded, calls `JNI_OnLoad`, and now reports the sharpened upstream blocker through `native_loading_state`, `native_app_start_bridge_state`, `native_post_jni_startup_state`, `native_loading_library_name`, and nested `native_execute.*` fields
- Current exact live blocker: `native_loading_state: linuxoid_managed_app_start_bridge_required`, `native_jni_state: called`, `native_loading_library_name: libjni_latinime.so`, `next_blocker: implement_linuxoid_managed_app_start_bridge_for_libjni_latinime_so`
- Current planning target: once that Linuxoid-managed bridge exists, the next exact seam must become a post-bridge dispatch, JNI registration, or managed bootstrap blocker instead of collapsing back into generic native launch wording
- Next managed-runtime seam after the native load moves: `framework_boundary_reason: android_activity_oncreate_bundle_stubbed_for_minimal_checkpoint` with `next_blocker: bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

## Session Continuity

Last session: 2026-05-20 00:05
Stopped at: Phase 9 planned; next work is to implement the Linuxoid-managed app-start bridge for `libjni_latinime.so`, expose the first post-bridge dispatch seam, and then bridge the managed `Activity.onCreate(Bundle)` seam
Resume file: .planning/ROADMAP.md
