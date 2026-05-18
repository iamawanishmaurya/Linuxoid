---
phase: 02-managed-activity-start
plan: "03"
subsystem: regression-and-repo-truth
tags: [tests, docs, status, keyboard]
completed: 2026-05-18
---

# Phase 2 Plan 03 Summary

The managed activity-start checkpoint is now pinned in tests, CLI proof output, and repo truth.

## What changed

- Added regression coverage for a keyboard-identity fixture that proves Linuxoid:
  - names `org.futo.inputmethod.latin.uix.settings.SettingsActivity`
  - resolves `onCreate(Landroid/os/Bundle;)V`
  - reports exact managed-start lookup state
  - preserves the exact next managed boundary blocker
- Updated docs, status, and changelog to say the same thing the runtime proves.

## Why it matters

Phase 3 now starts from a stable, reviewable checkpoint instead of a one-off development observation.

## Honest outcome

- The repo now says plainly that the real keyboard APK still blocks earlier at `libraries_failed_to_load` and `surface_not_ready_for_first_app_start`.
- Once that upstream seam is cleared, the next managed-runtime blocker is still:
  - `propagate_framework_invoke_receiver_registers`
