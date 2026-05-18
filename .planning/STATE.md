---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: active
stopped_at: Phase 7 planned; next work is to close the `libjni_latinime.so` Android-libc/native-entry seam
last_updated: "2026-05-19T16:40:00.000Z"
last_activity: 2026-05-19 -- Phase 7 Native libc Compatibility and Entry Bridge planned
progress:
  total_phases: 7
  completed_phases: 6
  total_plans: 19
  completed_plans: 16
  percent: 84
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-18)

**Core value:** Run a real Android app directly on Linux through Linuxoid's own compatibility/runtime path, with honest execution and honest blockers instead of emulator fallback.
**Current focus:** Phase 7 planned - Native libc Compatibility and Entry Bridge

## Current Position

Phase: 7 of 7 (Native libc Compatibility and Entry Bridge)
Plan: planned
Status: Phase 7 planned - Linuxoid now needs to get `libjni_latinime.so` past the Android-libc/native-entry seam on the real keyboard APK path
Last activity: 2026-05-19 -- Phase 7 planning created around the sharpened `__strchr_chk` / native-entry blocker

Progress: [████████░░] 84%

## Performance Metrics

**Velocity:**

- Total plans completed: 16
- Average duration: -
- Total execution time: 0.0 hours

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Initialization: Use `keyboard-0.1.28.apk` as the first real verification target
- Initialization: Prioritize direct Linux app execution over emulator fallback

### Pending Todos

- Resolve the upstream native blocker `resolve_native_symbol_gap_for_libjni_latinime_so___strchr_chk`
- Reach the first post-load native boundary `provide_native_activity_entrypoint_for_libjni_latinime_so`
- Bridge the downstream managed seam `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

### Blockers/Concerns

- Current project blocker: the real keyboard APK now resolves `org.futo.inputmethod.latin.uix.settings.SettingsActivity`, stages six `x86_64` native libraries, preserves repeated sandbox/permission/AppOps continuity, preloads the current Android-compat shim set, and still reports the sharpened upstream blocker through `native_loading_state`, `native_loading_library_name`, `native_loading_detail`, and nested `native_execute.android_compat_*` fields
- Current exact live blocker: `native_loading_state: native_activity_entrypoint_missing`, `native_loading_library_name: libjni_latinime.so`, `native_loading_detail: undefined symbol: __strchr_chk`
- Next managed-runtime seam after the native load moves: `framework_boundary_reason: android_activity_oncreate_bundle_stubbed_for_minimal_checkpoint` with `next_blocker: bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

## Session Continuity

Last session: 2026-05-19 16:40
Stopped at: Phase 7 planned; next work is to resolve the `libjni_latinime.so` Android-libc/native-entry blocker and then bridge the managed `Activity.onCreate(Bundle)` seam
Resume file: .planning/ROADMAP.md
