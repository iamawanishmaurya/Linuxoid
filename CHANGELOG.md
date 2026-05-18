# Changelog

## v0.1.125 - 2026-05-19

- Close **Phase 7: Native libc Compatibility and Entry Bridge** around the real keyboard APK entry library instead of widening the runtime sideways again.
- Extend the Android-compat shim slice so Linuxoid now gets `/home/astra/Downloads/keyboard-0.1.28.apk` past the earlier `__strchr_chk` seam, selects `libjni_latinime.so` as the authoritative entry library, calls `JNI_OnLoad`, records deterministic `jni_onload_results`, and preserves later libraries as `skipped_after_primary_selection`.
- Tighten the direct CLI exit path so blocked `launch-apk` and `native-execute-stub` reports flush and exit cleanly after emitting JSON, which keeps the new blocker stable and honest: `native_loading_state: native_activity_entrypoint_missing`, `native_jni_state: called`, `native_loading_library_name: libjni_latinime.so`, and `next_blocker: provide_native_activity_entrypoint_for_libjni_latinime_so`.

## v0.1.124 - 2026-05-19

- Tighten the direct keyboard APK native seam instead of widening the runtime: `launch-apk` now surfaces Android-compat preload details inside the nested `native_execute` report and no longer eagerly calls `JNI_OnLoad` on every loaded helper library before Linuxoid knows which library owns the activity entry boundary.
- Defer `JNI_OnLoad` to the selected entry library, add regression coverage for a non-entrypoint sidecar library with its own `JNI_OnLoad`, and keep the Android-compat fixture plus missing-symbol fixture coverage green.
- Move the real `/home/astra/Downloads/keyboard-0.1.28.apk` blocker forward from a crash in `libandroidx.graphics.path.so` to a sharper native seam: Linuxoid now reports `android_compat_state: preloaded_and_version_normalized`, `native_loading_state: native_activity_entrypoint_missing`, and `native_loading_detail: ...libjni_latinime.so: undefined symbol: __strchr_chk` while the managed first-app-start path still reaches staged DEX/class lookup and stops honestly with `next_blocker: provide_native_activity_entrypoint_for_libjni_latinime_so`.

## v0.1.123 - 2026-05-19

- Close **Phase 6: Recovery and Runtime Hardening** around repeated keyboard APK state continuity and exact watchdog gating instead of widening the runtime sideways again.
- Extend `launch-apk --storage-proof` and `launch-apk --permissions-proof` so repeated runs against the same staging root now preserve and validate deterministic sandbox, permission, and AppOps artifacts through explicit `persisted_state_preexisting`, `continuity_validated`, `continuity_state`, and `continuity_diagnostics` fields.
- Tighten the Self-Healing Android Device watchdog so blocked keyboard APK launches now keep `primary_blocker_reason: native_dlopen_failed:libandroidx.graphics.path.so` authoritative, emit `recovery_gating_state: upstream_native_blocker_gated`, and journal downstream launch-dependent repairs as `skipped_upstream_blocker` instead of attempting noisy process/window/runtime retries behind a known native `dlopen` failure.

## v0.1.122 - 2026-05-18

- Close **Phase 5: Visible Wayland Interaction** around the real keyboard APK window/focus seam instead of widening the runtime sideways.
- Extend `launch-apk --window-proof` and `inspect-apk-window` so Linuxoid now binds the resolved `org.futo.inputmethod.latin/.uix.settings.SettingsActivity` session to one deterministic `window_manager` target, records `visible_target_state`, `focus_state`, `focus_owned`, `focus_owner`, `interaction_state`, and `interaction_target_component`, and persists that same continuity through `window-state.json` and `window-session-map.json`.
- Keep the blocker honest on the real `/home/astra/Downloads/keyboard-0.1.28.apk` path: Linuxoid now preserves `blocking_reason: native_dlopen_failed:libandroidx.graphics.path.so` plus `recommended_recovery_action: inspect_native_launch_diagnostics` while still reporting best-effort live-host `backing_mode`, `wayland_surface_available`, and `egl_surface_available` truth.

## v0.1.121 - 2026-05-18

- Close **Phase 4: JNI and Native Loading** around the real keyboard APK native seam instead of widening the runtime sideways.
- Extend the direct `launch-apk` native execute report so Linuxoid now records deterministic per-library `dlopen`, `JNI_OnLoad`, and entrypoint attempt facts through `native_execute.library_load_attempts[]`, and surface exact top-level fields like `native_loading_state`, `native_jni_state`, `native_loading_library_name`, and `native_loading_detail`.
- Thread that upstream native blocker through `launch-apk --first-app-start-proof` so the managed checkpoint now stays honest about native load failures with `blocking_reason: native_dlopen_failed_for_first_app_start:<library>`, `recommended_recovery_action: inspect_native_launch_diagnostics`, and an exact next blocker instead of drifting into later surface/runtime placeholder guidance.

## v0.1.120 - 2026-05-18

- Close **Phase 3: Runtime Context Bridge** around the first post-receiver managed-runtime seam instead of widening the interpreter sideways.
- Extend the synthetic keyboard-identity `SettingsActivity.onCreate(Landroid/os/Bundle;)V` fixture path so Linuxoid now materializes the lifecycle receiver in the parameter-register window, materializes a deterministic `Landroid/os/Bundle;` placeholder alongside it, and reaches the stubbed `Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V` framework boundary with exact proof fields instead of collapsing at `invoke_receiver_missing`.
- Keep the blocker honest: the real keyboard APK still stops earlier at `launch_status: libraries_failed_to_load` and `surface_not_ready_for_first_app_start`, while the managed-runtime seam now reports `framework_boundary_reason: android_activity_oncreate_bundle_stubbed_for_minimal_checkpoint` with `next_blocker: bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`.

## v0.1.119 - 2026-05-18

- Close **Phase 2: Managed Activity Start** around the real keyboard verification target instead of only the synthetic `MainActivity` seam.
- Fix real-APK DEX string decoding for staged MUTF-8 metadata, add explicit `target_class_lookup_state`, `target_method_lookup_state`, and `code_item_lookup_state` reporting, and keep the direct `launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]` path honest about where real managed startup stops.
- Linuxoid now resolves `org.futo.inputmethod.latin/.uix.settings.SettingsActivity`, derives `Lorg/futo/inputmethod/latin/uix/settings/SettingsActivity;`, resolves `onCreate(Landroid/os/Bundle;)V` from staged DEX metadata, and reports exact managed-start boundaries instead of collapsing the real keyboard APK path into generic `dex_unavailable` or `not_attempted` states.
- Keep the blocker honest: the real keyboard APK still stops earlier at `launch_status: libraries_failed_to_load` and `surface_not_ready_for_first_app_start`, while the synthetic keyboard-identity execution seam now surfaces the next exact managed-runtime blocker as `framework_boundary_reason: invoke_receiver_missing` with `next_blocker: propagate_framework_invoke_receiver_registers`.

## v0.1.118 - 2026-05-18

- Harden the direct APK intake path for the real `keyboard-0.1.28.apk` verification target without expanding sideways into new runtime architecture.
- Add stored plus deflated ZIP entry support to the APK archive reader, replace the slow whole-file iterator path with a direct sized read, and add a minimal binary `AndroidManifest.xml` decoder that feeds the existing manifest/package/component/permission pipeline.
- Extend the direct `launch-apk`, `inspect-apk-resources`, and `inspect-apk-permissions` flow so the real keyboard APK now returns trustworthy `archive_binary_xml_decoded` manifest metadata, launcher activity facts, permission inventory, and asset/resource readiness instead of hanging behind the old plain-XML-only seam.
- Keep the blocker honest: the real keyboard APK now fails later at `launch_status: libraries_failed_to_load`, so the next narrow execution task is native library loading/runtime compatibility, not manifest decoding.

## v0.1.117 - 2026-05-18

- Advance the execution-first checkpoint into a **First DEX Constructor + Object Reference Checkpoint** on top of `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`, still scoped to one deterministic `MainActivity` seam instead of broad framework growth.
- Extend the minimal DEX interpreter and first-app-start report so Linuxoid now allocates a placeholder `Lcom/example/launchapk/StateCarrier;`, executes a tiny app-local constructor body `Lcom/example/launchapk/StateCarrier;-><init>()V` through `invoke-direct`, stores that object into `MainActivity.currentCarrier:Lcom/example/launchapk/StateCarrier;` with `iput-object`, reads it back through `iget-object`, reads `value:I` through `iget`, and returns the deterministic integer result.
- Keep the result honest: Linuxoid now supports one tiny constructor plus object-reference field seam, but this is still not a real ART class loader, ActivityThread, heap-backed framework dispatch, or end-to-end Android app execution, and the next blocker remains `bridge_activity_oncreate_into_real_art_runtime_context`.

## v0.1.116 - 2026-05-18

- Advance the execution-first checkpoint into a **First DEX App-Method Invocation Checkpoint** on top of `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`, still scoped to one deterministic `MainActivity` seam instead of broad framework growth.
- Extend the minimal DEX interpreter and first-app-start report so Linuxoid now resolves `Lcom/example/launchapk/MainActivity;` from staged DEX metadata, materializes a deterministic lifecycle receiver placeholder in `v0`, executes a tiny app-local `invoke-direct` helper method `linuxoidComputeValue()I`, and propagates its return value through `move-result` before the existing object/field checkpoint path.
- Keep the result honest: Linuxoid now supports one tiny app-local method invocation seam, but this is still not a real ART class loader, ActivityThread, heap-backed framework dispatch, or end-to-end Android app execution, and the next blocker remains `bridge_activity_oncreate_into_real_art_runtime_context`.

## v0.1.115 - 2026-05-18

- Advance the execution-first checkpoint into a **First DEX Class-Loading/Lifecycle Receiver Checkpoint** on top of `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`, still scoped to one deterministic `MainActivity` seam instead of broad framework growth.
- Extend the minimal DEX interpreter and first-app-start report so Linuxoid now resolves `Lcom/example/launchapk/MainActivity;` from staged DEX/class-loader metadata, materializes a deterministic lifecycle receiver placeholder in register `v0`, and then executes the existing stubbed `Landroid/app/Activity;->onCreate()V` boundary plus placeholder object/field round-trip.
- Keep the result honest: Linuxoid now models one tiny class-loading plus lifecycle-receiver contract before bytecode interpretation, but this is still not a real ART class loader, ActivityThread, or heap-backed Android framework execution, and the next blocker remains `bridge_activity_oncreate_into_real_art_runtime_context`.

## v0.1.114 - 2026-05-18

- Advance the execution-first checkpoint into a **First DEX Object/Register/Field Checkpoint** on top of `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`, still scoped to one deterministic `MainActivity` seam instead of broad framework growth.
- Extend the minimal DEX interpreter and first-app-start report so Linuxoid now executes a placeholder `new-instance` allocation plus deterministic `iput` and `iget` field access for `Lcom/example/launchapk/StateCarrier;->value:I` after the existing stubbed `Landroid/app/Activity;->onCreate()V` boundary.
- Keep the result honest: Linuxoid now models one tiny object/register/field path with `object_register_field_state: object-placeholder` and still returns from `MainActivity.onCreate()I`, but this is still not a real ART heap or full Android framework execution, and the next blocker remains `bridge_activity_oncreate_into_real_art_runtime_context`.

## v0.1.113 - 2026-05-18

- Advance the execution-first checkpoint into a **First Android Framework Boundary Checkpoint** on top of `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`, still scoped to one deterministic app lifecycle seam instead of adding broad framework architecture.
- Extend the minimal DEX interpreter and first-app-start report so Linuxoid resolves a real `invoke-super` boundary from `com.example.launchapk.MainActivity.onCreate()I` into `Landroid/app/Activity;->onCreate()V`, records the invoked class plus method plus signature, and marks that boundary explicitly as `framework-stubbed`.
- Keep the result honest: Linuxoid now supports a tiny method-resolution plus `invoke-super` slice and still returns from the lifecycle method, but the report continues to say that full ART-owned ActivityThread or Android framework execution has not happened yet and the next blocker remains `bridge_activity_oncreate_into_real_art_runtime_context`.

## v0.1.112 - 2026-05-18

- Advance the execution-first checkpoint from a standalone DEX helper method into a **First MainActivity Bytecode Checkpoint** on top of `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]`, keeping the change tightly scoped to one deterministic lifecycle entrypoint instead of adding new broad compatibility layers.
- Extend the fixture and first-app-start path so Linuxoid resolves the real `MainActivity` class and executes a tiny `onCreate()I`-style DEX method through the minimal interpreter, recording `lifecycle_method_name`, `lifecycle_method_signature`, decoded plus executed instruction counts, first plus last opcode, and the returned integer value.
- Keep the result honest: the checkpoint now proves a MainActivity lifecycle bytecode slice returns cleanly through Linuxoid's own interpreter, while still reporting `needs-real-activitythread-context` and `bridge_activity_oncreate_into_real_art_runtime_context` as the next exact blocker beyond this minimal managed-lifecycle seam.

## v0.1.111 - 2026-05-18

- Advance the **First DEX Method Return Checkpoint** on top of `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]` and `compatctl inspect-apk-first-start <apk-path> [staging-root]`, keeping the same execution-first scope and avoiding any new broad compatibility scaffolding.
- Extend the minimal DEX interpreter from a single-opcode `return-void` proof into a tiny real method-return slice that parses the same deterministic fixture method as `linuxoidCheckpoint()I`, executes `const/4` followed by `return`, records decoded plus executed instruction counts, and reports `returned_value_type: "I"` with `returned_value: "1"`.
- Keep unsupported boundaries precise and honest: Linuxoid still reports exact opcode plus offset plus method details when it stops, and the healthy fixture path still names `needs-real-activitythread-context` plus `bridge_activity_oncreate_into_real_art_runtime_context` as the next real blocker beyond this minimal interpreter checkpoint.
- Update repo docs and status output to say plainly that Linuxoid now reaches a real DEX `return` for one tiny fixture method, but this is still not full ART-owned Java/Kotlin execution or Android framework dispatch.

## v0.1.110 - 2026-05-18

- Add **First DEX Bytecode Execution Checkpoint** on top of `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]` and `compatctl inspect-apk-first-start <apk-path> [staging-root]`, reusing the real direct-session package, activity, process, window, runtime, and Java-proof seams instead of adding a disconnected compatibility layer.
- Extend the DEX bridge from header-and-count metadata into a minimal real DEX reader plus interpreter path that parses string, type, proto, method, and class tables, locates the deterministic fixture method `linuxoidCheckpoint()V`, decodes the first opcode, and either executes through `return-void` or reports the exact unsupported-opcode boundary with instruction offset, opcode name, and executed-instruction count.
- Keep the checkpoint honest: the healthy fixture path now reports `java_art_bytecode_executed: true`, `bytecode_execution_backend: linuxoid_minimal_dex_interpreter`, `blocking_reason: needs-real-activitythread-context`, and `next_blocker: bridge_activity_oncreate_into_real_art_runtime_context`, while the runtime-missing path stays blocked with `art_runtime_unavailable_for_first_app_start` even if the minimal interpreter can still execute the deterministic probe method.
- Update repo docs and status output to say plainly that Linuxoid now executes a tiny real DEX bytecode slice for one fixture app, but this is still not full ART-owned Java/Kotlin application execution or Android framework dispatch.

## v0.1.109 - 2026-05-18

- Add **First Android App Start Checkpoint** through `compatctl launch-apk --first-app-start-proof [--package <package>] [--component <component>] <apk-path> [staging-root]` and `compatctl inspect-apk-first-start <apk-path> [staging-root]`, wiring one deterministic Java/Kotlin-style fixture through APK inspection, launcher intent resolution, storage/sandbox setup, process/window/runtime proof, DEX staging, Java proof, and Self-Healing Android Device diagnostics.
- Persist a focused first-start artifact under `sandbox/data/data/<package>/first-app-start/first-app-start.json` and make the report explicit about the current managed-runtime boundary with fields such as `runtime_state`, `dex_state`, `class_loader_ready`, `java_art_bytecode_execution_requested`, `java_art_bytecode_executed`, `blocking_reason`, and `next_blocker`.
- Keep the checkpoint honest: the healthy fixture path now reports `blocking_reason: needs-real-art-execution` and `next_blocker: implement_real_art_activity_bytecode_invocation` instead of pretending real Java/Kotlin bytecode already executes.
- Add hard checkpoint regression coverage for the healthy ART-boundary path plus invalid-runtime and invalid-DEX blockers, while keeping the existing P1 through P15 behavior green.

## v0.1.108 - 2026-05-18

- Implement **P15 Third-Party APK Compatibility Sprint** through `compatctl inspect-apk-compatibility <apk-path> [staging-root]` and `compatctl inspect-apk-compatibility-suite <suite-root> <apk-path> [apk-path...]`, extending the direct staged APK session into a Linuxoid-owned compatibility-report contract instead of a disconnected mock matrix.
- Add a deterministic compatibility bridge that classifies manifest, package metadata, activity/intent resolution, permissions/AppOps, storage sandbox, native/JNI load, assets/resources, process/session, window/surface, runtime bootstrap, Java/Kotlin proof, and Self-Healing Android Device diagnostics across statuses such as `supported`, `partial`, `blocked`, `missing-runtime`, `missing-surface`, `missing-native-lib`, `needs-real-art`, `recovered`, and `degraded`.
- Persist deterministic compatibility artifacts under `sandbox/data/data/<package>/compatibility/compatibility-report.json`, `compatibility-domains.json`, and `compatibility-events.jsonl`, plus a suite-level `suite-compatibility-report.json`, and add regression coverage for three locally generated third-party-style APK fixtures: a Java/Kotlin activity app, a permission-heavy asset/resource/storage app, and a native/JNI app with broken DEX/bootstrap metadata.
- Harden direct `launch-apk --self-heal-proof` finalization so the post-watchdog report rehydrates repaired storage, permissions/AppOps, process manager, window manager, runtime bridge, Java proof, and related health fields before compatibility classification, keeping the Self-Healing Android Device output authoritative after recovery.
- Update repo docs and status output to make **P15 Third-Party APK Compatibility Sprint** the current direct-runtime slice and point the next handoff at **P16 Managed Bytecode Invocation + ActivityThread Contract**.

## v0.1.107 - 2026-05-18

- Implement **P14 Java/Kotlin APK Proof Contract** on the direct `launch-apk` path through `--java-proof` and `inspect-apk-java`, extending the staged APK session into a Linuxoid-owned `java_apk_proof` contract that wires package inspection, intent/activity resolution, process/session identity, window/surface state, runtime bootstrap state, and Self-Healing Android Device diagnostics together without claiming full Java/Kotlin bytecode execution.
- Persist deterministic Java/Kotlin proof artifacts under `sandbox/data/data/<package>/java-proof/java-proof-state.json`, `java-proof-session-map.json`, and `java-proof-events.jsonl`, and validate or heal missing, malformed, incomplete, stale, or incompatible proof state without introducing Waydroid, emulator, ADB, Android SDK, Gradle, network, or live-display test dependencies.
- Add regression coverage for successful Java/Kotlin-style proof APK execution wiring, deterministic session artifact mapping, focused `inspect-apk-java` operator output, useful blocked diagnostics for invalid DEX and missing runtime configuration, healing of malformed proof state, and preservation of the existing P2 through P13 behavior.
- Update repo docs and status output to make **P14 Java/Kotlin APK Proof Contract** the current direct-runtime slice and point the next handoff at **P15 Managed Bytecode Invocation + ActivityThread Contract**.

## v0.1.106 - 2026-05-18

- Implement **P13 Real ART Runtime Path / Java VM Bootstrap Contract** on the direct `launch-apk` path through `--runtime-proof` and `inspect-apk-runtime`, extending the staged APK session into a Linuxoid-owned `runtime_bridge` contract that models runtime-root discovery, boot classpath assembly, native runtime library directories, staged dex inputs, package/activity/process/window identity, and a deterministic runtime handle without Waydroid, emulator, or ADB.
- Persist deterministic runtime-manager artifacts under `sandbox/data/data/<package>/runtime-manager/runtime-state.json`, `runtime-session-map.json`, and `runtime-events.jsonl`, and validate or heal missing, malformed, incomplete, stale, or incompatible runtime-manager state without claiming full ART bytecode execution.
- Extend the Self-Healing Android Device watchdog so it now consumes `runtime_health`, records deterministic `retry_runtime_bootstrap` recovery attempts in `self-healing-android-device/recovery-journal.jsonl`, and can retry blocked or failed runtime bootstrap after upstream package/activity/process/window/storage/permission/AppOps/Binder/DEX repairs converge.
- Add regression coverage for direct runtime-proof success, deterministic runtime artifact emission, focused `inspect-apk-runtime` operator output, CLI healing of malformed runtime-manager artifacts, watchdog retry of failed runtime bootstrap, and preservation of the existing P2 through P12 behavior.
- Update repo docs and status output to make **P13 Real ART Runtime Path / Java VM Bootstrap Contract** the current direct-runtime slice and point the next handoff at **P14 Runtime Process Handoff + Resume Contract**.

## v0.1.105 - 2026-05-18

- Implement **P12 WindowManager + Wayland/EGL Surface Contract** on the direct `launch-apk` path through `--window-proof` and `inspect-apk-window`, extending the staged APK session into a Linuxoid-owned `window_manager` contract that maps activity/process identity onto the existing native surface proof without Waydroid, emulator, or ADB.
- Persist deterministic window-manager artifacts under `sandbox/data/data/<package>/window-manager/window-state.json`, `window-session-map.json`, and `window-events.jsonl`, and validate or heal missing, malformed, incomplete, stale, or incompatible window-manager state without claiming full Android WindowManagerService or production compositor compatibility.
- Extend the Self-Healing Android Device watchdog so it now consumes `window_health`, records deterministic `rebuild_window_manager_state` recovery attempts in `self-healing-android-device/recovery-journal.jsonl`, and can rebuild blocked window-manager contracts after upstream surface, process, storage, permission/AppOps, Binder, lifecycle, or DEX/bootstrap repairs converge.
- Add regression coverage for direct window-proof success, deterministic surface/session mapping, focused `inspect-apk-window` operator output, CLI healing of malformed window-manager artifacts, window recovery after blocked-surface repair, and preservation of the existing P2 through P11 behavior.
- Update repo docs and status output to make **P12 WindowManager + Wayland/EGL Surface Contract** the current direct-runtime slice and point the next handoff at **P13 Runtime Process Handoff + Resume Contract**.

## v0.1.104 - 2026-05-18

- Implement **P11 Minimal ActivityManager/ProcessManager Contract** on the direct `launch-apk` path through `--process-proof` and `inspect-apk-process`, extending the staged APK session into Linuxoid-owned `activity_manager` and `process_manager` contracts with deterministic process identity, lifecycle state, start reason, launch component, restart policy, termination policy, and sandbox-backed process artifacts.
- Persist deterministic process-manager artifacts under `sandbox/data/data/<package>/process-manager/activity-manager-state.json` and `sandbox/data/data/<package>/process-manager/process-state.json`, and validate or heal missing, malformed, incomplete, stale, or incompatible process-manager state without claiming full Android framework process execution.
- Extend the Self-Healing Android Device watchdog so it now consumes `activity_manager_health` plus `process_health`, records deterministic `rebuild_process_manager_state` recovery attempts in `self-healing-android-device/recovery-journal.jsonl`, and can rebuild blocked process-manager contracts after upstream storage, permission/AppOps, surface, Binder, lifecycle, or DEX/bootstrap repairs converge.
- Add regression coverage for direct process-proof success, deterministic sandbox artifact emission, focused `inspect-apk-process` operator output, CLI self-heal recovery of malformed process-manager artifacts, and preservation of the existing P2 through P10 behavior.
- Update repo docs and status output to make **P11 Minimal ActivityManager/ProcessManager Contract** the current direct-runtime slice and point the next handoff at **P12 Runtime Process Handoff + Resume Contract**.

## v0.1.103 - 2026-05-18

- Implement **P10 Android Permissions + AppOps Contract** on the direct `launch-apk` path through `--permissions-proof`, extending the earlier proof slice into a real sandbox-backed contract that persists deterministic permission and AppOps state under `sandbox/data/data/<package>/permissions/permission-state.json` and `sandbox/data/data/<package>/permissions/app-ops.json`.
- Add a minimal Linuxoid permission registry and AppOps policy layer for direct APK sessions, including deterministic `requested_permissions`, `granted_permissions`, `denied_permissions`, `schema_version`, `user_id`, `app_id`, `sandbox_root`, `contract_ready`, `updated_at_unix_ms`, `healing_actions`, `diagnostics`, and stable AppOps records for `storage_private_access`, `storage_sensitive_access`, `audio_capture_access`, `network_placeholder_access`, `binder_service_access`, `activity_launch_access`, and `dex_runtime_access`.
- Validate and self-heal missing, malformed, incomplete, stale, or incompatible permission/AppOps files across repeated launches, while staying honest about dangerous permissions: Linuxoid now rebuilds bad contract files deterministically but still does not silently grant unsupported or dangerous permissions like `android.permission.RECORD_AUDIO`.
- Extend the Self-Healing Android Device watchdog so it now consumes `permission_health` plus `app_ops_health`, records deterministic `rebuild_permission_state` recovery attempts in `self-healing-android-device/recovery-journal.jsonl`, and reports permission/AppOps health directly inside both the direct launch JSON and the watchdog artifacts.
- Add regression coverage for default permission extraction, persistence round-trip stability, AppOps mode decisions, missing-file healing, malformed JSON recovery, denied-audio diagnostics, and preservation of the existing P2 through P9 behavior.
- Add `compatctl inspect-apk-permissions <apk-path> [staging-root]` as a focused developer/operator surface for the durable P10 contract, and extend both unit and CLI regression coverage to prove deterministic healing for incomplete, incompatible, and stale sandbox-backed permission/AppOps files without silently granting dangerous permissions.
- Update repo docs and status output to make the P10 -> **P11 Minimal ActivityManager/ProcessManager Contract** handoff explicit: P11 will build on the existing sandbox-backed package/activity/storage/permission/AppOps contracts and the current Self-Healing Android Device health gates rather than replacing them.
- Next phase: **P11 Minimal ActivityManager/ProcessManager Contract**
- GitHub update blocked: direct GitHub authentication not available from this environment

- Implement **P9 Android App Storage + Sandbox Contract** on the direct `launch-apk` path through `--storage-proof`, adding a session-bound storage model that materializes deterministic `sandbox/data/data/<package>`-style app directories, `files` plus `cache` plus native-lib plus asset/resource roots, placeholder uid/gid plus permission metadata, `isolation_level: path_sandbox_only`, safe app-relative path resolution, marker write/read proof, and nested `storage` JSON with `storage_health` plus `sandbox_health`.
- Extend the Self-Healing Android Device watchdog so it now consumes storage/sandbox health, records deterministic `repair_app_storage` recovery attempts in `self-healing-android-device/recovery-journal.jsonl`, and reports storage/sandbox health directly inside `self_healing_android_device` and watchdog-report artifacts.
- Add regression coverage for deterministic storage directory materialization, safe path acceptance and traversal/absolute/symlink escape rejection, storage marker checksum proof, and a simulated storage failure that the watchdog repairs through `repair_app_storage`, while preserving the existing P2 through P8 behavior.
- GitHub update blocked: direct GitHub authentication not available from this environment

## v0.1.102 - 2026-05-18

- Implement **P8 Self-Healing Android Device Watchdog + Recovery Loop** on the direct `launch-apk` path through `--self-heal-proof [--package <package>] [--component <component>]`, adding a session-bound watchdog that consumes existing launch, surface, asset/resource, lifecycle, looper, input, Binder, DEX/bootstrap, package-manager, intent-resolution, and activity-launch health fields and emits nested `self_healing_android_device` JSON.
- Add deterministic local recovery actions for the direct APK session path, including `no_action`, `restage_assets`, `rebuild_resource_metadata`, `restart_surface`, `reset_input_queue`, `restart_lifecycle`, `refresh_binder_services`, `rebuild_dex_bootstrap`, `rerun_intent_resolution`, and `safe_mode_launch`, with explicit attempted/succeeded/failed reporting instead of silent recovery claims.
- Persist watchdog artifacts under the staged APK session root as `self-healing-android-device/watchdog-report.json` and `self-healing-android-device/recovery-journal.jsonl`, and add regression coverage for healthy sessions plus deterministic simulated failures in the asset bridge, surface proof, Binder/service registry, DEX/bootstrap, and intent/activity resolution seams while preserving the existing P2 through P7 behavior.
- GitHub update blocked: GitHub CLI not authenticated.

## v0.1.101 - 2026-05-18

- Extend the direct `launch-apk` slice with `--activity-proof [--package <package>] [--component <component>]`, adding a session-bound Linuxoid `package_manager`, `intent_resolution`, and `activity_launch` contract that writes deterministic `activity-launch/package-record.json`, `intent-resolution.json`, and `activity-launch.json` artifacts under the staged APK session root.
- Expand that package-manager contract so it now records labels, exported/enabled flags, and manifest intent filters for staged activity components, resolves either `ACTION_MAIN` plus `CATEGORY_LAUNCHER` or explicit component launches, and emits deterministic blocked reasons like `package_not_found`, `no_launcher_activity`, `ambiguous_launcher_activities`, `component_disabled`, `component_not_exported`, `unsupported_component_type`, and `unresolved_activity_class`.
- Tie that activity-launch contract back to existing lifecycle, surface, input, Binder/service-registry, and DEX/bootstrap readiness, exposing explicit `binder_health` plus `activity_health` fields while still reporting `art_runtime_available: false` and `java_execution_supported: false` honestly instead of claiming full Android framework startup.
- Add regression coverage for successful activity-proof launch, session-bound artifact materialization, package-registry metadata, missing launcher, ambiguous launcher, explicit component resolution, package-not-found, unsupported explicit component, deterministic repeated JSON output, truthful blocked behavior when DEX/bootstrap readiness is missing, and preservation of the existing P2 through P6 direct-APK proof surfaces.

## v0.1.100 - 2026-05-18

- Extend the direct `launch-apk` slice with `--dex-proof`, adding a session-bound Linuxoid DEX bridge that safely stages `classes.dex`, `classes2.dex`, and similar entries, parses DEX header-and-count metadata, records deterministic checksums plus staged paths, and emits nested `dex` plus `art_bootstrap` JSON with `class_loader_ready` while still keeping `java_execution_supported: false`.
- Add regression coverage for successful DEX proof on a valid native-only fixture APK, deterministic multi-DEX discovery, honest blocked behavior when DEX is missing but native launch still succeeded, and structured failure when a malformed DEX payload is required.
- Refresh the README, phased plan, self-healing runtime note, status text, changelog, and step log so the repo says plainly that native-only APK launch plus surface proof plus asset/resource bridge plus lifecycle/input foundation plus Binder/service registry plus DEX/bootstrap probe now exist, while full Java/Kotlin ART execution is still future work.

## v0.1.99 - 2026-05-17

- Extend `native-service-manager-fixture <bootstrap-manifest>` into a P5 Binder/service-registry foundation for the Self-Healing Android Device path, adding deterministic `package_manager`, `activity_manager`, and app-local placeholder service registration, stable owner-session plus owner-process metadata, honest missing-service lookup reporting, and explicit limitation flags like `local_foundation_only`, `real_android_binder: false`, and `system_server_present: false`.
- Persist richer Binder/service artifacts under the staged lifecycle session root, including `binder/service-manager.json`, `binder/registered-services.json`, `binder/service-lookups.json`, `binder/service-lookups.jsonl`, `binder/service-transactions.jsonl`, and `binder/transport-messages.jsonl`, while threading the lookup summary path and richer Binder evidence into the lifecycle shim and runtime-health reporting.
- Add regression coverage for deterministic Binder fixture artifacts, CLI output, missing-service lookup truth, PackageManager/ActivityManager-shaped transaction metadata, lifecycle-session wiring of the new lookup summary artifact, and preservation of the existing P4 lifecycle/Looper/InputQueue bridge outputs.

## v0.1.98 - 2026-05-17

- Extend the native-only `launch-apk` slice with `--lifecycle-proof`, binding the staged APK session to a deterministic Android-like lifecycle controller, Looper-style event pump, and InputQueue foundation that emit nested `lifecycle`, `looper`, and `input_queue` JSON with explicit `lifecycle_health`, `looper_health`, and `input_health` fields for future Self-Healing Android Device work.
- Add regression coverage for successful lifecycle proof, session-bound lifecycle metadata, deterministic lifecycle ordering, deterministic looper dispatch counts, malformed input rejection, and structured missing-native-library failure when lifecycle proof is requested.
- Refresh the README, phased plan, self-healing runtime note, status text, changelog, and step log so the repo says plainly that native-only APK launch plus surface proof plus asset/resource bridge plus lifecycle/input foundation now exist, while full Android framework services, Binder service ecosystems, Java/Kotlin ART execution, and production app compatibility remain future work.

## v0.1.97 - 2026-05-17

- Extend the native-only `launch-apk` slice with `--asset-proof`, binding the staged APK session to a deterministic Linuxoid AssetManager/Resources-shaped bridge that lists normalized assets, opens and checksums a staged asset, reports `resources.arsc` plus `res/` metadata, and surfaces explicit `asset_health` plus `resource_health` fields for future Self-Healing Android Device work.
- Add regression coverage for successful asset-proof launch, session-bound asset bridge metadata, deterministic asset listing, deterministic asset checksum and size reporting, unsafe path rejection, missing asset reporting, and structured nonzero behavior when launch fails even though staged asset/resource proof is still available.
- Refresh the README, phased plan, self-healing runtime note, status text, and step log so the repo says plainly that native-only APK launch plus surface proof plus asset/resource metadata bridge now exist, while full Android Resources decoding, theme/layout inflation, Java/Kotlin ART execution, and production Android graphics compatibility remain future work.

## v0.1.96 - 2026-05-17

- Extend the native-only `launch-apk` slice with `--surface-proof` and `launch-apk-surface`, binding the APK launch session to a deterministic native surface/session object with lifecycle states, first-frame proof, first-pixel marker artifacts, and explicit `backend: headless` reporting when Linuxoid is not using a real Wayland/EGL path.
- Add regression coverage for successful APK-launched surface proof, package/session binding, deterministic first-pixel marker stability, and structured failure behavior when surface proof is requested on a missing-native-library path.
- Refresh the README, phased build plan, self-healing runtime note, step log, and status text so the repo says plainly that native-only APK launch plus surface proof now exists, while full Java/Kotlin ART execution and production Android graphics compatibility remain future work.

## v0.1.95 - 2026-05-17

- Add `compatctl launch-apk <apk-path> [staging-root]`, Linuxoid's first end-to-end native-only APK launch slice. It opens a stored ZIP APK, reads plain-XML manifest metadata, stages ABI-matching native libraries plus assets into a deterministic session directory, loads the selected library, calls `JNI_OnLoad` when present, creates a minimal native activity/session object, and emits structured JSON for both success and structured failure.
- Add regression coverage for the new launch slice, including valid native-only fixture success plus structured failures for path traversal, missing native libraries, invalid APK input, missing manifest metadata, and unsupported ABI.
- Refresh the README, phased build plan, self-healing runtime note, step log, and status text so the repo says this new slice is real and useful, but still narrower than full Java/Kotlin ART-backed Android app execution.

## v0.1.94 - 2026-05-17

- Refresh the README, self-healing runtime note, and phased build plan so they say more plainly that Linuxoid’s strongest current guarantee is a stable diagnosis-and-replay contract across `preflight-runtime native`, `verify-package native`, `verify-package-matrix native`, and `launch-package native`, including blocked verification and matrix outcomes that are still correct self-healing passes.
- Clarify that repeated blocked `verify-package native` runs now keep the rendered report plus generated health/replay JSON stable, repeated blocked `verify-package-matrix native` runs keep row-level diagnosis stable, and that none of those guarantees mean Linuxoid has crossed the last real milestone of a default-path, no-override, host-ART-owned staged app startup.

## v0.1.93 - 2026-05-17

- Add blocked-host-ART CLI regressions for `verify-package native` and `verify-package-matrix native` so Linuxoid now proves four bridge-level guarantees at those public verification surfaces too: deterministic health classification, deterministic recovery selection, stable repeated command or JSON output, and no false success when ART/bootstrap is still missing.
- Pin the blocked verification contract through both the rendered operator reports and the generated `runtime-health.json` plus merged replay JSON, keeping the public native verification surfaces aligned with the deeper self-healing engine.

## v0.1.92 - 2026-05-17

- Extend `verify-package native` and `verify-package-matrix native` so they now surface the full replayable JSONL bundle directly: recovery-actions trace path, health replay path, diagnostic replay path, diagnostic fixture command, diagnostic trace-index path, canonical/found/missing trace-source counts, and per-source trace details now render at the bridge layer instead of only inside deeper preflight or health artifacts.
- Add regression coverage that pins those replay fields on both override-backed success and blocked host-ART native verification or matrix paths, keeping the public replay contract aligned with Linuxoid’s underlying self-healing fixtures.

## v0.1.91 - 2026-05-17

- Extend `verify-package native` and `verify-package-matrix native` so they now render the full canonical four-scenario recovery mapping with rank, retry budget, scope, and reason metadata, instead of only surfacing the currently selected blocked-path recovery actions.
- Add regression coverage that pins those deterministic canonical recovery details on both override-backed success and blocked host-ART native verification or matrix paths, keeping the bridge-level recovery contract aligned with the deeper self-healing runtime plan.

## v0.1.90 - 2026-05-17

- Extend the public native self-healing bridge so `preflight-runtime native`, `verify-package native`, `verify-package-matrix native`, and `launch-package native` now render the six required core runtime-health records directly through `Runtime Core Subsystems*` fields instead of leaving that projection only inside deeper `runtime-health.json`.
- Add regression coverage that pins the new core-subsystem projection across blocked native preflight, blocked native launch, and native matrix reporting, while keeping the existing bridge-level health, recovery, and replay contract green.

## v0.1.89 - 2026-05-17

- Extend `verify-package-matrix native` so each staged package row now carries the same bridge-level self-healing truth as native preflight, verification, and launch: runtime health classification, overall-ready verdict, dependency-blocked state, recovery details, and replay handles now render directly in the matrix output instead of staying trapped inside nested per-package reports.
- Add regression coverage for both override-backed native matrix success and blocked host-ART native matrix failure so the matrix surface stays aligned with the stronger self-healing contract already enforced on the single-package native bridge.

## v0.1.88 - 2026-05-17

- Extend `verify-package native` so it surfaces the same bridge-level self-healing truth as native preflight and launch: runtime health classification, overall-ready verdict, dependency-blocked state, recovery details, replay readiness, replay commands, and trace-bundle completeness now render directly in the verification report instead of being buried only inside `Preflight Output:`.
- Add regression coverage for both override-backed success and blocked host-ART verification so the native verification surface stays aligned with the preflight and launch self-healing contract.

## v0.1.87 - 2026-05-17

- Refresh the README, self-healing runtime note, and phased build plan so they state more plainly what self-healing means today at the public native bridge: truthful diagnosis, deterministic bounded recovery, stable replay, and no false claim of app execution success.
- Clarify again that the remaining milestone is still the first default-path, no-override, host-ART-owned staged app startup, plus the supporting Binder, graphics, input, and Android resource gaps around it.

## v0.1.86 - 2026-05-17

- Extend the public native self-healing bridge so blocked `preflight-runtime native` and `launch-package native` now render `Runtime Health Classification` plus `Runtime Overall Ready` directly, instead of forcing callers to infer those verdicts only from deeper JSON artifacts.
- Add CLI-level regressions that pin health classification, recovery decision selection, repeated runtime-health JSON stability, and no false success on the blocked host-ART path for both native preflight and native launch.

## v0.1.85 - 2026-05-17

- Extend the public native replay contract so `preflight-runtime native` and `launch-package native` now render exact replay commands for `native-runtime-health-replay`, `native-runtime-diagnostic-replay`, and `native-runtime-diagnostic-fixture` alongside the existing JSONL artifact paths and trace-source summaries.
- Add regression coverage that pins those replay-command lines on blocked native preflight and blocked native launch, keeping the public bridge aligned with Linuxoid’s replayable self-healing fixtures.

## v0.1.84 - 2026-05-17

- Extend the public native self-healing bridge so deterministic recovery details now carry operator-facing `action_reason` text alongside the existing `action_rank`, `retry_budget`, and `recovery_scope` metadata.
- Pin that richer recovery-detail contract with blocked-host-ART regressions for both `preflight-runtime native` and `launch-package native`, keeping the bridge output aligned with the deeper `runtime-health` recovery plan.

## v0.1.83 - 2026-05-17

- Extend the host ART probe contract to cover the common 64-bit binary names too: Linuxoid now recognizes `dalvikvm64` as the same bootstrap-capable host class as `dalvikvm`, and `app_process64` as the same detection-only host class as `app_process`.
- Add regression coverage that proves the widened host probe contract across runtime smoke, native preflight, and native launch, including a no-override host-`dalvikvm64` launch-success path in the test harness.
- Refresh the README, self-healing runtime note, phased plan, changelog, step log, and status text so the repo stays honest about broader host-ART name coverage without claiming real ART on this machine.

## v0.1.82 - 2026-05-17

- Refresh the README, self-healing runtime note, and phased build plan so they explain the current self-healing contract more plainly at the public native bridge: Linuxoid can now keep blocked preflight and launch answers deterministic, replayable, and honest without implying a successful Android app start.
- Clarify again that a successful self-healing pass today may still end in `Ready For Launch: no` or `Launch OK: no`, and that the remaining milestone is still the first default-path, no-override, host-ART-owned staged app startup.

## v0.1.81 - 2026-05-17

- Add consolidated native-bridge self-healing regressions that pin four operator-facing guarantees together on the blocked host-ART path: deterministic health classification, deterministic recovery-action selection, stable repeated runtime-health JSON, and no false success when a required runtime dependency is missing.
- Prove the same contract at both public bridge surfaces Linuxoid operators actually read today: `preflight-runtime native` and `launch-package native`.

## v0.1.80 - 2026-05-17

- Surface replayable trace-source details directly through the native bridge: `preflight-runtime native` and `launch-package native` now render per-source trace summaries with source name, trace path, event count, first and last event types, and deterministic fingerprints instead of stopping at bundle-level counts.
- Add regression coverage that pins those bridge-level trace details on blocked native preflight and blocked native launch paths, keeping the public replay contract aligned with the deeper diagnostic replay artifacts.

## v0.1.79 - 2026-05-17

- Surface deterministic recovery metadata more directly through the native bridge: `preflight-runtime native` and `launch-package native` now render detailed selected-recovery and canonical-scenario lines with stable action rank, retry budget, and recovery scope instead of only compressed `scenario=>action` summaries.
- Add regression coverage that pins those richer bridge-level recovery details on blocked native preflight and blocked native launch paths, keeping the public contract aligned with the structured `runtime-health` and `runtime-recovery-plan` JSON artifacts.

## v0.1.78 - 2026-05-17

- Distinguish host ART **detection** from host ART **bootstrap capability** across the native runtime seams: Linuxoid now classifies selected probes as `override_bootstrap_capable`, `host_dalvikvm_bootstrap_capable`, `host_app_process_detection_only`, or `missing` instead of treating every detected host probe as equally launch-ready.
- Thread that capability through `native-art-runtime-smoke`, activity-bootstrap, bootstrap-execution, `preflight-runtime native`, and `launch-package native`, so the public reports can explain why a detected host `app_process` surface still leaves the staged native launch path blocked.
- Add regression coverage for override-backed capability success plus honest host-`app_process` detection-only reporting in runtime smoke, native preflight, and native launch, then refresh the README, phased plan, self-healing runtime note, status text, and step log to keep the repo honest about the narrower but more truthful host ART gate.

## v0.1.77 - 2026-05-17

- Record the host ART gate as a first-class diagnostic contract: `native-art-classloader-fixture` and `native-art-runtime-smoke` now materialize `art/art-runtime-probe-inventory.json` and a deterministic probe-detection reason that explain which override, fixed-path, and PATH-based probe candidates were examined and why one was selected or none was usable.
- Thread that evidence through the public native bridge so `preflight-runtime native`, `verify-package native`, and `launch-package native` now expose `ART Runtime Probe Inventory Path` plus `ART Runtime Probe Detection Reason` alongside the existing health, recovery, and replay artifacts.
- Add regression coverage that pins the new probe-inventory contract across override-backed runtime-smoke output, host-ART-missing native preflight output, and host-ART-missing native launch output, then refresh the README, phased plan, self-healing runtime note, status text, and step log to keep the repo honest about this deeper default-path host ART diagnosis seam.

## v0.1.76 - 2026-05-18

- Refresh the self-healing docs so they say more plainly what a successful self-healing pass means today: truthful diagnosis, bounded next-step selection, replayable artifacts, and no fake launch claims while execution is still blocked.
- Clarify again that full Android app execution is still blocked on the first default-path, no-override, host-ART-owned staged app startup, plus richer Binder/framework behavior, bound Wayland+EGL rendering, fuller IME, and fuller Android resource semantics.

## v0.1.75 - 2026-05-18

- Strengthen the self-healing CLI contract regression again by adding one end-to-end matrix that proves health classification, deterministic recovery selection, stable repeated JSON output, and no false success when a dependency is missing all the way through the diagnostic replay surface.

## v0.1.74 - 2026-05-18

- Extend the public native bridge so `preflight-runtime native`, `verify-package native`, and `launch-package native` now surface the replayable trace bundle directly: health trace JSONL, recovery-actions JSONL, health replay JSON, merged diagnostic events JSONL, trace index JSON, and replay completeness counts are all emitted at the bridge layer instead of only inside deeper health artifacts.
- Add regression coverage that pins those trace and replay fields across ready, override-backed, and host-ART-missing native preflight or launch paths, plus a rendering test for the installed-app launch report and a verification-surface check that the native preflight output exposes the replay bundle cleanly.

## v0.1.73 - 2026-05-18

- Surface deterministic recovery policy directly in the public native bridge: `preflight-runtime native`, `verify-package native`, and `launch-package native` now expose both the currently selected recovery actions and the canonical four-scenario recovery mapping instead of hiding that detail in the deeper health JSON only.
- Add regression coverage that pins those recovery-action summaries on ready, override-backed, and host-ART-missing native paths, so the bridge keeps distinguishing “what Linuxoid can recover in principle” from “what Linuxoid needs to do next on this run.”

## v0.1.72 - 2026-05-18

- Extend the `native` runtime preflight contract so it now carries Linuxoid-owned runtime-probe readiness, bootstrap-planning readiness, blocked-subsystem summaries, bootstrap manifest paths, and health or replay artifact paths instead of stopping at staged metadata visibility.
- Make `preflight-runtime native` and `verify-package native` honest about actual launch-attempt readiness: they now stay blocked when host ART is missing, when the runtime probe only resolves through the override seam without explicit opt-in, or when the staged package cannot materialize a native spike candidate path.
- Add a deterministic host-ART-disable test seam for ART probe discovery and regression coverage for override-backed native preflight success, host-ART-missing native preflight failure, and non-candidate verification failure at the preflight boundary.

## v0.1.71 - 2026-05-18

- Extend the public `launch-package native` report so it now surfaces Linuxoid-owned bootstrap, runtime-health, recovery-plan, and replay-bundle artifact paths directly, plus blocked-subsystem summaries when a staged launch fails.
- Fix the replay-bundle contract on healthy native launches by treating an existing but empty `runtime-recovery-actions.jsonl` as a valid trace source instead of a missing artifact, so successful paths can still report `replay_ready: true` and `trace_bundle_complete: true` without inventing fake recovery actions.
- Add regression coverage for successful override-backed native launch, override rejection, and honest host-ART-missing failure so the native launch report stays pinned to that richer artifact and diagnostic contract.

## v0.1.70 - 2026-05-18

- Refresh the self-healing documentation so the repo explains the current public command contract more plainly: Linuxoid can diagnose, classify, choose bounded recovery, and replay failures, but it still cannot honestly claim full Android app execution on Linux.
- Clarify in the README, self-healing runtime note, and phased plan that the strongest successful path today is still a Linuxoid-owned fixture seam, while the remaining blocker is a real staged foreground app crossing the ART and bootstrap seams on the default host-side path.

## v0.1.69 - 2026-05-17

- Add a consolidated CLI-level self-healing contract regression that proves, in one end-to-end matrix, deterministic health classification, deterministic recovery decision selection, stable repeated JSON output, and no false success when a dependency is missing.
- Keep the verification focused on the public command surface Linuxoid harnesses consume: `native-runtime-health-fixture` and `native-runtime-recovery-plan`.

## v0.1.68 - 2026-05-17

- Strengthen the replayable-diagnostics contract by exposing the seven-source JSONL trace bundle through explicit summary fields: `health_trace_jsonl_path`, `health_replay_json_path`, `canonical_trace_source_count`, `canonical_trace_source_names`, `missing_trace_source_count`, and `trace_bundle_complete`.
- Add regression coverage that proves both the in-process diagnostic replay report and the public `native-runtime-diagnostic-replay` / `native-runtime-diagnostic-fixture` JSON expose that trace-bundle contract deterministically, including honest incomplete-state reporting when one trace source is missing.
- Refresh the README, self-healing runtime note, phased plan, changelog, and step log so the repo describes the stronger offline replay surface honestly.

## v0.1.67 - 2026-05-17

- Strengthen the deterministic recovery-plan contract by exposing the four required recovery cases as an explicit scenario matrix: `missing_artifact`, `failed_native_load`, `unavailable_display`, and `failed_service_lookup` now surface through `canonical_recovery_scenarios` with stable action names, ranks, retry budgets, scopes, and reasons.
- Add regression coverage that proves both the in-process recovery report and the public `native-runtime-recovery-plan` JSON expose that four-scenario contract deterministically, instead of forcing harnesses to infer it from whichever recovery actions were selected in one run.
- Refresh the README, self-healing runtime note, phased plan, changelog, and step log so the repo describes the stronger public recovery contract honestly.

## v0.1.66 - 2026-05-17

- Strengthen the self-healing runtime-health contract by exposing the six required core subsystems through an explicit summary projection: `core_subsystems`, `core_subsystem_count`, `core_ready_subsystem_count`, `core_subsystems_ready`, and `core_subsystem_records`, while keeping the full ordered `records` list intact for deeper diagnosis.
- Add regression coverage that proves both the in-process report and the public `native-runtime-health-fixture` JSON expose that core six-subsystem contract deterministically for APK staging, native loading, surface readiness, input queue readiness, Binder/service readiness, and DEX/classloader readiness.
- Refresh the README, self-healing runtime note, phased plan, changelog, and step log so the repo describes the stronger core-runtime-health surface honestly.

## v0.1.65 - 2026-05-17

- Teach the native launch bridge to distinguish Linuxoid fixture override success from genuine host ART launch success: override-backed bootstrap execution still produces stable artifacts and can still be used in tests, but `launch-package native` now rejects that path by default unless `LINUXOID_NATIVE_ALLOW_RUNTIME_OVERRIDE=1` is set explicitly.
- Add runtime-probe provenance fields to the ART runtime-smoke, activity-bootstrap, and bootstrap-execution reports so harnesses can tell whether a successful probe came from a host runtime or the Linuxoid-owned override seam.
- Add regression coverage for both sides of that boundary: default rejection of override-backed native launch success, explicit opt-in success when the override is allowed, and continued preflight-backed native verification through the shared installed-package contract.

## v0.1.64 - 2026-05-17

- Upgrade the backend-neutral installed-package verification seam so `verify-package` and `verify-package-matrix` now run runtime preflight first, report runtime-target selection plus package visibility plus component readiness explicitly, and avoid treating the local `native` backend as a launch-only special case.
- Add regression coverage for attached-ADB, Waydroid, and local `native` verification paths, including override-backed native success and honest non-candidate native failure through the same verification contract.
- Refresh the README, phased plan, changelog, and step log so the repo explains that backend-neutral verification now reuses the staged native preflight contract before launch.

## v0.1.63 - 2026-05-17

- Refresh the README, self-healing runtime note, and phased plan so the repo explains more directly what self-healing means now at the public command surface, what the Linuxoid-owned local `native` runtime bridge contributes, and what still remains before real host-side Android app execution on Linux.

## v0.1.62 - 2026-05-17

- Add a single public-command contract regression for the missing-native-dependency path, proving `native-runtime-health-fixture` keeps health classification, recovery decision selection, JSON stability, and no-false-success behavior aligned in one deterministic CLI output surface.

## v0.1.61 - 2026-05-17

- Tighten the replayable-diagnostics contract by adding a CLI-level regression that proves `native-runtime-diagnostic-fixture` materializes the full seven-source JSONL trace bundle and that `native-runtime-health-replay` can diagnose the resulting runtime state later from those artifacts alone, without rerunning the full UI path.

## v0.1.60 - 2026-05-17

- Strengthen the deterministic recovery contract at the public CLI surface by adding a scenario-matrix regression for `native-runtime-recovery-plan`, proving the four requested recovery cases stay mapped to the same action names and action ranks: missing artifact, failed native load, unavailable display, and failed service lookup.

## v0.1.59 - 2026-05-17

- Lock the Self-Healing Android Device runtime health contract more explicitly by adding a regression that proves the six core subsystem records appear in deterministic order with stable artifact paths and JSON output: `apk_staging`, `native_loading`, `surface_readiness`, `input_queue_readiness`, `binder_service_readiness`, and `dex_classloader_readiness`.

## v0.1.58 - 2026-05-17

- Replace the `native` runtime bridge hard stub with a Linuxoid-owned local staged-package path: `discover-runtime native` now reports a deterministic `linuxoid-native` target, `inspect-package native` reads staged manifest metadata from the compat root, `preflight-runtime native` resolves staged launcher readiness, and `launch-package native` reuses the bootstrap-execution seam for candidate packages.
- Add regression coverage for native runtime discovery, staged metadata lookup, staged-package preflight, override-backed bootstrap-execution success, and honest failure reporting for non-candidate packages.
- Refresh the README, phased plan, stub audit, changelog, and status text so Linuxoid now reports the native runtime bridge as a partial local implementation instead of a pure `not implemented` stub while still distinguishing that bridge from full Android app execution.

## v0.1.57 - 2026-05-17

- Teach the self-healing runtime to classify dex-only staged bundles more honestly: `native_loading` now resolves to `not_required` when an APK declares no native libraries at all, while unsupported-ABI or missing-required-lib cases still stay blocked and select deterministic native-loader recovery.
- Add native-library summary fields to the native spike plan and bootstrap manifest, then keep runtime-health parsing backward-compatible with older already-staged manifests that do not carry those fields yet.
- Add regression coverage that strips those new fields from a generated bootstrap manifest and proves the real `native-runtime-health-fixture` command still succeeds with stable `native_loading: not_required` output for dex-only bundles.
- Refresh the README, self-healing runtime note, changelog, status text, and step log so Linuxoid now distinguishes “no native libs required” from “native loading failed” without overstating native execution progress.

## v0.1.56 - 2026-05-17

- Add a Linuxoid-owned `LINUXOID_ART_RUNTIME_PROBE_OVERRIDE` seam so the ART classloader detector, runtime-smoke path, and supervised bootstrap-execution runner can all exercise a deterministic host-style runtime probe during fixtures even on machines that do not ship ART locally.
- Add regression coverage that proves the override-backed runtime-smoke path reaches real probe execution and that bootstrap execution can now run end to end through the supervised runner with stable application/activity logs and success classification.
- Teach runtime health to recognize that deeper success path too, so `dex_classloader_readiness` and `bootstrap_execution_readiness` can now resolve to `ready` when override-backed or real runtime class resolution plus supervised bootstrap execution succeed.
- Add regression coverage that proves override-backed runtime health can converge all the way to `overall_ready: true` instead of getting stuck in a generic pending state.
- Refresh the README, phased plan, self-healing runtime note, changelog, and status text so Linuxoid now describes the override-backed execution seam honestly while still distinguishing it from real host ART app execution.

## v0.1.55 - 2026-05-17

- Upgrade `native-art-bootstrap-execution-fixture` into a real supervised runner seam: Linuxoid now executes bootstrap phases through the generated runner script, writes `art/bootstrap-execution-runner-state.json`, and records raw runner, application, and activity exit codes alongside the existing plan, context, trace, result, and phase-log artifacts.
- Add regression coverage that proves the supervised runner path is used, that the runner-state artifact is stable, and that bootstrap-execution command JSON now exposes the new runner metadata.
- Refresh the README, phased plan, self-healing runtime note, changelog, and status text so Linuxoid now describes bootstrap execution as a supervised runner-backed seam while still reporting missing host ART honestly on this machine.

## v0.1.54 - 2026-05-17

- Split Linuxoid's post-class-resolution bootstrap seam cleanly into **activity-bootstrap planning** and **bootstrap execution** so `native-art-activity-bootstrap-fixture` now stays a deterministic planning artifact generator while `native-art-bootstrap-execution-fixture` owns the execution-context JSON, runner script, and per-phase execution logs.
- Add a first-class `bootstrap_execution_readiness` runtime-health subsystem with deterministic `attempt_host_bootstrap_execution` recovery output, while making `activity_bootstrap_readiness` accurately represent the readiness of the planning seam instead of the still-blocked execution seam.
- Add regression coverage that proves runtime health, recovery summaries, replay output, and JSON stability all reflect the new planning-versus-execution split honestly, then refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status text so Linuxoid reports the deeper self-healing execution contract truthfully.

## v0.1.53 - 2026-05-18

- Add a Linuxoid-owned `native-art-bootstrap-execution-fixture` seam so the post-class-resolution bootstrap path now writes deterministic `art/bootstrap-execution-plan.json`, `art/bootstrap-execution-trace.jsonl`, and `art/bootstrap-execution-result.json` artifacts instead of leaving host-side bootstrap execution implied by lower-level activity probes.
- Eliminate the live staged-bundle timeout in `native-runtime-health-fixture` by reusing one opened APK archive per command, preferring the already-staged bundle manifest before any decode fallback, and threading the new bootstrap-execution seam into runtime health plus diagnostic replay.
- Add regression coverage for opened-archive reads, staged-manifest fallback, bootstrap-execution artifact reuse, and the stronger health/replay contract, then refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so Linuxoid reports the faster live native diagnosis path honestly.

## v0.1.52 - 2026-05-18

- Upgrade `native-art-activity-bootstrap-fixture` from an activity-only planning seam into a host-side application-plus-launcher bootstrap attempt seam: Linuxoid now normalizes a manifest application class when present, emits explicit application and launcher activity probe events, and records probe-attempt plus probe-success state separately without faking ART success on hosts that lack ART.
- Tighten bootstrap truthfulness so Linuxoid no longer infers an application class from arbitrary non-activity manifest targets when the APK does not actually declare one.
- Add regression coverage that proves the activity-bootstrap JSON now includes normalized application-class output and the trace now captures both application and launcher-activity bootstrap sequence events.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so the repo now reflects the stronger post-class-resolution application/activity bootstrap attempt gate honestly.

## v0.1.51 - 2026-05-18

- Thread the existing `native-art-activity-bootstrap-fixture` seam into `native-runtime-health-fixture` and diagnostic replay so Linuxoid now records `activity_bootstrap_readiness`, selects the deterministic recovery action `attempt_host_activity_bootstrap`, and merges `art/activity-bootstrap-trace.jsonl` into the replay bundle.
- Add regression coverage that proves activity bootstrap is now a first-class health subsystem and diagnostic trace source instead of a side artifact.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so the repo now reflects the stronger post-class-resolution health and replay contract honestly.

## v0.1.50 - 2026-05-18

- Add a Linuxoid-owned `native-art-activity-bootstrap-fixture` seam so launcher resolution, Binder-readiness evidence, and host-ART runtime-smoke artifacts now flow into deterministic `art/activity-bootstrap-plan.json`, `art/activity-bootstrap-trace.jsonl`, and `art/activity-bootstrap-result.json` outputs for the first post-class-resolution activity bootstrap step.
- Add regression coverage that proves the new activity-bootstrap seam writes stable artifacts, keeps launcher-derived activity targeting deterministic, and stays honest when host ART is absent.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the post-class-resolution activity-bootstrap planning gate honestly while still marking real host ART app execution as pending.

## v0.1.49 - 2026-05-18

- Add explicit self-healing summary fields to `runtime-health.json` and CLI output so Linuxoid now reports `dependency_blocked`, `failing_subsystem_count`, `recovery_actions_selected`, and a deterministic `failing_subsystems` list directly instead of forcing harnesses to derive them from raw records.
- Add regression coverage that proves health classification, recovery decision selection, repeated command JSON stability, and no-false-success behavior when a required dependency is missing.
- Refresh the README, phased plan, self-healing runtime note, changelog, and status text so the repo explains what self-healing means now and what still remains before full native Android app execution.

## v0.1.48 - 2026-05-18

- Add a deterministic diagnostic trace index for the Self-Healing Android Device runtime so replay bundles now carry per-source fingerprints plus first and last event-type boundaries in `runtime-diagnostic-trace-index.json`.
- Add `native-runtime-diagnostic-fixture <bootstrap-manifest> [scenario]`, a one-shot non-UI seam that materializes runtime-health traces first and then emits the replayable diagnostic bundle without requiring a full UI rerun.
- Add regression coverage that proves the trace-index artifact and fixture command stay stable, and refresh the README, phased plan, self-healing note, changelog, and status output so Linuxoid reports the stronger offline diagnosis contract honestly.

## v0.1.47 - 2026-05-17

- Formalize Linuxoid's deterministic runtime recovery contract so `native-runtime-recovery-plan` and `runtime-recovery-actions.jsonl` now emit explicit `action_rank`, `retry_budget`, and `recovery_scope` metadata for missing artifact, failed native load, unavailable display, failed service lookup, pending ART/classloader work, and surface-dependent input recovery.
- Add regression coverage that proves those metadata fields stay stable across fixture reports, materialized recovery-plan artifacts, and command JSON output instead of only exposing stable action names.
- Refresh the README, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports bounded deterministic recovery policy honestly while still marking full ART bootstrap, Android framework behavior, compositor-backed rendering, and full IME/text composition as pending.

## v0.1.46 - 2026-05-17

- Upgrade `native-art-runtime-smoke` from a generic ART availability probe into a real host-side class-resolution attempt seam: Linuxoid now selects a deterministic manifest-derived target class and prepares or runs `dalvikvm -cp <apk> <class>` when a safe local ART runtime exists.
- Add regression coverage that proves the runtime-smoke seam records a deterministic target class, attempts host-side class resolution only when safe, and keeps fallback behavior honest when ART is absent.
- Refresh the README, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the first real host-side ART class-resolution attempt honestly while still marking full Android app bootstrap and execution as pending.

## v0.1.45 - 2026-05-17

- Clarify in the README, phased plan, and self-healing runtime note what Linuxoid self-healing means today: deterministic diagnosis, bounded recovery planning, replayable trace artifacts, and refusal of false success.
- Document more explicitly what still blocks full native Android app execution: real host-side ART/class execution, richer Binder/framework behavior, bound Wayland+EGL rendering, full IME/text composition, and full Android resource-table semantics.

## v0.1.44 - 2026-05-17

- Add explicit regression coverage for self-healing runtime honesty when a native dependency is missing, including fixture-level health classification, command-level JSON stability, and merged diagnostic replay output.
- Prove through tests that Linuxoid does not claim false success when `native_loading` is blocked, and that the deterministic recovery action `retry_native_load_after_bundle_refresh` is surfaced consistently across health and replay outputs.

## v0.1.43 - 2026-05-17

- Add a Linuxoid-owned `runtime-smoke-trace.jsonl` artifact so the ART runtime smoke seam now emits structured JSONL events instead of only a plan, log, and result file.
- Add a `native-runtime-diagnostic-replay <bootstrap-manifest>` seam so Linuxoid can merge health, recovery, classloader, class-resolution, and runtime-smoke traces into one replayable diagnostic bundle without rerunning the UI path.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the merged replay-bundle gate honestly while still marking real ART/DEX execution, full Android Binder behavior, compositor-backed rendering, and IME/text composition as pending.

## v0.1.42 - 2026-05-17

- Add a Linuxoid-owned `native-runtime-recovery-plan` seam so the self-healing runtime can now materialize deterministic recovery-plan and action-trace artifacts instead of only selecting bounded recovery actions inside `runtime-health.json`.
- Extend runtime-health records so recovery actions now carry stable action IDs, subsystem-scoped artifact paths, and replay-trace links for missing artifact, failed native load, unavailable display, failed service lookup, and pending DEX/ART work.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the deterministic recovery-plan gate honestly while still marking real ART/DEX execution, full Android Binder behavior, compositor-backed rendering, and IME/text composition as pending.

## v0.1.41 - 2026-05-17

- Add a Linuxoid-owned `native-art-class-resolution-fixture` seam so staged APK dex entries can now resolve manifest-target descriptors offline, write deterministic resolution-map plus trace artifacts, and report unresolved classes honestly without pretending ART has already executed them.
- Add a Linuxoid-owned `native-art-runtime-smoke` seam so the staged ART/classloader plan can now produce deterministic invocation-plan, runtime-log, and result artifacts while probing local ART availability safely and honestly.
- Thread the new offline class-resolution and runtime-smoke artifacts into the self-healing runtime story so `dex_classloader_readiness` now carries evidence about classpath readiness, resolved versus missing manifest targets, runtime detection, and probe attempts instead of stopping at a classloader-plan placeholder.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the offline DEX resolution plus host-ART smoke gates honestly while still marking real host ART class execution, Android framework loading, full Binder behavior, compositor-backed rendering, and IME/text composition as pending.

## v0.1.40 - 2026-05-17

- Add a Linuxoid-owned `native-art-classloader-fixture` seam so staged APK dex entries can be inventoried, manifest application/activity targets can be normalized into deterministic descriptors, and stable `art/classloader-plan.json` plus trace artifacts can be written without depending on Waydroid, ADB, or an emulator.
- Thread the new classloader preparation artifact into the self-healing runtime story so `dex_classloader_readiness` now points at a concrete classpath plan instead of a bundle-level placeholder while still refusing false success when host ART is unavailable.
- Refresh the README Mermaid architecture, phased plan, self-healing runtime note, changelog, and status output so Linuxoid now reports the ART/classloader preparation gate honestly while still marking real ART execution, full Android Binder behavior, compositor-backed rendering, and full IME/text composition as pending.

## v0.1.39 - 2026-05-17

- Add a self-healing runtime observability skeleton so Linuxoid now records structured health for APK staging, native loading, surface readiness, input readiness, Binder/service readiness, and DEX/classloader readiness through `compatctl native-runtime-health-fixture`.
- Add deterministic recovery-action selection plus replayable `runtime-health-trace.jsonl` and `runtime-health-replay.json` artifacts so native-path failures can be diagnosed without rerunning the full UI flow.
- Refresh the README Mermaid architecture, phased plan, status output, and solution trail so the repo now reports the self-healing Android Device skeleton honestly while still marking ART/DEX execution, full Android Binder semantics, compositor-backed rendering, and full IME/text composition as pending.

## v0.1.38 - 2026-05-17

- Add a minimal APK/ZIP resource-readiness bridge so Linuxoid can inspect manifest metadata, list normalized assets, reject path traversal, and emit stable JSON through `compatctl inspect-apk-resources` without depending on Waydroid, ADB, or an emulator for the native direct-run path.
- Add a socketpair-backed local transport seam to the Binder-shaped service-manager fixture so Linuxoid now records deterministic lookup/transaction request-response round trips in `binder/transport-messages.jsonl`.
- Refresh the README Mermaid architecture, phased plan, changelog, and status output so Linuxoid now reports the pre-ART resource bridge and local Binder transport seam honestly while still marking ART/DEX execution, full Android resource-table semantics, real Android Binder behavior, and compositor-backed rendering as pending.

## v0.1.37 - 2026-05-17

- Add a minimal `native-service-manager-fixture` command and Binder-shaped local service-manager contract so Linuxoid now writes deterministic service registration, lookup, and transaction artifacts for `package_manager` and `activity_manager` without depending on Waydroid, ADB, or an emulator for the native direct-run path.
- Thread the new Binder-shaped artifact set into the native lifecycle shim so session state now carries service-manager metadata, lookup logs, and transaction logs instead of relying only on a flat placeholder registry.
- Refresh the Mermaid architecture, phased plan, verify examples, and status output so Linuxoid now reports the local Binder-shaped manager honestly while still marking real Binder transport, DEX/ART, bound Wayland-EGL rendering, IME/text composition, and richer resources as pending.

## v0.1.36 - 2026-05-17

- Add a minimal `native-input-queue-fixture` contract so Linuxoid now proves focused native-surface ownership, deterministic pointer/key injection, and stable metadata plus JSONL event artifacts without depending on Waydroid, ADB, or an emulator for the native direct-run path.
- Keep the new input seam honest by reporting probe-only versus headless-fallback backing separately from full IME/text composition, and keep the existing P1/P2 surface, Wayland, EGL, and callback proofs green.
- Refresh the Mermaid architecture, phased plan, verify examples, and status output so Linuxoid now reports the focused input contract as verified while still marking full IME/text composition, bound Wayland-EGL rendering, DEX/ART, Binder, and Android resource-table work as pending.

## v0.1.35 - 2026-05-17

- Add a minimal `ANativeWindow` bridge contract over the existing headless, Wayland, and EGL fixture seams so Linuxoid now exposes width, height, format, stride, deterministic buffer-geometry updates, and stable bridge artifacts without pretending full Android rendering is complete.
- Add the new `native-window-bridge-fixture` command plus tests that verify deterministic geometry updates, stable metadata/event paths, and honest headless-fallback versus probe-only reporting depending on Wayland/EGL backing availability.
- Refresh the Mermaid architecture, phased plan, verify examples, and status output so Linuxoid now reports the `ANativeWindow` bridge contract honestly while still marking Wayland-EGL binding, real Android drawing, DEX/ART, Binder, input, and full resources as pending.

## v0.1.34 - 2026-05-17

- Add an optional real EGL smoke fixture so Linuxoid can initialize an EGL display, choose a config, create an OpenGL ES context plus pbuffer surface, and write a deterministic `egl-metadata.json` artifact under the requested root.
- Add the new `native-egl-smoke-fixture` command plus tests that verify the JSON/artifact contract, deterministic metadata paths, and honest fallback behavior when EGL support is unavailable at build or runtime.
- Refresh the Mermaid architecture, phased plan, status output, verify examples, and dependency notes so Linuxoid now reports the real EGL context plus pbuffer proof honestly while still marking Wayland-EGL binding, `ANativeWindow` backing, DEX/ART, Binder, input, and full resources as pending.

## v0.1.33 - 2026-05-17

- Add an optional real `wayland-client` surface fixture so Linuxoid can connect to `wl_display`, bind `wl_compositor`, create a real `wl_surface`, and write a deterministic `surface-metadata.json` artifact under the requested root.
- Add the new `native-wayland-surface-fixture` command plus tests that verify the JSON/artifact contract, deterministic metadata paths, and honest fallback behavior when Wayland support is unavailable at build or runtime.
- Refresh the Mermaid architecture, phased plan, status output, and dependency notes so Linuxoid now reports the real Wayland client surface proof honestly while still marking EGL binding, `ANativeWindow` backing, DEX/ART, Binder, input, and full resources as pending.

## v0.1.32 - 2026-05-17

- Add a minimal `ANativeActivityCallbacks` contract and a `native-window-callback-fixture` command so Linuxoid can dispatch deterministic `window_created`, `window_changed`, and `window_destroyed` events against the existing headless native-window surface.
- Write a stable `native-window-callbacks.jsonl` artifact and structured callback JSON so the future direct native graphics path now has a Linuxoid-owned lifecycle journal instead of a placeholder callback story.
- Refresh the tests, Mermaid architecture, phased plan, and status output so Linuxoid now reports verified callback plumbing honestly while still marking real Wayland/EGL surfaces, compositor-backed callbacks, DEX/ART, Binder, input, and full resources as pending.

## v0.1.31 - 2026-05-16

- Add the first `P2.1` host-side `ANativeWindow`-shaped surface seam with explicit width, height, format, and stride metadata plus lifecycle checks for a future direct native runner.
- Add `native-first-pixel-fixture`, a headless Wayland/EGL-style test double that writes a deterministic `first-pixel-marker.txt` artifact and structured JSON without pretending a real compositor window exists yet.
- Refresh the tests, Mermaid architecture, phased plan, and status output so Linuxoid now reports the verified first-pixel marker slice honestly while still marking real Wayland/EGL, callbacks, DEX/ART, Binder, input, and full resources as pending.

## v0.1.30 - 2026-05-16

- Extend the native spike planner so Linuxoid now stages host-ABI native libraries into the deterministic bundle `lib` root, copies extracted `assets/` and `res/` content into the bundle resource tree, and reports unsupported ABI payloads honestly instead of pretending they can run.
- Add the first minimal APK-backed asset bridge so the stub asset manager can resolve and read staged assets by path from the native resource tree.
- Refresh the bootstrap/session metadata, tests, Mermaid architecture, phased plan, and status output so GitHub now reflects the staged-library and asset-read seam while still marking DEX/ART, Binder, graphics, input, and full Android resource handling as pending.

## v0.1.29 - 2026-05-16

- Keep `native-execute-stub` as the public P1 bootstrap entrypoint and harden it so the manifest path now forks and `execve`s a controlled Linuxoid child runner with deterministic cwd, Linuxoid-only environment variables, and inherited-fd cleanup.
- Load native libraries in deterministic order, call `JNI_OnLoad` when present, and emit structured JSON that records `execution_engine_ready`, `libraries_loaded`, `jni_onload_results`, `exit_reason`, and stable artifact paths for replay and harness diffing.
- Refresh the fixture native test library, bootstrap/session tests, README Mermaid architecture, phased plan, and status text so the repo now reflects the stricter P1 contract while still marking DEX/ART, Binder, graphics, input, and full resources as pending.

## v0.1.28 - 2026-05-16

- Add `native-process-bootstrap` so Linuxoid now turns a bootstrap manifest into a real parent/child native runner handoff with `pid`, `process_state`, `exit_code`, runner logs, and structured session state.
- Make `native-lifecycle-shim` truthful before launch by keeping activity state at `NOT_CREATED` instead of prewriting `RESUMED`, and persist richer process/report fields for later native slices.
- Switch generated native entrypoints to `native-process-bootstrap`, add fixture-backed tests for the new bootstrap path, and refresh the README Mermaid architecture plus roadmap to match the new execution flow.

## v0.1.27 - 2026-05-16

- Complete a 15-track Linuxoid research wave covering process bootstrap, DEX/ART, JNI, Binder/services, graphics/input, browser-control surfaces, and storage/sandboxing for the direct Android-on-Linux roadmap.
- Add `docs/15-track-research-wave-2026-05-16.md` as the integrated architecture note with per-track findings, repo touchpoints, immediate implementation moves, and a cross-track execution order.
- Refresh the README roadmap and reference links so GitHub now points at the completed research wave and reflects the current `execution 20/100` state honestly.

## v0.1.26 - 2026-05-16

- Add the first real `P1` native-runner slice with a dedicated `native_execute_stub.cpp`, minimal JNI/asset/looper/signal-handler surfaces, `dlopen`-based library discovery, `ANativeActivity_onCreate` resolution, and a five-second watchdog gate.
- Switch the generated native bootstrap entrypoint from `native-lifecycle-shim` to `native-execute-stub`, and add fixture-based tests that prove the runner reaches `ANativeActivity_onCreate` and exits `0`.
- Document the current local Calculator APK as a dex-only negative oracle, update the phased plan and README to reflect the fixture-backed `P1` proof, and record the new status split as `scaffold 95/100, execution 20/100`.

## v0.1.25 - 2026-05-16

- Add `docs/phased-build-plan.md` as the repo-facing source of truth for the direct-execution roadmap from `P0 Freeze & Triage` through `P6 Polish + Release`.
- Freeze the browser track behind `P5` in the README and browser architecture note so Linuxoid stops drifting into browser implementation before native execution, graphics, DEX, and Binder work land.
- Add `docs/p0-stub-audit.md` and update `compatctl status` plus `compatctl foundation` so the project now says `scaffold 95/100, execution 0/100` explicitly and maps the current native stub exits into an ordered `P1` queue.

## v0.1.24 - 2026-05-16

- Add a researched self-healing Android browser architecture direction for Linuxoid based on an Android `WebView` browser shell, bounded recovery policy, and MCP/harness-friendly artifacts.
- Update the README Mermaid architecture and browser roadmap so the repo now records the self-healing browser track as a first-class Linuxoid design constraint.
- Add `docs/browser-self-healing-architecture.md` with the recommended browser foundation, recovery taxonomy, machine-facing contract, and next implementation steps.

## v0.1.23 - 2026-05-16

- Add `native-lifecycle-shim` so Linuxoid can turn a native bootstrap manifest into deterministic session artifacts, activity-state tracking, and a first service registry for native candidates.
- Route the generated native bootstrap entrypoint through the lifecycle shim, so local Linux execution now proves a real lifecycle handoff instead of jumping straight from bootstrap artifacts to a stubbed stop.
- Refresh the README Mermaid architecture, MCP/harness-aligned roadmap, and status narrative so GitHub reflects the new lifecycle/service seam and the next process-bootstrap step.

## v0.1.22 - 2026-05-16

- Add MCP and harness compatibility as an explicit Linuxoid architecture rule in the README and Mermaid diagrams.
- Document that Linuxoid control paths, artifact layouts, and verification flows should remain machine-friendly for agent runtimes, MCP clients, and validation harnesses as native execution work continues.

## v0.1.21 - 2026-05-16

- Add `bootstrap-native-spike` so Linuxoid can turn a native spike candidate into a Linuxoid-owned bootstrap manifest, env script, entrypoint stub, and bootstrap report.
- Add `native-execute-stub` so the generated native bootstrap path can run locally on Linux and report its still-missing execution engine honestly instead of pretending app code already runs.
- Refresh the README Mermaid architecture, dependency notes, verification commands, and next-step roadmap so GitHub now shows the native bootstrap surface and the next path toward real direct Linux execution.

## v0.1.20 - 2026-05-16

- Add a README Mermaid maintenance rule so the current and target Linuxoid architecture graphs are updated in the same commit as meaningful architecture changes.
- Keep the GitHub documentation honest by making diagram upkeep an explicit project workflow requirement instead of an informal convention.

## v0.1.19 - 2026-05-16

- Add the first `plan-native-spike` flow so Linuxoid can stage a local APK, assess whether it fits the first native app slice, and write a Linuxoid-owned bundle layout plus bootstrap spec for future no-runtime execution.
- Fix the native spike gate so launcher-resolved multi-activity apps are still eligible, which lets Calculator pass the first real native planner proof without weakening the service, IME, boot, or secondary-process blockers.
- Refresh the README Mermaid architecture, dependency notes, verification examples, and roadmap so the GitHub repo now reflects the new native-spike slice and the next work toward true direct Linux execution.

## v0.1.18 - 2026-05-16

- Add attached-target `inspect-package` metadata lookup so Linuxoid can read package visibility, resolved launcher, install path, and version information from a live `attached-adb` runtime.
- Add attached-ADB launcher auto-resolution to `preflight-runtime`, `launch-package`, `verify-package`, and `verify-package-matrix`, removing the need to hard-code simple launcher components for installed-app flows.
- Verify the new attached-target path live with `com.android.settings`, plus a `3/3` attached-target matrix for `com.android.settings`, `com.android.calculator2`, and `org.fdroid.fdroid`, then refresh the README roadmap toward the first native package-launch spike.

## v0.1.17 - 2026-05-16

- Add backend-neutral `verify-package` and `verify-package-matrix` commands so installed-package verification no longer depends on Waydroid-shaped product names.
- Add attached-ADB generic verification tests and fix the matrix fixture so mocked `am start -W` output proves the launched component the same way the real runtime bridge expects.
- Refresh the README architecture, verification examples, progress numbers, and next-step roadmap so the GitHub repo reflects the new generic verification surface and the next attached-target priorities.

## v0.1.16 - 2026-05-16

- Add `discover-runtime` and `preflight-runtime` so Linuxoid can enumerate attached Android targets and check launch readiness before a generic installed-package launch.
- Bound the new attached-ADB discovery/preflight path with timeouts so it fails fast instead of hanging in the live environment.
- Refresh the README architecture and next-step roadmap to reflect that runtime discovery/preflight is now implemented and the next frontier is generic verification plus launcher resolution on attached targets.

## v0.1.15 - 2026-05-16

- Refresh the GitHub README Mermaid architecture so it now shows both the current working backend-neutral transition architecture and the long-term native Linux runtime target.
- Add a clear external-dependencies section to the README covering the current build and runtime requirements, including `apktool`, `adb`, and a live Android runtime adapter.
- Add five concrete next-step recommendations in the README focused on the goal of running Android apps on Linux without external Android runtime dependencies.

## v0.1.14 - 2026-05-16

- Add a backend-neutral `launch-package` runtime seam for installed-package launches, with `waydroid`, `attached-adb`, and `native` backend routing.
- Shift installed-package host launch artifacts to the generic `launch-package` contract while keeping the current Waydroid-facing commands as compatibility aliases.
- Add generic installed-package contract tests and raise verified phase loading to `91/100` while checkpoint gates remain `70/100`.

## v0.1.13 - 2026-05-16

- Fix automatic launcher inference so Linuxoid skips disabled manifest launcher candidates when generating Linux launchers from local APKs.
- Verify the repaired APK-backed path live with `/home/astra/Downloads/F-Droid.apk`, reaching a successful Linux launch through the inferred main activity.
- Raise verified project loading to `90/100` while checkpoint gates remain `70/100`.

## v0.1.12 - 2026-05-16

- Add `verify-apk-host-launch-auto` so Linuxoid can verify the full local-APK Linux launcher flow against a live Android runtime.
- Verify the new APK-backed path live with `/home/astra/Downloads/keyboard-0.1.28.apk`, reaching `Ready for typing: yes` through the generated Linux launcher.
- Raise verified project loading to `89/100` while checkpoint gates remain `70/100`.

## v0.1.11 - 2026-05-16

- Add the current full working Mermaid architecture to the GitHub README so the live Linuxoid flow is visible at a glance.
- Document both verified execution paths in the README diagram: APK-backed IME provisioning and installed-package Waydroid launch verification.

## v0.1.10 - 2026-05-16

- Add `verify-waydroid-matrix` so Linuxoid can verify several installed Waydroid apps in one pass and report package-level pass/fail results.
- Extend the Waydroid integration layer with matrix reporting that keeps running even when one app fails, so compatibility gaps surface honestly instead of aborting the whole batch.
- Keep verified project loading at `88/100` while raising the repeatability of the direct-on-Linux verification workflow.

## v0.1.9 - 2026-05-16

- Add `verify-waydroid-package` so Linuxoid can prove a direct Linux launch path for an installed Waydroid app through three checks: runtime launch, launcher generation, and generated launcher execution.
- Verify the new direct-on-Linux path on live Waydroid with `com.android.calculator2`, `com.android.settings`, and `org.fdroid.fdroid`.
- Raise verified project loading to `88/100` while keeping checkpoint gates at `70/100`.

## v0.1.8 - 2026-05-16

- Add `launch-waydroid-package` so Linuxoid can launch installed non-IME apps on Waydroid without requiring APK reinstall or hardcoded component selection.
- Add `desktopify-waydroid-package` so Linuxoid can generate Linux launchers and `.desktop` entries for installed Waydroid apps.
- Verify the new installed-package path on live Waydroid with `com.android.calculator2`, including a Linuxoid-generated host launcher execution from Linux.
- Raise verified project loading to `87/100` and checkpoint gates to `70/100`.

## v0.1.7 - 2026-05-16

- Normalize fully qualified IME identifiers to the short `package/.Class` form before Linuxoid sends `ime enable` and `ime set`, fixing the Waydroid mutation failure for the keyboard app.
- Verify the Linuxoid-generated host launcher against a live Waydroid runtime and reach `Ready for typing: yes` for the FUTO keyboard APK from Linux.
- Raise verified project loading to `84/100` while runtime checkpoint gates remain `58/100`.

## v0.1.6 - 2026-05-16

- Rename the project-facing identity to `Linuxoid` and point the local Git remote at `https://github.com/iamawanishmaurya/Linuxoid`.
- Add `desktopify-apk-auto`, which infers the launcher activity from the APK manifest instead of requiring a caller-supplied component.
- Split desktop-entry and launcher-script roots so Linuxoid can target host-discoverable application directories without forcing the wrapper script to live beside the `.desktop` file.
- Raise verified project loading to `78/100` while keeping runtime checkpoint gates at `58/100`.

## v0.1.5 - 2026-05-16

- Add a generic `launch-activity` bridge for explicit Android component launches from Linux.
- Add a `desktopify-apk` path that generates a Linux wrapper script and `.desktop` entry for a caller-selected Android component.
- Harden component matching so the runtime treats short and fully qualified Android component forms as equivalent.
- Raise verified project loading to `76/100` and runtime checkpoint gates to `58/100`, with host integration now in active validation.

## v0.1.4 - 2026-05-16

- Add a real `provision-ime` path that installs a keyboard APK on a live Android target, enables the IME, sets it as default, and re-verifies the resulting runtime state.
- Extend live runtime verification to prove the target IME is explicitly present in `enabled_input_methods` in addition to being registered and selected as default.
- Guard the provisioning verdict against wrong-APK/package mismatches and preserve partial evidence when post-action readback fails.
- Raise verified project loading to `71/100` and runtime checkpoint gates to `48/100`, with the golden-app launch and repeatability checkpoints now complete.

## v0.1.3 - 2026-05-16

- Add a real `load-apk` path that stages APKs into a compat root using decoded manifest and `apktool.yml` metadata.
- Add an `adb-ime-status` runtime bridge that verifies install state, IME registration, default IME selection, and settings launch on a live Android target.
- Harden the loader/runtime slice with install-key sanitization, exact ADB matching, optional launch verification, and unique cleaned temp decode directories.
- Raise verified project loading to `65/100` and runtime checkpoint gates to `28/100`.

## v0.1.2 - 2026-05-16

- Add a Phase 4 decoded-manifest assessor for runtime and service requirements.
- Add CLI support for `compatctl assess-manifest`.
- Verify the keyboard APK manifest path and raise Phase 4 loading to `45/100`, bringing overall phase loading to `58/100`.

## v0.1.1 - 2026-05-16

- Add the first C++ MVP scaffold with `compatctl`, a checkpoint engine, and package-layout planning.
- Add local build and test verification for the new scaffold.
- Add reproducibility notes in the project README.

## v0.1.0 - 2026-05-16

- Bootstrap the repository, documentation workflow, and MVP planning baseline.
- Establish English-only project documentation and commit policy from the first version.
