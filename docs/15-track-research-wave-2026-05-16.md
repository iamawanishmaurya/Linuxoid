# Linuxoid 15-Track Research Wave — 2026-05-16

This document collects the findings from the 15-task Linuxoid research wave requested for the direct Android-on-Linux architecture gaps.

## Scope

- one research track per listed architecture gap
- MCP- and harness-compatible conclusions
- emphasis on the critical path toward the first native app

## Wave Status

| Task | Topic | Status | Notes |
| --- | --- | --- | --- |
| 1 | Process Bootstrap | Complete | Next move is a real `native-process-bootstrap` parent command that forks a child, updates session state, and treats bootstrap JSON as the source of truth. |
| 2 | DEX / Class Loader | Complete | Recommended path is ART plus `PathClassLoader`, with the first gate stopping at class resolution from staged `base.apk` rather than full Activity startup. |
| 3 | JNI Plumbing | Complete | ABI boundary is still invented; real JNI/NDK-compatible types and VM lifecycle need to land before app-owned libraries can be trusted. |
| 4 | ART Runtime Shim | Complete | Recommended path is a host-ART sidecar with Linuxoid-owned dex execution first, not immediate full Android app bootstrap. |
| 5 | Binder IPC | Complete | Recommended first layer is Linuxoid-owned userspace Binder over Unix sockets with loopback-first testing and live service registration. |
| 6 | Graphics / Window | Complete | Wayland is the first stack; blank-window and EGL-color proof should land before any real Calculator UI expectations. |
| 7 | Input / IME | Complete | Input depends on a real native process, focused-window ownership, looper-backed event queues, and a separate text/IME broker before direct IME APK hosting is even in scope. |
| 8 | Resource Loader | Complete | `bundle/base.apk` should stay the source of truth behind a three-layer stack: `ApkArchive`, `AssetStore`, and `ResourceTable`, with asset APIs landing before `resources.arsc` breadth. |
| 9 | Android Services Layer | Complete | The first services slice should stay tiny: loopback `ServiceManager`, minimal `PackageManager`, single-task `ActivityManager`, and a process-local resolver seam. |
| 10 | 3-App Native Matrix | Complete | The matrix should stay an honesty ladder: one simple native success, one complex DEX/ART success target, and one permanent honest-failure slot. |
| 11 | BrowserSession Skeleton | Complete | The first BrowserSession should be a tiny Linuxoid-owned WebView host with one session per run, deterministic artifacts, and no early planner/DOM/recovery overbuild. |
| 12 | Self-Healing Recovery Policy | Complete | Recovery should be a separate invariant contract with numeric budgets, explicit permission/auth/payment stop rules, and append-only policy artifacts. |
| 13 | DOM / JS Bridge | Complete | The first browser bridge should be anchor-first and WebView-based: document-start probe + message RPC + bounded `evaluateJavascript`, with accessibility only as a secondary fallback. |
| 14 | Trace / Replay / Eval Artifacts | Complete | Browser runs need a full typed contract: authoritative JSON envelopes, append-only traces, normalized DOM/page snapshots, replay inputs, and eval verdicts with anti-fake-progress checks. |
| 15 | Storage / Sandbox | Complete | Linuxoid should move from one flat `sandbox_root` to a per-package mount namespace with explicit guest paths, deterministic manifests, and no broad shared-storage exposure in the MVP. |

## Cross-Track Synthesis

### Immediate Execution Order

1. **Process truth first**: land `native-process-bootstrap`, session-state truth, and storage manifests together so Linuxoid stops faking `RESUMED` or collapsing everything into one `sandbox_root`.
2. **Native bundle realism next**: add native-library staging plus asset-backed `AAssetManager`/APK archive work so direct-process runs use real app inputs rather than placeholder folders.
3. **DEX boundary after that**: start a host-ART sidecar, construct `PathClassLoader`, and make JNI/VM ownership real enough to resolve classes from staged `base.apk`.
4. **Service loopback before app breadth**: replace placeholder services with loopback Binder-shaped `package_manager`, `activity_manager`, and a narrow resolver seam so lifecycle state comes from real events.
5. **Window and focus before rich UX**: land blank Wayland/EGL proof, then `ANativeWindow`, then focused keyboard/pointer input, clipboard, and later text/IME composition.
6. **Keep the matrix honest**: use a fixed three-slot ladder with one simple native success, one real DEX/ART foreground target, and one permanent classified failure canary.
7. **Keep browser work frozen until `P5`**: when the browser track reopens, build it in the order `BrowserSession Skeleton -> Recovery Policy -> DOM / JS Bridge -> Trace / Replay / Eval`.

### Main Non-Goals For The Near Term

- do not re-expand into Waydroid-backed “success” metrics for native progress
- do not expose broad host filesystem access just to make storage appear to work
- do not start browser healing, DOM tooling, or eval harness work before Linuxoid can host Android UI and app code directly
- do not pretend shared storage, provider graphs, or full Android security parity exist before the storage bridge and service layer actually enforce them

## Task 3 — JNI Plumbing

### Summary

- The current native runner boundary is [`ExecuteNativeStub`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:72>).
- The JNI seam is still a tiny stub in [`src/jni_stub.cpp`](</home/astra/codex/wine-for-android/src/jni_stub.cpp:9>).
- The local `JNIEnv`, `JavaVM`, and `ANativeActivity` definitions in [`include/wfa/native_types.hpp`](</home/astra/codex/wine-for-android/include/wfa/native_types.hpp:18>) are not yet ABI-compatible with real JNI/NDK layouts.

### Recommended Path

1. replace the invented JNI/NDK boundary types with ABI-compatible ones
2. stage real app libraries into `library_root`
3. create a real VM and attach `env` plus `clazz` before feeding control to `ANativeActivity_onCreate`

### Repo Touchpoints

- [`src/native_execute_stub.cpp`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:72>)
- [`src/jni_stub.cpp`](</home/astra/codex/wine-for-android/src/jni_stub.cpp:9>)
- [`include/wfa/native_types.hpp`](</home/astra/codex/wine-for-android/include/wfa/native_types.hpp:18>)
- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:208>)
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:119>)

### Immediate Implementation Moves

1. add APK native-lib extraction plus ABI selection to the staging path
2. introduce a real JNI/NDK boundary module with VM and thread-attach ownership
3. bootstrap a first ART-backed class loader so `vm`, `env`, and `clazz` become real execution inputs

## Task 1 — Process Bootstrap

### Summary

- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:191>) already creates the native package tree and bootstrap manifests.
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:77>) rehydrates bootstrap state and writes lifecycle files, but it hard-codes a post-launch worldview before any process exists.
- [`src/native_execute_stub.cpp`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:72>) already has the runner core Linuxoid should reuse inside a child process.

### Recommended Path

1. add a real parent-side `native-process-bootstrap <activity-bootstrap.json>` command
2. make `activity-bootstrap.json` the launch source of truth
3. let the parent create and update session state while the child runs the existing native runner

### Repo Touchpoints

- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:191>)
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:77>)
- [`src/native_execute_stub.cpp`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:72>)
- [`src/runtime_bridge.cpp`](</home/astra/codex/wine-for-android/src/runtime_bridge.cpp:626>)
- [`tests/test_main.cpp`](</home/astra/codex/wine-for-android/tests/test_main.cpp:672>)

### Immediate Implementation Moves

1. extend lifecycle session files to carry `pid`, `process_state`, `exit_code`, `failure_reason`, and log paths
2. add `native-process-bootstrap` that parses the bootstrap manifest, forks, redirects stdio, and updates session state
3. refactor `ExecuteNativeStub` into child-runner logic that reports milestones and does not `dlclose()` on the live path

## Task 4 — ART Runtime Shim

### Summary

- ART itself is probably not the hardest boundary; Android framework bootstrap is.
- The recommended near-term path is a **host ART sidecar**, not an in-process `libart` embed.
- Linuxoid should first prove **Linuxoid-owned dex execution**, not third-party Activity startup.

### Recommended Path

1. build or stage pinned AOSP host ART artifacts
2. launch a Linuxoid-owned child process that starts ART on the host
3. load a Linuxoid dex fixture through `DexClassLoader` and invoke a known entrypoint

### Repo Touchpoints

- [`src/native_execute_stub.cpp`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:150>)
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:119>)
- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:175>)
- [`src/runtime_bridge.cpp`](</home/astra/codex/wine-for-android/src/runtime_bridge.cpp:626>)

### Immediate Implementation Moves

1. prove host ART starts on Linux with a trivial Linuxoid dex fixture
2. load dex from the staged APK/bundle and invoke a known static method
3. verify JNI/native library lookup through ART using Linuxoid’s staged `library_root`

## Task 5 — Binder IPC

### Summary

- The current `binder_registry` is only a placeholder emitted by the lifecycle shim.
- The recommended MVP is a Linuxoid-owned Binder-shaped transport over Unix domain sockets.
- Kernel `binderfs` is explicitly not the recommended first step.

### Recommended Path

1. define a Linuxoid Binder core seam (`IBinder`-like handle, `Parcel`, service manager)
2. implement an in-process loopback transport first
3. add the Unix-socket transport behind the same interface

### Repo Touchpoints

- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:119>)
- [`include/wfa/native_lifecycle.hpp`](</home/astra/codex/wine-for-android/include/wfa/native_lifecycle.hpp:11>)
- [`tests/test_main.cpp`](</home/astra/codex/wine-for-android/tests/test_main.cpp:793>)
- [`docs/phased-build-plan.md`](</home/astra/codex/wine-for-android/docs/phased-build-plan.md:232>)

### Immediate Implementation Moves

1. introduce `include/wfa/binder.h` with Binder-shaped interfaces plus transaction codes
2. make the lifecycle shim register live services and write endpoints/descriptors into `services.txt`
3. implement `package_manager` and `activity_manager` as the first real Binder services with smoke tests

## Task 2 — DEX / Class Loader

### Summary

- The best first path is **ART + PathClassLoader + Linuxoid-owned process bootstrap**.
- `DexClassLoader` is not the preferred first choice because Linuxoid is already modeling an installed-app layout.
- The first real gate should be class resolution from staged `base.apk`, not full Activity startup.

### Recommended Path

1. start ART and construct a `PathClassLoader` for `bundle/base.apk` plus `library_root`
2. resolve the declared `Application` class or launcher activity class
3. only then grow into `Application` creation and a minimal `Context`/resources layer

### Repo Touchpoints

- [`src/apk_loader.cpp`](</home/astra/codex/wine-for-android/src/apk_loader.cpp:235>)
- [`src/package_layout.cpp`](</home/astra/codex/wine-for-android/src/package_layout.cpp:43>)
- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:191>)
- [`src/native_execute_stub.cpp`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:72>)
- [`src/runtime_bridge.cpp`](</home/astra/codex/wine-for-android/src/runtime_bridge.cpp:626>)

### Immediate Implementation Moves

1. add an ART smoke path behind the current runner seam: `dlopen(libart.so)` -> `JNI_CreateJavaVM`
2. construct a `PathClassLoader` for `bundle/base.apk` and prove class resolution
3. use the current dex-only Calculator APK as a DEX oracle while keeping the fixture/native proof for the pure native path

## Task 6 — Graphics / Window

### Summary

- The first rendering stack should be **Wayland + xdg-shell + wl_egl_window + EGL/GLES**.
- X11 is a later compatibility backend, not the fastest route to first window proof.
- The current dex-only Calculator bundle is not yet a literal graphics success target; first-window proof should begin with a blank/color fixture.

### Recommended Path

1. make Linuxoid own `wl_display`, `wl_surface`, `xdg_toplevel`, `wl_egl_window`, `EGLDisplay`, `EGLContext`, and `EGLSurface`
2. expose an Android-facing `ANativeWindow*` wrapper and store it in `ANativeActivity::window`
3. fire `onNativeWindowCreated` only after the Wayland surface and EGL surface are live

### Repo Touchpoints

- [`docs/phased-build-plan.md`](</home/astra/codex/wine-for-android/docs/phased-build-plan.md:120>)
- [`include/wfa/native_types.hpp`](</home/astra/codex/wine-for-android/include/wfa/native_types.hpp:18>)
- [`src/native_execute_stub.cpp`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:72>)
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:77>)

### Immediate Implementation Moves

1. open a blank Wayland window and paint a known EGL color
2. add a Linuxoid `ANativeWindow` bridge plus `eglCreateWindowSurface` shim/interposer
3. prove a fixture native library can draw into that window before aiming at the real Calculator activity

## Task 7 — Input / IME

### Summary

- The direct-process path is still blocked because the `native` backend remains a stub in [`src/runtime_bridge.cpp`](</home/astra/codex/wine-for-android/src/runtime_bridge.cpp:626>).
- The current native runner only reaches `ANativeActivity_onCreate`; it has no real input queue, no window callback plumbing, and no clipboard surface in [`include/wfa/native_types.hpp`](</home/astra/codex/wine-for-android/include/wfa/native_types.hpp:18>).
- Today’s IME proof is still runtime-backed through `provision-ime` and `adb-ime-status`, not Linuxoid-owned direct-process text input.

### Recommended Path

1. make the native backend real enough to host a live foreground process and focused window
2. add a looper-backed raw keyboard and mouse queue for one focused app
3. layer clipboard ownership plus a separate focused-text and IME composition broker on top of that

### Repo Touchpoints

- [`src/runtime_bridge.cpp`](</home/astra/codex/wine-for-android/src/runtime_bridge.cpp:626>)
- [`src/native_execute_stub.cpp`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:150>)
- [`src/looper_stub.cpp`](</home/astra/codex/wine-for-android/src/looper_stub.cpp:55>)
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:119>)
- [`src/manifest_assessment.cpp`](</home/astra/codex/wine-for-android/src/manifest_assessment.cpp:256>)
- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:153>)

### Immediate Implementation Moves

1. extend the lifecycle/session model with `input_manager`, `text_input_manager`, `clipboard_manager`, and focus ownership fields
2. implement one focused-window queue that maps Linux keyboard and pointer events into Android-shaped events backed by the looper seam
3. add a minimal focused-text contract for commit/backspace/enter before attempting full IME composition or direct keyboard-app hosting

## Task 8 — Resource Loader

### Summary

- Linuxoid already stages `bundle/base.apk` and threads `resource_root` and `library_root` through the native bundle plan, but it still does not read runtime assets or resources itself.
- The current `AAssetManager*` only carries the APK path and logs a message; it does not implement `AAsset` or `AAssetDir` behavior.
- The lifecycle shim already exposes a placeholder `resource_loader` service, which makes this a natural place for truthful capability promotion later.

### Recommended Path

1. keep `bundle/base.apk` as the single source of truth and treat `resource_root` as a cache, not the canonical resource source
2. split the resource work into `ApkArchive`, `AssetStore`, and `ResourceTable`
3. land real asset reads first, then binary XML parsing, then a narrow `resources.arsc` reader for app package `0x7f`

### Repo Touchpoints

- [`src/apk_loader.cpp`](</home/astra/codex/wine-for-android/src/apk_loader.cpp:149>)
- [`src/apk_loader.cpp`](</home/astra/codex/wine-for-android/src/apk_loader.cpp:235>)
- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:191>)
- [`src/native_execute_stub.cpp`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:72>)
- [`src/asset_manager_stub.cpp`](</home/astra/codex/wine-for-android/src/asset_manager_stub.cpp:8>)
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:119>)

### Immediate Implementation Moves

1. add a ZIP backend plus tests for stored and compressed asset entries
2. replace the current asset stub with real manager/asset/dir structs and asset-only APIs
3. add binary XML parsing and a minimal same-package `resources.arsc` reader only after asset reads are working

## Task 9 — Android Services Layer

### Summary

- The smallest Linuxoid-owned services layer that matters first is one that can support a single foreground app moving through `CREATED -> STARTED -> RESUMED`.
- The right first set is a loopback `ServiceManager`, a minimal `PackageManager`, a single-task `ActivityManager`, and a narrow process-local resolver seam.
- The current lifecycle shim still cheats by prewriting `created`, `started`, and `resumed` before any process exists.

### Recommended Path

1. replace placeholder services with real in-process loopback registrations
2. move lifecycle truth into `ActivityManager` rather than precomputing it in the shim
3. keep `ContentResolver`-like behavior local to staged resources and app-local storage until single-process foreground apps pass

### Repo Touchpoints

- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:119>)
- [`src/apk_loader.cpp`](</home/astra/codex/wine-for-android/src/apk_loader.cpp:235>)
- [`src/package_layout.cpp`](</home/astra/codex/wine-for-android/src/package_layout.cpp:43>)
- [`src/runtime_bridge.cpp`](</home/astra/codex/wine-for-android/src/runtime_bridge.cpp:797>)
- [`src/manifest_assessment.cpp`](</home/astra/codex/wine-for-android/src/manifest_assessment.cpp:245>)

### Immediate Implementation Moves

1. tighten manifest gating for provider-heavy apps before broadening the resolver surface
2. implement loopback Binder-shaped registrations for `package_manager` and `activity_manager`
3. stop prewriting `RESUMED`; persist lifecycle state only from actual attach/start/resume events

## Task 10 — 3-App Native Matrix

### Summary

- Linuxoid should use a fixed honesty ladder rather than a rotating “three green apps” score.
- The first three slots should be: a Linuxoid-owned minimal `NativeActivity` success app, `org.fdroid.fdroid` as the first real DEX/ART foreground target, and `org.futo.inputmethod.latin` as the honest failure canary.
- The current staged `com.android.calculator2` APK should remain a supplemental negative oracle because it is dex-only and not a valid `P1` native success gate.

### Recommended Path

1. make each matrix slot deterministic with required artifacts and explicit verdict fields
2. keep runtime-backed Waydroid or `attached-adb` runs as regression oracles only, never as native passes
3. preserve the failure slot until Linuxoid truly implements the missing runtime surface instead of deleting the red case

### Repo Touchpoints

- [`README.md`](</home/astra/codex/wine-for-android/README.md:242>)
- [`docs/phased-build-plan.md`](</home/astra/codex/wine-for-android/docs/phased-build-plan.md:10>)
- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:138>)
- [`src/manifest_assessment.cpp`](</home/astra/codex/wine-for-android/src/manifest_assessment.cpp:264>)
- [`src/native_execute_stub.cpp`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:101>)
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:147>)

### Immediate Implementation Moves

1. define the deterministic artifact contract, including `matrix-summary.json`
2. add one Linuxoid-owned minimal `NativeActivity` fixture APK as the simple green slot
3. keep `org.fdroid.fdroid` and `org.futo.inputmethod.latin` fixed as the complex-success and honest-failure slots

## Task 12 — Self-Healing Recovery Policy

### Summary

- The browser track is still documentation-only and frozen until `P5`, but the repo already captures the right top-level constraints: preserve user intent separately from selectors, keep recovery bounded and fail-closed, and keep outputs MCP/harness-friendly and deterministic.
- The current browser note already contains a useful failure taxonomy, retry ladder, and stable run-path idea, which means the next step is refinement rather than reinvention.
- The policy should live outside the agent and outside page code as a Linuxoid-owned contract around the session.

### Recommended Path

1. make recovery policy an invariant session contract whose immutable fields include `intent`, `allowed_scope`, `allowed_capabilities`, `forbidden_actions`, and completed-subgoal state
2. classify failures into six classes: `transient_target`, `context_drift`, `flow_mismatch`, `browser_runtime`, `permission_boundary`, and `hard_policy_stop`
3. enforce a strict numeric ladder with limited retries, one session recreation, and hard stops for auth, MFA, CAPTCHA, payment, installs, security warnings, clipboard writes, and scope drift

### Repo Touchpoints

- [`docs/browser-self-healing-architecture.md`](</home/astra/codex/wine-for-android/docs/browser-self-healing-architecture.md:30>)
- [`README.md`](</home/astra/codex/wine-for-android/README.md:85>)
- [`README.md`](</home/astra/codex/wine-for-android/README.md:146>)
- [`docs/phased-build-plan.md`](</home/astra/codex/wine-for-android/docs/phased-build-plan.md:291>)

### Immediate Implementation Moves

1. define a policy schema first: request envelope, budget state, decision log, stop/result enums
2. add a standalone `browser_policy` module when `P5` opens so classification and budget accounting stay auditable
3. make permission/auth/payment boundaries explicit deny-or-ask rules before any self-heal behavior is allowed

## Task 13 — DOM / JS Bridge

### Summary

- Browser work is still docs-only, but the repo already defines the intended shape: `WebView`, `DomBridge`, recovery, and deterministic artifacts under `browser-runs/<run_id>/`.
- Linuxoid’s strongest reusable pattern here is the machine-readable artifact/report discipline from its native lifecycle and staging surfaces.
- The bridge should start as a Linuxoid-owned `DOM probe + message RPC`, not as a broad injected interface.

### Recommended Path

1. use `WebViewCompat.addDocumentStartJavaScript(...)` plus `WebViewCompat.addWebMessageListener(...)` as the primary bridge foundation
2. make the bridge anchor-first, not selector-first, with compact `DomAnchor` objects and scored re-anchoring
3. reserve accessibility fallback for browser chrome and Android/system prompts only, not page DOM as the primary model

### Repo Touchpoints

- [`docs/browser-self-healing-architecture.md`](</home/astra/codex/wine-for-android/docs/browser-self-healing-architecture.md:132>)
- [`README.md`](</home/astra/codex/wine-for-android/README.md:142>)
- [`docs/phased-build-plan.md`](</home/astra/codex/wine-for-android/docs/phased-build-plan.md:291>)
- [`src/package_layout.cpp`](</home/astra/codex/wine-for-android/src/package_layout.cpp:43>)
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:218>)

### Immediate Implementation Moves

1. define the `browser-act` artifact contract before browser behavior so bridge outputs have a stable home
2. create a document-start JS probe that emits actionable node inventories plus `DomAnchor` records
3. wire a thin native dispatcher around the message bridge with strict origin rules and separate trace records for DOM versus accessibility actions

## Task 14 — Trace / Replay / Eval Artifacts

### Summary

- Browser work is still frozen, but the repo already prefers authoritative JSON sidecars, deterministic package-root layouts, and anti-fake-progress reporting.
- The current browser note names the artifact families Linuxoid needs, but it does not yet define a schema.
- The artifact contract should be strong enough that MCP clients and harnesses can validate runs without reading human prose.

### Recommended Path

1. create one deterministic run root under `packages/<package>/<install_id>/browser-runs/<run_id>/`
2. make `request.json`, `result.json`, trace JSONL, replay inputs, and eval verdicts the authoritative machine surfaces
3. enforce anti-fake-progress checks so screenshots or partial state can never be misreported as browser success

### Repo Touchpoints

- [`docs/browser-self-healing-architecture.md`](</home/astra/codex/wine-for-android/docs/browser-self-healing-architecture.md:155>)
- [`docs/phased-build-plan.md`](</home/astra/codex/wine-for-android/docs/phased-build-plan.md:293>)
- [`src/package_layout.cpp`](</home/astra/codex/wine-for-android/src/package_layout.cpp:58>)
- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:266>)
- [`src/waydroid_integration.cpp`](</home/astra/codex/wine-for-android/src/waydroid_integration.cpp:148>)
- [`docs/solutions/desktopify-false-success-paths.md`](</home/astra/codex/wine-for-android/docs/solutions/desktopify-false-success-paths.md:11>)

### Immediate Implementation Moves

1. add a small browser contract module with typed writers and validators for `request.json`, `result.json`, traces, replay, and eval files
2. add a skeletal `compatctl browser-act --json` path that writes a full run directory before any real `WebView` behavior exists
3. write red-bar tests first for artifact completeness, terminal trace consistency, replayability, and screenshot-only false-success cases

## Task 15 — Storage / Sandbox

### Summary

- Linuxoid already creates Android-shaped package paths, but they are still filesystem scaffolding rather than an enforced storage model.
- The native runner still collapses `internalDataPath` and `externalDataPath` to the same `sandbox_root`, which would teach the wrong storage contract if left in place.
- The right first storage boundary is a Linuxoid-owned single-app-visible filesystem using a private mount namespace and selective bind mounts.

### Recommended Path

1. keep immutable install assets under `users/0/packages/<pkg>/<install_id>/` and mutable app state under package-private data roots
2. launch apps inside a private mount namespace that exposes only the package’s allowed Android-shaped guest paths
3. keep the MVP narrow: app-private storage, app-specific external-style storage, and OBB roots only; no broad shared storage or host-filesystem passthrough

### Repo Touchpoints

- [`src/package_layout.cpp`](</home/astra/codex/wine-for-android/src/package_layout.cpp:43>)
- [`include/wfa/package_layout.hpp`](</home/astra/codex/wine-for-android/include/wfa/package_layout.hpp:14>)
- [`src/apk_loader.cpp`](</home/astra/codex/wine-for-android/src/apk_loader.cpp:235>)
- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:191>)
- [`src/native_execute_stub.cpp`](</home/astra/codex/wine-for-android/src/native_execute_stub.cpp:72>)
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:77>)
- [`docs/phased-build-plan.md`](</home/astra/codex/wine-for-android/docs/phased-build-plan.md:254>)
- [`README.md`](</home/astra/codex/wine-for-android/README.md:120>)

### Immediate Implementation Moves

1. emit a machine-readable `storage-manifest.json` with guest-path mappings and mount modes
2. replace the single `sandbox_root` assumption with distinct guest paths for private data, code cache, external app data, and OBB
3. add a sandbox launcher that creates a private mount namespace and bind-mounts only the package’s allowed roots

## Task 11 — BrowserSession Skeleton

### Summary

- There is no browser implementation yet; the repo only has a frozen design note and a `P5` placeholder for `src/browser_session.cpp`.
- Linuxoid already has the right overall control style for a future BrowserSession: backend-neutral commands, deterministic artifact paths, and machine-readable reports.
- The native roadmap already provides the right seams to reuse: package roots, bootstrap artifacts, and lifecycle session files.

### Recommended Path

1. make the first BrowserSession a Linuxoid-owned browser app process that hosts one `WebView` for one `run_id`
2. keep the session boundary strict: one foreground WebView, one journal, one per-run profile, no tabs or shared long-lived profile state
3. fit BrowserSession onto the native execution path and package-root conventions instead of inventing a separate browser runtime model

### Repo Touchpoints

- [`docs/browser-self-healing-architecture.md`](</home/astra/codex/wine-for-android/docs/browser-self-healing-architecture.md:20>)
- [`README.md`](</home/astra/codex/wine-for-android/README.md:87>)
- [`src/main.cpp`](</home/astra/codex/wine-for-android/src/main.cpp:24>)
- [`src/package_layout.cpp`](</home/astra/codex/wine-for-android/src/package_layout.cpp:58>)
- [`src/native_spike.cpp`](</home/astra/codex/wine-for-android/src/native_spike.cpp:330>)
- [`src/native_lifecycle.cpp`](</home/astra/codex/wine-for-android/src/native_lifecycle.cpp:147>)

### Immediate Implementation Moves

1. add a BrowserSession scaffold layer only: run-root creation plus typed `request.json`, `session.json`, and `result.json`
2. add `browser-session-scaffold`, `browser-session-start`, `browser-session-status`, and `browser-session-stop` before real `WebView` behavior
3. attach a real one-WebView host only after the native UI, ART, and service layers are ready enough to support it
