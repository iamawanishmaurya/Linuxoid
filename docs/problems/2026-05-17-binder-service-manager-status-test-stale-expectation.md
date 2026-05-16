## Exact Error

Command:

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Observed output:

```text
1/1 Test #1: wfa_tests ........................***Failed    0.01 sec
Test failure: expected average phase progress to equal 95
```

## Reproduction Steps

1. Increase the `P4` runtime/service phase progress in `src/project_status.cpp`.
2. Run the command above.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`

## First Hypothesis

The status-report test still encodes the older average scaffold progress value from before the Binder-shaped service-manager slice raised `P4`, so the test expectation now lags the code.
