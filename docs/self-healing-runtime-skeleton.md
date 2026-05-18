# Linuxoid Self-Healing Runtime Skeleton

Linuxoid now has a **self-healing runtime observability skeleton** for the native direct-run path.

Separate from that Self-Healing Android Device work, Linuxoid now also has a direct `launch-apk <apk-path> [staging-root]` native-only APK launch slice. That command inspects stored and deflated ZIP APK entries, reads either plain-XML manifests or a useful decoded subset of binary `AndroidManifest.xml`, stages ABI-matching native libraries plus assets into a deterministic session directory, loads the selected library, calls `JNI_OnLoad` when present, creates a minimal native activity/session object, and emits structured JSON. Linuxoid now also extends that same slice with `launch-apk --surface-proof` and `launch-apk-surface`, which bind the APK launch session to a deterministic surface/session object and a first-frame plus first-pixel proof, with `launch-apk --asset-proof`, which binds the staged APK session to a deterministic Linuxoid AssetManager/Resources-shaped bridge that can list normalized assets, open and checksum one staged asset, and report `resources.arsc` plus `res/` metadata at `decode_level: metadata_only`, with `launch-apk --lifecycle-proof`, which binds the staged APK session to a deterministic lifecycle controller, Looper-style event pump, and InputQueue foundation, with `launch-apk --dex-proof`, which stages `classes.dex` entries, parses safe DEX header plus string/type/proto/method/class metadata, and emits a session-bound `art_bootstrap` contract with `class_loader_ready` while still keeping `java_execution_supported: false`, with `launch-apk --activity-proof [--package <package>] [--component <component>]`, which binds the same staged APK session to a Linuxoid-owned `package_manager`, `intent_resolution`, and `activity_launch` contract that captures labels plus exported/enabled flags plus manifest intent filters, resolves either `MAIN` plus `LAUNCHER` or an explicit component request, and ties activity-launch readiness back to lifecycle, surface, input, Binder, and DEX/bootstrap state, with `launch-apk --storage-proof`, which implements **P9 Android App Storage + Sandbox Contract** by materializing deterministic `sandbox/data/data/<package>`-style storage roots, safe path resolution, and marker-file proof, with `launch-apk --permissions-proof`, which implements **P10 Android Permissions + AppOps Contract** by parsing plain-XML `uses-permission` entries when available or the same decoded binary-manifest subset when present, persisting deterministic requested plus granted plus denied permission state under `sandbox/data/data/<package>/permissions`, emitting stable AppOps records for storage-sensitive access, audio capture placeholder access, Binder/service access, activity launch access, and DEX/runtime access, validating or healing missing, malformed, incomplete, stale, or incompatible contract files without silently granting dangerous permissions, and exposing the same durable contract through `inspect-apk-permissions <apk-path> [staging-root]` for focused inspection, with `launch-apk --process-proof [--package <package>] [--component <component>]`, which implements **P11 Minimal ActivityManager/ProcessManager Contract** by persisting deterministic `activity_manager` plus `process_manager` artifacts under `sandbox/data/data/<package>/process-manager`, recording Linuxoid-owned process identity placeholders plus process policy metadata, validating or healing missing, malformed, incomplete, stale, or incompatible process-manager state, and exposing the same durable contract through `inspect-apk-process <apk-path> [staging-root]`, with `launch-apk --window-proof [--package <package>] [--component <component>]`, which implements **P12 WindowManager + Wayland/EGL Surface Contract** by persisting deterministic `window-state.json`, `window-session-map.json`, and `window-events.jsonl` artifacts under `sandbox/data/data/<package>/window-manager`, recording Linuxoid-owned activity/process/surface mappings, exposing explicit `created`, `attached`, `visible`, `resized`, `hidden`, `destroyed`, `failed`, and `recovered` states, validating or healing missing, malformed, incomplete, stale, or incompatible window-manager state, and exposing the same durable contract through `inspect-apk-window <apk-path> [staging-root]`, with `launch-apk --runtime-proof [--package <package>] [--component <component>]`, which implements **P13 Real ART Runtime Path / Java VM Bootstrap Contract** by persisting deterministic `runtime-state.json`, `runtime-session-map.json`, and `runtime-events.jsonl` artifacts under `sandbox/data/data/<package>/runtime-manager`, recording Linuxoid-owned runtime-root discovery plus boot classpath plus runtime-library inputs, exposing explicit `unavailable`, `discovered`, `configured`, `bootstrapping`, `ready`, `failed`, `degraded`, and `recovered` states, validating or healing missing, malformed, incomplete, stale, or incompatible runtime-manager state, and exposing the same durable contract through `inspect-apk-runtime <apk-path> [staging-root]`, and now with `launch-apk --self-heal-proof [--package <package>] [--component <component>]`, which runs the Self-Healing Android Device watchdog against that same session, consumes those existing health fields plus `storage_health`, `sandbox_health`, `permission_health`, `app_ops_health`, `activity_manager_health`, `process_health`, `window_health`, and `runtime_health`, chooses deterministic local recovery actions including `repair_app_storage`, `rebuild_permission_state`, `rebuild_process_manager_state`, `rebuild_window_manager_state`, and `retry_runtime_bootstrap`, and writes `self-healing-android-device/watchdog-report.json` plus `self-healing-android-device/recovery-journal.jsonl`. Linuxoid now also has a `native-service-manager-fixture <bootstrap-manifest>` path that binds the bootstrap/lifecycle session to a Linuxoid-owned Binder-shaped service-registry foundation with deterministic registration, lookup, missing-service lookup, and transaction artifacts plus explicit `local_foundation_only`, `real_android_binder`, and `system_server_present` truth fields. On the real `/home/astra/Downloads/keyboard-0.1.28.apk` verification target, this direct intake path now reaches trustworthy package, activity, permission, asset, and ABI inventory facts, preloads tiny Android soname compatibility shims when the staged libraries actually need them, normalizes undefined ELF version bindings when safe, defers `JNI_OnLoad` to the selected entry library, gets `libjni_latinime.so` loaded, calls `JNI_OnLoad`, and now stops later at `native_loading_state: linuxoid_managed_app_start_bridge_required` with `native_jni_state: called`, `native_app_start_bridge_state: linuxoid_managed_app_start_bridge_required`, `native_post_jni_startup_state: managed_activity_dispatch_required`, and `native_loading_library_name: libjni_latinime.so`. These direct proof commands are intentionally **not** full Java/Kotlin ART execution or full Android framework healing yet, even though they now expose future Self-Healing Android Device-facing status fields like `surface_health`, `window_health`, `asset_health`, `resource_health`, `lifecycle_health`, `looper_health`, `input_health`, `dex_health`, `art_health`, `binder_health`, `activity_health`, `storage_health`, `sandbox_health`, `permission_health`, `app_ops_health`, `activity_manager_health`, `process_health`, `runtime_health`, `launch_health`, `recoverable`, and `recommended_recovery_action`.

Linuxoid now also exposes **P14 Java/Kotlin APK Proof Contract** through `launch-apk --java-proof [--package <package>] [--component <component>] <apk-path> [staging-root]` and `inspect-apk-java <apk-path> [staging-root]`. That proof path persists `sandbox/data/data/<package>/java-proof/java-proof-state.json`, `java-proof-session-map.json`, and `java-proof-events.jsonl`, wires package plus activity plus process plus window plus runtime readiness into a deterministic Java/Kotlin-style APK session report, and keeps the Self-Healing Android Device diagnostics honest about the boundary: bootstrap and lifecycle wiring are proved, but full ART-owned Java/Kotlin bytecode execution is still future work.

Linuxoid now also exposes a **Managed Activity Start Checkpoint** through `launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]` and `inspect-apk-first-start <apk-path> [staging-root]`. That focused checkpoint still does not add a separate mock layer. It reuses the real direct-session package, activity, process, window, runtime, and Java-proof bridges, persists `sandbox/data/data/<package>/first-app-start/first-app-start.json`, parses real DEX string plus type plus proto plus field plus method plus class tables, and now answers two harder questions directly: can Linuxoid still execute the deterministic minimal lifecycle fixture end to end, and can it resolve the real keyboard verification activity into an exact managed-start seam? On the healthy deterministic fixture path today, Linuxoid resolves `Lcom/example/launchapk/MainActivity;` from staged DEX metadata, materializes a deterministic lifecycle receiver placeholder in register `v0`, executes the fixture `com.example.launchapk.MainActivity.onCreate()I` method through its minimal interpreter, crosses a stubbed `Landroid/app/Activity;->onCreate()V` framework boundary, allocates a placeholder `Lcom/example/launchapk/StateCarrier;` object, executes its tiny app-local constructor body `StateCarrier.<init>()V` through `invoke-direct`, stores that object into `MainActivity.currentCarrier:Lcom/example/launchapk/StateCarrier;` with `iput-object`, reads it back with `iget-object`, reads `value:I` with `iget`, reports `java_art_bytecode_executed: true`, reports the returned integer value, and still reports `blocking_reason: needs-real-activitythread-context`. On the keyboard-identity managed seam today, Linuxoid now resolves `org.futo.inputmethod.latin/.uix.settings.SettingsActivity`, derives `Lorg/futo/inputmethod/latin/uix/settings/SettingsActivity;`, resolves `onCreate(Landroid/os/Bundle;)V` from staged DEX metadata, materializes the lifecycle receiver in the parameter-register window, materializes a deterministic `Landroid/os/Bundle;` placeholder, and stops at the stubbed `Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V` framework boundary with exact `framework_boundary_reason: android_activity_oncreate_bundle_stubbed_for_minimal_checkpoint` plus `blocking_reason: framework-boundary-stubbed:Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V`. The real keyboard launch path still blocks earlier at `launch_status: libraries_failed_to_load` and `surface_not_ready_for_first_app_start`, and none of this is yet a claim of full ART-owned Java/Kotlin application execution.

Linuxoid now also exposes a **Native App-Start Bridge Checkpoint** on that same real keyboard path. `launch-apk` and `launch-apk --first-app-start-proof` now preserve exact upstream native blocker facts through `native_loading_state`, `native_jni_state`, `native_app_start_bridge_state`, `native_app_start_bridge_reason`, `native_post_jni_startup_state`, `native_loading_library_name`, `native_loading_detail`, deterministic `native_execute.library_load_attempts[]` evidence, and nested Android-compat preload facts like `native_execute.android_compat_state`. When a staged host-ABI library fails before managed startup can continue, the Self-Healing Android Device wording now stays native-aware and recommends `inspect_native_launch_diagnostics` instead of drifting straight to a later surface repair suggestion. The current real blocker is now narrower than the earlier helper-library crash and the earlier `__strchr_chk` seam: Linuxoid reaches `linuxoid_managed_app_start_bridge_required:libjni_latinime.so` after loading the library and calling `JNI_OnLoad`. This is still not a claim that JNI registration or framework dispatch succeeds; it only means Linuxoid now keeps the JNI-shaped native app-start boundary explicit and recoverable before the downstream stubbed `Activity.onCreate(Bundle)` seam.

Linuxoid now also exposes a **Visible Wayland Interaction Checkpoint** on that same real keyboard path. `launch-apk --window-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity ...` now binds the resolved `SettingsActivity` session to one deterministic `window_manager` target, records `visible_target_state`, `focus_state`, `focus_owned`, `focus_owner`, `interaction_state`, and `interaction_target_component`, and preserves the exact upstream native blocker as `blocking_reason: linuxoid_managed_app_start_bridge_required:libjni_latinime.so` with `native_jni_state: called`, `native_post_jni_startup_state: managed_activity_dispatch_required`, and `recommended_recovery_action: inspect_native_launch_diagnostics`. When the host can probe a live target, Linuxoid now also keeps `backing_mode`, `wayland_surface_available`, and `egl_surface_available` honest without pretending the app is already visibly rendered or interactive.

Linuxoid now also exposes a **Recovery and Runtime Hardening Checkpoint** on that same real keyboard path. Repeated `launch-apk --storage-proof` and `launch-apk --permissions-proof` runs against the same staging root now preserve and validate one deterministic sandbox, permission, and AppOps story through explicit `persisted_state_preexisting`, `continuity_validated`, `continuity_state`, and `continuity_diagnostics` fields. On the blocked real keyboard path, `launch-apk --self-heal-proof --window-proof --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity ...` now keeps the earliest blocker authoritative through `primary_blocker_reason: linuxoid_managed_app_start_bridge_required:libjni_latinime.so`, `recovery_gating_state: upstream_native_blocker_gated`, `recovery_gating_reason: linuxoid_managed_app_start_bridge_required:libjni_latinime.so`, and `recommended_next_action: inspect_native_launch_diagnostics`, while downstream launch-dependent repairs are journaled as `skipped_upstream_blocker` instead of being noisily attempted behind a known native load failure.

Linuxoid now also exposes **P15 Third-Party APK Compatibility Sprint** through `inspect-apk-compatibility <apk-path> [staging-root]` and `inspect-apk-compatibility-suite <suite-root> <apk-path> [apk-path...]`. That compatibility path reuses the real direct-session contracts rather than a mock: it drives package inspection, activity/intent resolution, storage sandboxing, permissions/AppOps, native/JNI loading, process/session state, window/surface state, runtime bootstrap, Java/Kotlin proof, and Self-Healing Android Device diagnostics into deterministic compatibility artifacts under `sandbox/data/data/<package>/compatibility/compatibility-report.json`, `compatibility-domains.json`, and `compatibility-events.jsonl`, plus a suite-level `suite-compatibility-report.json`. The contract stays explicit that this is current readiness classification for third-party-style APKs, not a claim of full Java/Kotlin bytecode execution or broad production APK compatibility yet.

Git/GitHub update path: use normal git remotes from this environment; if direct authentication is unavailable in a later environment, record `GitHub update blocked: direct GitHub authentication not available from this environment`.

What that means **today**:

- Linuxoid now exposes the self-healing runtime through stable public commands, not only internal helpers:
  - `native-runtime-health-fixture`
  - `native-runtime-recovery-plan`
  - `native-runtime-health-replay`
  - `native-runtime-diagnostic-replay`
  - `native-runtime-diagnostic-fixture`
- Linuxoid can record structured health for:
  - APK staging
  - native library loading readiness
  - surface readiness
  - input queue readiness
  - Binder/service readiness
  - DEX/classloader readiness
- Linuxoid can now hang Binder/service readiness off a deterministic Linuxoid-owned service-registry foundation with session-bound `package_manager`, `activity_manager`, and app-local placeholder services, explicit missing-service lookup artifacts, and honest `local_foundation_only` limitations instead of pretending kernel Binder or `system_server` already exist.
- Linuxoid now also has enough direct-session contract surface to hand off cleanly into **P16 Managed Bytecode Invocation + ActivityThread Contract**:
  - sandbox-backed `package_manager`
  - `intent_resolution`
  - `activity_launch`
  - `storage`
  - `permissions`
  - `app_ops`
  - `activity_manager`
  - `process_manager`
  - `window_manager`
  - `runtime_bridge`
  - `java_apk_proof`
  - `compatibility` report artifacts and suite summaries
  - Self-Healing Android Device health gates for storage, sandbox, permission, AppOps, Binder, DEX, activity, activity-manager, process, window, and runtime readiness
- Linuxoid can select deterministic recovery actions for:
  - missing artifact
  - failed native load
  - unavailable display
  - failed service lookup
  - pending DEX/classloader bootstrap
  - incomplete post-class-resolution activity-bootstrap planning
  - pending post-class-resolution bootstrap execution
- Every selected recovery action now carries deterministic machine-facing metadata:
  - `action_rank`
  - `retry_budget`
  - `recovery_scope`
- Every health report now also carries a deterministic summary that callers can trust directly:
  - `dependency_blocked`
  - `failing_subsystem_count`
  - `recovery_actions_selected`
  - `failing_subsystems`
- Every health report now also exposes the six required runtime-health areas as a stable core projection:
  - `core_subsystems`
  - `core_subsystem_count`
  - `core_ready_subsystem_count`
  - `core_subsystems_ready`
  - `core_subsystem_records`
- Every recovery-plan report now also exposes the four required deterministic recovery cases as a stable scenario contract:
  - `canonical_recovery_scenario_count`
  - `canonical_recovery_scenarios`
- Every diagnostic replay report now also exposes the replayable trace bundle as a stable contract:
  - `health_trace_jsonl_path`
  - `health_replay_json_path`
  - `canonical_trace_source_count`
  - `canonical_trace_source_names`
  - `missing_trace_source_count`
  - `trace_bundle_complete`
- The Linuxoid-owned `native` bridge now carries that same replay contract back out to operators:
  - `preflight-runtime native`
  - `verify-package native`
  - `verify-package-matrix native`
  - `launch-package native`
  - direct six-area core-runtime health projection fields in the public report:
    - `Runtime Core Subsystems Ready`
    - `Runtime Core Subsystem Count`
    - `Runtime Core Ready Subsystem Count`
    - `Runtime Core Subsystems`
    - `Runtime Core Subsystem Details`
  - direct health-trace, recovery-actions-trace, merged-diagnostic-events, and replay-completeness fields in the public report
  - direct replay-artifact and replay-command fields in the public report:
    - `Runtime Recovery Actions Trace Path`
    - `Runtime Health Replay Path`
    - `Runtime Health Replay Command`
    - `Runtime Diagnostic Replay Path`
    - `Runtime Diagnostic Replay Command`
    - `Runtime Diagnostic Fixture Command`
    - `Runtime Diagnostic Trace Index Path`
  - detailed selected-recovery and canonical-scenario lines with rank, retry budget, scope, and reason metadata in the public report
  - per-source replay detail lines with source name, trace path, event count, and deterministic fingerprint metadata in the public report
- Linuxoid now also pins that bridge-level self-healing contract explicitly on the blocked host-ART path:
  - health classification stays deterministic
  - recovery decision selection stays deterministic
  - repeated runtime-health JSON stays stable
  - the public bridge does not drift into false success when ART/bootstrap is still missing
- Linuxoid now also pins that same blocked-host-ART contract through the public verification surfaces:
  - repeated `verify-package native` runs keep the rendered report stable
  - repeated `verify-package native` runs keep both `runtime-health.json` and merged replay JSON stable
  - repeated `verify-package-matrix native` runs keep the rendered row-level diagnosis stable
  - neither verification surface drifts into a fake pass while ART/bootstrap is still missing
- That bridge-level contract now applies cleanly across the direct native operator surfaces plus the native matrix view:
  - `preflight-runtime native`
  - `verify-package native`
  - `launch-package native`
  - and now the native rows inside `verify-package-matrix`
- Linuxoid can now materialize those actions into stable plan artifacts:
  - `runtime-recovery-plan.json`
  - `runtime-recovery-actions.jsonl`
- Linuxoid now has a concrete DEX/classloader preparation seam behind that recovery story:
  - `native-art-classloader-fixture`
  - `art/classloader-plan.json`
  - `art/dex-inventory.json`
  - `art/art-classloader-trace.jsonl`
- Linuxoid now also has a direct APK-session DEX/bootstrap seam that stays separate from the native runtime-health fixtures:
  - `compatctl launch-apk --dex-proof <apk-path> [staging-root]`
  - staged `dex/classes*.dex`
  - `art/dex-proof.json`
  - `art/art-bootstrap.json`
  - `dex_health`
  - `art_health`
  - `java_execution_supported: false`
- Linuxoid now also has a direct APK-session PackageManager plus intent plus activity-launch seam that stays separate from the native runtime-health fixtures:
  - `compatctl launch-apk --activity-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
  - `activity-launch/package-record.json`
  - `activity-launch/intent-resolution.json`
  - `activity-launch/activity-launch.json`
  - `package_manager`
  - `intent_resolution`
  - `activity_launch`
  - `binder_health`
  - `activity_health`
  - honest local-contract-only behavior instead of fake framework startup
  - deterministic blocked reasons:
    - `package_not_found`
    - `no_launcher_activity`
    - `ambiguous_launcher_activities`
    - `component_disabled`
    - `component_not_exported`
    - `unsupported_component_type`
    - `unresolved_activity_class`
- Linuxoid now also has the **P8 Self-Healing Android Device Watchdog + Recovery Loop** on that direct APK-session seam:
  - `compatctl launch-apk --self-heal-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`
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
  - honest local-watchdog-only limitation fields instead of fake Android framework healing
- Linuxoid now persists the **P10 Android Permissions + AppOps Contract** as a real sandbox-backed session contract instead of a one-shot proof dump:
  - `sandbox/data/data/<package>/permissions/permission-state.json`
  - `sandbox/data/data/<package>/permissions/app-ops.json`
  - stable contract fields:
    - `schema_version`
    - `package_name`
    - `user_id`
    - `app_id`
    - `sandbox_root`
    - `updated_at_unix_ms`
    - `contract_ready`
    - `healing_actions`
    - `diagnostics`
  - deterministic healing behavior for:
    - missing contract files
    - malformed JSON
    - incompatible package or sandbox identity
    - stale contract metadata
  - no silent dangerous permission grants; unsupported or dangerous permissions still stay denied unless Linuxoid has an explicit local policy for them
- Linuxoid now has a concrete offline DEX class-resolution seam behind that preparation step:
  - `native-art-class-resolution-fixture`
  - `art/class-resolution-map.json`
  - `art/art-class-resolution-trace.jsonl`
  - `art/art-class-resolution-result.json`
- Linuxoid now also has a host-ART runtime smoke seam that builds on that plan:
  - `native-art-runtime-smoke`
  - `art/art-runtime-probe-inventory.json`
  - `art/runtime-smoke-invocation-plan.json`
  - `art/runtime-smoke-invocation.log`
  - `art/runtime-smoke-trace.jsonl`
  - `art/runtime-smoke-result.json`
  - a deterministic manifest-derived class target for the first real host-side class-resolution attempt when `dalvikvm` or `dalvikvm64` is safely available
- Linuxoid now also records the host ART gate itself as a diagnosable contract instead of a black box:
  - candidate inventory for override, fixed host paths, and PATH lookups
  - explicit `art_runtime_probe_detection_reason`
  - explicit `art_runtime_probe_capability`
  - public `ART Runtime Probe Inventory Path` and `ART Runtime Probe Detection Reason` lines in native preflight and launch reports
- Linuxoid now also distinguishes between **host ART detection** and **host ART bootstrap capability**:
  - `override_bootstrap_capable`
  - `host_dalvikvm_bootstrap_capable`
  - `host_app_process_detection_only`
- Linuxoid now also treats the common 64-bit host binary names as part of that same contract:
  - `dalvikvm64` is classified the same way as `dalvikvm`
  - `app_process64` is classified the same way as `app_process`
- That means a host can now be reported as “ART probe detected” without Linuxoid overstating that the same probe is ready to bootstrap a staged foreground app.
- Linuxoid now also has a deterministic activity-bootstrap seam on top of that runtime-smoke evidence:
  - `native-art-activity-bootstrap-fixture`
  - `art/activity-bootstrap-plan.json`
  - `art/activity-bootstrap-trace.jsonl`
  - `art/activity-bootstrap-result.json`
  - a manifest-derived application-plus-launcher target set plus Binder-readiness evidence for the first post-class-resolution bootstrap attempt
- Linuxoid now also folds that activity-bootstrap seam back into self-healing runtime health and replay:
  - `activity_bootstrap_readiness`
  - ready when the launcher/bootstrap plan is fully materialized
  - replay coverage through `art/activity-bootstrap-trace.jsonl`
- Linuxoid now also exposes a deterministic bootstrap-execution seam on top of that activity-bootstrap plan:
  - `native-art-bootstrap-execution-fixture`
  - `art/bootstrap-execution-plan.json`
  - `art/bootstrap-execution-trace.jsonl`
  - `art/bootstrap-execution-result.json`
  - `art/bootstrap-execution-context.json`
  - `art/bootstrap-execution-runner.sh`
  - `art/bootstrap-execution-runner-state.json`
  - `art/bootstrap-execution-application.log`
  - `art/bootstrap-execution-activity.log`
- Linuxoid now also folds that execution seam back into self-healing runtime health and replay:
  - `bootstrap_execution_readiness`
  - `attempt_host_bootstrap_execution`
  - replay coverage through `art/bootstrap-execution-trace.jsonl`
- Linuxoid now executes that seam through the generated runner script when a safe host runtime is available, and it preserves raw runner plus per-phase exit codes even when later success classification still depends on higher-level output checks.
- Linuxoid now also accepts a Linuxoid-owned ART probe override for deterministic fixture runs, which lets runtime-smoke and bootstrap-execution exercise the real runner-backed execution contract on hosts that do not provide ART locally.
- Runtime health now treats that deeper success path honestly too: when override-backed or real runtime class resolution succeeds and the supervised bootstrap runner completes, `dex_classloader_readiness` and `bootstrap_execution_readiness` can now resolve to `ready` instead of remaining generically pending.
- Runtime health now also treats DEX-only bundles more honestly: if a staged APK declares no native libraries, `native_loading` is classified as `not_required` instead of being treated like a failed native-loader dependency.
- Linuxoid now also exposes a real local `native` runtime bridge for staged target discovery, staged package inspection, staged-package preflight, and bootstrap-execution handoff. That means self-healing no longer depends only on attached Android targets to exercise the public runtime surface.
- Linuxoid now also carries the ART probe inventory and detection rationale through that native bridge, so a blocked default-path host startup can show exactly which probe candidates were examined and why no usable host runtime was selected.
- Linuxoid now also carries the probe-capability classification through that bridge, so native preflight and launch can explain the difference between “ART exists somewhere on this host” and “Linuxoid can safely bootstrap through it right now.”
- Linuxoid now also carries bounded recovery reasons through that bridge, so native preflight and launch render not only deterministic recovery action names plus rank plus retry budget plus scope, but also the operator-facing explanation for why that bounded next step was selected.
- Linuxoid now also carries exact replay commands through that bridge, so a blocked native preflight or launch can be replayed later with `native-runtime-health-replay`, `native-runtime-diagnostic-replay`, or `native-runtime-diagnostic-fixture` without reconstructing the shell command by hand.
- Linuxoid writes stable artifacts for diagnosis:
  - `runtime-health.json`
  - `runtime-health-trace.jsonl`
  - `runtime-health-replay.json`
  - `runtime-diagnostic-trace-index.json`
  - `runtime-diagnostic-events.jsonl`
  - `runtime-diagnostic-replay.json`
- The traces can now be replayed and merged without rerunning the full UI path.
- Linuxoid now also records per-source fingerprints plus first and last event types so replay bundles can be compared offline without reopening the UI path.
- Linuxoid now also reuses one opened APK archive plus the already-staged bundle manifest during native runtime diagnosis, which keeps live health and replay commands fast enough to rerun on real staged bundles instead of timing out behind repeated full-archive scans and repeated decode fallback.
- Linuxoid keeps those runtime-health and replay commands backward-compatible with older staged bootstrap manifests that do not yet include the newer native-library summary fields.

In plain terms, Linuxoid can now **detect, classify, explain, and replay** failure states on the native path. It can tell us why bootstrap is blocked, choose the next bounded recovery action, and preserve that decision in a machine-readable way for later diagnosis.

That is the practical meaning of “self-healing” here:

- Linuxoid can tell us the truth about a blocked run.
- Linuxoid can tell us the bounded next step that belongs to that failure.
- Linuxoid can give us stable artifacts and replay commands so we do not have to rerun the whole UI path just to inspect the failure again.
- Linuxoid still cannot turn that blocked state into a real successful Android app start on the default host path by itself.

A **successful self-healing pass today** means:

- the runtime state was classified truthfully
- the bounded next recovery action was selected deterministically
- the resulting JSON and JSONL artifacts are stable and replayable
- Linuxoid did not claim a launched app if the execution path was still blocked
- the public native bridge now also says that verdict directly through `Runtime Health Classification` and `Runtime Overall Ready`
- that pass may still end with `Ready For Launch: no` or `Launch OK: no` on the public native bridge when the correct outcome is still “blocked but diagnosable”
- that same truthful outcome can now also appear directly in verification and matrix reports as `Preflight OK: no`, `Direct Launch OK: no`, or `Packages Passed: 0/1`

What remains blocked after this phase:

- full Android framework permission-manager behavior
- full AppOps service semantics
- real Java/Kotlin ART execution
- real ActivityThread-style application bootstrap, managed class loading, and bytecode invocation beyond the current Java/Kotlin proof contract

Next phase: **P16 Managed Bytecode Invocation + ActivityThread Contract**

The practical command-level contract today is:

1. `native-runtime-health-fixture` tells us which subsystem is blocked.
2. `native-runtime-recovery-plan` tells us which bounded recovery action belongs to that failure class.
3. `native-runtime-health-replay` and `native-runtime-diagnostic-replay` let us revisit that diagnosis later from artifacts alone.
4. `preflight-runtime native`, `verify-package native`, `verify-package-matrix native`, and `launch-package native` surface that same diagnosis directly to operators instead of forcing them to open the deeper artifacts first.
5. Repeated `verify-package native` runs keep the rendered report plus the generated health and replay JSON artifacts stable on the blocked host-ART path.
6. Repeated `verify-package-matrix native` runs keep the rendered row-level diagnosis stable on that same blocked path.
7. None of those commands claim successful Android execution unless the deeper runtime seams actually succeed.

That summary is now explicit in the health JSON. A harness no longer has to scan every raw record to answer basic questions like:

- Is the runtime blocked on a real dependency?
- How many subsystems are failing right now?
- Which bounded recovery actions were selected?
- Which failing subsystems caused those actions?

What it **does not** mean yet:

- Linuxoid does **not** yet self-heal by automatically making the app run after a failure.
- Linuxoid does **not** yet auto-fix or rerun real Android app execution.
- Linuxoid does **not** yet own a full embedded ART runtime or a real in-process `PathClassLoader`.
- Linuxoid does **not** yet execute full Android app startup through host-side ART, even though it can now resolve manifest-target descriptors offline from real DEX contents, prepare a real host-side class-resolution command for `dalvikvm` or `dalvikvm64` when that runtime exists, exercise the runner-backed execution seam through a Linuxoid-owned override probe in fixtures, separate activity-bootstrap planning from bootstrap execution, and keep deterministic execution-context, runner-state, runner-script, and per-phase log artifacts around that execution seam.
- Linuxoid does **not** yet prove real host-side ART startup for a staged foreground app by default; even a detected host `app_process` probe is still reported as detection-only, and the strongest successful path today is still an override-backed Linuxoid fixture seam.
- Linuxoid does **not** yet guarantee that a healthy self-healing report implies a launch-ready public bridge; the bridge can still end in a truthful blocked state with replay-ready artifacts while host ART startup remains unavailable.
- Linuxoid does **not** yet have full Android Binder semantics.
- Linuxoid does **not** yet have real `PackageManagerService`, `ActivityManagerService`, or a full Android intent-dispatch stack; the new `launch-apk --activity-proof` surface is a Linuxoid-owned local contract for future Self-Healing Android Device work.
- Linuxoid does **not** yet have real host-side process ownership or a real Android process supervisor; the new `launch-apk --process-proof` and `inspect-apk-process` surfaces are Linuxoid-owned local contracts for future Self-Healing Android Device work.
- Linuxoid does **not** yet have compositor-backed Android rendering.
- Linuxoid does **not** yet have full IME/text composition.

The clean boundary is:

- **Self-healing now:** truthful diagnosis, deterministic bounded recovery, stable replay.
- **Full Android execution later:** real host ART class execution, real application/activity bootstrap, richer framework behavior, and real compositor/input binding.

For now, **self-healing means bounded recovery planning and replayable diagnosis**, not complete runtime autonomy.

Current meaning of “self-healing” in Linuxoid:

1. Detect runtime readiness and failure shape deterministically.
2. Classify the failure into a known subsystem bucket.
3. Select a bounded recovery action with explicit rank, retry budget, and scope.
4. Persist the diagnosis and recovery plan in replayable artifacts.
5. Merge the existing JSONL traces back into one diagnostic replay bundle.
6. Refuse false success when a required dependency is still missing.
7. Expose the self-healing summary directly so repeated runs produce stable machine-facing answers.
8. Carry class-resolution evidence forward into a deterministic application/activity bootstrap planning seam without pretending host ART already executed the target.
9. Treat post-class-resolution activity bootstrap planning as a first-class health and replay subsystem instead of leaving it as a side artifact.
10. Treat bootstrap execution as its own bounded health and replay subsystem so recovery can point at execution instead of the already-materialized plan.

## Remaining Gaps Before Full Android App Execution

The self-healing layer is no longer the main blocker. The remaining blockers are execution blockers:

1. A real host-side ART invocation that can move beyond the current class-resolution and activity-bootstrap planning seams into actual app bootstrap.
2. Real Android framework/service behavior behind the Binder-shaped local seam.
3. A bound Wayland + EGL + `ANativeWindow` path that can carry actual Android drawing.
4. Input that goes beyond deterministic pointer/key fixtures into real IME/text composition.
5. Resource handling beyond manifest/assets into full Android resource-table semantics.
6. A real staged foreground candidate app that can move through the public native runtime bridge and succeed without relying on the Linuxoid-owned ART override seam.
7. That same staged foreground candidate app must cross the default public path itself: `preflight-runtime native`, `verify-package native`, and `launch-package native` need to stop ending in “blocked but diagnosable” and start ending in a genuine host-ART-owned success path.

Until those gates land, Linuxoid can diagnose and replay failures very well, but it still cannot claim full native Android app execution on Linux. The strongest successful path today is still a Linuxoid-owned fixture seam, not the default host-side app-execution path the project is ultimately aiming for.

The short version is: self-healing is already a good **diagnosis and bounded-recovery** layer, but it is not yet the thing that makes a staged Android app fully run. The next real milestone is still the first **default-path, no-override, host-ART-owned** startup of a staged foreground candidate app.

Current commands:

```bash
./build/compatctl native-runtime-health-fixture <bootstrap-manifest> [scenario]
./build/compatctl native-runtime-recovery-plan <bootstrap-manifest> [scenario]
./build/compatctl native-runtime-health-replay <trace-jsonl-path>
./build/compatctl native-runtime-diagnostic-replay <bootstrap-manifest>
./build/compatctl native-runtime-diagnostic-fixture <bootstrap-manifest> [scenario]
./build/compatctl native-art-classloader-fixture <bootstrap-manifest>
./build/compatctl native-art-class-resolution-fixture <bootstrap-manifest>
./build/compatctl native-art-runtime-smoke <bootstrap-manifest>
./build/compatctl native-art-activity-bootstrap-fixture <bootstrap-manifest>
./build/compatctl native-art-bootstrap-execution-fixture <bootstrap-manifest>
```

Current deterministic scenarios:

- `baseline`
- `missing_artifact`
- `failed_native_load`
- `unavailable_display`
- `failed_service_lookup`

Next gate after this skeleton:

- use the existing offline class-resolution plus host-ART smoke seams, the upgraded activity-bootstrap planning seam, and the deterministic bootstrap-execution seam to drive the first real application or activity bootstrap execution step after class resolution, not just the first class-target lookup.
