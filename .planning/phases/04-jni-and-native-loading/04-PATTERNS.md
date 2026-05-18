---
phase: 4
slug: jni-and-native-loading
status: draft
created: 2026-05-18
---

# Phase 4 — Patterns

## Execution Pattern

1. Keep the real keyboard APK path authoritative.
2. Narrow the upstream native/JNI blocker before widening managed-runtime scope.
3. Preserve the already-known managed Bundle-boundary seam so Phase 5 does not need to rediscover it.

## Reporting Pattern

- `launch-apk` remains the authoritative real-APK native/JNI surface.
- `first_android_app_start` may mirror the upstream native blocker, but should not replace it.
- Per-library candidate order, `dlopen` outcome, `JNI_OnLoad` outcome, and entrypoint outcome must stay separate.

## Verification Pattern

- Fixture-native success stays in automated tests.
- Negative and partial native/JNI paths become more exact in automated tests.
- The real keyboard APK remains a manual smoke target, guarded by local `TMPDIR` to avoid host `/tmp` quota issues.

## Anti-Patterns

- Do not create a keyboard-only loader path outside `launch-apk`.
- Do not mix Wayland/window or broader framework work into this phase.
- Do not collapse native, JNI, and entrypoint failures into one generic blocked state.
