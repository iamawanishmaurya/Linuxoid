# 07-02 Summary

## Result

Linuxoid now exposes the first true native startup boundary for the real keyboard APK path: `libjni_latinime.so` loads, `JNI_OnLoad` is called, and the launch stops at the missing native activity entrypoint.

## What changed

- Extended the native execute path so the selected entry library can be JNI-shaped instead of only NativeActivity-shaped.
- Recorded deterministic per-library facts through `native_execute.library_load_attempts[]` and `native_execute.jni_onload_results[]`.
- Preserved separate machine-readable states for:
  - `native_loading_state`
  - `native_jni_state`
  - `native_loading_library_name`
  - `native_execute.execution_engine_ready`
- Added regression coverage that proves a JNI-only library reports `native_activity_entrypoint_missing` only after a real `JNI_OnLoad` call, instead of collapsing back to generic load failure.

## Verified truth

- `libjni_latinime.so` is now the selected library on the real keyboard path.
- `JNI_OnLoad` runs and returns `11`.
- The launch still blocks honestly at `native_activity_entrypoint_missing`.
