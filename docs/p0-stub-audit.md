# Linuxoid P0 Stub Audit

Historical note: this audit captures the baseline before the first `P1` native-runner slice landed. Some entries below are now partially resolved in later releases.

This document maps the current native-execution blockers that Linuxoid must clear before `P1 NDK Execution Core` can honestly begin.

## Scope

- Goal state for this audit: every current early exit or placeholder around direct execution is named.
- Excluded from this audit: Waydroid and attached-ADB success paths, except where they still matter as regression oracles.

## Current Blocking Commands

| Command | Current Result | Blocking File |
| --- | --- | --- |
| `compatctl native-execute-stub ...` | prints `Execution Engine Ready: no` and exits `2` | `src/main.cpp` |
| `compatctl bootstrap-native-spike ...` | writes bootstrap assets but reports execution not ready | `src/native_spike.cpp` |
| `compatctl native-lifecycle-shim ...` | writes lifecycle/session artifacts but still reports execution engine not ready | `src/native_lifecycle.cpp` |
| `compatctl discover-runtime native` | now reports one Linuxoid-owned local runtime target with host ABI plus compat-root metadata | `src/runtime_bridge.cpp` |
| `compatctl preflight-runtime native ...` | now uses staged package metadata for visibility plus launcher readiness, but still cannot promise successful ART execution | `src/runtime_bridge.cpp` |
| `compatctl launch-package native ...` | now routes staged packages into the bootstrap-execution seam and fails honestly for non-candidate or runtime-missing packages | `src/runtime_bridge.cpp` |
| `compatctl inspect-package native ...` | now reads staged package metadata from the native compat root | `src/runtime_bridge.cpp` |

## Current Stub Surface

### 1. Local native entrypoint still stops in the CLI layer

- File: `src/main.cpp`
- Command: `native-execute-stub`
- Behavior:
  - echoes the planned bundle paths
  - checks file presence
  - prints `Execution Engine Ready: no`
  - exits `2`

Why it matters:

- Linuxoid still does not attempt `dlopen()`
- no `.so` entrypoint lookup happens yet
- no process loop or app-owned code executes

### 2. Native bootstrap artifacts are real, but execution is not

- File: `src/native_spike.cpp`
- Command path: `bootstrap-native-spike`
- Behavior:
  - writes manifest, env script, entrypoint script, and report
  - keeps `Execution Engine Ready: no`
  - explicitly tells the next layer to replace the stub with real Linux execution

Why it matters:

- Linuxoid owns the artifact layout
- Linuxoid does not yet own a live Android-native process

### 3. Lifecycle handoff exists, but runtime services are still scaffold-only

- File: `src/native_lifecycle.cpp`
- Command path: `native-lifecycle-shim`
- Behavior:
  - writes deterministic session artifacts
  - exposes service bindings such as `activity_manager`, `package_manager`, `resource_loader`, and `binder_registry`
  - still stops before real execution

Why it matters:

- Linuxoid has a place to hang real native execution state
- those services are still placeholders instead of runtime behavior

### 4. The `native` backend contract is now a partial local implementation

- File: `src/runtime_bridge.cpp`
- Command paths:
  - `discover-runtime native`
  - `preflight-runtime native`
  - `launch-package native`
  - `inspect-package native`
- Behavior:
  - `discover-runtime native` now reports a Linuxoid-owned local target
  - `inspect-package native` now reads staged package metadata from the compat root
  - `preflight-runtime native` now resolves staged package visibility plus launcher readiness
  - `launch-package native` now routes candidate packages into the bootstrap-execution seam and fails honestly for non-candidates or runtime-missing bundles

Why it matters:

- the backend-neutral CLI contract is no longer a hard stub for the local native path
- Linuxoid now has a real place to hang staged-package launch behavior before full ART execution exists
- the remaining gap is deeper native execution, not basic bridge surface ownership

## Ordered Queue For P1

1. Replace the `native-execute-stub` command body with a real native bundle runner.
2. Extract the candidate native library from the staged APK and attempt `dlopen()`.
3. Resolve `ANativeActivity_onCreate`.
4. Build the smallest fake `ANativeActivity` that can survive the first entry call.
5. Add the smallest fake `JavaVM` and `JNIEnv` needed for `JNI_OnLoad` safety.
6. Add an APK-backed `AAssetManager`.
7. Add an `ALooper` implementation that keeps the process alive long enough to inspect failures.
8. Feed the resulting execution state back into the `native` backend bridge so discovery/preflight/launch can stop lying by omission.

## Regression Oracles To Keep

These are not the target runtime, but Linuxoid should keep them alive while building `P1-P4`:

- Waydroid-backed launch verification
- attached-ADB target inspection and launch verification
- APK-backed keyboard and F-Droid proofs
- installed-package matrix verification

They help catch regressions while the native path is still incomplete.
