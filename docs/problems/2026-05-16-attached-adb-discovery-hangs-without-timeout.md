# Problem: attached-adb-discovery-hangs-without-timeout

- Timestamp: 2026-05-16T14:54:48+05:30
- Environment: Codex desktop app, repository `/home/astra/codex/wine-for-android`, branch `main`, full-access mode, network enabled

## Exact Error

```text
./build/compatctl discover-runtime attached-adb
./build/compatctl preflight-runtime attached-adb

Observed behavior: both commands remained running with no output for more than 20 seconds instead of returning a report.
```

## Reproduction Steps

1. Build the current branch with `cmake --build build`.
2. Run `./build/compatctl discover-runtime attached-adb`.
3. Run `./build/compatctl preflight-runtime attached-adb`.
4. Observe that both commands hang instead of returning promptly.

## First Hypothesis

The live `adb devices` call can block in this environment, so the new discovery/preflight path needs an explicit timeout boundary instead of assuming the shell command will always return quickly.
