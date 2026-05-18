# Stack Research: Linuxoid v1.1 Visible App Launch

## Scope

This research is narrowly scoped to milestone `v1.1 Visible App Launch`: move the real keyboard APK from `activity_oncreate_bundle_dispatch_required` into a visible and interactive settings launch on Linux.

## Existing Project Stack

The repo already has the right core stack:

- `C++20`
- `CMake`
- `compatctl` as the operator CLI
- Linuxoid-owned bridge modules in `/home/astra/codex/wine-for-android/src/`
- deterministic staged JSON artifacts for runtime and session state
- optional host integrations for `Wayland` and `EGL`

## Recommended Stack Priorities for This Milestone

1. **Managed dispatch bridge**
   - Reuse the existing native execute and DEX bridge path
   - Turn the bound runtime-context placeholder into real `Activity.onCreate(Bundle)` dispatch

2. **Resource and app-state continuity**
   - Reuse staged assets, resource metadata, storage, and permissions
   - Keep any compiled-resource or app-state gap explicit if settings launch still cannot proceed

3. **Native and JNI continuity**
   - Preserve the current `libjni_latinime.so` load and JNI registration path
   - Do not regress the new `linuxoid_runtime_context_bound` checkpoint

4. **Window and surface truth**
   - Reuse the existing window-manager and runtime bridge contracts
   - Carry successful managed startup into visible Wayland/EGL readiness only after dispatch truly moves

5. **Interaction and recovery**
   - Keep focus and input tied to the same app session
   - Keep Self-Healing Android Device gating anchored to the earliest real blocker

## Implication for Linuxoid

The right stack for this milestone is still not a broad framework rewrite. It is:

- Linuxoid-owned dispatch and lifecycle progress for the real keyboard activity
- Linuxoid-owned staged resource and session continuity
- truthful Wayland and EGL surface reporting
- existing native and watchdog infrastructure carried forward without widening sideways

## What Not To Add Yet

- Emulator or Waydroid fallback
- System-wide IME enablement
- Broad compatibility or marketing layers before visible launch works
- Desktop packaging or polish work unrelated to launch

## Confidence

- High confidence: continue with the current C++ bridge architecture
- High confidence: visible settings launch is the right next milestone target
- Medium confidence: a real compiled-resource or framework-owned seam may appear immediately after `Activity.onCreate(Bundle)` dispatch moves
