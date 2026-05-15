# Solution: malformed-regex-literals-in-manifest-assessment

- Problem file: [2026-05-16-malformed-regex-literals-in-manifest-assessment.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-malformed-regex-literals-in-manifest-assessment.md)
- What failed:
  The first implementation used malformed raw-string regex literals, which the compiler treated as unterminated strings.

- What worked:
  Replacing the malformed raw strings with correctly escaped standard string literals fixed the compiler errors.

- Why it worked:
  The parser logic was fine. Only the literal syntax was invalid.

- All commands run:

```text
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure && apktool d -f -s -o /tmp/keyboard-apktool /home/astra/Downloads/keyboard-0.1.28.apk >/tmp/keyboard-apktool.log && ./build/compatctl status && ./build/compatctl assess-manifest /tmp/keyboard-apktool/AndroidManifest.xml
```
