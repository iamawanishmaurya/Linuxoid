# Phase 5: Visible Wayland Interaction - Research

**Researched:** 2026-05-18
**Domain:** Real APK surface/window/focus interaction on Linuxoid's direct runtime path
**Confidence:** MEDIUM

<user_constraints>
## User Constraints

### Locked Decisions
- Stay on Linuxoid's direct runtime path. No Waydroid, emulator, ADB, Android SDK, Gradle, network, or mandatory live-display dependency.
- Keep the work execution-first: Phase 5 should move one real app path closer to visible interaction on Linux instead of widening compatibility scope.
- Preserve the real verification target `/home/astra/Downloads/keyboard-0.1.28.apk`.
- Keep blockers explicit instead of faking real Android framework or rendering ownership.

### the agent's Discretion
- Whether the first visible-launch milestone is expressed primarily through `launch-apk --window-proof`, `inspect-apk-window`, or shared lower-level helpers, as long as `compatctl` stays the operator surface.
- Exact field names for surface-target, focus, or interaction diagnostics, provided they stay deterministic and honest.

### Deferred Ideas (OUT OF SCOPE)
- Solving full ART-owned framework dispatch in the same phase.
- Broad JNI/native-loader remediation beyond preserving the exact blocker already exposed in Phase 4.
- UI polish, launcher packaging, or distribution work.

</user_constraints>

<architectural_responsibility_map>
## Architectural Responsibility Map

Single-tier visible interaction ownership:
- `src/main.cpp` owns the CLI entrypoints like `launch-apk --window-proof`, `inspect-apk-window`, and the optional Wayland/EGL fixtures.
- `src/apk_native_launch.cpp` owns real session truth, blocker propagation, and operator-facing JSON for the verification APK.
- `src/apk_window_bridge.cpp` owns the persisted window/session contract and healing behavior.
- `src/native_window_surface.cpp` and `src/wayland_surface_fixture.cpp` own the headless-safe surface bridge plus optional live Wayland/EGL probing.
- `src/native_input_queue_fixture.cpp` owns deterministic focus and input ownership artifacts.
- `tests/test_main.cpp` owns deterministic CLI and JSON regression coverage.

</architectural_responsibility_map>

<research_summary>
## Summary

Phase 4 narrowed the real keyboard APK blocker to one exact upstream native seam:

- `launch_status: "libraries_failed_to_load"`
- `native_loading_state: "dlopen_failed"`
- `native_loading_library_name: "libandroidx.graphics.path.so"`
- `native_loading_detail: "/usr/lib/libm.so: invalid ELF header"`

The same truth now propagates through `launch-apk --first-app-start-proof`, so Linuxoid no longer confuses a native-loader failure with a later surface restart or managed-runtime placeholder.

The window/surface side is already stronger than the real keyboard APK path currently uses:

- `launch-apk --window-proof` on deterministic fixtures already writes a stable `window_manager` contract with `headless_safe: true`
- `src/native_window_surface.cpp` already runs optional Wayland and EGL probes and falls back honestly to `headless_fallback`
- `src/native_input_queue_fixture.cpp` already models deterministic focus ownership through `focus_owner = "linuxoid-native-window"`
- `inspect-apk-window` already exposes the persisted window-manager session contract

The real keyboard APK smoke today shows an important split:

1. Linuxoid resolves the real package, component, process/session, DEX, and window/session roots.
2. The visible surface path still blocks early because launch is not ready:
   - `surface.state: "blocked"`
   - `window_health: "blocked"`
   - `recommended_recovery_action: "inspect_native_launch_diagnostics"`

That means Phase 5 should not pretend the verification app is visibly rendering yet. It should do two smaller, useful things:

- bind the resolved real `SettingsActivity` session to a Linuxoid-owned surface target and focus contract in a way that remains valid whether the display is live or headless
- preserve the exact upstream native blocker when that blocker prevents visible launch

**Primary recommendation:** Treat the first visible-launch milestone as a Linuxoid-owned Wayland/window target bound to the resolved `org.futo.inputmethod.latin/.uix.settings.SettingsActivity` session. When a live Wayland/EGL environment is available, surface a best-effort real target. When it is not, keep the exact same session machine-readable through headless-safe metadata and explicit availability truth. Focus and input ownership should attach to that same session, not to a fixture-only synthetic window ID.
</research_summary>

<standard_stack>
## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Existing `wfa/apk_native_launch` path | repo-local | Real APK launch/session truth and operator-facing JSON | Already owns the keyboard APK blocker story |
| Existing `wfa/apk_window_bridge` contract | repo-local | Persisted window/session state and healing | Already models the window-manager session Linuxoid needs |
| Existing `wfa/native_window_surface` path | repo-local | Headless-safe surface bridge with optional Wayland/EGL probes | Already provides the closest thing to a visible target bridge |
| Existing `wfa/native_input_queue_fixture` | repo-local | Deterministic focus/input ownership | Already models focused input without external Android services |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Existing `wfa/wayland_surface_fixture` | repo-local | Optional real Wayland client probe | Use to expose live-availability truth without making CI depend on a display |
| Existing `wfa/egl_smoke_fixture` | repo-local | Optional EGL pbuffer probe | Use to distinguish probe-only availability from pure headless fallback |
| Existing activity/package/process contracts | repo-local | Real `SettingsActivity` identity and session binding | Reuse instead of inventing a second target-selection path |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Extending the current window/surface/session contracts | A brand-new visible-launch subsystem | Would split the app-start truth and slow down the first real app path |
| Preserving the upstream native blocker | Pretending surface work can ignore launch readiness | Would make Phase 5 output less honest than Phase 4 |
</standard_stack>

<architecture_patterns>
## Architecture Patterns

### Pattern 1: Same session, richer surface truth
The visible-launch milestone should reuse the existing package/activity/process/window session artifacts instead of introducing a second display-only contract.

### Pattern 2: Best-effort live Wayland, deterministic headless fallback
Linuxoid should surface real Wayland/EGL availability when present, but tests and CLI proofs must stay deterministic without a display.

### Pattern 3: Preserve upstream native truth
If the real keyboard APK cannot yet reach visible launch because native loading fails, the visible-launch report must keep that upstream blocker explicit rather than replacing it with a vague surface failure.

### Anti-Patterns to Avoid
- **Treating headless fallback as visible success:** headless-safe proof is useful, but it is not the same as a visible Linux window.
- **Breaking session continuity:** focus, input, window, process, and activity state should stay tied to the same real app session.
- **Hiding the native blocker:** the real keyboard APK still stops at `dlopen_failed`; Phase 5 must not blur that seam.

</architecture_patterns>

<dont_hand_roll>
## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Visible target state | A second mock display contract | Existing `window_manager` + `surface` + Wayland/EGL probe fields | Keeps one operator-facing truth path |
| Focus ownership | A brand-new event model | Existing `native_input_queue_fixture` focus ownership pattern | Already deterministic and headless-safe |
| Real activity target | A window-only synthetic component | Existing `package_manager` + `intent_resolution` + `activity_launch` data | Already verified against the keyboard APK |

**Key insight:** Phase 5 does not need a new architecture tier. It needs the existing session contracts to tell a better visible-launch story for one real app path.
</dont_hand_roll>

<common_pitfalls>
## Common Pitfalls

### Pitfall 1: Declaring visible success from a headless fallback
**What goes wrong:** Linuxoid reports a ready window contract and accidentally implies the app is visibly rendered.
**How to avoid:** Keep `headless_fallback`, live Wayland/EGL availability, and any visible-state claim distinct.

### Pitfall 2: Losing the exact native blocker
**What goes wrong:** `launch-apk --window-proof` becomes a generic `window_manager_not_ready` result and hides the real `dlopen_failed` cause.
**How to avoid:** Preserve the upstream native blocker in the window-proof and first-app-start surfaces.

### Pitfall 3: Focus without session identity
**What goes wrong:** input ownership gets proved against a generic window but not the real `SettingsActivity` session.
**How to avoid:** bind focus/input artifacts to the same package/activity/process/window IDs already resolved for the verification target.

</common_pitfalls>

## Validation Architecture

Phase 5 validation should stay deterministic and CLI-driven:

- **Quick path:** `cmake --build build`
- **Full path:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Visible-launch seam smoke:** `compatctl launch-apk --window-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>`
- **Optional live display smoke:** `compatctl native-wayland-surface-fixture <session-root> [width] [height]`

Critical assertions for this phase:

- the resolved real `SettingsActivity` session is bound to a surface/window target explicitly
- headless-safe and live-Wayland availability remain distinguishable
- the real keyboard APK path preserves the exact upstream native blocker when visible launch cannot proceed yet

<open_questions>
## Open Questions

- Can Linuxoid materialize a stronger surface target for the real `SettingsActivity` session before native loading succeeds, or should the visible-launch checkpoint remain blocked-but-descriptive on that path?
- Which report should own focus/input target truth most directly: `window_manager`, `surface`, or `first_android_app_start`?
- Is the right first visible milestone a real Wayland-created target, or a richer Linuxoid-owned surface contract that becomes visible only when the host display is available?

</open_questions>

---

*Phase: 05-visible-wayland-interaction*
*Research completed: 2026-05-18*
