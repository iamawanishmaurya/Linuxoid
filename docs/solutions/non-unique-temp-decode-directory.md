# Solution: non-unique-temp-decode-directory

- Problem file: [2026-05-16-non-unique-temp-decode-directory.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-non-unique-temp-decode-directory.md)
- What failed:
  The decode root was deterministic, so concurrent same-name/same-size loads could clobber each other and leave temp data behind.

- What worked:
  Switching to a unique `mkdtemp` decode directory with scoped cleanup fixed both the collision risk and the leftover-temp-data problem.

- Why it worked:
  Each load invocation now gets its own isolated temp path, and that path is removed automatically after the decode and install step completes.

- All commands run:

```text
cmake --build build && ctest --test-dir build --output-on-failure && rm -rf /tmp/wfa-load && ./build/compatctl load-apk /home/astra/Downloads/keyboard-0.1.28.apk /tmp/wfa-load && ./build/compatctl adb-ime-status emulator-5590 org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME org.futo.inputmethod.latin/.uix.settings.SettingsActivity && ./build/compatctl adb-ime-status emulator-5590 org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME
```
