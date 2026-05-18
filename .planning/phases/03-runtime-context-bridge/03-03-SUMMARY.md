---
phase: 03-runtime-context-bridge
plan: "03"
subsystem: regression-and-repo-truth
tags: [tests, docs, status, keyboard]
completed: 2026-05-18
---

# Phase 3 Plan 03 Summary

The narrowed runtime-context seam is now pinned in tests, CLI proof output, and repo truth.

## What changed

- Regression coverage now proves that the keyboard-identity `SettingsActivity` seam:
  - resolves `onCreate(Landroid/os/Bundle;)V`
  - materializes receiver plus Bundle placeholder state
  - stops at the exact stubbed framework boundary instead of `invoke_receiver_missing`
- Docs and status output now match the proof:
  - the real keyboard APK still blocks earlier at `libraries_failed_to_load` and `surface_not_ready_for_first_app_start`
  - the managed runtime seam now blocks later at `framework-boundary-stubbed:Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V`

## Why it matters

Phase 4 now inherits a stable managed-runtime blocker and a stable upstream native-library blocker instead of a moving observation from local development.

## Honest outcome

- The project still does not claim full ART-owned framework dispatch.
- The repo truth now matches the actual runtime seam Linuxoid proves today.
