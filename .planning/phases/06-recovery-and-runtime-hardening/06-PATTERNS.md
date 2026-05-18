# Phase 6: Recovery and Runtime Hardening - Pattern Map

**Generated:** 2026-05-19
**Phase:** 6 - Recovery and Runtime Hardening

## Target File Patterns

### 1. Repeated launch/session truth
- **Target file:** `src/apk_native_launch.cpp`
- **Analog region:** top-level health/blocker propagation, keyboard APK proof assembly, report JSON rendering
- **Why it matters:** This is where repeated verification runs should expose continuity truth and preserve the earliest blocker.
- **Reuse pattern:** extend the existing launch surfaces with continuity and recovery-gating fields instead of creating a second hardening-only report.

### 2. Durable sandbox/app-data continuity
- **Target files:** `src/apk_storage_bridge.cpp`, `include/wfa/apk_storage_bridge.hpp`
- **Analog region:** marker-file proof, deterministic directories, path safety, storage artifact paths
- **Why it matters:** Phase 6 needs repeated runs to validate and reuse the same sandbox-backed state intentionally.
- **Reuse pattern:** preserve deterministic marker/artifact behavior and make reuse or validation explicit.

### 3. Durable permission/AppOps continuity
- **Target files:** `src/apk_permission_bridge.cpp`, `include/wfa/apk_permission_bridge.hpp`
- **Analog region:** persisted JSON validation, healing actions, stale/incompatible/incomplete recovery paths
- **Why it matters:** Permission/AppOps state is already persisted and should become part of the repeated-run hardening story.
- **Reuse pattern:** reuse the existing validators and healing flow instead of inventing new permission storage.

### 4. Recovery gating and journal truth
- **Target files:** `src/apk_self_healing_watchdog.cpp`, `include/wfa/apk_self_healing_watchdog.hpp`
- **Analog region:** subsystem health classification, recovery-action selection, journal writing, recommended next action
- **Why it matters:** Phase 6 should reduce downstream repair noise when the earliest native blocker is already known.
- **Reuse pattern:** gate or classify repairs in the existing watchdog instead of creating a new recovery layer.

### 5. Regression coverage and repo truth
- **Target files:** `tests/test_main.cpp`, `README.md`, `CHANGELOG.md`, `docs/phased-build-plan.md`, `docs/self-healing-runtime-skeleton.md`, `docs/steps.md`, `src/project_status.cpp`
- **Why it matters:** The hardening checkpoint should become durable and visible to operators.
- **Reuse pattern:** assert exact JSON fields and exact blocker/recovery semantics, not broad snapshots.

## Recommended File Mapping

| Planned Output | Closest Existing Analog |
|----------------|-------------------------|
| Repeated-run continuity state | `src/apk_native_launch.cpp` launch/first-app-start proof fields |
| Sandbox/app-data reuse validation | `src/apk_storage_bridge.cpp` marker and directory contract |
| Permission/AppOps reuse validation | `src/apk_permission_bridge.cpp` persisted-state healing |
| Watchdog blocker gating | `src/apk_self_healing_watchdog.cpp` recovery-action selection |
| Phase-truth operator output | `README.md` + `src/project_status.cpp` checkpoint language |

## Landmines

- Do not solve the native `dlopen_failed` seam in a “hardening” phase by silently bypassing it.
- Do not let repeated-run continuity overwrite or erase evidence from previous runs before it is validated.
- Do not claim a recovered app start if only a local subsystem healed while the real keyboard APK path remains blocked.
- Do not introduce a second persistence or recovery story outside the existing launch path.
