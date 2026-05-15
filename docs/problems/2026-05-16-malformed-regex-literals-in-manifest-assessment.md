# Problem: malformed-regex-literals-in-manifest-assessment

- Date: 2026-05-16
- Exact error:

```text
/home/astra/codex/wine-for-android/src/manifest_assessment.cpp:31:59: warning: missing terminating " character
   31 |   const std::regex pattern(attribute_name + R"(="([^"]+)")");
      |                                                           ^
/home/astra/codex/wine-for-android/src/manifest_assessment.cpp:31:59: error: missing terminating " character
   31 |   const std::regex pattern(attribute_name + R"(="([^"]+)")");
      |                                                           ^~~
/home/astra/codex/wine-for-android/src/manifest_assessment.cpp:114:72: warning: missing terminating " character
  114 |   const std::regex package_pattern(R"(<manifest[^>]* package="([^"]+)")");
      |                                                                        ^
/home/astra/codex/wine-for-android/src/manifest_assessment.cpp:114:72: error: missing terminating " character
  114 |   const std::regex package_pattern(R"(<manifest[^>]* package="([^"]+)")");
      |                                                                        ^~~
/home/astra/codex/wine-for-android/src/manifest_assessment.cpp:124:54: warning: missing terminating " character
  124 |       R"(<uses-permission[^>]*android:name="([^"]+)")");
      |                                                      ^
/home/astra/codex/wine-for-android/src/manifest_assessment.cpp:124:54: error: missing terminating " character
  124 |       R"(<uses-permission[^>]*android:name="([^"]+)")");
      |                                                      ^~~
```

- Reproduction steps:
  1. Open the workspace at `/home/astra/codex/wine-for-android`.
  2. Add `src/manifest_assessment.cpp` with the initial regex-based parser.
  3. Run `cmake -S . -B build && cmake --build build`.

- Environment:
  - OS context: Linux workspace in Codex desktop
  - Current working directory: `/home/astra/codex/wine-for-android`
  - Shell: `zsh`
  - Compiler: `g++ 16.1.1`
  - CMake: `4.3.2`
  - Date: 2026-05-16

- First hypothesis:
  The regex literals used raw-string syntax incorrectly, so the compiler treated them as unterminated strings before any runtime logic could be tested.
