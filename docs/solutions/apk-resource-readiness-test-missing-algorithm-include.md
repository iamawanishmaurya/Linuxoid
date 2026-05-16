# APK Resource Readiness Test Missing `<algorithm>` Include

Links back to: [2026-05-17-apk-resource-readiness-test-missing-algorithm-include.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-apk-resource-readiness-test-missing-algorithm-include.md)

## What failed

The new missing-manifest test used `std::find` over `std::vector<std::string>`, but `tests/test_main.cpp` did not include `<algorithm>`.

## What worked

Added `#include <algorithm>` at the top of `tests/test_main.cpp`.

## Why it worked

That header exposes the generic iterator-based `std::find` overloads. Without it, the compiler only saw unrelated overloads and rejected the call.

## Commands run

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
