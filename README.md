# Linuxoid

Linuxoid currently contains the first executable MVP scaffold for an Android-on-Linux compatibility project.

## Current Direction

- Core language: `C++20`
- Runtime strategy: backend-neutral attached Android targets on the path to a native Linux compatibility layer
- Product target: a Self-Healing Android Device runtime on Linux; browser-specific work remains a downstream track, not the primary target
- Agent integration strategy: stable MCP- and harness-friendly control surfaces, artifact layouts, and verification commands
- Browser strategy: researched and frozen until `P5`; the target remains a Linuxoid-owned Android WebView browser shell with a bounded self-healing recovery loop
- Current executable: `compatctl`

## Current Working Architecture

```mermaid
flowchart TB
  User["Linux User"] --> Desktop["Linux Desktop Entry or Shell Launcher"]
  Agent["Agent / MCP Client / Harness"] --> Compatctl
  Desktop --> Compatctl["Linuxoid compatctl"]

  subgraph Host["Linux Host"]
    Compatctl --> Status["Status and Checkpoint Engine"]
    Compatctl --> Loader["APK Loader and Manifest Assessor"]
    Compatctl --> NativeApkLaunch["P2 Native-only APK Launch Slice"]
    Compatctl --> NativeApkSurface["P2.1 APK-launched Native Surface Proof"]
    Compatctl --> NativeResources["APK Resource and Asset Readiness Bridge"]
    Compatctl --> NativeApkDex["P6 APK DEX/ART Bootstrap Probe"]
    Compatctl --> NativeApkActivity["P7 PackageManager / Intent / Activity Launch Contract"]
    Compatctl --> NativeApkJava["P14 Java/Kotlin APK Proof Contract"]
    Compatctl --> NativeApkCompat["P15 Third-Party APK Compatibility Sprint"]
    Compatctl --> NativePlanner["Native Spike Planner"]
    Compatctl --> NativeBootstrap["Native Activity Bootstrap"]
    Compatctl --> NativeExecute["P1 Native Execute Bootstrap"]
    Compatctl --> NativeLifecycle["Native Lifecycle and Service Shim"]
    Compatctl --> NativeRunner["P1 Native Execution Runner"]
    Compatctl --> NativeSurface["P2.1 Native Window Surface Fixture"]
    Compatctl --> NativeCallbacks["P2.2 Native Window Callback Fixture"]
    Compatctl --> NativeWayland["P2.1 Real Wayland Surface Fixture"]
    Compatctl --> NativeEgl["P2.2 EGL Smoke Fixture"]
    Compatctl --> NativeBridge["P2.3 ANativeWindow Bridge Fixture"]
    Compatctl --> NativeInput["P2.4 Focused Input Queue Fixture"]
    Compatctl --> NativeBinder["P5 Minimal Binder Service Registry Foundation"]
    Compatctl --> NativeArt["P3 ART Classloader Prep Fixture"]
    Compatctl --> NativeArtResolve["P3 Offline DEX Class Resolution Fixture"]
    Compatctl --> NativeArtRuntime["P3 Host ART Runtime Smoke Fixture"]
    Compatctl --> NativeArtBootstrap["P3 Activity Bootstrap Fixture"]
    Compatctl --> NativeArtBootstrapExec["P3 Bootstrap Execution Fixture"]
    Compatctl --> RuntimeHealth["Self-Healing Runtime Health Fixture"]
    Compatctl --> RuntimeRecovery["Self-Healing Runtime Recovery Plan Fixture"]
    Compatctl --> RuntimeDiagnostic["Self-Healing Runtime Diagnostic Replay Fixture"]
    Compatctl --> Preflight["Runtime Discovery and Preflight"]
    Compatctl --> Inspector["Package Metadata and Launcher Resolver"]
    Compatctl --> Desktopify["Desktop Artifact Generator"]
    Compatctl --> Verifier["Installed-Package Verifier and Matrix Verifier"]
    Compatctl --> ApkVerifier["APK-backed Host Verifier"]
    Compatctl --> Runtime["Runtime Bridge Layer"]
    Compatctl --> MachineSurface["Machine-readable Control and Artifact Surface"]
    Loader --> CompatRoot["Compat Root and Package Staging"]
    NativeApkLaunch --> CompatRoot
    NativeApkLaunch --> NativeExecute
    NativeApkLaunch --> NativeApkSurface
    NativeApkLaunch --> NativeApkDex
    NativeApkLaunch --> NativeApkActivity
    NativeResources --> CompatRoot
    NativePlanner --> CompatRoot
    NativePlanner --> NativeBundle["Native Bundle Layout / ABI Lib Staging / Bootstrap Spec"]
    NativeBootstrap --> NativeBundle
    NativeBootstrap --> NativeExecute
    NativeExecute --> NativeLifecycle
    NativeExecute --> NativeRunner
    NativeSurface --> NativeRunner
    NativeCallbacks --> NativeSurface
    NativeWayland --> NativeSurface
    NativeEgl --> NativeWayland
    NativeBridge --> NativeSurface
    NativeBridge --> NativeWayland
    NativeBridge --> NativeEgl
    NativeInput --> NativeBridge
    NativeBinder --> NativeLifecycle
    NativeApkDex --> NativeArt
    NativeApkDex --> NativeApkActivity
    NativeArt --> NativeArtResolve
    NativeArtResolve --> NativeArtRuntime
    NativeArtRuntime --> NativeArtBootstrap
    NativeArtBootstrap --> NativeArtBootstrapExec
    NativeBinder --> NativeArtBootstrap
    NativeBinder --> NativeApkActivity
    NativeLifecycle --> NativeArtBootstrap
    NativeLifecycle --> NativeApkActivity
    NativeArtBootstrapExec --> RuntimeHealth
    RuntimeHealth --> RuntimeDiagnostic
    RuntimeRecovery --> RuntimeDiagnostic
    NativeArt --> RuntimeDiagnostic
    NativeArtResolve --> RuntimeDiagnostic
    NativeArtRuntime --> RuntimeDiagnostic
    NativeRunner --> NativeStubs["JNI Stub / APK-backed Asset Bridge / Looper Stub / Signal Handler"]
    NativeSurface --> NativeMarker["Headless First-pixel Marker"]
    NativeCallbacks --> NativeCallbackJournal["Window Callback Journal"]
    NativeWayland --> NativeWaylandArtifact["surface-metadata.json"]
    NativeEgl --> NativeEglArtifact["egl-metadata.json"]
    NativeBridge --> NativeBridgeArtifact["native-window-bridge-metadata.json / events.jsonl"]
    NativeInput --> NativeInputArtifact["native-input-queue-metadata.json / events.jsonl"]
    NativeBinder --> NativeBinderArtifact["binder/service-manager.json / registered-services.json / service-lookups.json / service-lookups.jsonl / service-transactions.jsonl"]
    NativeArt --> NativeArtArtifact["art/classloader-plan.json / dex-inventory.json / art-runtime-probe-inventory.json / trace.jsonl"]
    NativeArtResolve --> NativeArtResolveArtifact["art/class-resolution-map.json / result.json / trace.jsonl"]
    NativeArtRuntime --> NativeArtRuntimeArtifact["art/runtime-smoke-result.json / invocation-plan.json / invocation.log / trace.jsonl"]
    NativeArtBootstrap --> NativeArtBootstrapArtifact["art/activity-bootstrap-plan.json / result.json / trace.jsonl"]
    NativeArtBootstrapExec --> NativeArtBootstrapExecArtifact["art/bootstrap-execution-plan.json / result.json / trace.jsonl / context.json / runner.sh / runner-state.json / phase logs"]
    RuntimeHealth --> RuntimeHealthArtifact["health/runtime-health.json / trace.jsonl / replay.json / diagnostic-replay.json"]
    RuntimeRecovery --> RuntimeRecoveryArtifact["health/runtime-recovery-plan.json / runtime-recovery-actions.jsonl"]
    RuntimeDiagnostic --> RuntimeDiagnosticArtifact["health/runtime-diagnostic-trace-index.json / runtime-diagnostic-events.jsonl / runtime-diagnostic-replay.json"]
    NativeResources --> NativeResourceArtifact["inspect-apk-resources JSON / staged asset roots"]
    NativeApkDex --> NativeApkDexArtifact["dex/classes*.dex / art/dex-proof.json / art/art-bootstrap.json"]
    NativeApkActivity --> NativeApkActivityArtifact["activity-launch/package-record.json / intent-resolution.json / activity-launch.json"]
    NativeApkJava --> NativeApkJavaArtifact["java-proof/java-proof-state.json / java-proof-session-map.json / java-proof-events.jsonl"]
    NativeApkCompat --> NativeApkCompatArtifact["compatibility/compatibility-report.json / compatibility-domains.json / compatibility-events.jsonl / suite-compatibility-report.json"]
    NativeBinder --> NativeBinderTransportArtifact["binder/transport-messages.jsonl"]
    NativeLifecycle --> NativeStubRunner["Linuxoid-owned Native Entrypoint Stub"]
    NativeExecute --> NativeSession["Truthful Session / Runner Logs / JSON Reports"]
    NativeApkLaunch --> NativeApkLaunchArtifact["launch-apk-report.json / staged session dir / launch-bootstrap.json"]
    NativeApkSurface --> NativeApkSurfaceArtifact["surface-session.json / surface-events.jsonl / first-pixel-marker.txt"]
    MachineSurface --> Reports["Reports / Status / Bootstrap Specs"]
    Preflight --> Runtime
    Inspector --> Runtime
    Desktopify --> DesktopFiles[".desktop Files and Launcher Scripts"]
  end

  Runtime --> BackendContract["Installed-package Backend Contract"]
  Runtime --> AdbBridge["ADB Activity and IME Commands"]

  subgraph Backend["Current and Future Backends"]
    BackendContract --> WaydroidAdapter["Waydroid Adapter"]
    BackendContract --> AttachedAdb["Attached ADB Target"]
    BackendContract --> NativeStub["Native Linux Runtime Stub"]
    WaydroidAdapter --> WaydroidCLI["waydroid app and session commands"]
    WaydroidCLI --> Android["Android Userspace"]
    AttachedAdb --> Android
    AdbBridge --> Android
    Android --> InstalledApps["Installed Android Apps"]
  end

  Compatctl --> KeyboardFlow["APK-backed IME Provisioning Flow"]
  KeyboardFlow --> Loader
  KeyboardFlow --> Desktopify
  KeyboardFlow --> ApkVerifier
  KeyboardFlow --> Runtime
  Runtime --> KeyboardApp["FUTO Keyboard and F-Droid APK-backed Proofs"]

  Compatctl --> InstalledFlow["Installed-package Linux Launch Flow"]
  InstalledFlow --> Verifier
  Verifier --> DesktopFiles
  Verifier --> BackendContract
  InstalledApps --> ProvenApps["Calculator, Settings, and F-Droid"]
  NativeStubRunner --> NativeStub
  NativeBundle --> NativeProof["Calculator Native Spike Proof"]
  NativeLifecycle --> NativeLifecycleProof["Calculator Lifecycle Shim Proof"]
  NativeRunner --> NativeFixtureProof["Fixture Native Activity 5s Proof"]
  NativeRunner --> NativeOracleProof["Dex-only Calculator Missing-lib Oracle"]
  NativeSurface --> NativePixelProof["Headless First-pixel Marker Proof"]
  NativeCallbacks --> NativeCallbackProof["Headless Window Callback Proof"]
  NativeWayland --> NativeWaylandProof["Real wl_display / wl_surface Proof"]
  NativeEgl --> NativeEglProof["Real EGL Context / Pbuffer Proof"]
  NativeBridge --> NativeBridgeProof["ANativeWindow Bridge Contract Proof"]
  NativeInput --> NativeInputProof["Focused Input Queue Contract Proof"]
  NativeBinder --> NativeBinderProof["Binder-shaped Local Service Manager Proof"]
  NativeArt --> NativeArtProof["DEX Inventory / ART Classpath Prep Proof"]
  NativeArtResolve --> NativeArtResolveProof["Offline Manifest-target DEX Resolution Proof"]
  NativeArtRuntime --> NativeArtRuntimeProof["Host ART Invocation Smoke Proof"]
  NativeArtBootstrap --> NativeArtBootstrapProof["Application / Activity Bootstrap Attempt Proof"]
  NativeArtBootstrapExec --> NativeArtBootstrapExecProof["Bootstrap Execution Attempt Proof"]
  RuntimeHealth --> RuntimeHealthProof["Self-Healing Runtime Health / Replay Proof"]
  RuntimeRecovery --> RuntimeRecoveryProof["Deterministic Runtime Recovery Plan Proof"]
  NativeResources --> NativeResourceProof["APK Manifest / Asset Readiness Proof"]
  NativeApkActivity --> NativeApkActivityProof["PackageManager / MAIN-LAUNCHER Intent / Activity Launch Contract Proof"]
  NativeApkJava --> NativeApkJavaProof["Java/Kotlin-style APK Bootstrap Wiring Proof"]
  NativeStubRunner --> NativeStubProof["Generated Entrypoint Now Calls Native Runner"]
```

This diagram is the current working architecture and should stay in sync with the verified Linuxoid flow on GitHub.

## Architecture Constraints

- Keep Linuxoid control paths scriptable and machine-friendly so MCP clients, agent runtimes, and validation harnesses can drive them without reverse engineering human-only output.
- Preserve stable artifact locations for staged APKs, bootstrap manifests, reports, and launch scripts so harnesses can discover and validate state deterministically.
- Prefer backend-neutral commands and structured intermediate files over backend-specific ad hoc flows.
- Treat Waydroid, attached ADB, and future native execution as pluggable backends behind the same agent-usable control surface where possible.
- For browser-agent work, preserve user intent separately from selectors, page paths, or one-off recovery tactics so self-healing can retry safely without drifting scope.
- Browser recovery must stay bounded and fail-closed: idempotent fixes may auto-heal, but scope expansion, destructive actions, security prompts, and ambiguous targets must stop.

## Mermaid Update Rule

- Update the **Current Working Architecture** Mermaid graph in the same commit as every meaningful change to runtime flow, backend contracts, native execution slices, or verification surfaces.
- Update the **Target Architecture** Mermaid graph whenever the long-term no-runtime design changes.
- Keep the README diagrams GitHub-ready so the current state of Linuxoid is visible without opening source files first.

## Execution Plan

Linuxoid now treats the phased execution plan as the repo-facing source of truth for the direct-runtime push:

- Current state: scaffold `96/100`, execution `98/100`
- Current focus: `P1 NDK Execution Core -> P2 Window + Graphics`
- Critical path: `P1 NDK Execution Core -> P2 Window + Graphics`
- Browser work is frozen until `P5`

Plan document: [docs/phased-build-plan.md](/home/astra/codex/wine-for-android/docs/phased-build-plan.md)
Detailed gap research: [docs/15-track-research-wave-2026-05-16.md](/home/astra/codex/wine-for-android/docs/15-track-research-wave-2026-05-16.md)

## Direct APK Proof Modes

Linuxoid's direct `launch-apk` path now supports these session-bound proof modes:

- `compatctl launch-apk <apk-path> [staging-root]`
- `compatctl launch-apk --surface-proof <apk-path> [staging-root]`
- `compatctl launch-apk --asset-proof <apk-path> [staging-root]`
- `compatctl launch-apk --lifecycle-proof <apk-path> [staging-root]`
- `compatctl launch-apk --dex-proof <apk-path> [staging-root]`
- `compatctl launch-apk --storage-proof <apk-path> [staging-root]`
- `compatctl launch-apk --permissions-proof <apk-path> [staging-root]`
- `compatctl launch-apk --java-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
- `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
- `compatctl inspect-apk-first-start <apk-path> [staging-root]`
- `compatctl inspect-apk-java <apk-path> [staging-root]`
- `compatctl inspect-apk-permissions <apk-path> [staging-root]`
- `compatctl launch-apk --activity-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
- `compatctl launch-apk --process-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
- `compatctl inspect-apk-process <apk-path> [staging-root]`
- `compatctl launch-apk --window-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
- `compatctl inspect-apk-window <apk-path> [staging-root]`
- `compatctl launch-apk --self-heal-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
- `compatctl launch-apk-surface <apk-path> [staging-root]`

`--dex-proof` now stages `classes.dex`, `classes2.dex`, and similar entries into the deterministic APK session root, parses safe DEX header plus string/type/proto/method/class metadata, can locate a deterministic entrypoint code item, and emits structured `dex` plus `art_bootstrap` JSON without claiming full Java/Kotlin ART execution yet.

`--storage-proof` now implements **P9 Android App Storage + Sandbox Contract** for the direct APK session path. It materializes deterministic `sandbox/data/data/<package>`-style directories, exposes `files` plus `cache` plus native-lib plus asset/resource roots, validates app-relative paths through a Linuxoid safe resolver, writes a session marker file, rejects escape attempts explicitly, and emits nested `storage` JSON plus `storage_health` and `sandbox_health` fields for the Self-Healing Android Device loop while keeping `isolation_level: path_sandbox_only` honest.

`--permissions-proof` now implements **P10 Android Permissions + AppOps Contract** for the same direct APK session path. It parses plain-XML `uses-permission` entries when available, materializes deterministic `requested_permissions`, `granted_permissions`, and `denied_permissions`, persists sandbox-backed contract artifacts under `sandbox/data/data/<package>/permissions/permission-state.json` plus `sandbox/data/data/<package>/permissions/app-ops.json`, and emits nested `permissions` plus `app_ops` JSON with explicit `schema_version`, `package_name`, `user_id`, `app_id`, `sandbox_root`, `updated_at_unix_ms`, `contract_ready`, `healing_actions`, `diagnostics`, `permission_health`, and `app_ops_health` fields for the Self-Healing Android Device loop. Linuxoid now validates and heals missing, malformed, incomplete, stale, or incompatible permission/AppOps files across repeated launches, but it still does **not** silently grant dangerous permissions: unsupported/binary manifests report an explicit limitation, `RECORD_AUDIO` stays denied without an explicit local grant path, and AppOps remain Linuxoid-owned local contract records rather than full Android framework enforcement.

`inspect-apk-permissions` now gives that same durable contract a focused operator surface without requiring developers to sift through the full direct-launch report. It reuses the session-bound sandbox and permission/AppOps machinery, writes or heals deterministic contract files under `sandbox/data/data/<package>/permissions`, validates incomplete persisted state before trusting it, and returns stable JSON that exposes `storage`, `permissions`, `app_ops`, `sandbox_health`, `permission_health`, `app_ops_health`, and the current recovery guidance for the Self-Healing Android Device path.

`--activity-proof` now binds that same staged APK session to a Linuxoid-owned `package_manager`, `intent_resolution`, and `activity_launch` contract. It writes a deterministic package record with activity labels, exported flags, enabled flags, and manifest-declared intent filters, resolves either a `MAIN` plus `LAUNCHER` target or an explicit `--component` request, ties the resulting activity launch record back to lifecycle, surface, input, Binder, and DEX/bootstrap readiness, and emits explicit `binder_health` plus `activity_health` fields for future Self-Healing Android Device work without claiming full Android framework startup yet.

`--process-proof` now implements **P11 Minimal ActivityManager/ProcessManager Contract** for the same direct APK session path. It binds the staged session to a Linuxoid-owned `activity_manager` plus `process_manager` contract, persists deterministic process identity plus lifecycle state under `sandbox/data/data/<package>/process-manager/activity-manager-state.json` and `sandbox/data/data/<package>/process-manager/process-state.json`, exposes Linuxoid app identity placeholders like `user_id`, `app_id`, `uid_placeholder`, `gid_placeholder`, `process_name`, `pid_value`, `pid_source`, `start_reason`, `restart_policy`, and `termination_policy`, and ties process readiness back to the existing package, intent, activity, lifecycle, storage, permission/AppOps, Binder, surface/input, and DEX/bootstrap contracts without pretending full Android framework process execution already exists.

`inspect-apk-process` now gives that P11 contract a focused operator surface. It reuses the same sandbox-backed session data, heals missing, malformed, stale, or incompatible process-manager artifacts before trusting them, and returns stable JSON that exposes `activity_manager`, `process_manager`, `activity_manager_health`, `process_health`, and the current recovery guidance for the Self-Healing Android Device path.

`--window-proof` now implements **P12 WindowManager + Wayland/EGL Surface Contract** for the same direct APK session path. It binds the staged session to a Linuxoid-owned `window_manager` contract, persists deterministic `window-state.json`, `window-session-map.json`, and `window-events.jsonl` artifacts under `sandbox/data/data/<package>/window-manager`, maps the resolved activity and process identity onto the existing native surface proof, and exposes explicit `created`, `attached`, `visible`, `resized`, `hidden`, `destroyed`, `failed`, and `recovered` states without requiring a live Wayland display in CI. When a live Wayland/EGL environment is available, Linuxoid records that best-effort availability through `backing_mode` and probe metadata while keeping the contract headless-safe and honest.

`inspect-apk-window` now gives that P12 contract a focused operator surface. It reuses the same sandbox-backed session data, heals missing, malformed, stale, incompatible, or incomplete window-manager artifacts before trusting them, and returns stable JSON that exposes `window_manager`, `window_health`, current package/activity/process/surface mappings, and the current recovery guidance for the Self-Healing Android Device path.

`--runtime-proof` now implements **P13 Real ART Runtime Path / Java VM Bootstrap Contract** for the same direct APK session path. It binds the staged session to a Linuxoid-owned `runtime_bridge` contract, discovers runtime roots plus boot classpath inputs plus native library directories, stages a deterministic runtime handle that ties package, activity, process, window, dex, and sandbox identity together, persists `runtime-state.json`, `runtime-session-map.json`, and `runtime-events.jsonl` under `sandbox/data/data/<package>/runtime-manager`, and exposes explicit `unavailable`, `discovered`, `configured`, `bootstrapping`, `ready`, `failed`, `degraded`, and `recovered` state transitions without pretending full Java/Kotlin bytecode execution already exists.

`inspect-apk-runtime` now gives that P13 contract a focused operator surface. It reuses the same sandbox-backed session data, heals missing, malformed, stale, incompatible, or incomplete runtime-manager artifacts before trusting them, and returns stable JSON that exposes `runtime_bridge`, `runtime_health`, current package/activity/process/window/runtime mappings, and the current recovery guidance for the Self-Healing Android Device path.

`--java-proof` now implements **P14 Java/Kotlin APK Proof Contract** on top of the existing package, activity, process, window, and runtime seams. It does not claim real Java/Kotlin bytecode execution yet. Instead, it proves that a Java/Kotlin-style APK can be inspected, resolved through `MAIN`/`LAUNCHER`, mapped onto deterministic process plus window plus runtime session artifacts, and persisted as a Linuxoid-owned `java_apk_proof` contract under `sandbox/data/data/<package>/java-proof/java-proof-state.json`, `java-proof-session-map.json`, and `java-proof-events.jsonl`. `inspect-apk-java` exposes that same proof contract directly, and the diagnostics stay explicit about current limits: this is bootstrap-and-lifecycle wiring proof for the Self-Healing Android Device path, not full ART-owned bytecode execution yet.

### First MainActivity Bytecode Checkpoint

`launch-apk --first-app-start-proof` and `inspect-apk-first-start` now drive one minimal Android app fixture through the real Linuxoid direct-session path as far as the current runtime honestly can:

- APK inspection and plain-XML manifest/package parsing
- launcher `MAIN` / `LAUNCHER` intent resolution
- app data and sandbox setup
- process/session creation
- lifecycle and surface/window proof
- runtime-root discovery and bootstrap configuration
- DEX staging, parsing, and class-loader readiness
- Self-Healing Android Device diagnostics when anything upstream is blocked

What actually runs today:

- Linuxoid resolves the fixture `MainActivity`
- Linuxoid stages the APK, data directory, dex payload, and runtime inputs
- Linuxoid creates process, window, runtime, and Java-proof session artifacts
- Linuxoid parses real DEX header, string, type, proto, method, and class tables
- Linuxoid locates the deterministic fixture lifecycle method `com.example.launchapk.MainActivity.onCreate()I`
- Linuxoid decodes and executes a tiny real DEX instruction path through Linuxoid's minimal interpreter and records it in `first_android_app_start`
- Linuxoid currently supports the smallest opcode subset needed for this checkpoint: `nop`, `const/4`, `return-void`, `return`, `return-wide`, and `return-object`

What does **not** run yet:

- real ART-owned `ActivityThread` / application bootstrap
- managed `MainActivity` method invocation through ART
- broad Java/Kotlin APK execution beyond the deterministic lifecycle checkpoint method
- general Android framework dispatch or compatibility

The checkpoint stays explicit about that boundary. On the healthy fixture path today it reports:

- `first_android_app_start.ready: true`
- `first_android_app_start.lifecycle_method_name: "onCreate"`
- `first_android_app_start.lifecycle_method_signature: "()I"`
- `first_android_app_start.dex_parse_state: "header_tables_methods_and_code_item"`
- `first_android_app_start.bytecode_execution_state: "returned"`
- `first_android_app_start.first_executed_opcode: "const/4"`
- `first_android_app_start.last_executed_opcode: "return"`
- `first_android_app_start.decoded_instruction_count: 2`
- `first_android_app_start.executed_instruction_count: 2`
- `first_android_app_start.java_art_bytecode_executed: true`
- `first_android_app_start.reached_return: true`
- `first_android_app_start.returned_value_type: "I"`
- `first_android_app_start.returned_value: "1"`
- `first_android_app_start.activity_lifecycle_state: "destroyed"`
- `first_android_app_start.blocking_reason: "needs-real-activitythread-context"`

So this is a truthful first MainActivity lifecycle bytecode proof for the Self-Healing Android Device path. Linuxoid now executes one tiny real `MainActivity.onCreate()I`-style DEX method through its own minimal interpreter and reaches a real `return` instruction with a deterministic value, but that is still not a claim that Linuxoid already provides full ART or end-to-end Android framework execution.

Immediate next blocker:

- `bridge_activity_oncreate_into_real_art_runtime_context`

`inspect-apk-compatibility` and `inspect-apk-compatibility-suite` now implement **P15 Third-Party APK Compatibility Sprint** on top of those same direct-session seams. They do not invent a disconnected mock matrix. Instead, Linuxoid launches the real staged APK session through package inspection, intent/activity resolution, storage sandboxing, permissions/AppOps, native/JNI load, process/session creation, window/surface state, runtime bootstrap, Java/Kotlin proof, and Self-Healing Android Device diagnostics, then emits a deterministic compatibility report under `sandbox/data/data/<package>/compatibility/compatibility-report.json`, `compatibility-domains.json`, and `compatibility-events.jsonl`. The suite command materializes the same contract across a small locally generated APK-like fixture set and summarizes statuses such as `supported`, `partial`, `blocked`, `missing-runtime`, `missing-surface`, `missing-native-lib`, `needs-real-art`, `recovered`, and `degraded`.

`--self-heal-proof` now runs the existing Self-Healing Android Device watchdog on top of that same staged APK session, and **P13 Real ART Runtime Path / Java VM Bootstrap Contract** extends the inputs it consumes with `runtime_health` alongside `window_health`, `activity_manager_health`, and `process_health`. The watchdog can now record and replay deterministic `retry_runtime_bootstrap` attempts for blocked or failed runtime-bridge sessions and `rebuild_window_manager_state` attempts for missing, malformed, incomplete, stale, or incompatible sandbox-backed window-manager files alongside `rebuild_process_manager_state`, `rebuild_permission_state`, `repair_app_storage`, `restage_assets`, `restart_surface`, `refresh_binder_services`, `rebuild_dex_bootstrap`, and `rerun_intent_resolution`, without pretending real Android framework recovery already exists.

Current P10 inspection flow:

- `compatctl launch-apk --permissions-proof <apk-path> [staging-root]`
- `compatctl inspect-apk-permissions <apk-path> [staging-root]`
- inspect nested `permissions` and `app_ops` JSON in stdout
- inspect persisted sandbox-backed artifacts under:
  - `sandbox/data/data/<package>/permissions/permission-state.json`
  - `sandbox/data/data/<package>/permissions/app-ops.json`

Current P13 inspection flow:

- `compatctl launch-apk --runtime-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
- `compatctl inspect-apk-runtime <apk-path> [staging-root]`
- inspect nested `runtime_bridge` JSON in stdout
- inspect persisted sandbox-backed artifacts under:
  - `sandbox/data/data/<package>/runtime-manager/runtime-state.json`
  - `sandbox/data/data/<package>/runtime-manager/runtime-session-map.json`
  - `sandbox/data/data/<package>/runtime-manager/runtime-events.jsonl`

Current P14 inspection flow:

- `compatctl launch-apk --java-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
- `compatctl inspect-apk-java <apk-path> [staging-root]`
- inspect nested `java_apk_proof` JSON in stdout
- inspect persisted sandbox-backed artifacts under:
  - `sandbox/data/data/<package>/java-proof/java-proof-state.json`
  - `sandbox/data/data/<package>/java-proof/java-proof-session-map.json`
  - `sandbox/data/data/<package>/java-proof/java-proof-events.jsonl`

Current first app start checkpoint flow:

- `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
- `compatctl inspect-apk-first-start <apk-path> [staging-root]`
- inspect nested `first_android_app_start` JSON in stdout
- inspect persisted checkpoint artifact under:
  - `sandbox/data/data/<package>/first-app-start/first-app-start.json`

Current P15 inspection flow:

- `compatctl inspect-apk-compatibility <apk-path> [staging-root]`
- `compatctl inspect-apk-compatibility-suite <suite-root> <apk-path> [apk-path...]`
- inspect compact compatibility JSON in stdout
- inspect persisted sandbox-backed artifacts under:
  - `sandbox/data/data/<package>/compatibility/compatibility-report.json`
  - `sandbox/data/data/<package>/compatibility/compatibility-domains.json`
  - `sandbox/data/data/<package>/compatibility/compatibility-events.jsonl`
- inspect the suite-level summary under:
  - `<suite-root>/suite-compatibility-report.json`

What remains blocked after the first app start checkpoint:

- real host-side process creation and liveness beyond the current Linuxoid placeholder process identity
- real Java/Kotlin ART bytecode execution
- real Android WindowManagerService / SurfaceFlinger behavior and production compositor compatibility
- full Android `ActivityManagerService` / `ProcessList` behavior
- full Android framework permission manager and AppOps service semantics
- real ART/dex2oat/classloader execution against a discovered runtime root
- broad third-party APK execution beyond the current deterministic compatibility-report fixtures and bootstrap proof wiring

Next phase: **P16 Managed Bytecode Invocation + ActivityThread Contract**

P16 handoff from the first app start checkpoint:

- reuse the existing sandbox-backed `package_manager`, `intent_resolution`, `activity_launch`, `storage`, `permissions`, `app_ops`, `activity_manager`, `process_manager`, `window_manager`, `runtime_bridge`, and `java_apk_proof` session contracts as the stable inputs for managed bytecode invocation and ActivityThread-style bootstrap sequencing
- reuse the new `compatibility` report layer as the admission gate for which third-party APK profiles are currently `supported`, `partial`, `blocked`, `missing-runtime`, `missing-surface`, `missing-native-lib`, `needs-real-art`, `recovered`, or `degraded`
- treat `runtime_health`, `java_proof_health`, `activity_manager_health`, `process_health`, `window_health`, `permission_health`, `app_ops_health`, `storage_health`, `sandbox_health`, `binder_health`, `dex_health`, and `activity_health` as first-class gating signals for Linuxoid runtime start, stop, retry, and eventual managed-thread ownership decisions
- keep the current Self-Healing Android Device recovery loop honest by distinguishing:
  - contract-ready package plus activity plus process plus window plus runtime wiring
  - future real ART-owned class loading, ActivityThread bootstrap, and Java/Kotlin bytecode execution
- preserve the current no-Waydroid, no-emulator, no-ADB constraint on the direct native Linuxoid runtime path

Git/GitHub update path: use normal git remotes from this environment; if direct authentication is unavailable in a later environment, record `GitHub update blocked: direct GitHub authentication not available from this environment`.

Current direct APK activity-proof operator surface:

- `--package <package>` lets operators force package selection against the staged package registry and gets an honest `package_not_found` block when it does not match the staged APK session.
- `--component <component>` lets operators resolve an explicit activity component instead of the launcher path.
- Deterministic blocked reasons now include:
  - `package_not_found`
  - `no_launcher_activity`
  - `ambiguous_launcher_activities`
  - `component_disabled`
  - `component_not_exported`
  - `unsupported_component_type`
  - `unresolved_activity_class`
- The direct Self-Healing Android Device watchdog now writes:
  - `self-healing-android-device/watchdog-report.json`
  - `self-healing-android-device/recovery-journal.jsonl`
  - nested `self_healing_android_device` JSON with:
    - `initial_health`
    - `final_health`
    - `actions_attempted`
    - `actions_succeeded`
    - `actions_failed`
    - `journal_path`
    - `recommended_next_action`

## What Self-Healing Means Now

Today, **self-healing** in Linuxoid means:

- Linuxoid can expose the native direct-run path through stable public command contracts before full Android execution exists:
  - `native-runtime-health-fixture`
  - `native-runtime-recovery-plan`
  - `native-runtime-health-replay`
  - `native-runtime-diagnostic-replay`
  - `native-runtime-diagnostic-fixture`
- Linuxoid can classify native-runtime state into deterministic subsystem records instead of treating failures as opaque crashes.
- Linuxoid can select bounded recovery actions for known failure classes such as missing bundle artifacts, failed native loading, unavailable display surfaces, failed local service lookup, and pending ART/classloader work.
- Linuxoid now also tracks pending post-class-resolution activity bootstrap as its own bounded runtime state instead of folding it into generic classloader readiness.
- Those recovery actions now carry explicit deterministic metadata for harnesses:
  - stable `action_rank`
  - bounded `retry_budget`
  - stable `recovery_scope`
- Linuxoid now also exposes direct summary fields so callers do not need to re-derive the runtime state from raw records:
  - `dependency_blocked`
  - `failing_subsystem_count`
  - `recovery_actions_selected`
  - `failing_subsystems`
- Linuxoid now also exposes the six required runtime-health areas as an explicit core projection instead of leaving callers to recover them from the larger record list:
  - `core_subsystems`
  - `core_subsystem_count`
  - `core_ready_subsystem_count`
  - `core_subsystems_ready`
  - `core_subsystem_records`
- Linuxoid now also exposes the four required deterministic recovery cases as an explicit scenario contract instead of leaving callers to reconstruct them from whichever actions happened to be selected in one run:
  - `canonical_recovery_scenario_count`
  - `canonical_recovery_scenarios`
- Linuxoid now also exposes the replayable trace bundle as an explicit contract instead of leaving callers to infer completeness from the raw source list:
  - `health_trace_jsonl_path`
  - `health_replay_json_path`
  - `canonical_trace_source_count`
  - `canonical_trace_source_names`
  - `missing_trace_source_count`
  - `trace_bundle_complete`
- Linuxoid now also records the host ART probe gate as a first-class diagnosable seam:
  - `art/art-runtime-probe-inventory.json`
  - `ART Runtime Probe Inventory Path`
  - `ART Runtime Probe Detection Reason`
  - `ART Runtime Probe Capability`
  - deterministic candidate inventory and selected-probe rationale for override, host, and missing-runtime paths
- Linuxoid now also exposes a direct APK-session DEX/bootstrap probe for future Self-Healing Android Device work:
  - `compatctl launch-apk --dex-proof <apk-path> [staging-root]`
  - staged `dex/classes*.dex` artifacts
  - `art/dex-proof.json`
  - `art/art-bootstrap.json`
  - explicit `dex_health` and `art_health` fields
  - honest `java_execution_supported: false` while the class-loader/bootstrap contract is still metadata-only
- Linuxoid now also exposes a direct APK-session PackageManager/intent/activity launch contract for future Self-Healing Android Device work:
  - `compatctl launch-apk --activity-proof <apk-path> [staging-root]`
  - `activity-launch/package-record.json`
  - `activity-launch/intent-resolution.json`
  - `activity-launch/activity-launch.json`
  - explicit `binder_health` and `activity_health` fields
  - nested `package_manager`, `intent_resolution`, and `activity_launch` JSON without claiming full Android framework or `system_server` behavior
  - activity registry details including labels, exported flags, enabled flags, and manifest-declared intent filters
  - deterministic explicit-component and blocked-reason reporting suitable for the Self-Healing Android Device loop
- Linuxoid now also exposes the **P8 Self-Healing Android Device Watchdog + Recovery Loop** on the direct APK-session path:
  - `compatctl launch-apk --self-heal-proof <apk-path> [staging-root]`
  - `self-healing-android-device/watchdog-report.json`
  - `self-healing-android-device/recovery-journal.jsonl`
  - nested `self_healing_android_device` JSON
  - deterministic local recovery actions:
    - `no_action`
    - `restage_assets`
    - `rebuild_resource_metadata`
    - `restart_surface`
    - `reset_input_queue`
    - `restart_lifecycle`
    - `refresh_binder_services`
    - `rebuild_dex_bootstrap`
    - `rerun_intent_resolution`
    - `safe_mode_launch`
  - honest local-contract-only recovery reporting for the future Self-Healing Android Device loop, not full Android framework healing
- Linuxoid now also distinguishes **host ART detection** from **host ART bootstrap capability**:
  - override-backed probes are `override_bootstrap_capable`
  - host `dalvikvm` and `dalvikvm64` probes are `host_dalvikvm_bootstrap_capable`
  - host `app_process` and `app_process64` probes are recorded honestly as `host_app_process_detection_only`
- Linuxoid now also surfaces that replay contract directly through the public native bridge, so `preflight-runtime native`, `verify-package native`, `verify-package-matrix native`, and `launch-package native` name the health trace JSONL, recovery-actions JSONL, merged diagnostic events JSONL, trace index, and replay completeness counts without forcing callers to open the deeper health JSON first.
- Those native bridge reports now also include per-source replay details, so operators can see which JSONL traces were merged, where each one lives, how many events it contributed, and which deterministic fingerprint Linuxoid computed for it.
- Those same native bridge reports now also hand back the exact replay commands to run later, so operators can invoke `native-runtime-health-replay`, `native-runtime-diagnostic-replay`, and `native-runtime-diagnostic-fixture` directly from the blocked preflight or launch report instead of reconstructing the command line by hand.
- Those same native verification and matrix reports now also surface the full replay bundle itself, not just a couple of handles:
  - `Runtime Recovery Actions Trace Path`
  - `Runtime Health Replay Path`
  - `Runtime Diagnostic Replay Path`
  - `Runtime Diagnostic Fixture Command`
  - `Runtime Diagnostic Trace Index Path`
  - canonical/found/missing trace-source counts
  - `Runtime Trace Source Details`
- Those same native bridge reports now also carry the full canonical four-scenario recovery contract, not just the currently selected blocked-path actions, so operators can see the deterministic mapping for missing artifact, failed native load, unavailable display, and failed service lookup directly in verification and matrix output too.
- Linuxoid now also pins that public native-bridge contract with explicit blocked-host-ART regressions, so the operator-facing reports stay stable on:
  - health classification
  - recovery decision selection
  - repeated runtime-health JSON stability
  - no false success when the required ART/bootstrap dependency is still missing
- Linuxoid now also pins that same blocked-host-ART contract through the public verification surfaces:
  - repeated `verify-package native` runs keep the rendered report stable
  - repeated `verify-package native` runs keep both `runtime-health.json` and merged replay JSON stable
  - repeated `verify-package-matrix native` runs keep the rendered row-level diagnosis stable
  - neither verification surface drifts into a fake pass while ART/bootstrap is still missing
- Those reports now also say the health verdict directly at the bridge layer:
  - `Runtime Health Classification: recovery_needed`
  - `Runtime Overall Ready: no`
- Those reports now also expose the six required core health records directly at the bridge layer:
  - `Runtime Core Subsystems Ready`
  - `Runtime Core Subsystem Count`
  - `Runtime Core Ready Subsystem Count`
  - `Runtime Core Subsystems`
  - `Runtime Core Subsystem Details`
- `verify-package native` now carries those same bridge-level self-healing verdicts too, instead of hiding them only inside the nested `Preflight Output:` block.
- `verify-package-matrix native` now carries that same bridge-level self-healing truth per staged package entry, so matrix runs expose health classification, dependency-blocked state, recovery details, and replay handles without forcing operators to crack open nested reports one package at a time.
- Linuxoid can persist those decisions into stable artifacts for agents, harnesses, and replay tooling:
  - `runtime-health.json`
  - `runtime-health-trace.jsonl`
  - `runtime-recovery-plan.json`
  - `runtime-recovery-actions.jsonl`
  - `runtime-diagnostic-trace-index.json`
  - `runtime-diagnostic-events.jsonl`
  - `runtime-diagnostic-replay.json`
- Linuxoid can now also carry class-resolution evidence forward into a deterministic post-resolution activity-bootstrap seam:
  - `native-art-activity-bootstrap-fixture`
  - `art/activity-bootstrap-plan.json`
  - `art/activity-bootstrap-trace.jsonl`
  - `art/activity-bootstrap-result.json`
- Linuxoid now upgrades that seam from activity-only planning into a host-side application-plus-launcher bootstrap attempt contract:
  - normalized manifest application class when present
  - explicit application bootstrap probe event
  - explicit launcher-activity bootstrap probe event
  - separate attempt and success state for each probe
- Linuxoid now also treats activity bootstrap planning as a first-class self-healing subsystem:
  - `activity_bootstrap_readiness`
  - ready when the launcher/bootstrap plan is fully materialized
  - merged replay coverage through `art/activity-bootstrap-trace.jsonl`
- Linuxoid now also upgrades that activity seam into a deterministic bootstrap-execution contract:
  - `native-art-bootstrap-execution-fixture`
  - `art/bootstrap-execution-plan.json`
  - `art/bootstrap-execution-trace.jsonl`
  - `art/bootstrap-execution-result.json`
  - `art/bootstrap-execution-context.json`
  - `art/bootstrap-execution-runner.sh`
  - `art/bootstrap-execution-runner-state.json`
  - `art/bootstrap-execution-application.log`
  - `art/bootstrap-execution-activity.log`
- Linuxoid now also treats bootstrap execution as its own bounded self-healing subsystem:
  - `bootstrap_execution_readiness`
  - `attempt_host_bootstrap_execution`
- Linuxoid now executes that seam through the generated runner script when a safe host runtime is available, and it preserves raw runner plus per-phase exit codes for replay and diagnosis.
- Linuxoid now also supports a Linuxoid-owned ART probe override for deterministic fixture runs, so the runtime-smoke and bootstrap-execution seams can be exercised end to end even on hosts that do not ship ART locally.
- Linuxoid now also carries that ART probe inventory and detection rationale through runtime smoke, native preflight, native verification, and native launch reports, so the default-path host ART gate is diagnosable without reverse-engineering the deeper artifacts by hand.
- Linuxoid now also carries explicit probe-capability classification through those same reports, so a detected host `app_process` surface no longer looks launch-ready when it is only sufficient for diagnosis.
- Linuxoid runtime health now recognizes that deeper success path too: when the ART-style class-resolution seam and supervised bootstrap-execution seam both succeed, `dex_classloader_readiness` and `bootstrap_execution_readiness` now converge to `ready` instead of staying stuck in a generic pending state.
- Linuxoid now also distinguishes **DEX-only** bundles from broken native loading: when an APK declares no native libraries at all, `native_loading` resolves to `not_required` instead of falsely reporting a blocked native-loader failure.
- Linuxoid now also exposes a real local `native` runtime bridge for staged package discovery, staged metadata lookup, staged-package preflight, and bootstrap-execution handoff, so self-healing can reason about a Linuxoid-owned local target instead of only attached Android runtimes.
- Linuxoid now also threads that same staged-package preflight into the backend-neutral verification surface: `verify-package` and `verify-package-matrix` now report runtime-target selection plus package visibility plus component readiness before launch, and the local `native` backend uses that same contract instead of a launch-only shortcut.
- Linuxoid now also makes that `native` preflight contract honest about real launch-attempt readiness: staged package visibility and launcher resolution can still succeed, but `preflight-runtime native`, `verify-package native`, and `verify-package-matrix native` now stay blocked when host ART is missing, when the runtime probe is fixture-only without explicit opt-in, or when the staged bundle is not actually a native spike candidate.
- Linuxoid now also surfaces deterministic recovery policy directly in the public native bridge: the current selected recovery actions and the canonical four scenario mappings for missing artifact, failed native load, unavailable display, and failed service lookup are rendered in native preflight and launch reports without forcing callers to open the deeper health JSON first.
- Those native bridge reports now also carry the bounded recovery metadata itself, not just the action names:
  - `action_rank`
  - `retry_budget`
  - `recovery_scope`
  - `action_reason`
- Linuxoid now also separates **fixture override success** from **real host launch success** on the native launch path: `launch-package native` records override-backed ART/bootstrap success in artifacts, but it only classifies that as a successful app launch when `LINUXOID_NATIVE_ALLOW_RUNTIME_OVERRIDE=1` is set explicitly.
- Linuxoid now also surfaces the deeper native-launch diagnosis directly in the `launch-package native` report: bootstrap manifest and execution artifact paths, runtime-health and recovery-plan paths, replay-bundle paths, and the blocked subsystem summary are all emitted up front so a failed staged launch can be diagnosed without rerunning the whole flow blindly.
- Linuxoid can replay and merge those traces later without rerunning the full UI path.
- Linuxoid can now fingerprint each trace source and record first/last event types so failures can be compared offline across runs.
- Linuxoid now reuses one opened APK archive plus the already-staged bundle manifest while building those runtime records, so live health and replay commands stay fast enough to rerun on real staged bundles without falling back to repeated full-archive scans and repeated `apktool` decode work.
- Linuxoid now merges the activity-bootstrap trace into that replay bundle, so launcher targeting and Binder-readiness failures stay diagnosable offline too.
- Linuxoid refuses false success when a critical dependency is missing. A missing native library payload or missing ART runtime still leaves the runtime in `recovery_needed`, not `ready`.
- Linuxoid also stays backward-compatible with older staged bootstrap manifests that predate the native-library summary fields, so replay and health commands keep working across already-materialized bundles.
- Linuxoid now has regression coverage that checks this both structurally and behaviorally:
  - health classification stays stable
  - recovery decision selection stays deterministic
  - repeated command JSON stays stable
  - missing dependencies do not flip the runtime into false success

A **successful self-healing pass today** means something narrower than “the app launched”:

- Linuxoid identified the blocked subsystem truthfully.
- Linuxoid selected the bounded next recovery action deterministically.
- Linuxoid wrote stable JSON and JSONL artifacts that a harness or operator can replay later.
- Linuxoid did **not** claim Android app execution success if the runtime was still blocked.
- That successful pass can still end with `Ready For Launch: no` or `Launch OK: no` on the public native bridge, as long as the blocked state is truthful, replayable, and paired with the right bounded recovery action.
- That same truthful pass can now also end with `Preflight OK: no`, `Direct Launch OK: no`, or `Packages Passed: 0/1` on the public verification and matrix surfaces when the correct outcome is still “blocked but diagnosable.”

Put plainly:

- **What self-healing means now:** Linuxoid can diagnose, classify, explain, and replay a blocked native runtime state without pretending the app already ran.
- **What self-healing does not mean yet:** Linuxoid cannot automatically push a staged Android app across the real ART/bootstrap boundary and declare success on the default host path.

The practical reading of that contract today is:

- Linuxoid can tell a harness which subsystem is blocked.
- Linuxoid can tell a harness which bounded recovery step belongs to that failure class.
- Linuxoid can preserve enough JSON and JSONL state to replay that diagnosis later without rerunning the full UI path.
- Linuxoid can keep that same answer stable at the bridge layer, so repeated `preflight-runtime native`, `verify-package native`, and `launch-package native` runs do not drift into a different classification or a fake success.
- Linuxoid now keeps that same answer stable at the matrix layer too, so repeated `verify-package-matrix native` runs do not drift into a different row-level diagnosis or a fake pass count.
- Linuxoid cannot yet convert a blocked Java or Kotlin APK into a real successful host-side Android launch on its own.

This is **observability and bounded recovery planning**, not autonomous app repair or full Android execution.

## What Still Blocks Full Android App Execution

Linuxoid is still **not** at “run Android apps directly on Linux end to end” yet. The main remaining blockers are:

1. **Real ART / DEX execution**
   - host-side `PathClassLoader` or equivalent class resolution still needs to move from planning/smoke to actual execution
   - application code is not yet running through a real ART-owned path by default; the current override-backed fixture path is a Linuxoid test seam, not proof of embedded ART execution on this host
   - the current supervised bootstrap-execution seam is a truthful launch-attempt contract, not yet a successful real host ART startup for a staged foreground app

2. **Android framework and services**
   - the Binder-shaped local manager is still a Linuxoid-owned seam, not full Android Binder semantics
   - Linuxoid now has a direct APK-session PackageManager plus MAIN/LAUNCHER intent plus activity-launch contract, but `ActivityManager`, `PackageManager`, and related behavior are still local partial stubs rather than a full Android framework stack

3. **Graphics binding**
   - Wayland and EGL proofs exist separately
   - the real bound path from Android-style rendering into a live compositor-backed surface is still pending

4. **Input and text**
   - focused pointer/key injection is verified
   - full IME, text composition, clipboard fidelity, and richer input routing are still pending

5. **Android resources**
   - manifest and asset readiness are in place
   - full `resources.arsc`, binary XML, themed resource lookup, and framework-style resource semantics are still pending

6. **Real app bootstrap**
   - Linuxoid can now stage, classify, preflight, diagnose, replay, and materialize a deterministic application-plus-activity bootstrap planning seam plus a separate execution seam
   - Linuxoid can now also stage DEX payloads, parse safe DEX header/table/code-item metadata, and prepare a session-bound class-loader bootstrap contract from `launch-apk --dex-proof`
   - that DEX/bootstrap probe still stops honestly at `java_execution_supported: false`
   - it still needs the first successful host-side Android class execution and application or activity bootstrap on the native path for a real staged candidate app, not only override-backed fixture success
   - in practice, that means the public default path still needs to cross from honest blocked reports in `preflight-runtime native`, `verify-package native`, and `launch-package native` into a genuine no-override host-ART-owned success path

Put more simply: Linuxoid can now **diagnose, classify, stage, and replay** the native Android path well. It still cannot honestly claim **full Android app execution on Linux** until a real staged app crosses the current ART/bootstrap seams without relying on the Linuxoid-owned override path.

That is the current line in the sand: Linuxoid is already good at telling us **why** startup is blocked and **what bounded next step** belongs to that failure, but it still needs the first **default-path, no-override, host-ART-owned** staged app startup before we can say Android apps are genuinely running natively on Linux.

The shortest honest summary is:

- Linuxoid is already strong at **diagnosis, bounded recovery choice, and replay**.
- Linuxoid is **not yet** at the first **default-path, no-override, host-ART-owned** staged app startup.

## Target Architecture

```mermaid
flowchart TB
  User["Linux User"] --> Linuxoid["Linuxoid Native Runtime"]
  Agent["Agent / MCP Client / Harness"] --> Control["Control / MCP / Harness Adapter Layer"]

  subgraph Host["Linux Host"]
    Control --> Linuxoid
    Linuxoid --> Loader["APK / Resources / Manifest Loader"]
    Linuxoid --> DexArt["DEX / ART Execution Layer"]
    Linuxoid --> Binder["Binder-Compatible IPC Layer"]
    Linuxoid --> Services["Android Service and Lifecycle Layer"]
    Linuxoid --> Graphics["Linux Window / Graphics Integration"]
    Linuxoid --> Input["Keyboard / Mouse / IME / Clipboard Integration"]
    Linuxoid --> Storage["App Sandbox / Filesystem Mapping"]
    Linuxoid --> Reports["Structured Reports / Bootstrap Specs / Test Hooks"]
  end

  Loader --> App["Android App Process"]
  DexArt --> App
  Binder --> Services
  Services --> App
  Graphics --> App
  Input --> App
  Storage --> App
```

This is the long-term goal state: run Android apps on Linux without depending on Waydroid, an emulator, or another external Android runtime.

## Self-Healing Browser Target Slice

```mermaid
flowchart TB
  User["User Goal"] --> Planner["Intent Planner"]
  Planner --> Policy["Recovery Policy"]
  Planner --> BrowserSession["Linuxoid BrowserSession"]
  Agent["MCP Client / Harness / Agent"] --> Control["Machine-readable Browser Control Layer"]
  Control --> Planner
  Control --> Trace["Trace / Replay / Eval Artifacts"]

  subgraph AndroidBrowser["Linuxoid Android Browser Slice"]
    BrowserSession --> WebView["android.webkit.WebView Surface"]
    BrowserSession --> DomBridge["DOM + JS Bridge"]
    BrowserSession --> PermissionBroker["Permission Broker"]
    BrowserSession --> DownloadBroker["Download Broker"]
    BrowserSession --> Recovery["Recovery Supervisor"]
    BrowserSession --> Profile["Cookies / Storage / Profile State"]
    BrowserSession --> Lifecycle["Lifecycle / Session State"]
  end

  WebView --> Page["Web Content"]
  DomBridge --> Observe["Observe / Act / Re-anchor"]
  Recovery --> Policy
  Recovery --> Lifecycle
  PermissionBroker --> AndroidUI["Android Prompts / System UI"]
  Policy --> Stop["Fail-closed Stop Conditions"]
  Observe --> Trace
  Recovery --> Trace
```

This is the researched browser target slice, not a shipped Linuxoid feature yet. It is explicitly frozen until `P5` while Linuxoid focuses on native execution, Wayland graphics, DEX, and Binder. The recommended foundation remains a **Linuxoid-owned browser shell on the Android WebView API surface**, with the self-healing logic outside the page and the control surface kept MCP- and harness-friendly from day one.

## What Exists Today

- A C++ checkpoint engine with weighted runtime gates
- A phase-progress reporter with `0-100` loading output
- A package-layout planner for APK, app data, and OBB storage
- A decoded-manifest assessor for runtime and service requirements
- A real `load-apk` path that stages an APK into a compat root
- A `launch-apk <apk-path> [staging-root]` path that opens a native-only APK as a ZIP, reads plain-XML manifest and package metadata, stages ABI-matching native libraries plus assets into a deterministic app-session directory, loads the selected library, calls `JNI_OnLoad` when present, creates a minimal native activity session object, and emits structured JSON for both success and failure
- A `launch-apk --surface-proof <apk-path> [staging-root]` mode and `launch-apk-surface <apk-path> [staging-root]` alias that bind that APK launch session to a deterministic native surface/session object, drive a first-frame plus first-pixel proof, emit nested surface JSON with explicit `backend: headless` fallback when Linuxoid is not using a real Wayland/EGL path, and expose future Self-Healing Android Device-facing fields like `surface_health`, `launch_health`, `recoverable`, and `recommended_recovery_action`
- A `launch-apk --asset-proof <apk-path> [staging-root]` mode that binds the staged APK session to a Linuxoid AssetManager- and Resources-shaped bridge, lists normalized staged assets in deterministic order, opens and checksums one asset proof path, detects `resources.arsc` plus `res/` entry metadata, and emits nested `asset_bridge` plus `resource_bridge` JSON without pretending full Android resource decoding already exists
- A `launch-apk --lifecycle-proof <apk-path> [staging-root]` mode that binds the staged APK session to a deterministic Android-like lifecycle controller, Looper-style event pump, and InputQueue foundation, drives ordered `created -> started -> resumed -> paused -> stopped -> destroyed` state changes, dispatches synthetic touch/key events, and emits nested `lifecycle`, `looper`, and `input_queue` JSON
- A `launch-apk --storage-proof <apk-path> [staging-root]` mode that implements **P9 Android App Storage + Sandbox Contract**, binds the staged APK session to deterministic `sandbox/data/data/<package>`-style app directories, resolves app-relative paths through a safe Linuxoid path sandbox, writes and verifies a session marker file, emits nested `storage` JSON with `isolation_level: path_sandbox_only`, and feeds `storage_health` plus `sandbox_health` into the Self-Healing Android Device loop
- A `launch-apk --activity-proof [--package <package>] [--component <component>] <apk-path> [staging-root]` mode that binds the staged APK session to a Linuxoid-owned PackageManager record, captures labels plus exported flags plus enabled flags plus manifest intent filters, resolves either a `MAIN` plus `LAUNCHER` component or an explicit component request, emits deterministic blocked reasons and recovery actions, ties the resulting `activity_launch` record to lifecycle plus surface plus input plus Binder plus DEX/bootstrap readiness, and writes stable `activity-launch/package-record.json`, `intent-resolution.json`, and `activity-launch.json` artifacts without pretending full Android framework startup already exists
- A `launch-apk --self-heal-proof [--package <package>] [--component <component>] <apk-path> [staging-root]` mode that now consumes the P9 storage/sandbox contract too, records deterministic `repair_app_storage` recovery attempts alongside the earlier watchdog actions, writes `self-healing-android-device/recovery-journal.jsonl` plus `self-healing-android-device/watchdog-report.json`, and emits nested `self_healing_android_device` JSON without claiming real Android framework recovery or Java/Kotlin execution already exists
- A `discover-runtime` path that enumerates current runtime targets for `attached-adb`, `waydroid`, and `native`
- A `preflight-runtime` path that checks backend availability, target selection, package visibility, and launch readiness before a generic installed-package launch, including attached-target launcher auto-resolution when the component is omitted and Linuxoid-owned staged-package lookup for the `native` backend
- An `inspect-package` path that reads attached-target package visibility, resolved launcher component, install path, version code, and version name from a live runtime target, and now reads the same staged metadata from a Linuxoid compat root for the `native` backend
- A generic `launch-activity` bridge for explicit Android component launches from Linux
- A generic `launch-package` bridge for installed-package launches across runtime backends, with `waydroid`, `attached-adb`, and `native` routing, and attached-target launcher fallback when callers omit a component
- The `native` branch of that `launch-package` bridge now reuses staged package metadata plus the bootstrap-execution seam, and it reports non-candidate packages honestly instead of falling back to a hard `not implemented` stub
- An `adb-ime-status` runtime bridge for installed IME verification on a live Android target
- A `provision-ime` runtime bridge that installs, enables, sets default, and re-verifies an IME on a live Android target
- A `desktopify-apk` path that generates a Linux wrapper script and `.desktop` entry for a caller-selected Android component, and for IME apps it generates a provisioning launcher with explicit side effects
- A `desktopify-apk-auto` path that infers the launcher activity from the APK manifest and splits desktop-entry and launcher-script roots for cleaner host integration
- A `verify-apk-host-launch-auto` path that stages a local APK, generates Linux launcher artifacts for it, and verifies the generated launcher path against a live Android runtime
- A backend-neutral installed-package launcher-artifact seam that now emits `launch-package` wrappers instead of hard-coding Waydroid in the generated host script
- A `plan-native-spike` path that stages a local APK into a compat root, assesses whether it fits the first native app slice, writes a Linuxoid-owned bundle layout, stages host-ABI native libraries into the deterministic `lib` root, copies extracted `assets/` and `res/` content into stable resource roots, and emits a bootstrap spec for future no-runtime execution
- A `bootstrap-native-spike` path that turns a native candidate into a Linuxoid-owned bootstrap manifest, environment script, entrypoint stub, and bootstrap report
- A `native-lifecycle-shim` path that consumes the bootstrap manifest, creates deterministic lifecycle session artifacts, and now keeps pre-launch activity/process state truthful instead of claiming `RESUMED` before a process exists
- A `native-execute-stub <bootstrap-manifest>` path that now owns the public Linuxoid native bootstrap surface, forks and `execve`s a controlled child runner, sets deterministic cwd plus Linuxoid-only environment variables, closes inherited file descriptors, and emits structured JSON for harnesses and replay tooling
- A `native-process-bootstrap` compatibility alias that still resolves the same parent bootstrap flow for older local automation while the public entrypoint stays on `native-execute-stub`
- A `native-execute-stub <package> ...` runner path that now scans a bundle library root, loads `.so` files in deterministic order, calls `JNI_OnLoad` when present, resolves `ANativeActivity_onCreate`, installs crash logging, and keeps the process alive to the first five-second gate
- A `native-first-pixel-fixture <session-root> [width] [height] [format]` path that creates a headless `ANativeWindow`-shaped surface with explicit metadata and writes a deterministic first-pixel marker instead of pretending a real compositor window already exists
- A `native-window-callback-fixture <session-root> [width] [height] [format]` path that dispatches Linuxoid-owned `window_created`, `window_changed`, and `window_destroyed` activity callbacks and writes a deterministic callback journal artifact for the future real graphics path
- A `native-wayland-surface-fixture <session-root> [width] [height]` path that uses the system `wayland-client` library when available to connect to a real `wl_display`, bind `wl_compositor`, create a `wl_surface`, and write a stable `surface-metadata.json` artifact, while still falling back honestly when Wayland is unavailable
- A `native-egl-smoke-fixture <session-root> [width] [height]` path that uses the system `libEGL` when available to initialize a real EGL display, choose a config, create an OpenGL ES context plus pbuffer surface, and write a stable `egl-metadata.json` artifact, while still falling back honestly when EGL is unavailable
- A `native-window-bridge-fixture <session-root> [width] [height] [format]` path that exposes a minimal `ANativeWindow` bridge contract with width, height, format, stride, deterministic buffer-geometry updates, stable metadata/event artifacts, and honest probe-only versus headless-fallback reporting
- A `native-input-queue-fixture <session-root> [width] [height] [format]` path that injects a deterministic focus-acquire plus pointer/key event sequence against the bridge contract, writes stable metadata plus JSONL event artifacts, and reports honest probe-only versus headless-fallback backing without claiming full IME or text composition support
- A `native-service-manager-fixture <bootstrap-manifest>` path that emits a Linuxoid-owned Binder-shaped service-registry foundation with deterministic registration, lookup, and transaction artifacts for `package_manager`, `activity_manager`, and an app-local placeholder service, plus stable owner-session, owner-process, handle-id, limitation, and missing-service lookup reporting for the future Self-Healing Android Device path
- A `native-art-classloader-fixture <bootstrap-manifest>` path that inventories staged APK dex entries, normalizes manifest target classes into deterministic descriptors, writes classpath-plan artifacts, and reports missing host ART honestly without pretending real class execution exists yet
- A `native-art-class-resolution-fixture <bootstrap-manifest>` path that resolves manifest-target descriptors from real staged DEX contents, writes deterministic resolution-map artifacts, and records honest unresolved targets without pretending ART has executed anything yet
- A `native-art-runtime-smoke <bootstrap-manifest>` path that reuses the classloader plan plus offline class-resolution result, selects a deterministic manifest-derived class target, writes deterministic invocation-plan plus runtime-log plus trace artifacts, and attempts a real host-side `dalvikvm` or `dalvikvm64` class-resolution probe only when a safe local ART surface is available
- A `native-runtime-health-fixture <bootstrap-manifest> [scenario]` path that records staged-runtime health, selects deterministic recovery actions, and writes replayable JSON plus JSONL artifacts for the Self-Healing Android Device skeleton
- A `native-runtime-recovery-plan <bootstrap-manifest> [scenario]` path that materializes the bounded recovery actions into a stable recovery-plan JSON plus per-action JSONL artifact stream for harness replay and diagnosis
- A `native-runtime-health-replay <trace-jsonl-path>` path that replays the health trace into a stable summary without rerunning the full UI path
- A `native-runtime-diagnostic-replay <bootstrap-manifest>` path that merges health, recovery, ART/classloader, class-resolution, and runtime-smoke JSONL traces into one replayable diagnostic bundle without rerunning the UI path
- A `native-runtime-diagnostic-fixture <bootstrap-manifest> [scenario]` path that materializes the runtime-health traces first, then emits a replayable diagnostic bundle plus trace index in one deterministic non-UI step
- Minimal `P1` runtime surfaces for a future direct runner:
  - JNI stub
  - asset-manager stub
  - looper stub
  - signal handler
- Minimal `P2.1` host graphics seams for a future direct runner:
  - headless `ANativeWindow`-shaped surface metadata
  - lifecycle checks for host surface readiness
  - deterministic first-pixel marker artifact
- Minimal `P2.2` native-activity seams for a future direct runner:
  - Linuxoid-owned `ANativeActivityCallbacks` contract
  - created/changed/destroyed window dispatch helpers
  - deterministic callback journal artifact
- Minimal `P2.4` focused input seams for a future direct runner:
  - deterministic focus ownership metadata
  - pointer down/move/up injection journal
  - keyboard down/up injection journal
  - explicit note that full IME and text composition are still pending
- Minimal `P5` Binder/service-registry seams for a future direct runner:
  - local service registration metadata
  - deterministic package/activity-manager/app-local lookups
  - deterministic missing-service lookup reporting
  - transaction JSONL artifacts for bootstrap-time service calls
  - socketpair-backed local transport messages for lookup/transaction round trips
  - explicit note that full Parcel semantics, kernel Binder, and real Android `system_server` behavior are still pending
- Minimal pre-ART APK resource seams for a future direct runner:
  - plain APK/ZIP manifest inspection with package and SDK metadata
  - normalized asset listing and read paths with traversal rejection
  - structured readiness JSON for manifest, assets, and staged resource roots
- A native-only direct APK launch seam for early end-to-end Linux execution proofs:
  - deterministic per-app session staging under `launch-apk`
  - path-traversal rejection for unsafe ZIP entries
  - host-ABI native library selection and structured unsupported-ABI failure
  - `JNI_OnLoad` plus minimal native activity/session execution reporting
  - optional APK-launched native surface proof with deterministic lifecycle states plus first-pixel marker reporting
  - explicit limitation that full Java/Kotlin ART execution and binary Android manifest decoding are still pending
- Minimal ART/classloader preparation seams for a future direct runner:
  - deterministic dex inventory from staged APK archives
  - normalized application and activity target classes plus descriptors
  - classpath-plan artifacts that the self-healing runtime can point at before real ART exists
- Minimal offline DEX class-resolution seams for a future direct runner:
  - deterministic manifest-target descriptor lookup against real staged DEX contents
  - resolution-map plus trace artifacts for replay and harness diffing
  - explicit separation between offline descriptor resolution and real ART execution
- Minimal host-ART smoke seams for a future direct runner:
  - deterministic invocation-plan and runtime-log artifacts
  - reuse of offline DEX resolution evidence before probing host ART
  - safe ART probe attempts only when a local `dalvikvm` surface exists
  - explicit separation between runtime probing and real class resolution
- Minimal self-healing runtime seams for a future direct runner:
  - structured subsystem health records for staging, native load, surface, input, Binder, and DEX/classloader readiness
  - deterministic recovery actions for missing artifact, failed native load, unavailable display, and failed service lookup
  - deterministic recovery-plan and action-trace artifacts for MCP and harness replay
  - explicit action rank, retry-budget, and recovery-scope metadata so replay tooling can make the same bounded choice every time
  - JSONL trace plus replayable fixture artifacts for diagnosis without rerunning the full UI path
  - a deterministic trace index with per-source fingerprints and event boundaries for offline diffing
- A `launch-waydroid-package` compatibility alias that still launches an already installed app through the Waydroid adapter without requiring an APK reinstall or a hardcoded ADB serial
- A `desktopify-waydroid-package` path that generates a Linux launcher and `.desktop` entry for an installed Waydroid app
- A generic `verify-package` path that proves direct Linux launch for an installed package by checking runtime launch, generated host-launch artifacts, and generated launcher execution across backend contracts
- A generic `verify-package-matrix` path that runs the installed-package verification loop across several apps and reports pass/fail per package without Waydroid-shaped command names
- A `verify-waydroid-package` path that proves direct Linux launch for an installed Waydroid app by checking the runtime launch, the generated host launcher artifacts, and the generated launcher execution
- A `verify-waydroid-matrix` path that runs the direct Linux verification loop across several installed Waydroid apps and reports pass/fail per package
- A live Waydroid-backed proof that a Linuxoid-generated launcher can install the keyboard APK, enable it, set it as default, and return `Ready for typing: yes` from Linux
- A live Waydroid-backed proof that the keyboard APK now verifies end to end from its local file path through the Linuxoid-generated launcher flow
- A live Waydroid-backed proof that the local F-Droid APK now verifies end to end from its file path through the Linuxoid-generated launcher flow
- A live Waydroid-backed proof that a Linuxoid-generated launcher can open `com.android.calculator2` from Linux through the new installed-package path
- A live Waydroid-backed proof that the new generic `verify-package` command passes for `com.android.calculator2`
- A live Waydroid-backed proof that the new generic `verify-package-matrix` command passes `3/3` for `com.android.calculator2`, `com.android.settings`, and `org.fdroid.fdroid`
- A live Waydroid-backed mini-matrix that verifies direct Linux launch for `com.android.calculator2`, `com.android.settings`, and `org.fdroid.fdroid`
- A live attached-ADB proof that `inspect-package` resolves `com.android.settings/.Settings` and reads its install path plus version metadata from the target itself
- A live attached-ADB proof that `launch-package attached-adb com.android.settings 192.168.240.112:5555` succeeds without an explicit component
- A live attached-ADB proof that `verify-package` now passes for `com.android.settings` without an explicit component
- A live attached-ADB proof that `verify-package-matrix` now passes `3/3` for `com.android.settings`, `com.android.calculator2`, and `org.fdroid.fdroid` without explicit components
- A live local-APK proof that `plan-native-spike` now accepts Calculator as a native candidate, writes a native bundle plan, and emits a bootstrap spec with no blockers
- A live local-Linux proof that `native-lifecycle-shim` creates Calculator session artifacts, keeps the pre-launch state at `NOT_CREATED`, and exposes the first Linuxoid-owned service bindings
- A live local-Linux proof that `native-service-manager-fixture` now writes deterministic Binder-shaped registration, lookup, transaction, and limitation artifacts for `package_manager`, `activity_manager`, an app-local placeholder service, and an honest missing-service lookup under the lifecycle session root
- A live local-Linux proof that the current dex-only Calculator bundle now returns a structured soft failure with `exit_reason = no_native_libraries_found` instead of pretending the missing execution core is generic
- A live local-Linux proof that `native-execute-stub` now loads a fixture shared library, calls `JNI_OnLoad`, resolves `ANativeActivity_onCreate`, reaches the five-second watchdog gate, and exits `0`
- A local test-backed proof that `native-execute-stub <bootstrap-manifest>` forks the fixture runner, writes `runner.log` plus `runner-report.json`, records `jni_onload_results`, and exits `0`
- A local test-backed proof that the native planner now chooses a stable host ABI, stages matching `.so` files into the bundle `lib` root, reports unsupported ABI libraries without pretending they can run, and exposes a minimal asset read through the stub manager
- A local test-backed proof that the first `ANativeWindow`-shaped host surface now preserves explicit width, height, format, and stride metadata and can write a deterministic `first-pixel-marker.txt` artifact with a rendered marker value
- A local test-backed proof that a headless native activity now receives ordered `window_created`, `window_changed`, and `window_destroyed` lifecycle callbacks through a Linuxoid-owned callback journal artifact
- A local test-backed proof that the new Wayland surface fixture always writes a deterministic metadata artifact and reports either a real `wl_surface` creation or an honest fallback reason depending on host availability
- A local test-backed proof that the new EGL smoke fixture always writes a deterministic metadata artifact and reports either a real EGL context plus pbuffer or an honest fallback reason depending on host availability
- A local test-backed proof that the new `ANativeWindow` bridge contract applies one deterministic geometry update, writes stable metadata and event artifacts, and reports whether it is operating in headless fallback or probe-only mode
- A local test-backed proof that `inspect-apk-resources` can read manifest metadata plus asset/resource readiness from a plain APK/ZIP fixture and emit stable JSON for harnesses
- A local test-backed proof that `native-art-classloader-fixture` can inventory dex entries, normalize manifest target classes, write stable classpath artifacts, and report missing host ART honestly without claiming false execution success
- A local test-backed proof that `native-art-class-resolution-fixture` can resolve manifest-target descriptors from real staged DEX contents, report missing descriptors honestly, and write stable resolution artifacts
- A local test-backed proof that `native-art-runtime-smoke` can reuse the classpath plan plus offline class-resolution result, write stable invocation artifacts, and report ART runtime availability honestly without claiming false class execution
- A local test-backed proof that `native-runtime-health-fixture` and `native-runtime-health-replay` classify runtime readiness, select deterministic recovery actions, and emit stable replayable health artifacts without claiming false success
- A local test-backed proof that `native-runtime-recovery-plan` materializes stable recovery-plan artifacts and deterministic action metadata for missing artifact, failed native load, unavailable display, and failed service lookup scenarios
- A local test-backed proof that `native-runtime-diagnostic-replay` merges the existing JSONL traces into a stable replay bundle, writes a deterministic trace index with per-source fingerprints, reports missing trace sources honestly, and diagnoses recovery-needed states without rerunning the UI path
- A local test-backed proof that missing native dependencies keep `overall_ready: false`, preserve `recovery_needed` state, and surface `retry_native_load_after_bundle_refresh` consistently across fixture, command, and replay outputs
- A generated native bootstrap entrypoint that now calls `native-execute-stub <bootstrap-manifest>` instead of routing through a text-only parent shim
- A local test suite that verifies the first scaffold behavior

## Current External Dependencies

Linuxoid does **not** yet run Android apps natively on Linux by itself. The current project still relies on these external dependencies:

- `CMake 4.0+`
- a `C++20` compiler such as `g++` or `clang++`
- `adb` for runtime attachment, activity launch, IME control, and status checks
- `timeout` from GNU coreutils for bounded attached-ADB discovery and preflight checks
- `apktool` for APK decode and manifest/resource staging during `load-apk` flows
- optional `wayland-client` headers and library for the real `native-wayland-surface-fixture` path; Linuxoid falls back honestly when they are not available
- optional `EGL/egl.h` headers and `libEGL` for the real `native-egl-smoke-fixture` path; Linuxoid falls back honestly when they are not available
- a live Android runtime target for execution paths:
  - `Waydroid` for the currently verified installed-package Linux launch flow
  - or an attached ADB target for the generic `attached-adb` backend contract
- a Linux desktop environment that supports `.desktop` launchers and shell scripts for host integration

### Dependency Notes

- The new `launch-package` core path is backend-neutral, and the `native` branch now owns real local discovery, staged package inspection, staged-package preflight, and bootstrap-execution handoff, but **full native Linux Android app execution is still not implemented**.
- The new `launch-apk` path is Linuxoid's first end-to-end native-only APK launch slice, but it is intentionally narrower than full Android app startup: today it expects a stored ZIP APK with a plain-XML `AndroidManifest.xml`, stages native libraries and assets into a deterministic session directory, and does **not** yet decode binary Android manifest XML or execute Java/Kotlin code through ART.
- The new APK-launched surface proof mode extends that same native-only slice with a deterministic surface/session object plus first-frame marker, but it still uses an explicit `backend: headless` fallback when Linuxoid is not driving a real production Android graphics path.
- The new APK asset/resource proof mode extends that same native-only slice with a deterministic session-bound asset and resource bridge, but it still stops at metadata-only `resources.arsc` or `res/` detection rather than full Android Resources decoding, theme inflation, or layout inflation.
- The new APK lifecycle/input proof mode extends that same native-only slice with a deterministic lifecycle controller plus event pump plus input queue foundation, but it still stops well short of full Android framework scheduling, Binder-backed services, or Java/Kotlin ART execution.
- The new `native-service-manager-fixture` path extends that bootstrap/lifecycle session with a Linuxoid-owned Binder-shaped service-registry foundation, but it still reports `local_foundation_only: true`, `real_android_binder: false`, and `system_server_present: false` because full Parcel semantics, kernel Binder, and real Android service behavior are not part of this slice yet.
- The new `plan-native-spike` core path materializes Linuxoid-owned native launch assets, but **those assets are not executing Android bytecode on Linux yet**.
- The new `bootstrap-native-spike` and `native-execute-stub` paths now prove Linuxoid can own the local bootstrap surface, `execve` a real child runner, persist truthful session state, stage host-ABI native libraries plus extracted assets/resources, and emit structured JNI/library results, the new `inspect-apk-resources` path proves Linuxoid can inspect plain APK/ZIP manifest metadata plus normalized asset/resource readiness before ART exists, the `native-art-classloader-fixture` path proves Linuxoid can turn staged APK dex entries plus manifest targets into deterministic classpath artifacts for the next host-ART gate, the `native-art-class-resolution-fixture` path proves Linuxoid can resolve manifest-target descriptors from real staged DEX contents and emit deterministic resolution-map artifacts without pretending ART already executed them, the `native-art-runtime-smoke` path proves Linuxoid can turn that classpath plus resolution evidence into deterministic invocation-plan, runtime-log, and trace artifacts, select a real manifest-derived class target, classify the selected ART probe as bootstrap-capable or detection-only, and attempt a real host-side ART class-resolution command only when a safe local `dalvikvm` surface is available, the upgraded `native-art-activity-bootstrap-fixture` path now proves Linuxoid can normalize a real manifest application class when present, carry launcher resolution plus Binder readiness plus runtime-smoke evidence into deterministic application-plus-activity bootstrap **planning** artifacts, and leave that planning seam marked `ready` when it is materialized, the `native-art-bootstrap-execution-fixture` path now proves Linuxoid can carry that seam one step further into a supervised bootstrap-execution path with deterministic plan, context, runner-script, runner-state, phase-log, trace, and result artifacts, and the `native-runtime-health-fixture` plus `native-runtime-health-replay` paths now prove Linuxoid can classify APK staging, native loading, surface, input, Binder, DEX/classloader, activity-bootstrap planning, and bootstrap-execution readiness separately, expose direct summary fields like `dependency_blocked`, `failing_subsystem_count`, `recovery_actions_selected`, and `failing_subsystems`, emit deterministic recovery plans plus replayable JSONL traces, and rerun live staged-bundle diagnosis quickly enough to be practical by reusing one opened APK archive plus the already-staged bundle manifest, while still leaving full Parcel semantics, real ART/DEX execution, real Android Binder behavior, and full IME/text composition pending.
- The new `native-lifecycle-shim` path proves Linuxoid can own lifecycle/session handoff and service binding artifacts locally, but **it is still a pre-DEX, pre-real-Binder, pre-graphics scaffold seam**.
- The current local `com.android.calculator2` APK staged for Linuxoid is **dex-only** and contains no `lib/*.so`, so it currently serves as a negative oracle rather than the literal `P1` gate app.
- The current live proofs on GitHub are still **runtime-backed**: Waydroid handles the installed-package Linux launch path, and `attached-adb` remains a transition backend plus regression oracle.
- `attached-adb` is now part of the core contract with target-side launcher and metadata lookup, but it is still not the final goal.
- Waydroid and `attached-adb` stay in scope as regression oracles through `P0-P4`, but they are not acceptable end-state runtimes for Linuxoid.
- Future native slices should preserve **MCP and harness compatibility** by keeping commands backend-neutral, outputs inspectable, and artifact paths deterministic for agent workflows.
- The self-healing browser track should start from **Android WebView + Linuxoid BrowserSession**, not a full Chromium browser shell or `Chrome.apk` port, and it should remain frozen until `P5`.

## What To Do Next

These are the next five highest-value moves from the current state if the goal is to run Android apps directly on Linux without depending on Waydroid or any other external Android runtime:

1. Attempt the first real host-ART `PathClassLoader` or equivalent application or activity bootstrap execution path and make JNI ownership real.
   Linuxoid now has the APK manifest/asset/resource seam, a deterministic ART/classloader preparation fixture, an offline DEX class-resolution fixture, a host-ART smoke seam, a deterministic application-plus-activity bootstrap planning seam, and a separate supervised bootstrap-execution seam with runner, runner-state, context, and phase-log artifacts, so the next major boundary is an actual host runtime bootstrap execution step from staged `base.apk`.

2. Widen the asset/resource seam from copied files to real Android resource-table handling.
   Linuxoid can now inspect plain APK/ZIP manifest metadata, list and open normalized staged assets, and report `resources.arsc` plus `res/` metadata through the `launch-apk --asset-proof` slice, but it still needs full `resources.arsc` decoding, binary XML handling, and richer `AAssetManager` or `Resources` behavior before normal apps can rely on Android-style resources.

3. Widen the lifecycle/input seam from deterministic proof to real Android runtime behavior.
   Linuxoid can now drive ordered lifecycle transitions, a deterministic Looper-style queue, and a synthetic InputQueue stream through the `launch-apk --lifecycle-proof` slice, but it still needs full Android framework scheduling, Binder service behavior, and broader production app compatibility before normal apps can rely on it.

4. Replace the local Binder/service-registry foundation with fuller Parcel semantics behind the same transport seam.
   Linuxoid now has deterministic service registration, lookup, package/activity-manager/app-local transactions, honest missing-service lookup reporting, and a socketpair-backed local transport seam, but it still needs Binder object semantics instead of JSON-only fixture payloads.

4. Bind the current Wayland/EGL probes plus focused input seam into one real native activity surface path.
   Linuxoid now has separate Wayland, EGL, `ANativeWindow`, and focused input proofs; the next user-visible step is one combined path that lets a native activity observe the same surface and input contract instead of isolated fixtures.

5. Add a deterministic storage manifest and replace the flat `sandbox_root` assumption.
   Linuxoid now has a truthful parent/child bootstrap path; the next filesystem step is to expose distinct guest paths for private data, code cache, app-specific external data, and OBB roots.

For every step above, keep the interfaces **MCP- and harness-compatible**:
- machine-readable outputs should remain stable
- intermediate artifacts should stay discoverable
- verification commands should stay composable in agent workflows

## Browser Track Status

The self-healing browser track is researched but frozen until `P5 Audio + Network + Browser`.

What stays frozen until then:

- BrowserSession implementation
- Self-healing recovery policy implementation
- DOM and JS bridge work
- Permission, download, and prompt broker work

Why the freeze exists:

- Linuxoid still has only `execution 98/100` on the native path.
- `P1 -> P2` is the real blocker for the whole project.
- Browser work only makes sense after Linuxoid can already host Android UI and app code directly.

The browser target architecture is still useful as a design reference and stays documented here:

Detailed architecture note: [docs/browser-self-healing-architecture.md](/home/astra/codex/wine-for-android/docs/browser-self-healing-architecture.md)
Detailed research wave: [docs/15-track-research-wave-2026-05-16.md](/home/astra/codex/wine-for-android/docs/15-track-research-wave-2026-05-16.md)
Current self-healing runtime note: [docs/self-healing-runtime-skeleton.md](/home/astra/codex/wine-for-android/docs/self-healing-runtime-skeleton.md)

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Verify

```bash
ctest --test-dir build --output-on-failure
./build/compatctl status
./build/compatctl foundation
./build/compatctl layout com.example.demo alpha01 42 /var/lib/wfa
./build/compatctl assess-manifest /path/to/decoded/AndroidManifest.xml
./build/compatctl load-apk /path/to/app.apk /tmp/wfa-load
./build/compatctl launch-apk /path/to/native-only.apk /tmp/linuxoid-apk-launch
./build/compatctl launch-apk --asset-proof /path/to/native-only.apk /tmp/linuxoid-apk-launch
./build/compatctl launch-apk --lifecycle-proof /path/to/native-only.apk /tmp/linuxoid-apk-launch
./build/compatctl launch-apk --storage-proof /path/to/native-only.apk /tmp/linuxoid-apk-launch
./build/compatctl launch-apk --surface-proof /path/to/native-only.apk /tmp/linuxoid-apk-launch
./build/compatctl launch-apk-surface /path/to/native-only.apk /tmp/linuxoid-apk-launch
./build/compatctl discover-runtime attached-adb
./build/compatctl preflight-runtime attached-adb
./build/compatctl inspect-package attached-adb 192.168.240.112:5555 com.android.settings
./build/compatctl launch-activity emulator-5590 org.example.app/.SettingsActivity
./build/compatctl launch-package waydroid com.android.calculator2
./build/compatctl launch-package attached-adb com.android.settings 192.168.240.112:5555
./build/compatctl launch-package native com.example.demo
./build/compatctl verify-package waydroid com.android.calculator2 - - /tmp/linuxoid-generic-applications /tmp/linuxoid-generic-launchers
./build/compatctl verify-package-matrix waydroid /tmp/linuxoid-generic-matrix - com.android.calculator2 com.android.settings org.fdroid.fdroid
./build/compatctl verify-package attached-adb com.android.settings 192.168.240.112:5555 - /tmp/linuxoid-attached-applications /tmp/linuxoid-attached-launchers
./build/compatctl verify-package-matrix attached-adb /tmp/linuxoid-attached-matrix 192.168.240.112:5555 com.android.settings com.android.calculator2 org.fdroid.fdroid
./build/compatctl plan-native-spike /path/to/app.apk /tmp/linuxoid-native-compat /tmp/linuxoid-native-spike
./build/compatctl bootstrap-native-spike /path/to/app.apk /tmp/linuxoid-native-compat /tmp/linuxoid-native-spike
./build/compatctl native-art-classloader-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-art-class-resolution-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-art-runtime-smoke /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-art-activity-bootstrap-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-art-bootstrap-execution-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-runtime-recovery-plan /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
./build/compatctl native-runtime-diagnostic-replay /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-lifecycle-shim /tmp/linuxoid-native-spike/packages/com.example.app/vc1/bootstrap/activity-bootstrap.json
./build/compatctl native-execute-stub /tmp/linuxoid-native-spike/packages/com.example.app/vc1/bootstrap/activity-bootstrap.json
./build/compatctl native-execute-stub com.example.app com.example.app/.MainActivity /tmp/linuxoid-native-spike/packages/com.example.app/vc1/bundle/base.apk /tmp/linuxoid-native-spike/packages/com.example.app/vc1/sandbox /tmp/linuxoid-native-spike/packages/com.example.app/vc1/dex-cache /tmp/linuxoid-native-spike/packages/com.example.app/vc1/resources /tmp/linuxoid-native-spike/packages/com.example.app/vc1/lib /tmp/linuxoid-native-spike/packages/com.example.app/vc1/bootstrap/activity-bootstrap.json
./build/compatctl native-first-pixel-fixture /tmp/linuxoid-first-pixel-smoke
./build/compatctl native-egl-smoke-fixture /tmp/linuxoid-egl-smoke 96 72
./build/compatctl native-wayland-surface-fixture /tmp/linuxoid-wayland-surface-smoke 120 90
./build/compatctl native-window-bridge-fixture /tmp/linuxoid-native-window-bridge-smoke 44 28 1
./build/compatctl native-input-queue-fixture /tmp/linuxoid-native-input-queue-smoke 48 32 1
./build/compatctl native-service-manager-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-window-callback-fixture /tmp/linuxoid-native-window-callback-smoke
./build/compatctl verify-apk-host-launch-auto emulator-5590 /path/to/app.apk /tmp/linuxoid-apk-verify /tmp/linuxoid-apk-applications /tmp/linuxoid-apk-launchers
./build/compatctl launch-waydroid-package com.android.calculator2
./build/compatctl verify-waydroid-package com.android.calculator2 /tmp/linuxoid-applications /tmp/linuxoid-launchers
./build/compatctl verify-waydroid-matrix /tmp/linuxoid-matrix com.android.calculator2 com.android.settings org.fdroid.fdroid
./build/compatctl adb-ime-status emulator-5590 org.example.app org.example.app/.ImeService
./build/compatctl adb-ime-status emulator-5590 org.example.app org.example.app/.ImeService org.example.app/.SettingsActivity
./build/compatctl provision-ime emulator-5590 /path/to/app.apk org.example.app org.example.app/.ImeService org.example.app/.SettingsActivity
./build/compatctl desktopify-apk emulator-5590 /path/to/app.apk org.example.app/.SettingsActivity /tmp/wfa-load /tmp/wfa-desktop
./build/compatctl desktopify-apk-auto emulator-5590 /path/to/app.apk /tmp/wfa-load /tmp/linuxoid-applications /tmp/linuxoid-launchers
./build/compatctl desktopify-waydroid-package com.android.calculator2 /tmp/linuxoid-applications /tmp/linuxoid-launchers
```

## Current Progress

- Phase loading: `96/100`
- Native execution readiness: `98/100`
- Runtime checkpoint gates: `70/100`

These values are generated by the code, not written by hand.
