# Requirements: Linuxoid

**Defined:** 2026-05-19
**Core Value:** Run a real Android app directly on Linux through Linuxoid's own compatibility/runtime path, with honest execution and honest blockers instead of emulator fallback.

## v1 Requirements

Requirements for milestone `v1.1 Visible App Launch`, centered on pushing the real keyboard APK from the current `activity_oncreate_bundle_dispatch_required` seam into a visible and interactive Linux launch.

### Managed Dispatch

- [ ] **JNI-15**: Linuxoid can dispatch `org.futo.inputmethod.latin.uix.settings.SettingsActivity->onCreate(Landroid/os/Bundle;)V` inside the already bound Linuxoid runtime context for the real keyboard APK path
- [ ] **JNI-16**: Linuxoid keeps runtime-context binding, `Activity.onCreate(Bundle)` dispatch, and any later framework/resource blocker distinct in `launch-apk`, `--first-app-start-proof`, and watchdog reporting

### Visible Launch

- [ ] **WIN-03**: User can drive the real keyboard settings activity path into a visible Wayland-backed or equivalent surfaced window state when the host display is available
- [ ] **WIN-04**: User can focus the launched settings activity and Linuxoid reports meaningful interaction ownership tied to the real package/activity/process/window session

### Runtime Resources

- [ ] **APP-04**: Linuxoid makes the settings activity's required resource, asset, and app-state inputs available deeply enough that visible launch no longer fails at a hidden setup gap

### Verification Target

- [ ] **VER-10**: User can push `/home/astra/Downloads/keyboard-0.1.28.apk` one step past `activity_oncreate_bundle_dispatch_required` on the same direct Linuxoid path and either see a visible settings launch or the next exact blocker
- [ ] **VER-11**: The Self-Healing Android Device runtime harness keeps the earliest visible-launch blocker authoritative and actionable across repeated launch attempts

## v2 Requirements

Deferred until after one visible settings launch works through Linuxoid's direct runtime path.

### IME Integration

- **IME-01**: User can enable the keyboard app as an input method through Linuxoid-managed Android service behavior
- **IME-02**: User can type into Linux desktop applications through the Android keyboard app running under Linuxoid
- **IME-03**: Voice-input or microphone-backed keyboard features work through Linuxoid with explicit permission handling

### Broader Compatibility

- **COMP-01**: Additional third-party Android apps can run through the same direct Linux runtime path
- **COMP-02**: Linuxoid supports deeper Android framework services beyond the first visible settings launch milestone

## Out of Scope

Explicitly excluded to keep the visible-launch milestone tight.

| Feature | Reason |
|---------|--------|
| Emulator or full Android VM fallback | Conflicts with the direct Android-on-Linux goal |
| Waydroid-dependent validation path | The milestone must prove Linuxoid's own runtime path |
| Full system-wide IME enablement | Settings launch and visible interaction come first |
| Broad compatibility-matrix growth | Pulls effort away from the real keyboard launch seam |
| Desktop packaging and end-user polish | This milestone is still a runtime engineering checkpoint |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| JNI-15 | Phase 13 | Pending |
| JNI-16 | Phase 13 | Pending |
| WIN-03 | Phase 14 | Pending |
| APP-04 | Phase 14 | Pending |
| WIN-04 | Phase 15 | Pending |
| VER-10 | Phase 13 | Pending |
| VER-11 | Phase 15 | Pending |

**Coverage:**
- v1 requirements: 7 total
- Mapped to phases: 7
- Unmapped: 0 ✓

---
*Requirements defined: 2026-05-19*
*Last updated: 2026-05-19 after starting milestone v1.1 Visible App Launch*
