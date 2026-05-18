# Features Research: First Real Android App on Linux

## Target App Findings

Inspection of `/home/astra/Downloads/keyboard-0.1.28.apk` shows:

- package: `org.futo.inputmethod.latin`
- one large `classes.dex`
- many assets, especially keyboard layouts and fonts
- native libraries for `arm64-v8a`, `armeabi-v7a`, `x86`, and `x86_64`
- launcher activity: `org.futo.inputmethod.latin.uix.settings.SettingsActivity`
- IME service: `org.futo.inputmethod.latin.LatinIME`
- permissions including `RECORD_AUDIO`, `POST_NOTIFICATIONS`, dictionary permissions, and foreground service permissions

## Table Stakes for This Milestone

These are the features Linuxoid must support well enough for the first verification target:

### Package and launch

- Parse package identity from a real APK
- Resolve the launcher activity
- Stage the app into a Linuxoid-owned sandbox/runtime root
- Create a process/session record for the app launch

### Managed execution

- Parse real `classes.dex`
- Resolve the target class and lifecycle method
- Execute enough app bytecode to move beyond synthetic helper proofs
- Report exact blockers if real framework/runtime behavior is still missing

### Native integration

- Stage and load `x86_64` native libraries when present
- Keep JNI/native dependency failures explicit
- Preserve ABI diagnostics in public reports

### Assets and resources

- Make fonts, layout files, and resource metadata available
- Handle the app's large asset footprint deterministically
- Keep binary-resource decode limits explicit where they still exist

### Window and input

- Open a visible Linux window for the launcher activity path
- Support focus and meaningful keyboard interaction
- Track Wayland-first surface state honestly

### Permissions and storage

- Persist app state and settings directories
- Reflect requested permissions and operation decisions
- Avoid silently granting sensitive capabilities

## Differentiators

These are meaningful Linuxoid differentiators, but not the first milestone's success bar:

- Self-Healing Android Device recovery diagnostics
- compatibility reporting across many APKs
- replayable runtime-health artifacts
- deeper Binder/service fidelity

## Anti-Features for v1

These should stay out of the first milestone:

- broad app-store compatibility claims
- full Android-framework parity claims
- generalized compatibility marketing before one app really works
- UI polish unrelated to getting the keyboard APK running

## Complexity Notes

- Launcher activity support is easier than full IME service enablement
- IME service behavior is a second-stage goal after app launch and settings interaction
- Native library presence means JNI is likely part of the real critical path, not an optional future enhancement
- Real resource decoding may become necessary if settings UI depends on compiled resources beyond current metadata-only handling

## Confidence

- High confidence: launcher activity execution and visible UI are the right first target
- High confidence: IME service registration and full system keyboard behavior should be a later checkpoint, not the first launch bar
- Medium confidence: the app will quickly stress binary manifest/resource decoding and framework-class availability
