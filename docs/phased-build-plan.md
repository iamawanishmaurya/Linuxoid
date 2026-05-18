# Linuxoid — Phased Build Plan

> Current state: scaffold `96/100` · execution `98/100`  
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

Current repo note as of `2026-05-18`: Linuxoid now also has a hardened direct `compatctl launch-apk <apk-path> [staging-root]` intake slice for real APKs. It can inspect stored and deflated ZIP entries, decode a useful subset of binary `AndroidManifest.xml` into the existing manifest/package/component/permission pipeline, stage ABI-matching native libraries plus assets into a deterministic session directory, `dlopen` the selected library, call `JNI_OnLoad` when present, create a minimal native activity/session object, and emit structured JSON. The real `/home/astra/Downloads/keyboard-0.1.28.apk` target now clears manifest, asset, and permission intake through this path and stops later at `launch_status: libraries_failed_to_load`, so the next narrow blocker is native library/runtime compatibility rather than manifest decoding. This slice is still intentionally narrow: full Java/Kotlin startup and ART-owned execution are still pending.

Current repo note as of `2026-05-17`: Linuxoid now also has an APK-launched native surface proof mode through `compatctl launch-apk --surface-proof <apk-path> [staging-root]` and `compatctl launch-apk-surface <apk-path> [staging-root]`. That mode binds the APK launch session to a deterministic surface/session object, drives a first-frame plus first-pixel proof, emits lifecycle-state JSON, and reports `backend: headless` explicitly when Linuxoid is not driving a real Wayland/EGL surface yet. This is still a native-only proof slice, not full Android graphics compatibility.

Current repo note as of `2026-05-17`: Linuxoid now also has an APK-launched asset/resource proof mode through `compatctl launch-apk --asset-proof <apk-path> [staging-root]`. That mode binds the staged APK session to a deterministic Linuxoid AssetManager/Resources-shaped bridge, lists staged assets in normalized sorted order, opens and checksums one asset proof path, detects `resources.arsc` plus `res/` metadata, and emits nested JSON showing `asset_bridge.ready` and metadata-only `resource_bridge` state. This is still a native-only metadata bridge, not full Android resource decoding, theme inflation, or Java/Kotlin ART execution.

Current repo note as of `2026-05-17`: Linuxoid now also has an APK-launched lifecycle/input proof mode through `compatctl launch-apk --lifecycle-proof <apk-path> [staging-root]`. That mode binds the staged APK session to a deterministic lifecycle controller, Looper-style event pump, and InputQueue foundation, visits ordered `created`, `started`, `resumed`, `paused`, `stopped`, and `destroyed` states, dispatches deterministic synthetic touch/key events, and emits nested `lifecycle`, `looper`, and `input_queue` JSON. This is still a native-only foundation slice, not full Android framework scheduling, Binder-backed services, or Java/Kotlin ART execution.

Current repo note as of `2026-05-17`: Linuxoid now also has an APK-launched activity proof mode through `compatctl launch-apk --activity-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`. That mode binds the staged APK session to a Linuxoid-owned `package_manager`, `intent_resolution`, and `activity_launch` contract, writes deterministic `activity-launch/package-record.json`, `activity-launch/intent-resolution.json`, and `activity-launch/activity-launch.json` artifacts, captures labels plus exported/enabled flags plus manifest intent filters, resolves either a manifest `MAIN` plus `LAUNCHER` component or an explicit component request, emits deterministic blocked reasons like `package_not_found`, `no_launcher_activity`, and `ambiguous_launcher_activities`, and ties activity-launch readiness back to lifecycle, surface, input, Binder, and DEX/bootstrap state. This is still a local Android-runtime-shaped contract, not full Android framework startup or `system_server`.

Current repo note as of `2026-05-18`: Linuxoid now also has **P8 Self-Healing Android Device Watchdog + Recovery Loop** through `compatctl launch-apk --self-heal-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`. That mode consumes the existing launch, surface, asset/resource, lifecycle, Looper, InputQueue, Binder, DEX/bootstrap, PackageManager, intent-resolution, and activity-launch health fields, chooses deterministic local recovery actions like `restage_assets`, `restart_surface`, `refresh_binder_services`, `rebuild_dex_bootstrap`, and `rerun_intent_resolution`, writes `self-healing-android-device/watchdog-report.json` plus `self-healing-android-device/recovery-journal.jsonl`, and stays explicit that this is a Linuxoid-owned local recovery loop rather than full Android framework healing.

Current repo note as of `2026-05-18`: Linuxoid now also has **P9 Android App Storage + Sandbox Contract** through `compatctl launch-apk --storage-proof <apk-path> [staging-root]`, and the same storage/sandbox health now feeds `compatctl launch-apk --self-heal-proof ...`. That mode materializes deterministic `sandbox/data/data/<package>`-style app directories, `files` plus `cache` roots, placeholder uid/gid plus permission metadata, a safe app-relative path resolver with traversal/absolute/symlink escape rejection, and a marker-file proof with checksum output. The Self-Healing Android Device watchdog can now consume `storage_health` plus `sandbox_health` and attempt deterministic `repair_app_storage` recovery while staying explicit that the current isolation level is `path_sandbox_only`, not real Linux UID/container isolation.

Current repo note as of `2026-05-18`: Linuxoid now also has **P10 Android Permissions + AppOps Contract** through `compatctl launch-apk --permissions-proof <apk-path> [staging-root]`, `compatctl inspect-apk-permissions <apk-path> [staging-root]`, and the same permission/AppOps health now feeds `compatctl launch-apk --self-heal-proof ...`. That mode parses plain-XML `uses-permission` entries when available, persists deterministic requested plus granted plus denied permission state under `sandbox/data/data/<package>/permissions`, emits stable AppOps records for storage-sensitive access, audio capture placeholder access, Binder/service access, activity launch access, and DEX/runtime access, validates or heals missing, malformed, incomplete, stale, and incompatible sandbox-backed contract files across repeated launches, and lets the Self-Healing Android Device watchdog attempt deterministic `rebuild_permission_state` recovery while staying explicit that this is still a Linuxoid-owned local permission/AppOps contract rather than full Android framework enforcement.

Current repo note as of `2026-05-18`: Linuxoid now also has **P11 Minimal ActivityManager/ProcessManager Contract** through `compatctl launch-apk --process-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`, `compatctl inspect-apk-process <apk-path> [staging-root]`, and the same process/activity-manager health now feeds `compatctl launch-apk --self-heal-proof ...`. That mode persists deterministic `activity_manager` plus `process_manager` artifacts under `sandbox/data/data/<package>/process-manager`, records Linuxoid-owned process identity placeholders like `user_id`, `app_id`, `uid_placeholder`, `gid_placeholder`, `process_name`, `pid_value`, `pid_source`, `start_reason`, `restart_policy`, and `termination_policy`, validates or heals missing, malformed, incomplete, stale, or incompatible process-manager files, and lets the Self-Healing Android Device watchdog attempt deterministic `rebuild_process_manager_state` recovery while staying explicit that this is still a Linuxoid-owned local process contract rather than full Android framework process execution.

Current repo note as of `2026-05-18`: Linuxoid now also has **P12 WindowManager + Wayland/EGL Surface Contract** through `compatctl launch-apk --window-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`, `compatctl inspect-apk-window <apk-path> [staging-root]`, and the same window-manager health now feeds `compatctl launch-apk --self-heal-proof ...`. That mode persists deterministic `window-state.json`, `window-session-map.json`, and `window-events.jsonl` artifacts under `sandbox/data/data/<package>/window-manager`, records Linuxoid-owned activity/process/surface mappings, exposes explicit `created`, `attached`, `visible`, `resized`, `hidden`, `destroyed`, `failed`, and `recovered` state reporting, validates or heals missing, malformed, incomplete, stale, or incompatible window-manager files, and lets the Self-Healing Android Device watchdog attempt deterministic `rebuild_window_manager_state` recovery while staying explicit that this is still a Linuxoid-owned local WindowManager/surface contract rather than full Android WindowManagerService or production graphics compatibility.

Current repo note as of `2026-05-18`: Linuxoid now also has **P13 Real ART Runtime Path / Java VM Bootstrap Contract** through `compatctl launch-apk --runtime-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`, `compatctl inspect-apk-runtime <apk-path> [staging-root]`, and the same runtime health now feeds `compatctl launch-apk --self-heal-proof ...`. That mode persists deterministic `runtime-state.json`, `runtime-session-map.json`, and `runtime-events.jsonl` artifacts under `sandbox/data/data/<package>/runtime-manager`, records Linuxoid-owned runtime-root discovery plus boot classpath plus runtime-library inputs, exposes explicit `unavailable`, `discovered`, `configured`, `bootstrapping`, `ready`, `failed`, `degraded`, and `recovered` state reporting, validates or heals missing, malformed, incomplete, stale, or incompatible runtime-manager files, and lets the Self-Healing Android Device watchdog attempt deterministic `retry_runtime_bootstrap` recovery while staying explicit that this is still a Linuxoid-owned local runtime bootstrap contract rather than full ART bytecode execution.

Current repo note as of `2026-05-18`: Linuxoid now also has **P14 Java/Kotlin APK Proof Contract** through `compatctl launch-apk --java-proof [--package <package>] [--component <component>] <apk-path> [staging-root]` and `compatctl inspect-apk-java <apk-path> [staging-root]`. That mode persists deterministic `java-proof-state.json`, `java-proof-session-map.json`, and `java-proof-events.jsonl` artifacts under `sandbox/data/data/<package>/java-proof`, proves that a Java/Kotlin-style APK can be wired end to end through the existing package, activity, process, window, and runtime contracts, validates or heals missing, malformed, incomplete, stale, or incompatible Java proof state, and keeps the Self-Healing Android Device diagnostics honest about the current boundary: bootstrap and lifecycle wiring are proved, but full Java/Kotlin bytecode execution is still future work.

Current repo note as of `2026-05-18`: Linuxoid now also has **P15 Third-Party APK Compatibility Sprint** through `compatctl inspect-apk-compatibility <apk-path> [staging-root]` and `compatctl inspect-apk-compatibility-suite <suite-root> <apk-path> [apk-path...]`. That mode persists deterministic `compatibility-report.json`, `compatibility-domains.json`, and `compatibility-events.jsonl` artifacts under `sandbox/data/data/<package>/compatibility`, emits a suite-level `suite-compatibility-report.json`, and classifies manifest, package metadata, activity/intent resolution, permissions/AppOps, storage sandbox, native/JNI load, assets/resources, process/session, window/surface, runtime bootstrap, Java/Kotlin proof, and Self-Healing Android Device diagnostics across `supported`, `partial`, `blocked`, `missing-runtime`, `missing-surface`, `missing-native-lib`, `needs-real-art`, `recovered`, and `degraded` without introducing Android SDK, Gradle, network, emulator, ADB, Waydroid, or live-display test dependencies.

Current repo note as of `2026-05-18`: Linuxoid now also has a **Managed Activity Start Checkpoint** through `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]` and `compatctl inspect-apk-first-start <apk-path> [staging-root]`. That checkpoint still reuses the real direct-session package, activity, process, window, runtime, and Java-proof seams and persists `sandbox/data/data/<package>/first-app-start/first-app-start.json`, but it now exposes two honest layers instead of one vague managed-runtime claim. On the healthy deterministic fixture path, Linuxoid still resolves `Lcom/example/launchapk/MainActivity;`, materializes a deterministic lifecycle receiver placeholder, crosses the stubbed `Landroid/app/Activity;->onCreate()V` framework boundary, executes the tiny `StateCarrier.<init>()V` constructor path, models the `MainActivity.currentCarrier -> StateCarrier.value:I` object-reference field seam, and reaches a real DEX `return`. On the keyboard-identity synthetic seam, Linuxoid now goes one step deeper: it resolves `Lorg/futo/inputmethod/latin/uix/settings/SettingsActivity;`, materializes the lifecycle receiver in the parameter-register window, materializes a deterministic `Landroid/os/Bundle;` placeholder, and reaches the stubbed `Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V` framework boundary with exact `framework_boundary_state`, `framework_boundary_reason`, and `blocking_reason` reporting instead of stopping at `invoke_receiver_missing`. The real `/home/astra/Downloads/keyboard-0.1.28.apk` launch path still blocks earlier at `launch_status: libraries_failed_to_load` and `surface_not_ready_for_first_app_start`, so full ART-owned managed execution is still not claimed here.

Current repo note as of `2026-05-18`: The next direct-runtime milestone after this checkpoint is still **P16 Managed Bytecode Invocation + ActivityThread Contract**, but it should now start from a narrower concrete blocker set: the managed seam no longer stops at missing receiver propagation, and now stops at the stubbed `Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V` boundary with `next_blocker: bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`. The existing sandbox-backed `package_manager`, `intent_resolution`, `activity_launch`, `storage`, `permissions`, `app_ops`, `activity_manager`, `process_manager`, `window_manager`, `runtime_bridge`, `java_apk_proof`, and `compatibility` contracts remain the real input seams. What remains blocked after this checkpoint is not DEX parsing anymore; it is upstream native-library plus surface readiness for the real keyboard APK, real ART-owned framework dispatch for the lifecycle Bundle boundary, ActivityThread-style application bootstrap, richer framework-level permission/AppOps enforcement, and broader Java/Kotlin execution across real third-party APKs.

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

Current repo note as of `2026-05-18`: Linuxoid now has a verified pre-ART APK resource bridge that can inspect plain APK/ZIP manifest metadata, list normalized asset paths, reject traversal, and emit stable readiness JSON for harnesses. Linuxoid also now has a verified self-healing runtime health skeleton that can classify staging, native-load, surface, input, Binder, DEX/classloader, activity-bootstrap, and bootstrap-execution readiness, select deterministic recovery actions, emit replayable JSONL traces, materialize stable recovery-plan artifacts, attach explicit `action_rank`, `retry_budget`, and `recovery_scope` metadata to those bounded recovery decisions, and now write a deterministic diagnostic trace index with per-source fingerprints and event boundaries. Linuxoid now also has a verified ART/classloader preparation fixture that inventories staged dex entries, normalizes manifest target classes, writes deterministic classpath artifacts, and now records a deterministic `art-runtime-probe-inventory.json` artifact plus probe-detection reason and probe-capability classification for override, host, and missing-runtime paths; a verified offline DEX class-resolution fixture that resolves manifest-target descriptors from real staged DEX contents and writes stable resolution artifacts; a host-ART runtime smoke seam that writes invocation-plan, runtime-log, and JSONL trace artifacts, distinguishes detection-only host `app_process` or `app_process64` surfaces from bootstrap-capable host `dalvikvm` or `dalvikvm64` surfaces, and now prepares a real host-side ART class-resolution attempt when a safe runtime exists; a deterministic activity-bootstrap fixture that turns launcher resolution plus Binder readiness plus runtime-smoke evidence into stable post-class-resolution bootstrap-plan, trace, and result artifacts; and a deterministic bootstrap-execution fixture that upgrades that seam into stable execution-plan, trace, and result artifacts. Linuxoid can now also exercise the runtime-smoke and bootstrap-execution seams through an explicit Linuxoid-owned ART probe override in fixtures, which gives the supervised runner path end-to-end coverage even on hosts that do not ship ART locally. Linuxoid now also reuses one opened APK archive plus the staged bundle manifest during native runtime diagnosis, keeps dex-only bundles out of false native-loader failure states, and exposes a real local `native` runtime bridge for staged package discovery, metadata lookup, preflight, and bootstrap-execution handoff instead of a hard stub. Full `resources.arsc` semantics, binary XML handling, actual host ART app bootstrap, and in-process `PathClassLoader` execution are still pending.

Current repo note as of `2026-05-18`: Linuxoid now also has a direct `compatctl launch-apk --dex-proof <apk-path> [staging-root]` slice. That path stages `classes.dex`, `classes2.dex`, and similar entries into the deterministic APK session root, parses safe DEX magic plus header/count metadata, records checksums and staged file paths, and emits a session-bound `art_bootstrap` contract with `class_loader_ready` while still keeping `art_runtime_available: false` and `java_execution_supported: false`. This is the first honest Java/Kotlin bootstrap probe on the direct APK path, not full ART execution.

Current repo note on self-healing scope: Linuxoid can now diagnose runtime failures, classify them into deterministic subsystem states, expose direct summary fields like `dependency_blocked` plus `failing_subsystem_count` plus `recovery_actions_selected`, expose the six required health areas again through an explicit `core_subsystem_records` projection with stable counts and readiness fields, expose the four required deterministic recovery cases again through an explicit `canonical_recovery_scenarios` contract, expose the replayable JSONL trace bundle again through explicit completeness and source-summary fields, and now expose the host ART probe gate again through explicit inventory-path, detection-reason, and probe-capability fields. Linuxoid now also exposes those seams through stable public commands and a real local `native` runtime bridge for staged package discovery, staged metadata lookup, staged-package preflight, verification, and bootstrap-execution handoff. That same staged-package preflight contract now feeds the backend-neutral `verify-package` and `verify-package-matrix` commands too, so native verification reports package visibility, target selection, component readiness, health verdicts, recovery details, replay handles, and now the six-area core-runtime subsystem projection before launch instead of jumping straight to a launch attempt or burying the diagnosis inside a nested preflight blob. Linuxoid now also surfaces the health-trace JSONL, recovery-actions JSONL, merged diagnostic events JSONL, trace index, replay completeness counts, per-source trace details, ART probe inventory path, ART probe detection reason, ART probe capability, detailed recovery rank/retry/scope/reason metadata, direct `Runtime Health Classification` plus `Runtime Overall Ready` verdict lines, and direct `Runtime Core Subsystems*` lines directly in `preflight-runtime native`, `verify-package native`, `verify-package-matrix native`, and `launch-package native`, so a blocked staged launch or matrix row can be diagnosed from the public bridge contract without opening the deeper health JSON first. Linuxoid now also pins that public bridge contract with explicit blocked-host-ART regressions for deterministic health classification, deterministic recovery selection, repeated runtime-health JSON stability, no false success, repeated `verify-package native` report plus replay JSON stability, and repeated `verify-package-matrix native` row-level stability. Linuxoid also now distinguishes override-backed ART/bootstrap success from genuine host ART launch success on the native `launch-package` path: override-backed success is preserved in artifacts and fixture commands, but it only counts as a successful native app launch when `LINUXOID_NATIVE_ALLOW_RUNTIME_OVERRIDE=1` is set explicitly, while a detected host `app_process` or `app_process64` surface still stays honest as detection-only and not yet bootstrap-capable, and a detected host `dalvikvm` or `dalvikvm64` surface is now treated as the same bootstrap-capable class. Linuxoid also splits post-class-resolution bootstrap into an `activity_bootstrap_readiness` planning seam and a `bootstrap_execution_readiness` execution seam with deterministic context, supervised runner, runner-state, and phase-log artifacts, and it can drive that execution seam through an explicit Linuxoid-owned ART probe override during fixtures. When that deeper override-backed path succeeds, runtime health now lets `dex_classloader_readiness` and `bootstrap_execution_readiness` converge to `ready` instead of leaving them generically pending. In other words, Linuxoid now has a strong diagnosis and replay contract, but not a finished Android execution contract. That does **not** yet mean Linuxoid can autonomously repair execution or finish Android app startup on its own, and it does **not** yet mean a real staged foreground app is starting through host ART by default. A successful self-healing pass today can still end in a truthful `Ready For Launch: no`, `Launch OK: no`, or `Packages Passed: 0/1` result on the public native bridge, as long as the blocked state, bounded next step, and replay artifacts are correct. The remaining blockers are still real ART-owned app bootstrap beyond the current class-resolution, bootstrap-planning, and bootstrap-execution seams, richer Binder/framework behavior, bound Wayland+EGL rendering, full IME/text composition, and full Android resource semantics.

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

Current repo note as of `2026-05-17`: Linuxoid now has a verified local Binder-shaped service-registry foundation that writes deterministic service registration, lookup, and transaction artifacts for `package_manager`, `activity_manager`, and an app-local placeholder service, persists both `service-lookups.json` and `service-lookups.jsonl`, records an honest missing-service lookup plus stable limitation flags, threads those artifacts into the lifecycle shim, and lets the Self-Healing Android Device runtime classify service-lookup failures into a deterministic recovery plan. Full Parcel semantics, kernel Binder, and real cross-process Android Binder or `system_server` behavior are still pending.

Current repo note as of `2026-05-17`: Linuxoid now also has a direct APK-session PackageManager plus MAIN/LAUNCHER intent plus activity-launch contract through `compatctl launch-apk --activity-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`. That proof path materializes a deterministic package record, resolves either a launcher intent or an explicit component from manifest metadata, records deterministic blocked reasons and recovery actions, and writes an activity-launch record whose readiness is tied back to lifecycle, surface, input, Binder, and DEX/bootstrap state. This is still a Linuxoid-owned local contract, not full `PackageManagerService`, `ActivityManagerService`, or framework app startup.

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

- Native execution is now partially implemented through local bootstrap, staged-package runtime bridging, and supervised bootstrap-execution seams, but full Android app execution is still pending and remains the main `P1` through `P4` gap.
- `plan-native-spike` materializes assets but does not execute app code; `P1` and `P2` address that gap.
- `bootstrap-native-spike` and `native-execute-stub` now own the local bootstrap and child-runner surface with deterministic cwd, Linuxoid-only environment variables, fd hygiene, and structured JNI reporting, but they remain pre-bound-graphics, pre-DEX/ART, pre-real-Binder, and pre-full-resource-loading until later phases.
- `native-lifecycle-shim` now owns truthful pre-launch session handoff, process-state artifacts, and Binder-shaped service-manager paths, but it remains a scaffold seam until the project has real transport, class loading, and activity rendering behind those contracts.
- Waydroid and attached ADB remain regression oracles through `P0-P4`.
- All new commands must remain MCP- and harness-compatible: machine-readable output, stable artifact paths, and composable verification.
