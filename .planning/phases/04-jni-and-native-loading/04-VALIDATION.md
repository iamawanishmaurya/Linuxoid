---
phase: 4
slug: jni-and-native-loading
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-18
---

# Phase 4 — Validation Strategy

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
| 4-01-01 | 01 | 1 | JNI-01 | T-04-01 | Candidate `x86_64` libraries must be reported and attempted deterministically, with exact native-load outcomes instead of generic collapse | source + cli | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 4-02-01 | 02 | 2 | JNI-02 | T-04-02 | JNI and native blocker reporting must stay exact through operator-facing launch and first-app-start surfaces without false success | cli + regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Existing infrastructure covers all phase requirements.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Real keyboard APK smoke exposes exact `x86_64` native/JNI blocker details | JNI-01, JNI-02 | The verification APK is local and opt-in, so CI cannot require it | Run `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` and inspect `selected_abi`, `native_libraries`, `native_execute`, `jni_onload_called`, `launch_status`, `recommended_recovery_action`, and any new per-library blocker fields |
| Managed-start proof still preserves the downstream `SettingsActivity.onCreate(Landroid/os/Bundle;)V` seam while the upstream native blocker remains | JNI-02 | The keyboard APK and its runtime seam are local/manual | Run `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` and confirm native/JNI blocker truth plus existing managed seam truth |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 210s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
