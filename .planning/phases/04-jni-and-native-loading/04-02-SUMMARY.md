---
phase: 04-jni-and-native-loading
plan: "02"
subsystem: native-blocker-propagation
tags: [keyboard, native, jni, first-app-start, docs]
completed: 2026-05-18
---

# Phase 4 Plan 02 Summary

Linuxoid now keeps the real keyboard APK native blocker explicit through operator-facing proof surfaces and Self-Healing Android Device wording.

## What changed

- `launch-apk --first-app-start-proof` now preserves upstream native blockers with exact fields such as:
  - `blocking_reason: native_dlopen_failed_for_first_app_start:<library>`
  - `recommended_recovery_action: inspect_native_launch_diagnostics`
  - `next_blocker: resolve_dlopen_failure_for_<library>`
- The watchdog no longer suggests a surface restart first when the launch has already failed at an upstream native-library seam.
- Regression coverage now pins:
  - exact native-load failure reporting on `launch-apk`
  - exact upstream native blocker propagation on `--first-app-start-proof`
- Repo truth now says plainly that the downstream managed seam is still the stubbed `Activity.onCreate(Bundle)` boundary once native loading is solved.

## Why it matters

Phase 5 now inherits one coherent native/JNI blocker story instead of a mix of generic launch failure, surface repair guidance, and later managed placeholders.

## Honest outcome

- Linuxoid still does not claim JNI-backed success or full app startup.
- The real keyboard APK now stops at an exact upstream native-load seam, while the later managed Bundle-boundary seam remains visible as the next downstream blocker.
