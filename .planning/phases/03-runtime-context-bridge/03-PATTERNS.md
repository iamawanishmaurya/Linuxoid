# Phase 3: Runtime Context Bridge - Pattern Map

**Generated:** 2026-05-18
**Phase:** 3 - Runtime Context Bridge

## Target File Patterns

### 1. First-app-start report assembly
- **Target file:** `src/apk_native_launch.cpp`
- **Analog region:** `BuildFirstAppStartProof`, `DetermineFirstAppStartBlockingReason`, `DetermineFirstAppStartNextBlocker`, `RenderFirstAppStartJson`
- **Why it matters:** This is where Linuxoid threads DEX probe state into the operator-facing managed-runtime truth.
- **Reuse pattern:** extend the existing proof fields with precise receiver/invoke/runtime-context state rather than creating a second report object.

### 2. Minimal interpreter invoke handling
- **Target file:** `src/apk_dex_bridge.cpp`
- **Analog region:** existing `invoke-super`, `invoke-direct`, `move-result`, `new-instance`, `iget-object`, `iput-object`, and field-value handling
- **Why it matters:** Phase 3 starts from `invoke_receiver_missing`, so the next work belongs inside the existing invoke-state machine.
- **Reuse pattern:** when Linuxoid crosses one deeper invoke seam, report the new state and exact blocker through the same `execution_probe` contract.

### 3. Regression coverage for real and synthetic seams
- **Target file:** `tests/test_main.cpp`
- **Analog region:** existing first-app-start proof tests, including the keyboard-identity fixture that currently expects `framework_boundary_reason: invoke_receiver_missing`
- **Why it matters:** Phase 3 needs to prove the synthetic seam advanced without losing the real keyboard APK lookup truth.
- **Reuse pattern:** keep assertions on exact JSON substrings and blocker fields instead of brittle full-document comparisons.

### 4. Repo truth and planning handoff
- **Target files:** `README.md`, `CHANGELOG.md`, `docs/phased-build-plan.md`, `docs/self-healing-runtime-skeleton.md`, `docs/steps.md`, `src/project_status.cpp`
- **Why it matters:** Once the receiver seam advances, the docs must say what moved and what still blocks a real app.
- **Reuse pattern:** separate the synthetic managed seam, the real keyboard APK seam, and the remaining upstream native/surface blocker.

## Recommended File Mapping

| Planned Output | Closest Existing Analog |
|----------------|-------------------------|
| Receiver/invoke-state propagation fields | `src/apk_dex_bridge.cpp` execution probe fields |
| Post-receiver blocker in first-app-start JSON | `src/apk_native_launch.cpp` blocker/next-blocker helpers |
| Synthetic managed-seam regression | `tests/test_main.cpp` keyboard-identity first-app-start test |
| Repo-truth update for the narrowed blocker | `README.md` + `docs/steps.md` execution-first sections |

## Landmines

- Do not hide the real keyboard APK's upstream `libraries_failed_to_load` and `surface_not_ready_for_first_app_start` blockers.
- Do not create a second interpreter or report path for receiver propagation.
- Do not let the synthetic seam claim ART or ActivityThread ownership when the work is still placeholder- or stub-based.
