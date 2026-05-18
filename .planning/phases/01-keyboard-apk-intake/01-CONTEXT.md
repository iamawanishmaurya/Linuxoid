# Phase 1: Keyboard APK Intake - Context

**Gathered:** 2026-05-18
**Status:** Ready for planning

<domain>
## Phase Boundary

Linuxoid must ingest `/home/astra/Downloads/keyboard-0.1.28.apk` as a real APK and surface trustworthy package, launcher, permission, resource, and native-library metadata through the existing direct-session path. This phase is about intake and staging, not full activity execution.

</domain>

<decisions>
## Implementation Decisions

### Real APK target
- **D-01:** Use `/home/astra/Downloads/keyboard-0.1.28.apk` as the real verification target for this phase.
- **D-02:** Treat `org.futo.inputmethod.latin` and `org.futo.inputmethod.latin.uix.settings.SettingsActivity` as the authoritative package/activity identities when metadata extraction succeeds.

### Runtime boundary
- **D-03:** Stay inside Linuxoid's native direct APK path (`compatctl launch-apk`, `inspect-apk-resources`, `inspect-apk-permissions`, and related staged-session reports). Do not introduce Waydroid, emulator, ADB, Android SDK, or Gradle dependencies for the runtime path.
- **D-04:** Keep results honest. If the real APK stops at binary manifest parsing, resource decoding, or another intake seam, report the exact blocker instead of manufacturing fake readiness.

### Intake strategy
- **D-05:** Extend the existing archive/package/launch code rather than adding a new top-level intake stack.
- **D-06:** Prioritize binary AndroidManifest metadata extraction, package/activity/permission inventory, ABI/native-library inventory, and deterministic staging/report artifacts before deeper managed execution work.
- **D-07:** Prefer x86_64 Linux host relevance first when choosing what native-library inventory to highlight for the verification target.

### Verification shape
- **D-08:** Reuse the existing `compatctl` surfaces for proof and regression coverage instead of inventing a parallel verification command for this phase.
- **D-09:** Keep deterministic offline tests with local generated APK-like fixtures, then add an opt-in smoke path for the real keyboard APK when present on disk.

### the agent's Discretion
- Exact helper/function extraction inside the existing launch and archive code
- Whether to add a focused real-APK intake report surface or keep everything under existing JSON
- Exact artifact naming as long as it stays under the existing staged session layout

</decisions>

<specifics>
## Specific Ideas

- The current direct launch path already carries package, activity, storage, permissions, DEX, runtime, and first-app-start reporting. Phase 1 should push the real keyboard APK through those existing seams as far as intake honestly allows.
- The current code appears to expect plain-text `AndroidManifest.xml` in at least one path. The real keyboard APK is likely to stop at binary manifest decoding unless Linuxoid grows a minimal parser for the needed metadata.
- `apktool` decoding is useful as a local reference during development, but it should not become a runtime dependency for Linuxoid's normal intake path.

</specifics>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project requirements and milestone scope
- `.planning/PROJECT.md` — Product target, verification APK, and anti-emulator constraints
- `.planning/REQUIREMENTS.md` — Phase-1 requirements `APK-01`, `APK-02`, and `APP-03`
- `.planning/ROADMAP.md` — Phase 1 goal, success criteria, and plan slots
- `.planning/STATE.md` — Current milestone position and blockers

### Prior research
- `.planning/research/SUMMARY.md` — First real app execution summary and verification-target framing
- `.planning/research/FEATURES.md` — Extracted verification APK capabilities and Android components
- `.planning/research/PITFALLS.md` — Known runtime and integration traps

### Existing repo truth
- `README.md` — Current `compatctl` direct APK surfaces and first-app-start checkpoint story
- `docs/steps.md` — Chronological record of the existing APK/session/runtime proof slices

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/main.cpp` — Existing `compatctl` command routing for `launch-apk`, `inspect-apk-resources`, `inspect-apk-permissions`, and first-app-start proofs
- `src/apk_native_launch.cpp` — Central direct-session intake/launch path where manifest parsing, staging, native-library selection, and reporting already converge
- `include/wfa/apk_native_launch.hpp` — Launch/session report contract that Phase 1 should extend instead of replacing
- `src/apk_dex_bridge.cpp` — Existing staged DEX handling and first managed-execution checkpoint infrastructure

### Established Patterns
- Linuxoid already prefers staged session artifacts plus JSON reports under deterministic package/session roots
- Self-Healing Android Device diagnostics already flow through the direct APK path; real-APK intake blockers should fit that same reporting style
- Existing tests concentrate in `tests/test_main.cpp`, so Phase 1 regressions should land there unless a helper split becomes unavoidable

### Integration Points
- Manifest/package metadata extraction currently lives in the direct launch path and is the likeliest place to bridge binary AndroidManifest handling
- Resource/asset inventory already has an operator-facing surface via `inspect-apk-resources`
- Permission inventory already has an operator-facing surface via `inspect-apk-permissions`

</code_context>

<deferred>
## Deferred Ideas

- Full launcher activity execution for the real keyboard app — Phase 2 and beyond
- JNI/native library loading through the real app path — Phase 4
- Visible Wayland interaction for the verification target — Phase 5
- Full IME registration/system integration behavior on Linux desktop — outside Phase 1

</deferred>

---

*Phase: 01-keyboard-apk-intake*
*Context gathered: 2026-05-18*
