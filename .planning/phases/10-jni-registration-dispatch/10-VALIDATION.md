---
phase: 10-jni-registration-dispatch
created: 2026-05-19
---

# Phase 10 Validation

## Verification matrix

| Test ID | Plan | Wave | Requirement | Threat | Method | Command | Expected |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 10-01-01 | 01 | 1 | JNI-09 | T-10-01 | source + tests | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build` | build passes with registration-dispatch support present |
| 10-02-01 | 02 | 2 | JNI-10 | T-10-02 | tests | `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | offline fixture proves exact registration outcome or exact blocker |
| 10-03-01 | 03 | 3 | VER-07 | T-10-03 | live smoke | `TMPDIR=/home/astra/codex/wine-for-android/.tmp timeout 25 ./build/compatctl launch-apk --first-app-start-proof --self-heal-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` | launch JSON shows a smaller seam than `jni_registration_dispatch_required` or reports exact registration execution blocker |

## Required truths

- Launch JSON must keep these distinct:
  - `JNI_OnLoad`
  - registration-helper selection
  - registration dispatch
  - registration outcome
  - later managed bootstrap seam
- Self-Healing Android Device recovery wording must stay rooted in the earliest live blocker
- The later managed seam must remain visible:
  - `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context`
