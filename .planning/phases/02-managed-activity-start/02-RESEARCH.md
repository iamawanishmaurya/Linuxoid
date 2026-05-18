# Phase 2: Managed Activity Start - Research

**Researched:** 2026-05-18
**Domain:** Real activity resolution, DEX/class loading seams, first-app-start proof on `keyboard-0.1.28.apk`
**Confidence:** MEDIUM

<user_constraints>
## User Constraints

### Locked Decisions
- Use `/home/astra/Downloads/keyboard-0.1.28.apk` as the real verification target.
- Stay on Linuxoid's direct runtime path. No Waydroid, emulator, ADB, Android SDK, Gradle, network, or live-display dependency for the normal runtime path.
- Keep execution blockers explicit instead of overstating readiness.
- Continue the execution-first strategy: the next slice must move one real activity start path forward, not broaden compatibility work.

### the agent's Discretion
- Whether the real activity-start checkpoint advances through `launch-apk --first-app-start-proof`, `--activity-proof`, or a shared lower-level helper as long as `compatctl` remains the operator surface.
- Exact artifact field names for new managed activity diagnostics, provided they stay deterministic and honest.

### Deferred Ideas (OUT OF SCOPE)
- Fixing all native library loading/JNI compatibility in the same phase.
- Visible Wayland interaction for the keyboard target.
- System-wide IME enablement or real keyboard typing.

</user_constraints>

<architectural_responsibility_map>
## Architectural Responsibility Map

Single-tier native runtime ownership:
- `src/main.cpp` owns the CLI entrypoints.
- `src/apk_native_launch.cpp` owns staged session, first-app-start proof assembly, and report JSON.
- `src/apk_dex_bridge.cpp` owns real DEX table parsing, method lookup, and the minimal interpreter/probe seam.
- `tests/test_main.cpp` owns the deterministic regression surface.

</architectural_responsibility_map>

<research_summary>
## Summary

The real keyboard APK already clears Phase 1 intake. Linuxoid can now decode the binary manifest, identify package `org.futo.inputmethod.latin`, and list the declared activity `org.futo.inputmethod.latin.uix.settings.SettingsActivity`. A guarded real run of:

`timeout 20 ./build/compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk /tmp/linuxoid-phase2-first-start`

returns quickly and proves the current seam:

- `launcher_component`: `org.futo.inputmethod.latin/.uix.settings.SettingsActivity`
- `launch_status`: `libraries_failed_to_load`
- `first_android_app_start.activity_name`: `org.futo.inputmethod.latin.uix.settings.SettingsActivity`
- `first_android_app_start.lifecycle_method_name`: `onCreate`
- `first_android_app_start.runtime_state`: `unavailable`
- `first_android_app_start.dex_state`: `dex_unavailable`
- `first_android_app_start.dex_parse_state`: `entrypoint_lookup_unresolved`
- `first_android_app_start.bytecode_execution_state`: `entrypoint_missing`
- `first_android_app_start.class_loading_state`: `not_attempted`
- `first_android_app_start.blocking_reason`: `surface_not_ready_for_first_app_start`

That is the useful Phase 2 seam. Linuxoid already knows which real activity it wants, but the real managed-start proof still collapses before class loading and lifecycle method lookup on the actual APK because native launch readiness blocks the later contracts.

**Primary recommendation:** Phase 2 should decouple the real launcher activity's managed-start proof from the earlier native launch blocker just enough to (1) bind `SettingsActivity` into first-app-start explicitly, (2) resolve the real DEX class and lifecycle entrypoint or emit an exact DEX/class blocker, and (3) report that boundary through the existing proof JSON without pretending the full app actually started.
</research_summary>

<standard_stack>
## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Existing `wfa/apk_native_launch` path | repo-local | First-app-start proof assembly and staged session truth | Already owns the report fields Phase 2 must deepen |
| Existing `wfa/apk_dex_bridge` path | repo-local | Real DEX parsing, method lookup, minimal interpreter/probe | Already powers the fixture bytecode checkpoints |
| Existing `tests/test_main.cpp` | repo-local | Deterministic CLI and JSON regression coverage | Existing first-app-start proof tests already live here |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Existing `apk_loader` / `apk_archive` code | repo-local | Real APK staging and ZIP/manifest truth | Reuse for staged `classes.dex` and component metadata |
| Existing `apk_activity_launch_bridge` code | repo-local | Real component resolution and activity contract state | Reuse for the `SettingsActivity` target identity |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Extending `first-app-start` for the real activity | A separate one-off `inspect-keyboard-activity` path | Would fork runtime truth and make later execution work harder to trust |
| Explicit real activity DEX lookup with exact blockers | Broad native/JNI loading work first | That would solve a later phase's problem before proving whether the managed activity-start seam is otherwise wired correctly |
</standard_stack>

<architecture_patterns>
## Architecture Patterns

### Pattern 1: Real target identity first
Resolve the manifest-selected activity class and thread that exact class through DEX lookup, lifecycle method selection, and JSON reporting before trying to broaden execution behavior.

### Pattern 2: Managed proof can be narrower than native launch
The real app may still fail plain launch at `libraries_failed_to_load`, but the proof path can still inspect and report the real activity-start boundary if it is careful not to claim app startup happened.

### Pattern 3: Exact blocker propagation
Keep `entrypoint_lookup_unresolved`, `class_loading_state`, and future unsupported-opcode or missing-method seams exact so later phases know whether to work on DEX lookup, runtime context, or JNI/native loading next.

### Anti-Patterns to Avoid
- **Pretending native-launch readiness equals managed activity readiness:** the current run disproves that.
- **Skipping the real target class:** Phase 2 must not keep reporting only fixture `MainActivity` seams when the real APK already names `SettingsActivity`.
- **Solving JNI loading by stealth in this phase:** Phase 4 owns native loading; Phase 2 should only do the smallest amount of work needed to expose the real activity-start seam.

</architecture_patterns>

<dont_hand_roll>
## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Real activity identity | A second manifest/component resolver | Existing package manager + intent resolution + activity launch data | The real APK already produces these fields |
| DEX entrypoint proof | A new sidecar DEX inspector | Existing `apk_dex_bridge` execution probe structs and report plumbing | Keeps the first-app-start contract authoritative |
| Regression surface | A new test binary | Existing `compatctl launch-apk --first-app-start-proof` and `tests/test_main.cpp` | Later execution work already builds on this path |

**Key insight:** the smallest successful Phase 2 is not "run the whole app." It is "name the real `SettingsActivity` startup seam truthfully from staged DEX and prove exactly where it stops."
</dont_hand_roll>

<common_pitfalls>
## Common Pitfalls

### Pitfall 1: Activity identity drift
**What goes wrong:** JSON or tests keep referring to the fixture `MainActivity` path after the real APK activity has already been resolved.
**How to avoid:** Thread the resolved `SettingsActivity` component/class through the same first-app-start report fields the fixture already uses.

### Pitfall 2: DEX parse ambiguity
**What goes wrong:** Phase 2 stops at "dex unavailable" without distinguishing missing staged DEX, unresolved class descriptor, missing lifecycle method, or malformed code item.
**How to avoid:** Split the real activity-start seam into exact states like class resolved, method resolved, code item resolved, or exact blocker.

### Pitfall 3: Hidden native blocker regression
**What goes wrong:** managed-start reporting "passes" only because the earlier `libraries_failed_to_load` blocker was accidentally bypassed or suppressed.
**How to avoid:** keep `launch_status`, native execute diagnostics, and managed-start blocker reporting side by side in the same proof output.

</common_pitfalls>

## Validation Architecture

Phase 2 validation should stay deterministic and CLI-driven:

- **Fast path:** `cmake --build build`
- **Full path:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Real target smoke:** `timeout 20 ./build/compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>`

The critical assertions for this phase are report-structure assertions, not "the app visually runs":
- real package and activity identity appear in first-app-start JSON
- DEX/class loading state is more precise than `not_attempted` when the managed proof path reaches the real activity seam
- blockers remain explicit if bytecode or runtime context still cannot advance

<open_questions>
## Open Questions

- Can Phase 2 resolve the real `SettingsActivity.onCreate` DEX entrypoint without requiring native libraries to load first?
- Is the first exact real-APK DEX blocker a class-descriptor mapping issue, missing lifecycle method resolution, or code-item lookup gap?
- Which existing fixture-first-app-start tests are the closest analogs for new keyboard-APK regression coverage?

</open_questions>

---

*Phase: 02-managed-activity-start*
*Research completed: 2026-05-18*
