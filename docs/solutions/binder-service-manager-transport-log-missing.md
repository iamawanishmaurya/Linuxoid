# Binder Service Manager Transport Log Missing

Links back to: [2026-05-17-binder-service-manager-transport-log-missing.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-binder-service-manager-transport-log-missing.md)

## What failed

The Binder-shaped fixture already produced stable registration, lookup, and transaction artifacts, but it did not populate a deterministic transport log or transport round-trip metadata.

## What worked

Added a socketpair-backed local transport seam in `src/binder_service_manager.cpp` that:

- simulates lookup and transaction request/response pairs
- records each round trip in `binder/transport-messages.jsonl`
- exposes `transport_kind`, `transport_log_path`, and `transport_round_trips`
- threads the transport artifact path into the lifecycle/session JSON

## Why it worked

The tests only needed a truthful local transport contract, not a full Android Binder implementation. A Unix socketpair gives Linuxoid a real kernel-backed message round trip while keeping the payload model deterministic and easy to verify.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl bootstrap-native-spike /tmp/linuxoid-native-calculator.apk /tmp/linuxoid-native-compat /tmp/linuxoid-native-spike
./build/compatctl native-service-manager-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
```
