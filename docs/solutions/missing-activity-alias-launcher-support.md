# Solution: missing-activity-alias-launcher-support

- Problem file: [2026-05-16-missing-activity-alias-launcher-support.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-missing-activity-alias-launcher-support.md)
- What failed:
  The parser ignored launcher entries published through `activity-alias`, so alias-based apps were misclassified as non-launchable.

- What worked:
  Adding an `activity-alias` scan to the manifest parser fixed launcher detection and preserved the alias component name as the launch target.

- Why it worked:
  `activity-alias` is a normal Android launch surface. Treating it the same way as a launcher `activity` makes the readiness assessment match real manifest behavior.

- All commands run:

```text
cmake --build build && ctest --test-dir build --output-on-failure
cmake --build build && ctest --test-dir build --output-on-failure && ./build/compatctl assess-manifest /tmp/keyboard-apktool/AndroidManifest.xml
```
