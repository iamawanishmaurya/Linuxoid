# Solution: unsanitized-install-id-from-version-name

- Problem file: [2026-05-16-unsanitized-install-id-from-version-name.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-unsanitized-install-id-from-version-name.md)
- What failed:
  The install ID used the raw human-facing `versionName`, which could contain characters rejected by the path validator.

- What worked:
  Sanitizing the install-key segment before building the install ID fixed the path-safety issue while keeping the original `version_name` in metadata.

- Why it worked:
  Filesystem-safe path keys and human-facing version labels are different concerns. Separating them prevents valid APK metadata from breaking local load paths.

- All commands run:

```text
cmake --build build && ctest --test-dir build --output-on-failure && rm -rf /tmp/wfa-load && ./build/compatctl load-apk /home/astra/Downloads/keyboard-0.1.28.apk /tmp/wfa-load && ./build/compatctl adb-ime-status emulator-5590 org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME org.futo.inputmethod.latin/.uix.settings.SettingsActivity && ./build/compatctl adb-ime-status emulator-5590 org.futo.inputmethod.latin org.futo.inputmethod.latin/.LatinIME
```
