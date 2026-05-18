---
phase: 7
slug: native-libc-and-entry-bridge
status: draft
created: 2026-05-19
---

# Phase 7: Native libc Compatibility and Entry Bridge - Research

**Researched:** 2026-05-19  
**Domain:** Real keyboard APK native-entry loading through Linuxoid's Android-libc compatibility seam  
**Confidence:** MEDIUM

<user_constraints>
## User Constraints

### Locked Decisions
- Stay on Linuxoid's direct runtime path. No Waydroid, emulator, ADB, Android SDK, Gradle, network, or live-display dependency for the normal runtime path.
- Keep the work execution-first and narrow: get one real app closer to running instead of expanding broad architecture.
- Preserve the real verification target `/home/astra/Downloads/keyboard-0.1.28.apk`.
- Keep blockers explicit. If the library still fails, Linuxoid must report the exact unresolved symbol or missing entry seam instead of claiming native success.

### The Agent's Discretion
- Exact shim/preload approach for Android-libc compatibility, as long as it stays deterministic and local to Linuxoid's native load path.
- Whether the first native seam is surfaced through `launch-apk` alone or mirrored into `--first-app-start-proof`, as long as `compatctl` remains authoritative.

### Deferred Ideas (OUT OF SCOPE)
- Broad framework recreation beyond the first real native-entry seam.
- Solving IME integration or general Linux desktop typing in this phase.
- Broad compatibility expansion beyond the keyboard APK verification target.

</user_constraints>

<research_summary>
## Summary

The first six phases got Linuxoid to a much sharper live blocker than the original roadmap anticipated. A fresh guarded smoke against the real verification target now reaches the staged entry library and reports:

- `package_name: "org.futo.inputmethod.latin"`
- `selected_abi: "x86_64"`
- `native_execute.android_compat_state: "preloaded_and_version_normalized"`
- `native_loading_state: "native_activity_entrypoint_missing"`
- `native_jni_state: "not_attempted"`
- `native_loading_library_name: "libjni_latinime.so"`
- `native_loading_detail: "...libjni_latinime.so: undefined symbol: __strchr_chk"`
- `first_android_app_start.class_loading_state: "resolved-from-staged-dex"`
- `first_android_app_start.target_class_lookup_state: "class_resolved"`
- `first_android_app_start.target_method_lookup_state: "method_resolved"`
- `first_android_app_start.next_blocker: "provide_native_activity_entrypoint_for_libjni_latinime_so"`

This tells us three useful things:

1. The earlier `libandroidx.graphics.path.so` / `invalid ELF header` seam is no longer the first blocker.
2. Linuxoid already preloads and normalizes enough Android compatibility state to reach the real entry library.
3. The next work is not "more architecture." It is a specific native load and entry seam around `libjni_latinime.so`, `__strchr_chk`, and whatever native boundary appears immediately after that symbol gap moves.

**Primary recommendation:** Phase 7 should close the first Android-libc symbol gap for the real entry library, then surface the first true post-load native boundary (`JNI_OnLoad`, registration, or explicit missing entrypoint) through the same `launch-apk` / `--first-app-start-proof` path, while preserving the already-known downstream managed blocker at `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`.
</research_summary>

<standard_stack>
## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Existing `wfa/native_execute_stub` path | repo-local | Candidate library ordering, compatibility preloading, `dlopen`, `JNI_OnLoad`, and entrypoint lookup | Already owns the exact native seam we need to move |
| Existing `wfa/apk_native_launch` path | repo-local | Real APK launch JSON and first-app-start proof surfaces | Already exposes the keyboard APK blocker and should remain authoritative |
| Existing native compat shims | repo-local | Minimal `libc.so`, `liblog.so`, `libm.so`, `libdl.so` compatibility surface for staged Android libraries | Already got Linuxoid from the older helper-library seam to the sharper `libjni_latinime.so` seam |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Existing runtime-health/watchdog helpers | repo-local | Keep the Self-Healing Android Device wording rooted in the earliest blocker | Use when native-entry truth needs to feed recovery diagnostics |
| Existing real keyboard APK smoke path | local artifact | Manual verification against the true target APK | Use only for opt-in smoke, not CI gating |

</standard_stack>

<architecture_patterns>
## Architecture Patterns

### Pattern 1: Earliest native blocker stays authoritative
If `libjni_latinime.so` fails before JNI or managed execution, Linuxoid should stop there and say exactly why.

### Pattern 2: One authoritative operator surface
`compatctl launch-apk` and `--first-app-start-proof` should stay the source of truth. Avoid spinning up a second loader tool just for native debugging.

### Pattern 3: Honest native progression
Move from:
- unresolved helper-library blocker
to:
- entry-library unresolved symbol
to:
- entry-library loaded but `JNI_OnLoad`/registration missing
to:
- entry-library loaded and next managed/native boundary exposed

Each step should be explicit and deterministic.

### Anti-Patterns to Avoid
- **Broad Android framework expansion in this phase:** Phase 7 owns the real entry-library seam first.
- **Pretending load success:** if the library still cannot resolve symbols, Linuxoid must not drift into later fake managed steps.
- **Breaking the downstream managed seam:** once the native load moves, the existing `Activity.onCreate(Bundle)` blocker still needs to stay visible.

</architecture_patterns>

<common_pitfalls>
## Common Pitfalls

### Pitfall 1: Solving the wrong native library
The real blocker is now `libjni_latinime.so`, not the older helper-library seam. Phase 7 should stay anchored there.

### Pitfall 2: Conflating unresolved symbols with missing entrypoints
`undefined symbol: __strchr_chk` is different from missing `JNI_OnLoad` or a missing activity entrypoint. The report needs to keep those states separate.

### Pitfall 3: Letting recovery or window reporting mask the native seam
The window and managed proof surfaces should preserve the upstream native truth instead of layering over it.

</common_pitfalls>

## Validation Architecture

Phase 7 validation should stay deterministic and CLI-driven:

- **Quick path:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build`
- **Full path:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure`
- **Real keyboard smoke:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>`

Critical assertions:

- Linuxoid gets past the current `__strchr_chk` unresolved-symbol boundary or reports an even smaller exact native seam
- `native_loading_state`, `native_loading_library_name`, `native_loading_detail`, and nested `native_execute` fields stay deterministic
- the downstream managed blocker remains distinct once native loading moves

---

*Phase: 07-native-libc-and-entry-bridge*  
*Research completed: 2026-05-19*
