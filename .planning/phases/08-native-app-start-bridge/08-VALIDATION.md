---
phase: 8
slug: native-app-start-bridge
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-19
---

# Phase 8 — Validation Strategy

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
| 8-01-01 | 01 | 1 | JNI-05 | T-08-01 | A JNI-shaped primary library must advance to a Linuxoid-owned app-start strategy or a smaller exact post-`JNI_OnLoad` seam | source + cli | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 8-02-01 | 02 | 2 | JNI-06 | T-08-02 | Once the app-start bridge exists, Linuxoid must expose the first true registration or startup seam without pretending the app fully launched | cli + regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 8-03-01 | 03 | 3 | VER-05 | T-08-03 | First-app-start, recovery, and docs must agree on the new post-`JNI_OnLoad` blocker for the real APK path | cli + regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Existing infrastructure covers all phase requirements.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| The real keyboard APK reaches a smaller seam than `native_activity_entrypoint_missing` | JNI-05, JNI-06 | The verification APK is local and opt-in, so CI cannot require it | Run `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` and inspect `native_loading_*`, `native_execute.*`, `blocking_reason`, and `next_blocker` |
| The downstream managed seam remains distinct after the native app-start seam moves | VER-05 | The keyboard APK and its managed context are local/manual | Re-run the same guarded command and confirm the first-app-start report still preserves the managed `Activity.onCreate(Bundle)` blocker once the native app-start seam advances |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 210s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
