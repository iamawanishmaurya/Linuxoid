# Solution: apktool-yaml-parser-too-strict

- Problem file: [2026-05-16-apktool-yaml-parser-too-strict.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-apktool-yaml-parser-too-strict.md)
- What failed:
  The `apktool.yml` parser expected keys like `versionCode` and `versionName` to start at column zero, but real `apktool.yml` nests them under indented sections.

- What worked:
  Allowing leading whitespace in the YAML key matcher fixed the parser and let the metadata tests pass.

- Why it worked:
  The parser logic only needed to become indentation-tolerant; the field extraction model was otherwise correct for the current scope.

- All commands run:

```text
cmake --build build && ctest --test-dir build --output-on-failure
cmake --build build && ctest --test-dir build --output-on-failure && rm -rf /tmp/wfa-load && ./build/compatctl load-apk /home/astra/Downloads/keyboard-0.1.28.apk /tmp/wfa-load && ./build/compatctl adb-ime-status emulator-5590 org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME org.futo.inputmethod.latin/.uix.settings.SettingsActivity
```
