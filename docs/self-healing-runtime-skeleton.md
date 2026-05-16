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
- Linuxoid writes stable artifacts for diagnosis:
  - `runtime-health.json`
  - `runtime-health-trace.jsonl`
  - `runtime-health-replay.json`
  - `runtime-diagnostic-events.jsonl`
  - `runtime-diagnostic-replay.json`
- The traces can now be replayed and merged without rerunning the full UI path.

What it **does not** mean yet:

- Linuxoid does **not** yet auto-fix or rerun real Android app execution.
- Linuxoid does **not** yet have ART or a real `PathClassLoader`.
- Linuxoid does **not** yet execute application classes through a real host-side ART invocation, even though it can now resolve manifest-target descriptors offline from real DEX contents.
- Linuxoid does **not** yet have full Android Binder semantics.
- Linuxoid does **not** yet have compositor-backed Android rendering.
- Linuxoid does **not** yet have full IME/text composition.

Current meaning of “self-healing” in Linuxoid:

1. Detect runtime readiness and failure shape deterministically.
2. Classify the failure into a known subsystem bucket.
3. Select a bounded recovery action.
4. Persist the diagnosis and recovery plan in replayable artifacts.
5. Merge the existing JSONL traces back into one diagnostic replay bundle.
6. Refuse false success when a required dependency is still missing.

Current commands:

```bash
./build/compatctl native-runtime-health-fixture <bootstrap-manifest> [scenario]
./build/compatctl native-runtime-recovery-plan <bootstrap-manifest> [scenario]
./build/compatctl native-runtime-health-replay <trace-jsonl-path>
./build/compatctl native-runtime-diagnostic-replay <bootstrap-manifest>
./build/compatctl native-art-classloader-fixture <bootstrap-manifest>
./build/compatctl native-art-class-resolution-fixture <bootstrap-manifest>
./build/compatctl native-art-runtime-smoke <bootstrap-manifest>
```

Current deterministic scenarios:

- `baseline`
- `missing_artifact`
- `failed_native_load`
- `unavailable_display`
- `failed_service_lookup`

Next gate after this skeleton:

- use the existing offline class-resolution plus host-ART smoke seams to drive the first real `PathClassLoader` or equivalent class-resolution attempt for simple APKs.
