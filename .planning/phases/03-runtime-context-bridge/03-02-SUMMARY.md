---
phase: 03-runtime-context-bridge
plan: "02"
subsystem: post-receiver-runtime-seam
tags: [keyboard, dex, framework-boundary, bundle]
completed: 2026-05-18
---

# Phase 3 Plan 02 Summary

Linuxoid now turns the first post-receiver runtime seam into one exact framework-owned lifecycle blocker.

## What changed

- The DEX probe now distinguishes:
  - missing receiver state
  - missing lifecycle argument placeholder state
  - a reached-but-stubbed framework boundary
- The synthetic keyboard seam now stops at:
  - `framework_boundary_state: framework-stubbed`
  - `framework_boundary_reason: android_activity_oncreate_bundle_stubbed_for_minimal_checkpoint`
  - `blocking_reason: framework-boundary-stubbed:Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V`
- First-app-start recovery and next-blocker mapping now point to:
  - `recommended_recovery_action: extend_runtime_context_bridge`
  - `next_blocker: bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

## Why it matters

Phase 4 and later managed-runtime work no longer need to infer where the lifecycle path failed. The runtime now says plainly that receiver and Bundle placeholders survived the invoke edge and the next missing piece is framework dispatch into a real managed context.

## Honest outcome

- Linuxoid still does not claim full ART-owned lifecycle dispatch.
- The narrower blocker is the stubbed `Activity.onCreate(Landroid/os/Bundle;)V` boundary, not a generic ActivityThread label.
