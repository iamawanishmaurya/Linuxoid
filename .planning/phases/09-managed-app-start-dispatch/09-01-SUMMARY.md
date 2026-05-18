# 09-01 Summary

## Result

Linuxoid no longer stops at the generic `linuxoid_managed_app_start_bridge_required` seam once `libjni_latinime.so` loads and `JNI_OnLoad` succeeds.

## What changed

- Extended `native_execute_stub` so a loaded JNI-shaped primary library is inspected for post-`JNI_OnLoad` dispatch exports.
- Added deterministic native report fields:
  - `post_jni_dispatch_symbol_kind`
  - `post_jni_dispatch_symbol`
  - `post_jni_dispatch_reason`
- Changed the blocked exit seam from `linuxoid_managed_app_start_bridge_required` to `jni_registration_dispatch_required` when Linuxoid can prove the library exposes a registration-helper boundary.

## Remaining blocker

- `dispatch_jni_registration_for_libjni_latinime_so`
