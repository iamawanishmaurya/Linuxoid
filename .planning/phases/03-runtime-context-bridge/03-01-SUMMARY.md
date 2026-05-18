---
phase: 03-runtime-context-bridge
plan: "01"
subsystem: receiver-and-argument-propagation
tags: [keyboard, dex, lifecycle, invoke, bundle]
completed: 2026-05-18
---

# Phase 3 Plan 01 Summary

Linuxoid now carries the synthetic keyboard lifecycle receiver and its first lifecycle argument farther than the old `invoke_receiver_missing` seam.

## What changed

- The synthetic keyboard-identity lifecycle fixture now encodes the real signature:
  - `onCreate(Landroid/os/Bundle;)V`
- The minimal DEX probe now:
  - respects the code-item `ins_size` window
  - materializes the lifecycle receiver in the parameter-register window
  - materializes a deterministic `Landroid/os/Bundle;` placeholder alongside it
- The first-app-start proof now records:
  - `lifecycle_receiver_state`
  - `lifecycle_receiver_register`
  - `lifecycle_parameter_state`
  - `lifecycle_parameter_class_descriptor`
  - `lifecycle_parameter_register`

## Why it matters

Phase 3 needed to stop failing at a missing receiver before the real framework seam was even reached. Linuxoid now proves that the managed keyboard seam can carry receiver and Bundle placeholder state into the first framework-owned lifecycle call.

## Honest outcome

- The old blocker `dex_invoke_receiver_missing` is gone for the targeted synthetic keyboard seam.
- This is still not a real ART heap or ActivityThread context.
- The runtime is now blocked one step later, at the stubbed framework-owned `Activity.onCreate(Landroid/os/Bundle;)V` boundary.
