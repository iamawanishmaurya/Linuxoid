# Research Summary: Linuxoid First Real App Execution

## Project Focus

Linuxoid should stay tightly focused on one goal: run a real Android app directly on Linux through its own compatibility/runtime path, without emulator or Waydroid fallback. The concrete first target is `/home/astra/Downloads/keyboard-0.1.28.apk`.

## Key Findings

### Stack

The existing C++/CMake bridge architecture is still the right stack for this milestone. The most important pieces are:

- real APK intake and staging
- DEX/class resolution and managed execution
- x86_64 JNI/native library loading
- Wayland-first window/input behavior
- sandbox, permissions, and explicit recovery reporting

### Verification App Reality

The target APK is not a toy app. It includes:

- package `org.futo.inputmethod.latin`
- a launcher settings activity
- an `InputMethodService`
- one large `classes.dex`
- many assets and keyboard layout files
- multiple native libraries, including `x86_64`
- sensitive permissions such as `RECORD_AUDIO`

This makes it a strong milestone target because it forces Linuxoid to prove real managed execution, resource access, native library handling, and visible interaction.

### First Real Success Bar

The correct first success bar is:

- resolve the real APK
- start the launcher settings activity on Linux
- show a visible Wayland-backed window
- allow meaningful interaction
- keep every remaining framework/runtime blocker explicit

Do **not** define first success as full IME enablement across the desktop yet.

## Table Stakes

- package and launcher activity resolution from a real APK
- class loading and managed lifecycle method execution
- x86_64 native library staging/loading
- asset/resource availability
- app sandbox and permission/AppOps state
- visible window plus usable input interaction

## Watch Out For

- binary manifest or compiled resource decode becoming the true first blocker
- JNI/native library loading failures after managed startup begins
- too many framework stubs masking the real missing runtime context
- confusing "settings activity launches" with "keyboard works system-wide"
- drifting back into broad compatibility work before the first real app is usable

## Recommended Direction

The next roadmap should aim straight at:

1. real APK metadata intake for `keyboard-0.1.28.apk`
2. deeper managed execution past the current minimal DEX checkpoints
3. the first real ART/framework-owned runtime seam needed by the app
4. JNI/native loading where the app requires it
5. visible Wayland launch plus interaction for the settings activity

## Bottom Line

Linuxoid already has a strong proof-oriented execution spine.
The next milestone should not widen the architecture.
It should turn that spine into one real app start on Linux, with the keyboard APK as the anchor and every remaining blocker reported honestly.
