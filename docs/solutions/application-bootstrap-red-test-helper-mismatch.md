# Solution: Application Bootstrap Red Test Helper Mismatch

Problem: [2026-05-17-application-bootstrap-red-test-helper-mismatch.md](/home/astra/codex/wine-for-android/docs/problems/2026-05-17-application-bootstrap-red-test-helper-mismatch.md)

## What failed

The initial red test patch referenced the rendered JSON string before it was declared and used `ReadFile(...)` instead of the local `ReadTextFile(...)` helper inside `tests/test_main.cpp`.

## What worked

- Moved the `rendered` JSON string declaration above the new expectations.
- Switched the trace-file read to `ReadTextFile(report.trace_jsonl_path)`.

## Why it worked

The test harness was failing before it could exercise the product seam. Fixing the local test wiring let the suite reach the intended product-level red failure for the missing application/bootstrap trace behavior.

## Commands run

```bash
cd /home/astra/codex/wine-for-android
cmake --build build && ctest --test-dir build --output-on-failure
```
