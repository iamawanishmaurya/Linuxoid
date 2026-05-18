# Linuxoid

## What This Is

Linuxoid is a Wine-style Android-on-Linux runtime that aims to run Android APKs directly on a Linux desktop without an emulator, full Android VM, or Waydroid-style dependency. The current codebase already stages the real keyboard APK, resolves `SettingsActivity`, gets `libjni_latinime.so` loaded, completes JNI registration, and binds a deterministic Linuxoid runtime-context placeholder; the current milestone is to carry that exact path into a visible, interactive settings launch on Wayland while keeping every remaining blocker honest.

## Current Milestone: v1.1 Visible App Launch

**Goal:** Turn the real keyboard APK's `activity_oncreate_bundle_dispatch_required` seam into a visible, interactive settings launch on Linux without emulator fallback.

**Target features:**
- Bridge `org.futo.inputmethod.latin.uix.settings.SettingsActivity->onCreate(Landroid/os/Bundle;)V` into the already bound Linuxoid runtime context
- Carry the real app from post-dispatch startup into a visible Wayland-backed settings surface
- Preserve exact native, JNI, resource, window, and Self-Healing Android Device blockers while visible launch moves forward

## Core Value

Run a real Android app directly on Linux through Linuxoid's own compatibility/runtime path, with honest execution and honest blockers instead of emulator fallback.

## Requirements

### Validated

- ✓ Direct APK inspection, staging, and launch-proof contracts exist through `compatctl launch-apk`
- ✓ Package, intent, activity, process, storage, permissions, window, runtime, and self-healing session artifacts are materialized under deterministic staged runtime roots
- ✓ Linuxoid parses real DEX structures and executes minimal managed bytecode checkpoints with explicit opcode and framework-boundary reporting
- ✓ The real keyboard APK path now gets `libjni_latinime.so` loaded, calls `JNI_OnLoad`, dispatches a real JNI registration callback, observes `RegisterNatives`, and binds `linuxoid_runtime_context_bound` for `SettingsActivity->onCreate(Landroid/os/Bundle;)V`
- ✓ Deterministic offline tests and live smoke paths already keep the direct runtime path honest instead of hiding blockers behind emulator fallback

### Active

- [ ] Dispatch the real keyboard settings activity through `Activity.onCreate(Bundle)` inside the bound Linuxoid runtime context
- [ ] Reach a visible settings launch for `keyboard-0.1.28.apk` on a Wayland Linux desktop through Linuxoid's direct path
- [ ] Preserve meaningful focus, interaction, and session continuity once the visible launch exists
- [ ] Keep exact launch blockers, native diagnostics, and Self-Healing Android Device recovery truth authoritative throughout the visible-launch push

### Out of Scope

- Emulator or full Android VM fallback — violates the direct-runtime goal
- Broad compatibility work before one real app visibly launches — distracts from the milestone
- Full system-wide IME enablement in this milestone — the settings activity launch comes first
- Broad Android framework recreation before one visible launch works — too much sideways architecture for the current seam
- Desktop packaging or general-user polish — the milestone is still a developer-facing runtime checkpoint

## Context

The repository is already a brownfield runtime prototype with substantial bridge-based infrastructure in `/home/astra/codex/wine-for-android/src/` and `/home/astra/codex/wine-for-android/include/wfa/`. The exact live blocker is now precise and reproducible: the real keyboard APK resolves `org.futo.inputmethod.latin/.uix.settings.SettingsActivity`, stages six `x86_64` native libraries, keeps sandbox and permission continuity intact, gets `libjni_latinime.so` loaded, calls `JNI_OnLoad`, completes JNI registration, selects `SettingsActivity->onCreate(Landroid/os/Bundle;)V`, records `linuxoid_runtime_context_bound`, and then stops at `activity_oncreate_bundle_dispatch_required`. Verification should stay anchored to `/home/astra/Downloads/keyboard-0.1.28.apk`, `compatctl launch-apk --first-app-start-proof`, and `compatctl launch-apk --window-proof` on a Wayland Linux desktop.

## Constraints

- **Runtime model**: Native Linux compatibility layer first — no emulator, VM, or Waydroid detour
- **Verification app**: `keyboard-0.1.28.apk` — milestone success must be grounded in the real target app
- **Host target**: Wayland Linux desktop — visible launch and interaction must stay truthful there
- **Execution honesty**: Unsupported ART or framework seams must remain explicit and actionable
- **Development style**: Execution-first slices — every phase must reduce the real app blocker, not widen architecture sideways

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Linuxoid targets direct Android app execution on Linux without emulator fallback | The project exists to prove a Wine-style runtime path, not a wrapper around a full Android system image | ✓ Good |
| `keyboard-0.1.28.apk` remains the first verification target | A concrete APK prevents planning drift and keeps success measurable | ✓ Good |
| Wayland desktop behavior remains the primary host target | The first usable checkpoint needs visible launch and real interaction on a modern Linux desktop | ✓ Good |
| The next milestone optimizes for visible settings launch before IME behavior | The settings activity is the shortest honest path to a real app launch; IME service behavior is later work | ✓ Good |
| The current blocker must stay explicit as `activity_oncreate_bundle_dispatch_required` until code truly moves beyond it | Honest blocker progression is more valuable than papering over missing framework behavior | ✓ Good |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `$gsd-transition`):
1. Requirements invalidated? -> Move to Out of Scope with reason
2. Requirements validated? -> Move to Validated with phase reference
3. New requirements emerged? -> Add to Active
4. Decisions to log? -> Add to Key Decisions
5. "What This Is" still accurate? -> Update if drifted

**After each milestone** (via `$gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-19 after starting milestone v1.1 Visible App Launch*
