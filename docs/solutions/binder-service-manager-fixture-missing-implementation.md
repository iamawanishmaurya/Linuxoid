Links back to: [2026-05-17-binder-service-manager-fixture-missing-implementation.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-binder-service-manager-fixture-missing-implementation.md)

## What Failed

The new Binder-shaped manager tests referenced:

- `wfa::RunBinderServiceManagerFixture(...)`
- `wfa::RenderBinderServiceManagerFixtureJson(...)`

but Linuxoid had no implementation file or target wiring for that contract, so `wfa_tests` failed at link time.

## What Worked

Linuxoid now implements the missing fixture in `/home/astra/codex/wine-for-android/src/binder_service_manager.cpp`, wires it into `wfa_core` in `/home/astra/codex/wine-for-android/CMakeLists.txt`, exposes it through `compatctl` in `/home/astra/codex/wine-for-android/src/main.cpp`, and threads its artifacts into the lifecycle shim in `/home/astra/codex/wine-for-android/src/native_lifecycle.cpp`.

The new fixture writes:

- `binder/service-manager.json`
- `binder/registered-services.json`
- `binder/service-lookups.jsonl`
- `binder/service-transactions.jsonl`

## Why It Worked

The missing seam was structural rather than algorithmic. A deterministic in-process Binder-shaped manager was enough to satisfy the current P4.1 gate because it provides stable service registration, lookup, and transaction artifacts for harnesses without pretending real Binder transport already exists.

## Commands Run

```bash
cmake --build build
./build/compatctl bootstrap-native-spike /tmp/linuxoid-native-calculator.apk /tmp/linuxoid-native-compat /tmp/linuxoid-native-spike
./build/compatctl native-service-manager-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
ctest --test-dir build --output-on-failure
./build/compatctl status
```
