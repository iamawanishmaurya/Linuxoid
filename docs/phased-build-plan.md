# Linuxoid — Phased Build Plan

> Current state: scaffold `96/100` · execution `78/100`  
> Goal: Android apps on Linux. No Waydroid. No ADB. No emulator.

---

## Phase Map

| Phase | Label | Duration | Gate |
| --- | --- | --- | --- |
| P0 | Freeze & Triage | 1 week | Every stub mapped |
| P1 | NDK Execution Core | 3-4 weeks | Process lives, exit 0 |
| P2 | Window + Graphics | 3-4 weeks | Calculator renders pixel |
| P3 | DEX + ART Bridge | 4-6 weeks | F-Droid opens natively |
| P4 | Binder + Services | 4-6 weeks | Settings navigates |
| P5 | Audio + Network + Browser | 3-4 weeks | 10-app compat matrix |
| P6 | Polish + Release | 2-3 weeks | `apt install linuxoid` |

Total estimate: about 5-6 months solo.

## P0 — Freeze & Triage

Duration: 1 week  
Goal: stop adding scaffold and audit what actually runs.  
Outcome: clear execution baseline with every current stub mapped.

### Tasks

#### P0.1 — Audit `native-execute-stub`

- Run it on the Calculator APK.
- Record the exact crash or exit point.
- Treat this as the ground-zero execution baseline.
- Files: `src/native_execute_stub.cpp`

#### P0.2 — Freeze browser track

- Comment-gate all BrowserSession implementation work.
- Do not add new browser code until `P5`.
- Files: `docs/browser-self-healing-architecture.md`

#### P0.3 — Tag `v0-scaffold`

- Tag the current scaffold state.
- Keep a clean separation between the scaffold era and the execution era.
- Files: `CHANGELOG.md`

#### P0.4 — Map every stub exit point

- List each command that returns early with a `not implemented` style result.
- Turn those stubs into the ordered queue for `P1+`.
- Files: `src/`

## P1 — NDK Execution Core

Duration: 3-4 weeks  
Goal: `dlopen()` Calculator `.so` -> `ANativeActivity_onCreate` -> process stays alive.  
Outcome: the first real Android code runs on Linux without Waydroid or ADB.

### Why NDK first?

Calculator is the fastest path to direct execution because it is a pure native foreground target. Linuxoid does not need DEX, Java, or ART to start proving the execution core.

Current repo note as of `2026-05-16`: the local staged `com.android.calculator2` APK in this workspace is dex-only and contains no `lib/*.so`, so Linuxoid is using it as a negative oracle while the first runner slice is verified against a fixture native library.

### Tasks

#### P1.1 — Extract + `dlopen` native `.so`

```cpp
// Unzip APK -> lib/x86_64/libcalculator.so
void* lib = dlopen("libcalculator.so", RTLD_NOW);
auto entry = (ANativeActivityCreateFunc*)
    dlsym(lib, "ANativeActivity_onCreate");
// Log result. No crash = win.
```

- Files: `src/native_execute_stub.cpp`

#### P1.2 — Fake `ANativeActivity` struct

- Populate `ANativeActivity` fields with nulls first.
- Call the entrypoint.
- Catch `SIGSEGV`; every crash reveals the next required field.
- Iterate until it no longer crashes.
- Files: `include/wfa/native_activity.h`

#### P1.3 — Fake `JavaVM` + `JNIEnv`

- NDK apps still call `JNI_OnLoad`.
- Build a minimal fake `JavaVM` with a stub vtable.
- Provide enough `JNIEnv` behavior to avoid crashing on calls like `FindClass`.
- Files: `src/jni_stub.cpp`, `include/wfa/jni_stub.h`

#### P1.4 — `AAssetManager` stub

- Implement `AAssetManager_open` over the staged APK payload.
- Back it with `libzip`, `miniz`, or the smallest viable zip reader.
- Files: `src/asset_manager_stub.cpp`

#### P1.5 — `ALooper` stub

- Implement `ALooper_prepare` and `ALooper_pollAll`.
- Back it with `epoll` and a self-pipe.
- Let the app event loop spin without hanging forever.
- Files: `src/looper_stub.cpp`

#### P1.6 — Verify process lives for 5 seconds

- Run `compatctl native-execute-stub`.
- Process stays alive.
- Exits cleanly.
- Gate: exit code `0`.
- Files: `tests/native_execute_test.cpp`

## P2 — Window + Graphics

Duration: 3-4 weeks  
Goal: Calculator renders a real pixel in a Wayland window.  
Outcome: first visible frame from an Android NDK app on the Linux desktop.

Current repo note as of `2026-05-17`: Linuxoid now has a verified headless `ANativeWindow`-shaped surface fixture that writes a deterministic first-pixel marker, a verified native-activity callback fixture that records ordered `window_created`, `window_changed`, and `window_destroyed` events without a real compositor, a verified real Wayland client surface fixture that can connect to `wl_display` and create a `wl_surface` when Wayland is available, a verified EGL smoke fixture that can initialize a real EGL display plus context plus pbuffer when EGL is available, a verified minimal `ANativeWindow` bridge contract with deterministic geometry updates and stable artifacts, and a verified focused input queue fixture with deterministic pointer/key injection plus focus ownership metadata. Binding EGL to the real Wayland surface, routing that bound surface through the bridge contract, and adding full IME/text composition are still pending.

### Tasks

#### P2.1 — Wayland surface via `libwayland-client`

```text
wl_display_connect
  -> wl_compositor -> wl_surface
  -> xdg_surface -> xdg_toplevel
  -> real OS window
```

- Files: `src/wayland_window.cpp`, `include/wfa/window.h`

#### P2.2 — EGL context on the Wayland surface

```text
eglGetDisplay(wl_display)
  -> eglInitialize
  -> eglCreateContext
  -> eglCreateWindowSurface(wl_egl_window)
  -> OpenGL ES 3.0 context live
```

- Files: `src/egl_surface.cpp`

#### P2.3 — `ANativeWindow` bridge

- Implement the `ANativeWindow` vtable over `wl_egl_window`.
- Route `ANativeWindow_setBuffersGeometry` to `wl_egl_window_resize`.
- Pass the result into `ANativeActivity::window`.
- Files: `src/native_window_bridge.cpp`, `include/wfa/native_window.h`

#### P2.4 — Surface created and changed callbacks

- Call `ANativeActivityCallbacks::onNativeWindowCreated` after EGL init.
- Make `eglSwapBuffers` land on screen.
- Files: `src/native_activity_callbacks.cpp`

#### P2.5 — `AInputQueue` stub

- Read input from `libinput` or the smallest suitable Linux input path.
- Translate to `AInputEvent`.
- Feed the events through the looper fd.
- Current repo note: Linuxoid now has a deterministic focused input queue fixture and JSONL event artifacts, but it is still not a real `AInputQueue`, not connected to native activity code yet, and not a full IME/text composition path.
- Files: `src/input_queue.cpp`

#### P2.6 — Verify Calculator window opens

- Run `compatctl native-execute-stub calculator.apk`.
- A Wayland window appears.
- Calculator UI is visible.
- Input works.
- Gate: screenshot proof committed to the repo.
- Files: `tests/window_smoke_test.cpp`

## P3 — DEX + ART Bridge

Duration: 4-6 weeks  
Goal: run Kotlin and Java APKs, not only NDK-first apps.  
Outcome: F-Droid or Settings launches natively.

Current repo note as of `2026-05-17`: Linuxoid now has a verified pre-ART APK resource bridge that can inspect plain APK/ZIP manifest metadata, list normalized asset paths, reject traversal, and emit stable readiness JSON for harnesses. Full `resources.arsc` semantics, binary XML handling, and actual ART/DEX execution are still pending.

### Tasks

#### P3.1 — Embed ART as a library

- Build `libart.so` from AOSP source.
- Link Linuxoid against it.
- Call `JNI_CreateJavaVM` with minimal options.
- Files: `src/art_bridge.cpp`, `CMakeLists.txt`

#### P3.2 — DEX classloader bootstrap

- Use `dalvik.system.DexClassLoader` to load `classes.dex` from the staged APK.
- Resolve the main Activity class.
- Call `Application.onCreate` through JNI.
- Files: `src/dex_loader.cpp`

#### P3.3 — `Context` + `Resources` stub

- Provide fake `Context` paths.
- Back `Resources` with the staged APK zip for drawables and strings.
- Files: `src/context_stub.cpp`, `src/resources_stub.cpp`

#### P3.4 — Activity thread bootstrap

- Recreate `ActivityThread.main()` behavior enough for app startup.
- Attach the main looper.
- Call `Application.attach`, `Application.onCreate`, and `Activity.onCreate`.
- Files: `src/activity_thread.cpp`

#### P3.5 — View rendering via HWUI or Skia

- Route `View` drawing to a canvas Linuxoid owns.
- Candidate backends: `libhwui.so` or Skia-to-EGL.
- Files: `src/view_renderer.cpp`

#### P3.6 — Verify a Java app renders

- Launch `org.fdroid.fdroid` or `com.android.settings`.
- Gate: visible native UI.
- Files: `tests/dex_smoke_test.cpp`

## P4 — Binder + Services

Duration: 4-6 weeks  
Goal: fake Binder IPC so apps can query essential services without a real Android runtime.  
Outcome: PackageManager, ActivityManager, and peer services become callable.

Current repo note as of `2026-05-17`: Linuxoid now has a verified local Binder-shaped service-manager fixture that writes deterministic service registration, lookup, and transaction artifacts for `package_manager` and `activity_manager`, the lifecycle shim now points at those machine-readable artifacts instead of only a flat text registry, and the local fixture now includes a socketpair-backed transport log for lookup/transaction round trips. Full Parcel semantics and real cross-process Android Binder behavior are still pending.

### Tasks

#### P4.1 — Userspace Binder over Unix sockets

- Replace `/dev/binder` with a Unix socket transport.
- Keep Parcel semantics compatible enough for Linuxoid-owned services.
- Files: `src/binder_transport.cpp`, `include/wfa/binder.h`

#### P4.2 — ServiceManager stub

- `ServiceManager.getService(name)` returns Linuxoid-owned stub binders.
- Files: `src/service_manager.cpp`

#### P4.3 — PackageManager stub

- Implement `getInstalledPackages`, `resolveActivity`, and `getPackageInfo`.
- Read from the Linuxoid compat root instead of Android.
- Files: `src/package_manager_stub.cpp`

#### P4.4 — ActivityManager stub

- Route `startActivity` and `getRunningTasks` through Linuxoid's own activity stack.
- Files: `src/activity_manager_stub.cpp`

#### P4.5 — SharedPreferences + SQLite

- Map them into the Linux compat root.
- Reuse Linux SQLite directly.
- Files: `src/storage_bridge.cpp`

#### P4.6 — Verify Settings navigates

- Open `com.android.settings`.
- Tap between screens.
- No Binder-driven crashes.
- Files: `tests/binder_smoke_test.cpp`

## P5 — Audio + Network + Browser

Duration: 3-4 weeks  
Goal: broaden app-class coverage and unfreeze the browser track.  
Outcome: audio works, network works, BrowserSession MVP ships.

### Tasks

#### P5.1 — Audio bridge

- Route `AudioTrack`, `AudioRecord`, and `libaaudio` into PipeWire or PulseAudio.
- Files: `src/audio_bridge.cpp`

#### P5.2 — Network passthrough

- Let Java sockets use Linux sockets directly.
- Stub `ConnectivityManager` as connected.
- Files: `src/network_stub.cpp`

#### P5.3 — Camera stub

- Map camera requests to V4L2 or return a clean unsupported error.
- Files: `src/camera_stub.cpp`

#### P5.4 — BrowserSession MVP

- Unfreeze the browser track here.
- Add `android.webkit.WebView`, a journal, bounded self-healing, and a DOM/JS bridge.
- Files: `src/browser_session.cpp`

#### P5.5 — 10-app compatibility matrix

- Run `verify-package-matrix` across 10 diverse apps.
- Classify failures by subsystem and feed them back into `P3` and `P4`.
- Files: `tests/compat_matrix_test.cpp`

## P6 — Polish + Release

Duration: 2-3 weeks  
Goal: Linuxoid ships as an installable tool.  
Outcome: `apt install linuxoid` or equivalent.

### Tasks

#### P6.1 — Packaging

- Bundle dependencies.
- Produce a single-binary or clean package flow.
- Targets: Ubuntu 22.04+, Arch, Fedora.
- Files: `packaging/`

#### P6.2 — `compatctl` UX pass

- Add progress bars, color, and `--json`.
- Keep MCP/harness integration clean.
- Files: `src/compatctl.cpp`

#### P6.3 — Docs rewrite

- Show real screenshots, not only diagrams.
- Add `INSTALL.md` and `COMPAT.md`.
- Remove scaffold-era language.
- Files: `README.md`, `docs/`

#### P6.4 — CI matrix

- Build and test on Ubuntu 22.04, 24.04, and Arch.
- Fail PRs on regressions.
- Files: `.github/workflows/ci.yml`

## Critical Path

```text
P0 (audit)
  -> P1 (dlopen NDK .so -> process lives)
       -> P2 (Wayland window -> pixel on screen)
            -> P3 (DEX/ART -> Java apps)
            -> P4 (Binder -> system services)
                 -> P5 (audio + network + browser)
                      -> P6 (ship)
```

`P1 -> P2` is the current blocker. Browser, Binder, audio, and packaging all sit downstream of first pixel.

## Frozen Until P5

- BrowserSession
- Self-healing recovery policy
- DOM and JS bridge work
- Permission and download brokers

## Dependency Notes

- Native execution still is not implemented; `P1` fixes that.
- `plan-native-spike` materializes assets but does not execute app code; `P1` and `P2` address that gap.
- `bootstrap-native-spike` and `native-execute-stub` now own the local bootstrap and child-runner surface with deterministic cwd, Linuxoid-only environment variables, fd hygiene, and structured JNI reporting, but they remain pre-bound-graphics, pre-DEX/ART, pre-real-Binder, and pre-full-resource-loading until later phases.
- `native-lifecycle-shim` now owns truthful pre-launch session handoff, process-state artifacts, and Binder-shaped service-manager paths, but it remains a scaffold seam until the project has real transport, class loading, and activity rendering behind those contracts.
- Waydroid and attached ADB remain regression oracles through `P0-P4`.
- All new commands must remain MCP- and harness-compatible: machine-readable output, stable artifact paths, and composable verification.
