# Runtime recovery baseline action count wrong

## Exact error

```text
Test failure: expected deterministic baseline recovery actions
```

## Reproduction steps

1. `cd /home/astra/codex/wine-for-android`
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`
3. Observe `TestRuntimeRecoveryPlanWritesStableArtifacts` fail

## Environment

- Repository: `/home/astra/codex/wine-for-android`
- Branch: `main`
- Date: `2026-05-17`
- Toolchain: CMake + C++20 local build

## First hypothesis

The new test assumed the baseline fixture would always yield both a native-loading recovery action and a DEX/ART recovery action. In reality the baseline fixture already stages a host native library successfully, so the only remaining deterministic baseline recovery action is the DEX/ART one.
