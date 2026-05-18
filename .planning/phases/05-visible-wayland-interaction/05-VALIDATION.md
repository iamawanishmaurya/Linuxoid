---
phase: 5
slug: visible-wayland-interaction
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-18
---

# Phase 5 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake + `ctest` |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `cmake --build build` |
| **Full suite command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~210 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build`
- **After every plan wave:** Run `cmake --build build && ctest --test-dir build --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 210 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 5-01-01 | 01 | 1 | WIN-01 | T-05-01 | The real settings-activity session must bind to one deterministic surface/window target without hiding upstream native blockers | source + cli | `cmake --build build && ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 5-02-01 | 02 | 2 | WIN-02 | T-05-02 | Focus/input ownership must attach to the same visible-launch session and stay distinct from headless fallback truth | source + cli | `cmake --build build && ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 5-03-01 | 03 | 3 | VER-01 | T-05-03 | Repo truth and regression coverage must agree on visible-launch state versus exact native block state | cli + regression | `cmake --build build && ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Existing infrastructure covers all phase requirements.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Real keyboard APK window-proof smoke preserves the exact upstream native blocker while still resolving the real `SettingsActivity` session | WIN-01, VER-01 | The verification APK is local and opt-in, so CI cannot require it | Run guarded `launch-apk --window-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` and inspect `native_loading_*`, `surface`, and `window_manager` fields |
| Optional live Wayland target reports availability honestly when a display exists | WIN-01, WIN-02 | CI and many developer hosts may be headless | Run `compatctl native-wayland-surface-fixture <session-root> [width] [height]` and inspect `wayland_available`, `surface_created`, and `exit_reason` |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 210s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
