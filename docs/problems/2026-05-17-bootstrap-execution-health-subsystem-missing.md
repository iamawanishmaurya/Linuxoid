# Bootstrap execution health subsystem missing

## Exact error

`Test failure: expected eight subsystem health records`

## Reproduction steps

1. Open `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe `wfa_tests` fail in the runtime-health assertions after adding bootstrap-execution expectations.

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`
- Toolchain: `CMake 4.x`, `C++20`
- Native path: Linux-only, no Waydroid, no ADB, no emulator

## First hypothesis

Linuxoid now has deterministic bootstrap-execution plan/result/trace artifacts, but `src/runtime_health.cpp` still folds that seam into `activity_bootstrap_readiness` instead of emitting a separate `bootstrap_execution_readiness` record with its own recovery action and replay presence.
