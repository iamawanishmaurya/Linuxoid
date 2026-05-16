# Problem: malformed raw-string regex in native lifecycle JSON array parser

## Exact error

```
/home/astra/codex/wine-for-android/src/native_lifecycle.cpp:110:41: warning: missing terminating " character
  110 |   const std::regex pattern(R"("([^"]*)")");
      |                                         ^
/home/astra/codex/wine-for-android/src/native_lifecycle.cpp:110:41: error: missing terminating " character
  110 |   const std::regex pattern(R"("([^"]*)")");
      |                                         ^~~
/home/astra/codex/wine-for-android/src/native_lifecycle.cpp: In function ‘std::vector<std::__cxx11::basic_string<char> > wfa::{anonymous}::ExtractJsonStringArray(const std::string&, const std::string&)’:
/home/astra/codex/wine-for-android/src/native_lifecycle.cpp:111:3: error: expected ‘,’ or ‘;’ before ‘std’
  111 |   std::vector<std::string> values;
      |   ^~~
```

## Reproduction steps

1. Add `ExtractJsonStringArray` in `src/native_lifecycle.cpp`.
2. Run `cmake --build build`.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Date: `2026-05-16`
- Toolchain: local `cmake` + C++20 build

## First hypothesis

The raw-string delimiter is malformed because the regex body itself includes `")`, which closes the raw string early. The parser should use a normal escaped string or a custom raw-string delimiter.
