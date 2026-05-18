# Roadmap: Linuxoid

## Overview

This roadmap turns Linuxoid's existing proof-oriented runtime into a first real Android app execution path on Linux. The journey starts with real-world APK intake for `keyboard-0.1.28.apk`, then pushes managed execution, native/JNI loading, and Wayland interaction far enough that the verification app can start and be meaningfully used on a Linux desktop, while keeping every missing Android-runtime seam explicit.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [x] **Phase 1: Keyboard APK Intake** - Make the real verification APK parse, stage, and surface trustworthy runtime metadata
- [x] **Phase 2: Managed Activity Start** - Resolve and drive the launcher settings activity through a deeper managed startup path
- [x] **Phase 3: Runtime Context Bridge** - Reduce the gap between the minimal interpreter path and a real ART-owned activity context
- [x] **Phase 4: JNI and Native Loading** - Bring x86_64 native libraries and JNI boundaries into the verification path
- [ ] **Phase 5: Visible Wayland Interaction** - Make the verification app visibly launch and accept meaningful interaction on Linux
- [ ] **Phase 6: Recovery and Runtime Hardening** - Stabilize app state, permissions, and recovery diagnostics around the first real app path

## Phase Details

### Phase 1: Keyboard APK Intake

**Goal**: Linuxoid can ingest `keyboard-0.1.28.apk` as a real APK, not just a synthetic fixture, and stage its package, component, permissions, resources, and libraries into a trustworthy session root.
**Mode:** mvp
**Depends on**: Nothing (first phase)
**Requirements**: APK-01, APK-02, APP-03
**Success Criteria** (what must be TRUE):

  1. User can point Linuxoid at `/home/astra/Downloads/keyboard-0.1.28.apk` and receive the correct package name, launcher activity, permission list, and ABI inventory
  2. Linuxoid stages the real APK into its sandbox/session root without emulator or Waydroid dependencies
  3. Assets and resource metadata required by the verification path are visible through Linuxoid reports instead of fixture-only assumptions

**Plans**: 3 plans

Plans:

- [x] 01-01: Harden real-world APK manifest and metadata intake for the keyboard target
- [x] 01-02: Validate staging, assets, permissions, and native library inventory for the target APK
- [x] 01-03: Turn the real APK intake into a stable launch precondition and regression proof

### Phase 2: Managed Activity Start

**Goal**: Linuxoid resolves `org.futo.inputmethod.latin.uix.settings.SettingsActivity` and drives its startup path deeper than the current minimal DEX checkpoints.
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: APK-03, DEX-01, DEX-03
**Success Criteria** (what must be TRUE):

  1. Linuxoid resolves the launcher settings activity class and startup lifecycle method from the real `classes.dex`
  2. First-app-start reporting names the real activity and managed execution seam instead of only synthetic fixture methods
  3. Any unsupported opcode, class, or framework boundary is reported precisely and reproducibly

**Plans**: 3 plans

Plans:
**Wave 1**

- [x] 02-01: Bind real launcher activity resolution to the first-app-start proof

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 02-02: Extend managed execution reporting around the activity startup path

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 02-03: Add hard regression coverage for real activity-start boundaries

### Phase 3: Runtime Context Bridge

**Goal**: Linuxoid crosses the next runtime boundary beyond the current minimal interpreter so the activity startup path depends less on placeholder context and more on real managed runtime behavior.
**Mode:** mvp
**Depends on**: Phase 2
**Requirements**: DEX-02, VER-02
**Success Criteria** (what must be TRUE):

  1. Linuxoid executes more of the real app startup path than the current helper and placeholder seams
  2. Reports distinguish clearly between Linuxoid interpreter behavior, framework stubs, and any real ART-owned context
  3. The remaining blocker toward full launcher activity startup is narrower and more actionable than `needs-real-activitythread-context`

**Plans**: 3 plans

Plans:

- [x] 03-01: Extend method invocation, receiver, and lifecycle context handling for the real activity path
- [x] 03-02: Tighten bytecode/runtime diagnostics around the next framework boundary
- [x] 03-03: Convert the next remaining blocker into a smaller managed-runtime seam

### Phase 4: JNI and Native Loading

**Goal**: Linuxoid stages and loads the verification APK's x86_64 native libraries through the same real app launch path and exposes exact JNI blockers when they appear.
**Mode:** mvp
**Depends on**: Phase 3
**Requirements**: JNI-01, JNI-02
**Success Criteria** (what must be TRUE):

  1. Linuxoid selects and stages the `x86_64` native libraries from `keyboard-0.1.28.apk`
  2. The launch path records successful native-library readiness or an exact JNI/native blocker
  3. No false success is reported when managed execution reaches a JNI-backed path that Linuxoid still cannot satisfy

**Plans**: 2 plans

Plans:

**Wave 1**

- [x] 04-01: Bring x86_64 native library staging/loading into the real APK path

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 04-02: Add exact JNI/native failure reporting for the verification app

### Phase 5: Visible Wayland Interaction

**Goal**: The verification app's launcher settings activity opens visibly on a Wayland Linux desktop and supports meaningful interaction.
**Mode:** mvp
**Depends on**: Phase 4
**Requirements**: WIN-01, WIN-02, VER-01
**Success Criteria** (what must be TRUE):

  1. User can launch the verification app and see a visible Linux window or equivalent surfaced state for the settings activity
  2. The launched app can receive focus and meaningful user interaction on the first milestone path
  3. Window, input, and activity/process/session state stay tied together in Linuxoid reports

**Plans**: 3 plans

Plans:

**Wave 1**

- [ ] 05-01: Bind the real settings activity launch to the Wayland window/surface path

**Wave 2** *(blocked on Wave 1 completion)*

- [ ] 05-02: Make focus and interaction meaningful for the verification app

**Wave 3** *(blocked on Wave 2 completion)*

- [ ] 05-03: Add visible-launch regression coverage around the target APK

### Phase 6: Recovery and Runtime Hardening

**Goal**: Linuxoid preserves app runtime state, permissions, and recovery diagnostics strongly enough that the first real app path is repeatable and debuggable.
**Mode:** mvp
**Depends on**: Phase 5
**Requirements**: APP-01, APP-02, VER-03
**Success Criteria** (what must be TRUE):

  1. App storage, permissions, and AppOps are preserved and reported accurately for repeated verification APK launches
  2. The Self-Healing Android Device runtime harness emits actionable diagnostics and recovery actions for degraded or failed app starts
  3. Re-running the verification flow produces stable, comparable artifacts instead of one-off success

**Plans**: 2 plans

Plans:

- [ ] 06-01: Harden app state, permissions, and sandbox continuity for the verification APK
- [ ] 06-02: Tighten Self-Healing Android Device recovery reporting for the first real app path

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Keyboard APK Intake | 3/3 | Complete | 2026-05-18 |
| 2. Managed Activity Start | 3/3 | Complete | 2026-05-18 |
| 3. Runtime Context Bridge | 3/3 | Complete | 2026-05-18 |
| 4. JNI and Native Loading | 2/2 | Complete | Exact native load/JNI blockers now propagate through `launch-apk` and `--first-app-start-proof` |
| 5. Visible Wayland Interaction | 0/3 | Not started | - |
| 6. Recovery and Runtime Hardening | 0/2 | Not started | - |
