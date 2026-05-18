---
phase: 04-jni-and-native-loading
plan: "01"
subsystem: per-library-native-loading
tags: [keyboard, native, jni, dlopen]
completed: 2026-05-18
---

# Phase 4 Plan 01 Summary

Linuxoid now turns the real keyboard APK native seam into deterministic per-library load truth instead of only `libraries_failed_to_load`.

## What changed

- The native execute report now records `library_load_attempts[]` for each staged host-ABI candidate.
- Each attempt preserves:
  - library path and name
  - candidate order
  - `load_state`
  - `jni_state`
  - `entrypoint_state`
  - `failure_reason`
  - exact loader detail when available
- `launch-apk` now surfaces exact native fields:
  - `native_loading_state`
  - `native_jni_state`
  - `native_loading_library_name`
  - `native_loading_library_path`
  - `native_loading_detail`

## Why it matters

Phase 4 needed to replace a generic native failure with one exact seam the next checkpoint can actually attack. Linuxoid now exposes deterministic evidence for which staged library failed and whether the stop happened at `dlopen`, `JNI_OnLoad`, or native entrypoint discovery.

## Honest outcome

- The real keyboard APK still does not launch successfully.
- Linuxoid now reports exact upstream native-load facts instead of only a generic `libraries_failed_to_load` collapse.
