# 11-02 Summary

## Result

Linuxoid now exposes the first real post-dispatch runtime seam for the keyboard APK path as a distinct managed runtime-context requirement instead of collapsing managed progress back into a generic native or dispatch failure.

## What changed

- `launch-apk` now keeps these states machine-readable and separate:
  - `native_jni_state: called`
  - `native_app_start_bridge_state: linuxoid_managed_app_start_bridge_selected`
  - `native_registration_outcome_state: register_natives_completed`
  - `native_managed_activity_dispatch_state: linuxoid_dispatch_attempted`
  - `native_post_jni_dispatch_symbol: org.futo.inputmethod.latin.uix.settings.SettingsActivity->onCreate(Landroid/os/Bundle;)V`
  - `native_post_jni_startup_state: managed_runtime_context_required`
- `--first-app-start-proof` now reports:
  - `blocking_reason: managed_runtime_context_required_for_first_app_start:libjni_latinime.so`
  - `next_blocker: bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`

## Remaining blocker

- `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`
