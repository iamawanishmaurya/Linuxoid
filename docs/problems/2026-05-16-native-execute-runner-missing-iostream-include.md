# Problem: native-execute-runner-missing-iostream-include

## Exact Error

```text
/home/astra/codex/wine-for-android/src/native_execute_stub.cpp: In lambda function:
/home/astra/codex/wine-for-android/src/native_execute_stub.cpp:161:12: error: ‘cout’ is not a member of ‘std’
/home/astra/codex/wine-for-android/src/native_execute_stub.cpp:16:1: note: ‘std::cout’ is defined in header ‘<iostream>’; this is probably fixable by adding ‘#include <iostream>’
```

## Reproduction Steps

1. Add the new `src/native_execute_stub.cpp` implementation.
2. Run `cmake --build build`.
3. Observe the compile fail in the watchdog lambda.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Date: `2026-05-16`
- Build: local CMake build

## First Hypothesis

The new native runner writes watchdog status with `std::cout`, but the translation unit forgot to include `<iostream>`.
