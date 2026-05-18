---
phase: 9
slug: managed-app-start-dispatch
status: draft
created: 2026-05-20
---

# Phase 9: Managed App-Start Dispatch - Research

**Researched:** 2026-05-20  
**Domain:** Real keyboard APK post-bridge startup through Linuxoid's direct runtime path  
**Confidence:** MEDIUM

<user_constraints>
## User Constraints

### Locked Decisions
- Stay on Linuxoid's direct runtime path. No Waydroid, emulator, ADB, Android SDK, Gradle, network, or live-display dependency for the normal runtime path.
- Keep the work execution-first and narrow: get the real keyboard APK one seam closer to running instead of broad architecture expansion.
- Preserve `/home/astra/Downloads/keyboard-0.1.28.apk` as the verification target.
- Keep blockers explicit. If the app still does not start, Linuxoid must report the exact post-bridge dispatch, JNI registration, or managed bootstrap seam.

### The Agent's Discretion
- Exact shape of the Linuxoid-managed dispatch bridge after `JNI_OnLoad`, as long as it stays deterministic and local to the existing `launch-apk` / `--first-app-start-proof` path.
- Whether the first post-bridge seam is phrased as dispatch, registration, or managed bootstrap, as long as the JSON stays honest and stable.

### Deferred Ideas (OUT OF SCOPE)
- Broad Android framework recreation beyond the first post-bridge dispatch seam.
- Full IME integration, typing into host apps, or UI polish.
- Broad third-party compatibility expansion.

</user_constraints>

<research_summary>
## Summary

Phase 8 moved the real keyboard APK past the older `native_activity_entrypoint_missing` seam. A guarded live run now reaches:

- `native_loading_state: "linuxoid_managed_app_start_bridge_required"`
- `native_jni_state: "called"`
- `native_app_start_bridge_state: "linuxoid_managed_app_start_bridge_required"`
- `native_post_jni_startup_state: "managed_activity_dispatch_required"`
- `native_loading_library_name: "libjni_latinime.so"`
- `blocking_reason: "linuxoid_managed_app_start_bridge_required_for_first_app_start:libjni_latinime.so"`
- `next_blocker: "implement_linuxoid_managed_app_start_bridge_for_libjni_latinime_so"`

This tells us the next narrow move:

1. Linuxoid already has a trustworthy JNI-shaped primary library and a selected activity target.
2. The next work is not generic native loading anymore; it is the first Linuxoid-owned handoff from that JNI-shaped library into app-start dispatch.
3. The later managed seam is still the same downstream truth:
   - `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

**Primary recommendation:** Phase 9 should implement the smallest deterministic post-bridge dispatch step possible, then expose the first exact seam after that handoff: JNI registration, managed bootstrap dependency, or managed runtime dispatch.
</research_summary>

<standard_stack>
## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Existing `wfa/native_execute_stub` path | repo-local | Primary-library selection, `JNI_OnLoad`, app-start bridge state, and native seam reporting | Already owns the bridge seam we need to advance |
| Existing `wfa/apk_native_launch` path | repo-local | Real APK launch JSON and first-app-start proof surfaces | Already exposes the live keyboard blocker and should remain authoritative |
| Existing staged DEX/class lookup path | repo-local | Real activity and lifecycle-method resolution for `SettingsActivity` | Already keeps the downstream managed seam visible |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Existing runtime-health/watchdog helpers | repo-local | Keep Self-Healing Android Device wording rooted in the earliest post-bridge blocker | Use when the new seam changes recovery diagnostics |
| Existing real keyboard APK smoke path | local artifact | Manual verification against the true target APK | Use only for opt-in smoke, not CI gating |

</standard_stack>

<architecture_patterns>
## Architecture Patterns

### Pattern 1: One authoritative launch path
Keep new post-bridge dispatch truth flowing through `launch-apk` and `--first-app-start-proof`.

### Pattern 2: Native-to-managed seam stays ordered
Move from:
- library loaded
- `JNI_OnLoad` called
- Linuxoid-managed bridge selected
- first dispatch step attempted
- first exact post-bridge blocker exposed
- later `Activity.onCreate(Bundle)` seam still visible

### Pattern 3: Deterministic bridge before framework recreation
If Linuxoid needs a placeholder or stub dispatch contract, keep it explicit, deterministic, and narrow to the keyboard APK path.

### Anti-Patterns to Avoid
- **Do not widen into ActivityThread recreation yet:** Phase 9 owns the first post-bridge dispatch seam first.
- **Do not bypass the real APK path:** synthetic fixtures can pin the seam, but the real keyboard APK remains the source of truth.
- **Do not collapse dispatch and managed-runtime seams together:** keep the new post-bridge seam distinct from the later managed `Activity.onCreate(Bundle)` seam.

</architecture_patterns>

<common_pitfalls>
## Common Pitfalls

### Pitfall 1: Treating bridge selection as launch success
Selecting the Linuxoid-managed bridge is not the same as dispatching app start.

### Pitfall 2: Overclaiming managed progress
If Linuxoid only reaches a JNI registration or managed bootstrap dependency, report that exact seam instead of calling it a started app.

### Pitfall 3: Letting watchdog or surface reporting blur the seam
The earliest blocker still needs to stay anchored in the post-bridge dispatch boundary.

</common_pitfalls>

## Validation Architecture

Phase 9 validation should stay deterministic and CLI-driven:

- **Quick path:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build`
- **Full path:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure`
- **Real keyboard smoke:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>`

Critical assertions:

- Linuxoid reaches a smaller seam than `linuxoid_managed_app_start_bridge_required`
- `native_app_start_bridge_*`, `native_post_jni_startup_state`, `blocking_reason`, and `next_blocker` stay deterministic
- the downstream managed blocker remains distinct after the bridge seam moves

---

*Phase: 09-managed-app-start-dispatch*  
*Research completed: 2026-05-20*
