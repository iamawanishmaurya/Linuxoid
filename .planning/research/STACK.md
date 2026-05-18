# Stack Research: Linuxoid First Real App Execution

## Scope

This research is narrowly scoped to one goal: run a real Android APK directly on Linux through Linuxoid's own runtime path, with `keyboard-0.1.28.apk` as the first verification target.

## Existing Project Stack

The current repo already establishes the core implementation stack:

- `C++20`
- `CMake`
- `compatctl` as the operator CLI
- Linuxoid-owned bridge modules in `/home/astra/codex/wine-for-android/src/`
- deterministic staged JSON artifacts for runtime/session state
- optional host integrations for `Wayland` and `EGL`

## Recommended Runtime Stack for This Milestone

For the next milestone, the standard stack should remain execution-first and close to the current code:

1. **APK and manifest loader**
   - Existing Linuxoid archive and manifest pipeline
   - Must be extended to handle real-world APK manifest/resource decoding more reliably

2. **DEX and managed execution**
   - Existing Linuxoid DEX parser/interpreter path
   - Extend toward method resolution, object state, constructor invocation, and controlled framework call boundaries
   - Keep full ART integration explicit as a later step, not a hidden assumption

3. **Class loading and runtime metadata**
   - Linuxoid-owned staged DEX/class metadata
   - Deterministic class resolution for app classes like `org.futo.inputmethod.latin.*`
   - Runtime root and bootclasspath discovery should continue to be recorded explicitly

4. **JNI and native library loading**
   - Required because `keyboard-0.1.28.apk` ships native libraries for `arm64-v8a`, `armeabi-v7a`, `x86`, and `x86_64`
   - Existing Linuxoid library staging is directly relevant
   - x86_64 host path is especially important for desktop Linux verification

5. **Window/input host path**
   - Wayland-first Linux desktop host target
   - Existing window/surface bridge remains the right foundation
   - Input and focus need to work well enough for keyboard interaction

6. **App sandbox and state**
   - Existing storage and permission/AppOps contracts
   - Must be realistic enough for app settings, resource access, and native library execution

## Official Platform Findings

Android's official runtime docs say ART is the managed runtime that executes DEX bytecode, and applications require ART to be present before startup. The activity lifecycle docs confirm that `onCreate()`, `onStart()`, and `onResume()` are the core callbacks that define visible app startup. Android's IME documentation says a keyboard app is an app containing a class that extends `InputMethodService`, declared through a service with the `android.view.InputMethod` action and associated metadata.

## Implication for Linuxoid

The right stack for this milestone is **not** a broad framework rewrite.
It is:

- Linuxoid-owned APK and DEX handling
- Linuxoid-owned managed execution checkpoints
- selective framework boundary stubs only where necessary
- real host-side JNI, window, and input progress for the target APK

## What Not To Add Yet

- Full emulator path
- Full Android system image
- Broad app-compat abstraction layers before `keyboard-0.1.28.apk` works
- Large UI/desktop packaging work before execution is proven

## Confidence

- High confidence: continue with C++ bridge architecture and execution-first DEX/runtime work
- High confidence: Wayland-first Linux desktop is the right host target
- Medium confidence: selective ART-owned runtime bridging will be needed soon, but the exact minimum shape should be learned from the keyboard APK path itself
