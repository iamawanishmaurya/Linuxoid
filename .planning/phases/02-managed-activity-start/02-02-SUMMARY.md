---
phase: 02-managed-activity-start
plan: "02"
subsystem: lifecycle-dex-boundary
tags: [keyboard, dex, lifecycle, oncreate]
completed: 2026-05-18
---

# Phase 2 Plan 02 Summary

Linuxoid now reports the real keyboard activity lifecycle seam precisely enough to hand the next execution task a concrete managed-runtime blocker.

## What changed

- Real APK DEX string decoding now handles staged MUTF-8 data correctly, which unblocked class and method lookup on the verification APK.
- The DEX probe now reports:
  - `target_class_lookup_state`
  - `target_method_lookup_state`
  - `code_item_lookup_state`
- The real lifecycle method is now surfaced as:
  - `onCreate(Landroid/os/Bundle;)V`

## Why it matters

Before this step, the keyboard path could collapse into generic states like `dex_unavailable` or `not_attempted`. Now Linuxoid can say whether the class was resolved, whether the lifecycle method was found, and whether the code item was actually present.

## Honest outcome

- The real `SettingsActivity` class and lifecycle method are resolved from staged DEX metadata.
- The synthetic keyboard-identity execution seam still stops at the first framework-call receiver boundary:
  - `framework_boundary_reason: invoke_receiver_missing`
- Full ART-owned lifecycle dispatch is still not claimed.
