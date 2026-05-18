# Phase 5: Visible Wayland Interaction - Pattern Map

**Generated:** 2026-05-18
**Phase:** 5 - Visible Wayland Interaction

## Target File Patterns

### 1. Real APK launch/session truth
- **Target file:** `src/apk_native_launch.cpp`
- **Analog region:** `LaunchNativeApk`, `BuildFirstAppStartProof`, surface/window blocker propagation, top-level JSON rendering
- **Why it matters:** This is where Linuxoid ties the real keyboard APK's package/activity/process/runtime truth to the visible-launch operator surface.
- **Reuse pattern:** extend the existing launch/window proof fields with exact target, visibility, and blocker state instead of introducing a second display-only report.

### 2. Persisted window-manager session contract
- **Target files:** `src/apk_window_bridge.cpp`, `include/wfa/apk_window_bridge.hpp`
- **Analog region:** existing contract validation/healing, `window_state`, `blocking_reason`, `recommended_recovery_action`, and persisted artifact fields
- **Why it matters:** Phase 5 needs one stable session contract for surface attachment, visibility, focus, and recovery.
- **Reuse pattern:** keep new visible/focus state in the existing window-manager contract and healing flow.

### 3. Headless-safe surface bridge plus optional live probes
- **Target files:** `src/native_window_surface.cpp`, `src/wayland_surface_fixture.cpp`
- **Analog region:** existing `headless_fallback`, Wayland/EGL probe-only availability, geometry updates, and metadata/event artifacts
- **Why it matters:** Phase 5 must stay deterministic without a display while still exposing a real live-Wayland target when one exists.
- **Reuse pattern:** keep live probes optional and machine-readable; never make them a CI precondition.

### 4. Focus and input ownership
- **Target files:** `src/native_input_queue_fixture.cpp`, `src/apk_native_launch.cpp`
- **Analog region:** `focus_owned`, `focus_owner`, deterministic input events, and first-app-start or launch-proof propagation
- **Why it matters:** meaningful interaction needs one explicit focus owner tied to the same window/activity session.
- **Reuse pattern:** reuse the deterministic focus-owner contract instead of inventing a new input-only layer.

### 5. Regression coverage and repo truth
- **Target files:** `tests/test_main.cpp`, `README.md`, `CHANGELOG.md`, `docs/phased-build-plan.md`, `docs/self-healing-runtime-skeleton.md`, `docs/steps.md`, `src/project_status.cpp`
- **Why it matters:** Phase 5 should be very clear about what became visibly interactive, what stayed headless-only, and what still blocks the real keyboard APK.
- **Reuse pattern:** keep assertions on exact JSON fields and exact blockers rather than brittle full-document snapshots.

## Recommended File Mapping

| Planned Output | Closest Existing Analog |
|----------------|-------------------------|
| Real settings-activity surface target state | `src/apk_native_launch.cpp` launch/window proof fields |
| Persisted visible/focus state | `src/apk_window_bridge.cpp` contract report fields |
| Live Wayland availability honesty | `src/wayland_surface_fixture.cpp` probe report fields |
| Focus/input session continuity | `src/native_input_queue_fixture.cpp` focus ownership fields |
| Operator-facing phase truth | `README.md` + `docs/steps.md` execution-first sections |

## Landmines

- Do not treat `headless_fallback` as visible app success.
- Do not hide the upstream native blocker when the real keyboard APK cannot yet reach surface creation.
- Do not create a second window/display contract that bypasses the existing package/activity/process/window session chain.
- Do not let optional live Wayland probing become mandatory for tests.
