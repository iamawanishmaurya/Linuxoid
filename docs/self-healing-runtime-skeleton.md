# Linuxoid Self-Healing Runtime Skeleton

Linuxoid now has a **self-healing runtime observability skeleton** for the native direct-run path.

What that means **today**:

- Linuxoid can record structured health for:
  - APK staging
  - native library loading readiness
  - surface readiness
  - input queue readiness
  - Binder/service readiness
  - DEX/classloader readiness
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
- Linuxoid can now materialize those actions into stable plan artifacts:
  - `runtime-recovery-plan.json`
  - `runtime-recovery-actions.jsonl`
- Linuxoid now has a concrete DEX/classloader preparation seam behind that recovery story:
  - `native-art-classloader-fixture`
  - `art/classloader-plan.json`
  - `art/dex-inventory.json`
  - `art/art-classloader-trace.jsonl`
- Linuxoid now has a concrete offline DEX class-resolution seam behind that preparation step:
  - `native-art-class-resolution-fixture`
  - `art/class-resolution-map.json`
  - `art/art-class-resolution-trace.jsonl`
  - `art/art-class-resolution-result.json`
- Linuxoid now also has a host-ART runtime smoke seam that builds on that plan:
  - `native-art-runtime-smoke`
  - `art/runtime-smoke-invocation-plan.json`
  - `art/runtime-smoke-invocation.log`
  - `art/runtime-smoke-trace.jsonl`
  - `art/runtime-smoke-result.json`
  - a deterministic manifest-derived class target for the first real host-side class-resolution attempt when `dalvikvm` is safely available
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

That summary is now explicit in the health JSON. A harness no longer has to scan every raw record to answer basic questions like:

- Is the runtime blocked on a real dependency?
- How many subsystems are failing right now?
- Which bounded recovery actions were selected?
- Which failing subsystems caused those actions?

What it **does not** mean yet:

- Linuxoid does **not** yet self-heal by automatically making the app run after a failure.
- Linuxoid does **not** yet auto-fix or rerun real Android app execution.
- Linuxoid does **not** yet own a full embedded ART runtime or a real in-process `PathClassLoader`.
- Linuxoid does **not** yet execute full Android app startup through host-side ART, even though it can now resolve manifest-target descriptors offline from real DEX contents, prepare a real host-side class-resolution command for `dalvikvm` when that runtime exists, exercise the runner-backed execution seam through a Linuxoid-owned override probe in fixtures, separate activity-bootstrap planning from bootstrap execution, and keep deterministic execution-context, runner-state, runner-script, and per-phase log artifacts around that execution seam.
- Linuxoid does **not** yet have full Android Binder semantics.
- Linuxoid does **not** yet have compositor-backed Android rendering.
- Linuxoid does **not** yet have full IME/text composition.

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

Until those gates land, Linuxoid can diagnose and replay failures very well, but it still cannot claim full native Android app execution on Linux.

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
