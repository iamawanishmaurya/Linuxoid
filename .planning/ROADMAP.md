# Roadmap: Linuxoid

## Overview

This roadmap turns Linuxoid's existing proof-oriented runtime into a first real Android app execution path on Linux. The journey starts with real-world APK intake for `keyboard-0.1.28.apk`, then pushes managed execution, native/JNI loading, and Wayland interaction far enough that the verification app can start and be meaningfully used on a Linux desktop, while keeping every missing Android-runtime seam explicit. The newest live seam is now narrower again: `libjni_latinime.so` now loads, runs `JNI_OnLoad`, and reaches a real JNI registration dispatch boundary. Linuxoid still needs to execute that registration path before the later `Activity.onCreate(Bundle)` seam can move. The next planning slice should stay just as narrow: dispatch JNI registration without widening into broad framework recreation.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [x] **Phase 1: Keyboard APK Intake** - Make the real verification APK parse, stage, and surface trustworthy runtime metadata
- [x] **Phase 2: Managed Activity Start** - Resolve and drive the launcher settings activity through a deeper managed startup path
- [x] **Phase 3: Runtime Context Bridge** - Reduce the gap between the minimal interpreter path and a real ART-owned activity context
- [x] **Phase 4: JNI and Native Loading** - Bring x86_64 native libraries and JNI boundaries into the verification path
- [x] **Phase 5: Visible Wayland Interaction** - Make the verification app visibly launch and accept meaningful interaction on Linux
- [x] **Phase 6: Recovery and Runtime Hardening** - Stabilize app state, permissions, and recovery diagnostics around the first real app path
- [x] **Phase 7: Native libc Compatibility and Entry Bridge** - Get the real keyboard APK past the current `libjni_latinime.so` Android-libc/native-entry blocker and into the first true native startup boundary
- [x] **Phase 8: Native App-Start Bridge** - Turn the JNI-shaped `libjni_latinime.so` boundary into a Linuxoid-owned app-start strategy and expose the next exact startup seam
- [x] **Phase 9: Managed App-Start Dispatch** - Turn the Linuxoid-managed app-start bridge candidate into the first real post-bridge dispatch seam for the keyboard APK path
- [ ] **Phase 10: JNI Registration Dispatch** - Execute the registration-helper boundary for `libjni_latinime.so` and expose the first post-registration managed bootstrap seam

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

- [x] 05-01: Bind the real settings activity launch to the Wayland window/surface path

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 05-02: Make focus and interaction meaningful for the verification app

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 05-03: Add visible-launch regression coverage around the target APK

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

**Wave 1**

- [x] 06-01: Harden app state, permissions, and sandbox continuity for the verification APK

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 06-02: Tighten Self-Healing Android Device recovery reporting for the first real app path

### Phase 7: Native libc Compatibility and Entry Bridge

**Goal**: Linuxoid gets `keyboard-0.1.28.apk` past the current `libjni_latinime.so` Android-libc/native-entry seam and exposes the first true native startup boundary on the same direct Linux path.
**Mode:** mvp
**Depends on**: Phase 6
**Requirements**: JNI-03, JNI-04, VER-04
**Success Criteria** (what must be TRUE):

  1. Linuxoid narrows the real keyboard APK blocker from a generic native-entry failure to a specific Android-libc symbol or first native entry boundary that is either satisfied or reported exactly
  2. `compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` reaches a smaller post-load native seam than `undefined symbol: __strchr_chk`
  3. The downstream managed blocker `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context` stays visible and distinct once the upstream native seam moves

**Plans**: 3 plans

Plans:

**Wave 1**

- [x] 07-01: Close the first Android-libc symbol gap for `libjni_latinime.so`

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 07-02: Expose the first true native entry boundary for the keyboard APK

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 07-03: Lock the native-entry seam into first-app-start, recovery, and regression truth

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 10

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Keyboard APK Intake | 3/3 | Complete | 2026-05-18 |
| 2. Managed Activity Start | 3/3 | Complete | 2026-05-18 |
| 3. Runtime Context Bridge | 3/3 | Complete | 2026-05-18 |
| 4. JNI and Native Loading | 2/2 | Complete | Exact native load/JNI blockers now propagate through `launch-apk` and `--first-app-start-proof` |
| 5. Visible Wayland Interaction | 3/3 | Complete | Real keyboard `SettingsActivity` now owns a concrete window/focus target while preserving the exact native `dlopen` blocker |
| 6. Recovery and Runtime Hardening | 2/2 | Complete | Repeated keyboard-state continuity is validated and watchdog recovery now stays gated on the earliest native blocker |
| 7. Native libc Compatibility and Entry Bridge | 3/3 | Complete | `libjni_latinime.so` now loads, `JNI_OnLoad` runs, and the remaining blocker is the missing native activity entrypoint plus later managed `Activity.onCreate(Bundle)` dispatch |
| 8. Native App-Start Bridge | 3/3 | Complete | `libjni_latinime.so` now loads, runs `JNI_OnLoad`, and is reported as `linuxoid_managed_app_start_bridge_required`, while the next blockers are the Linuxoid-managed app-start bridge implementation and the later managed `Activity.onCreate(Bundle)` seam |
| 9. Managed App-Start Dispatch | 3/3 | Complete | `libjni_latinime.so` now reaches `jni_registration_dispatch_required`, exposes the registration-helper symbol boundary, and keeps the later managed `Activity.onCreate(Bundle)` seam distinct |
| 10. JNI Registration Dispatch | 0/3 | Planned | Next real blocker: dispatch `_ZN8latinime21registerNativeMethodsEP7_JNIEnvPKcPK15JNINativeMethodi` and expose the first post-registration managed bootstrap seam |

### Phase 8: Native App-Start Bridge

**Goal**: Linuxoid turns the JNI-shaped `libjni_latinime.so` boundary into a Linuxoid-owned app-start strategy and exposes the next exact native or managed startup seam for the real keyboard APK path.
**Mode:** mvp
**Depends on**: Phase 7
**Requirements**: JNI-05, JNI-06, VER-05
**Success Criteria** (what must be TRUE):

  1. Linuxoid no longer stops at a generic missing `ANativeActivity_onCreate` boundary for `libjni_latinime.so`; it either bridges that JNI-shaped library into a Linuxoid-owned app-start path or reports the next exact registration/bootstrap seam
  2. `compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` reaches a smaller seam than `native_activity_entrypoint_missing`
  3. The downstream managed blocker `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context` stays visible and distinct once the upstream native app-start seam moves

**Plans**: 3 plans

Plans:

**Wave 1**

- [x] 08-01: Research and bind a Linuxoid-owned app-start strategy for JNI-shaped primary libraries

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 08-02: Expose the first post-`JNI_OnLoad` registration or app-start boundary exactly

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 08-03: Lock the new app-start seam into first-app-start, recovery, and regression truth

### Phase 9: Managed App-Start Dispatch

**Goal**: Linuxoid turns the `linuxoid_managed_app_start_bridge_required` seam into the first real post-bridge dispatch boundary for the real keyboard APK path, while keeping later managed `Activity.onCreate(Bundle)` bootstrap truth explicit.
**Mode:** mvp
**Depends on**: Phase 8
**Requirements**: JNI-07, JNI-08, VER-06
**Success Criteria** (what must be TRUE):

  1. `compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` no longer stops at `linuxoid_managed_app_start_bridge_required`; it reaches the first exact post-bridge dispatch, JNI registration, or managed bootstrap seam
  2. Launch JSON keeps `JNI_OnLoad`, Linuxoid-managed bridge ownership, post-bridge dispatch, and later framework bootstrap seams distinct instead of collapsing them into one generic native failure
  3. The later managed blocker `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context` stays visible as the next seam once the post-bridge dispatch boundary moves

**Plans**: 3 plans

Plans:

**Wave 1**

- [x] 09-01: Research and bind the first Linuxoid-managed app-start dispatch strategy for `libjni_latinime.so`

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 09-02: Expose the first post-bridge dispatch or JNI registration boundary exactly

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 09-03: Lock the post-bridge dispatch seam into first-app-start, recovery, and regression truth

### Phase 10: JNI Registration Dispatch

**Goal**: Linuxoid executes the `libjni_latinime.so` registration-helper boundary and exposes the first post-registration managed bootstrap seam for the real keyboard APK path, while keeping the later `Activity.onCreate(Bundle)` bootstrap truth explicit.
**Mode:** mvp
**Depends on**: Phase 9
**Requirements**: JNI-09, JNI-10, VER-07
**Success Criteria** (what must be TRUE):

  1. `compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` no longer stops at `jni_registration_dispatch_required`; it executes or precisely attempts the registration-helper boundary and reaches the first exact post-registration managed bootstrap seam
  2. Launch JSON keeps `JNI_OnLoad`, registration-helper selection, registration dispatch, registration outcome, and later framework bootstrap seams distinct instead of collapsing them into one generic native failure
  3. The later managed blocker `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context` stays visible as the next seam once registration dispatch advances

**Plans**: 3 plans

Plans:

**Wave 1**

- [ ] 10-01: Research and bind a Linuxoid-owned JNI registration dispatch strategy for `libjni_latinime.so`

**Wave 2** *(blocked on Wave 1 completion)*

- [ ] 10-02: Expose the first post-registration managed bootstrap boundary exactly

**Wave 3** *(blocked on Wave 2 completion)*

- [ ] 10-03: Lock the post-registration seam into first-app-start, recovery, and regression truth
