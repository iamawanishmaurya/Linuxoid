# Linuxoid

## What This Is

Linuxoid is a Wine-style Android-on-Linux runtime that aims to run Android APKs directly on a Linux desktop without an emulator, full Android VM, or Waydroid-style dependency. The current codebase already stages APKs, resolves activities, bootstraps runtime/session state, and executes small real DEX bytecode checkpoints; the next milestone is to turn that execution-first spine into a first genuinely usable Android app launch on Linux. The first verification target is `keyboard-0.1.28.apk` from Downloads, and the first host target is a Wayland Linux desktop.

## Core Value

Run a real Android app directly on Linux through Linuxoid’s own compatibility/runtime path, with honest execution and honest blockers instead of emulator fallback.

## Requirements

### Validated

- ✓ Direct APK inspection, staging, and launch-proof contracts exist through `compatctl launch-apk` — existing
- ✓ Package, intent, activity, process, storage, permissions, window, runtime, and self-healing session artifacts are materialized under `.planning`-adjacent staged runtime roots — existing
- ✓ Linuxoid parses real DEX structures and executes a minimal `MainActivity`-style bytecode checkpoint with explicit opcode/blocker reporting — existing
- ✓ Deterministic offline tests exist for the execution-first spine, including DEX parsing, method invocation, object/register/field handling, and first-app-start proof reports — existing
- ✓ The codebase already prefers direct Linux runtime ownership over emulator-only validation and records explicit blockers like `needs-real-activitythread-context` — existing

### Active

- [ ] Launch `keyboard-0.1.28.apk` through Linuxoid’s direct APK path on Linux without emulator or Waydroid fallback
- [ ] Reach a first real Activity-managed execution seam beyond the current minimal DEX interpreter checkpoints, with class-loading, lifecycle receiver, and runtime context wired tightly enough for one app to start
- [ ] Make the first target app visibly render and accept meaningful user interaction on a Wayland Linux desktop
- [ ] Keep the execution path honest by reporting exact blockers whenever Linuxoid still lacks real ART/framework behavior needed by the app

### Out of Scope

- Full Android system-image execution via emulator or VM — violates the project’s core runtime goal
- Broad compatibility-matrix expansion before one real app works end to end — distracts from the execution-first checkpoint
- UI polish, launcher packaging, or general-user setup flows before the developer prototype can run the verification APK directly

## Context

The repository is already a brownfield runtime prototype with substantial bridge-based infrastructure in `/home/astra/codex/wine-for-android/src/` and `/home/astra/codex/wine-for-android/include/wfa/`. The direct execution spine currently runs through `compatctl launch-apk --first-app-start-proof`, which stages APK contents, resolves `MainActivity`, builds session/process/window/runtime state, and executes a minimal DEX path with explicit reporting. The codebase map under `/home/astra/codex/wine-for-android/.planning/codebase/` shows that Linuxoid already has strong deterministic artifact and test patterns; those should be reused rather than replaced. Verification should center on the real APK `keyboard-0.1.28.apk` from Downloads behaving on Linux as closely as possible to Android for a first developer-facing milestone.

## Constraints

- **Runtime model**: Native Linux compatibility layer first — the project goal is direct APK execution without emulator or full Android container dependency
- **Host target**: Wayland Linux desktop — first visible launch, focus, and input work should optimize for modern Wayland environments
- **Verification app**: `keyboard-0.1.28.apk` — milestone success must be anchored to a concrete real APK, not only synthetic fixtures
- **Execution honesty**: No fake ART/framework claims — unsupported boundaries must stay explicit and actionable
- **Development style**: Execution-first slices — prefer the smallest real runtime improvement that gets one app closer to running

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Linuxoid targets direct Android app execution on Linux without emulator fallback | The whole point is Wine-style app execution, not another wrapper around a full Android system image | — Pending |
| `keyboard-0.1.28.apk` is the first real verification target | A concrete APK keeps the project grounded and prevents architecture from drifting away from actual app execution | — Pending |
| Wayland desktop behavior is the first host environment to optimize | The first usable checkpoint should prove visible launch and input on a modern Linux desktop | — Pending |
| Continue from the current direct APK and minimal DEX execution spine | The repo already has a working execution-first base, and replacing it would waste proven progress | ✓ Good |

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
*Last updated: 2026-05-18 after initialization*
