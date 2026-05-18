# 06-02 Summary

## Result

The Self-Healing Android Device watchdog now keeps the earliest native blocker authoritative on the real keyboard APK path and stops attempting noisy downstream launch-dependent repairs behind it.

## What changed

- Added explicit watchdog fields for `primary_blocker_reason`, `recovery_gating_state`, and `recovery_gating_reason`.
- Taught the watchdog to detect upstream native launch blockers like `libraries_failed_to_load` and `native_loading_state: dlopen_failed` before scheduling downstream launch-dependent repairs.
- Launch-dependent repairs such as `restart_surface`, `rerun_intent_resolution`, `rebuild_process_manager_state`, `rebuild_window_manager_state`, and `retry_runtime_bootstrap` are now journaled as `skipped_upstream_blocker` when the session is already blocked by native loading.
- Added regression coverage that proves repeated blocked keyboard-like runs produce stable watchdog JSON and journal artifacts with zero attempted downstream repairs.
- Updated repo-facing docs and status output so the same blocker and gating semantics are visible through CLI status, docs, and the persisted recovery report.

## Remaining blockers

- Upstream native blocker: `resolve_dlopen_failure_for_libandroidx_graphics_path_so`
- Downstream managed-runtime seam: `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`
