# Runtime Health Replay Regex Syntax Build Failure Solution

Problem: [docs/problems/2026-05-17-runtime-health-replay-regex-syntax-build-failure.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-runtime-health-replay-regex-syntax-build-failure.md)

## What failed

The first runtime-health replay parser used raw-string regex literals whose delimiters collided with the embedded `)"` sequence inside the JSON-shaped patterns. The compiler rejected `src/runtime_health.cpp` before the replay code could build.

## What worked

The replay parser now uses ordinary escaped string literals for the regex patterns:

- `"\"subsystem_name\": \"([^\"]+)\""`
- `"\"state\": \"([^\"]+)\""`
- `"\"action_name\": \"([^\"]+)\""`
- `"\"ready\": (true|false)"`

## Why it worked

Escaped string literals avoid the raw-string delimiter collision entirely while keeping the patterns deterministic and easy to test against the JSONL trace format Linuxoid emits.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
