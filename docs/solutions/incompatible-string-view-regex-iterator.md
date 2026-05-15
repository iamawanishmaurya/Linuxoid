# Solution: incompatible-string-view-regex-iterator

- Problem file: [2026-05-16-incompatible-string-view-regex-iterator.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-incompatible-string-view-regex-iterator.md)
- What failed:
  The permission scan used `std::sregex_iterator` with `std::string_view` iterators, which do not match the iterator type expected by that regex iterator template.

- What worked:
  Switching the scan to `std::cregex_iterator` over `const char*` pointers fixed the type mismatch.

- Why it worked:
  `std::cregex_iterator` is the correct regex iterator variant for C-style character ranges, which matches the `std::string_view` memory layout used by the parser.

- All commands run:

```text
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure && apktool d -f -s -o /tmp/keyboard-apktool /home/astra/Downloads/keyboard-0.1.28.apk >/tmp/keyboard-apktool.log && ./build/compatctl status && ./build/compatctl assess-manifest /tmp/keyboard-apktool/AndroidManifest.xml
```
