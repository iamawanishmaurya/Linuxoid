---
phase: 8
slug: native-app-start-bridge
status: draft
created: 2026-05-19
---

# Phase 8: Native App-Start Bridge - Research

**Researched:** 2026-05-19  
**Domain:** Real keyboard APK post-`JNI_OnLoad` app-start bridge through Linuxoid's direct runtime path  
**Confidence:** MEDIUM

<user_constraints>
## User Constraints

### Locked Decisions
- Stay on Linuxoid's direct runtime path. No Waydroid, emulator, ADB, Android SDK, Gradle, network, or live-display dependency for the normal runtime path.
- Keep the work execution-first and narrow: get one real app closer to running instead of expanding broad architecture.
- Preserve the real verification target `/home/astra/Downloads/keyboard-0.1.28.apk`.
- Keep blockers explicit. If the app still does not start, Linuxoid must report the exact native, JNI registration, or managed bootstrap seam instead of pretending app launch succeeded.

### The Agent's Discretion
- Exact Linuxoid-owned app-start bridge shape for JNI-shaped libraries, as long as it stays deterministic and local to the existing `launch-apk` / `--first-app-start-proof` path.
- Whether the first post-`JNI_OnLoad` seam is expressed as registration, app-start dispatch, or managed bootstrap, as long as the JSON stays stable and honest.

### Deferred Ideas (OUT OF SCOPE)
- Broad Android framework recreation beyond the first real post-`JNI_OnLoad` seam.
- Full IME integration, typing into host apps, or desktop polish in this phase.
- Broad compatibility expansion beyond the keyboard APK verification target.

</user_constraints>

<research_summary>
## Summary

Phase 7 moved Linuxoid past the earlier `__strchr_chk` seam. A fresh guarded smoke against the real verification target now reaches the entry library and reports:

- `native_loading_state: "native_activity_entrypoint_missing"`
- `native_jni_state: "called"`
- `native_loading_library_name: "libjni_latinime.so"`
- `native_execute.execution_engine_ready: true`
- `native_execute.jni_onload_results[0].return_code: 11`
- `first_android_app_start.class_loading_state: "resolved-from-staged-dex"`
- `first_android_app_start.target_class_lookup_state: "class_resolved"`
- `first_android_app_start.target_method_lookup_state: "method_resolved"`
- `first_android_app_start.next_blocker: "provide_native_activity_entrypoint_for_libjni_latinime_so"`

This tells us three useful things:

1. The upstream Android-libc shim work is good enough for the real entry library to load.
2. `libjni_latinime.so` is JNI-shaped, not `ANativeActivity`-shaped; Linuxoid should stop treating missing `ANativeActivity_onCreate` as the final story.
3. The next work is not "more framework." It is a Linuxoid-owned app-start bridge that turns successful `JNI_OnLoad` plus manifest/DEX readiness into the next exact seam: JNI registration, startup dispatch, or the first managed bootstrap dependency.

**Primary recommendation:** Phase 8 should keep the direct `launch-apk --first-app-start-proof` path authoritative, add the smallest Linuxoid-owned bridge for a JNI-shaped primary library, and expose the first post-`JNI_OnLoad` seam exactly. The downstream managed blocker `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context` should remain visible as the next later seam, not be replaced by generic native wording.
</research_summary>

<standard_stack>
## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Existing `wfa/native_execute_stub` path | repo-local | Candidate selection, compatibility preload, `dlopen`, `JNI_OnLoad`, and native boundary reporting | Already owns the exact post-`JNI_OnLoad` seam we need to move |
| Existing `wfa/apk_native_launch` path | repo-local | Real APK launch JSON and first-app-start proof surfaces | Already exposes the keyboard APK blocker and should remain authoritative |
| Existing staged DEX/class lookup path | repo-local | Real activity and method resolution for `SettingsActivity` | Already keeps the downstream managed seam explicit |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Existing runtime-health/watchdog helpers | repo-local | Keep the Self-Healing Android Device wording rooted in the earliest blocker | Use when new app-start truth needs to feed recovery diagnostics |
| Existing real keyboard APK smoke path | local artifact | Manual verification against the true target APK | Use only for opt-in smoke, not CI gating |

</standard_stack>

<architecture_patterns>
## Architecture Patterns

### Pattern 1: One authoritative launch path
Keep new app-start bridge truth flowing through `launch-apk` and `--first-app-start-proof` instead of inventing a second launcher or separate debug tool.

### Pattern 2: Native-to-managed seam stays ordered
Move from:
- library loaded
- `JNI_OnLoad` called
- Linuxoid-owned app-start bridge selected
- first post-`JNI_OnLoad` registration/dispatch/bootstrap seam exposed
- later managed `Activity.onCreate(Bundle)` seam still visible

### Pattern 3: Deterministic bridge before real framework recreation
If Linuxoid needs a placeholder or stub bridge, make it explicit, deterministic, and narrow to the real keyboard app-start path.

### Anti-Patterns to Avoid
- **Do not widen into generic framework work:** Phase 8 owns the post-`JNI_OnLoad` app-start seam first.
- **Do not collapse back to missing entrypoint:** once `JNI_OnLoad` succeeds, Linuxoid should report a smaller seam than generic `native_activity_entrypoint_missing`.
- **Do not bypass the real APK path:** synthetic fixtures can pin the seam, but the real keyboard APK remains the source of truth.

</architecture_patterns>

<common_pitfalls>
## Common Pitfalls

### Pitfall 1: Treating every loaded JNI library like a NativeActivity
The current live blocker already proves `libjni_latinime.so` is not shaped like `ANativeActivity_onCreate`.

### Pitfall 2: Overclaiming managed progress
Successful `JNI_OnLoad` is not the same as successful app start. Registration, dispatch, or managed runtime dependencies must stay explicit.

### Pitfall 3: Letting watchdog/window surfaces blur the seam
The earliest blocker still needs to stay anchored in the post-`JNI_OnLoad` app-start boundary.

</common_pitfalls>

## Validation Architecture

Phase 8 validation should stay deterministic and CLI-driven:

- **Quick path:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build`
- **Full path:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure`
- **Real keyboard smoke:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>`

Critical assertions:

- Linuxoid reaches a smaller seam than `native_activity_entrypoint_missing`
- `native_loading_state`, `native_jni_state`, `blocking_reason`, `next_blocker`, and nested `native_execute.*` fields stay deterministic
- the downstream managed blocker remains distinct once the native app-start seam moves

---

*Phase: 08-native-app-start-bridge*  
*Research completed: 2026-05-19*
