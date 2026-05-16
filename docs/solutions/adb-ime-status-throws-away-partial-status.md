# Solution: adb-ime-status-throws-away-partial-status

- Problem file: [2026-05-16-adb-ime-status-throws-away-partial-status.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-adb-ime-status-throws-away-partial-status.md)
- What failed:
  The old `adb-ime-status` path always launched settings and could throw away useful install and IME facts if that final step failed.

- What worked:
  Making the settings component optional and treating launch verification as non-fatal fixed the issue.

- Why it worked:
  Status collection and UI launch verification are related but not identical operations. Allowing a read-only status path preserves the useful facts even when launch verification is skipped or fails.

- All commands run:

```text
cmake --build build && ctest --test-dir build --output-on-failure && rm -rf /tmp/wfa-load && ./build/compatctl load-apk /home/astra/Downloads/keyboard-0.1.28.apk /tmp/wfa-load && ./build/compatctl adb-ime-status emulator-5590 org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME org.futo.inputmethod.latin/.uix.settings.SettingsActivity && ./build/compatctl adb-ime-status emulator-5590 org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME
```
