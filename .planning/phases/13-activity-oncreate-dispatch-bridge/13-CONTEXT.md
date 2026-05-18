# Phase 13: Activity onCreate Dispatch Bridge - Context

**Gathered:** 2026-05-19
**Status:** Ready for planning

<domain>
## Phase Boundary

Linuxoid must bridge the real keyboard app's `SettingsActivity->onCreate(Landroid/os/Bundle;)V` lifecycle call into the already bound Linuxoid runtime context and surface the first exact post-dispatch blocker. This phase is about real managed lifecycle dispatch for the verification APK, not yet full visible launch or IME behavior.

</domain>

<decisions>
## Implementation Decisions

### Verification target
- **D-01:** Keep `/home/astra/Downloads/keyboard-0.1.28.apk` as the authoritative verification APK.
- **D-02:** Keep `org.futo.inputmethod.latin/.uix.settings.SettingsActivity` as the authoritative component for this phase.

### Runtime boundary
- **D-03:** Stay inside Linuxoid's direct `compatctl launch-apk` path and the existing staged session contracts.
- **D-04:** Do not introduce emulator, Waydroid, Android SDK, Gradle, or network-dependent detours.
- **D-05:** Keep results honest. If lifecycle dispatch moves forward but startup still fails, report the first exact post-dispatch blocker instead of claiming visible launch success.

### Dispatch strategy
- **D-06:** Reuse the existing `linuxoid_runtime_context_bound` checkpoint rather than inventing a second managed-context abstraction.
- **D-07:** Keep runtime-context binding, `Activity.onCreate(Bundle)` dispatch, and later framework/resource/window seams distinct in JSON and watchdog output.
- **D-08:** Prefer the smallest real implementation slice that advances the keyboard activity, even if later Android framework behavior remains explicit and missing.

### Verification shape
- **D-09:** Reuse `launch-apk`, `launch-apk --first-app-start-proof`, `launch-apk --window-proof`, and `launch-apk --self-heal-proof` as the operator surfaces for this phase.
- **D-10:** Keep deterministic offline tests in `tests/test_main.cpp`, then use the local keyboard APK as the live smoke path.

### the agent's Discretion
- Exact helper extraction inside the existing native execute, DEX bridge, and launch reporting code
- Exact naming for any new post-dispatch JSON fields, as long as the seam remains machine-readable
- Whether the next blocker is surfaced first through launch JSON, first-app-start JSON, window JSON, or all three together

</decisions>

<specifics>
## Specific Ideas

- The real keyboard path already reaches `linuxoid_runtime_context_bound`; this phase should spend that progress by attempting the actual `Activity.onCreate(Bundle)` dispatch instead of adding more pre-dispatch reports.
- The current fixture path already proves a stubbed `Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V` boundary in the minimal interpreter; that pattern should guide the real keyboard dispatch attempt.
- If dispatch succeeds but the app still cannot continue, Phase 13 should expose the next exact boundary in a way Phase 14 can target directly for visible launch work.

</specifics>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Milestone scope
- `.planning/PROJECT.md` — Current milestone `v1.1 Visible App Launch`
- `.planning/REQUIREMENTS.md` — Phase 13 requirements `JNI-15`, `JNI-16`, and `VER-10`
- `.planning/ROADMAP.md` — Phase 13 goal and success criteria
- `.planning/STATE.md` — Current blocker and milestone focus

### Research
- `.planning/research/SUMMARY.md` — Milestone research summary
- `.planning/research/ARCHITECTURE.md` — Build order and integration seam
- `.planning/research/PITFALLS.md` — Likely failure modes after dispatch moves

### Existing repo truth
- `README.md` — Current keyboard APK blocker, window proof, and first-app-start truth
- `docs/steps.md` — Latest checkpoint chronology
- `src/project_status.cpp` — Operator-facing current runtime phase summary

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/native_execute_stub.cpp` — Current bound runtime-context checkpoint and native-launch seam narrowing
- `src/apk_dex_bridge.cpp` — Existing managed execution, receiver, bundle placeholder, and framework-boundary reporting
- `src/apk_native_launch.cpp` — Top-level launch, first-app-start, and window/runtime/report aggregation
- `src/apk_window_bridge.cpp` — Visible target and host-surface truth
- `src/apk_self_healing_watchdog.cpp` — Earliest-blocker authority and recovery wording

### Established Patterns
- Narrow one real blocker at a time
- Mirror exact seams into both top-level launch JSON and nested first-app-start JSON
- Keep live smoke and offline fixture coverage aligned on the same blocker vocabulary

### Integration Points
- Lifecycle dispatch attempts belong in the current native execute and DEX bridge path
- Post-dispatch truth should flow through launch JSON, window proof, and watchdog output
- Phase 14 will consume whatever post-dispatch seam this phase surfaces

</code_context>

<deferred>
## Deferred Ideas

- Visible settings surface creation — Phase 14
- Focus and usable interaction — Phase 15
- System-wide IME enablement and input-method behavior — future milestone
- Broad Android framework parity — future work after one visible launch path exists

</deferred>

---

*Phase: 13-activity-oncreate-dispatch-bridge*
*Context gathered: 2026-05-19*
