---
phase: 02-managed-activity-start
plan: "01"
subsystem: managed-start-targeting
tags: [keyboard, dex, first-app-start, activity-resolution]
completed: 2026-05-18
---

# Phase 2 Plan 01 Summary

Linuxoid now threads the real keyboard verification target into `launch-apk --first-app-start-proof` instead of speaking only in fixture `MainActivity` terms.

## What changed

- The first-app-start proof now preserves the real activity identity:
  - `org.futo.inputmethod.latin/.uix.settings.SettingsActivity`
  - `org.futo.inputmethod.latin.uix.settings.SettingsActivity`
- The proof now records `activity_target_resolution_state` so later phases can tell whether the activity came from intent resolution, an explicit component, or a fallback.
- The DEX seam now derives the real class descriptor:
  - `Lorg/futo/inputmethod/latin/uix/settings/SettingsActivity;`

## Why it matters

Phase 2 needed to stop handing Phase 3 a synthetic-only story. Linuxoid can now point the managed-start proof at the real keyboard settings activity even when end-to-end launch is still blocked upstream.

## Honest outcome

- The real activity target is resolved.
- The app is **not** claimed to be fully started.
- The broader real launch still blocks earlier at `libraries_failed_to_load`.
