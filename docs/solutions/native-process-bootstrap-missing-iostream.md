# Solution: native process bootstrap compile fails without `<iostream>`

## What Failed

The new `RunNativeProcessBootstrap(...)` path in `src/native_lifecycle.cpp` writes the child-runner report to `std::cout`, but the file did not include `<iostream>`, so `cmake --build build` failed at compile time.

Related problem: [2026-05-16-native-process-bootstrap-missing-iostream.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-16-native-process-bootstrap-missing-iostream.md)

## What Worked

Adding `#include <iostream>` to `src/native_lifecycle.cpp` resolves the missing symbol and lets the file compile again.

## Why It Worked

`std::cout` and its flush surface are declared in `<iostream>`. The new child-runner path introduced the first direct `std::cout` use in this file, so the translation unit needed that header.

## Commands Run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
