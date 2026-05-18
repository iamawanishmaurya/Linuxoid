---
phase: 3
slug: runtime-context-bridge
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-18
---

# Phase 3 — Validation Strategy

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
| 3-01-01 | 01 | 1 | DEX-02 | T-03-01 | Receiver and invoke-argument state must not silently disappear or drift across the first framework boundary | source + cli | `cmake --build build && ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 3-02-01 | 02 | 2 | DEX-02, VER-02 | T-03-02 | The next post-receiver blocker must stay exact and reproducible instead of collapsing into a generic runtime failure | source + cli | `cmake --build build && ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 3-03-01 | 03 | 3 | VER-02 | T-03-03 | Repo truth and regression coverage must agree on the same narrowed managed-runtime blocker | cli + regression | `cmake --build build && ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Existing infrastructure covers all phase requirements.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Real keyboard APK smoke still reports `SettingsActivity` plus the upstream native/surface blocker while the synthetic seam advances | DEX-02, VER-02 | The verification APK is local and opt-in, so CI cannot require it | Run the guarded `launch-apk --first-app-start-proof` command against `/home/astra/Downloads/keyboard-0.1.28.apk` and inspect `first_android_app_start` plus `launch_status` fields |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 210s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
