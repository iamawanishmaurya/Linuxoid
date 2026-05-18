---
phase: 4
slug: jni-and-native-loading
status: draft
created: 2026-05-18
---

# Phase 4: JNI and Native Loading - Research

**Researched:** 2026-05-18  
**Domain:** Real keyboard APK `x86_64` native-library staging, `dlopen`, `JNI_OnLoad`, and exact blocker reporting  
**Confidence:** MEDIUM

<user_constraints>
## User Constraints

### Locked Decisions
- Stay on Linuxoid's direct runtime path. No Waydroid, emulator, ADB, Android SDK, Gradle, network, or live-display dependency for the normal runtime path.
- Keep the execution-first strategy: this phase should narrow the real keyboard APK native/JNI seam instead of broadening Android-framework scope.
- Preserve the real verification target `/home/astra/Downloads/keyboard-0.1.28.apk`.
- Keep blockers explicit instead of faking native-load success, JNI readiness, or ART/framework ownership.

### the agent's Discretion
- Whether the first exact seam is exposed through `launch-apk` alone or also reflected in `--first-app-start-proof`, as long as `compatctl` remains the operator surface.
- Exact new JSON field names for per-library load attempts, JNI status, or recovery diagnostics, provided they stay deterministic and truthful.

### Deferred Ideas (OUT OF SCOPE)
- Solving visible Wayland interaction in this phase.
- Broad ActivityThread or full Android framework recreation.
- Full JNI environment completeness beyond the minimal native-load seam needed to expose the next blocker honestly.

</user_constraints>

<architectural_responsibility_map>
## Architectural Responsibility Map

Single-tier native/JNI ownership:
- `src/main.cpp` owns the CLI entrypoints.
- `src/apk_native_launch.cpp` owns staged session truth, launch report assembly, first-app-start proof assembly, and blocker propagation.
- `src/native_execute_stub.cpp` owns native-library candidate ordering, `dlopen`, `JNI_OnLoad`, entrypoint lookup, and exact native-execution failure seams.
- `src/runtime_health.cpp` owns higher-level native/JNI health classification when launch facts need to feed recovery output.
- `tests/test_main.cpp` owns deterministic regression coverage for CLI and JSON proof surfaces.

</architectural_responsibility_map>

<research_summary>
## Summary

The real verification target has now cleared manifest, package, permissions, assets, resources, and ABI inventory. A guarded live smoke against `/home/astra/Downloads/keyboard-0.1.28.apk` currently returns:

- `package_name: "org.futo.inputmethod.latin"`
- `selected_abi: "x86_64"`
- `launcher_component: "org.futo.inputmethod.latin/.uix.settings.SettingsActivity"`
- six staged `x86_64` native libraries:
  - `libandroidx.graphics.path.so`
  - `libdatastore_shared_counter.so`
  - `libjni_latinime.so`
  - `libmozc.so`
  - `librime-jni.so`
  - `libvad_jni.so`
- `jni_onload_called: false`
- `launch_status: "libraries_failed_to_load"`
- `recommended_recovery_action: "inspect_native_launch_diagnostics"`

That means the next narrow seam is no longer APK intake or managed activity lookup. It is native library loading itself:

1. Linuxoid stages the correct host-ABI payloads.
2. Linuxoid does **not** yet get any staged keyboard library successfully through `dlopen`.
3. Linuxoid therefore never reaches `JNI_OnLoad`, entrypoint lookup, or a later JNI-backed blocker on the real APK path.

Phase 4 should not broaden into ART or UI work. It should make one native/JNI blocker smaller by:

- exposing deterministic per-library load attempts and failure reasons,
- keeping candidate ordering and selected-library truth visible,
- distinguishing "all `dlopen` attempts failed" from "`JNI_OnLoad` missing or failed" from "entrypoint missing after load",
- preserving the already-narrowed managed `SettingsActivity.onCreate(Landroid/os/Bundle;)V` seam so later phases still know what comes after the upstream native blocker.

**Primary recommendation:** Phase 4 should first turn `libraries_failed_to_load` into a smaller, exact native-load seam for the real keyboard APK, then thread that exact native/JNI state through the first-app-start and recovery surfaces without claiming native success where none exists.
</research_summary>

<standard_stack>
## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Existing `wfa/apk_native_launch` path | repo-local | Real APK staging, session truth, and operator-facing JSON | Already owns the `libraries_failed_to_load` blocker and the keyboard APK smoke path |
| Existing `wfa/native_execute_stub` path | repo-local | Candidate library ordering, `dlopen`, `JNI_OnLoad`, and entrypoint lookup | Already owns the exact native-load seam that must be narrowed |
| Existing `tests/test_main.cpp` | repo-local | Deterministic CLI and JSON regression coverage | Already pins both fixture-native success and negative native-load paths |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Existing runtime-health helpers | repo-local | Native/JNI health classification and recovery wording | Use when exact native blockers need to feed higher-level health output |
| Existing real keyboard APK smoke path | local artifact | Live verification target for manual/native smoke | Use only for manual smoke, not mandatory CI |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|---------|---------|---------|
| Extending current native-execute diagnostics | Adding a separate keyboard-only loader tool | Would split the execution truth away from `launch-apk` |
| Narrow Phase 4 native/JNI scope | Jump straight to Wayland or broader managed-runtime work | Would skip the exact blocker the real APK is already surfacing |

</standard_stack>

<architecture_patterns>
## Architecture Patterns

### Pattern 1: Upstream blocker first
The real keyboard APK currently stops at native loading before managed startup can continue. Phase 4 should narrow that upstream seam first instead of routing around it.

### Pattern 2: Per-library deterministic truth
When multiple staged `.so` files are present, Linuxoid should report deterministic candidate order, exact load attempts, and exact failure or success state for each step.

### Pattern 3: Native/JNI truth without false success
If no library loads, Linuxoid should say so precisely. If a library loads but `JNI_OnLoad` is missing or fails, Linuxoid should say that instead. If JNI succeeds but an entrypoint is missing, Linuxoid should keep that as the next exact blocker.

### Anti-Patterns to Avoid
- **Mixing Wayland work into the phase:** surface readiness is still real, but Phase 4 owns native/JNI first.
- **Hiding the managed seam:** the stubbed `Activity.onCreate(Bundle)` boundary should remain visible for downstream work.
- **Adding a second native reporting path:** `launch-apk` and its first-app-start companion should remain authoritative.

</architecture_patterns>

<dont_hand_roll>
## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Real keyboard native loader truth | A keyboard-only sidecar loader | Existing `launch-apk` + `native_execute` report path | Keeps the blocker in the same operator surface users already run |
| JNI/native recovery output | A second ad hoc diagnostics format | Existing launch JSON, runtime-health, and recovery fields | Keeps one machine-readable truth |
| Verification surface | A new standalone test harness | Existing `tests/test_main.cpp` fixture/native launch coverage | Keeps the phase grounded in the established CLI path |

**Key insight:** Phase 4 does not need more architecture. It needs better truth about why the real keyboard APK's staged `x86_64` libraries fail before JNI even begins.
</dont_hand_roll>

<common_pitfalls>
## Common Pitfalls

### Pitfall 1: Generic native failure collapse
**What goes wrong:** Linuxoid still reports only `libraries_failed_to_load`, leaving no clue which library failed first or why.
**How to avoid:** Record deterministic candidate order and exact `dlopen`/JNI status details in the existing report.

### Pitfall 2: Losing managed-start truth while fixing native load
**What goes wrong:** Native/JNI work masks or removes the already-known managed Bundle-boundary seam.
**How to avoid:** Preserve the managed `first_android_app_start` seam as a secondary truth even when the upstream native blocker remains.

### Pitfall 3: Claiming JNI progress too early
**What goes wrong:** A library gets staged and Linuxoid starts implying JNI readiness despite `JNI_OnLoad` never being called.
**How to avoid:** Keep `jni_onload_called`, entrypoint lookup, and execution readiness separate and explicit.

</common_pitfalls>

## Validation Architecture

Phase 4 validation should stay deterministic and CLI-driven:

- **Quick path:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build`
- **Full path:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure`
- **Real keyboard smoke:** `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>`

Critical assertions for this phase:

- the real keyboard APK no longer stops at only a generic `libraries_failed_to_load` reason without deeper deterministic native-load facts
- Linuxoid preserves the selected `x86_64` library set and exact native/JNI seam
- the managed `SettingsActivity.onCreate(Landroid/os/Bundle;)V` seam remains truthful and visible for downstream work

<open_questions>
## Open Questions

- Which keyboard `x86_64` library fails first under `dlopen`, and is the root cause a missing host dependency, a relocation issue, or another exact loader error?
- Once one or more libraries load, is the next seam `JNI_OnLoad` missing/failed or later entrypoint lookup?
- Should `launch-apk --first-app-start-proof` mirror the native/JNI blocker directly when the upstream native seam still prevents managed execution?

</open_questions>

---

*Phase: 04-jni-and-native-loading*  
*Research completed: 2026-05-18*
