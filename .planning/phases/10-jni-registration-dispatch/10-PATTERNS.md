---
phase: 10-jni-registration-dispatch
created: 2026-05-19
---

# Phase 10 Patterns

## Existing patterns to reuse

- Keep native seam detection in `src/native_execute_stub.cpp`
- Propagate operator-facing truth through `src/apk_native_launch.cpp`
- Keep watchdog wording rooted in the earliest blocker through `src/apk_self_healing_watchdog.cpp`
- Add deterministic offline fixture coverage in `tests/test_main.cpp`

## Preferred implementation style

- Extend the existing `native_execute` report instead of inventing a new side channel
- Keep blocked outcomes structured and machine-readable
- Preserve exact symbol, library, and seam naming
- Keep any JNI environment support minimal and explicit

## Testing pattern

- Add one focused fixture that models JNI registration dispatch without real Android services
- Keep real keyboard APK smoke optional/manual and guarded with timeout
- Make regressions fail on blocker drift, not only on process exit code

## Scope guard

Phase 10 is about JNI registration dispatch, not full framework bootstrap.

Keep post-registration seams distinct from:

- the current registration-helper boundary
- the later managed seam:
  - `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`
