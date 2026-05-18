---
phase: 7
slug: native-libc-and-entry-bridge
status: complete
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-19
---

# Phase 7 — Validation Strategy

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
| 7-01-01 | 01 | 1 | JNI-03 | T-07-01 | The real entry library must move past the current `__strchr_chk` blocker or emit a smaller exact Android-libc seam | source + cli | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ✅ green |
| 7-02-01 | 02 | 2 | JNI-04 | T-07-02 | Once the entry library loads, Linuxoid must expose the first true native entry boundary without pretending JNI or app start succeeded | cli + regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ✅ green |
| 7-03-01 | 03 | 3 | VER-04 | T-07-03 | First-app-start, recovery, and docs must agree on the narrowed post-load blocker for the real APK path | cli + regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ✅ green |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing infrastructure covers all phase requirements.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| The real keyboard APK reaches a smaller native seam than `undefined symbol: __strchr_chk` and now reports the JNI-shaped missing-entrypoint boundary exactly | JNI-03, JNI-04 | The verification APK is local and opt-in, so CI cannot require it | Run `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` and inspect `native_loading_*`, `native_execute.*`, `blocking_reason`, and `next_blocker` |
| The downstream managed seam remains distinct after the native-entry seam moves | VER-04 | The keyboard APK and its managed context are local/manual | Re-run the same guarded command and confirm the first-app-start report still preserves the managed `Activity.onCreate(Bundle)` blocker once native loading advances |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 210s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** complete
