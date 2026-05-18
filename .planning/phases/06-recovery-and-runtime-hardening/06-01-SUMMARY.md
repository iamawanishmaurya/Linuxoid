# 06-01 Summary

## Result

Linuxoid now preserves and validates deterministic sandbox, permission, and AppOps state across repeated verification APK launches against the same staging root.

## What changed

- Extended the storage proof contract so repeated runs expose `persisted_state_preexisting`, `continuity_validated`, `marker_preexisting`, `marker_reused`, `continuity_state`, and `continuity_diagnostics` instead of silently rewriting the marker file on every launch.
- Extended the permission and AppOps contracts so repeated runs expose the same continuity truth, including whether persisted files were reused, validated, or rebuilt from incomplete or stale state.
- Threaded those continuity fields through the direct `launch-apk` JSON so repeated verification runs can be compared without reverse engineering sandbox artifacts by hand.
- Added regression coverage that proves repeated runs preserve deterministic marker and permission/AppOps state under the same app-data root.

## Verified truth

- Repeated `launch-apk --storage-proof` runs now converge on `continuity_state: validated_existing_state`.
- Repeated `launch-apk --permissions-proof` runs now converge on `permissions.continuity_state: validated_existing_state` and `app_ops.continuity_state: validated_existing_state`.
- The exact upstream native blocker on the keyboard APK path remains explicit while continuity hardening runs underneath it.
