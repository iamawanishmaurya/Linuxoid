# Phase 12 Validation

| Test ID | Plan | Wave | Requirement | Script/Test | Type | Command | Expected |
|--------|------|------|-------------|-------------|------|---------|----------|
| T-12-01 | 01 | 1 | JNI-13 | managed runtime-context binding fixture / launch proof | build+unit | `TMPDIR=/home/astra/codex/wine-for-android/.tmp cmake --build build` | Build stays green while the managed runtime-context seam is introduced |
| T-12-02 | 02 | 2 | JNI-14 | `wfa_tests` | regression | `TMPDIR=/home/astra/codex/wine-for-android/.tmp ctest --test-dir build --output-on-failure` | Tests pin distinct managed-dispatch, managed-runtime-context, and later framework seams |
| T-12-03 | 03 | 3 | VER-09 | live smoke | launch proof | `TMPDIR=/home/astra/codex/wine-for-android/.tmp timeout 25 ./build/compatctl launch-apk --first-app-start-proof --self-heal-proof --package org.futo.inputmethod.latin --component org.futo.inputmethod.latin/.uix.settings.SettingsActivity /home/astra/Downloads/keyboard-0.1.28.apk <staging-root>` | Launch JSON shows a smaller seam than `managed_runtime_context_required` or reports an exact managed runtime-context binding attempt boundary |
