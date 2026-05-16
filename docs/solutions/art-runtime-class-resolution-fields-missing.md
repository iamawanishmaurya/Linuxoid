# ART runtime class-resolution fields missing

Problem: [docs/problems/2026-05-17-art-runtime-class-resolution-fields-missing.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-art-runtime-class-resolution-fields-missing.md)

## What failed

The new runtime-smoke tests expected a real host-side class-resolution attempt surface, but `NativeArtRuntimeSmokeReport` still only modeled a generic ART probe. It did not expose:

- `resolved_target_class_name`
- `resolved_target_class_descriptor`
- `runtime_class_resolution_succeeded`

It also did not yet build a real `dalvikvm -cp <apk> <class>` command.

## What worked

We extended the runtime-smoke seam to:

1. pick a deterministic manifest-derived target class from offline DEX resolution results
2. expose that target in the report and JSON artifacts
3. build a real host-side class-resolution command when a safe `dalvikvm` surface exists
4. distinguish between:
   - ART not detected
   - ART detected but unsafe to probe
   - class resolved but not executable because no `main`
   - class not found
   - generic runtime probe failure

## Why it worked

Linuxoid already had the ingredients for a real class-resolution attempt: staged APK path, offline class-resolution results, and ART runtime detection. The fix was to connect those pieces so runtime smoke targets a real class instead of a generic `-help` probe.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/compatctl native-art-runtime-smoke /tmp/linuxoid-native-spike/packages/com.android.calculator2/vc33-13/bootstrap/activity-bootstrap.json
./build/compatctl status
```
