# 08-02 Summary

## Result

Linuxoid now exposes the first real post-`JNI_OnLoad` startup seam for the keyboard APK path as a distinct Linuxoid-managed bridge requirement instead of collapsing JNI and startup progress into one generic native error.

## What changed

- `launch-apk` now keeps these states machine-readable and separate:
  - `native_jni_state: called`
  - `native_app_start_bridge_state: linuxoid_managed_app_start_bridge_required`
  - `native_post_jni_startup_state: managed_activity_dispatch_required`
- `--first-app-start-proof` now reports:
  - `blocking_reason: linuxoid_managed_app_start_bridge_required_for_first_app_start:libjni_latinime.so`
  - `next_blocker: implement_linuxoid_managed_app_start_bridge_for_libjni_latinime_so`

## Remaining blockers

- Upstream native seam: `implement_linuxoid_managed_app_start_bridge_for_libjni_latinime_so`
- Downstream managed seam: `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`
