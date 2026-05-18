# Requirements: Linuxoid

**Defined:** 2026-05-18
**Core Value:** Run a real Android app directly on Linux through Linuxoid's own compatibility/runtime path, with honest execution and honest blockers instead of emulator fallback.

## v1 Requirements

Requirements for the first real Android app execution milestone centered on `keyboard-0.1.28.apk`.

### APK Intake

- [ ] **APK-01**: User can point Linuxoid at a local APK path and Linuxoid extracts the package name, launcher component, native ABI inventory, permissions, and staged artifact roots for the app
- [ ] **APK-02**: User can stage `keyboard-0.1.28.apk` into a Linuxoid-owned sandbox/session root without emulator or Waydroid dependencies
- [ ] **APK-03**: User can launch the verification APK through `compatctl launch-apk --first-app-start-proof` and receive a structured first-app-start report

### Managed Execution

- [ ] **DEX-01**: Linuxoid can decode the verification APK's real `classes.dex` well enough to resolve the launcher activity class and its startup lifecycle method
- [ ] **DEX-02**: Linuxoid can execute enough real app bytecode to move beyond minimal helper proofs and advance the launcher activity startup path through method invocation, object/register/field handling, and lifecycle receiver state
- [ ] **DEX-03**: Linuxoid reports the exact bytecode backend, invoked lifecycle method, execution state, and first unsupported opcode or framework boundary whenever startup is not complete

### Native and JNI

- [ ] **JNI-01**: Linuxoid stages the verification APK's `x86_64` native libraries into the launch session and selects the correct host ABI path
- [ ] **JNI-02**: Linuxoid reports exact native load or JNI blockers instead of silently falling back or claiming success
- [ ] **JNI-03**: Linuxoid satisfies enough Android-libc compatibility for the verification APK's primary native entry library to load through the host loader instead of stopping at unresolved symbols such as `__strchr_chk`
- [ ] **JNI-04**: Linuxoid reaches and reports the first true native entry boundary for the verification APK's primary library, including `JNI_OnLoad`, registration, or a specific missing entrypoint or symbol seam
- [ ] **JNI-05**: Linuxoid can bridge a JNI-shaped primary library into a Linuxoid-owned app-start strategy when the library loads and `JNI_OnLoad` succeeds but no `ANativeActivity_onCreate` entrypoint exists
- [ ] **JNI-06**: Linuxoid reports exact JNI registration, native app-start, or managed bootstrap blockers after `JNI_OnLoad` instead of collapsing back to generic missing-entrypoint failure

### Window and Input

- [ ] **WIN-01**: User can launch the verification APK's settings activity into a visible Wayland-backed Linux window or a deterministic surface path with equivalent state reporting
- [ ] **WIN-02**: User can focus the launched app window and perform meaningful interaction on the first milestone path

### App Runtime State

- [ ] **APP-01**: Linuxoid creates and preserves app data, files, cache, and runtime state for the verification APK inside its sandbox contract
- [ ] **APP-02**: Linuxoid exposes the verification APK's requested permissions, granted or denied state, and AppOps decisions through the same launch/runtime path
- [ ] **APP-03**: Linuxoid makes the verification APK's required assets and resource metadata available to the app startup path

### Verification Target

- [ ] **VER-01**: User can reach `org.futo.inputmethod.latin.uix.settings.SettingsActivity` on Linux through Linuxoid's direct runtime path
- [ ] **VER-02**: Linuxoid's first-app-start report states clearly whether real Java/Kotlin bytecode executed, whether startup reached a return boundary, and what exact blocker remains if the full activity path still does not run
- [ ] **VER-03**: The Self-Healing Android Device runtime harness emits actionable recovery diagnostics for verification APK launch failures or degraded startup
- [ ] **VER-04**: User can push the real verification APK one step past generic native-library load failure and see the next exact native or managed startup blocker on the same direct Linuxoid path
- [ ] **VER-05**: User can push the real verification APK one step past the current `native_activity_entrypoint_missing` seam and see the next exact native, JNI registration, or managed startup blocker on the same direct Linuxoid path

## v2 Requirements

Deferred to future milestones after the first real app execution path works.

### IME Integration

- **IME-01**: User can enable the keyboard app as an input method through Linuxoid-managed Android service behavior
- **IME-02**: User can type into Linux desktop applications through the Android keyboard app running under Linuxoid
- **IME-03**: Voice-input or microphone-backed keyboard features work through Linuxoid with explicit permission handling

### Broader Compatibility

- **COMP-01**: Additional third-party Android apps can be run through the same direct Linux runtime path
- **COMP-02**: Linuxoid supports deeper Android framework services beyond the first launcher/settings activity milestone

## Out of Scope

Explicitly excluded from this milestone to keep the first real app path tight.

| Feature | Reason |
|---------|--------|
| Emulator or full Android VM fallback | Conflicts with the core goal of direct Android-on-Linux execution |
| Waydroid-dependent validation path | The milestone must prove Linuxoid's own runtime path |
| Broad compatibility matrix growth before one app works | Pulls effort away from the first real app checkpoint |
| Full system-wide IME enablement in the same milestone | The first milestone should prove app start and interaction before full keyboard service parity |
| General-user polish and desktop packaging | This milestone is a developer prototype checkpoint first |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| APK-01 | Phase 1 | Pending |
| APK-02 | Phase 1 | Pending |
| APK-03 | Phase 2 | Pending |
| DEX-01 | Phase 2 | Pending |
| DEX-02 | Phase 3 | Pending |
| DEX-03 | Phase 2 | Pending |
| JNI-01 | Phase 4 | Pending |
| JNI-02 | Phase 4 | Pending |
| JNI-03 | Phase 7 | Pending |
| JNI-04 | Phase 7 | Pending |
| JNI-05 | Phase 8 | Pending |
| JNI-06 | Phase 8 | Pending |
| WIN-01 | Phase 5 | Pending |
| WIN-02 | Phase 5 | Pending |
| APP-01 | Phase 6 | Pending |
| APP-02 | Phase 6 | Pending |
| APP-03 | Phase 1 | Pending |
| VER-01 | Phase 5 | Pending |
| VER-02 | Phase 3 | Pending |
| VER-03 | Phase 6 | Pending |
| VER-04 | Phase 7 | Pending |
| VER-05 | Phase 8 | Pending |

**Coverage:**
- v1 requirements: 22 total
- Mapped to phases: 22
- Unmapped: 0 ✓

---
*Requirements defined: 2026-05-18*
*Last updated: 2026-05-19 after adding the native app-start bridge follow-on phase*
