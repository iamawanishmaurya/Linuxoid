# Solution: adb-status-prefix-false-positives

- Problem file: [2026-05-16-adb-status-prefix-false-positives.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-adb-status-prefix-false-positives.md)
- What failed:
  The runtime bridge used raw substring matching, so package and IME prefixes could be mistaken for exact matches.

- What worked:
  Parsing the ADB output line-by-line and matching exact tokens fixed the false-positive risk.

- Why it worked:
  ADB outputs one logical package or IME entry per line. Exact line matching maps cleanly to the actual command output shape.

- All commands run:

```text
cmake --build build && ctest --test-dir build --output-on-failure && rm -rf /tmp/wfa-load && ./build/compatctl load-apk /home/astra/Downloads/keyboard-0.1.28.apk /tmp/wfa-load && ./build/compatctl adb-ime-status emulator-5590 org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME org.futo.inputmethod.latin/.uix.settings.SettingsActivity && ./build/compatctl adb-ime-status emulator-5590 org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME
```
