# 09-02 Summary

## Result

Linuxoid now exposes the first real post-bridge startup seam for the keyboard APK path as a distinct JNI registration dispatch requirement instead of collapsing JNI and startup progress into one generic managed-bridge error.

## What changed

- `launch-apk` now keeps these states machine-readable and separate:
  - `native_jni_state: called`
  - `native_app_start_bridge_state: linuxoid_managed_app_start_bridge_selected`
  - `native_post_jni_startup_state: jni_registration_dispatch_required`
  - `native_post_jni_dispatch_symbol_kind: registration_helper`
  - `native_post_jni_dispatch_symbol: _ZN8latinime21registerNativeMethodsEP7_JNIEnvPKcPK15JNINativeMethodi`
- `--first-app-start-proof` now reports:
  - `blocking_reason: jni_registration_dispatch_required_for_first_app_start:libjni_latinime.so`
  - `next_blocker: dispatch_jni_registration_for_libjni_latinime_so`

## Remaining blockers

- Upstream native seam: `dispatch_jni_registration_for_libjni_latinime_so`
- Downstream managed seam: `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`
