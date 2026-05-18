---
phase: 2
slug: managed-activity-start
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-18
---

# Phase 2 — Validation Strategy

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
| 2-01-01 | 01 | 1 | DEX-01 | T-02-01 | Real APK activity/component mapping cannot silently drift to a fixture class | source + cli | `cmake --build build && ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 2-02-01 | 02 | 2 | DEX-01 | T-02-02 | Real DEX class/method lookup must stop at an exact blocker instead of out-of-bounds or vague failure | source + cli | `cmake --build build && ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 2-03-01 | 03 | 3 | APK-03, DEX-03 | T-02-03 | First-app-start proof must preserve honest blockers and deterministic JSON for the real APK path | cli + regression | `cmake --build build && ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Existing infrastructure covers all phase requirements.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Real keyboard APK smoke run returns within timeout with updated `first_android_app_start` fields | APK-03, DEX-03 | The verification target is local and opt-in, so CI cannot require the APK file | Run the guarded `launch-apk --first-app-start-proof` command against `/home/astra/Downloads/keyboard-0.1.28.apk` and inspect the JSON fields named in the plan acceptance criteria |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 210s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
