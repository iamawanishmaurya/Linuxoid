# Solution: native-execute-runner-missing-iostream-include

Problem reference: [2026-05-16-native-execute-runner-missing-iostream-include.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-native-execute-runner-missing-iostream-include.md)

## What Failed

The new watchdog path in `src/native_execute_stub.cpp` wrote status through `std::cout`, but the file did not include `<iostream>`.

## What Worked

Added the missing `#include <iostream>` and rebuilt.

## Why It Worked

The compile error was not architectural; it was a missing standard-library include in the new translation unit.

## Commands Run

```bash
cmake --build build
```
