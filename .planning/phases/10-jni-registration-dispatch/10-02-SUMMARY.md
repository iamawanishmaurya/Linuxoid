# 10-02 Summary

## Result

Linuxoid now exposes the first real post-registration startup seam for the keyboard APK path as a distinct managed activity-dispatch requirement instead of collapsing registration progress back into one generic native failure.

## What changed

- `launch-apk` now keeps these states machine-readable and separate:
  - `native_jni_state: called`
  - `native_app_start_bridge_state: linuxoid_managed_app_start_bridge_selected`
  - `native_registration_dispatch_state: called`
  - `native_registration_dispatch_symbol_kind: registration_callback`
  - `native_registration_dispatch_symbol: _ZN8latinime22register_LanguageModelEP7_JNIEnv`
  - `native_registration_outcome_state: register_natives_completed`
  - `native_post_jni_startup_state: managed_activity_dispatch_required`
- `--first-app-start-proof` now reports:
  - `blocking_reason: managed_activity_dispatch_required_for_first_app_start:libjni_latinime.so`
  - `next_blocker: bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

## Remaining blocker

- `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

