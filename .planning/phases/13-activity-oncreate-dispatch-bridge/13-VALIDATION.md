# Phase 13 Validation

| Test ID | Plan | Wave | Requirement | Script/Test | Type | Command | Expected |
|--------|------|------|-------------|-------------|------|---------|----------|
| T-13-01 | 01 | 1 | JNI-15 | managed dispatch fixture and launch proof | build+unit | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build` | Build stays green while the keyboard lifecycle dispatch seam moves |
| T-13-02 | 02 | 2 | JNI-16 | `wfa_tests` | regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | Tests pin distinct runtime-context, lifecycle-dispatch, and post-dispatch blockers |
| T-13-03 | 03 | 3 | VER-10 | live keyboard smoke | launch proof | `TMPDIR=/home/astra/codex/wine-for-android/.tmp timeout 25 ./build/compatctl launch-apk --window-proof --first-app-start-proof --self-heal-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` | Launch JSON no longer stops at `activity_oncreate_bundle_dispatch_required` or reports the next exact post-dispatch seam |
