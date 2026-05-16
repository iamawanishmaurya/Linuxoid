# Runtime Health Test ZIP Helper Order Build Failure Solution

Problem: [docs/problems/2026-05-17-runtime-health-test-zip-helper-order-build-failure.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-runtime-health-test-zip-helper-order-build-failure.md)

## What failed

The new runtime-health bootstrap fixture helper in `tests/test_main.cpp` called `WriteStoredZipFixture` before the compiler had seen any declaration for that helper.

## What worked

Added narrow forward declarations for:

- `ComputeCrc32`
- `WriteLe16`
- `WriteLe32`
- `WriteStoredZipFixture`

above the runtime-health fixture helper.

## Why it worked

The test file keeps its existing helper order, but the compiler now knows the ZIP-fixture helper signatures before the new bootstrap helper calls them, so the file builds cleanly without restructuring unrelated test code.

## Commands run

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
