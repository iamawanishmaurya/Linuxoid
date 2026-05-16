# Solution: replace malformed raw-string regex with an escaped string literal

Related problem: [2026-05-16-native-lifecycle-json-array-regex-literal.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-native-lifecycle-json-array-regex-literal.md)

## What failed

`ExtractJsonStringArray` used a raw-string regex literal that terminated early because the regex body itself contained `")`.

## What worked

Replaced the malformed raw string with the escaped regex string `"\"([^\"]*)\""`.

## Why it worked

The escaped normal string avoids the raw-string delimiter collision while preserving the intended pattern for quoted JSON string values.

## Commands run

```bash
cmake --build build
```
