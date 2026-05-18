# Features Research: Linuxoid v1.1 Visible App Launch

## Target App Findings

Inspection of `/home/astra/Downloads/keyboard-0.1.28.apk` still shows the same facts that matter for this milestone:

- package: `org.futo.inputmethod.latin`
- launcher activity: `org.futo.inputmethod.latin.uix.settings.SettingsActivity`
- IME service: `org.futo.inputmethod.latin.LatinIME`
- one large `classes.dex`
- many assets, fonts, and keyboard layout files
- native libraries for `arm64-v8a`, `armeabi-v7a`, `x86`, and `x86_64`
- permissions including `RECORD_AUDIO`, notification, dictionary, and foreground-service capabilities

## Table Stakes for This Milestone

### Managed dispatch

- Execute `SettingsActivity->onCreate(Landroid/os/Bundle;)V` beyond the current bound-context placeholder
- Keep launch reports explicit about whether dispatch succeeded, stubbed, or blocked
- Preserve the exact next blocker when the real app still cannot continue

### Visible startup

- Carry the real launch session into a visible Wayland-backed or equivalent surfaced state
- Keep host availability truthful through `wayland_surface_available` and `egl_surface_available`
- Tie surface truth to the same package, activity, process, and runtime session

### Interaction

- Report focus ownership and interaction state for the real settings activity
- Keep interaction tied to the same visible launch session

### Resource and state readiness

- Keep the settings activity's asset, resource, and app-state inputs available through the same launch path
- Make any compiled-resource or initialization gap explicit instead of masking it behind generic launch failure

### Recovery truth

- Keep the Self-Healing Android Device watchdog anchored to the earliest visible-launch blocker
- Avoid attempting downstream repairs behind known upstream launch failures

## Differentiators

These remain meaningful Linuxoid differentiators, but they are not the milestone success bar:

- broader Binder or service fidelity
- compatibility reporting across many APKs
- richer automated healing after first visible launch works

## Anti-Features for v1.1

- claiming full IME enablement from a settings activity launch
- broad framework parity claims
- compatibility expansion before the real keyboard settings UI visibly starts

## Complexity Notes

- The next blocker is likely to move from managed dispatch into resource, framework, or visible-surface readiness
- A visible window alone is not enough; the milestone still needs meaningful focus and interaction truth
- Native/JNI continuity must stay in the critical path even if the next seam looks managed

## Confidence

- High confidence: visible settings launch is the right milestone bar
- High confidence: IME service behavior should remain future work
- Medium confidence: compiled resources or framework-owned context may become the next real blocker after dispatch moves
