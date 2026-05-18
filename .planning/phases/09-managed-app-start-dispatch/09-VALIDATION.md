---
phase: 9
slug: managed-app-start-dispatch
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-20
---

# Phase 9 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake + `ctest` |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build` |
| **Full suite command** | `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~210 seconds |

---

## Sampling Rate

- **After every task commit:** Run `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build`
- **After every plan wave:** Run `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 210 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 9-01-01 | 01 | 1 | JNI-07 | T-09-01 | Linuxoid must turn the managed bridge candidate into a smaller post-bridge dispatch seam | source + cli | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 9-02-01 | 02 | 2 | JNI-08 | T-09-02 | Dispatch, registration, and later managed seams must stay distinct and machine-readable | cli + regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 9-03-01 | 03 | 3 | VER-06 | T-09-03 | First-app-start, recovery, and docs must agree on the first post-bridge blocker | cli + regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Existing infrastructure covers all phase requirements.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| The real keyboard APK reaches a smaller seam than `linuxoid_managed_app_start_bridge_required` | JNI-07, JNI-08 | The verification APK is local and opt-in, so CI cannot require it | Run `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` and inspect `native_app_start_bridge_*`, `native_post_jni_startup_state`, `blocking_reason`, and `next_blocker` |
| The downstream managed seam remains distinct after the post-bridge dispatch seam moves | VER-06 | The keyboard APK and its managed context are local/manual | Re-run the same guarded command and confirm the first-app-start report still preserves `bridge_activity_oncreate_bundle_dispatch_into_managed_runtime_context` as a later seam once the post-bridge dispatch boundary advances |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 210s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
