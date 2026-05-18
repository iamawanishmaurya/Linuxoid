---
phase: 01-keyboard-apk-intake
plan: "01"
subsystem: runtime
tags: [apk, manifest, binary-xml, zip, keyboard]
requires: []
provides:
  - Linuxoid-owned binary AndroidManifest decode for the real keyboard APK intake path
  - Deflated and stored ZIP entry support in the direct APK archive reader
  - Trustworthy package, launcher, permission, and ABI facts for the verification APK
affects: [keyboard-apk-intake, managed-activity-start, runtime-intake]
tech-stack:
  added: []
  patterns:
    - binary AndroidManifest decode before plain-XML fallback
    - real APK intake stays on the existing compatctl direct launch path
key-files:
  created:
    - include/wfa/android_binary_xml.hpp
    - src/android_binary_xml.cpp
  modified:
    - include/wfa/apk_archive.hpp
    - src/apk_archive.cpp
    - src/apk_loader.cpp
    - src/apk_native_launch.cpp
    - src/apk_permission_bridge.cpp
    - tests/test_main.cpp
key-decisions:
  - "Decode binary AndroidManifest.xml inside Linuxoid instead of depending on external Android tooling."
  - "Extend the existing APK archive reader for deflated ZIP entries so real APK intake uses the same path as fixture launches."
patterns-established:
  - "Real APK intake must either report truthful package/component metadata or stop at an explicit blocker."
  - "Phase checkpoints stay inside compatctl surfaces instead of adding sidecar inspection tools first."
requirements-completed: []
duration: 75min
completed: 2026-05-18
---

# Phase 1 Plan 01: Harden real-world APK manifest and metadata intake Summary

**Binary AndroidManifest decode and deflated ZIP intake now let Linuxoid identify the real keyboard APK package, launcher, permissions, and ABI facts through the direct compatctl path**

## Performance

- **Duration:** 75 min
- **Started:** 2026-05-18T17:00:00+05:30
- **Completed:** 2026-05-18T18:22:29+05:30
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- Added a Linuxoid-owned binary AndroidManifest decoder for real APK intake.
- Extended APK archive handling so deflated entries no longer stall or block the keyboard APK path.
- Bound the decoded package, launcher, permission, and ABI facts into the existing direct launch/report flow.

## Task Commits

Each task was committed atomically:

1. **Task 1: Characterize the real keyboard APK intake failure boundary** - `6a63448` (feat)
2. **Task 2: Add the smallest real-APK manifest metadata bridge** - `6a63448` (feat)

**Plan metadata:** Summary/state close-out committed separately after the implementation checkpoint.

## Files Created/Modified
- `include/wfa/android_binary_xml.hpp` - Public contract for the Linuxoid binary AndroidManifest decoder.
- `src/android_binary_xml.cpp` - Binary AndroidManifest parser used by real APK intake.
- `include/wfa/apk_archive.hpp` - Archive API surface extended for deflated-entry handling.
- `src/apk_archive.cpp` - Stored and deflated ZIP entry reading for real APKs.
- `src/apk_loader.cpp` - Loader integration for decoded manifest payloads.
- `src/apk_native_launch.cpp` - Direct launch/report path now consumes decoded real-APK metadata.
- `src/apk_permission_bridge.cpp` - Permission inventory updated to reuse decoded manifest state.
- `tests/test_main.cpp` - Regression coverage for real APK binary-manifest and archive-intake seams.

## Decisions Made
- Used a Linuxoid-owned binary XML decoder instead of Android SDK or emulator-side tooling so the checkpoint stays offline and reproducible.
- Kept the real APK intake on the existing `compatctl launch-apk` path so later phases build on one truthful runtime entrypoint.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- The verification APK depended on both binary AndroidManifest decoding and deflated ZIP entry support, so the intake fix needed both seams before the package facts became trustworthy.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Ready for `01-02` staging and inventory validation work.
- Real APK intake now reaches trustworthy metadata; the next seam is staged asset/permission/native-library inventory continuity.

---
*Phase: 01-keyboard-apk-intake*
*Completed: 2026-05-18*
