# ART class-resolution missing module

Problem: [2026-05-17-art-class-resolution-missing-module.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-art-class-resolution-missing-module.md)

## What failed

The new ART class-resolution test gate would not compile because the production module did not exist yet. The first red-state failure was a missing include for `wfa/art_class_resolution_fixture.hpp`.

## What worked

Linuxoid now has a dedicated offline DEX class-resolution seam:

- `include/wfa/art_class_resolution_fixture.hpp`
- `src/art_class_resolution_fixture.cpp`
- `compatctl native-art-class-resolution-fixture <bootstrap-manifest>`

The new module reuses the staged bootstrap/classloader plan, reads real `classes*.dex` payloads from the APK archive, extracts class descriptors from DEX metadata, resolves manifest-target classes offline, and writes deterministic artifacts:

- `art/class-resolution-map.json`
- `art/art-class-resolution-trace.jsonl`
- `art/art-class-resolution-result.json`

The runtime health skeleton now points `dex_classloader_readiness` at this richer evidence and advances the deterministic recovery action to `attempt_host_art_class_resolution`.

## Why it worked

The missing gate was not a host ART runtime yet; it was the lack of deterministic evidence that Linuxoid could map manifest targets to real DEX contents. By adding an offline resolver first, Linuxoid now has a truthful bridge between classpath planning and future host-side ART execution.

## Commands run

```bash
cd /home/astra/codex/wine-for-android
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-art-class-resolution-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl native-runtime-health-fixture /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json baseline
```
