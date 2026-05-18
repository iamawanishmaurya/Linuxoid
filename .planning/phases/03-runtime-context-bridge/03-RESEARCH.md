# Phase 3: Runtime Context Bridge - Research

**Researched:** 2026-05-18
**Domain:** Managed receiver propagation, invoke-state continuity, and the next framework/runtime seam after `SettingsActivity.onCreate(Landroid/os/Bundle;)V`
**Confidence:** MEDIUM

<user_constraints>
## User Constraints

### Locked Decisions
- Stay on Linuxoid's direct runtime path. No Waydroid, emulator, ADB, Android SDK, Gradle, network, or live-display dependency for the normal runtime path.
- Keep the execution-first strategy: Phase 3 should narrow the next real managed-runtime seam instead of widening compatibility scope.
- Preserve the real verification target `/home/astra/Downloads/keyboard-0.1.28.apk`.
- Keep blockers explicit instead of faking ART, ActivityThread, or Android framework ownership.

### the agent's Discretion
- Whether the next seam is exposed through `launch-apk --first-app-start-proof` alone or via shared lower-level helpers, as long as `compatctl` remains the operator surface.
- Exact new field names for managed receiver, invoke-state, or lifecycle-parameter diagnostics, provided they stay deterministic and honest.

### Deferred Ideas (OUT OF SCOPE)
- Solving native `x86_64` library loading and JNI compatibility in the same phase.
- Visible Wayland interaction for the keyboard verification target.
- Broad ActivityThread or full Android framework recreation.

</user_constraints>

<architectural_responsibility_map>
## Architectural Responsibility Map

Single-tier managed-runtime ownership:
- `src/main.cpp` owns the CLI entrypoints.
- `src/apk_native_launch.cpp` owns staged session truth, first-app-start proof assembly, blocker propagation, and report JSON.
- `src/apk_dex_bridge.cpp` owns staged DEX parsing, minimal interpreter state, invoke handling, and exact bytecode or framework blockers.
- `tests/test_main.cpp` owns deterministic regression coverage for CLI proof output and fixture-managed seams.

</architectural_responsibility_map>

<research_summary>
## Summary

Phase 2 proved two things at once:

1. The real keyboard APK path now resolves `org.futo.inputmethod.latin/.uix.settings.SettingsActivity`, derives `Lorg/futo/inputmethod/latin/uix/settings/SettingsActivity;`, and resolves `onCreate(Landroid/os/Bundle;)V` from staged DEX metadata.
2. The current synthetic keyboard-identity execution seam still stops at the first framework-owned invoke boundary with:
   - `framework_boundary_state: "blocked"`
   - `framework_boundary_reason: "invoke_receiver_missing"`
   - `blocking_reason: "dex_invoke_receiver_missing"`
   - `next_blocker: "propagate_framework_invoke_receiver_registers"`

That means Phase 3 should not spend its first move on more DEX lookup work. The next narrow seam is receiver and argument continuity across the invoke boundary, plus whatever exact framework or runtime blocker appears *after* that seam is crossed.

The real keyboard APK still stops earlier at:

- `launch_status: "libraries_failed_to_load"`
- `surface_not_ready_for_first_app_start`

So Phase 3 should keep two truths visible:

- the end-to-end real APK launch is still blocked upstream by native-library and surface readiness
- the managed-runtime seam has moved forward enough that Linuxoid can now work on receiver propagation, argument placeholders, and the next post-invoke blocker independently

**Primary recommendation:** Phase 3 should advance the synthetic managed seam from `invoke_receiver_missing` into one smaller, more concrete runtime-context blocker by propagating the receiver register, tracking invoke arguments/pending results more faithfully, and modeling the minimal lifecycle parameter or framework-owned callee state honestly.
</research_summary>

<standard_stack>
## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Existing `wfa/apk_dex_bridge` path | repo-local | Minimal interpreter state, method lookup, invoke handling, blocker reporting | Already owns the current `invoke_receiver_missing` seam |
| Existing `wfa/apk_native_launch` path | repo-local | First-app-start proof assembly and operator-facing JSON | Already exposes the exact blocker and next-step contract |
| Existing `tests/test_main.cpp` | repo-local | Deterministic CLI and JSON regression coverage | Already owns the first-app-start checkpoint harness |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Existing keyboard-identity fixture helpers | repo-local | Deterministic managed-start regression inputs | Reuse for receiver/argument/runtime-context seams |
| Existing `apk_activity_launch_bridge` data | repo-local | Real activity identity and lifecycle target selection | Reuse for the real `SettingsActivity` target without adding a second resolver |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Extending the existing interpreter state | A new ART-bridge side path | Would split the execution truth before the current seam is exhausted |
| Narrow runtime-context bridge planning | Solving native/JNI load first | That would skip the exact managed seam we now know how to measure |
</standard_stack>

<architecture_patterns>
## Architecture Patterns

### Pattern 1: One blocker smaller
Each phase should replace one vague managed-runtime blocker with one smaller, exact seam. Phase 3 starts from `invoke_receiver_missing` and should end with a sharper post-receiver blocker.

### Pattern 2: Managed seam independent of upstream launch failure
The real keyboard APK may still fail early at `libraries_failed_to_load`, but the synthetic keyboard-identity execution seam can still advance the managed-runtime boundary and report it honestly.

### Pattern 3: State-rich invoke reporting
When crossing an invoke boundary, Linuxoid should expose receiver state, argument state, pending-result state, and the exact boundary status so later phases know whether the next work is interpreter-state, framework-stub, or ART-context work.

### Anti-Patterns to Avoid
- **Jumping straight to full ActivityThread claims:** the phase should narrow the seam, not pretend the seam is gone.
- **Hiding upstream native blockers:** the real keyboard APK still fails earlier and should continue to say so.
- **Adding a second reporting path:** `first_android_app_start` should stay authoritative.

</architecture_patterns>

<dont_hand_roll>
## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Receiver and invoke-state reporting | A separate sidecar interpreter log format | Existing `execution_probe` and `first_android_app_start` fields | Keeps the runtime truth in one place |
| Real activity targeting | A keyboard-only resolver | Existing activity/intent resolution plus staged DEX descriptor mapping | Already verified in Phase 2 |
| Regression surface | A new test binary | Existing `tests/test_main.cpp` first-app-start harness | Keeps later work on one CLI path |

**Key insight:** Phase 3 should not broaden into “real ART.” It should make Linuxoid honest and useful at the next managed seam after class and method lookup by carrying the receiver and invoke state one step farther.
</dont_hand_roll>

<common_pitfalls>
## Common Pitfalls

### Pitfall 1: Receiver propagation without visibility
**What goes wrong:** Linuxoid starts carrying the receiver through invoke handling but the report does not say whether that happened, so later blockers become ambiguous.
**How to avoid:** Add deterministic receiver/invoke-state fields to the existing probe and first-app-start report.

### Pitfall 2: Swallowing the next blocker
**What goes wrong:** After fixing `invoke_receiver_missing`, the runtime falls into a different vague failure such as generic framework-blocked or execution-failed.
**How to avoid:** Require an exact post-receiver blocker in every acceptance criterion.

### Pitfall 3: Mixing native/JNI work into the phase
**What goes wrong:** Phase 3 gets dragged into `x86_64` library loading because the real keyboard APK still fails earlier.
**How to avoid:** Keep Phase 3 focused on the managed seam; Phase 4 owns native/JNI loading.

</common_pitfalls>

## Validation Architecture

Phase 3 validation should stay deterministic and CLI-driven:

- **Quick path:** `cmake --build build`
- **Full path:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Managed seam smoke:** `compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity <apk> <staging-root>`

Critical assertions for this phase:

- the synthetic managed seam no longer stops at generic `invoke_receiver_missing` without richer invoke-state detail
- the report names the post-receiver blocker precisely
- the real keyboard APK path still preserves honest upstream `libraries_failed_to_load` and `surface_not_ready_for_first_app_start` blockers

<open_questions>
## Open Questions

- After receiver propagation is added, is the next exact blocker a missing Bundle parameter placeholder, a pending-result continuity gap, or a deeper framework-owned invoke boundary?
- Can Linuxoid keep the real keyboard DEX lookup path stable while deepening only the synthetic managed seam?
- Which existing first-app-start fixture path is the tightest place to model one minimal lifecycle parameter or framework-owned callee contract?

</open_questions>

---

*Phase: 03-runtime-context-bridge*
*Research completed: 2026-05-18*
