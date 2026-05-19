# Structure

The repository is centered around a conventional C++ source tree plus extensive technical docs.

Top-level directories of interest:
- `/home/astra/codex/wine-for-android/include/wfa/`
- `/home/astra/codex/wine-for-android/src/`
- `/home/astra/codex/wine-for-android/tests/`
- `/home/astra/codex/wine-for-android/docs/`
- `/home/astra/codex/wine-for-android/build/`

Header layout:
- `/home/astra/codex/wine-for-android/include/wfa/` contains almost all public subsystem interfaces
- naming is mostly one-header-per-bridge or fixture
- headers track runtime slices cleanly and are a good map of domain boundaries

Source layout:
- `/home/astra/codex/wine-for-android/src/` mirrors the headers closely
- there is a corresponding `.cpp` for most bridge headers
- `src/main.cpp` is the CLI router
- `src/project_status.cpp` is the human-facing status summary

Testing layout:
- `/home/astra/codex/wine-for-android/tests/test_main.cpp` is the main test harness
- `/home/astra/codex/wine-for-android/tests/p1_fixture_native_activity.cpp` provides a shared native fixture library

Documentation layout:
- `/home/astra/codex/wine-for-android/README.md` is the operator-facing summary
- `/home/astra/codex/wine-for-android/CHANGELOG.md` tracks phase-level progress
- `/home/astra/codex/wine-for-android/docs/phased-build-plan.md` tracks roadmap status
- `/home/astra/codex/wine-for-android/docs/self-healing-runtime-skeleton.md` tracks the runtime narrative
- `/home/astra/codex/wine-for-android/docs/steps.md` acts like a running execution log

There is also a dense design journal pattern:
- `/home/astra/codex/wine-for-android/docs/problems/`
- `/home/astra/codex/wine-for-android/docs/solutions/`

Generated and staged runtime artifacts are usually not kept in the repo.
They are created under staging roots such as `/tmp/linuxoid-*` or user-provided roots.

The `.planning/` directory is active and already contains:
- roadmap and milestone state
- per-phase plans and summaries
- research notes
- the codebase map in `/home/astra/codex/wine-for-android/.planning/codebase/`
