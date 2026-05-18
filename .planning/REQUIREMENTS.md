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
| APK-01 | Phase TBD | Pending |
| APK-02 | Phase TBD | Pending |
| APK-03 | Phase TBD | Pending |
| DEX-01 | Phase TBD | Pending |
| DEX-02 | Phase TBD | Pending |
| DEX-03 | Phase TBD | Pending |
| JNI-01 | Phase TBD | Pending |
| JNI-02 | Phase TBD | Pending |
| WIN-01 | Phase TBD | Pending |
| WIN-02 | Phase TBD | Pending |
| APP-01 | Phase TBD | Pending |
| APP-02 | Phase TBD | Pending |
| APP-03 | Phase TBD | Pending |
| VER-01 | Phase TBD | Pending |
| VER-02 | Phase TBD | Pending |
| VER-03 | Phase TBD | Pending |

**Coverage:**
- v1 requirements: 16 total
- Mapped to phases: 0
- Unmapped: 16 ⚠️

---
*Requirements defined: 2026-05-18*
*Last updated: 2026-05-18 after initial definition*
