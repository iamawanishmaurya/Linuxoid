# Phase 6: Recovery and Runtime Hardening - Research

**Researched:** 2026-05-19
**Domain:** Repeated app-state continuity and exact recovery truth for the real keyboard APK path
**Confidence:** MEDIUM

<user_constraints>
## User Constraints

### Locked Decisions
- Stay on Linuxoid's direct runtime path. No Waydroid, emulator, ADB, Android SDK, Gradle, network, or mandatory live-display dependency.
- Keep the work execution-first: this phase should harden the first real keyboard app path instead of broadening compatibility or framework scope.
- Preserve the real verification target `/home/astra/Downloads/keyboard-0.1.28.apk`.
- Keep exact blockers explicit instead of hiding them behind optimistic recovery output.

### the agent's Discretion
- Whether repeated-run continuity is surfaced mainly through `launch-apk`, `--first-app-start-proof`, or a narrower helper path, as long as `compatctl` remains the operator surface.
- Exact field names for persistence continuity, artifact reuse, or recovery-gating diagnostics, provided they stay deterministic and truthful.

### Deferred Ideas (OUT OF SCOPE)
- Solving the upstream native `dlopen_failed` seam in this phase.
- Solving the downstream `Activity.onCreate(Bundle)` managed-runtime seam in this phase.
- Adding broader app compatibility, new framework tiers, or UI polish.

</user_constraints>

<architectural_responsibility_map>
## Architectural Responsibility Map

Single-tier runtime-hardening ownership:
- `src/apk_native_launch.cpp` owns repeated launch/session truth, top-level report assembly, and operator-facing blocker propagation.
- `src/apk_storage_bridge.cpp` owns sandbox/app-data continuity and deterministic marker/artifact behavior.
- `src/apk_permission_bridge.cpp` owns persisted permission/AppOps state, validation, and healing.
- `src/apk_self_healing_watchdog.cpp` owns recovery sequencing, gating, journal truth, and recommended next actions.
- `src/apk_window_bridge.cpp`, `src/apk_process_bridge.cpp`, and `src/apk_runtime_bridge.cpp` own downstream persisted contracts whose repair behavior should respect upstream launch blockers.
- `tests/test_main.cpp` owns repeated-run regression coverage and recovery truth assertions.

</architectural_responsibility_map>

<research_summary>
## Summary

The real keyboard APK path has already become much more precise than it was at project start:

- package resolution works
- binary manifest decode works
- permissions/AppOps state is persisted under the app sandbox
- storage state is persisted under the app sandbox
- the resolved `SettingsActivity` owns a concrete window/focus target
- the upstream native blocker is exact:
  - `native_loading_state: "dlopen_failed"`
  - `native_loading_library_name: "libandroidx.graphics.path.so"`
  - `native_loading_detail: "/usr/lib/libm.so: invalid ELF header"`

A fresh guarded smoke against the real verification target shows Phase 6's real opportunity clearly:

- `storage_health: "ready"`
- `sandbox_health: "ready"`
- `permission_health: "ready"`
- `app_ops_health: "ready"`
- `recoverable: true`
- `recommended_recovery_action: "inspect_native_launch_diagnostics"`
- the Self-Healing Android Device watchdog still attempts six recovery actions and only one succeeds:
  - `restart_surface` -> failed
  - `restart_lifecycle` -> succeeded
  - `rerun_intent_resolution` -> failed
  - `rebuild_process_manager_state` -> failed
  - `rebuild_window_manager_state` -> failed
  - `retry_runtime_bootstrap` -> failed

That means Linuxoid's first real app path is already preserving meaningful state, but two hardening gaps remain:

1. **Repeated-run continuity is not yet the explicit phase focus.**
   The storage, permission, and AppOps contracts exist, but Phase 6 should make repeated verification runs preserve and report those same sandbox-backed artifacts intentionally instead of merely re-creating them incidentally.

2. **Recovery sequencing still overshoots the upstream blocker.**
   Once the native launch seam is already known blocked, the watchdog should stop pretending later window/process/runtime repairs are equally actionable. The first real app path needs recovery output that is comparable across runs and rooted in the earliest true failure.

**Primary recommendation:** Phase 6 should keep the exact native blocker untouched, harden the sandbox-backed app state so repeated keyboard APK launches reuse and validate the same persisted contracts, and tighten the Self-Healing Android Device recovery logic so downstream repairs are gated by upstream launch readiness instead of generating noisy attempted-failed actions.
</research_summary>

<standard_stack>
## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Existing `wfa/apk_native_launch` path | repo-local | Real APK launch/session truth and operator-facing JSON | Already owns the keyboard APK blocker story |
| Existing `wfa/apk_storage_bridge` contract | repo-local | Persisted sandbox/app-data continuity | Already owns deterministic app-data markers and path safety |
| Existing `wfa/apk_permission_bridge` contract | repo-local | Persisted permission/AppOps continuity and healing | Already validates and heals persisted permission state |
| Existing `wfa/apk_self_healing_watchdog` path | repo-local | Recovery sequencing, journal output, and next-action truth | Already owns the Self-Healing Android Device runtime harness |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Existing process/window/runtime contracts | repo-local | Downstream persisted state and repair entrypoints | Use when recovery gating needs to suppress or permit downstream repairs |
| Existing `tests/test_main.cpp` keyboard-like regression path | repo-local | Deterministic repeated-run and recovery assertions | Use instead of inventing a second hardening-only harness |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|---------|---------|---------|
| Hardening the current sandbox-backed contracts | A new project-level persistence subsystem | Would widen scope away from the first real app path |
| Tightening watchdog gating in place | A second native-blocker triage command | Would split recovery truth away from `launch-apk` and `--first-app-start-proof` |
</standard_stack>

<architecture_patterns>
## Architecture Patterns

### Pattern 1: Same app-data root across repeated verification runs
The keyboard APK path should keep one deterministic sandbox/app-data story so repeated runs can validate and compare persisted artifacts instead of rebuilding everything blindly.

### Pattern 2: Earliest blocker wins
When launch is blocked upstream by native loading, downstream process/window/runtime repair actions should either be suppressed or clearly marked as gated by that earlier seam.

### Pattern 3: Comparable recovery evidence
The Self-Healing Android Device journal should make repeated failures comparable by preserving exact subsystem, reason, action, result, and next-action truth instead of emitting a different pile of downstream failures each time.

### Anti-Patterns to Avoid
- **Mistaking re-created state for durable state:** recreating files on each launch is not the same thing as preserving and validating them.
- **Letting downstream repairs obscure an upstream blocker:** Phase 6 should reduce recovery noise, not add more of it.
- **Adding a second hardening-only report path:** `launch-apk` and `--first-app-start-proof` must stay authoritative.

</architecture_patterns>

<dont_hand_roll>
## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Sandbox continuity | A new persistence layer | Existing storage + permission/AppOps contracts under `sandbox/data/data/<package>` | Keeps the first real app path on one contract chain |
| Recovery triage | A separate diagnostics utility | Existing Self-Healing Android Device watchdog and journal | Keeps one operator-facing truth |
| Repeated-run verification | An ad hoc manual-only checklist | Existing CLI surfaces plus deterministic regression coverage | Keeps the phase grounded and testable |

**Key insight:** Phase 6 does not need more architecture. It needs the existing sandbox, permission, and recovery contracts to behave like durable runtime state for one real app path.
</dont_hand_roll>

<common_pitfalls>
## Common Pitfalls

### Pitfall 1: Clobbering persisted contracts on repeated launch
**What goes wrong:** every run rewrites state in a way that hides continuity bugs or stale-state bugs.
**How to avoid:** make repeated-run artifact reuse and validation explicit in the same sandbox/app-data root.

### Pitfall 2: Recovery noise outrunning the root cause
**What goes wrong:** the watchdog emits many downstream attempted-failed repairs even though the real launch cannot proceed past native loading.
**How to avoid:** gate or classify downstream repairs against the earliest upstream blocker.

### Pitfall 3: Docs claiming “recovered” when the app is still blocked
**What goes wrong:** some subsystem recovers locally and the project story sounds more successful than the real app path is.
**How to avoid:** keep `final_health`, `recommended_next_action`, and the exact native blocker aligned in launch, first-app-start, and watchdog outputs.

</common_pitfalls>

## Validation Architecture

Phase 6 validation should stay deterministic and CLI-driven:

- **Quick path:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build`
- **Full path:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure`
- **Repeated-run keyboard smoke:** run `launch-apk` or `launch-apk --first-app-start-proof --window-proof` twice against `/home/astra/Downloads/keyboard-0.1.28.apk` with the same staging root and inspect continuity/report fields

Critical assertions for this phase:

- storage, permission, and AppOps artifacts remain deterministic and comparable across repeated keyboard APK runs
- the real keyboard APK preserves the exact upstream native blocker on repeated runs
- the Self-Healing Android Device watchdog recommends and records recovery actions that reflect the earliest true blocker instead of downstream noise

<open_questions>
## Open Questions

- Should repeated-run continuity be reported as artifact reuse, artifact validation, or both?
- Which downstream repairs should be explicitly suppressed once `native_loading_state: "dlopen_failed"` is known?
- Should `launch-apk --first-app-start-proof` mirror the watchdog's tightened recovery story directly, or should it only carry the top-level recommended next action?

</open_questions>

---

*Phase: 06-recovery-and-runtime-hardening*
*Research completed: 2026-05-19*
