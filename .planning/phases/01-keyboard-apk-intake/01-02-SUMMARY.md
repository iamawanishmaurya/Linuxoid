---
phase: 01-keyboard-apk-intake
plan: "02"
subsystem: runtime
tags: [staging, assets, permissions, native-libs, keyboard]
requires:
  - phase: 01-keyboard-apk-intake
    provides: binary AndroidManifest decode and trustworthy real-APK package metadata
provides:
  - Real keyboard APK staging into the standard Linuxoid session root
  - Permission, asset/resource, and native-library inventory surfaced through existing proof reports
  - A precise downstream blocker at native-library loading instead of manifest intake
affects: [keyboard-apk-intake, managed-activity-start, jni-and-native-loading]
tech-stack:
  added: []
  patterns:
    - real APKs reuse the same session layout as fixture proof runs
    - intake checkpoints must surface later-phase blockers instead of timing out
key-files:
  created: []
  modified:
    - src/apk_native_launch.cpp
    - src/apk_permission_bridge.cpp
    - tests/test_main.cpp
    - README.md
    - CHANGELOG.md
key-decisions:
  - "Treat the real keyboard APK as a first-class staged session instead of a one-off smoke path."
  - "Keep native-library readiness honest: inventory is surfaced now, loading failure remains a later blocker."
patterns-established:
  - "Stage real APK metadata under the deterministic Linuxoid package/session root."
  - "Use existing proof/report seams for permissions, resources, assets, and ABI coverage before adding new tooling."
requirements-completed: []
duration: 50min
completed: 2026-05-18
---

# Phase 1 Plan 02: Validate staging, assets, permissions, and native library inventory Summary

**The real keyboard APK now stages into Linuxoid's deterministic session root with truthful permission, asset/resource, and x86_64 native-library inventory exposed through the existing proof surfaces**

## Performance

- **Duration:** 50 min
- **Started:** 2026-05-18T17:35:00+05:30
- **Completed:** 2026-05-18T18:22:29+05:30
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Reused the standard Linuxoid package/session layout for the real keyboard APK.
- Surfaced permission inventory, resource/asset visibility, and x86_64 library inventory from the staged session path.
- Narrowed the next failure boundary from "intake hangs" to `libraries_failed_to_load`.

## Task Commits

Each task was committed atomically:

1. **Task 1: Push real-APK staging through the existing session root** - `6a63448` (feat)
2. **Task 2: Expose real-APK permission, resource, and native-library inventory** - `6a63448` (feat)

**Plan metadata:** Summary/state close-out committed separately after the implementation checkpoint.

## Files Created/Modified
- `src/apk_native_launch.cpp` - Session staging and launch reporting now preserve real APK inventory facts.
- `src/apk_permission_bridge.cpp` - Permission proof path now reports decoded binary-manifest permissions for the keyboard APK.
- `tests/test_main.cpp` - Added deterministic coverage plus real-APK smoke-guard coverage for staged inventory behavior.
- `README.md` - Repository truth updated for the real keyboard APK intake checkpoint.
- `CHANGELOG.md` - Recorded the real APK staging/inventory checkpoint.

## Decisions Made
- Used the existing staged session root as the only source of truth so later phases can reuse artifacts instead of re-inspecting the APK ad hoc.
- Left native-library execution blocked on actual loading compatibility rather than smoothing it over with false readiness.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- The most useful result from this checkpoint was not a successful launch; it was turning the real APK failure into an exact later blocker: `libraries_failed_to_load`.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Ready for `01-03` proof-surface stabilization and repo-truth lock-in.
- The next execution seam after Phase 1 is direct native-library/runtime compatibility for the keyboard APK.

---
*Phase: 01-keyboard-apk-intake*
*Completed: 2026-05-18*
