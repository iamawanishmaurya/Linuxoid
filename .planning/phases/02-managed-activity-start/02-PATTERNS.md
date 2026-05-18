# Phase 2: Managed Activity Start - Pattern Map

**Generated:** 2026-05-18
**Phase:** 2 - Managed Activity Start

## Target File Patterns

### 1. First-app-start report assembly
- **Target file:** `src/apk_native_launch.cpp`
- **Analog region:** `BuildFirstAppStartProof`, `DetermineFirstAppStartBlockingReason`, `RenderFirstAppStartJson`
- **Why it matters:** This is where Linuxoid threads package/activity/process/runtime/DEX state into the operator-facing JSON. Phase 2 should extend these helpers instead of creating a second report path.
- **Reuse pattern:** add exact new state fields next to existing proof fields, preserve blocker strings, and keep `RenderFirstAppStartJson` deterministic.

### 2. DEX class/method lookup and execution probe state
- **Target files:** `src/apk_dex_bridge.cpp`, `include/wfa/apk_dex_bridge.hpp`
- **Analog region:** existing execution probe fields for `class_loading_state`, `lifecycle_receiver_state`, `app_method_invocation_state`, `framework_boundary_state`, and `object_register_field_state`
- **Why it matters:** Phase 2's real APK work should extend this same probe struct and method-resolution path for `SettingsActivity`, not invent a separate keyboard-only parser.
- **Reuse pattern:** when unsupported, set explicit probe state and blocker strings instead of falling back to a generic launch failure.

### 3. Real APK smoke and deterministic regression tests
- **Target file:** `tests/test_main.cpp`
- **Analog region:** existing `launch-apk --first-app-start-proof` tests plus keyboard-APK manifest/activity smoke coverage around `org.futo.inputmethod.latin/.uix.settings.SettingsActivity`
- **Why it matters:** The repo already keeps most runtime checkpoint coverage in one file, with fixture helpers plus opt-in real APK smoke assertions. Phase 2 should stay in that style for consistency.
- **Reuse pattern:** assert stable JSON substrings for exact activity, blocker, and proof fields instead of fragile full-document string matches.

### 4. Repo-truth and status narration
- **Target files:** `README.md`, `CHANGELOG.md`, `docs/phased-build-plan.md`, `docs/steps.md`, `src/project_status.cpp`
- **Analog region:** execution-first checkpoint sections that name current capability, exact blocker, and next narrow seam
- **Why it matters:** Phase 2 will deepen a real activity-start checkpoint, so docs should update the exact blocker and nothing broader.
- **Reuse pattern:** explicitly separate "real APK activity resolution/proof" from "full Android app execution."

## Recommended File Mapping

| Planned Output | Closest Existing Analog |
|----------------|-------------------------|
| Real `SettingsActivity` binding in first-app-start proof | `src/apk_native_launch.cpp` first-app-start proof fields |
| Real activity DEX class/method resolution state | `src/apk_dex_bridge.cpp` execution probe fields used by fixture checkpoints |
| Keyboard APK managed-start regression assertions | `tests/test_main.cpp` first-app-start proof tests and keyboard APK smoke helpers |
| Checkpoint/blocker docs | `README.md` + `docs/steps.md` execution-first sections |

## Landmines

- Do not let real APK activity resolution overwrite or break the synthetic fixture checkpoints.
- Do not hide `libraries_failed_to_load`; Phase 2 should report a deeper managed-start seam without pretending the native blocker disappeared.
- Do not create a keyboard-only code path if the same bridge can be expressed through the existing DEX probe and first-app-start report.
