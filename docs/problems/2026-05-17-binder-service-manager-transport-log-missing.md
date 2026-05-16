## Exact Error

Command:

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Observed output:

```text
1/1 Test #1: wfa_tests ........................***Failed    0.24 sec
Test failure: expected deterministic binder transport log path
```

## Reproduction Steps

1. Extend the Binder-shaped service-manager test to require transport-log artifacts and round-trip tracking.
2. Run the command above.

## Environment

- Repo: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`

## First Hypothesis

The current Binder-shaped manager fixture records registration, lookup, and transaction artifacts, but it still does not expose a local transport log or round-trip metadata, so the new transport-seam contract is not implemented yet.
