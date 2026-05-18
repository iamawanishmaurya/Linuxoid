# 12-02 Summary

## Result

The first post-binding seam is now explicit: Linuxoid reports `activity_oncreate_bundle_dispatch_required` instead of continuing to report the broader `managed_runtime_context_required` blocker.

## What changed

- Promoted successful runtime-context binding to:
  - `native_managed_activity_runtime_binding_state: linuxoid_runtime_context_bound`
  - `native_managed_activity_runtime_binding_reason: managed_activity_dispatch_target_bound_to_linuxoid_runtime_context`
  - `native_managed_activity_runtime_context_kind: linuxoid_managed_runtime_context_placeholder`
- Added launch and first-app-start JSON fields that preserve the binding state, binding reason, context id, and context kind.
- Kept the next exact blocker stable as `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`.

## Remaining blocker

- Real `Activity.onCreate(Bundle)` dispatch into the bound Linuxoid runtime context is still future work.
