# Problem: incompatible-string-view-regex-iterator

- Date: 2026-05-16
- Exact error:

```text
/home/astra/codex/wine-for-android/src/manifest_assessment.cpp:125:74: error: no matching function for call to ‘std::__cxx11::regex_iterator<...>::regex_iterator(std::basic_string_view<char>::const_iterator, std::basic_string_view<char>::const_iterator, const std::__cxx11::regex&)’
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Build the manifest-assessment slice after the regex literal fix.
  3. Run `cmake -S . -B build && cmake --build build`.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The permission scan uses `std::sregex_iterator`, which expects `std::string` iterators. The current code passes `std::string_view` iterators instead, so the iterator type does not match the regex iterator template.
