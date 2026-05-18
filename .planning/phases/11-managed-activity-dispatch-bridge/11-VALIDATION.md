# Phase 11 Validation

| Test ID | Plan | Wave | Requirement | Script/Test | Type | Command | Expected |
|--------|------|------|-------------|-------------|------|---------|----------|
| T-11-01 | 01 | 1 | JNI-11 | managed dispatch fixture / launch proof | build+unit | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build` | Build stays green while the managed dispatch bridge seam is introduced |
| T-11-02 | 02 | 2 | JNI-12 | `wfa_tests` | regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | Tests pin distinct JNI-registration, managed-dispatch, and later framework seams |
| T-11-03 | 03 | 3 | VER-08 | live smoke | launch proof | `TMPDIR=/home/astra/codex/wine-for-android/.tmp timeout 25 ./build/compatctl launch-apk --first-app-start-proof --self-heal-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` | Launch JSON shows a smaller seam than `managed_activity_dispatch_required` or reports an exact managed dispatch attempt boundary |

