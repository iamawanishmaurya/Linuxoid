# Roadmap: Linuxoid

## Overview

Milestone `v1.1 Visible App Launch` starts from the exact live seam after Phase 12. The real keyboard APK now gets `libjni_latinime.so` loaded, calls `JNI_OnLoad`, completes JNI registration, selects `org.futo.inputmethod.latin.uix.settings.SettingsActivity->onCreate(Landroid/os/Bundle;)V`, and records `linuxoid_runtime_context_bound`, but the launch still stops at `activity_oncreate_bundle_dispatch_required`. This roadmap keeps working straight down that seam until the keyboard settings activity can surface visibly and accept meaningful interaction on a Wayland Linux desktop without pretending the rest of Android already exists.

## Milestone

- **Current Milestone:** `v1.1 Visible App Launch`
- **Previous Milestone:** `v1.0` completed through Phase 12
- **Numbering Mode:** Continue phase numbering from the previous milestone

## Phases

- [ ] **Phase 13: Activity onCreate Dispatch Bridge** - Bridge the bound Linuxoid runtime context into real `Activity.onCreate(Bundle)` dispatch for the keyboard settings activity
- [ ] **Phase 14: Visible Settings Surface** - Carry successful managed startup into a visible settings surface with truthful resource and window state
- [ ] **Phase 15: Interactive Visible Launch** - Make the surfaced settings launch focusable, usable, and stable under Self-Healing diagnostics

## Progress

**Execution Order:**
Phases execute in numeric order: 13 -> 14 -> 15

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 13. Activity onCreate Dispatch Bridge | 0/3 | Planned | Current exact blocker: `activity_oncreate_bundle_dispatch_required` |
| 14. Visible Settings Surface | 0/3 | Planned | Depends on real post-dispatch startup and resource readiness |
| 15. Interactive Visible Launch | 0/3 | Planned | Depends on visible surface availability and focusable interaction |

## Phase Details

### Phase 13: Activity onCreate Dispatch Bridge

**Goal**: Linuxoid dispatches the real keyboard `SettingsActivity->onCreate(Landroid/os/Bundle;)V` lifecycle method inside the already bound runtime context and exposes the first exact post-dispatch blocker without widening into broad framework recreation.
**Mode:** mvp
**Depends on**: Phase 12
**Requirements**: JNI-15, JNI-16, VER-10
**Success Criteria** (what must be TRUE):

  1. `compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` no longer stops at `activity_oncreate_bundle_dispatch_required`
  2. Launch JSON keeps runtime-context binding, `Activity.onCreate(Bundle)` dispatch, and any later framework/resource boundary distinct instead of collapsing them into one generic managed failure
  3. The next blocker after successful dispatch is narrower and explicit, whether it is resource, surface, or another framework-owned seam

**Plans**: 3 plans

Plans:

**Wave 1**

- [ ] 13-01: Dispatch `Activity.onCreate(Bundle)` inside the bound Linuxoid runtime context for the real keyboard path

**Wave 2** *(blocked on Wave 1 completion)*

- [ ] 13-02: Expose the first exact post-dispatch framework, resource, or window-readiness boundary

**Wave 3** *(blocked on Wave 2 completion)*

- [ ] 13-03: Lock the post-dispatch seam into first-app-start, watchdog, and regression truth

### Phase 14: Visible Settings Surface

**Goal**: Linuxoid carries the real keyboard settings launch from successful managed dispatch into a visible Wayland-backed or equivalent surfaced window while keeping host availability and resource gaps honest.
**Mode:** mvp
**Depends on**: Phase 13
**Requirements**: WIN-03, APP-04
**Success Criteria** (what must be TRUE):

  1. The real keyboard settings activity reaches a visible surface state when the host display is available, or Linuxoid reports the exact remaining blocker
  2. Resource, asset, and app-state readiness for the settings launch are tied to the same launch session instead of disappearing behind generic window failure
  3. `launch-apk --window-proof` and the top-level launch JSON agree on the visible-launch truth

**Plans**: 3 plans

Plans:

**Wave 1**

- [ ] 14-01: Make post-dispatch resource and session state sufficient for settings UI startup

**Wave 2** *(blocked on Wave 1 completion)*

- [ ] 14-02: Carry successful startup into visible Wayland or EGL-backed surface readiness

**Wave 3** *(blocked on Wave 2 completion)*

- [ ] 14-03: Lock visible-launch truth into window-proof, docs, and regressions

### Phase 15: Interactive Visible Launch

**Goal**: Linuxoid makes the visible keyboard settings launch focusable, meaningfully interactive, and stable under repeated Self-Healing Android Device launch attempts.
**Mode:** mvp
**Depends on**: Phase 14
**Requirements**: WIN-04, VER-11
**Success Criteria** (what must be TRUE):

  1. The visible settings activity can own focus and Linuxoid reports real interaction state tied to the same app session
  2. The earliest blocker remains authoritative across repeated visible-launch attempts and watchdog recovery does not drift into noisy downstream guesses
  3. Visible-launch verification is repeatable enough that the next milestone can move toward IME behavior without re-opening the same launch seam

**Plans**: 3 plans

Plans:

**Wave 1**

- [ ] 15-01: Bind focus and interaction ownership to the real settings session

**Wave 2** *(blocked on Wave 1 completion)*

- [ ] 15-02: Tighten Self-Healing recovery gating around visible-launch failures

**Wave 3** *(blocked on Wave 2 completion)*

- [ ] 15-03: Add live and regression verification for a usable visible launch
