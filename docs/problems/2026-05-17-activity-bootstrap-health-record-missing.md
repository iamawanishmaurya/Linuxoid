# Problem: Activity bootstrap seam missing from runtime health and replay

- Date: 2026-05-17
- Environment: `/home/astra/codex/wine-for-android` on Linux, CMake build directory `build`

## Exact error

```text
Test failure: expected activity bootstrap readiness record
```

## Reproduction steps

1. Open the repo at `/home/astra/codex/wine-for-android`.
2. Run `cmake --build build && ctest --test-dir build --output-on-failure`.
3. Observe `wfa_tests` fail in `TestRuntimeHealthFixtureTracksActivityBootstrapReadiness`.

## First hypothesis

Linuxoid already writes deterministic activity-bootstrap plan, trace, and result artifacts, but `RunRuntimeHealthFixture` and `ReplayRuntimeDiagnosticBundle` still only track classloader/runtime-smoke and do not treat activity bootstrap as a first-class subsystem. The self-healing runtime therefore stops one seam too early.
