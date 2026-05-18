---
phase: 6
slug: recovery-and-runtime-hardening
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-19
---

# Phase 6 — Validation Strategy

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
| 6-01-01 | 01 | 1 | APP-01, APP-02 | T-06-01 | Repeated keyboard APK launches preserve and validate sandbox, permission, and AppOps artifacts without hiding continuity failures | source + cli | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 6-02-01 | 02 | 2 | VER-03 | T-06-02 | Recovery reporting must stay rooted in the earliest upstream blocker and emit comparable journal truth across repeated runs | cli + regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build && TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Existing infrastructure covers all phase requirements.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Repeated keyboard APK launches preserve deterministic sandbox, permission, and AppOps artifacts under the same staging root | APP-01, APP-02 | The verification APK is local and opt-in, so CI cannot require it | Run `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk --permissions-proof --storage-proof /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` twice and inspect `storage`, `permissions`, `app_ops`, artifact paths, and continuity/reuse fields |
| The Self-Healing Android Device watchdog stays rooted in the exact native blocker on the real keyboard APK path | VER-03 | The verification APK and host-native seam are local/manual | Run `TMPDIR=/home/astra/codex/wine-for-android/.tmp ./build/compatctl launch-apk --window-proof --first-app-start-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` and confirm the watchdog preserves `native_loading_*` truth plus the tightened `recommended_next_action` and action journal semantics |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 210s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
